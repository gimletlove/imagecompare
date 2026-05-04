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

struct ComparisonSummary {
    double overall_de00 = 0.0;
    double peak_de00 = 0.0;
};

struct ComparisonResult {
    bool success = false;
    QString error_text;
    QString output_path;
    ComparisonSummary summary;
};

Q_DECLARE_METATYPE(ComparisonResult)
