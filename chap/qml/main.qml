import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import chap

ApplicationWindow {
    id: rootWindow
    minimumWidth: 800
    minimumHeight: 400
    width: 1280
    height: 720
    visible: true
    title: `${Qt.application.displayName} (v${Qt.application.version})`
    flags: platform === "ios" ? Qt.Window | Qt.MaximizeUsingFullscreenGeometryHint : Qt.Window

    color: "#333"
    readonly property string platform: Qt.platform.os || ""
    readonly property int safeTop: {
        if (platform === "ios" && height > 600) {
            return 30
        } else {
            return 0
        }
    }
    readonly property int safeBottom: {
        if (platform === "ios" && height > 600) {
            return 20
        } else {
            return 0
        }
    }

    GridLayout {
        // shock collar ip
        // chat reward name/id
        // twitch api key
        Button {
            text: qsTr("Login")

            onClicked: twitch.login()
        }

        Button {
            text: qsTr("Logout")

            onClicked: twitch.logout()
        }

        Button {
            text: qsTr("Refresh")

            onClicked: twitch.refresh()
        }

        Label {
            text: twitch.loggedIn ? qsTr("Logged In") : qsTr("Logged Out")
            color: "#eee"
        }
    }
}
