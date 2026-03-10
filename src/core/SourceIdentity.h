#pragma once

#include <QDateTime>
#include <QFileInfo>
#include <QString>

struct SourceIdentity {
    QString canonical_path;
    qint64 file_size = -1;
    QDateTime modified_at;

    [[nodiscard]] static SourceIdentity from_file_info(const QFileInfo& info);
    [[nodiscard]] bool is_valid() const noexcept;

    friend bool operator==(const SourceIdentity&, const SourceIdentity&) = default;
};
