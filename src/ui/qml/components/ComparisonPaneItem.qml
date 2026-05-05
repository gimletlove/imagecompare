import QtQuick
import QtQuick.Controls
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
    property bool active: false
    property bool can_move: true

    readonly property real zoom_factor_value: pane_image.zoom_factor
    readonly property point image_center_value: pane_image.image_center
    readonly property real image_pixel_width: pane_image.image_pixel_size.width
    readonly property real image_pixel_height: pane_image.image_pixel_size.height

    signal remove_requested(string entry_id)
    signal activate_requested(string entry_id)
    signal view_changed(var pane, real zoom_factor, point image_center)
    signal copy_path_requested(string path)
    signal open_folder_requested(string path)
    signal move_requested(string entry_id, int direction)
    signal export_heatmap_requested(string entry_id)

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

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: function(eventPoint, button) {
            root.activate_requested(root.entry_id_value)
            pane_context_menu.x = eventPoint.position.x
            pane_context_menu.y = eventPoint.position.y
            pane_context_menu.open()
        }
    }

    Menu {
        id: pane_context_menu

        MenuItem {
            text: "Copy Path (Ctrl+C)"
            visible: root.source_pane_value
            height: visible ? implicitHeight : 0
            onTriggered: root.copy_path_requested(root.image_path_value)
        }

        MenuItem {
            text: "Open Containing Folder"
            visible: root.source_pane_value
            height: visible ? implicitHeight : 0
            onTriggered: root.open_folder_requested(root.image_path_value)
        }

        MenuSeparator {
            visible: root.source_pane_value
            height: visible ? implicitHeight : 0
        }

        MenuItem {
            text: "Move Left (Ctrl+Left)"
            enabled: root.can_move
            onTriggered: root.move_requested(root.entry_id_value, -1)
        }

        MenuItem {
            text: "Move Right (Ctrl+Right)"
            enabled: root.can_move
            onTriggered: root.move_requested(root.entry_id_value, 1)
        }

        MenuSeparator {
            visible: !root.source_pane_value
            height: visible ? implicitHeight : 0
        }

        MenuItem {
            text: "Export Heatmap"
            visible: !root.source_pane_value
            height: visible ? implicitHeight : 0
            onTriggered: root.export_heatmap_requested(root.entry_id_value)
        }

        MenuSeparator {}

        MenuItem {
            text: "Close File (Ctrl+W)"
            onTriggered: root.remove_requested(root.entry_id_value)
        }
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
        anchors.left: pane_header.left
        anchors.right: pane_header.right
        anchors.top: pane_header.bottom
        visible: root.active
        height: 1
        color: "#6aa7ff"
        z: 2
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
        onActivated: root.activate_requested(root.entry_id_value)
        onZoom_factor_changed: root.view_changed(root, pane_image.zoom_factor, pane_image.image_center)
        onImage_center_changed: root.view_changed(root, pane_image.zoom_factor, pane_image.image_center)
    }
}
