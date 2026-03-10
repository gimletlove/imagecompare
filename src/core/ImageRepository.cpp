#include "core/ImageRepository.h"

#include <QFileInfo>
#include <mutex>
#include <stdexcept>

QUuid ImageRepository::load(const QString& path) {
    const QFileInfo file_info(path);
    const SourceIdentity identity = SourceIdentity::from_file_info(file_info);
    if (!identity.is_valid()) {
        throw std::runtime_error("image path does not exist");
    }
    const QString key = identity_key(identity);

    std::unique_lock lock(m_mutex);
    if (const auto found = m_handle_by_source_identity.find(key); found != m_handle_by_source_identity.end()) {
        return found.value();
    }

    const QUuid handle_id = QUuid::createUuid();
    const QString source_path = identity.canonical_path.isEmpty() ? file_info.absoluteFilePath() : identity.canonical_path;
    m_source_by_handle.insert(handle_id, std::make_shared<ImageSource>(source_path));
    m_handle_by_source_identity.insert(key, handle_id);
    m_identity_key_by_handle.insert(handle_id, key);
    return handle_id;
}

bool ImageRepository::release(const QUuid& image_handle_id) {
    std::unique_lock lock(m_mutex);
    const auto found = m_source_by_handle.find(image_handle_id);
    if (found == m_source_by_handle.end()) {
        return false;
    }

    m_source_by_handle.erase(found);

    const QString identity_key = m_identity_key_by_handle.take(image_handle_id);
    if (!identity_key.isEmpty()) {
        m_handle_by_source_identity.remove(identity_key);
    }
    return true;
}

std::shared_ptr<const ImageSource> ImageRepository::image(QUuid image_handle_id) const {
    std::shared_lock lock(m_mutex);
    if (const auto found = m_source_by_handle.find(image_handle_id); found != m_source_by_handle.end()) {
        return found.value();
    }
    throw std::runtime_error("unknown image handle");
}

QString ImageRepository::identity_key(const SourceIdentity& identity) {
    return QStringLiteral("%1|%2|%3").arg(identity.canonical_path).arg(identity.file_size).arg(identity.modified_at.toMSecsSinceEpoch());
}
