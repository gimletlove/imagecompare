#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "core/ImageComparison.h"
#include "core/WorkspaceModel.h"

class ApplicationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(WorkspaceModel* workspace READ workspace CONSTANT)
    Q_PROPERTY(bool heatmap_in_progress READ heatmap_in_progress NOTIFY heatmap_in_progress_changed)

   public:
    explicit ApplicationController(QObject* parent = nullptr);

    Q_INVOKABLE void import_image_paths(const QStringList& paths);
    Q_INVOKABLE void open_images_with_native_dialog();
    Q_INVOKABLE bool remove_workspace_entry_by_id(const QString& entry_id);
    Q_INVOKABLE bool move_workspace_entry_by_id(const QString& entry_id, int direction);
    Q_INVOKABLE bool copy_path_to_clipboard(const QString& path) const;
    Q_INVOKABLE bool open_containing_folder(const QString& path) const;
    Q_INVOKABLE bool export_heatmap_by_id(const QString& entry_id) const;
    Q_INVOKABLE void toggle_color_mode();
    Q_INVOKABLE void build_heatmap();

    [[nodiscard]] WorkspaceModel* workspace() noexcept;
    [[nodiscard]] bool heatmap_in_progress() const noexcept { return m_heatmap_in_progress; }

   Q_SIGNALS:
    void heatmap_in_progress_changed();

   private:
    void on_comparison_finished(ComparisonResult result);
    void clear_existing_heatmap();
    void cancel_comparison();

    WorkspaceModel m_workspace;
    ImageComparison m_image_comparison;
    bool m_heatmap_in_progress = false;
};
