#pragma once

#include <QAbstractListModel>
#include <QImage>
#include <QSize>
#include <QString>
#include <QUuid>
#include <QVector>

struct WorkspaceEntry {
    [[nodiscard]] bool is_source() const noexcept { return !source_path.isEmpty(); }
    [[nodiscard]] bool is_heatmap() const noexcept { return !heatmap.isNull(); }

    QUuid entry_id;
    QString source_path;
    QImage source_image;
    QImage heatmap;
    QString title;
    QString subtitle;
};

class WorkspaceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool apply_color_profile READ apply_color_profile NOTIFY apply_color_profile_changed)
    Q_PROPERTY(bool can_build_heatmap READ can_build_heatmap NOTIFY can_build_heatmap_changed)

   public:
    enum Role {
        EntryIdRole = Qt::UserRole + 1,
        SourcePathRole,
        HeatmapRole,
        TitleRole,
        SubtitleRole,
        IsSourceRole,
    };

    explicit WorkspaceModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QUuid add_source(const QString& path, const QSize& pixel_size);
    void add_heatmap(QImage heatmap, QString subtitle);
    [[nodiscard]] int entry_count() const noexcept { return static_cast<int>(m_entries.size()); }
    [[nodiscard]] WorkspaceEntry entry_at(int index) const;
    [[nodiscard]] bool remove_entry(const QUuid& entry_id);
    bool remove_heatmap();
    [[nodiscard]] bool move_entry(const QUuid& entry_id, int direction);
    [[nodiscard]] bool apply_color_profile() const noexcept { return m_apply_color_profile; }
    [[nodiscard]] bool can_build_heatmap() const noexcept;
    void toggle_color_profile();
    Q_INVOKABLE void set_source_image(const QString& entry_id, QImage image);

   Q_SIGNALS:
    void apply_color_profile_changed();
    void can_build_heatmap_changed();

   private:
    static constexpr int k_max_entries = 12;

    QVector<WorkspaceEntry> m_entries;
    bool m_apply_color_profile = true;
};
