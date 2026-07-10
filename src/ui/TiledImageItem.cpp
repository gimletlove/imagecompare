#include "ui/TiledImageItem.h"

#include <vips/vips.h>

#include <QDebug>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPointer>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGNode>
#include <QSGSimpleTextureNode>
#include <QScopeGuard>
#include <QSet>
#include <QThread>
#include <QThreadPool>
#include <QWheelEvent>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

namespace {
    constexpr int k_tile_size = 256;
    constexpr int k_interaction_tile_budget = 8;
    constexpr int k_viewport_tile_budget = 12;
    constexpr int k_refresh_tile_budget = 16;
    constexpr double k_prefetch_margin_fraction = 0.75;
    constexpr double k_max_prefetch_margin_viewport_pixels = 1536.0;
    constexpr int k_max_preview_dimension = 3072;
    constexpr qsizetype k_max_preview_pixels = 4'000'000;

    TileTextureKey tile_texture_key(int display_mode, int level, const QPoint& tile_index) {
        return {
            .display_mode = display_mode,
            .level = level,
            .tile_index = tile_index,
        };
    }

    struct TileCandidate {
        QPoint tile_index;
        bool visible = false;
        double distance_to_center = 0.0;
    };

    struct TileSceneGraphRootNode final : public QSGNode {
        QSGSimpleTextureNode* preview_node = nullptr;
        quint64 preview_generation = 0;
        QHash<TileRenderRequestKey, QSGSimpleTextureNode*> nodes_by_key;
    };

    void clear_preview_node(TileSceneGraphRootNode* root_node) {
        if (root_node == nullptr || root_node->preview_node == nullptr) {
            return;
        }
        root_node->removeChildNode(root_node->preview_node);
        delete root_node->preview_node;
        root_node->preview_node = nullptr;
        root_node->preview_generation = 0;
    }

    void clear_tile_nodes(TileSceneGraphRootNode* root_node) {
        if (root_node == nullptr) {
            return;
        }
        clear_preview_node(root_node);
        for (auto it = root_node->nodes_by_key.begin(); it != root_node->nodes_by_key.end(); ++it) {
            QSGSimpleTextureNode* node = it.value();
            root_node->removeChildNode(node);
            delete node;
        }
        root_node->nodes_by_key.clear();
    }

    QSGSimpleTextureNode* create_texture_node(QQuickWindow* window, const QImage& image) {
        if (window == nullptr || image.isNull()) {
            return nullptr;
        }

        auto* node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
        QQuickWindow::CreateTextureOptions texture_flags = QQuickWindow::TextureCanUseAtlas;
        if (!image.hasAlphaChannel()) {
            texture_flags |= QQuickWindow::TextureIsOpaque;
        }

        QSGTexture* texture = window->createTextureFromImage(image, texture_flags);
        if (texture == nullptr) {
            delete node;
            return nullptr;
        }

        node->setTexture(texture);
        return node;
    }

    void set_texture_node_rect(QSGSimpleTextureNode* node, const QRect& image_rect, const QRectF& visible_rect, double zoom) {
        if (node == nullptr) {
            return;
        }
        node->setRect((image_rect.x() - visible_rect.left()) * zoom, (image_rect.y() - visible_rect.top()) * zoom,
                      image_rect.width() * zoom, image_rect.height() * zoom);
    }

