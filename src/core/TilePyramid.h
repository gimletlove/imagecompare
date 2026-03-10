#pragma once

#include <QRect>
#include <QRectF>
#include <QSize>
#include <QVector>

class TilePyramid {
   public:
    explicit TilePyramid(QSize base_size = {}, int tile_size = 256);

    [[nodiscard]] int level_for_zoom(double zoom_factor) const;
    [[nodiscard]] QRect visible_tile_bounds(int level, const QRectF& image_rect) const;
    [[nodiscard]] int level_count() const noexcept;
    [[nodiscard]] int tile_size() const noexcept;

   private:
    static int clamped_tile_index(int value, int max_inclusive);

    QSize m_base_size;
    int m_tile_size = 256;
    QVector<QSize> m_level_sizes;
};
