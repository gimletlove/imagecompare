#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <vips/vips8>

struct RenderSpec {
    bool ignore_color_profile = false;
};

class ImageSource {
   public:
    explicit ImageSource(const QString& path);

    [[nodiscard]] static bool supported_image_path(const QString& path);
    [[nodiscard]] static vips::VImage load_for_render(const QString& path, const RenderSpec& spec);

    [[nodiscard]] const QString& path() const noexcept;
    [[nodiscard]] QSize pixel_size() const;
    [[nodiscard]] QImage render(const RenderSpec& spec) const;

   private:
    [[nodiscard]] static QString normalized_path_for_source(const QString& path);
    void ensure_loaded() const;

    QString m_path;
    mutable bool m_loaded = false;
    mutable QSize m_pixel_size;
};
