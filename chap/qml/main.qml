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

    readonly property var shockDefinition: {

    }

    readonly property var shockReward: twitch.rewards.find(
                                           x => x.title === "Shock The Streamer")
    readonly property var smokeReward: twitch.rewards.find(
                                           x => x.title === "Hotbox The Streamer")
    readonly property bool hasAllRewards: !!shockReward && !!smokeReward

    function getRedemptions() {
        if (hasAllRewards) {
            var data = {
                "reward_id": shockReward.id,
                "status": "UNFULFILLED"
            }
            twitch.getRedemptions(data)
            data = {
                "reward_id": smokeReward.id,
                "status": "UNFULFILLED"
            }
            twitch.getRedemptions(data)
        } else if (twitch.rewards.length === 0) {
            twitch.getRewards()
        }
    }

    function processRedemptions(redemptions) {
        if (redemptions.length > 0 || !hasAllRewards) {
            var shouldShock = false
            var shouldSmoke = false
            redemptions.map(x => {
                                if (x.reward.id === shockReward.id) {
                                    shouldShock = true
                                }
                                if (x.reward.id === smokeReward.id) {
                                    shouldSmoke = true
                                }
                                twitch.updateRedemption(x.reward.id, x.id,
                                                        "FULFILLED")
                            })
            if (shouldShock) {
                shockCollar.shock()
            }
            if (shouldSmoke) {
                smokeMachine.activate()
            }
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

        function onGotRedemptions(redemptions) {
            processRedemptions(redemptions)
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

    Connections {
        target: smokeMachine

        function onOnlineChanged(online) {
            if (!!smokeReward) {
                var data = {
                    "id": smokeReward.id,
                    "cost": 1,
                    "is_paused": online ? "false" : "true",
                    "is_global_cooldown_enabled": "true",
                    "global_cooldown_seconds": 2 * 60,
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
                text: qsTr("Get Redemptions")
                enabled: !twitch.loading && twitch.loggedIn && hasAllRewards

                onClicked: getRedemptions()
            }
        }

        RowLayout {
            id: shockControls
            Layout.fillWidth: true

            Label {
                text: qsTr("Shock Collar Controls:")
                color: "#eee"
            }

            Button {
                text: qsTr("Create Reward")
                enabled: !twitch.loading && twitch.loggedIn && !shockReward

                onClicked: {
                    var data = {
                        "title": "Shock The Streamer",
                        "cost": 100,
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
                        "cost": 100,
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
                text: qsTr("Trigger")
                enabled: shockCollar.online

                onClicked: {
                    shockCollar.shock()
                }
            }
        }

        RowLayout {
            id: smokeControls
            Layout.fillWidth: true

            Label {
                text: qsTr("Smoke Machine Controls:")
                color: "#eee"
            }

            TextField {
                text: smokeMachine.duration
                validator: IntValidator {
                    top: 90
                    bottom: 1
                }
                Layout.preferredWidth: 80

                onTextEdited: {
                    if (acceptableInput) {
                        smokeMachine.duration = text
                        console.info(`Changed smoke duration to: ${smokeMachine.duration}`)
                    }
                }
            }

            Button {
                text: qsTr("Create Reward")
                enabled: !twitch.loading && twitch.loggedIn && !smokeReward

                onClicked: {
                    var data = {
                        "title": "Hotbox The Streamer",
                        "cost": 1,
                        "is_paused": "true",
                        "is_enabled": "true",
                        "is_global_cooldown_enabled": "true",
                        "global_cooldown_seconds": 2 * 60,
                        "should_redemptions_skip_request_queue": "false"
                    }
                    twitch.createReward(data)
                }
            }

            Button {
                text: qsTr("Enable Reward")
                visible: !!smokeReward && smokeReward.is_paused
                enabled: !twitch.loading && twitch.loggedIn && !!smokeReward

                onClicked: {
                    var data = {
                        "id": smokeReward.id,
                        "cost": 1,
                        "is_paused": "false",
                        "is_global_cooldown_enabled": "true",
                        "global_cooldown_seconds": 2 * 60,
                        "should_redemptions_skip_request_queue": "false"
                    }
                    twitch.updateReward(data)
                }
            }

            Button {
                text: qsTr("Pause Reward")
                visible: !!smokeReward && !smokeReward.is_paused
                enabled: !twitch.loading && twitch.loggedIn && !!smokeReward

                onClicked: {
                    var data = {
                        "id": smokeReward.id,
                        "is_paused": "true",
                        "is_global_cooldown_enabled": "true",
                        "global_cooldown_seconds": 2 * 60,
                        "should_redemptions_skip_request_queue": "false"
                    }
                    twitch.updateReward(data)
                }
            }

            Button {
                text: qsTr("Trigger")
                enabled: smokeMachine.online

                onClicked: {
                    smokeMachine.activate()
                }
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

        Label {
            text: smokeMachine.online ? qsTr("Smoke Machine Online") : qsTr(
                                            "Smoke Machine Offline")
            color: "#eee"
            Layout.fillWidth: true
        }
    }

    // ColumnLayout {
    //     spacing: 2
    //     anchors.top: controlsLayout.bottom
    //     anchors.left: parent.left
    //     anchors.right: parent.right
    //     anchors.margins: 5

    //     Repeater {
    //         model: twitch.redemptions

    //         Rectangle {
    //             height: 60
    //             width: parent.width
    //             color: index % 2 ? "#444" : "#555"

    //             Label {
    //                 text: `${modelData.user_name} redeemed ${modelData.reward.title} at ${modelData.redeemed_at}`
    //                 verticalAlignment: Label.AlignVCenter
    //                 color: "#eee"
    //                 anchors.fill: parent
    //                 anchors.margins: 5
    //             }
    //         }
    //     }
    // }
}
