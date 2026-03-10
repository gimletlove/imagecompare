#pragma once

#include <QMetaType>
#include <QString>
#include <QUuid>

#include "core/WorkspaceDocument.h"

struct ComparisonRequest {
    QUuid left_image_handle_id;
    QUuid right_image_handle_id;
    DisplayMode display_mode = DisplayMode::Faithful;
};

enum class ComparisonErrorCode {
    None,
    InvalidRequest,
    InvalidImageHandle,
    DimensionMismatch,
    ProcessingFailed,
};

struct ComparisonSummary {
    double normalized_mean = 0.0;
    double normalized_p95 = 0.0;
    double changed_pixel_ratio = 0.0;
    double max_value = 0.0;
};

struct ComparisonResult {
    bool success = false;
    ComparisonErrorCode error_code = ComparisonErrorCode::None;
    QString error_text;
    QString output_path;
    ComparisonSummary summary;
};

Q_DECLARE_METATYPE(ComparisonResult)