    QThreadPool& tile_render_thread_pool() {
        static QThreadPool pool;
        static const int configured = [] {
            const int ideal_threads = std::max(1, QThread::idealThreadCount());
            pool.setMaxThreadCount(std::max(1, ideal_threads / 2));
            pool.setExpiryTimeout(10'000);
            return 0;
        }();
        Q_UNUSED(configured);
        return pool;
    }
}  // namespace

TiledImageItem::TiledImageItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

QString TiledImageItem::image_path() const { return m_image_path; }

void TiledImageItem::clear_tile_state() {
    m_texture_cache.clear();
    m_pending_tile_requests.clear();
    m_failed_tile_requests.clear();
    m_active_tile_indices.clear();
    m_preview_image = QImage();
    m_preview_image_rect = QRect();
    m_preview_generation = 0;
    m_pending_preview_generation = 0;
    m_failed_preview_generation = 0;
    m_missing_tiles_pending = false;
    m_pending_tile_budget = 0;
    update();
}

void TiledImageItem::set_image_path(QString path) {
    if (path == m_image_path) {
        return;
    }

    const QSize previous_image_size = m_image_size;
    m_image_path = std::move(path);
    ++m_scene_generation;
    clear_tile_state();
    m_image_size = QSize();
    m_tile_pyramid = TilePyramid();

    if (!m_image_path.isEmpty()) {
        try {
            ImageSource source(m_image_path);
            m_image_size = source.pixel_size();
            m_tile_pyramid = TilePyramid(m_image_size, k_tile_size);
            m_view_state.set_image_size(QSizeF(m_image_size));
            m_view_state.center_on(QPointF(m_image_size.width() / 2.0, m_image_size.height() / 2.0));
            Q_EMIT image_center_changed();
        } catch (const std::exception& ex) {
            qWarning().noquote() << "Failed to load image for tiled view" << m_image_path << ":" << ex.what();
            m_image_size = QSize();
            m_tile_pyramid = TilePyramid();
            m_view_state.set_image_size(QSizeF());
            m_view_state.center_on(QPointF());
            Q_EMIT image_center_changed();
        }
    }

    schedule_requested_tiles_update(k_refresh_tile_budget);
    Q_EMIT image_path_changed();
    if (m_image_size != previous_image_size) {
        Q_EMIT image_pixel_size_changed();
    }
}

int TiledImageItem::display_mode() const noexcept { return m_display_mode; }

void TiledImageItem::set_display_mode(int mode) {
    if (mode == m_display_mode) {
        return;
    }
    m_display_mode = mode;
    ++m_scene_generation;
    m_pending_tile_requests.clear();
    m_failed_tile_requests.clear();
    m_pending_preview_generation = 0;
    m_failed_preview_generation = 0;
    m_missing_tiles_pending = true;
    schedule_requested_tiles_update(k_refresh_tile_budget);
    Q_EMIT display_mode_changed();
}

QRectF TiledImageItem::viewport_rect() const noexcept { return m_viewport_rect; }

void TiledImageItem::set_viewport_rect(const QRectF& rect) {
    if (rect == m_viewport_rect) {
        return;
    }
    m_viewport_rect = rect;
    m_view_state.set_viewport_size(rect.size());
    if (m_view_state.image_center().isNull() && m_image_size.isValid()) {
        m_view_state.center_on(QPointF(m_image_size.width() / 2.0, m_image_size.height() / 2.0));
        Q_EMIT image_center_changed();
    }
    schedule_requested_tiles_update(k_viewport_tile_budget);
    Q_EMIT viewport_rect_changed();
}

double TiledImageItem::zoom_factor() const noexcept { return m_view_state.zoom_factor(); }

void TiledImageItem::set_zoom_factor(double value) {
    const double previous = m_view_state.zoom_factor();
    m_view_state.set_zoom_factor(value);
    if (qFuzzyCompare(previous, m_view_state.zoom_factor())) {
        return;
    }
    schedule_requested_tiles_update(k_interaction_tile_budget);
    Q_EMIT zoom_factor_changed();
}

QPointF TiledImageItem::image_center() const noexcept { return m_view_state.image_center(); }

void TiledImageItem::set_image_center(const QPointF& center_point) {
    const QPointF previous_center = m_view_state.image_center();
    m_view_state.center_on(center_point);
    if (m_view_state.image_center() == previous_center) {
        return;
    }
    schedule_requested_tiles_update(k_interaction_tile_budget);
    Q_EMIT image_center_changed();
}

QSize TiledImageItem::image_pixel_size() const noexcept { return m_image_size; }

void TiledImageItem::zoom_at(const QPointF& viewport_point, double zoom_multiplier) {
    if (zoom_multiplier <= 0.0) {
        return;
    }

    const QPointF previous_center = m_view_state.image_center();
    const double previous_zoom = m_view_state.zoom_factor();
    const QPointF anchor = m_view_state.viewport_to_image_point(viewport_point);
    const double next_zoom = std::max(0.01, previous_zoom * zoom_multiplier);
    m_view_state.set_zoom_factor(next_zoom);
    const double applied_zoom = m_view_state.zoom_factor();
    m_view_state.center_on(anchor - (viewport_point - m_view_state.viewport_center()) / applied_zoom);
    schedule_requested_tiles_update(k_interaction_tile_budget);
    if (m_view_state.image_center() != previous_center) {
        Q_EMIT image_center_changed();
    }
    if (!qFuzzyCompare(previous_zoom, m_view_state.zoom_factor())) {
        Q_EMIT zoom_factor_changed();
    }
}

void TiledImageItem::pan_by(const QPointF& viewport_delta) {
    const QPointF previous_center = m_view_state.image_center();
    m_view_state.pan_by_viewport_delta(viewport_delta);
    if (m_view_state.image_center() == previous_center) {
        return;
    }
    schedule_requested_tiles_update(k_interaction_tile_budget);
    Q_EMIT image_center_changed();
}

void TiledImageItem::set_best_fit() {
    const QPointF previous_center = m_view_state.image_center();
    const double previous_zoom = m_view_state.zoom_factor();
    const double fit_zoom = m_view_state.best_fit_zoom();
    m_view_state.set_zoom_factor(fit_zoom);
    if (m_image_size.isValid()) {
        m_view_state.center_on(QPointF(m_image_size.width() / 2.0, m_image_size.height() / 2.0));
    }
    schedule_requested_tiles_update(k_viewport_tile_budget);
    if (!qFuzzyCompare(previous_zoom, m_view_state.zoom_factor())) {
        Q_EMIT zoom_factor_changed();
    }
    if (m_view_state.image_center() != previous_center) {
        Q_EMIT image_center_changed();
    }
}

double TiledImageItem::best_fit_zoom() const noexcept { return m_view_state.best_fit_zoom(); }

QSGNode* TiledImageItem::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*) {
    auto* root_node = old_node != nullptr ? static_cast<TileSceneGraphRootNode*>(old_node) : new TileSceneGraphRootNode();

    if (window() == nullptr || m_image_path.isEmpty()) {
        clear_tile_nodes(root_node);
        return root_node;
    }

    const QRectF visible_rect = visible_image_rect();
    const double zoom = m_view_state.zoom_factor();
    const QRectF viewport_bounds(0.0, 0.0, width(), height());
    const bool can_reuse_stale_tile_nodes = !m_texture_cache.isEmpty();

    if (!m_preview_image.isNull() && !m_preview_image_rect.isEmpty()) {
        if (root_node->preview_node == nullptr || root_node->preview_generation != m_preview_generation) {
            clear_preview_node(root_node);

            auto* new_node = create_texture_node(window(), m_preview_image);
            if (new_node != nullptr) {
                root_node->prependChildNode(new_node);
                root_node->preview_node = new_node;
                root_node->preview_generation = m_preview_generation;
            }
        }

        set_texture_node_rect(root_node->preview_node, m_preview_image_rect, visible_rect, zoom);
    } else {
        clear_preview_node(root_node);
    }

    const int level = m_tile_pyramid.level_for_zoom(m_view_state.zoom_factor());
    QSet<TileRenderRequestKey> active_keys;
    active_keys.reserve(m_active_tile_indices.size());

    for (const QPoint& tile_index : std::as_const(m_active_tile_indices)) {
        const auto tile_image = m_texture_cache.constFind(tile_texture_key(m_display_mode, level, tile_index));
        if (tile_image == m_texture_cache.cend() || tile_image->isNull()) {
            continue;
        }

        const TileRenderRequestKey key{
            .generation = m_scene_generation,
            .level = level,
            .tile_index = tile_index,
        };
        active_keys.insert(key);

        const QRect image_rect = tile_image_rect(level, tile_index);
        QSGSimpleTextureNode* texture_node = nullptr;
        auto existing = root_node->nodes_by_key.find(key);
        if (existing == root_node->nodes_by_key.end()) {
            auto* new_node = create_texture_node(window(), tile_image.value());
            if (new_node == nullptr) {
                continue;
            }
            root_node->appendChildNode(new_node);
            root_node->nodes_by_key.insert(key, new_node);
            texture_node = new_node;
        } else {
            texture_node = existing.value();
        }

        set_texture_node_rect(texture_node, image_rect, visible_rect, zoom);
    }

    for (auto it = root_node->nodes_by_key.begin(); it != root_node->nodes_by_key.end();) {
        if (active_keys.contains(it.key())) {
            ++it;
            continue;
        }

        if (it.key().generation != m_scene_generation) {
            const TileRenderRequestKey current_tile_key{
                .generation = m_scene_generation,
                .level = it.key().level,
                .tile_index = it.key().tile_index,
            };
            if (!can_reuse_stale_tile_nodes || active_keys.contains(current_tile_key)) {
                QSGSimpleTextureNode* stale_node = it.value();
                root_node->removeChildNode(stale_node);
                delete stale_node;
                it = root_node->nodes_by_key.erase(it);
                continue;
            }
        }

        const QRect fallback_image_rect = tile_image_rect(it.key().level, it.key().tile_index);
        const QRectF fallback_rect((fallback_image_rect.x() - visible_rect.left()) * zoom,
                                   (fallback_image_rect.y() - visible_rect.top()) * zoom, fallback_image_rect.width() * zoom,
                                   fallback_image_rect.height() * zoom);
        if (m_missing_tiles_pending && fallback_rect.intersects(viewport_bounds)) {
            it.value()->setRect(fallback_rect);
            ++it;
            continue;
        }

        QSGSimpleTextureNode* stale_node = it.value();
        root_node->removeChildNode(stale_node);
        delete stale_node;
        it = root_node->nodes_by_key.erase(it);
    }

    return root_node;
}

