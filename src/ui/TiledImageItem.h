#pragma once

#include <QtQmlIntegration/qqmlintegration.h>

#include <QHash>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QQuickItem>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QSize>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <cstddef>

#include "core/ImageSource.h"
#include "core/TilePyramid.h"
#include "core/ViewState.h"
#include "core/WorkspaceDocument.h"

struct TileTextureKey {
    int display_mode = 0;
    int level = 0;
    QPoint tile_index;

    friend bool operator==(const TileTextureKey&, const TileTextureKey&) = default;
};

Q_ALWAYS_INLINE std::size_t qHash(const TileTextureKey& key, std::size_t seed = 0) noexcept {
    return qHashMulti(seed, key.display_mode, key.level, key.tile_index.x(), key.tile_index.y());
}

struct TileRenderRequestKey {
    quint64 generation = 0;
    int level = 0;
    QPoint tile_index;

    friend bool operator==(const TileRenderRequestKey&, const TileRenderRequestKey&) = default;
};

Q_ALWAYS_INLINE std::size_t qHash(const TileRenderRequestKey& key, std::size_t seed = 0) noexcept {
    return qHashMulti(seed, key.generation, key.level, key.tile_index.x(), key.tile_index.y());
}

class TiledImageItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString image_path READ image_path WRITE set_image_path NOTIFY image_path_changed)
    Q_PROPERTY(int display_mode READ display_mode WRITE set_display_mode NOTIFY display_mode_changed)
    Q_PROPERTY(QRectF viewport_rect READ viewport_rect WRITE set_viewport_rect NOTIFY viewport_rect_changed)
    Q_PROPERTY(double zoom_factor READ zoom_factor WRITE set_zoom_factor NOTIFY zoom_factor_changed)
    Q_PROPERTY(QPointF image_center READ image_center WRITE set_image_center NOTIFY image_center_changed)
    Q_PROPERTY(QSize image_pixel_size READ image_pixel_size NOTIFY image_pixel_size_changed)

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
    Q_INVOKABLE void zoom_at(const QPointF& viewport_point, double zoom_multiplier);
    Q_INVOKABLE void pan_by(const QPointF& viewport_delta);
    Q_INVOKABLE void set_best_fit();
    [[nodiscard]] Q_INVOKABLE double best_fit_zoom() const noexcept;

   Q_SIGNALS:
    void image_path_changed();
    void display_mode_changed();
    void viewport_rect_changed();
    void zoom_factor_changed();
    void image_center_changed();
    void image_pixel_size_changed();
    void activated();

   protected:
    QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

   private:
    void clear_tile_state();
    [[nodiscard]] QRectF visible_image_rect() const;
    [[nodiscard]] QRectF prefetch_image_rect(const QRectF& visible_rect) const;
    [[nodiscard]] QRect tile_image_rect(int level, const QPoint& tile_index) const;
    [[nodiscard]] QSize preview_output_size() const;
    [[nodiscard]] static TileRenderRequestKey tile_request_key(quint64 generation, int level, const QPoint& tile_index);
    void request_tile_render(quint64 generation, QString image_path, int display_mode, RenderSpec spec, int level, QPoint tile_index,
                             QRect image_rect);
    void request_preview_render(quint64 generation, QString image_path, int display_mode, RenderSpec spec, QRect image_rect,
                                QSize output_size);
    void schedule_requested_tiles_update(int max_missing_tiles_per_pass);
    void update_requested_tiles(int max_missing_tiles_per_pass);

    QString m_image_path;
    int m_display_mode = static_cast<int>(DisplayMode::Faithful);
    QSize m_image_size;
    QRectF m_viewport_rect;
    ViewState m_view_state;
    TilePyramid m_tile_pyramid;
    QHash<TileTextureKey, QImage> m_texture_cache;
    QSet<TileRenderRequestKey> m_pending_tile_requests;
    QSet<TileRenderRequestKey> m_failed_tile_requests;
    QVector<QPoint> m_active_tile_indices;
    quint64 m_scene_generation = 0;
    quint64 m_preview_generation = 0;
    quint64 m_pending_preview_generation = 0;
    quint64 m_failed_preview_generation = 0;
    QImage m_preview_image;
    QRect m_preview_image_rect;
    bool m_tile_update_scheduled = false;
    int m_pending_tile_budget = 0;
    bool m_missing_tiles_pending = false;
    bool m_dragging = false;
    QPointF m_last_drag_position;
};
