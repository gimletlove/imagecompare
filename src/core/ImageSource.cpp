#include "core/ImageSource.h"

#include <vips/vips.h>

#include <QFileInfo>
#include <QThread>
#include <algorithm>
#include <cstddef>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <vips/vips8>

namespace {
    void ensure_vips_initialized() {
        static std::once_flag init_flag;
        std::call_once(init_flag, []() {
            if (VIPS_INIT("imagecompare") != 0) {
                throw std::runtime_error("failed to initialize libvips");
            }
            if (!qEnvironmentVariableIsSet("VIPS_CONCURRENCY")) {
                vips_concurrency_set(std::min(8, std::max(1, QThread::idealThreadCount())));
            }
        });
    }

    vips::VImage apply_render_spec(vips::VImage image, const RenderSpec& spec) {
        if (!spec.ignore_color_profile) {
            const bool has_embedded_icc_profile = image.get_typeof("icc-profile-data") != 0;
            if (has_embedded_icc_profile) {
                try {
                    image = image.icc_transform("srgb", vips::VImage::option()->set("embedded", true));
                } catch (const std::exception&) {
                    if (image.interpretation() != VIPS_INTERPRETATION_sRGB) {
                        image = image.colourspace(VIPS_INTERPRETATION_sRGB);
                    }
                }
            } else if (image.interpretation() != VIPS_INTERPRETATION_sRGB) {
                image = image.colourspace(VIPS_INTERPRETATION_sRGB);
            }
        }
        return image;
    }

    void free_vips_memory(void* memory) { g_free(memory); }

    QImage to_q_image(vips::VImage image) {
        if (image.width() <= 0 || image.height() <= 0) {
            return {};
        }

        if (image.interpretation() != VIPS_INTERPRETATION_sRGB && image.interpretation() != VIPS_INTERPRETATION_B_W) {
            try {
                image = image.colourspace(VIPS_INTERPRETATION_sRGB);
            } catch (const std::exception&) {
                // Unsupported color layouts fall back to band handling below.
            }
        }
        if (image.format() != VIPS_FORMAT_UCHAR) {
            image = image.cast(VIPS_FORMAT_UCHAR);
        }
        if (image.bands() == 2) {
            const vips::VImage gray = image.extract_band(0);
            const vips::VImage alpha = image.extract_band(1);
            image = gray.bandjoin(gray).bandjoin(gray).bandjoin(alpha);
        }
        if (image.bands() > 4) {
            image = image.extract_band(0, vips::VImage::option()->set("n", 4));
        }

        const qsizetype bytes_per_line = static_cast<qsizetype>(image.width()) * image.bands();
        const size_t expected_byte_count = static_cast<size_t>(bytes_per_line) * static_cast<size_t>(image.height());
        size_t byte_count = expected_byte_count;
        void* memory = image.write_to_memory(&byte_count);
        if (memory == nullptr || byte_count < expected_byte_count) {
            if (memory != nullptr) {
                g_free(memory);
            }
            return {};
        }

        QImage::Format format = QImage::Format_Invalid;
        switch (image.bands()) {
            case 1:
                format = QImage::Format_Grayscale8;
                break;
            case 3:
                format = QImage::Format_RGB888;
                break;
            case 4:
                format = QImage::Format_RGBA8888;
                break;
            default:
                break;
        }

        if (format == QImage::Format_Invalid) {
            g_free(memory);
            return {};
        }

        QImage result(static_cast<uchar*>(memory), image.width(), image.height(), bytes_per_line, format, free_vips_memory, memory);
        if (result.isNull()) {
            g_free(memory);
        }
        return result;
    }

}  // namespace

ImageSource::ImageSource(const QString& path) : m_path(normalized_path_for_source(path)) {}

bool ImageSource::supported_image_path(const QString& path) {
    if (path.isEmpty()) {
        return false;
    }
    ensure_vips_initialized();
    const QString normalized_path = normalized_path_for_source(path);
    if (normalized_path.isEmpty()) {
        return false;
    }
    return vips_foreign_find_load(normalized_path.toUtf8().constData()) != nullptr;
}

vips::VImage ImageSource::load_for_render(const QString& path, const RenderSpec& spec) {
    ensure_vips_initialized();
    vips::VImage image = vips::VImage::new_from_file(path.toUtf8().constData());
    return apply_render_spec(image.autorot(), spec);
}

const QString& ImageSource::path() const noexcept { return m_path; }

QSize ImageSource::pixel_size() const {
    ensure_loaded();
    return m_pixel_size;
}

QImage ImageSource::render(const RenderSpec& spec) const { return to_q_image(load_for_render(m_path, spec)); }

void ImageSource::ensure_loaded() const {
    if (m_loaded) {
        return;
    }

    ensure_vips_initialized();
    vips::VImage image = vips::VImage::new_from_file(m_path.toUtf8().constData()).autorot();

    m_pixel_size = QSize(image.width(), image.height());
    m_loaded = true;
}

QString ImageSource::normalized_path_for_source(const QString& path) {
    const QFileInfo file_info(path);
    QString canonical_path = file_info.canonicalFilePath();
    if (!canonical_path.isEmpty()) {
        return canonical_path;
    }
    if (file_info.exists()) {
        return file_info.absoluteFilePath();
    }
    return path;
}