void TiledImageItem::schedule_requested_tiles_update(int max_missing_tiles_per_pass) {
    m_missing_tiles_pending = true;
    m_pending_tile_budget = std::max(m_pending_tile_budget, std::max(1, max_missing_tiles_per_pass));
    update();
    if (m_tile_update_scheduled) {
        return;
    }
    m_tile_update_scheduled = true;
    QMetaObject::invokeMethod(
        this,
        [this]() {
            m_tile_update_scheduled = false;
            const int budget = std::max(1, m_pending_tile_budget);
            m_pending_tile_budget = 0;
            update_requested_tiles(budget);
        },
        Qt::QueuedConnection);
}

void TiledImageItem::update_requested_tiles(int max_missing_tiles_per_pass) {
    m_active_tile_indices.clear();
    if (m_image_path.isEmpty() || m_viewport_rect.isEmpty()) {
        m_missing_tiles_pending = false;
        update();
        return;
    }

    if (m_tile_pyramid.level_count() == 0) {
        m_missing_tiles_pending = false;
        update();
        return;
    }

    const RenderSpec spec{.ignore_color_profile = m_display_mode == static_cast<int>(DisplayMode::StrictRaw)};
    const bool current_preview_ready = !m_preview_image.isNull() && m_preview_generation == m_scene_generation;
    const bool current_preview_failed = m_failed_preview_generation == m_scene_generation;
    if (!current_preview_ready && !current_preview_failed && m_pending_preview_generation == 0) {
        const QRect image_bounds(0, 0, m_image_size.width(), m_image_size.height());
        request_preview_render(m_scene_generation, m_image_path, m_display_mode, spec, image_bounds, preview_output_size());
    }

    const QRectF visible_rect = visible_image_rect();
    const QRectF prefetch_rect = prefetch_image_rect(visible_rect);
    const int level = m_tile_pyramid.level_for_zoom(m_view_state.zoom_factor());
    const QRect visible_bounds = m_tile_pyramid.visible_tile_bounds(level, visible_rect);
    const QRect prefetch_bounds = m_tile_pyramid.visible_tile_bounds(level, prefetch_rect);
    const QPointF center_tile((visible_bounds.left() + visible_bounds.right()) / 2.0,
                              (visible_bounds.top() + visible_bounds.bottom()) / 2.0);

    QVector<TileCandidate> candidates;
    QSet<QPoint> seen_tiles;
    const qsizetype prefetch_tile_count = static_cast<qsizetype>(prefetch_bounds.width()) * prefetch_bounds.height();
    candidates.reserve(prefetch_tile_count);
    seen_tiles.reserve(prefetch_tile_count);
    auto append_candidates = [&](const QRect& bounds, bool visible) {
        for (int y = bounds.top(); y < bounds.top() + bounds.height(); ++y) {
            for (int x = bounds.left(); x < bounds.left() + bounds.width(); ++x) {
                const QPoint tile{x, y};
                if (seen_tiles.contains(tile)) {
                    continue;
                }
                seen_tiles.insert(tile);
                const double dx = static_cast<double>(x) - center_tile.x();
                const double dy = static_cast<double>(y) - center_tile.y();
                candidates.push_back(TileCandidate{
                    .tile_index = tile,
                    .visible = visible,
                    .distance_to_center = dx * dx + dy * dy,
                });
            }
        }
    };
    append_candidates(visible_bounds, true);
    append_candidates(prefetch_bounds, false);

    std::sort(candidates.begin(), candidates.end(), [](const TileCandidate& lhs, const TileCandidate& rhs) {
        if (lhs.visible != rhs.visible) {
            return lhs.visible;
        }
        return lhs.distance_to_center < rhs.distance_to_center;
    });

    m_active_tile_indices.reserve(static_cast<qsizetype>(visible_bounds.width()) * visible_bounds.height());
    const int render_budget = std::max(1, max_missing_tiles_per_pass);
    int requested_missing_tiles = 0;
    bool visible_missing_tiles = false;

    for (const TileCandidate& candidate : candidates) {
        if (candidate.visible) {
            m_active_tile_indices.push_back(candidate.tile_index);
        }

        const auto tile_image = m_texture_cache.constFind(tile_texture_key(m_display_mode, level, candidate.tile_index));
        if (tile_image != m_texture_cache.cend() && !tile_image->isNull()) {
            continue;
        }

        if (candidate.visible) {
            visible_missing_tiles = true;
        }

        const TileRenderRequestKey request_key = tile_request_key(m_scene_generation, level, candidate.tile_index);
        if (m_failed_tile_requests.contains(request_key)) {
            continue;
        }
        if (!m_pending_tile_requests.contains(request_key) && requested_missing_tiles < render_budget) {
            m_pending_tile_requests.insert(request_key);
            request_tile_render(m_scene_generation, m_image_path, m_display_mode, spec, level, candidate.tile_index,
                                tile_image_rect(level, candidate.tile_index));
            ++requested_missing_tiles;
        }
    }

    m_missing_tiles_pending = visible_missing_tiles;
    update();
}

