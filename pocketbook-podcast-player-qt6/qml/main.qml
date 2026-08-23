import QtQuick
import QtQuick.Window

Window {
    visible: true
    width: screenWidth
    height: screenHeight
    color: "white"
    title: "Helciny podcasty - audio test"

    Column {
        anchors.fill: parent
        anchors.margins: 50
        spacing: 28

        Text { text: "Test integrovaneho prehravace"; font.pixelSize: 40; font.bold: true }
        Text { text: "Firmware U743g 6.11"; font.pixelSize: 26 }

        Repeater {
            model: [
                { label: "PREHRAT TESTOVACI WAV", action: function() { audioTest.playWav() } },
                { label: "PREHRAT STAZENY PODCAST", action: function() { audioTest.playPodcast() } },
                { label: "PREHRAT / PAUZA", action: function() { audioTest.togglePause() } },
                { label: "POSUNOUT NA POLOVINU", action: function() { audioTest.seekHalf() } }
            ]
            Rectangle {
                required property var modelData
                width: parent.width
                height: 105
                color: "white"
                border.color: "black"
                border.width: 2
                Text { anchors.centerIn: parent; text: modelData.label; font.pixelSize: 28; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: modelData.action() }
            }
        }

        Text { text: audioTest.status; font.pixelSize: 25; wrapMode: Text.Wrap; width: parent.width }
        Text { text: "Pozice: " + Math.floor(audioTest.position / 1000) + " s / " + Math.floor(audioTest.duration / 1000) + " s"; font.pixelSize: 25 }
        Text { text: audioTest.error.length ? "Chyba: " + audioTest.error : "Chyba: zadna"; font.pixelSize: 25; wrapMode: Text.Wrap; width: parent.width }
    }
}

