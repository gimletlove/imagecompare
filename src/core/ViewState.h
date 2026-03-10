#pragma once

#include <QPointF>
#include <QSizeF>

class ViewState {
   public:
    void set_zoom_factor(double value);
    void center_on(QPointF image_point);
    void set_viewport_size(QSizeF size);
    void set_image_size(QSizeF size);
    void set_best_fit_active(bool active) noexcept;

    [[nodiscard]] double zoom_factor() const noexcept;
    [[nodiscard]] QPointF image_center() const noexcept;
    [[nodiscard]] QSizeF viewport_size() const noexcept;
    [[nodiscard]] QSizeF image_size() const noexcept;
    [[nodiscard]] bool best_fit_active() const noexcept;
    [[nodiscard]] QPointF viewport_center() const noexcept;
    [[nodiscard]] QPointF viewport_to_image_point(const QPointF& viewport_point) const;
    void pan_by_viewport_delta(const QPointF& viewport_delta);
    [[nodiscard]] double best_fit_zoom() const noexcept;

   private:
    void clamp_center_to_image_bounds();

    double m_zoom_factor = 1.0;
    QPointF m_image_center;
    QSizeF m_viewport_size;
    QSizeF m_image_size;
    bool m_best_fit_active = false;
};