void TiledImageItem::wheelEvent(QWheelEvent* event) {
    if (event == nullptr) {
        return;
    }
    Q_EMIT activated();
    const double multiplier = event->angleDelta().y() > 0 ? 1.2 : (1.0 / 1.2);
    zoom_at(event->position(), multiplier);
    event->accept();
}

void TiledImageItem::mousePressEvent(QMouseEvent* event) {
    if (event == nullptr || event->button() != Qt::LeftButton) {
        QQuickItem::mousePressEvent(event);
        return;
    }
    Q_EMIT activated();
    m_dragging = true;
    m_last_drag_position = event->position();
    event->accept();
}

void TiledImageItem::mouseMoveEvent(QMouseEvent* event) {
    if (event == nullptr || !m_dragging) {
        QQuickItem::mouseMoveEvent(event);
        return;
    }

    const QPointF delta = event->position() - m_last_drag_position;
    m_last_drag_position = event->position();
    pan_by(delta);
    event->accept();
}

void TiledImageItem::mouseReleaseEvent(QMouseEvent* event) {
    if (event != nullptr && event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
        return;
    }
    QQuickItem::mouseReleaseEvent(event);
}

QRectF TiledImageItem::visible_image_rect() const {
    const double zoom = std::max(0.01, m_view_state.zoom_factor());
    const QSizeF viewport = m_view_state.viewport_size();
    const QSizeF visible_size(viewport.width() / zoom, viewport.height() / zoom);
    const QPointF center = m_view_state.image_center();
    return QRectF(center.x() - visible_size.width() / 2.0, center.y() - visible_size.height() / 2.0, visible_size.width(),
                  visible_size.height());
}

