#include "core/WorkspaceModel.h"

#include <QFileInfo>
#include <QLocale>
#include <algorithm>
#include <utility>

WorkspaceModel::WorkspaceModel(QObject* parent) : QAbstractListModel(parent) {}

int WorkspaceModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : entry_count(); }

QVariant WorkspaceModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const auto& entry = m_entries[index.row()];
    switch (role) {
        case EntryIdRole:
            return entry.entry_id;
        case SourcePathRole:
            return entry.source_path;
        case HeatmapRole:
            return entry.heatmap;
        case TitleRole:
            return entry.title;
        case SubtitleRole:
            return entry.subtitle;
        case IsSourceRole:
            return entry.is_source();
        default:
            return {};
    }
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const {
    return {
        {EntryIdRole, "entryId"}, {SourcePathRole, "sourcePath"}, {HeatmapRole, "heatmapImage"},
        {TitleRole, "title"},     {SubtitleRole, "subtitle"},     {IsSourceRole, "isSource"},
    };
}

QUuid WorkspaceModel::add_source(const QString& path, const QSize& pixel_size) {
    if (entry_count() >= k_max_entries || path.isEmpty() || pixel_size.isEmpty()) {
        return {};
    }

    WorkspaceEntry entry;
    entry.entry_id = QUuid::createUuid();
    entry.source_path = path;
    const QFileInfo file_info(path);
    entry.title = file_info.fileName();
    const QString resolution = QStringLiteral("%1x%2").arg(pixel_size.width()).arg(pixel_size.height());
    entry.subtitle =
        QStringLiteral("%1 • %2").arg(resolution, QLocale().formattedDataSize(file_info.size(), 2, QLocale::DataSizeTraditionalFormat));

    const int row = entry_count();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.push_back(std::move(entry));
    endInsertRows();
    Q_EMIT can_build_heatmap_changed();
    return m_entries.constLast().entry_id;
}

void WorkspaceModel::add_heatmap(QImage heatmap, QString subtitle) {
    WorkspaceEntry entry;
    entry.entry_id = QUuid::createUuid();
    entry.heatmap = std::move(heatmap);
    entry.title = QStringLiteral("Heatmap");
    entry.subtitle = std::move(subtitle);

    const int row = entry_count();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.push_back(std::move(entry));
    endInsertRows();
    Q_EMIT can_build_heatmap_changed();
}

WorkspaceEntry WorkspaceModel::entry_at(int index) const { return m_entries.at(index); }

bool WorkspaceModel::remove_entry(const QUuid& entry_id) {
    const auto entry = std::ranges::find(m_entries, entry_id, &WorkspaceEntry::entry_id);
    if (entry == m_entries.end()) {
        return false;
    }

    const int index = static_cast<int>(entry - m_entries.begin());
    beginRemoveRows(QModelIndex(), index, index);
    m_entries.erase(entry);
    endRemoveRows();
    Q_EMIT can_build_heatmap_changed();
    return true;
}

bool WorkspaceModel::remove_heatmap() {
    const auto entry = std::ranges::find_if(m_entries, &WorkspaceEntry::is_heatmap);
    return entry != m_entries.end() && remove_entry(entry->entry_id);
}

bool WorkspaceModel::move_entry(const QUuid& entry_id, int direction) {
    if (direction == 0) {
        return false;
    }

    const auto entry = std::ranges::find(m_entries, entry_id, &WorkspaceEntry::entry_id);
    if (entry == m_entries.end()) {
        return false;
    }
    const int from = static_cast<int>(entry - m_entries.begin());

    const int to = std::clamp(from + (direction < 0 ? -1 : 1), 0, entry_count() - 1);
    if (from == to) {
        return false;
    }

    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    m_entries.move(from, to);
    endMoveRows();
    return true;
}

void WorkspaceModel::toggle_color_profile() {
    m_apply_color_profile = !m_apply_color_profile;
    Q_EMIT apply_color_profile_changed();
}

bool WorkspaceModel::can_build_heatmap() const noexcept {
    return m_entries.size() == 2 && m_entries[0].is_source() && m_entries[1].is_source() && !m_entries[0].source_image.isNull() &&
           !m_entries[1].source_image.isNull() && m_entries[0].source_image.size() == m_entries[1].source_image.size();
}

void WorkspaceModel::set_source_image(const QString& entry_id, QImage image) {
    const auto entry = std::ranges::find(m_entries, QUuid(entry_id), &WorkspaceEntry::entry_id);
    if (entry == m_entries.end() || !entry->is_source() || entry->source_image.cacheKey() == image.cacheKey()) {
        return;
    }
    entry->source_image = std::move(image);
    Q_EMIT can_build_heatmap_changed();
}
