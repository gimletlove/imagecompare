#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "core/ViewableImageEntry.h"

class ViewableImageListModel : public QAbstractListModel {
    Q_OBJECT

   public:
    enum Role {
        EntryIdRole = Qt::UserRole + 1,
        ImagePathRole,
        PrimaryHeaderRole,
        SecondaryHeaderRole,
        IsSourceRole,
    };
    Q_ENUM(Role)

    explicit ViewableImageListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void append_entry(const ViewableImageEntry& entry);
    void remove_entry_at(int index);
    void move_entry(int from, int to);

   private:
    QVector<ViewableImageEntry> m_entries;
};