QRectF TiledImageItem::prefetch_image_rect(const QRectF& visible_rect) const {
    if (visible_rect.isEmpty()) {
        return visible_rect;
    }

    const double zoom = std::max(0.01, m_view_state.zoom_factor());
    const double max_margin_x = k_max_prefetch_margin_viewport_pixels / zoom;
    const double max_margin_y = k_max_prefetch_margin_viewport_pixels / zoom;
    const double margin_x = std::min(visible_rect.width() * k_prefetch_margin_fraction, max_margin_x);
    const double margin_y = std::min(visible_rect.height() * k_prefetch_margin_fraction, max_margin_y);
    return visible_rect.adjusted(-margin_x, -margin_y, margin_x, margin_y);
}

QRect TiledImageItem::tile_image_rect(int level, const QPoint& tile_index) const {
    const int level_scale = 1 << std::max(0, level);
    const int tile_size = m_tile_pyramid.tile_size() * level_scale;
    const QRect raw_rect(tile_index.x() * tile_size, tile_index.y() * tile_size, tile_size, tile_size);
    const QRect bounds(0, 0, m_image_size.width(), m_image_size.height());
    return raw_rect.intersected(bounds);
}

QSize TiledImageItem::preview_output_size() const {
    if (!m_image_size.isValid()) {
        return {};
    }

    const double width = static_cast<double>(m_image_size.width());
    const double height = static_cast<double>(m_image_size.height());
    double scale = std::min(1.0, static_cast<double>(k_max_preview_dimension) / std::max(width, height));
    const double scaled_pixels = width * height * scale * scale;
    if (scaled_pixels > static_cast<double>(k_max_preview_pixels)) {
        scale *= std::sqrt(static_cast<double>(k_max_preview_pixels) / scaled_pixels);
    }

    return QSize(std::max(1, static_cast<int>(std::floor(width * scale))), std::max(1, static_cast<int>(std::floor(height * scale))));
}

