#pragma once

#include <QHash>
#include <QImage>
#include <QPoint>
#include <QString>
#include <QtGlobal>

struct TileTextureKey {
    QString image_path;
    int display_mode = 0;
    int level = 0;
    QPoint tile_index;

    friend bool operator==(const TileTextureKey&, const TileTextureKey&) = default;
};

Q_ALWAYS_INLINE uint qHash(const TileTextureKey& key, uint seed = 0) noexcept {
    return qHashMulti(seed, key.image_path, key.display_mode, key.level, key.tile_index.x(), key.tile_index.y());
}

class TileTextureCache {
   public:
    void store_tile(const QString& image_path, int display_mode, int level, const QPoint& tile_index, const QImage& tile_image);
    [[nodiscard]] const QImage* tile_ptr(const QString& image_path, int display_mode, int level, const QPoint& tile_index) const;
    void clear();

   private:
    [[nodiscard]] static Q_ALWAYS_INLINE TileTextureKey key(const QString& image_path, int display_mode, int level,
                                                            const QPoint& tile_index) {
        return TileTextureKey{
            .image_path = image_path,
            .display_mode = display_mode,
            .level = level,
            .tile_index = tile_index,
        };
    }

    QHash<TileTextureKey, QImage> m_tile_by_key;
};
