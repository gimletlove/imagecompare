import QtQuick
import ImageCompare 1.0

Item {
    id: root
    property string primary_header_text: ""
    property string secondary_header_text: ""
    property string entry_id_value: ""
    property string image_path_value: ""
    property bool source_pane_value: false
    property int display_mode_value: 1
    property real shared_zoom_factor: 1.0

    readonly property real zoom_factor_value: pane_image.zoom_factor
    readonly property point image_center_value: pane_image.image_center
    readonly property real image_pixel_width: pane_image.image_pixel_size.width
    readonly property real image_pixel_height: pane_image.image_pixel_size.height

    signal remove_requested(string entry_id)
    signal view_changed(var pane, real zoom_factor, point image_center)

    function apply_shared_view_state(zoom_value, center_point) {
        pane_image.zoom_factor = zoom_value
        pane_image.image_center = center_point
    }

    function apply_best_fit() {
        pane_image.set_best_fit()
    }

    function current_best_fit_zoom() {
        return pane_image.best_fit_zoom()
    }

    function set_zoom100() {
        pane_image.zoom_factor = 1.0
    }

    PaneHeader {
        id: pane_header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        primary_text: root.primary_header_text
        secondary_text: root.secondary_header_text
        onRemove_requested: root.remove_requested(root.entry_id_value)
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: pane_header.bottom
        anchors.topMargin: 5
        anchors.bottom: parent.bottom
        color: "#000000"
    }

    TiledImageItem {
        id: pane_image
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: pane_header.bottom
        anchors.topMargin: 5
        anchors.bottom: parent.bottom
        image_path: root.image_path_value
        display_mode: root.display_mode_value
        viewport_rect: Qt.rect(0, 0, width, height)
        zoom_factor: root.shared_zoom_factor
        clip: true
        onZoom_factor_changed: root.view_changed(root, pane_image.zoom_factor, pane_image.image_center)
        onImage_center_changed: root.view_changed(root, pane_image.zoom_factor, pane_image.image_center)
    }
}
