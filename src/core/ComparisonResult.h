#pragma once

#include <QMetaType>
#include <QString>
#include <QUuid>

#include "core/WorkspaceDocument.h"

struct ComparisonRequest {
    QUuid first_image_handle_id;
    QUuid second_image_handle_id;
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
    double average_de00 = 0.0;
    double p95_de00 = 0.0;
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
