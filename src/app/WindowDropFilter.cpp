#include "app/WindowDropFilter.h"

#include <KUrlMimeData>

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QUrl>

#include "app/ApplicationController.h"

WindowDropFilter::WindowDropFilter(ApplicationController* controller, QObject* parent)
    : QObject(parent), m_controller(controller) {}

bool WindowDropFilter::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched);

    if (event == nullptr || m_controller == nullptr) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
        case QEvent::DragEnter: {
            auto* drag_enter_event = static_cast<QDragEnterEvent*>(event);
            if (!decode_urls(drag_enter_event->mimeData()).isEmpty()) {
                drag_enter_event->acceptProposedAction();
                return false;
            }
            break;
        }
        case QEvent::Drop: {
            auto* drop_event = static_cast<QDropEvent*>(event);
            const QStringList normalized_paths = normalize_urls(decode_urls(drop_event->mimeData()));
            if (normalized_paths.isEmpty()) {
                break;
            }
            m_controller->import_image_paths(normalized_paths);
            drop_event->acceptProposedAction();
            return false;
        }
        default:
            break;
    }

    return QObject::eventFilter(watched, event);
}

QList<QUrl> WindowDropFilter::decode_urls(const QMimeData* mime_data) const {
    if (mime_data == nullptr) {
        return {};
    }

    QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(mime_data, KUrlMimeData::PreferLocalUrls);
    if (urls.isEmpty()) { // fallback
        urls = mime_data->urls(); 
    }
    return urls;
}

QStringList WindowDropFilter::normalize_urls(const QList<QUrl>& urls) const {
    QStringList normalized_paths;
    normalized_paths.reserve(urls.size());

    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            normalized_paths.push_back(url.toLocalFile());
            continue;
        }
        normalized_paths.push_back(url.toString());
    }
    return normalized_paths;
}
