#include "core/ComparisonService.h"

#include <QDir>
#include <QUuid>
#include <exception>
#include <utility>

#include "core/ImageRepository.h"

namespace {
    constexpr double k_transparent_background = 127.0;
    constexpr double k_heatmap_reference_de00 = 10.0;
    constexpr double k_spatial_sigma = 1.2;

    ComparisonResult fail(ComparisonErrorCode code, const QString& message) {
        ComparisonResult result;
        result.success = false;
        result.error_code = code;
        result.error_text = message;
        return result;
    }

    bool dimensions_match(const vips::VImage& first, const vips::VImage& second) {
        return first.width() == second.width() && first.height() == second.height();
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
        if (image.bands() == 1) {
            return image.bandjoin(image).bandjoin(image);
        }
        if (image.bands() == 3) {
            return image;
        }
        if (image.bands() == 4) {
            const vips::VImage rgb = image.extract_band(0, vips::VImage::option()->set("n", 3)).cast(VIPS_FORMAT_FLOAT);
            const vips::VImage alpha = image.extract_band(3).cast(VIPS_FORMAT_FLOAT) / 255.0;
            const vips::VImage background = rgb.new_from_image(k_transparent_background);
            return ((rgb * alpha) + (background * (1.0 - alpha))).cast(VIPS_FORMAT_UCHAR);
        }

        // Last-resort compatibility path for unusual source layouts.
        const vips::VImage gray = image.bandmean().cast(VIPS_FORMAT_UCHAR);
        return gray.bandjoin(gray).bandjoin(gray);
    }

    std::pair<vips::VImage, vips::VImage> normalize_for_perceptual_compare(vips::VImage first, vips::VImage second) {
        return {rgb_for_perceptual_compare(first), rgb_for_perceptual_compare(second)};
    }

    QString next_output_path() {
        const QString file_name = QStringLiteral("imagecompare-%1.png").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        return QDir(QDir::tempPath()).filePath(file_name);
    }
}  // namespace

ComparisonService::ComparisonService(ImageRepository& repository) : m_repository(repository) {}

ComparisonResult ComparisonService::run(const ComparisonRequest& request) const {
    if (request.first_image_handle_id.isNull() || request.second_image_handle_id.isNull()) {
        return fail(ComparisonErrorCode::InvalidRequest, QStringLiteral("comparison request has null image handles"));
    }

    try {
        const auto first_source = m_repository.image(request.first_image_handle_id);
        const auto second_source = m_repository.image(request.second_image_handle_id);

        const RenderSpec first_spec = first_source->render_spec(request.display_mode);
        const RenderSpec second_spec = second_source->render_spec(request.display_mode);
        vips::VImage first = ImageSource::load_for_render(first_source->path(), first_spec);
        vips::VImage second = ImageSource::load_for_render(second_source->path(), second_spec);

        if (!dimensions_match(first, second)) {
            return fail(ComparisonErrorCode::DimensionMismatch, QStringLiteral("selected image dimensions differ"));
        }
        auto normalized_inputs = normalize_for_perceptual_compare(first, second);
        first = std::move(normalized_inputs.first);
        second = std::move(normalized_inputs.second);

        ComparisonResult result;
        const vips::VImage first_lab = first.colourspace(VIPS_INTERPRETATION_LAB);
        const vips::VImage second_lab = second.colourspace(VIPS_INTERPRETATION_LAB);
        const vips::VImage de00 = first_lab.dE00(second_lab).cast(VIPS_FORMAT_FLOAT);
        const vips::VImage perceptual_diff = de00.gaussblur(k_spatial_sigma).cast(VIPS_FORMAT_FLOAT);

        result.summary.max_value = perceptual_diff.max();
        result.summary.average_de00 = perceptual_diff.avg();
        result.summary.p95_de00 = perceptual_diff.percent(95.0);

        vips::VImage normalized = (perceptual_diff * (255.0 / k_heatmap_reference_de00)).cast(VIPS_FORMAT_UCHAR);
        vips::VImage display_heatmap = normalized.falsecolour();
        result.output_path = next_output_path();
        display_heatmap.pngsave(result.output_path.toUtf8().constData());

        result.success = true;
        result.error_code = ComparisonErrorCode::None;
        return result;
    } catch (const std::exception& ex) {
        return fail(ComparisonErrorCode::ProcessingFailed, QString::fromUtf8(ex.what()));
    }
}
