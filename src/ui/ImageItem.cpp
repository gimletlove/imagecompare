#include "ui/ImageItem.h"

#include <QDebug>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPointer>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QThread>
#include <QThreadPool>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

#include "core/ImageSource.h"

namespace {
    struct ImageTextureNode final : public QSGSimpleTextureNode {
        qint64 image_cache_key = 0;
    };

    QThreadPool& decode_thread_pool() {
        static QThreadPool pool;
        static const int configured = [] {
            const int ideal_threads = std::max(1, QThread::idealThreadCount());
            pool.setMaxThreadCount(std::min(8, std::max(1, ideal_threads / 2)));
            pool.setExpiryTimeout(10'000);
            return 0;
        }();
        Q_UNUSED(configured);
        return pool;
    }
}  // namespace

ImageItem::ImageItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

QString ImageItem::source_path() const { return m_source_path; }

QImage ImageItem::input_image() const { return m_input_image; }

void ImageItem::clear_image() {
    m_image = {};
    update();
}

void ImageItem::set_source_path(QString path) {
    if (path == m_source_path) {
        return;
    }
    m_source_path = std::move(path);
    ++m_load_generation;
    if (m_input_image.isNull()) {
        load_effective_image();
    }
}

void ImageItem::set_input_image(QImage image) {
    if (image.cacheKey() == m_input_image.cacheKey()) {
        return;
    }
    m_input_image = std::move(image);
    ++m_load_generation;
    load_effective_image();
}

int ImageItem::color_mode() const noexcept { return static_cast<int>(m_color_mode); }

void ImageItem::set_color_mode(int mode) {
    if (mode < static_cast<int>(ColorMode::Raw) || mode > static_cast<int>(ColorMode::Faithful)) {
        return;
    }

    const auto color_mode = static_cast<ColorMode>(mode);
    if (color_mode == m_color_mode) {
        return;
    }

    m_color_mode = color_mode;
    if (m_input_image.isNull() && !m_source_path.isEmpty() && m_pixel_size.isValid()) {
        ++m_load_generation;
        clear_image();
        request_decode(m_load_generation, m_source_path, m_color_mode == ColorMode::Faithful);
    }
}

void ImageItem::load_effective_image() {
    const QSize previous_pixel_size = m_pixel_size;
    clear_image();
    m_pixel_size = {};

    if (!m_input_image.isNull()) {
        m_image = m_input_image;
        m_pixel_size = m_image.size();
    } else if (!m_source_path.isEmpty()) {
        try {
            const ImageSource source(m_source_path);
            m_pixel_size = source.pixel_size();
            request_decode(m_load_generation, m_source_path, m_color_mode == ColorMode::Faithful);
        } catch (const std::exception& ex) {
            qWarning().noquote() << "Failed to load image" << m_source_path << ":" << ex.what();
        }
    }

    m_view_state.set_image_size(QSizeF(m_pixel_size));
    m_view_state.center_on(m_pixel_size.isValid() ? QPointF(m_pixel_size.width() / 2.0, m_pixel_size.height() / 2.0) : QPointF{});
    Q_EMIT image_center_changed();
    if (m_pixel_size != previous_pixel_size) {
        Q_EMIT image_pixel_size_changed();
    }
}

void ImageItem::geometryChange(const QRectF& new_geometry, const QRectF& old_geometry) {
    QQuickItem::geometryChange(new_geometry, old_geometry);
    if (new_geometry.size() != old_geometry.size()) {
        update_viewport_size();
    }
}

void ImageItem::itemChange(ItemChange change, const ItemChangeData& value) {
    QQuickItem::itemChange(change, value);
    if (change == ItemDevicePixelRatioHasChanged) {
        m_device_pixel_ratio = std::max<qreal>(1.0, value.realValue);
        update_viewport_size();
    } else if (change == ItemSceneChange && value.window != nullptr) {
        m_device_pixel_ratio = std::max<qreal>(1.0, value.window->effectiveDevicePixelRatio());
        update_viewport_size();
    }
}

void ImageItem::update_viewport_size() {
    m_view_state.set_viewport_size(QSizeF(width() * m_device_pixel_ratio, height() * m_device_pixel_ratio));
    if (m_view_state.image_center().isNull() && m_pixel_size.isValid()) {
        m_view_state.center_on(QPointF(m_pixel_size.width() / 2.0, m_pixel_size.height() / 2.0));
        Q_EMIT image_center_changed();
    }
    update();
}

double ImageItem::zoom_factor() const noexcept { return m_view_state.zoom_factor(); }

void ImageItem::set_zoom_factor(double value) {
    const double previous = m_view_state.zoom_factor();
    m_view_state.set_zoom_factor(value);
    if (qFuzzyCompare(previous, m_view_state.zoom_factor())) {
        return;
    }
    update();
    Q_EMIT zoom_factor_changed();
}

QPointF ImageItem::image_center() const noexcept { return m_view_state.image_center(); }

void ImageItem::set_image_center(const QPointF& center_point) {
    const QPointF previous_center = m_view_state.image_center();
    m_view_state.center_on(center_point);
    if (m_view_state.image_center() == previous_center) {
        return;
    }
    update();
    Q_EMIT image_center_changed();
}

QSize ImageItem::image_pixel_size() const noexcept { return m_pixel_size; }

QPointF ImageItem::viewport_point_in_pixels(const QPointF& point) const noexcept { return point * m_device_pixel_ratio; }

void ImageItem::zoom_at(const QPointF& viewport_point, double zoom_multiplier) {
    if (zoom_multiplier <= 0.0) {
        return;
    }

    const QPointF previous_center = m_view_state.image_center();
    const double previous_zoom = m_view_state.zoom_factor();
    const QPointF viewport_pixels = viewport_point_in_pixels(viewport_point);
    const QPointF anchor = m_view_state.viewport_to_image_point(viewport_pixels);
    m_view_state.set_zoom_factor(std::max(0.01, previous_zoom * zoom_multiplier));
    const double applied_zoom = m_view_state.zoom_factor();
    m_view_state.center_on(anchor - (viewport_pixels - m_view_state.viewport_center()) / applied_zoom);
    update();
    if (m_view_state.image_center() != previous_center) {
        Q_EMIT image_center_changed();
    }
    if (!qFuzzyCompare(previous_zoom, m_view_state.zoom_factor())) {
        Q_EMIT zoom_factor_changed();
    }
}

void ImageItem::pan_by(const QPointF& viewport_delta) {
    const QPointF previous_center = m_view_state.image_center();
    m_view_state.pan_by_viewport_delta(viewport_point_in_pixels(viewport_delta));
    if (m_view_state.image_center() == previous_center) {
        return;
    }
    update();
    Q_EMIT image_center_changed();
}

void ImageItem::set_best_fit() {
    const QPointF previous_center = m_view_state.image_center();
    const double previous_zoom = m_view_state.zoom_factor();
    m_view_state.set_zoom_factor(m_view_state.best_fit_zoom());
    if (m_pixel_size.isValid()) {
        m_view_state.center_on(QPointF(m_pixel_size.width() / 2.0, m_pixel_size.height() / 2.0));
    }
    update();
    if (!qFuzzyCompare(previous_zoom, m_view_state.zoom_factor())) {
        Q_EMIT zoom_factor_changed();
    }
    if (m_view_state.image_center() != previous_center) {
        Q_EMIT image_center_changed();
    }
}

double ImageItem::best_fit_zoom() const noexcept { return m_view_state.best_fit_zoom(); }

QSGNode* ImageItem::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*) {
    auto* node = static_cast<ImageTextureNode*>(old_node);
    if (window() == nullptr || m_image.isNull()) {
        delete node;
        return nullptr;
    }

    const qint64 image_cache_key = m_image.cacheKey();
    if (node == nullptr || node->image_cache_key != image_cache_key) {
        delete node;
        node = new ImageTextureNode();

        QQuickWindow::CreateTextureOptions texture_flags;
        if (!m_image.hasAlphaChannel()) {
            texture_flags |= QQuickWindow::TextureIsOpaque;
        }
        QSGTexture* texture = window()->createTextureFromImage(m_image, texture_flags);
        if (texture == nullptr) {
            delete node;
            return nullptr;
        }

        node->setOwnsTexture(true);
        node->setTexture(texture);
        node->image_cache_key = image_cache_key;
    }

    const double zoom = m_view_state.zoom_factor();
    const QRectF visible_rect = visible_image_rect();
    const double scale = zoom / m_device_pixel_ratio;
    node->setFiltering(zoom >= 1.0 ? QSGTexture::Nearest : QSGTexture::Linear);
    node->setRect(std::round(-visible_rect.left() * scale), std::round(-visible_rect.top() * scale), std::round(m_image.width() * scale),
                  std::round(m_image.height() * scale));
    return node;
}

