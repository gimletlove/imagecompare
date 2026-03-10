#include "ui/TiledImageItem.h"

#include <QDebug>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPointer>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGNode>
#include <QSGSimpleTextureNode>
#include <QSet>
#include <QThread>
#include <QThreadPool>
#include <QWheelEvent>
#include <algorithm>
#include <exception>

extern "C" {
    void vips_thread_shutdown(void);
}

namespace {
    struct TileNodeKey {
        quint64 generation = 0;
        int level = 0;
        QPoint tile_index;

        friend bool operator==(const TileNodeKey&, const TileNodeKey&) = default;
    };

    uint qHash(const TileNodeKey& key, uint seed = 0) noexcept {
        return qHashMulti(seed, key.generation, key.level, key.tile_index.x(), key.tile_index.y());
    }

    struct TileSceneGraphRootNode final : public QSGNode {
        QHash<TileNodeKey, QSGSimpleTextureNode*> nodes_by_key;
    };

    void clear_tile_nodes(TileSceneGraphRootNode* root_node) {
        if (root_node == nullptr) {
            return;
        }
        for (auto it = root_node->nodes_by_key.begin(); it != root_node->nodes_by_key.end(); ++it) {
            QSGSimpleTextureNode* node = it.value();
            root_node->removeChildNode(node);
            delete node;
        }
        root_node->nodes_by_key.clear();
    }
}  // namespace

TiledImageItem::TiledImageItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    const int ideal_threads = std::max(1, QThread::idealThreadCount());
    m_tile_thread_pool.setMaxThreadCount(std::max(1, ideal_threads / 2));
    m_tile_thread_pool.setExpiryTimeout(10'000);
}

QString TiledImageItem::image_path() const { return m_image_path; }

void TiledImageItem::clear_tile_state() {
    const bool previous_renderable = m_has_renderable_content;
    m_texture_cache.clear();
    m_pending_tile_requests.clear();
    m_failed_tile_requests.clear();
    m_last_requested_tiles.clear();
    m_has_renderable_content = false;
    m_missing_tiles_pending = false;
    m_pending_tile_budget = 0;
    m_tile_fill_scheduled = false;
    if (previous_renderable != m_has_renderable_content) {
        Q_EMIT has_renderable_content_changed();
    }
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
            m_tile_pyramid = TilePyramid(m_image_size, 256);
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

    schedule_requested_tiles_update(8);
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
    m_texture_cache.clear();
    m_pending_tile_requests.clear();
    m_failed_tile_requests.clear();
    schedule_requested_tiles_update(8);
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
    schedule_requested_tiles_update(4);
    Q_EMIT viewport_rect_changed();
}

double TiledImageItem::zoom_factor() const noexcept { return m_view_state.zoom_factor(); }

void TiledImageItem::set_zoom_factor(double value) {
    const double previous = m_view_state.zoom_factor();
    m_view_state.set_zoom_factor(value);
    if (qFuzzyCompare(previous, m_view_state.zoom_factor())) {
        return;
    }
    schedule_requested_tiles_update(2);
    Q_EMIT zoom_factor_changed();
}

QPointF TiledImageItem::image_center() const noexcept { return m_view_state.image_center(); }

void TiledImageItem::set_image_center(const QPointF& center_point) {
    const QPointF previous_center = m_view_state.image_center();
    m_view_state.center_on(center_point);
    if (m_view_state.image_center() == previous_center) {
        return;
    }
    schedule_requested_tiles_update(2);
    Q_EMIT image_center_changed();
}

QSize TiledImageItem::image_pixel_size() const noexcept { return m_image_size; }

bool TiledImageItem::has_renderable_content() const noexcept { return m_has_renderable_content; }

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
    schedule_requested_tiles_update(2);
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
    schedule_requested_tiles_update(2);
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
    schedule_requested_tiles_update(2);
    if (!qFuzzyCompare(previous_zoom, m_view_state.zoom_factor())) {
        Q_EMIT zoom_factor_changed();
    }
    if (m_view_state.image_center() != previous_center) {
        Q_EMIT image_center_changed();
    }
}

double TiledImageItem::best_fit_zoom() const noexcept { return m_view_state.best_fit_zoom(); }

const QVector<QPoint>& TiledImageItem::last_requested_tiles() const noexcept { return m_last_requested_tiles; }

