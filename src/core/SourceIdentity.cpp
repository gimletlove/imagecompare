#include "core/SourceIdentity.h"

SourceIdentity SourceIdentity::from_file_info(const QFileInfo& info) {
    SourceIdentity identity;
    if (!info.exists()) {
        return identity;
    }

    identity.canonical_path = info.canonicalFilePath();
    if (identity.canonical_path.isEmpty()) {
        identity.canonical_path = info.absoluteFilePath();
    }
    identity.file_size = info.size();
    identity.modified_at = info.lastModified().toUTC();
    return identity;
}

bool SourceIdentity::is_valid() const noexcept { return !canonical_path.isEmpty() && file_size >= 0 && modified_at.isValid(); }
