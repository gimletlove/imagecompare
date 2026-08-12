#include "core/ImageComparison.h"

#include <rmgr/ssim.h>

#include <QMetaObject>
#include <QPointer>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "core/ImageSource.h"

namespace {
    constexpr int k_transparent_background = 127;
    constexpr int k_luma_red = 19595;
    constexpr int k_luma_green = 38470;
    constexpr int k_luma_blue = 7471;
    constexpr int k_luma_rounding = 32768;
    constexpr int k_heatmap_lut_size = 256;

    struct HeatmapColor {
        double red;
        double green;
        double blue;
    };

    constexpr std::array<HeatmapColor, 12> k_heatmap_palette = {
        HeatmapColor{0.0, 0.0, 0.0}, HeatmapColor{0.0, 0.0, 1.0}, HeatmapColor{0.0, 1.0, 1.0}, HeatmapColor{0.0, 1.0, 0.0},
        HeatmapColor{1.0, 1.0, 0.0}, HeatmapColor{1.0, 0.0, 0.0}, HeatmapColor{1.0, 0.0, 1.0}, HeatmapColor{0.5, 0.5, 1.0},
        HeatmapColor{1.0, 0.5, 0.5}, HeatmapColor{1.0, 1.0, 0.5}, HeatmapColor{1.0, 1.0, 1.0}, HeatmapColor{1.0, 1.0, 1.0},
    };

    [[nodiscard]] ComparisonResult fail(QString error) { return {.error = std::move(error), .heatmap = {}, .ssim = 0.0}; }

    [[nodiscard]] std::uint8_t heatmap_channel(double value) {
        return static_cast<std::uint8_t>(std::lround(std::sqrt(std::clamp(value, 0.0, 1.0)) * 255.0));
    }

    [[nodiscard]] const std::array<QRgb, k_heatmap_lut_size>& heatmap_lut() {
        static const std::array<QRgb, k_heatmap_lut_size> lut = [] {
            std::array<QRgb, k_heatmap_lut_size> result{};
            for (std::size_t index = 0; index < result.size(); ++index) {
                const double level = static_cast<double>(index) / static_cast<double>(result.size() - 1);
                const double position = level * static_cast<double>(k_heatmap_palette.size() - 1);
                const std::size_t palette_index = std::min(static_cast<std::size_t>(position), k_heatmap_palette.size() - 2);
                const double blend = position - static_cast<double>(palette_index);
                const HeatmapColor& low = k_heatmap_palette[palette_index];
                const HeatmapColor& high = k_heatmap_palette[palette_index + 1];
                result[index] = qRgb(heatmap_channel(low.red + (high.red - low.red) * blend),
                                     heatmap_channel(low.green + (high.green - low.green) * blend),
                                     heatmap_channel(low.blue + (high.blue - low.blue) * blend));
            }
            return result;
        }();
        return lut;
    }

    [[nodiscard]] QImage to_luminance(QImage image, const std::stop_token& token) {
        if (image.isNull()) {
            throw std::runtime_error("decoded image is empty");
        }

        image.convertTo(QImage::Format_ARGB32);
        if (image.isNull()) {
            throw std::runtime_error("could not normalize decoded image pixels");
        }

        QImage luminance(image.size(), QImage::Format_Grayscale8);
        if (luminance.isNull()) {
            throw std::runtime_error("could not allocate luminance image");
        }

        for (int y = 0; y < image.height(); ++y) {
            if (token.stop_requested()) {
                return {};
            }
            const auto* source = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            uchar* destination = luminance.scanLine(y);
            for (int x = 0; x < image.width(); ++x) {
                const QRgb pixel = source[x];
                const int luma =
                    (k_luma_red * qRed(pixel) + k_luma_green * qGreen(pixel) + k_luma_blue * qBlue(pixel) + k_luma_rounding) >> 16;
                destination[x] = static_cast<uchar>((qAlpha(pixel) * luma + (255 - qAlpha(pixel)) * k_transparent_background + 127) / 255);
            }
        }
        return luminance;
    }

    void colorize(QImage& heatmap, const std::stop_token& token) {
        const auto& lut = heatmap_lut();
        for (int y = 0; y < heatmap.height(); ++y) {
            if (token.stop_requested()) {
                return;
            }
            uchar* scanline = heatmap.scanLine(y);
            for (int x = 0; x < heatmap.width(); ++x) {
                uchar* slot = scanline + static_cast<qsizetype>(x) * static_cast<qsizetype>(sizeof(float));
                float pixel_ssim = 0.0F;
                std::memcpy(&pixel_ssim, slot, sizeof(pixel_ssim));
                if (!std::isfinite(pixel_ssim)) {
                    throw std::runtime_error("SSIM produced a non-finite map value");
                }

                const double difference = std::sqrt(std::clamp(1.0 - static_cast<double>(pixel_ssim), 0.0, 1.0));
                const auto palette_index = static_cast<std::size_t>(std::lround(difference * 255.0));
                const QRgb color = lut[palette_index];
                std::memcpy(slot, &color, sizeof(color));
            }
        }
    }

