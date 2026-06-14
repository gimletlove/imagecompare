#include "core/ComparisonService.h"

#include <QDir>
#include <QUuid>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <vector>

#include "core/ImageRepository.h"

namespace {
    constexpr double k_transparent_background = 127.0;
    constexpr double k_heatmap_good_de00 = 2.0;
    constexpr double k_heatmap_bad_de00 = 5.0;
    constexpr double k_spatial_sigma = 0.6;
    constexpr int k_heatmap_lut_width = 256;
    constexpr std::size_t k_heatmap_lut_size = static_cast<std::size_t>(k_heatmap_lut_width) * 3;

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

    ComparisonResult fail(const QString& message) {
        ComparisonResult result;
        result.success = false;
        result.error_text = message;
        return result;
    }

    vips::VImage normalize_uncommon_bands(vips::VImage image) {
        const int band_count = image.bands();
        if (band_count == 2) {
            const vips::VImage gray = image.extract_band(0);
            const vips::VImage alpha = image.extract_band(1);
            return gray.bandjoin(gray).bandjoin(gray).bandjoin(alpha);
        }
        if (band_count > 4) {
            return image.extract_band(0, vips::VImage::option()->set("n", 4));
        }
        return image;
    }

    vips::VImage rgb_for_perceptual_compare(vips::VImage image) {
        image = normalize_uncommon_bands(image);
        if (image.interpretation() != VIPS_INTERPRETATION_sRGB) {
            try {
                image = image.colourspace(VIPS_INTERPRETATION_sRGB);
                image = normalize_uncommon_bands(image);
            } catch (const std::exception&) {
            }
        }
        if (image.bands() == 1) {
            return image.bandjoin(image).bandjoin(image);
        }
        if (image.bands() == 3) {
            return image.format() == VIPS_FORMAT_UCHAR ? image : image.cast(VIPS_FORMAT_UCHAR);
        }
        if (image.bands() == 4) {
            return image
                .flatten(vips::VImage::option()->set("background", std::vector<double>{k_transparent_background, k_transparent_background,
                                                                                       k_transparent_background}))
                .cast(VIPS_FORMAT_UCHAR);
        }

        // Last-resort compatibility path for unusual source layouts.
        const vips::VImage gray = image.bandmean().cast(VIPS_FORMAT_UCHAR);
        return gray.bandjoin(gray).bandjoin(gray);
    }

    std::uint8_t heatmap_channel(double value) {
        const double clamped = std::clamp(value, 0.0, 1.0);
        return static_cast<std::uint8_t>(std::lround(std::sqrt(clamped) * 255.0));
    }

    std::array<std::uint8_t, k_heatmap_lut_size> build_heatmap_lut() {
        std::array<std::uint8_t, k_heatmap_lut_size> lut = {};
        for (std::size_t index = 0; index < 256; ++index) {
            const double level = static_cast<double>(index) / 255.0;
            const double palette_position = level * static_cast<double>(k_heatmap_palette.size() - 1);
            const std::size_t palette_index = std::min(static_cast<std::size_t>(palette_position), k_heatmap_palette.size() - 2);
            const double blend = palette_position - static_cast<double>(palette_index);
            const HeatmapColor& low = k_heatmap_palette[palette_index];
            const HeatmapColor& high = k_heatmap_palette[palette_index + 1];

            lut[index * 3 + 0] = heatmap_channel(low.red + (high.red - low.red) * blend);
            lut[index * 3 + 1] = heatmap_channel(low.green + (high.green - low.green) * blend);
            lut[index * 3 + 2] = heatmap_channel(low.blue + (high.blue - low.blue) * blend);
        }
        return lut;
    }

    vips::VImage heatmap_lut() {
        static std::array<std::uint8_t, k_heatmap_lut_size> lut = build_heatmap_lut();
        return vips::VImage::new_from_memory_copy(lut.data(), lut.size() * sizeof(std::uint8_t), k_heatmap_lut_width, 1, 3,
                                                  VIPS_FORMAT_UCHAR);
    }

    vips::VImage heatmap_display_level(const vips::VImage& perceptual_diff) {
        const vips::VImage low = (perceptual_diff / k_heatmap_good_de00) * 0.30;
        const vips::VImage mid = ((perceptual_diff - k_heatmap_good_de00) / (k_heatmap_bad_de00 - k_heatmap_good_de00)) * 0.15 + 0.30;
        const vips::VImage high = ((perceptual_diff - k_heatmap_bad_de00) / (k_heatmap_bad_de00 * 12.0)) * 0.50 + 0.45;
        vips::VImage level =
            (perceptual_diff < k_heatmap_good_de00).ifthenelse(low, (perceptual_diff < k_heatmap_bad_de00).ifthenelse(mid, high));
        return (level > 1.0).ifthenelse(1.0, level);
    }

    vips::VImage colorize_heatmap(const vips::VImage& perceptual_diff) {
        vips::VImage indexed = (heatmap_display_level(perceptual_diff) * 255.0).rint();
        indexed = (indexed > 255.0).ifthenelse(255.0, indexed);
        return indexed.cast(VIPS_FORMAT_UCHAR).maplut(heatmap_lut());
    }

    QString next_output_path() {
        const QString file_name = QStringLiteral("imagecompare-%1.png").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        return QDir(QDir::tempPath()).filePath(file_name);
    }
}  // namespace

ComparisonResult run_comparison(ImageRepository& repository, const ComparisonRequest& request) {
    if (request.first_image_handle_id.isNull() || request.second_image_handle_id.isNull()) {
        return fail(QStringLiteral("comparison request has null image handles"));
    }

    try {
        const auto first_source = repository.image(request.first_image_handle_id);
        const auto second_source = repository.image(request.second_image_handle_id);

        const RenderSpec spec{.ignore_color_profile = request.display_mode == DisplayMode::StrictRaw};
        vips::VImage first = ImageSource::load_for_render(first_source->path(), spec);
        vips::VImage second = ImageSource::load_for_render(second_source->path(), spec);

        if (first.width() != second.width() || first.height() != second.height()) {
            return fail(QStringLiteral("selected image dimensions differ"));
        }
        first = rgb_for_perceptual_compare(first);
        second = rgb_for_perceptual_compare(second);

        ComparisonResult result;
        const vips::VImage first_lab = first.colourspace(VIPS_INTERPRETATION_LAB);
        const vips::VImage second_lab = second.colourspace(VIPS_INTERPRETATION_LAB);
        const vips::VImage de00 = first_lab.dE00(second_lab).cast(VIPS_FORMAT_FLOAT);

        result.summary.overall_de00 = de00.avg();
        result.summary.peak_de00 = de00.max();

        const vips::VImage perceptual_diff = de00.gaussblur(k_spatial_sigma).cast(VIPS_FORMAT_FLOAT);
        const vips::VImage display_heatmap = colorize_heatmap(perceptual_diff);
        result.output_path = next_output_path();
        display_heatmap.pngsave(result.output_path.toUtf8().constData());

        result.success = true;
        return result;
    } catch (const std::exception& ex) {
        return fail(QString::fromUtf8(ex.what()));
    }
}