void ImageItem::wheelEvent(QWheelEvent* event) {
    if (event == nullptr) {
        return;
    }
    Q_EMIT activated();
    const double multiplier = event->angleDelta().y() > 0 ? 1.2 : (1.0 / 1.2);
    zoom_at(event->position(), multiplier);
    event->accept();
}

void ImageItem::mousePressEvent(QMouseEvent* event) {
    if (event == nullptr || event->button() != Qt::LeftButton) {
        QQuickItem::mousePressEvent(event);
        return;
    }
    Q_EMIT activated();
    m_dragging = true;
    m_last_drag_position = event->position();
    event->accept();
}

void ImageItem::mouseMoveEvent(QMouseEvent* event) {
    if (event == nullptr || !m_dragging) {
        QQuickItem::mouseMoveEvent(event);
        return;
    }

    const QPointF delta = event->position() - m_last_drag_position;
    m_last_drag_position = event->position();
    pan_by(delta);
    event->accept();
}

void ImageItem::mouseReleaseEvent(QMouseEvent* event) {
    if (event != nullptr && event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
        return;
    }
    QQuickItem::mouseReleaseEvent(event);
}

QRectF ImageItem::visible_image_rect() const {
    const double zoom = std::max(0.01, m_view_state.zoom_factor());
    const QSizeF viewport = m_view_state.viewport_size();
    const QSizeF visible_size(viewport.width() / zoom, viewport.height() / zoom);
    const QPointF center = m_view_state.image_center();
    return QRectF(center.x() - visible_size.width() / 2.0, center.y() - visible_size.height() / 2.0, visible_size.width(),
                  visible_size.height());
}

void ImageItem::request_decode(quint64 generation, QString source_path, bool apply_color_profile) {
    QPointer<ImageItem> item_guard(this);

    decode_thread_pool().start([item_guard, generation, source_path = std::move(source_path), apply_color_profile]() {
        QImage image;
        QString error_text;
        try {
            image = ImageSource(source_path).decode(apply_color_profile);
        } catch (const std::exception& ex) {
            error_text = QString::fromUtf8(ex.what());
        }

        if (item_guard == nullptr) {
            return;
        }

        QMetaObject::invokeMethod(
            item_guard,
            [item_guard, generation, source_path, image, error_text]() {
                if (item_guard == nullptr) {
                    return;
                }
                if (generation != item_guard->m_load_generation || !item_guard->m_input_image.isNull()) {
                    return;
                }

                if (image.isNull()) {
                    qWarning().noquote() << "Failed to decode image" << source_path << ":"
                                         << (error_text.isEmpty() ? QStringLiteral("empty image") : error_text);
                    return;
                }

                item_guard->m_image = image;
                item_guard->update();
            },
            Qt::QueuedConnection);
    });
}
