import QtQuick

QtObject {
    id: root

    property var pane_repeater
    property real shared_zoom_factor: 1.0
    property point shared_image_center: Qt.point(0, 0)
    property bool shared_best_fit_active: true
    property bool match_zoom_enabled: false
    property var last_sync_source_pane: null
    property bool best_fit_refresh_scheduled: false
    property bool syncing_view_state: false
    readonly property string zoom_readout: Math.round(Number(shared_zoom_factor) * 100.0).toString() + "%"

    function first_visible_pane() {
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane && pane.visible) {
                return pane;
            }
        }
        return pane_repeater.count > 0 ? pane_repeater.itemAt(0) : null;
    }

    function pane_is_active(pane) {
        if (!pane) {
            return false;
        }
        for (let index = 0; index < pane_repeater.count; ++index) {
            if (pane_repeater.itemAt(index) === pane) {
                return pane.visible;
            }
        }
        return false;
    }

    function active_sync_pane() {
        if (pane_is_active(last_sync_source_pane)) {
            return last_sync_source_pane;
        }
        return first_visible_pane();
    }

    function reconcile_match_zoom_now() {
        if (!match_zoom_enabled || pane_repeater.count <= 0) {
            return false;
        }
        const source_pane = active_sync_pane();
        if (!source_pane) {
            return false;
        }
        update_shared_from_pane(source_pane, source_pane.zoom_factor_value, source_pane.image_center_value);
        return true;
    }

    function restore_numeric_zoom_now() {
        if (match_zoom_enabled || pane_repeater.count <= 0) {
            return false;
        }
        const source_pane = active_sync_pane();
        if (!source_pane) {
            return false;
        }
        update_shared_from_pane(source_pane, source_pane.zoom_factor_value, source_pane.image_center_value);
        return true;
    }

    function update_shared_from_pane(source_pane, zoom_value, center_point) {
        if (!source_pane || syncing_view_state) {
            return;
        }

        const has_zoom_value = zoom_value !== undefined && zoom_value !== null;
        const has_center_point = center_point !== undefined && center_point !== null;

        syncing_view_state = true;
        shared_best_fit_active = false;
        shared_zoom_factor = has_zoom_value ? Number(zoom_value) : source_pane.zoom_factor_value;
        shared_image_center = has_center_point ? center_point : source_pane.image_center_value;
        last_sync_source_pane = source_pane;

        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane || pane === source_pane) {
                continue;
            }
            let target_zoom = shared_zoom_factor;
            let target_center = shared_image_center;
            if (match_zoom_enabled) {
                const source_best_fit = Number(source_pane.current_best_fit_zoom());
                const target_best_fit = Number(pane.current_best_fit_zoom());
                if (source_best_fit > 0.0 && target_best_fit > 0.0) {
                    const normalized_zoom = shared_zoom_factor / source_best_fit;
                    if (isFinite(normalized_zoom) && normalized_zoom > 0.0) {
                        target_zoom = normalized_zoom * target_best_fit;
                    }
                }

                const source_width = Number(source_pane.image_pixel_width);
                const source_height = Number(source_pane.image_pixel_height);
                const target_width = Number(pane.image_pixel_width);
                const target_height = Number(pane.image_pixel_height);
                if (source_width > 0.0 && source_height > 0.0 && target_width > 0.0 && target_height > 0.0) {
                    const normalized_x = shared_image_center.x / source_width;
                    const normalized_y = shared_image_center.y / source_height;
                    if (isFinite(normalized_x) && isFinite(normalized_y)) {
                        target_center = Qt.point(normalized_x * target_width, normalized_y * target_height);
                    }
                }
            }
            pane.apply_shared_view_state(target_zoom, target_center);
        }
        syncing_view_state = false;
    }

    function set_zoom100() {
        if (pane_repeater.count <= 0) {
            return false;
        }

        if (match_zoom_enabled) {
            const source_pane = active_sync_pane();
            if (!source_pane) {
                return false;
            }
            source_pane.set_zoom100();
            update_shared_from_pane(source_pane, source_pane.zoom_factor_value, source_pane.image_center_value);
            return true;
        }

        syncing_view_state = true;
        shared_best_fit_active = false;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane) {
                continue;
            }
            pane.set_zoom100();
        }
        syncing_view_state = false;

        const first_pane = pane_repeater.itemAt(0);
        if (first_pane) {
            shared_zoom_factor = first_pane.zoom_factor_value;
            shared_image_center = first_pane.image_center_value;
        }
        return true;
    }

    function set_best_fit() {
        if (pane_repeater.count <= 0) {
            return false;
        }

        syncing_view_state = true;
        shared_best_fit_active = true;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane) {
                continue;
            }
            pane.apply_best_fit();
        }
        syncing_view_state = false;

        const first_pane = pane_repeater.itemAt(0);
        if (first_pane) {
            shared_zoom_factor = first_pane.zoom_factor_value;
            shared_image_center = first_pane.image_center_value;
        }
        return true;
    }

    function schedule_best_fit_refresh() {
        if (!shared_best_fit_active || best_fit_refresh_scheduled) {
            return;
        }
        best_fit_refresh_scheduled = true;
        Qt.callLater(function() {
            best_fit_refresh_scheduled = false;
            if (shared_best_fit_active) {
                set_best_fit();
            }
        });
    }

}
