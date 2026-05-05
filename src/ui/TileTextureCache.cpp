#include "ui/TileTextureCache.h"

void TileTextureCache::store_tile(int display_mode, int level, const QPoint& tile_index, const QImage& tile_image) {
    m_tile_by_key.insert(key(display_mode, level, tile_index), tile_image);
}

const QImage* TileTextureCache::tile_ptr(int display_mode, int level, const QPoint& tile_index) const {
    const auto it = m_tile_by_key.constFind(key(display_mode, level, tile_index));
    if (it == m_tile_by_key.cend()) {
        return nullptr;
    }
    return &it.value();
}

bool TileTextureCache::empty() const noexcept { return m_tile_by_key.isEmpty(); }

void TileTextureCache::clear() { m_tile_by_key.clear(); }
