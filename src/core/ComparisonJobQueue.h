#pragma once

#include <QObject>
#include <QThreadPool>
#include <QUuid>

#include "core/ComparisonResult.h"

class ComparisonService;

class ComparisonJobQueue : public QObject {
    Q_OBJECT

   public:
    explicit ComparisonJobQueue(ComparisonService& service, QObject* parent = nullptr);
    ~ComparisonJobQueue() override;
    QUuid enqueue(const ComparisonRequest& request);

   Q_SIGNALS:
    void job_started(QUuid job_id);
    void job_finished(QUuid job_id, ComparisonResult result);
    void job_failed(QUuid job_id, QString error_text);

   private:
    ComparisonService& m_service;
    QThreadPool m_thread_pool;
};
