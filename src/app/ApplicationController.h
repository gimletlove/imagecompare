#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

#include "core/ComparisonJobQueue.h"
#include "core/ComparisonService.h"
#include "core/ImageRepository.h"
#include "core/WorkspaceDocument.h"

class ApplicationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(WorkspaceDocument* workspace READ workspace CONSTANT)
    Q_PROPERTY(DisplayMode display_mode READ display_mode NOTIFY display_mode_changed)

   public:
    explicit ApplicationController(QObject* parent = nullptr);
    ~ApplicationController() override;

    Q_INVOKABLE void import_image_paths(const QStringList& paths);
    Q_INVOKABLE void open_images_with_native_dialog();
    Q_INVOKABLE bool remove_workspace_entry_by_id(const QString& entry_id);
    Q_INVOKABLE void set_display_mode_faithful();
    Q_INVOKABLE void set_display_mode_strict_raw();
    Q_INVOKABLE void build_heatmap();

    [[nodiscard]] WorkspaceDocument* workspace() noexcept;
    [[nodiscard]] DisplayMode display_mode() const noexcept { return m_workspace.display_mode(); }

   Q_SIGNALS:
    void display_mode_changed();

   private Q_SLOTS:
    void on_job_finished(QUuid job_id, ComparisonResult result);
    void on_job_failed(QUuid job_id, const QString& error_text);

   private:
    void set_display_mode_and_reset_heatmap(DisplayMode mode);
    void clear_existing_derived_heatmaps();
    [[nodiscard]] bool workspace_references_image_handle(const QUuid& handle_id) const;
    [[nodiscard]] bool in_flight_heatmap_uses_image_handle(const QUuid& handle_id) const;
    void release_image_handle_if_unused(const QUuid& handle_id);
    void flush_deferred_image_handle_releases();
    [[nodiscard]] QStringList tracked_generated_heatmap_paths_in_workspace() const;
    void delete_generated_heatmap_file(const QString& path) const;
    void remove_tracked_generated_heatmap_path(const QString& path, bool immediate = false);
    void remove_tracked_generated_heatmap_paths(const QStringList& paths, bool immediate = false);

    WorkspaceDocument m_workspace;
    ImageRepository m_repository;
    ComparisonService m_comparison_service;
    ComparisonJobQueue m_job_queue;
    QSet<QUuid> m_pending_heatmap_jobs;
    QHash<QUuid, QSet<QUuid>> m_in_flight_heatmap_handles_by_job;
    QSet<QUuid> m_deferred_release_handles;
    QSet<QString> m_generated_heatmap_paths;
};
