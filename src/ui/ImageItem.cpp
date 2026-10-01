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
    constexpr double k_min_zoom_factor = 0.01;
    constexpr double k_max_zoom_factor = 50.0;  // 5000%

    struct ImageTextureNode final : public QSGSimpleTextureNode {
        qint64 image_cache_key = 0;
    };

    struct DecodeThreadPool : QThreadPool {
        DecodeThreadPool() { setMaxThreadCount(std::clamp(QThread::idealThreadCount() / 2, 1, 8)); }
    };

    QThreadPool& decode_thread_pool() {
        static DecodeThreadPool pool;
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
    Q_EMIT snapshot_changed();
    update();
}

void ImageItem::set_source_path(QString path) {
    if (path == m_source_path) {
        return;
    }
    m_source_path = std::move(path);
    ++m_load_generation;
    if (isComponentComplete() && m_input_image.isNull()) {
        load_effective_image();
    }
}

void ImageItem::set_input_image(QImage image) {
    if (image.cacheKey() == m_input_image.cacheKey()) {
        return;
    }
    m_input_image = std::move(image);
    ++m_load_generation;
    if (isComponentComplete()) {
        load_effective_image();
    }
}

void ImageItem::set_apply_color_profile(bool apply) {
    if (apply == m_apply_color_profile) {
        return;
    }

    m_apply_color_profile = apply;
    if (m_input_image.isNull() && !m_source_path.isEmpty() && m_pixel_size.isValid()) {
        ++m_load_generation;
        clear_image();
        request_decode(m_load_generation, m_source_path, m_apply_color_profile);
    }
}

void ImageItem::componentComplete() {
    QQuickItem::componentComplete();
    load_effective_image();
}

void ImageItem::load_effective_image() {
    const QSize previous_pixel_size = m_pixel_size;
    clear_image();
    m_pixel_size = {};

    if (!m_input_image.isNull()) {
        m_image = m_input_image;
        m_pixel_size = m_image.size();
        Q_EMIT snapshot_changed();
    } else if (!m_source_path.isEmpty()) {
        try {
            m_pixel_size = probe_image_size(m_source_path);
            request_decode(m_load_generation, m_source_path, m_apply_color_profile);
        } catch (const std::exception& ex) {
            qWarning().noquote() << "Failed to load image" << m_source_path << ":" << ex.what();
        }
    }

    center_on(m_pixel_size.isValid() ? QPointF(m_pixel_size.width() / 2.0, m_pixel_size.height() / 2.0) : QPointF{});
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
    center_on(m_image_center);
    if (m_image_center.isNull() && m_pixel_size.isValid()) {
        center_on(QPointF(m_pixel_size.width() / 2.0, m_pixel_size.height() / 2.0));
        Q_EMIT image_center_changed();
    }
    update();
}

QPointF ImageItem::viewport_center() const { return QPointF(width(), height()) * m_device_pixel_ratio / 2.0; }

QPointF ImageItem::viewport_to_image_point(QPointF viewport_point) const {
    return m_image_center + (viewport_point - viewport_center()) / m_zoom_factor;
}

void ImageItem::apply_zoom(double value) {
    if (std::isfinite(value)) {
        m_zoom_factor = std::clamp(value, k_min_zoom_factor, k_max_zoom_factor);
    }
    center_on(m_image_center);
}

void ImageItem::center_on(QPointF image_point) {
    m_image_center = image_point;
    if (m_pixel_size.isEmpty()) {
        return;
    }
    const QPointF half_visible = viewport_center() / m_zoom_factor;
    const double min_x = std::min(half_visible.x(), m_pixel_size.width() / 2.0);
    const double min_y = std::min(half_visible.y(), m_pixel_size.height() / 2.0);
    m_image_center.setX(std::clamp(image_point.x(), min_x, std::max(min_x, m_pixel_size.width() - half_visible.x())));
    m_image_center.setY(std::clamp(image_point.y(), min_y, std::max(min_y, m_pixel_size.height() - half_visible.y())));
}

double ImageItem::zoom_factor() const noexcept { return m_zoom_factor; }

void ImageItem::set_zoom_factor(double value) {
    const double previous = m_zoom_factor;
    apply_zoom(value);
    if (qFuzzyCompare(previous, m_zoom_factor)) {
        return;
    }
    update();
    Q_EMIT zoom_factor_changed();
}

