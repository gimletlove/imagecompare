#pragma once

#include <QImage>
#include <QSize>
#include <QString>

[[nodiscard]] QString canonical_image_path(const QString& path);
[[nodiscard]] QSize probe_image_size(const QString& path);
[[nodiscard]] QImage decode_image(const QString& path, bool apply_color_profile);
