#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "core/ViewableImageEntry.h"

class ViewableImageListModel : public QAbstractListModel {
    Q_OBJECT

   public:
    enum Role {
        EntryIdRole = Qt::UserRole + 1,
        ImageHandleIdRole,
        ImagePathRole,
        PrimaryHeaderRole,
        SecondaryHeaderRole,
        IsSourceRole,
        IsDerivedRole,
        LabelRole,
        PixelSizeRole,
        EntryKindRole,
    };
    Q_ENUM(Role)

    explicit ViewableImageListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void append_entry(const ViewableImageEntry& entry);
    void remove_entry_at(int index);
    void clear_entries();

   private:
    QVector<ViewableImageEntry> m_entries;
};
