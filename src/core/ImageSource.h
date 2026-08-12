#pragma once

#include <QImage>
#include <QSize>
#include <QString>

class ImageSource {
   public:
    explicit ImageSource(const QString& path);

    [[nodiscard]] const QString& path() const noexcept;
    [[nodiscard]] QSize pixel_size() const;
    [[nodiscard]] QImage decode(bool apply_color_profile) const;

   private:
    [[nodiscard]] static QString normalized_path(const QString& path);

    QString m_path;
};
