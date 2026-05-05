#pragma once

#include <QHash>
#include <QImage>
#include <QPoint>
#include <QtGlobal>

struct TileTextureKey {
    int display_mode = 0;
    int level = 0;
    QPoint tile_index;

    friend bool operator==(const TileTextureKey&, const TileTextureKey&) = default;
};

Q_ALWAYS_INLINE uint qHash(const TileTextureKey& key, uint seed = 0) noexcept {
    return qHashMulti(seed, key.display_mode, key.level, key.tile_index.x(), key.tile_index.y());
}

class TileTextureCache {
   public:
    void store_tile(int display_mode, int level, const QPoint& tile_index, const QImage& tile_image);
    [[nodiscard]] const QImage* tile_ptr(int display_mode, int level, const QPoint& tile_index) const;
    [[nodiscard]] bool empty() const noexcept;
    void clear();

   private:
    [[nodiscard]] static Q_ALWAYS_INLINE TileTextureKey key(int display_mode, int level, const QPoint& tile_index) {
        return TileTextureKey{
            .display_mode = display_mode,
            .level = level,
            .tile_index = tile_index,
        };
    }

    QHash<TileTextureKey, QImage> m_tile_by_key;
};
