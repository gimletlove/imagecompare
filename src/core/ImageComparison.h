#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <stop_token>

#include "core/WorkspaceModel.h"

struct ComparisonRequest {
    QString first_image_path;
    QString second_image_path;
    ColorMode color_mode = ColorMode::Faithful;
};

struct ComparisonResult {
    QString error;
    QImage heatmap;
    double ssim = 0.0;

    [[nodiscard]] bool succeeded() const noexcept { return !heatmap.isNull(); }
};

Q_DECLARE_METATYPE(ComparisonResult)

class ImageComparison : public QObject {
    Q_OBJECT

   public:
    explicit ImageComparison(QObject* parent = nullptr);
    ~ImageComparison() override;

    void start(ComparisonRequest request);
    void cancel();

   Q_SIGNALS:
    void finished(ComparisonResult result);

   private:
    QThreadPool m_thread_pool;
    std::stop_source m_cancellation_source{std::nostopstate};
};
