#include "ui/TileTextureCache.h"

void TileTextureCache::store_tile(const QString& image_path, int display_mode, int level, const QPoint& tile_index,
                                  const QImage& tile_image) {
    m_tile_by_key.insert(key(image_path, display_mode, level, tile_index), tile_image);
}

const QImage* TileTextureCache::tile_ptr(const QString& image_path, int display_mode, int level, const QPoint& tile_index) const {
    const auto it = m_tile_by_key.constFind(key(image_path, display_mode, level, tile_index));
    if (it == m_tile_by_key.cend()) {
        return nullptr;
    }
    return &it.value();
}

void TileTextureCache::clear() { m_tile_by_key.clear(); }
