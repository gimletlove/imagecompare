#pragma once

#include <QHash>
#include <QString>
#include <QUuid>
#include <memory>
#include <shared_mutex>

#include "core/ImageSource.h"
#include "core/SourceIdentity.h"

class ImageRepository {
   public:
    QUuid load(const QString& path);
    bool release(const QUuid& image_handle_id);
    [[nodiscard]] std::shared_ptr<const ImageSource> image(QUuid image_handle_id) const;

   private:
    static QString identity_key(const SourceIdentity& identity);

    QHash<QString, QUuid> m_handle_by_source_identity;
    QHash<QUuid, QString> m_identity_key_by_handle;
    QHash<QUuid, std::shared_ptr<ImageSource>> m_source_by_handle;
    mutable std::shared_mutex m_mutex;
};
