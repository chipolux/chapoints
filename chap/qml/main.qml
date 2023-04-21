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

    readonly property var shockReward: twitch.rewards.find(
                                           x => x.title === "Shock The Streamer")

    function getRedemptions() {
        if (!!shockReward) {
            var data = {
                "reward_id": shockReward.id,
                "status": "UNFULFILLED"
            }
            twitch.getRedemptions(data)
        } else {
            twitch.getRewards()
        }
    }

    function processRedemptions() {
        if (twitch.redemptions.length > 0) {
            twitch.redemptions.map(x => {
                                       twitch.updateRedemption(x.reward.id,
                                                               x.id,
                                                               "FULFILLED")
                                   })
            shockCollar.shock()
        }
    }

    Timer {
        running: true
        repeat: true
        interval: 10 * 1000

        onTriggered: getRedemptions()
    }

    Connections {
        target: twitch

        function onValidated() {
            twitch.getRewards()
        }

        function onRewardsChanged() {
            getRedemptions()
        }

        function onRedemptionsChanged() {
            processRedemptions()
        }
    }

    Connections {
        target: shockCollar

        function onOnlineChanged(online) {
            if (!!shockReward) {
                var data = {
                    "id": shockReward.id,
                    "cost": 100,
                    "is_paused": online ? "false" : "true",
                    "is_global_cooldown_enabled": "true",
                    "global_cooldown_seconds": 30,
                    "should_redemptions_skip_request_queue": "false"
                }
                twitch.updateReward(data)
            }
        }
    }

    ColumnLayout {
        id: controlsLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 5

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: qsTr("Login")
                enabled: !twitch.loading && !twitch.loggedIn

                onClicked: twitch.login()
            }

            Button {
                text: qsTr("Logout")
                enabled: !twitch.loading && twitch.loggedIn

                onClicked: twitch.logout()
            }

            Button {
                text: qsTr("Refresh")
                enabled: !twitch.loading && twitch.loggedIn

                onClicked: twitch.refresh()
            }

            Button {
                text: qsTr("Get Rewards")
                enabled: !twitch.loading && twitch.loggedIn

                onClicked: twitch.getRewards()
            }

            Button {
                text: qsTr("Create Reward")
                enabled: !twitch.loading && twitch.loggedIn && !shockReward

                onClicked: {
                    var data = {
                        "title": qsTr("Shock The Streamer"),
                        "cost": 1000,
                        "is_paused": "true",
                        "is_enabled": "true",
                        "is_global_cooldown_enabled": "true",
                        "global_cooldown_seconds": 30,
                        "should_redemptions_skip_request_queue": "false"
                    }
                    twitch.createReward(data)
                }
            }

            Button {
                text: qsTr("Enable Reward")
                visible: !!shockReward && shockReward.is_paused
                enabled: !twitch.loading && twitch.loggedIn && !!shockReward

                onClicked: {
                    var data = {
                        "id": shockReward.id,
                        "is_paused": "false",
                        "is_global_cooldown_enabled": "true",
                        "global_cooldown_seconds": 60,
                        "should_redemptions_skip_request_queue": "false"
                    }
                    twitch.updateReward(data)
                }
            }

            Button {
                text: qsTr("Pause Reward")
                visible: !!shockReward && !shockReward.is_paused
                enabled: !twitch.loading && twitch.loggedIn && !!shockReward

                onClicked: {
                    var data = {
                        "id": shockReward.id,
                        "is_paused": "true",
                        "is_global_cooldown_enabled": "true",
                        "global_cooldown_seconds": 60,
                        "should_redemptions_skip_request_queue": "false"
                    }
                    twitch.updateReward(data)
                }
            }

            Button {
                text: qsTr("Get Redemptions")
                enabled: !twitch.loading && twitch.loggedIn && !!shockReward

                onClicked: getRedemptions()
            }

            Button {
                text: qsTr("Process Redemptions")
                enabled: !twitch.loading && twitch.loggedIn
                         && twitch.redemptions.length > 0

                onClicked: processRedemptions()
            }
        }

        Label {
            text: twitch.loggedIn ? qsTr("Logged In As %1").arg(
                                        twitch.userName) : qsTr("Logged Out")
            color: "#eee"
            Layout.fillWidth: true
        }

        Label {
            text: shockCollar.online ? qsTr("Shock Collar Online") : qsTr(
                                           "Shock Collar Offline")
            color: "#eee"
            Layout.fillWidth: true
        }
    }

    ColumnLayout {
        spacing: 2
        anchors.top: controlsLayout.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 5

        Repeater {
            model: twitch.redemptions

            Rectangle {
                height: 60
                width: parent.width
                color: index % 2 ? "#444" : "#555"

                Label {
                    text: `${modelData.user_name} redeemed ${modelData.reward.title} at ${modelData.redeemed_at}`
                    verticalAlignment: Label.AlignVCenter
                    color: "#eee"
                    anchors.fill: parent
                    anchors.margins: 5
                }
            }
        }
    }
}