TileRenderRequestKey TiledImageItem::tile_request_key(quint64 generation, int level, const QPoint& tile_index) {
    return TileRenderRequestKey{
        .generation = generation,
        .level = level,
        .tile_index = tile_index,
    };
}

void TiledImageItem::request_tile_render(quint64 generation, QString image_path, int display_mode, RenderSpec spec, int level,
                                         QPoint tile_index, QRect image_rect) {
    QPointer<TiledImageItem> item_guard(this);
    const TileRenderRequestKey request_key = tile_request_key(generation, level, tile_index);

    auto task = QRunnable::create(
        [item_guard, generation, image_path = std::move(image_path), display_mode, spec, level, tile_index, image_rect, request_key]() {
            const auto vips_thread_guard = qScopeGuard([] { vips_thread_shutdown(); });

            QImage tile_image;
            QString error_text;
            try {
                ImageSource source(image_path);
                const int level_scale = 1 << std::max(0, level);
                const QSize output_size(std::max(1, (image_rect.width() + level_scale - 1) / level_scale),
                                        std::max(1, (image_rect.height() + level_scale - 1) / level_scale));
                tile_image = source.render_region(image_rect, spec, output_size);
            } catch (const std::exception& ex) {
                error_text = QString::fromUtf8(ex.what());
            }

            if (item_guard == nullptr) {
                return;
            }

            QMetaObject::invokeMethod(
                item_guard,
                [item_guard, generation, image_path, display_mode, level, tile_index, request_key, tile_image, error_text]() {
                    if (item_guard == nullptr) {
                        return;
                    }

                    item_guard->m_pending_tile_requests.remove(request_key);
                    if (generation != item_guard->m_scene_generation || image_path != item_guard->m_image_path ||
                        display_mode != item_guard->m_display_mode) {
                        item_guard->schedule_requested_tiles_update(k_refresh_tile_budget);
                        return;
                    }

                    if (!tile_image.isNull()) {
                        item_guard->m_failed_tile_requests.remove(request_key);
                        item_guard->m_texture_cache.insert(tile_texture_key(display_mode, level, tile_index), tile_image);
                    } else {
                        if (!error_text.isEmpty()) {
                            qWarning().noquote()
                                << "Failed to render tile" << image_path << "level" << level << "tile" << tile_index << ":" << error_text;
                        }
                        item_guard->m_failed_tile_requests.insert(request_key);
                    }
                    item_guard->update_requested_tiles(k_refresh_tile_budget);
                },
                Qt::QueuedConnection);
        });
    tile_render_thread_pool().start(task);
}

