#include "app/ApplicationController.h"

#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <exception>

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent), m_workspace(this), m_comparison_service(m_repository), m_job_queue(m_comparison_service, this) {
    connect(&m_job_queue, &ComparisonJobQueue::job_finished, this, &ApplicationController::on_job_finished);
    connect(&m_job_queue, &ComparisonJobQueue::job_failed, this, &ApplicationController::on_job_failed);
    connect(&m_workspace, &WorkspaceDocument::display_mode_changed, this, &ApplicationController::display_mode_changed);
}

ApplicationController::~ApplicationController() {
    remove_tracked_generated_heatmap_paths(QStringList(m_generated_heatmap_paths.begin(), m_generated_heatmap_paths.end()), true);
}

void ApplicationController::import_image_paths(const QStringList& paths) {
    QSet<QUuid> existing_source_handles;
    for (int index = 0; index < m_workspace.entry_count(); ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (entry.is_source()) {
            existing_source_handles.insert(entry.image_handle_id());
        }
    }

    for (const QString& raw_path : paths) {
        QString normalized_path = raw_path.trimmed();
        if (normalized_path.isEmpty()) {
            continue;
        }

        const QUrl input_url(normalized_path);
        if (input_url.isValid() && input_url.isLocalFile()) {
            normalized_path = input_url.toLocalFile();
        }

        if (!ImageSource::supported_image_path(normalized_path)) {
            continue;
        }

        try {
            const QUuid handle_id = m_repository.load(normalized_path);
            if (existing_source_handles.contains(handle_id)) {
                continue;
            }
            const auto source = m_repository.image(handle_id);
            if (m_workspace.add_source_entry(handle_id, normalized_path, source->pixel_size()).isNull()) {
                release_image_handle_if_unused(handle_id);
                continue;
            }
            existing_source_handles.insert(handle_id);
        } catch (const std::exception& ex) {
            qWarning().noquote() << "Failed to import image path" << normalized_path << ":" << ex.what();
        }
    }
}

void ApplicationController::open_images_with_native_dialog() {
    QFileDialog dialog;
    dialog.setWindowTitle(QStringLiteral("Open Images"));
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter(QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.avif *.jxl *.heic *.heif *.tif *.tiff *.bmp)"));
    dialog.setOption(QFileDialog::DontUseNativeDialog, false);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    import_image_paths(dialog.selectedFiles());
}

bool ApplicationController::remove_workspace_entry_by_id(const QString& entry_id) {
    const QUuid parsed_entry_id(entry_id);
    QString generated_path_to_remove;
    QUuid image_handle_id_to_remove;
    for (int index = 0; index < m_workspace.entry_count(); ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (entry.entry_id() != parsed_entry_id) {
            continue;
        }
        image_handle_id_to_remove = entry.image_handle_id();
        if (entry.is_derived()) {
            generated_path_to_remove = entry.image_path();
        }
        break;
    }

    const bool removed = m_workspace.remove_entry_by_id(parsed_entry_id);
    if (removed && !generated_path_to_remove.isEmpty()) {
        remove_tracked_generated_heatmap_path(generated_path_to_remove);
    }
    if (removed) {
        release_image_handle_if_unused(image_handle_id_to_remove);
    }
    return removed;
}

void ApplicationController::set_display_mode_faithful() { set_display_mode_and_reset_heatmap(DisplayMode::Faithful); }

void ApplicationController::set_display_mode_strict_raw() { set_display_mode_and_reset_heatmap(DisplayMode::StrictRaw); }

void ApplicationController::set_display_mode_and_reset_heatmap(DisplayMode mode) {
    if (m_workspace.display_mode() == mode) {
        return;
    }
    m_workspace.set_display_mode(mode);
    clear_existing_derived_heatmaps();
}

void ApplicationController::clear_existing_derived_heatmaps() {
    const bool was_heatmap_in_progress = heatmap_in_progress();
    const QStringList paths_to_remove = tracked_generated_heatmap_paths_in_workspace();
    QVector<QUuid> handles_to_release;
    for (int index = 0; index < m_workspace.entry_count(); ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (entry.is_derived()) {
            handles_to_release.push_back(entry.image_handle_id());
        }
    }

    static_cast<void>(m_workspace.remove_derived_heatmap_entries());
    m_pending_heatmap_jobs.clear();
    remove_tracked_generated_heatmap_paths(paths_to_remove);
    for (const QUuid& handle_id : handles_to_release) {
        release_image_handle_if_unused(handle_id);
    }
    if (was_heatmap_in_progress != heatmap_in_progress()) {
        Q_EMIT heatmap_in_progress_changed();
    }
}

void ApplicationController::build_heatmap() {
    if (heatmap_in_progress() || !m_workspace.can_build_heatmap()) {
        return;
    }

    const bool was_heatmap_in_progress = heatmap_in_progress();
    QVector<ViewableImageEntry> source_images;
    source_images.reserve(2);
    for (int index = 0; index < m_workspace.entry_count() && source_images.size() < 2; ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (entry.is_source()) {
            source_images.push_back(entry);
        }
    }
    if (source_images.size() != 2 || source_images[0].image_handle_id().isNull() || source_images[1].image_handle_id().isNull()) {
        return;
    }

    clear_existing_derived_heatmaps();
    const QUuid job_id = m_job_queue.enqueue({
        .first_image_handle_id = source_images[0].image_handle_id(),
        .second_image_handle_id = source_images[1].image_handle_id(),
        .display_mode = m_workspace.display_mode(),
    });
    m_pending_heatmap_jobs.insert(job_id);
    m_in_flight_heatmap_handles_by_job.insert(job_id, {source_images[0].image_handle_id(), source_images[1].image_handle_id()});
    if (was_heatmap_in_progress != heatmap_in_progress()) {
        Q_EMIT heatmap_in_progress_changed();
    }
}

