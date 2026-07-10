#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>

#include "core/ComparisonJobQueue.h"
#include "core/WorkspaceDocument.h"

class ApplicationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(WorkspaceDocument* workspace READ workspace CONSTANT)
    Q_PROPERTY(DisplayMode display_mode READ display_mode NOTIFY display_mode_changed)
    Q_PROPERTY(bool heatmap_in_progress READ heatmap_in_progress NOTIFY heatmap_in_progress_changed)

   public:
    explicit ApplicationController(QObject* parent = nullptr);
    ~ApplicationController() override;

    Q_INVOKABLE void import_image_paths(const QStringList& paths);
    Q_INVOKABLE void open_images_with_native_dialog();
    Q_INVOKABLE bool remove_workspace_entry_by_id(const QString& entry_id);
    Q_INVOKABLE bool move_workspace_entry_by_id(const QString& entry_id, int direction);
    Q_INVOKABLE bool copy_path_to_clipboard(const QString& path) const;
    Q_INVOKABLE bool open_containing_folder(const QString& path) const;
    Q_INVOKABLE bool export_heatmap_by_id(const QString& entry_id) const;
    Q_INVOKABLE void set_display_mode_faithful();
    Q_INVOKABLE void set_display_mode_strict_raw();
    Q_INVOKABLE void build_heatmap();

    [[nodiscard]] WorkspaceDocument* workspace() noexcept;
    [[nodiscard]] DisplayMode display_mode() const noexcept { return m_workspace.display_mode(); }
    [[nodiscard]] bool heatmap_in_progress() const noexcept { return !m_pending_heatmap_job.isNull(); }

   Q_SIGNALS:
    void display_mode_changed();
    void heatmap_in_progress_changed();

   private:
    void on_job_finished(QUuid job_id, const ComparisonResult& result);
    void on_job_failed(QUuid job_id, const QString& error_text);
    void set_display_mode_and_reset_heatmap(DisplayMode mode);
    void clear_existing_derived_heatmaps();
    void cancel_pending_heatmap();
    void delete_generated_heatmap_file(const QString& path) const;
    void remove_generated_heatmap(bool immediate = false);

    WorkspaceDocument m_workspace;
    ComparisonJobQueue m_job_queue;
    QUuid m_pending_heatmap_job;
    QString m_generated_heatmap_path;
};
