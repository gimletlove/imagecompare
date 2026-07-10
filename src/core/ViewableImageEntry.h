#pragma once

#include <QSize>
#include <QString>
#include <QUuid>
#include <cstdint>

struct ViewableImageEntry {
    enum class EntryKind : std::uint8_t {
        Source,
        Derived,
    };

    static ViewableImageEntry from_source(QUuid entry_id, QString path, QSize pixel_size);
    static ViewableImageEntry from_derived_heatmap(QUuid entry_id, QString output_path, QSize pixel_size,
                                                   QString secondary_header_label = {});

    [[nodiscard]] bool is_source() const noexcept { return kind == EntryKind::Source; }
    [[nodiscard]] bool is_derived() const noexcept { return kind == EntryKind::Derived; }

    EntryKind kind = EntryKind::Source;
    QUuid entry_id;
    QString image_path;
    QString primary_header_label;
    QString secondary_header_label;
    QSize pixel_size;
};
