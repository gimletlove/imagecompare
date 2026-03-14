#include "core/ImageSource.h"

#include <vips/vips.h>

#include <QFileInfo>
#include <QHash>
#include <algorithm>
#include <deque>
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
        });
    }

    struct RenderCacheKey {
        QString normalized_path;
        bool ignore_color_profile = false;

        friend bool operator==(const RenderCacheKey&, const RenderCacheKey&) = default;
    };

    uint qHash(const RenderCacheKey& key, uint seed = 0) noexcept {
        return qHashMulti(seed, key.normalized_path, key.ignore_color_profile);
    }

    vips::VImage load_image_for_spec(const QString& path, const RenderSpec& spec) {
        vips::VImage image = vips::VImage::new_from_file(path.toUtf8().constData());
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

    QImage to_q_image(vips::VImage image) {
        if (image.width() <= 0 || image.height() <= 0) {
            return {};
        }

        if (image.interpretation() == VIPS_INTERPRETATION_CMYK) {
            image = image.colourspace(VIPS_INTERPRETATION_sRGB);
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

        size_t byte_count = static_cast<size_t>(image.width()) * static_cast<size_t>(image.height()) * static_cast<size_t>(image.bands());
        void* memory = image.write_to_memory(&byte_count);
        if (memory == nullptr || byte_count == 0) {
            if (memory != nullptr) {
                g_free(memory);
            }
            return {};
        }

        QImage result;
        switch (image.bands()) {
            case 1: {
                QImage wrapped(static_cast<const uchar*>(memory), image.width(), image.height(), image.width(), QImage::Format_Grayscale8);
                result = wrapped.copy();
                break;
            }
            case 3: {
                QImage wrapped(static_cast<const uchar*>(memory), image.width(), image.height(), image.width() * 3, QImage::Format_RGB888);
                result = wrapped.copy();
                break;
            }
            case 4: {
                QImage wrapped(static_cast<const uchar*>(memory), image.width(), image.height(), image.width() * 4,
                               QImage::Format_RGBA8888);
                result = wrapped.copy();
                break;
            }
            default:
                break;
        }

        g_free(memory);
        return result;
    }

    RenderCacheKey make_render_cache_key(const QString& normalized_path, const RenderSpec& spec) {
        return RenderCacheKey{
            .normalized_path = normalized_path,
            .ignore_color_profile = spec.ignore_color_profile,
        };
    }

    std::mutex& render_cache_mutex() {
        static std::mutex cache_mutex;
        return cache_mutex;
    }

    QHash<RenderCacheKey, vips::VImage>& render_cache_by_key() {
        static QHash<RenderCacheKey, vips::VImage> cache_by_key;
        return cache_by_key;
    }

    std::deque<RenderCacheKey>& render_cache_recency() {
        static std::deque<RenderCacheKey> recency;
        return recency;
    }

    vips::VImage cached_image_for_spec(const QString& normalized_path, const RenderSpec& spec) {
        constexpr int k_max_cached_images = 10;

        const RenderCacheKey cache_key = make_render_cache_key(normalized_path, spec);
        {
            std::lock_guard lock(render_cache_mutex());
            auto& cache_by_key = render_cache_by_key();
            auto& recency = render_cache_recency();
            const auto it = cache_by_key.constFind(cache_key);
            if (it != cache_by_key.cend()) {
                const auto found = std::find(recency.begin(), recency.end(), cache_key);
                if (found != recency.end()) {
                    recency.erase(found);
                }
                recency.push_front(cache_key);
                return it.value();
            }
        }

        vips::VImage loaded_image = load_image_for_spec(cache_key.normalized_path, spec);

        {
            std::lock_guard lock(render_cache_mutex());
            auto& cache_by_key = render_cache_by_key();
            auto& recency = render_cache_recency();
            const auto existing_entry = cache_by_key.constFind(cache_key);
            if (existing_entry != cache_by_key.cend()) {
                return existing_entry.value();
            }
            cache_by_key.insert(cache_key, loaded_image);
            recency.push_front(cache_key);
            while (static_cast<int>(recency.size()) > k_max_cached_images) {
                const RenderCacheKey evicted = recency.back();
                recency.pop_back();
                cache_by_key.remove(evicted);
            }
        }

        return loaded_image;
    }
}  // namespace

ImageSource::ImageSource(QString path) : m_path(normalized_path_for_source(path)) {}

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
    return load_image_for_spec(path, spec);
}

const QString& ImageSource::path() const noexcept { return m_path; }

QSize ImageSource::pixel_size() const {
    ensure_loaded();
    return m_pixel_size;
}

RenderSpec ImageSource::render_spec(DisplayMode mode) const noexcept {
    if (mode == DisplayMode::StrictRaw) {
        return {
            .ignore_color_profile = true,
        };
    }
    return {};
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
    vips::VImage image = vips::VImage::new_from_file(m_path.toUtf8().constData());

    m_pixel_size = QSize(image.width(), image.height());
    m_loaded = true;
}

QString ImageSource::normalized_path_for_source(const QString& path) {
    const QFileInfo file_info(path);
    const QString canonical_path = file_info.canonicalFilePath();
    if (!canonical_path.isEmpty()) {
        return canonical_path;
    }
    if (file_info.exists()) {
        return file_info.absoluteFilePath();
    }
    return path;
}
