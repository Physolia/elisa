/*
   SPDX-FileCopyrightText: 2017 (c) Alexander Stippich <a.stippich@gmx.net>
   SPDX-FileCopyrightText: 2018 (c) Matthieu Gallien <matthieu_gallien@yahoo.fr>
   SPDX-FileCopyrightText: 2020 (c) Devin Lin <espidev@gmail.com>

   SPDX-License-Identifier: LGPL-3.0-or-later
 */


import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Window 2.2
import QtQml.Models 2.2
import QtQuick.Layouts 1.2
import ElisaGraphicalEffects 1.15
import org.kde.kirigami 2.13 as Kirigami
import org.kde.elisa 1.0

import ".."

Kirigami.ScrollablePage {
    id: trackMetadata

    property var modelType
    property url fileName
    property bool editableMetadata
    property bool isModifying: false
    property bool isCreating: false
    property bool canAddMoreMetadata: false
    property bool showImage
    property bool showTrackFileName
    property bool showDeleteButton: false

    signal rejected()

    LayoutMirroring.enabled: Qt.application.layoutDirection == Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    title: isCreating ? i18nc("@title:window", "Create a Radio") :
                        i18nc("@title:window", "View details")

    actions: [
        Kirigami.Action {
            icon.name: "dialog-cancel"
                text: i18nc("@action:button", "Close")
            onTriggered: {
                trackMetadata.rejected();
                mainWindow.pageStack.layers.pop();
            }
        },
        Kirigami.Action {
            icon.name: "dialog-ok-apply"
            text: i18nc("@action:button", "Save")
            onTriggered: {
                realModel.saveData()
                if (isCreating) {
                    isCreating = false
                    isModifying = true
                }
                mainWindow.pageStack.layers.pop();
            }
        }]
    Component.onCompleted: {
        if (ElisaApplication.musicManager) {
            if (isCreating) {
                realModel.initializeForNewRadio()
            } else {
                realModel.initializeByUrl(modelType, fileName)
            }
        }
    }

    ColumnLayout {
        Layout.minimumWidth: parent.width
        Layout.maximumWidth: parent.width

        Connections {
            target: ElisaApplication

            function onMusicManagerChanged() {
                if (isCreating) {
                    realModel.initializeForNewRadio()
                } else {
                    realModel.initializeByUrl(modelType, fileName)
                }
            }
        }

        EditableTrackMetadataModel {
            id: realModel
            manager: ElisaApplication.musicManager
        }

        // file location
        RowLayout {
            id: fileNameRow
            visible: showTrackFileName

            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                Layout.preferredWidth: fileNameLabel.height
                Layout.preferredHeight: fileNameLabel.height

                source: elisaTheme.folderIcon
            }

            LabelWithToolTip {
                id: fileNameLabel

                Layout.fillWidth: true
                text: realModel.fileUrl
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }
        }

        MediaTrackMetadataForm {
            Layout.fillWidth: true

            metadataModel: realModel
            modelType: trackMetadata.modelType
            showDeleteButton: trackMetadata.showDeleteButton
            isCreating: trackMetadata.isCreating
            isModifying: trackMetadata.isModifying
            canAddMoreMetadata: trackMetadata.canAddMoreMetadata
            showImage: trackMetadata.showImage
            showModifyDeleteButtons: true
            onClose: trackMetadata.close()
        }
    }
}
