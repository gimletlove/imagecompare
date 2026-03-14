#pragma once

#include <QObject>
#include <QStringList>

class QMimeData;
class QUrl;

class ApplicationController;

class WindowDropFilter : public QObject {
    Q_OBJECT

   public:
    explicit WindowDropFilter(ApplicationController* controller, QObject* parent = nullptr);

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

   private:
    [[nodiscard]] QList<QUrl> decode_urls(const QMimeData* mime_data) const;
    [[nodiscard]] QStringList normalize_urls(const QList<QUrl>& urls) const;

    ApplicationController* m_controller = nullptr;
};
