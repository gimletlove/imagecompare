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
    QImage heatmap;
    QString title;
    QString subtitle;
    QSize pixel_size;
};

class WorkspaceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(ColorMode color_mode READ color_mode WRITE set_color_mode NOTIFY color_mode_changed)
    Q_PROPERTY(bool can_build_heatmap READ can_build_heatmap NOTIFY entry_count_changed)

   public:
    enum class ColorMode {
        Raw,
        Faithful,
    };
    Q_ENUM(ColorMode)

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
    [[nodiscard]] QUuid add_heatmap(QImage heatmap, QString subtitle = {});
    [[nodiscard]] int entry_count() const noexcept { return static_cast<int>(m_entries.size()); }
    [[nodiscard]] WorkspaceEntry entry_at(int index) const;
    [[nodiscard]] bool remove_entry(const QUuid& entry_id);
    [[nodiscard]] bool remove_heatmap();
    [[nodiscard]] bool move_entry(const QUuid& entry_id, int direction);
    [[nodiscard]] ColorMode color_mode() const noexcept { return m_color_mode; }
    [[nodiscard]] bool can_build_heatmap() const noexcept;
    void set_color_mode(ColorMode mode);

   Q_SIGNALS:
    void entry_count_changed();
    void color_mode_changed();

   private:
    static constexpr int k_max_entries = 10;

    QVector<WorkspaceEntry> m_entries;
    ColorMode m_color_mode = ColorMode::Faithful;
};

using ColorMode = WorkspaceModel::ColorMode;
