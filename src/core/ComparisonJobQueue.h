#pragma once

#include <QObject>
#include <QThreadPool>
#include <QUuid>

#include "core/ComparisonResult.h"

class ComparisonJobQueue : public QObject {
    Q_OBJECT

   public:
    explicit ComparisonJobQueue(QObject* parent = nullptr);
    ~ComparisonJobQueue() override;
    QUuid enqueue(const ComparisonRequest& request);

   Q_SIGNALS:
    void job_finished(QUuid job_id, ComparisonResult result);
    void job_failed(QUuid job_id, QString error_text);

   private:
    QThreadPool m_thread_pool;
};
