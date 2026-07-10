#pragma once

#include <QMetaType>
#include <QString>

#include "core/WorkspaceDocument.h"

struct ComparisonRequest {
    QString first_image_path;
    QString second_image_path;
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
