#include "core/ComparisonService.h"

#include <QDir>
#include <QUuid>
#include <algorithm>
#include <exception>

#include "core/ImageRepository.h"

namespace {
    ComparisonResult fail(ComparisonErrorCode code, const QString& message) {
        ComparisonResult result;
        result.success = false;
        result.error_code = code;
        result.error_text = message;
        return result;
    }

    bool dimensions_match(const vips::VImage& left, const vips::VImage& right) {
        return left.width() == right.width() && left.height() == right.height();
    }

    QString next_output_path() {
        const QString file_name = QStringLiteral("imagecompare-%1.png").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        return QDir(QDir::tempPath()).filePath(file_name);
    }
}  // namespace

ComparisonService::ComparisonService(ImageRepository& repository) : m_repository(repository) {}

ComparisonResult ComparisonService::run(const ComparisonRequest& request) const {
    if (request.left_image_handle_id.isNull() || request.right_image_handle_id.isNull()) {
        return fail(ComparisonErrorCode::InvalidRequest, QStringLiteral("comparison request has null image handles"));
    }

    try {
        const auto left_source = m_repository.image(request.left_image_handle_id);
        const auto right_source = m_repository.image(request.right_image_handle_id);

        const RenderSpec left_spec = left_source->render_spec(request.display_mode);
        const RenderSpec right_spec = right_source->render_spec(request.display_mode);
        vips::VImage left = ImageSource::load_for_render(left_source->path(), left_spec);
        vips::VImage right = ImageSource::load_for_render(right_source->path(), right_spec);

        if (!dimensions_match(left, right)) {
            return fail(ComparisonErrorCode::DimensionMismatch, QStringLiteral("left/right dimensions differ"));
        }

        ComparisonResult result;
        vips::VImage raw_diff = (left - right).abs();
        if (raw_diff.bands() > 1) {
            raw_diff = raw_diff.bandmean();
        }
        raw_diff = raw_diff.cast(VIPS_FORMAT_FLOAT);

        result.summary.max_value = raw_diff.max();
        const double p95_value = raw_diff.percent(95.0);
        if (result.summary.max_value > 0.0) {
            result.summary.normalized_mean = raw_diff.avg() / result.summary.max_value;
            result.summary.normalized_p95 = p95_value / result.summary.max_value;
        }

        vips::VImage changed_mask = raw_diff > 0.0;
        result.summary.changed_pixel_ratio = changed_mask.avg() / 255.0;

        const auto max_value = std::max(1.0, result.summary.max_value);
        vips::VImage normalized = (raw_diff * (255.0 / max_value)).cast(VIPS_FORMAT_UCHAR);
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
