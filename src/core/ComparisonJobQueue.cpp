#include "core/ComparisonJobQueue.h"

#include <vips/vips.h>

#include <QMetaObject>
#include <QMetaType>
#include <QPointer>
#include <QRunnable>
#include <QScopeGuard>
#include <QThreadPool>
#include <exception>

#include "core/ComparisonService.h"

ComparisonJobQueue::ComparisonJobQueue(ImageRepository& repository, QObject* parent) : QObject(parent), m_repository(repository) {
    qRegisterMetaType<ComparisonResult>("ComparisonResult");
}

ComparisonJobQueue::~ComparisonJobQueue() { m_thread_pool.waitForDone(); }

QUuid ComparisonJobQueue::enqueue(const ComparisonRequest& request) {
    const QUuid job_id = QUuid::createUuid();
    QPointer<ComparisonJobQueue> queue_guard(this);

    auto task = QRunnable::create([queue_guard, &repository = m_repository, job_id, request]() {
        const auto vips_thread_guard = qScopeGuard([] { vips_thread_shutdown(); });

        try {
            const ComparisonResult result = run_comparison(repository, request);
            if (queue_guard == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                queue_guard,
                [queue_guard, job_id, result]() {
                    if (queue_guard == nullptr) {
                        return;
                    }
                    if (result.success) {
                        Q_EMIT queue_guard->job_finished(job_id, result);
                    } else {
                        const QString text = result.error_text.isEmpty() ? QStringLiteral("comparison job failed") : result.error_text;
                        Q_EMIT queue_guard->job_failed(job_id, text);
                    }
                },
                Qt::QueuedConnection);
        } catch (const std::exception& ex) {
            if (queue_guard == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                queue_guard,
                [queue_guard, job_id, ex_text = QString::fromUtf8(ex.what())]() {
                    if (queue_guard == nullptr) {
                        return;
                    }
                    Q_EMIT queue_guard->job_failed(job_id, ex_text);
                },
                Qt::QueuedConnection);
        }
    });
    m_thread_pool.start(task);

    return job_id;
}
