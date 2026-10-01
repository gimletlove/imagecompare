import QtQuick

QtObject {
    id: root

    property var pane_repeater
    property bool shared_best_fit_active: true
    property bool match_zoom_enabled: true
    property var last_sync_source_pane: null
    property bool syncing_view_state: false

    function panes() {
        const result = [];
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane) {
                result.push(pane);
            }
        }
        return result;
    }

    function unlocked_panes() {
        return panes().filter(pane => !pane.synchronization_locked);
    }

    function first_visible_pane() {
        const all = panes();
        return all.find(pane => pane.visible) || all[0] || null;
    }

    function first_sync_pane(excluded_pane = null) {
        const candidates = unlocked_panes().filter(pane => pane !== excluded_pane);
        return candidates.find(pane => pane.visible) || candidates[0] || null;
    }

    function active_sync_pane(excluded_pane = null) {
        if (last_sync_source_pane !== excluded_pane
                && panes().includes(last_sync_source_pane)
                && last_sync_source_pane.visible
                && !last_sync_source_pane.synchronization_locked) {
            return last_sync_source_pane;
        }
        return first_sync_pane(excluded_pane);
    }

    function set_pane_from_source(source_pane, target_pane, zoom_value, center_point): void {
        let target_zoom = zoom_value;
        let target_center = center_point;
        if (match_zoom_enabled) {
            target_zoom = zoom_value / source_pane.image_item.best_fit_zoom() * target_pane.image_item.best_fit_zoom();
            const from = source_pane.image_item.image_pixel_size;
            const to = target_pane.image_item.image_pixel_size;
            if (from.width > 0 && from.height > 0 && to.width > 0 && to.height > 0) {
                target_center = Qt.point(center_point.x / from.width * to.width, center_point.y / from.height * to.height);
            }
        }
        target_pane.image_item.zoom_factor = target_zoom;
        target_pane.image_item.image_center = target_center;
    }

    function rejoin_pane(pane, source_pane): void {
        if (!source_pane) {
            shared_best_fit_active = pane.local_best_fit_active;
            last_sync_source_pane = pane;
            return;
        }

        syncing_view_state = true;
        if (shared_best_fit_active) {
            pane.image_item.set_best_fit();
        } else {
            set_pane_from_source(source_pane, pane, source_pane.image_item.zoom_factor, source_pane.image_item.image_center);
        }
        last_sync_source_pane = source_pane;
        syncing_view_state = false;
    }

    function sync_current_view_now(): void {
        const source_pane = active_sync_pane();
        if (source_pane) {
            update_shared_from_pane(source_pane, source_pane.image_item.zoom_factor, source_pane.image_item.image_center);
        }
    }

    function update_shared_from_pane(source_pane, zoom_value, center_point) {
        if (!source_pane || syncing_view_state || source_pane.synchronization_locked) {
            return;
        }

        syncing_view_state = true;
        shared_best_fit_active = false;
        last_sync_source_pane = source_pane;

        for (const pane of unlocked_panes()) {
            if (pane !== source_pane) {
                set_pane_from_source(source_pane, pane, zoom_value, center_point);
            }
        }
        syncing_view_state = false;
    }

    function set_zoom100(source_pane): void {
        if (source_pane.synchronization_locked) {
            source_pane.image_item.zoom_factor = 1.0;
            source_pane.local_best_fit_active = false;
            return;
        }

        if (match_zoom_enabled) {
            source_pane.image_item.zoom_factor = 1.0;
            update_shared_from_pane(source_pane, source_pane.image_item.zoom_factor, source_pane.image_item.image_center);
            return;
        }

        syncing_view_state = true;
        shared_best_fit_active = false;
        for (const pane of unlocked_panes()) {
            pane.image_item.zoom_factor = 1.0;
        }
        syncing_view_state = false;

        last_sync_source_pane = source_pane;
    }

    function set_best_fit(source_pane): void {
        if (source_pane.synchronization_locked) {
            source_pane.image_item.set_best_fit();
            source_pane.local_best_fit_active = true;
            return;
        }

        syncing_view_state = true;
        shared_best_fit_active = true;
        for (const pane of unlocked_panes()) {
            pane.image_item.set_best_fit();
        }
        syncing_view_state = false;

        last_sync_source_pane = source_pane;
    }

    function toggle_zoom_fit(active_pane): void {
        const source_pane = active_pane || active_sync_pane();
        if (!source_pane) {
            return;
        }

        const current_zoom = Number(source_pane.image_item.zoom_factor);
        const best_fit_zoom = Number(source_pane.image_item.best_fit_zoom());
        if (best_fit_zoom > 0.0 && Math.abs(current_zoom - best_fit_zoom) <= Math.max(0.001, best_fit_zoom * 0.01)) {
            set_zoom100(source_pane);
        } else {
            set_best_fit(source_pane);
        }
    }

    function refresh_best_fit(): void {
        syncing_view_state = true;
        for (const pane of panes()) {
            if (pane.synchronization_locked ? pane.local_best_fit_active : shared_best_fit_active) {
                pane.image_item.set_best_fit();
            }
        }
        syncing_view_state = false;

        const source_pane = active_sync_pane();
        if (source_pane && shared_best_fit_active) {
            last_sync_source_pane = source_pane;
        }
    }

}
