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
        if (path.isEmpty()) {
            fail(QStringLiteral("open"), path, QStringLiteral("path is empty"));
        }

        const QFileInfo file_info(path);
        if (!file_info.exists()) {
            fail(QStringLiteral("open"), path, QStringLiteral("file does not exist"));
        }
        if (file_info.isDir()) {
            fail(QStringLiteral("open"), path, QStringLiteral("path is a directory"));
        }
        if (!file_info.isFile()) {
            fail(QStringLiteral("open"), path, QStringLiteral("path is not a regular file"));
        }
    }

    QString reader_error(const QImageReader& reader) {
        const QString detail = reader.errorString();
        return detail.isEmpty() ? QStringLiteral("unsupported or invalid image data") : detail;
    }
}  // namespace

ImageSource::ImageSource(const QString& path) : m_path(normalized_path(path)) {}

const QString& ImageSource::path() const noexcept { return m_path; }

QSize ImageSource::pixel_size() const {
    validate_path(m_path);

    QImageReader reader(m_path);
    configure_reader(reader);
    if (!reader.canRead()) {
        fail(QStringLiteral("probe"), m_path, reader_error(reader));
    }

    QSize size = reader.size();
    if (!size.isValid() || size.isEmpty()) {
        fail(QStringLiteral("probe"), m_path, reader_error(reader));
    }
    if (reader.transformation().testFlag(QImageIOHandler::TransformationRotate90)) {
        size.transpose();
    }
    return size;
}

QImage ImageSource::decode(bool apply_color_profile) const {
    validate_path(m_path);

    QImageReader reader(m_path);
    configure_reader(reader);
    if (!reader.canRead()) {
        fail(QStringLiteral("decode"), m_path, reader_error(reader));
    }

    QImage image = reader.read();
    if (image.isNull()) {
        fail(QStringLiteral("decode"), m_path, reader_error(reader));
    }

    const QColorSpace source_color_space = image.colorSpace();
    if (apply_color_profile && source_color_space.isValid()) {
        QImage converted = image.convertedToColorSpace(QColorSpace::SRgb);
        if (converted.isNull()) {
            fail(QStringLiteral("convert color profile for"), m_path, QStringLiteral("conversion to sRGB failed"));
        }
        image = std::move(converted);
    } else {
        image.setColorSpace(QColorSpace::SRgb);
    }
    return image;
}

QString ImageSource::normalized_path(const QString& path) {
    const QFileInfo file_info(path);
    const QString canonical_path = file_info.canonicalFilePath();
    if (!canonical_path.isEmpty()) {
        return canonical_path;
    }
    return file_info.exists() ? file_info.absoluteFilePath() : path;
}
