#include "core/ImageComparison.h"

#include <rmgr/ssim.h>

#include <QColorSpace>
#include <QDebug>
#include <QMetaObject>
#include <QThread>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
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

    // Run rmgr::ssim tile jobs on short-lived threads
    rmgr::ssim::int32_t dispatch_ssim_jobs(void*, rmgr::ssim::ThreadFct job, void* const args[], rmgr::ssim::uint32_t thread_count,
                                           rmgr::ssim::uint32_t job_count) noexcept {
        try {
            std::atomic<rmgr::ssim::uint32_t> next_job{0};
            std::vector<std::jthread> threads;
            threads.reserve(thread_count);
            for (rmgr::ssim::uint32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
                threads.emplace_back([&next_job, job, job_count, arg = args[thread_index]] {
                    for (auto job_index = next_job++; job_index < job_count; job_index = next_job++) {
                        job(arg, job_index);
                    }
                });
            }
            return 0;
        } catch (...) {
            return ECHILD;
        }
    }

    [[nodiscard]] QImage to_luminance(QImage image, const std::stop_token& token) {
        if (image.isNull()) {
            throw std::runtime_error("decoded image is empty");
        }

        // RGB32 already has the ARGB32 layout with opaque alpha
        if (image.format() != QImage::Format_RGB32) {
            image.convertTo(QImage::Format_ARGB32);
        }
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
                destination[x] = static_cast<uchar>((qAlpha(pixel) * luma + 127) / 255);
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
                const auto palette_index = static_cast<std::size_t>(difference * 255.0 + 0.5);
                const QRgb color = lut[palette_index];
                std::memcpy(slot, &color, sizeof(color));
            }
        }
    }

    [[nodiscard]] ComparisonResult compare_images(const QImage& first_image, const QImage& second_image, const std::stop_token& token) {
        static_assert(sizeof(float) == sizeof(QRgb));

        if (first_image.isNull() || second_image.isNull()) {
            throw std::invalid_argument("comparison request has empty images");
        }
        if (first_image.size() != second_image.size()) {
            throw std::runtime_error("selected image dimensions differ");
        }

        QImage first_luminance = to_luminance(first_image, token);
        QImage second_luminance = to_luminance(second_image, token);
        if (token.stop_requested()) {
            return {};
        }

        QImage heatmap(first_luminance.size(), QImage::Format_RGB32);
        if (heatmap.isNull()) {
            throw std::runtime_error("could not allocate SSIM map");
        }
        heatmap.setColorSpace(QColorSpace::SRgb);
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

        const rmgr::ssim::ThreadPool thread_pool{
            .dispatch = dispatch_ssim_jobs,
            .context = nullptr,
            .threadCount = static_cast<rmgr::ssim::uint32_t>(std::max(1, QThread::idealThreadCount() / 2)),
        };
        float score = 0.0F;
        const int result = rmgr::ssim::compute_ssim(&score, params, &thread_pool);
        if (result != 0) {
            throw std::system_error(result, std::generic_category(), "SSIM failed");
        }
        if (!std::isfinite(score)) {
            throw std::runtime_error("SSIM produced a non-finite global score");
        }

        first_luminance = {};
        second_luminance = {};
        colorize(heatmap, token);
        return {.heatmap = std::move(heatmap), .ssim = static_cast<double>(score)};
    }
}  // namespace

ImageComparison::ImageComparison(QObject* parent) : QObject(parent) { m_thread_pool.setMaxThreadCount(1); }

ImageComparison::~ImageComparison() { cancel(); }

void ImageComparison::start(QImage first_image, QImage second_image) {
    cancel();
    m_cancellation_source = std::stop_source{};
    m_thread_pool.start(
        [this, token = m_cancellation_source.get_token(), first_image = std::move(first_image), second_image = std::move(second_image)] {
            ComparisonResult result;
            try {
                result = compare_images(first_image, second_image, token);
            } catch (const std::exception& exception) {
                qWarning().noquote() << "Heatmap job failed:" << exception.what();
            }
            QMetaObject::invokeMethod(
                this,
                [this, token, result = std::move(result)] {
                    if (!token.stop_requested()) {
                        Q_EMIT finished(result);
                    }
                },
                Qt::QueuedConnection);
        });
}

void ImageComparison::cancel() {
    m_cancellation_source.request_stop();
    m_thread_pool.clear();
}
