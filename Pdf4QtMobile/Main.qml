// MIT License - Andre PDF mobile app (based on PDF4QT by Jakub Melka and Contributors)

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 420
    height: 760
    visible: true
    title: controller.fileName.length > 0 ? controller.fileName : qsTr("Andre PDF")

    // Placing a signature by a tap on the page
    property bool placing: false
    property url certificateUrl

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton { text: qsTr("Open"); onClicked: openDialog.open() }
            ToolButton { text: qsTr("Sign"); enabled: controller.pageCount > 0; onClicked: signDialog.open() }
            ToolButton { text: qsTr("Signatures"); onClicked: libraryDialog.open() }
            Item { Layout.fillWidth: true }
            ToolButton {
                text: qsTr("Share")
                enabled: controller.signedFileUrl.toString().length > 0
                onClicked: Qt.openUrlExternally(controller.signedFileUrl)
            }
        }
    }

    footer: Label {
        padding: 8
        wrapMode: Text.Wrap
        text: window.placing ? qsTr("Tap the page where the signature goes.") : controller.status
    }

    ListView {
        id: pages
        objectName: "pages"
        anchors.fill: parent
        spacing: 8
        model: controller.pageCount
        clip: true
        delegate: Image {
            width: pages.width
            height: width * controller.getPageAspectRatio(index)
            asynchronous: true
            cache: false
            sourceSize.width: width
            source: "image://page/" + controller.revision + "/" + index

            MouseArea {
                anchors.fill: parent
                enabled: window.placing
                onClicked: (mouse) => {
                    window.placing = false
                    controller.sign(window.certificateUrl, passwordField.text, lockCombo.currentIndex === 3 ? 0 : lockCombo.currentIndex + 1,
                                    slotCombo.currentIndex, index, mouse.x / width, mouse.y / height)
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: controller.pageCount === 0
        text: qsTr("Open a PDF to read and sign it.")
    }

    FileDialog {
        id: openDialog
        nameFilters: [ qsTr("PDF documents (*.pdf)") ]
        onAccepted: controller.open(selectedFile)
    }

    FileDialog {
        id: certificateDialog
        nameFilters: [ qsTr("Certificates (*.pfx *.p12)") ]
        onAccepted: window.certificateUrl = selectedFile
    }

    FileDialog {
        id: signatureImageDialog
        nameFilters: [ qsTr("Images (*.jpg *.jpeg *.png)") ]
        onAccepted: controller.importSignature(librarySlotCombo.currentIndex, selectedFile, signatureNameField.text)
    }

    Dialog {
        id: signDialog
        objectName: "signDialog"
        title: qsTr("Sign and Lock")
        modal: true
        anchors.centerIn: parent
        width: Math.min(window.width - 24, 400)
        standardButtons: Dialog.Cancel

        ColumnLayout {
            anchors.fill: parent
            Button {
                text: window.certificateUrl.toString().length > 0 ? qsTr("Certificate chosen") : qsTr("Choose Certificate (.pfx)...")
                Layout.fillWidth: true
                onClicked: certificateDialog.open()
            }
            TextField { id: passwordField; placeholderText: qsTr("Certificate password"); echoMode: TextInput.Password; Layout.fillWidth: true }
            ComboBox { id: slotCombo; model: controller.signatureSlots; Layout.fillWidth: true }
            ComboBox {
                id: lockCombo
                model: [ qsTr("Lock: no changes allowed"), qsTr("Lock: allow form filling and signing"), qsTr("Lock: allow filling, signing and comments"), qsTr("Do not lock") ]
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Sign All %1 Signature Lines").arg(controller.signatureLineCount)
                enabled: controller.signatureLineCount > 0 && window.certificateUrl.toString().length > 0
                Layout.fillWidth: true
                onClicked: {
                    signDialog.close()
                    controller.sign(window.certificateUrl, passwordField.text, lockCombo.currentIndex === 3 ? 0 : lockCombo.currentIndex + 1, slotCombo.currentIndex, -1, 0, 0)
                }
            }
            Button {
                text: qsTr("Tap Where to Sign")
                enabled: window.certificateUrl.toString().length > 0
                Layout.fillWidth: true
                onClicked: { signDialog.close(); window.placing = true }
            }
        }
    }

    Dialog {
        id: libraryDialog
        title: qsTr("Signature Library")
        modal: true
        anchors.centerIn: parent
        width: Math.min(window.width - 24, 400)
        standardButtons: Dialog.Close

        ColumnLayout {
            anchors.fill: parent
            Label { text: qsTr("Sign a white sheet of paper, photograph it and import the photo. The paper is removed."); wrapMode: Text.Wrap; Layout.fillWidth: true }
            ComboBox { id: librarySlotCombo; model: controller.signatureSlots; Layout.fillWidth: true }
            TextField { id: signatureNameField; placeholderText: qsTr("Name, for example Initials"); Layout.fillWidth: true }
            Button { text: qsTr("Import Photo..."); Layout.fillWidth: true; onClicked: signatureImageDialog.open() }
        }
    }
}
