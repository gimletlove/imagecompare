#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QUuid>
#include <QVector>

#include "core/ViewableImageEntry.h"
#include "core/ViewableImageListModel.h"

class WorkspaceDocument : public QObject {
    Q_OBJECT
    Q_PROPERTY(int entry_count READ entry_count NOTIFY entry_count_changed)
    Q_PROPERTY(DisplayMode display_mode READ display_mode WRITE set_display_mode NOTIFY display_mode_changed)
    Q_PROPERTY(bool can_build_heatmap READ can_build_heatmap NOTIFY can_build_heatmap_changed)
    Q_PROPERTY(ViewableImageListModel* entries_model READ entries_model CONSTANT)

   public:
    enum class DisplayMode {
        StrictRaw,
        Faithful,
    };
    Q_ENUM(DisplayMode)

    explicit WorkspaceDocument(QObject* parent = nullptr);

    [[nodiscard]] QUuid add_source_entry(const QUuid& image_handle_id, const QString& path = {}, const QSize& pixel_size = {});
    [[nodiscard]] QUuid add_derived_entry(const QUuid& image_handle_id, const QString& output_path, const QSize& pixel_size,
                                          const QString& secondary_header_label = {});

    [[nodiscard]] int entry_count() const noexcept { return m_entries.size(); }
    [[nodiscard]] DisplayMode display_mode() const noexcept { return m_display_mode; }
    [[nodiscard]] int source_entry_count() const noexcept;
    [[nodiscard]] bool can_build_heatmap() const noexcept;
    [[nodiscard]] ViewableImageEntry entry_at(int index) const;
    [[nodiscard]] ViewableImageListModel* entries_model() noexcept { return &m_entries_model; }
    [[nodiscard]] bool remove_derived_heatmap_entries();
    [[nodiscard]] bool remove_entry_by_id(const QUuid& entry_id);
    [[nodiscard]] bool remove_entry_at(int index);
    void set_display_mode(DisplayMode mode);

   Q_SIGNALS:
    void entry_count_changed();
    void display_mode_changed();
    void can_build_heatmap_changed();

   private:
    static constexpr int k_max_entries = 10;

    QVector<ViewableImageEntry> m_entries;
    ViewableImageListModel m_entries_model;
    DisplayMode m_display_mode = DisplayMode::Faithful;
};

using DisplayMode = WorkspaceDocument::DisplayMode;
