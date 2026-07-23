import QtQuick

QtObject {
    id: root

    property var pane_repeater
    property real shared_zoom_factor: 1.0
    property bool shared_best_fit_active: true
    property bool match_zoom_enabled: true
    property var last_sync_source_pane: null
    property bool best_fit_refresh_scheduled: false
    property bool syncing_view_state: false

    function first_visible_pane() {
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane && pane.visible) {
                return pane;
            }
        }
        return pane_repeater.count > 0 ? pane_repeater.itemAt(0) : null;
    }

    function pane_is_present(pane) {
        if (!pane) {
            return false;
        }
        for (let index = 0; index < pane_repeater.count; ++index) {
            if (pane_repeater.itemAt(index) === pane) {
                return true;
            }
        }
        return false;
    }

    function first_sync_pane(excluded_pane = null) {
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane && pane !== excluded_pane && pane.visible && !pane.synchronization_locked) {
                return pane;
            }
        }
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane && pane !== excluded_pane && !pane.synchronization_locked) {
                return pane;
            }
        }
        return null;
    }

    function active_sync_pane(excluded_pane = null) {
        if (last_sync_source_pane !== excluded_pane
                && pane_is_present(last_sync_source_pane)
                && last_sync_source_pane.visible
                && !last_sync_source_pane.synchronization_locked) {
            return last_sync_source_pane;
        }
        return first_sync_pane(excluded_pane);
    }

    function set_pane_from_source(source_pane, target_pane, zoom_value, center_point): void {
        let target_zoom = Number(zoom_value);
        let target_center = center_point;
        if (match_zoom_enabled) {
            const source_best_fit = Number(source_pane.image_item.best_fit_zoom());
            const target_best_fit = Number(target_pane.image_item.best_fit_zoom());
            if (source_best_fit > 0.0 && target_best_fit > 0.0) {
                const normalized_zoom = target_zoom / source_best_fit;
                if (isFinite(normalized_zoom) && normalized_zoom > 0.0) {
                    target_zoom = normalized_zoom * target_best_fit;
                }
            }

            const source_width = Number(source_pane.image_pixel_width);
            const source_height = Number(source_pane.image_pixel_height);
            const target_width = Number(target_pane.image_pixel_width);
            const target_height = Number(target_pane.image_pixel_height);
            if (source_width > 0.0 && source_height > 0.0 && target_width > 0.0 && target_height > 0.0) {
                const normalized_x = center_point.x / source_width;
                const normalized_y = center_point.y / source_height;
                if (isFinite(normalized_x) && isFinite(normalized_y)) {
                    target_center = Qt.point(normalized_x * target_width, normalized_y * target_height);
                }
            }
        }
        target_pane.image_item.zoom_factor = target_zoom;
        target_pane.image_item.image_center = target_center;
    }

    function rejoin_pane(pane, source_pane): bool {
        if (!source_pane) {
            shared_zoom_factor = Number(pane.zoom_factor_value);
            shared_best_fit_active = pane.local_best_fit_active;
            last_sync_source_pane = pane;
            return true;
        }

        syncing_view_state = true;
        if (shared_best_fit_active) {
            pane.image_item.set_best_fit();
        } else {
            set_pane_from_source(source_pane, pane, source_pane.zoom_factor_value, source_pane.image_center_value);
        }
        shared_zoom_factor = Number(source_pane.zoom_factor_value);
        last_sync_source_pane = source_pane;
        syncing_view_state = false;
        return true;
    }

    function sync_current_view_now(): bool {
        const source_pane = active_sync_pane();
        if (!source_pane) {
            return false;
        }
        update_shared_from_pane(source_pane, source_pane.zoom_factor_value, source_pane.image_center_value);
        return true;
    }

    function update_shared_from_pane(source_pane, zoom_value, center_point) {
        if (!source_pane || syncing_view_state || source_pane.synchronization_locked) {
            return;
        }

        syncing_view_state = true;
        shared_best_fit_active = false;
        shared_zoom_factor = Number(zoom_value);
        last_sync_source_pane = source_pane;

        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane || pane === source_pane || pane.synchronization_locked) {
                continue;
            }
            set_pane_from_source(source_pane, pane, shared_zoom_factor, center_point);
        }
        syncing_view_state = false;
    }

    function set_zoom100(active_pane): bool {
        const source_pane = active_pane || active_sync_pane();
        if (!source_pane) {
            return false;
        }

        if (source_pane.synchronization_locked) {
            source_pane.image_item.zoom_factor = 1.0;
            source_pane.local_best_fit_active = false;
            return true;
        }

        if (match_zoom_enabled) {
            source_pane.image_item.zoom_factor = 1.0;
            update_shared_from_pane(source_pane, source_pane.zoom_factor_value, source_pane.image_center_value);
            return true;
        }

        syncing_view_state = true;
        shared_best_fit_active = false;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane || pane.synchronization_locked) {
                continue;
            }
            pane.image_item.zoom_factor = 1.0;
        }
        syncing_view_state = false;

        shared_zoom_factor = Number(source_pane.zoom_factor_value);
        last_sync_source_pane = source_pane;
        return true;
    }

    function set_best_fit(active_pane): bool {
        const source_pane = active_pane || active_sync_pane();
        if (!source_pane) {
            return false;
        }

        if (source_pane.synchronization_locked) {
            source_pane.image_item.set_best_fit();
            source_pane.local_best_fit_active = true;
            return true;
        }

        syncing_view_state = true;
        shared_best_fit_active = true;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane || pane.synchronization_locked) {
                continue;
            }
            pane.image_item.set_best_fit();
        }
        syncing_view_state = false;

        shared_zoom_factor = Number(source_pane.zoom_factor_value);
        last_sync_source_pane = source_pane;
        return true;
    }

    function toggle_zoom_fit(active_pane): bool {
        const source_pane = active_pane || active_sync_pane();
        if (!source_pane) {
            return false;
        }

        const current_zoom = Number(source_pane.zoom_factor_value);
        const best_fit_zoom = Number(source_pane.image_item.best_fit_zoom());
        if (best_fit_zoom > 0.0 && Math.abs(current_zoom - best_fit_zoom) <= Math.max(0.001, best_fit_zoom * 0.01)) {
            return set_zoom100(source_pane);
        }
        return set_best_fit(source_pane);
    }

    function refresh_best_fit(): void {
        syncing_view_state = true;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane) {
                continue;
            }
            if ((!pane.synchronization_locked && shared_best_fit_active)
                    || (pane.synchronization_locked && pane.local_best_fit_active)) {
                pane.image_item.set_best_fit();
            }
        }
        syncing_view_state = false;

        const source_pane = active_sync_pane();
        if (source_pane && shared_best_fit_active) {
            shared_zoom_factor = Number(source_pane.zoom_factor_value);
            last_sync_source_pane = source_pane;
        }
    }

    function schedule_best_fit_refresh(): void {
        if (best_fit_refresh_scheduled) {
            return;
        }
        best_fit_refresh_scheduled = true;
        Qt.callLater(function() {
            best_fit_refresh_scheduled = false;
            refresh_best_fit();
        });
    }

}