void TiledImageItem::request_preview_render(quint64 generation, QString image_path, int display_mode, RenderSpec spec, QRect image_rect,
                                            QSize output_size) {
    QPointer<TiledImageItem> item_guard(this);
    m_pending_preview_generation = generation;

    auto task =
        QRunnable::create([item_guard, generation, image_path = std::move(image_path), display_mode, spec, image_rect, output_size]() {
            const auto vips_thread_guard = qScopeGuard([] { vips_thread_shutdown(); });

            QImage preview_image;
            QString error_text;
            try {
                ImageSource source(image_path);
                preview_image = source.render_region(image_rect, spec, output_size);
            } catch (const std::exception& ex) {
                error_text = QString::fromUtf8(ex.what());
            }

            if (item_guard == nullptr) {
                return;
            }

            QMetaObject::invokeMethod(
                item_guard,
                [item_guard, generation, image_path, display_mode, image_rect, preview_image, error_text]() {
                    if (item_guard == nullptr) {
                        return;
                    }

                    if (item_guard->m_pending_preview_generation == generation) {
                        item_guard->m_pending_preview_generation = 0;
                    }
                    if (generation != item_guard->m_scene_generation || image_path != item_guard->m_image_path ||
                        display_mode != item_guard->m_display_mode) {
                        return;
                    }

                    if (!preview_image.isNull()) {
                        item_guard->m_preview_image = preview_image;
                        item_guard->m_preview_image_rect = image_rect;
                        item_guard->m_preview_generation = generation;
                        item_guard->m_failed_preview_generation = 0;
                        item_guard->update();
                    } else {
                        if (!error_text.isEmpty()) {
                            qWarning().noquote() << "Failed to render preview" << image_path << ":" << error_text;
                        }
                        item_guard->m_failed_preview_generation = generation;
                    }
                    item_guard->update_requested_tiles(k_refresh_tile_budget);
                },
                Qt::QueuedConnection);
        });
    tile_render_thread_pool().start(task);
}
