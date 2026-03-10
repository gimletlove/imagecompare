#pragma once

#include <QPoint>
#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QThreadPool>
#include <QVector>

#include "core/ImageSource.h"
#include "core/TilePyramid.h"
#include "core/ViewState.h"
#include "ui/TileTextureCache.h"

class TiledImageItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QString image_path READ image_path WRITE set_image_path NOTIFY image_path_changed)
    Q_PROPERTY(int display_mode READ display_mode WRITE set_display_mode NOTIFY display_mode_changed)
    Q_PROPERTY(QRectF viewport_rect READ viewport_rect WRITE set_viewport_rect NOTIFY viewport_rect_changed)
    Q_PROPERTY(double zoom_factor READ zoom_factor WRITE set_zoom_factor NOTIFY zoom_factor_changed)
    Q_PROPERTY(QPointF image_center READ image_center WRITE set_image_center NOTIFY image_center_changed)
    Q_PROPERTY(QSize image_pixel_size READ image_pixel_size NOTIFY image_pixel_size_changed)
    Q_PROPERTY(bool has_renderable_content READ has_renderable_content NOTIFY has_renderable_content_changed)

   public:
    explicit TiledImageItem(QQuickItem* parent = nullptr);

    [[nodiscard]] QString image_path() const;
    void set_image_path(QString path);
    [[nodiscard]] int display_mode() const noexcept;
    void set_display_mode(int mode);

    [[nodiscard]] QRectF viewport_rect() const noexcept;
    void set_viewport_rect(const QRectF& rect);

    [[nodiscard]] double zoom_factor() const noexcept;
    void set_zoom_factor(double value);
    [[nodiscard]] QPointF image_center() const noexcept;
    void set_image_center(const QPointF& center_point);
    [[nodiscard]] QSize image_pixel_size() const noexcept;
    [[nodiscard]] bool has_renderable_content() const noexcept;
    Q_INVOKABLE void zoom_at(const QPointF& viewport_point, double zoom_multiplier);
    Q_INVOKABLE void pan_by(const QPointF& viewport_delta);
    Q_INVOKABLE void set_best_fit();
    [[nodiscard]] Q_INVOKABLE double best_fit_zoom() const noexcept;

    [[nodiscard]] const QVector<QPoint>& last_requested_tiles() const noexcept;

   Q_SIGNALS:
    void image_path_changed();
    void display_mode_changed();
    void viewport_rect_changed();
    void zoom_factor_changed();
    void image_center_changed();
    void image_pixel_size_changed();
    void has_renderable_content_changed();

   protected:
    QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

   private:
    void clear_tile_state();
    [[nodiscard]] RenderSpec current_render_spec() const noexcept;
    [[nodiscard]] QRectF visible_image_rect() const;
    [[nodiscard]] QRect tile_image_rect(int level, const QPoint& tile_index) const;
    [[nodiscard]] static QString tile_request_key(quint64 generation, const QString& image_path, int display_mode, int level,
                                                  const QPoint& tile_index);
    void request_tile_render(quint64 generation, QString image_path, int display_mode, RenderSpec spec, int level, QPoint tile_index,
                             QRect image_rect);
    void schedule_requested_tiles_update(int max_missing_tiles_per_pass);
    void update_requested_tiles(int max_missing_tiles_per_pass = 6);

    QString m_image_path;
    int m_display_mode = static_cast<int>(DisplayMode::Faithful);
    QSize m_image_size;
    QRectF m_viewport_rect;
    ViewState m_view_state;
    TilePyramid m_tile_pyramid;
    TileTextureCache m_texture_cache;
    QSet<QString> m_pending_tile_requests;
    QSet<QString> m_failed_tile_requests;
    QVector<QPoint> m_last_requested_tiles;
    quint64 m_scene_generation = 0;
    bool m_has_renderable_content = false;
    bool m_tile_update_scheduled = false;
    int m_pending_tile_budget = 0;
    bool m_missing_tiles_pending = false;
    bool m_tile_fill_scheduled = false;
    bool m_dragging = false;
    QPointF m_last_drag_position;
    QThreadPool m_tile_thread_pool;
};
