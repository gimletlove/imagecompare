#include "core/ViewableImageListModel.h"

ViewableImageListModel::ViewableImageListModel(QObject* parent) : QAbstractListModel(parent) {}

int ViewableImageListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant ViewableImageListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const auto& entry = m_entries[index.row()];
    switch (role) {
        case EntryIdRole:
            return entry.entry_id();
        case ImageHandleIdRole:
            return entry.image_handle_id();
        case ImagePathRole:
            return entry.image_path();
        case PrimaryHeaderRole:
            return entry.primary_header_label();
        case SecondaryHeaderRole:
            return entry.secondary_header_label();
        case IsSourceRole:
            return entry.is_source();
        case IsDerivedRole:
            return entry.is_derived();
        case LabelRole:
            return entry.label();
        case PixelSizeRole:
            return entry.pixel_size();
        case EntryKindRole:
            return entry.entry_kind_name();
        default:
            return {};
    }
}

QHash<int, QByteArray> ViewableImageListModel::roleNames() const {
    return {
        {EntryIdRole, "entryId"},
        {ImageHandleIdRole, "imageHandleId"},
        {ImagePathRole, "imagePath"},
        {PrimaryHeaderRole, "primaryHeader"},
        {SecondaryHeaderRole, "secondaryHeader"},
        {IsSourceRole, "isSource"},
        {IsDerivedRole, "isDerived"},
        {LabelRole, "label"},
        {PixelSizeRole, "pixelSize"},
        {EntryKindRole, "entryKind"},
    };
}

void ViewableImageListModel::append_entry(const ViewableImageEntry& entry) {
    const int next_row = m_entries.size();
    beginInsertRows(QModelIndex(), next_row, next_row);
    m_entries.push_back(entry);
    endInsertRows();
}

void ViewableImageListModel::remove_entry_at(int index) {
    if (index < 0 || index >= m_entries.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), index, index);
    m_entries.removeAt(index);
    endRemoveRows();
}

void ViewableImageListModel::clear_entries() {
    if (m_entries.isEmpty()) {
        return;
    }
    beginRemoveRows(QModelIndex(), 0, m_entries.size() - 1);
    m_entries.clear();
    endRemoveRows();
}
