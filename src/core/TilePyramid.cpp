#include "core/TilePyramid.h"

#include <QtMath>
#include <algorithm>

TilePyramid::TilePyramid(QSize base_size, int tile_size) : m_base_size(base_size), m_tile_size(std::max(1, tile_size)) {
    if (!m_base_size.isValid()) {
        return;
    }

    QSize level_size = m_base_size;
    m_level_sizes.push_back(level_size);
    while (level_size.width() > 1 || level_size.height() > 1) {
        level_size = QSize(std::max(1, level_size.width() / 2), std::max(1, level_size.height() / 2));
        m_level_sizes.push_back(level_size);
    }
}

int TilePyramid::level_for_zoom(double zoom_factor) const {
    if (m_level_sizes.isEmpty() || zoom_factor >= 1.0) {
        return 0;
    }
    const int max_level = static_cast<int>(m_level_sizes.size()) - 1;
    const int level = qRound(qLn(1.0 / zoom_factor) / qLn(2.0));
    return std::clamp(level, 0, max_level);
}

QRect TilePyramid::visible_tile_bounds(int level, const QRectF& image_rect) const {
    if (m_level_sizes.isEmpty()) {
        return {};
    }

    const int max_level = static_cast<int>(m_level_sizes.size()) - 1;
    const int clamped_level = std::clamp(level, 0, max_level);
    const QSize level_size = m_level_sizes[clamped_level];
    const double scale = 1.0 / static_cast<double>(1 << clamped_level);
    const QRectF scaled_rect(image_rect.left() * scale, image_rect.top() * scale, image_rect.width() * scale, image_rect.height() * scale);

    const int max_tile_x = std::max(0, (level_size.width() - 1) / m_tile_size);
    const int max_tile_y = std::max(0, (level_size.height() - 1) / m_tile_size);

    const int left = clamped_tile_index(qFloor(scaled_rect.left() / m_tile_size), max_tile_x);
    const int top = clamped_tile_index(qFloor(scaled_rect.top() / m_tile_size), max_tile_y);
    const int right = clamped_tile_index(qFloor((scaled_rect.right()) / m_tile_size), max_tile_x);
    const int bottom = clamped_tile_index(qFloor((scaled_rect.bottom()) / m_tile_size), max_tile_y);

    return QRect(left, top, (right - left) + 1, (bottom - top) + 1);
}

int TilePyramid::level_count() const noexcept { return m_level_sizes.size(); }

int TilePyramid::tile_size() const noexcept { return m_tile_size; }

int TilePyramid::clamped_tile_index(int value, int max_inclusive) { return std::clamp(value, 0, std::max(0, max_inclusive)); }