WorkspaceDocument* ApplicationController::workspace() noexcept { return &m_workspace; }

void ApplicationController::on_job_finished(QUuid job_id, ComparisonResult result) {
    const bool was_heatmap_in_progress = heatmap_in_progress();
    const auto emit_heatmap_state_if_changed = [this, was_heatmap_in_progress]() {
        if (was_heatmap_in_progress != heatmap_in_progress()) {
            Q_EMIT heatmap_in_progress_changed();
        }
    };
    m_in_flight_heatmap_handles_by_job.remove(job_id);
    flush_deferred_image_handle_releases();
    if (!m_pending_heatmap_jobs.remove(job_id)) {
        if (result.success && !result.output_path.isEmpty()) {
            delete_generated_heatmap_file(result.output_path);
        }
        emit_heatmap_state_if_changed();
        return;
    }
    if (!result.success || result.output_path.isEmpty()) {
        emit_heatmap_state_if_changed();
        return;
    }

    try {
        const QUuid result_handle_id = m_repository.load(result.output_path);
        const auto result_source = m_repository.image(result_handle_id);
        const QString summary_label = QStringLiteral("overall dE00 %1 • hotspots dE00 %2")
                                          .arg(result.summary.average_de00, 0, 'f', 2)
                                          .arg(result.summary.p95_de00, 0, 'f', 2);

        const QUuid entry_id =
            m_workspace.add_derived_entry(result_handle_id, result.output_path, result_source->pixel_size(), summary_label);
        if (entry_id.isNull()) {
            delete_generated_heatmap_file(result.output_path);
            static_cast<void>(m_repository.release(result_handle_id));
            emit_heatmap_state_if_changed();
            return;
        }
        m_generated_heatmap_paths.insert(result.output_path);
    } catch (const std::exception& ex) {
        if (!result.output_path.isEmpty()) {
            delete_generated_heatmap_file(result.output_path);
        }
        qWarning().noquote() << "Failed to load generated heatmap result" << result.output_path << ":" << ex.what();
    }
    emit_heatmap_state_if_changed();
}

void ApplicationController::on_job_failed(QUuid job_id, const QString& error_text) {
    const bool was_heatmap_in_progress = heatmap_in_progress();
    m_pending_heatmap_jobs.remove(job_id);
    m_in_flight_heatmap_handles_by_job.remove(job_id);
    flush_deferred_image_handle_releases();
    qWarning().noquote() << "Heatmap job failed for" << job_id.toString(QUuid::WithoutBraces) << ":"
                         << (error_text.isEmpty() ? QStringLiteral("unknown error") : error_text);
    if (was_heatmap_in_progress != heatmap_in_progress()) {
        Q_EMIT heatmap_in_progress_changed();
    }
}

bool ApplicationController::workspace_references_image_handle(const QUuid& handle_id) const {
    if (handle_id.isNull()) {
        return false;
    }

    for (int index = 0; index < m_workspace.entry_count(); ++index) {
        if (m_workspace.entry_at(index).image_handle_id() == handle_id) {
            return true;
        }
    }
    return false;
}

bool ApplicationController::in_flight_heatmap_uses_image_handle(const QUuid& handle_id) const {
    if (handle_id.isNull()) {
        return false;
    }

    for (auto it = m_in_flight_heatmap_handles_by_job.cbegin(); it != m_in_flight_heatmap_handles_by_job.cend(); ++it) {
        if (it.value().contains(handle_id)) {
            return true;
        }
    }
    return false;
}

void ApplicationController::release_image_handle_if_unused(const QUuid& handle_id) {
    if (handle_id.isNull() || workspace_references_image_handle(handle_id)) {
        return;
    }
    if (in_flight_heatmap_uses_image_handle(handle_id)) {
        m_deferred_release_handles.insert(handle_id);
        return;
    }
    m_deferred_release_handles.remove(handle_id);
    static_cast<void>(m_repository.release(handle_id));
}

void ApplicationController::flush_deferred_image_handle_releases() {
    const auto deferred_handles = m_deferred_release_handles.values();
    for (const QUuid& handle_id : deferred_handles) {
        if (workspace_references_image_handle(handle_id) || in_flight_heatmap_uses_image_handle(handle_id)) {
            continue;
        }
        m_deferred_release_handles.remove(handle_id);
        static_cast<void>(m_repository.release(handle_id));
    }
}

QStringList ApplicationController::tracked_generated_heatmap_paths_in_workspace() const {
    QStringList paths;
    for (int index = 0; index < m_workspace.entry_count(); ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (!entry.is_derived()) {
            continue;
        }
        const QString path = entry.image_path();
        if (m_generated_heatmap_paths.contains(path)) {
            paths.push_back(path);
        }
    }
    return paths;
}

void ApplicationController::delete_generated_heatmap_file(const QString& path) const {
    if (!QFileInfo::exists(path)) {
        return;
    }
    if (!QFile::remove(path)) {
        qWarning().noquote() << "Failed to remove generated heatmap file" << path;
    }
}

void ApplicationController::remove_tracked_generated_heatmap_path(const QString& path, bool immediate) {
    if (!m_generated_heatmap_paths.remove(path)) {
        return;
    }
    if (path.isEmpty()) {
        return;
    }
    if (immediate) {
        delete_generated_heatmap_file(path);
        return;
    }
    QMetaObject::invokeMethod(this, [this, path]() { delete_generated_heatmap_file(path); }, Qt::QueuedConnection);
}

void ApplicationController::remove_tracked_generated_heatmap_paths(const QStringList& paths, bool immediate) {
    for (const QString& path : paths) {
        remove_tracked_generated_heatmap_path(path, immediate);
    }
}
