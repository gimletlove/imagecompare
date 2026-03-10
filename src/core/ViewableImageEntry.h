#pragma once

#include <QSize>
#include <QString>
#include <QUuid>

class ViewableImageEntry {
   public:
    enum class EntryKind {
        Source,
        Derived,
    };

    static ViewableImageEntry from_source(QUuid entry_id, QUuid image_handle_id, QString path, QSize pixel_size);
    static ViewableImageEntry from_derived_heatmap(QUuid entry_id, QUuid image_handle_id, QString output_path, QSize pixel_size,
                                                   QString secondary_header_label = {});

    [[nodiscard]] bool is_source() const noexcept;
    [[nodiscard]] bool is_derived() const noexcept;
    [[nodiscard]] QSize pixel_size() const noexcept;
    [[nodiscard]] QUuid entry_id() const noexcept;
    [[nodiscard]] QUuid image_handle_id() const noexcept;
    [[nodiscard]] QString image_path() const;
    [[nodiscard]] QString primary_header_label() const;
    [[nodiscard]] QString secondary_header_label() const;
    [[nodiscard]] QString label() const;
    [[nodiscard]] QString entry_kind_name() const;

   private:
    EntryKind m_kind = EntryKind::Source;
    QUuid m_entry_id;
    QUuid m_image_handle_id;
    QString m_image_path;
    QString m_primary_header_label;
    QString m_secondary_header_label;
    QSize m_pixel_size;
};
