#include "core/ImageSource.h"

#include <vips/vips.h>

#include <QCache>
#include <QFileInfo>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vips/vips8>

namespace {
    constexpr int k_tile_cache_tile_size = 256;
    constexpr int k_tile_cache_max_tiles = 512;
    constexpr qsizetype k_max_cached_images = 10;

    void ensure_vips_initialized() {
        static std::once_flag init_flag;
        std::call_once(init_flag, []() {
            if (VIPS_INIT("imagecompare") != 0) {
                throw std::runtime_error("failed to initialize libvips");
            }
        });
    }

    struct RenderCacheKey {
        QString normalized_path;
        bool ignore_color_profile = false;

        friend bool operator==(const RenderCacheKey&, const RenderCacheKey&) = default;
    };

    std::size_t qHash(const RenderCacheKey& key, std::size_t seed = 0) noexcept {
        return qHashMulti(seed, key.normalized_path, key.ignore_color_profile);
    }

    vips::VImage add_random_access_tile_cache(const vips::VImage& image) {
        return image.tilecache(vips::VImage::option()
                                   ->set("tile_width", k_tile_cache_tile_size)
                                   ->set("tile_height", k_tile_cache_tile_size)
                                   ->set("max_tiles", k_tile_cache_max_tiles)
                                   ->set("threaded", true)
                                   ->set("persistent", true));
    }

    vips::VImage load_image_for_spec(const QString& path, const RenderSpec& spec, bool random_access) {
        vips::VImage image = random_access ? vips::VImage::new_from_file(path.toUtf8().constData(),
                                                                         vips::VImage::option()->set("access", VIPS_ACCESS_RANDOM))
                                           : vips::VImage::new_from_file(path.toUtf8().constData());
        image = image.autorot();
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

    std::mutex& render_cache_mutex() {
        static std::mutex cache_mutex;
        return cache_mutex;
    }

    QCache<RenderCacheKey, vips::VImage>& render_cache() {
        static QCache<RenderCacheKey, vips::VImage> cache(k_max_cached_images);
        return cache;
    }

    vips::VImage cached_image_for_spec(const QString& normalized_path, const RenderSpec& spec) {
        const RenderCacheKey cache_key{normalized_path, spec.ignore_color_profile};
        {
            std::lock_guard lock(render_cache_mutex());
            if (const auto* cached = render_cache().object(cache_key)) {
                return *cached;
            }
        }

        vips::VImage loaded_image = add_random_access_tile_cache(load_image_for_spec(cache_key.normalized_path, spec, true));

        {
            std::lock_guard lock(render_cache_mutex());
            if (const auto* cached = render_cache().object(cache_key)) {
                return *cached;
            }
            auto cached = std::make_unique<vips::VImage>(loaded_image);
            [[maybe_unused]] const bool inserted = render_cache().insert(cache_key, cached.get());
            [[maybe_unused]] auto* transferred = cached.release();
            Q_ASSERT(inserted && transferred != nullptr);
        }

        return loaded_image;
    }

    void drop_cached_image_for_path(const QString& normalized_path) {
        if (normalized_path.isEmpty()) {
            return;
        }

        std::lock_guard lock(render_cache_mutex());
        auto& cache = render_cache();
        for (const RenderCacheKey& key : cache.keys()) {
            if (key.normalized_path == normalized_path) {
                cache.remove(key);
            }
        }
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
    return load_image_for_spec(path, spec, false);
}

void ImageSource::drop_cached_render_data(const QString& path) { drop_cached_image_for_path(normalized_path_for_source(path)); }

const QString& ImageSource::path() const noexcept { return m_path; }

QSize ImageSource::pixel_size() const {
    ensure_loaded();
    return m_pixel_size;
}

QImage ImageSource::render_region(const QRect& image_rect, const RenderSpec& spec, const QSize& output_size) const {
    if (!image_rect.isValid()) {
        return {};
    }

    ensure_vips_initialized();
    vips::VImage image = cached_image_for_spec(m_path, spec);

    const QRect image_bounds(0, 0, image.width(), image.height());
    const QRect clipped_rect = image_rect.intersected(image_bounds);
    if (!clipped_rect.isValid()) {
        return {};
    }

    vips::VImage tile = image.crop(clipped_rect.x(), clipped_rect.y(), clipped_rect.width(), clipped_rect.height());
    if (output_size.isValid() && output_size != clipped_rect.size()) {
        const double scale_x = static_cast<double>(output_size.width()) / static_cast<double>(clipped_rect.width());
        const double scale_y = static_cast<double>(output_size.height()) / static_cast<double>(clipped_rect.height());
        tile = tile.resize(scale_x, vips::VImage::option()->set("vscale", scale_y));
    }

    return to_q_image(tile);
}

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