QPointF ImageItem::image_center() const noexcept { return m_image_center; }

void ImageItem::set_image_center(const QPointF& center_point) {
    const QPointF previous_center = m_image_center;
    center_on(center_point);
    if (m_image_center == previous_center) {
        return;
    }
    update();
    Q_EMIT image_center_changed();
}

QSize ImageItem::image_pixel_size() const noexcept { return m_pixel_size; }

void ImageItem::zoom_at(const QPointF& viewport_point, double zoom_multiplier) {
    const QPointF previous_center = m_image_center;
    const double previous_zoom = m_zoom_factor;
    const QPointF viewport_pixels = viewport_point * m_device_pixel_ratio;
    const QPointF anchor = viewport_to_image_point(viewport_pixels);
    apply_zoom(previous_zoom * zoom_multiplier);
    center_on(anchor - (viewport_pixels - viewport_center()) / m_zoom_factor);
    update();
    if (m_image_center != previous_center) {
        Q_EMIT image_center_changed();
    }
    if (!qFuzzyCompare(previous_zoom, m_zoom_factor)) {
        Q_EMIT zoom_factor_changed();
    }
}

void ImageItem::pan_by(const QPointF& viewport_delta) {
    const QPointF previous_center = m_image_center;
    center_on(m_image_center - viewport_delta * m_device_pixel_ratio / m_zoom_factor);
    if (m_image_center == previous_center) {
        return;
    }
    update();
    Q_EMIT image_center_changed();
}

void ImageItem::set_best_fit() {
    const QPointF previous_center = m_image_center;
    const double previous_zoom = m_zoom_factor;
    apply_zoom(best_fit_zoom());
    if (m_pixel_size.isValid()) {
        center_on(QPointF(m_pixel_size.width() / 2.0, m_pixel_size.height() / 2.0));
    }
    update();
    if (!qFuzzyCompare(previous_zoom, m_zoom_factor)) {
        Q_EMIT zoom_factor_changed();
    }
    if (m_image_center != previous_center) {
        Q_EMIT image_center_changed();
    }
}

double ImageItem::best_fit_zoom() const noexcept {
    const QPointF viewport = viewport_center() * 2.0;
    if (viewport.x() <= 0.0 || viewport.y() <= 0.0 || m_pixel_size.isEmpty()) {
        return 1.0;
    }
    return std::clamp(std::min(viewport.x() / m_pixel_size.width(), viewport.y() / m_pixel_size.height()), k_min_zoom_factor,
                      k_max_zoom_factor);
}

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

    const double zoom = m_zoom_factor;
    const QPointF top_left = viewport_to_image_point({});
    const double scale = zoom / m_device_pixel_ratio;
    node->setFiltering(zoom >= 1.0 ? QSGTexture::Nearest : QSGTexture::Linear);
    node->setRect(std::round(-top_left.x() * scale), std::round(-top_left.y() * scale), std::round(m_image.width() * scale),
                  std::round(m_image.height() * scale));
    return node;
}

void ImageItem::wheelEvent(QWheelEvent* event) {
    Q_EMIT activated();
    const double multiplier = event->angleDelta().y() > 0 ? 1.2 : (1.0 / 1.2);
    zoom_at(event->position(), multiplier);
    event->accept();
}

void ImageItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QQuickItem::mousePressEvent(event);
        return;
    }
    Q_EMIT activated();
    m_dragging = true;
    m_last_drag_position = event->position();
    event->accept();
}

void ImageItem::mouseMoveEvent(QMouseEvent* event) {
    if (!m_dragging) {
        QQuickItem::mouseMoveEvent(event);
        return;
    }

    const QPointF delta = event->position() - m_last_drag_position;
    m_last_drag_position = event->position();
    pan_by(delta);
    event->accept();
}

void ImageItem::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
        return;
    }
    QQuickItem::mouseReleaseEvent(event);
}

void ImageItem::request_decode(quint64 generation, QString source_path, bool apply_color_profile) {
    QPointer<ImageItem> item_guard(this);

    decode_thread_pool().start([item_guard, generation, source_path = std::move(source_path), apply_color_profile]() {
        if (item_guard == nullptr) {
            return;
        }

        QImage image;
        QString error_text;
        try {
            image = decode_image(source_path, apply_color_profile);
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
                Q_EMIT item_guard->snapshot_changed();
                item_guard->update();
            },
            Qt::QueuedConnection);
    });
}
