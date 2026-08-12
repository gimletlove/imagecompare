#pragma once

#include <QtQmlIntegration/qqmlintegration.h>

#include <QImage>
#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QtGlobal>

#include "core/ViewState.h"
#include "core/WorkspaceModel.h"

class ImageItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString source_path READ source_path WRITE set_source_path)
    Q_PROPERTY(QImage input_image READ input_image WRITE set_input_image)
    Q_PROPERTY(int color_mode READ color_mode WRITE set_color_mode)
    Q_PROPERTY(double zoom_factor READ zoom_factor WRITE set_zoom_factor NOTIFY zoom_factor_changed)
    Q_PROPERTY(QPointF image_center READ image_center WRITE set_image_center NOTIFY image_center_changed)
    Q_PROPERTY(QSize image_pixel_size READ image_pixel_size NOTIFY image_pixel_size_changed)

   public:
    explicit ImageItem(QQuickItem* parent = nullptr);

    [[nodiscard]] QString source_path() const;
    void set_source_path(QString path);
    [[nodiscard]] QImage input_image() const;
    void set_input_image(QImage image);
    [[nodiscard]] int color_mode() const noexcept;
    void set_color_mode(int mode);

    [[nodiscard]] double zoom_factor() const noexcept;
    void set_zoom_factor(double value);
    [[nodiscard]] QPointF image_center() const noexcept;
    void set_image_center(const QPointF& center_point);
    [[nodiscard]] QSize image_pixel_size() const noexcept;
    Q_INVOKABLE void zoom_at(const QPointF& viewport_point, double zoom_multiplier);
    Q_INVOKABLE void pan_by(const QPointF& viewport_delta);
    Q_INVOKABLE void set_best_fit();
    [[nodiscard]] Q_INVOKABLE double best_fit_zoom() const noexcept;

   Q_SIGNALS:
    void zoom_factor_changed();
    void image_center_changed();
    void image_pixel_size_changed();
    void activated();

   protected:
    QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& new_geometry, const QRectF& old_geometry) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

   private:
    void clear_image();
    void load_effective_image();
    void request_decode(quint64 generation, QString source_path, bool apply_color_profile);
    [[nodiscard]] QRectF visible_image_rect() const;
    [[nodiscard]] QPointF viewport_point_in_pixels(const QPointF& point) const noexcept;
    void update_viewport_size();

    QString m_source_path;
    QImage m_input_image;
    ColorMode m_color_mode = ColorMode::Faithful;
    QSize m_pixel_size;
    ViewState m_view_state;
    QImage m_image;
    quint64 m_load_generation = 0;
    qreal m_device_pixel_ratio = 1.0;
    bool m_dragging = false;
    QPointF m_last_drag_position;
};
