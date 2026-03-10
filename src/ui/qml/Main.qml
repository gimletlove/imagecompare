import QtQuick
import QtQuick.Controls
import "components"

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 800
    title: "Image Compare"
    property bool drop_active: false

    function normalize_paths(paths) {
        const out = [];
        if (!paths) {
            return out;
        }
        for (let i = 0; i < paths.length; ++i) {
            out.push(String(paths[i]));
        }
        return out;
    }

    DropArea {
        anchors.fill: parent
        onEntered: window.drop_active = true
        onExited: window.drop_active = false
        onDropped: (drop_event) => {
            window.drop_active = false;
            if (!applicationController) {
                return;
            }
            applicationController.import_image_paths(window.normalize_paths(drop_event.urls));
        }
    }

    ComparisonWorkspace {
        anchors.fill: parent
        controller: applicationController
        onOpen_requested: {
            if (applicationController) {
                applicationController.open_images_with_native_dialog();
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: window.drop_active
        z: 10
        color: Qt.rgba(0, 0, 0, 0.2)

        Pane {
            anchors.centerIn: parent
            padding: 12

            Label {
                text: "Drop images to import"
            }
        }
    }
}