QSGNode* TiledImageItem::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*) {
    auto* root_node = old_node != nullptr ? static_cast<TileSceneGraphRootNode*>(old_node) : new TileSceneGraphRootNode();

    if (!m_has_renderable_content || window() == nullptr || m_image_path.isEmpty()) {
        clear_tile_nodes(root_node);
        return root_node;
    }

    const int level = m_tile_pyramid.level_for_zoom(m_view_state.zoom_factor());
    const QRectF visible_rect = visible_image_rect();
    const double zoom = m_view_state.zoom_factor();
    const QRectF viewport_bounds(0.0, 0.0, width(), height());
    QSet<TileNodeKey> active_keys;
    active_keys.reserve(m_last_requested_tiles.size());

    for (const QPoint& tile_index : m_last_requested_tiles) {
        const QImage* tile_image = m_texture_cache.tile_ptr(m_image_path, m_display_mode, level, tile_index);
        if (tile_image == nullptr || tile_image->isNull()) {
            continue;
        }

        const TileNodeKey key{
            .generation = m_scene_generation,
            .level = level,
            .tile_index = tile_index,
        };
        active_keys.insert(key);

        const QRect image_rect = tile_image_rect(level, tile_index);
        QSGSimpleTextureNode* texture_node = nullptr;
        auto existing = root_node->nodes_by_key.find(key);
        if (existing == root_node->nodes_by_key.end()) {
            auto* new_node = new QSGSimpleTextureNode();
            new_node->setOwnsTexture(true);
            QQuickWindow::CreateTextureOptions texture_flags = QQuickWindow::TextureCanUseAtlas;
            if (!tile_image->hasAlphaChannel()) {
                texture_flags |= QQuickWindow::TextureIsOpaque;
            }
            QSGTexture* texture = window()->createTextureFromImage(*tile_image, texture_flags);
            if (texture == nullptr) {
                delete new_node;
                continue;
            }
            new_node->setTexture(texture);
            root_node->appendChildNode(new_node);
            root_node->nodes_by_key.insert(key, new_node);
            texture_node = new_node;
        } else {
            texture_node = existing.value();
        }

        texture_node->setRect((image_rect.x() - visible_rect.left()) * zoom, (image_rect.y() - visible_rect.top()) * zoom,
                              image_rect.width() * zoom, image_rect.height() * zoom);
    }

    for (auto it = root_node->nodes_by_key.begin(); it != root_node->nodes_by_key.end();) {
        if (active_keys.contains(it.key())) {
            ++it;
            continue;
        }

        if (m_missing_tiles_pending && it.key().generation == m_scene_generation) {
            const QRect fallback_image_rect = tile_image_rect(it.key().level, it.key().tile_index);
            const QRectF fallback_rect((fallback_image_rect.x() - visible_rect.left()) * zoom,
                                       (fallback_image_rect.y() - visible_rect.top()) * zoom, fallback_image_rect.width() * zoom,
                                       fallback_image_rect.height() * zoom);
            if (fallback_rect.intersects(viewport_bounds)) {
                it.value()->setRect(fallback_rect);
                ++it;
                continue;
            }
        }

        QSGSimpleTextureNode* stale_node = it.value();
        root_node->removeChildNode(stale_node);
        delete stale_node;
        it = root_node->nodes_by_key.erase(it);
    }

    return root_node;
}

void TiledImageItem::schedule_requested_tiles_update(int max_missing_tiles_per_pass) {
    m_pending_tile_budget = std::max(m_pending_tile_budget, std::max(1, max_missing_tiles_per_pass));
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
    m_last_requested_tiles.clear();
    if (m_image_path.isEmpty() || m_viewport_rect.isEmpty()) {
        const bool previous_renderable = m_has_renderable_content;
        m_has_renderable_content = false;
        m_missing_tiles_pending = false;
        if (previous_renderable != m_has_renderable_content) {
            Q_EMIT has_renderable_content_changed();
        }
        update();
        return;
    }

    if (m_tile_pyramid.level_count() == 0) {
        const bool previous_renderable = m_has_renderable_content;
        m_has_renderable_content = false;
        m_missing_tiles_pending = false;
        if (previous_renderable != m_has_renderable_content) {
            Q_EMIT has_renderable_content_changed();
        }
        update();
        return;
    }

    const int level = m_tile_pyramid.level_for_zoom(m_view_state.zoom_factor());
    const QRect tile_bounds = m_tile_pyramid.visible_tile_bounds(level, visible_image_rect());
    const RenderSpec spec = current_render_spec();
    const int render_budget = std::max(1, max_missing_tiles_per_pass);
    int requested_missing_tiles = 0;
    bool pending_missing_tiles = false;
    bool renderable = false;

    for (int y = tile_bounds.top(); y < tile_bounds.top() + tile_bounds.height(); ++y) {
        for (int x = tile_bounds.left(); x < tile_bounds.left() + tile_bounds.width(); ++x) {
            const QPoint tile{x, y};
            m_last_requested_tiles.push_back(tile);

            const QImage* tile_image = m_texture_cache.tile_ptr(m_image_path, m_display_mode, level, tile);
            if (tile_image == nullptr || tile_image->isNull()) {
                const QString request_key = tile_request_key(m_scene_generation, m_image_path, m_display_mode, level, tile);
                if (m_failed_tile_requests.contains(request_key)) {
                    continue;
                }
                if (!m_pending_tile_requests.contains(request_key) && requested_missing_tiles < render_budget) {
                    m_pending_tile_requests.insert(request_key);
                    request_tile_render(m_scene_generation, m_image_path, m_display_mode, spec, level, tile, tile_image_rect(level, tile));
                    ++requested_missing_tiles;
                }
                pending_missing_tiles = true;
            }
            if (tile_image != nullptr && !tile_image->isNull()) {
                renderable = true;
            }
        }
    }

    const bool previous_renderable = m_has_renderable_content;
    m_has_renderable_content = renderable || (previous_renderable && pending_missing_tiles);
    m_missing_tiles_pending = pending_missing_tiles;
    if (previous_renderable != m_has_renderable_content) {
        Q_EMIT has_renderable_content_changed();
    }
    update();

    if (pending_missing_tiles && m_pending_tile_requests.isEmpty() && !m_tile_fill_scheduled) {
        m_tile_fill_scheduled = true;
        QMetaObject::invokeMethod(
            this,
            [this]() {
                m_tile_fill_scheduled = false;
                update_requested_tiles(12);
            },
            Qt::QueuedConnection);
    }
}

