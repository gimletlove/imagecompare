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

    DropArea {
        anchors.fill: parent
        onEntered: window.drop_active = true
        onExited: window.drop_active = false
        onDropped: () => {
            window.drop_active = false;
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