    [[nodiscard]] ComparisonResult compare_images(const ComparisonRequest& request, const std::stop_token& token) {
        static_assert(sizeof(float) == sizeof(QRgb));

        if (request.first_image_path.isEmpty() || request.second_image_path.isEmpty()) {
            throw std::invalid_argument("comparison request has empty image paths");
        }

        const bool apply_color_profile = request.color_mode == ColorMode::Faithful;
        QImage first_luminance;
        {
            QImage decoded = ImageSource(request.first_image_path).decode(apply_color_profile);
            if (token.stop_requested()) {
                return {};
            }
            first_luminance = to_luminance(std::move(decoded), token);
        }
        if (token.stop_requested()) {
            return {};
        }

        QImage second_luminance;
        {
            QImage decoded = ImageSource(request.second_image_path).decode(apply_color_profile);
            if (token.stop_requested()) {
                return {};
            }
            if (decoded.size() != first_luminance.size()) {
                throw std::runtime_error("selected image dimensions differ");
            }
            second_luminance = to_luminance(std::move(decoded), token);
        }
        if (token.stop_requested()) {
            return {};
        }

        QImage heatmap(first_luminance.size(), QImage::Format_RGB32);
        if (heatmap.isNull()) {
            throw std::runtime_error("could not allocate SSIM map");
        }
        uchar* const map_storage = heatmap.bits();

        rmgr::ssim::GeneralParams params{};
        params.width = static_cast<rmgr::ssim::uint32_t>(first_luminance.width());
        params.height = static_cast<rmgr::ssim::uint32_t>(first_luminance.height());
        params.imgA = {.topLeft = first_luminance.constBits(), .step = 1, .stride = first_luminance.bytesPerLine()};
        params.imgB = {.topLeft = second_luminance.constBits(), .step = 1, .stride = second_luminance.bytesPerLine()};
        params.ssimMap = reinterpret_cast<float*>(map_storage);
        params.ssimStep = 1;
        params.ssimStride = heatmap.bytesPerLine() / static_cast<qsizetype>(sizeof(float));
        params.use_default_allocator();

        float score = 0.0F;
        const int result = rmgr::ssim::compute_ssim(&score, params, nullptr);
        if (token.stop_requested()) {
            return {};
        }
        if (result != 0) {
            throw std::system_error(result, std::generic_category(), "SSIM failed");
        }
        if (!std::isfinite(score)) {
            throw std::runtime_error("SSIM produced a non-finite global score");
        }

        first_luminance = {};
        second_luminance = {};
        colorize(heatmap, token);
        if (token.stop_requested()) {
            return {};
        }

        return {.error = {}, .heatmap = std::move(heatmap), .ssim = static_cast<double>(score)};
    }
}  // namespace

ImageComparison::ImageComparison(QObject* parent) : QObject(parent) { m_thread_pool.setMaxThreadCount(1); }

ImageComparison::~ImageComparison() {
    cancel();
    m_thread_pool.waitForDone();
}

void ImageComparison::start(ComparisonRequest request) {
    cancel();
    m_cancellation_source = std::stop_source{};
    const std::stop_token token = m_cancellation_source.get_token();
    const QPointer<ImageComparison> comparison_guard(this);

    m_thread_pool.start([comparison_guard, token, request = std::move(request)]() mutable {
        ComparisonResult result;
        try {
            result = compare_images(request, token);
        } catch (const std::bad_alloc&) {
            result = fail(QStringLiteral("not enough memory to compare images"));
        } catch (const std::exception& exception) {
            result = fail(QString::fromUtf8(exception.what()));
        } catch (...) {
            result = fail(QStringLiteral("unexpected image comparison failure"));
        }

        if (token.stop_requested() || comparison_guard == nullptr) {
            return;
        }
        QMetaObject::invokeMethod(
            comparison_guard,
            [comparison_guard, token, result = std::move(result)]() mutable {
                if (comparison_guard == nullptr || token.stop_requested() || comparison_guard->m_cancellation_source.get_token() != token) {
                    return;
                }
                comparison_guard->m_cancellation_source = std::stop_source{std::nostopstate};
                Q_EMIT comparison_guard->finished(std::move(result));
            },
            Qt::QueuedConnection);
    });
}

void ImageComparison::cancel() {
    m_cancellation_source.request_stop();
    m_cancellation_source = std::stop_source{std::nostopstate};
    m_thread_pool.clear();
}