void TiledImageItem::wheelEvent(QWheelEvent* event) {
    if (event == nullptr) {
        return;
    }
    const double multiplier = event->angleDelta().y() > 0 ? 1.2 : (1.0 / 1.2);
    zoom_at(event->position(), multiplier);
    event->accept();
}

void TiledImageItem::mousePressEvent(QMouseEvent* event) {
    if (event == nullptr || event->button() != Qt::LeftButton) {
        QQuickItem::mousePressEvent(event);
        return;
    }
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

RenderSpec TiledImageItem::current_render_spec() const noexcept {
    if (m_display_mode == static_cast<int>(DisplayMode::StrictRaw)) {
        return {
            .ignore_orientation = false,
            .ignore_color_profile = true,
        };
    }
    return {};
}

QRectF TiledImageItem::visible_image_rect() const {
    const double zoom = std::max(0.01, m_view_state.zoom_factor());
    const QSizeF viewport = m_view_state.viewport_size();
    const QSizeF visible_size(viewport.width() / zoom, viewport.height() / zoom);
    const QPointF center = m_view_state.image_center();
    return QRectF(center.x() - visible_size.width() / 2.0, center.y() - visible_size.height() / 2.0, visible_size.width(),
                  visible_size.height());
}

QRect TiledImageItem::tile_image_rect(int level, const QPoint& tile_index) const {
    const int level_scale = 1 << std::max(0, level);
    const int tile_size = m_tile_pyramid.tile_size() * level_scale;
    const QRect raw_rect(tile_index.x() * tile_size, tile_index.y() * tile_size, tile_size, tile_size);
    const QRect bounds(0, 0, m_image_size.width(), m_image_size.height());
    return raw_rect.intersected(bounds);
}

QString TiledImageItem::tile_request_key(quint64 generation, const QString& image_path, int display_mode, int level,
                                         const QPoint& tile_index) {
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(generation)
        .arg(image_path)
        .arg(display_mode)
        .arg(level)
        .arg(tile_index.x())
        .arg(tile_index.y());
}

void TiledImageItem::request_tile_render(quint64 generation, QString image_path, int display_mode, RenderSpec spec, int level,
                                         QPoint tile_index, QRect image_rect) {
    QPointer<TiledImageItem> item_guard(this);
    const QString request_key = tile_request_key(generation, image_path, display_mode, level, tile_index);

    auto task = QRunnable::create([item_guard, generation, image_path = std::move(image_path), display_mode, spec, level, tile_index,
                                   image_rect, request_key]() {
        struct VipsThreadGuard {
            ~VipsThreadGuard() { vips_thread_shutdown(); }
        } vips_thread_guard;

        QImage tile_image;
        try {
            ImageSource source(image_path);
            const int level_scale = 1 << std::max(0, level);
            const QSize output_size(std::max(1, (image_rect.width() + level_scale - 1) / level_scale),
                                    std::max(1, (image_rect.height() + level_scale - 1) / level_scale));
            tile_image = source.render_region(image_rect, spec, output_size);
        } catch (const std::exception& ex) {
            qWarning().noquote() << "Failed to render tile" << image_path << "level" << level << "tile" << tile_index << ":" << ex.what();
        }

        if (item_guard == nullptr) {
            return;
        }

        QMetaObject::invokeMethod(
            item_guard,
            [item_guard, generation, image_path, display_mode, level, tile_index, request_key, tile_image]() {
                if (item_guard == nullptr) {
                    return;
                }

                item_guard->m_pending_tile_requests.remove(request_key);
                if (generation != item_guard->m_scene_generation || image_path != item_guard->m_image_path ||
                    display_mode != item_guard->m_display_mode) {
                    item_guard->update_requested_tiles(12);
                    return;
                }

                if (!tile_image.isNull()) {
                    item_guard->m_failed_tile_requests.remove(request_key);
                    item_guard->m_texture_cache.store_tile(image_path, display_mode, level, tile_index, tile_image);
                } else {
                    item_guard->m_failed_tile_requests.insert(request_key);
                }
                item_guard->update_requested_tiles(12);
            },
            Qt::QueuedConnection);
    });
    m_tile_thread_pool.start(task);
}
