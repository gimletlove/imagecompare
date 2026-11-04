import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ImageCompare 1.0

Item {
    id: root
    property string title: ""
    property string subtitle: ""
    property string entry_id: ""
    property alias source_path: pane_image.source_path
    property alias input_image: pane_image.input_image
    property bool is_source: false
    property alias apply_color_profile: pane_image.apply_color_profile
    property bool synchronization_locked: false
    property bool local_best_fit_active: false
    property bool active: false
    property bool focused: false
    property bool can_move: true
    property var stack_entries: null
    property alias image_item: pane_image

    signal remove_requested(string entry_id)
    signal activate_requested(string entry_id)
    signal view_changed(var pane, real zoom_factor, point image_center)
    signal copy_path_requested(string path)
    signal open_folder_requested(string path)
    signal move_requested(string entry_id, int direction)
    signal export_heatmap_requested(string entry_id)
    signal focus_toggle_requested(string entry_id)
    signal synchronization_lock_toggle_requested(string entry_id)
    signal snapshot_changed(string entry_id, var image)

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: function(eventPoint, button) {
            root.activate_requested(root.entry_id)
            pane_context_menu.x = eventPoint.position.x
            pane_context_menu.y = eventPoint.position.y
            pane_context_menu.open()
        }
    }

    Menu {
        id: pane_context_menu

        MenuItem {
            text: "Focus Image (Enter)"
            checkable: true
            checked: root.focused
            onTriggered: root.focus_toggle_requested(root.entry_id)
        }

        MenuItem {
            text: "Lock synchronization (L)"
            checkable: true
            checked: root.synchronization_locked
            onTriggered: root.synchronization_lock_toggle_requested(root.entry_id)
        }

        MenuSeparator {}

        MenuItem {
            text: "Copy Path (Ctrl+C)"
            visible: root.is_source
            height: visible ? implicitHeight : 0
            onTriggered: root.copy_path_requested(root.source_path)
        }

        MenuItem {
            text: "Open Containing Folder"
            visible: root.is_source
            height: visible ? implicitHeight : 0
            onTriggered: root.open_folder_requested(root.source_path)
        }

        MenuSeparator {
            visible: root.is_source
            height: visible ? implicitHeight : 0
        }

        MenuItem {
            text: "Move Left (Ctrl+Left)"
            enabled: root.can_move
            onTriggered: root.move_requested(root.entry_id, -1)
        }

        MenuItem {
            text: "Move Right (Ctrl+Right)"
            enabled: root.can_move
            onTriggered: root.move_requested(root.entry_id, 1)
        }

        MenuSeparator {
            visible: !root.is_source
            height: visible ? implicitHeight : 0
        }

        MenuItem {
            text: "Export Heatmap"
            visible: !root.is_source
            height: visible ? implicitHeight : 0
            onTriggered: root.export_heatmap_requested(root.entry_id)
        }

        MenuSeparator {}

        MenuItem {
            text: "Close File (Ctrl+W)"
            onTriggered: root.remove_requested(root.entry_id)
        }
    }

    Item {
        id: pane_header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        implicitHeight: 26
        height: implicitHeight

        readonly property bool has_secondary_in_display: root.subtitle.trim().length > 0
        readonly property string full_header_tool_tip_text: has_secondary_in_display
            ? root.title + " • " + root.subtitle.trim()
            : root.title

        RowLayout {
            anchors.fill: parent
            spacing: 4

            Label {
                text: "[F]"
                visible: root.focused
                color: "orange"
                font.bold: true
                Accessible.name: "Focused view"
                HoverHandler {
                    id: focus_label_hover
                }
                ToolTip.delay: 300
                ToolTip.visible: focus_label_hover.hovered
                ToolTip.text: "Focused view (Enter to exit)"
            }

            Label {
                text: "[L]"
                visible: root.synchronization_locked
                color: "orange"
                font.bold: true
                Accessible.name: "Synchronization locked"
                HoverHandler {
                    id: lock_label_hover
                }
                ToolTip.delay: 300
                ToolTip.visible: lock_label_hover.hovered
                ToolTip.text: "Synchronization locked"
            }

            Repeater {
                model: root.stack_entries

                delegate: Rectangle {
                    id: stack_entry
                    required property string entryId
                    required property string title
                    required property bool isSource
                    readonly property bool current: entryId === root.entry_id

                    visible: isSource
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    color: current ? palette.highlight : "transparent"
                    radius: 2

                    Label {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        verticalAlignment: Text.AlignVCenter
                        text: stack_entry.title
                        color: stack_entry.current ? palette.highlightedText : palette.windowText
                        elide: Text.ElideMiddle
                    }

                    HoverHandler {
                        id: stack_entry_hover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: root.activate_requested(stack_entry.entryId)
                    }

                    ToolTip.delay: 300
                    ToolTip.visible: stack_entry_hover.hovered
                    ToolTip.text: stack_entry.title
                }
            }

            // Reserves the width of a large image's details, so the filenames don't shift when flipping between images.
            TextMetrics {
                id: stack_details_metrics
                font: stack_details.font
                text: "00000x00000 • 000.00 MB"
            }

            Label {
                id: stack_details
                visible: root.stack_entries !== null
                text: root.subtitle
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                Layout.preferredWidth: stack_details_metrics.advanceWidth
                Layout.leftMargin: 12
            }

            Label {
                visible: root.stack_entries === null
                text: pane_header.full_header_tool_tip_text
                elide: Text.ElideMiddle
                HoverHandler {
                    id: primary_label_hover
                }
                ToolTip.delay: 300
                ToolTip.visible: primary_label_hover.hovered
                ToolTip.text: pane_header.full_header_tool_tip_text
                Layout.fillWidth: true
            }

            ToolButton {
                text: "x"
                Layout.leftMargin: root.stack_entries !== null ? 8 : 0
                implicitWidth: 22
                implicitHeight: 22
                focusPolicy: Qt.NoFocus
                ToolTip.delay: 300
                ToolTip.visible: hovered
                ToolTip.text: "Remove image"
                onClicked: root.remove_requested(root.entry_id)
            }
        }
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
        anchors.topMargin: 4
        anchors.bottom: parent.bottom
        color: "#000000"
    }

    ImageItem {
        id: pane_image
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: pane_header.bottom
        anchors.topMargin: 4
        anchors.bottom: parent.bottom
        clip: true
        onActivated: root.activate_requested(root.entry_id)
        onSnapshot_changed: root.snapshot_changed(root.entry_id, pane_image.snapshot)
        onZoom_factor_changed: root.view_changed(root, pane_image.zoom_factor, pane_image.image_center)
        onImage_center_changed: root.view_changed(root, pane_image.zoom_factor, pane_image.image_center)
    }
}
