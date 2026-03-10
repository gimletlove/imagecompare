#include "core/ViewState.h"

#include <algorithm>

namespace {
    constexpr double k_min_zoom_factor = 0.01;
    constexpr double k_max_zoom_factor = 50.0;  // 5000%
}  // namespace

void ViewState::set_zoom_factor(double value) {
    m_zoom_factor = std::clamp(value, k_min_zoom_factor, k_max_zoom_factor);
    clamp_center_to_image_bounds();
    m_best_fit_active = false;
}

void ViewState::center_on(QPointF image_point) {
    m_image_center = image_point;
    clamp_center_to_image_bounds();
}

void ViewState::set_viewport_size(QSizeF size) {
    m_viewport_size = size;
    clamp_center_to_image_bounds();
}

void ViewState::set_image_size(QSizeF size) {
    m_image_size = size;
    clamp_center_to_image_bounds();
}

void ViewState::set_best_fit_active(bool active) noexcept { m_best_fit_active = active; }

double ViewState::zoom_factor() const noexcept { return m_zoom_factor; }

QPointF ViewState::image_center() const noexcept { return m_image_center; }

QSizeF ViewState::viewport_size() const noexcept { return m_viewport_size; }

QSizeF ViewState::image_size() const noexcept { return m_image_size; }

bool ViewState::best_fit_active() const noexcept { return m_best_fit_active; }

QPointF ViewState::viewport_center() const noexcept { return QPointF(m_viewport_size.width() / 2.0, m_viewport_size.height() / 2.0); }

QPointF ViewState::viewport_to_image_point(const QPointF& viewport_point) const {
    return image_center() + (viewport_point - viewport_center()) / zoom_factor();
}

void ViewState::pan_by_viewport_delta(const QPointF& viewport_delta) {
    if (zoom_factor() <= 0.0) {
        return;
    }

    center_on(image_center() - viewport_delta / zoom_factor());
    m_best_fit_active = false;
}

double ViewState::best_fit_zoom() const noexcept {
    if (m_viewport_size.width() <= 0.0 || m_viewport_size.height() <= 0.0) {
        return 1.0;
    }
    
    if (m_image_size.width() <= 0.0 || m_image_size.height() <= 0.0) {
        return 1.0;
    }

    const double fit_x = m_viewport_size.width() / m_image_size.width();
    const double fit_y = m_viewport_size.height() / m_image_size.height();
    return std::clamp(std::min(fit_x, fit_y), k_min_zoom_factor, k_max_zoom_factor);
}

void ViewState::clamp_center_to_image_bounds() {
    if (m_image_size.width() <= 0.0 || m_image_size.height() <= 0.0) {
        return;
    }

    const double zoom = std::max(k_min_zoom_factor, m_zoom_factor);
    const double half_visible_width = std::max(0.0, m_viewport_size.width() / zoom / 2.0);
    const double half_visible_height = std::max(0.0, m_viewport_size.height() / zoom / 2.0);

    const double min_center_x = std::min(half_visible_width, m_image_size.width() / 2.0);
    const double max_center_x = std::max(min_center_x, m_image_size.width() - half_visible_width);
    const double min_center_y = std::min(half_visible_height, m_image_size.height() / 2.0);
    const double max_center_y = std::max(min_center_y, m_image_size.height() - half_visible_height);

    m_image_center.setX(std::clamp(m_image_center.x(), min_center_x, max_center_x));
    m_image_center.setY(std::clamp(m_image_center.y(), min_center_y, max_center_y));
}
