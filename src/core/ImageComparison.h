#pragma once

#include <QImage>
#include <QObject>
#include <QThreadPool>
#include <stop_token>

struct ComparisonResult {
    QImage heatmap;
    double ssim = 0.0;
};

class ImageComparison : public QObject {
    Q_OBJECT

   public:
    explicit ImageComparison(QObject* parent = nullptr);
    ~ImageComparison() override;

    void start(QImage first_image, QImage second_image);
    void cancel();

   Q_SIGNALS:
    void finished(ComparisonResult result);

   private:
    QThreadPool m_thread_pool;
    std::stop_source m_cancellation_source{std::nostopstate};
};
