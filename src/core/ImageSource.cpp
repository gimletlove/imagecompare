#include "core/ImageSource.h"

#include <QColorSpace>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <stdexcept>
#include <utility>

namespace {
    void configure_reader(QImageReader& reader) {
        reader.setDecideFormatFromContent(true);
        reader.setAutoTransform(true);
    }

    [[noreturn]] void fail(const QString& operation, const QString& path, const QString& detail) {
        throw std::runtime_error(QStringLiteral("Failed to %1 image '%2': %3").arg(operation, path, detail).toUtf8().toStdString());
    }

    void validate_path(const QString& path) {
        if (!QFileInfo(path).isFile()) {
            fail(QStringLiteral("open"), path, QStringLiteral("not a regular file"));
        }
    }

    QString reader_error(const QImageReader& reader) {
        const QString detail = reader.errorString();
        return detail.isEmpty() ? QStringLiteral("unsupported or invalid image data") : detail;
    }
}  // namespace

QSize probe_image_size(const QString& path) {
    validate_path(path);

    QImageReader reader(path);
    configure_reader(reader);
    if (!reader.canRead()) {
        fail(QStringLiteral("probe"), path, reader_error(reader));
    }

    QSize size = reader.size();
    if (!size.isValid() || size.isEmpty()) {
        fail(QStringLiteral("probe"), path, reader_error(reader));
    }
    if (reader.transformation().testFlag(QImageIOHandler::TransformationRotate90)) {
        size.transpose();
    }
    return size;
}

QImage decode_image(const QString& path, bool apply_color_profile) {
    validate_path(path);

    QImageReader reader(path);
    configure_reader(reader);
    if (!reader.canRead()) {
        fail(QStringLiteral("decode"), path, reader_error(reader));
    }

    QImage image = reader.read();
    if (image.isNull()) {
        fail(QStringLiteral("decode"), path, reader_error(reader));
    }

    const QColorSpace source_color_space = image.colorSpace();
    if (apply_color_profile && source_color_space.isValid()) {
        QImage converted = image.convertedToColorSpace(QColorSpace::SRgb);
        if (converted.isNull()) {
            fail(QStringLiteral("convert color profile for"), path, QStringLiteral("conversion to sRGB failed"));
        }
        image = std::move(converted);
    } else {
        image.setColorSpace(QColorSpace::SRgb);
    }
    return image;
}

QString canonical_image_path(const QString& path) {
    const QString canonical_path = QFileInfo(path).canonicalFilePath();
    return canonical_path.isEmpty() ? path : canonical_path;
}
