import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window

ApplicationWindow {
    id: window
    visible: true
    width: 1920
    height: 1080
    title: "TeknoKonsole"
    color: controller.background
    visibility: controller.fullscreen ? Window.FullScreen : Window.Windowed
    flags: Qt.FramelessWindowHint | Qt.Window
    onActiveChanged: {
        if (active) controller.resumeGamepad()
        else controller.suspendGamepad()
    }

    property int screen: 0
    property int homePanel: 0
    property int listIndex: 0
    property int editIndex: -1
    property int assetIndex: -1
    property string assetTarget: ""
    property var mainItems: []

    property var gameSourceTypes: ["steam", "appimage", "desktop", "command", "wine"]

    function rebuildMainItems() {
        var items = []
        for (var gameIndex = 0; gameIndex < controller.games.length; gameIndex++) {
            var game = controller.games[gameIndex]
            items.push({
                kind: "game",
                sourceIndex: gameIndex,
                name: game.name || "",
                subtitle: (game.sourceType || "steam") + "  •  " + (game.appId || game.launchPath || ""),
                description: game.description || "",
                imagePath: game.displayAsset || game.cover || game.icon || "qrc:/assets/TeknoKonsole.png"
            })
        }
        for (var emulatorIndex = 0; emulatorIndex < controller.emulators.length; emulatorIndex++) {
            var emulator = controller.emulators[emulatorIndex]
            items.push({
                kind: "emulator",
                sourceIndex: emulatorIndex,
                name: emulator.name || "",
                subtitle: (emulator.type || "emulator") + "  •  " + (emulator.command || ""),
                description: emulator.type === "appimage" ? "AppImage emülatörü" : emulator.type === "desktop" ? "Linux masaüstü emülatörü" : "Kullanıcı tanımlı komut",
                imagePath: emulator.displayAsset || emulator.banner || emulator.icon || "qrc:/assets/TeknoKonsole.png"
            })
        }
        mainItems = items
        if (listIndex >= mainItems.length) listIndex = Math.max(0, mainItems.length - 1)
    }

    function launchMainItem(index) {
        if (index < 0 || index >= mainItems.length) return
        var item = mainItems[index]
        if (item.kind === "game") controller.launchGame(item.sourceIndex)
        else controller.launchEmulator(item.sourceIndex)
    }

    function openEmulatorForAdd() {
        editIndex = -1
        emulatorName.text = ""
        emulatorCommand.text = ""
        emulatorDialog.open()
    }

    function assetUrl(path) {
        if (!path || path.length === 0) return ""
        if (path.startsWith("qrc:") || path.startsWith("file:") || path.startsWith("http:")) return path
        return "file://" + path
    }

    function gameSourceIndex(value) {
        var index = gameSourceTypes.indexOf(value || "")
        return index >= 0 ? index : 0
    }

    function clearGameDialog() {
        window.editIndex = -1
        gameName.text = ""
        gameAppId.text = ""
        gameSteamUri.text = ""
        gameLaunchPath.text = ""
        gameDescription.text = ""
        gameCategory.text = "Tüm Oyunlar"
        gameIconPath.text = ""
        gameSourceType.currentIndex = 0
    }

    function openGameForAdd() {
        clearGameDialog()
        gameDialog.open()
    }

    function goBack() {
        if (screen !== 0) {
            screen = 0
            listIndex = 0
        }
    }

    function ensureSelectedVisible() {
        var view = screen === 0 ? mainList : screen === 1 ? gameList : emulatorList
        if (view && listIndex >= 0 && listIndex < view.count) {
            view.positionViewAtIndex(listIndex, ListView.Contain)
        }
    }

    onListIndexChanged: Qt.callLater(ensureSelectedVisible)
    onScreenChanged: {
        listIndex = 0
        Qt.callLater(ensureSelectedVisible)
    }

    function move(direction) {
        if (screen === 0) {
            listIndex = Math.max(0, Math.min(Math.max(0, mainItems.length - 1), listIndex + direction))
        } else if (screen === 1) {
            listIndex = Math.max(0, Math.min(Math.max(0, controller.games.length - 1), listIndex + direction))
        } else if (screen === 2) {
            listIndex = Math.max(0, Math.min(Math.max(0, controller.emulators.length - 1), listIndex + direction))
        }
    }

    function selectCurrent() {
        if (screen === 0) {
            launchMainItem(listIndex)
        } else if (screen === 1 && controller.games.length > 0) {
            controller.launchGame(listIndex)
        } else if (screen === 2 && controller.emulators.length > 0) {
            controller.launchEmulator(listIndex)
        }
    }

    function openGameForEdit(index) {
        editIndex = index
        gameName.text = controller.games[index].name || ""
        gameAppId.text = controller.games[index].appId || ""
        gameSteamUri.text = controller.games[index].steamUri || ""
        gameLaunchPath.text = controller.games[index].launchPath || ""
        gameDescription.text = controller.games[index].description || ""
        gameCategory.text = controller.games[index].category || "Tüm Oyunlar"
        gameIconPath.text = controller.games[index].icon || ""
        gameSourceType.currentIndex = gameSourceIndex(controller.games[index].sourceType)
        gameDialog.open()
    }

    function openEmulatorForEdit(index) {
        editIndex = index
        emulatorName.text = controller.emulators[index].name
        emulatorCommand.text = controller.emulators[index].command
        emulatorDialog.open()
    }

    Connections {
        target: controller
        function onNavigate(direction) { window.move(direction) }
        function onSelectPressed() { window.selectCurrent() }
        function onBackPressed() { window.goBack() }
        function onHomePressed() { window.screen = 0 }
        function onMenuPressed() { window.screen = 3 }
        function onContentChanged() { window.rebuildMainItems() }
    }

    Component.onCompleted: window.rebuildMainItems()

    Item {
        id: keyCatcher
        anchors.fill: parent
        focus: true
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
                move(-1); event.accepted = true
            } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
                move(1); event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                selectCurrent(); event.accepted = true
            } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) {
                goBack(); event.accepted = true
            } else if (event.key === Qt.Key_Home) {
                screen = 0; event.accepted = true
            } else if (event.key === Qt.Key_F11) {
                controller.fullscreen = !controller.fullscreen; event.accepted = true
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: controller.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 44
        spacing: 24

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            spacing: 22

            Image {
                Layout.preferredWidth: 76
                Layout.preferredHeight: 76
                source: controller.logoPath
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: "TeknoKonsole"
                    color: controller.textColor
                    font.pixelSize: 32
                    font.bold: true
                }
                Text {
                    text: screen === 0 ? "Tüm Oyunlar ve Uygulamalar" : screen === 1 ? "Steam Oyunları" : screen === 2 ? "Emülatörler" : "Ayarlar ve İçerik Yönetimi"
                    color: controller.secondaryText
                    font.pixelSize: 18
                }
            }

            Rectangle {
                Layout.preferredWidth: 270
                Layout.preferredHeight: 54
                radius: 27
                color: controller.surface
                border.color: controller.accent
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: controller.statusMessage.length > 0 ? controller.statusMessage : "Gamepad veya klavye hazır"
                    color: controller.secondaryText
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    width: parent.width - 24
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Button {
                text: "Kapat"
                onClicked: controller.quit()
                contentItem: Text { text: parent.text; color: controller.textColor; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 18; color: controller.surface; border.color: controller.secondaryText }
            }
        }

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: screen

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 16

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Tüm Oyunlar ve Uygulamalar"; color: controller.textColor; font.pixelSize: 28; font.bold: true }
                        Item { Layout.fillWidth: true }
                        Button { text: "+ Oyun Ekle"; onClicked: window.openGameForAdd() }
                        Button { text: "+ Emülatör Ekle"; onClicked: window.openEmulatorForAdd() }
                        Button { text: "Ayarlar"; onClicked: window.screen = 3 }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: "AppImage / Emülatör"; onClicked: emulatorAppImageFileDialog.open() }
                        Button { text: "Wine EXE Ekle"; onClicked: wineExeFileDialog.open() }
                        Button { text: ".desktop / Oyun"; onClicked: desktopFileDialog.open() }
                        Button { text: ".desktop / Emülatör"; onClicked: emulatorDesktopFileDialog.open() }
                        Item { Layout.fillWidth: true }
                        Label { text: window.mainItems.length + " kayıt"; color: controller.secondaryText; font.pixelSize: 16 }
                    }

                    ListView {
                        id: mainList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        orientation: ListView.Vertical
                        interactive: true
                        clip: true
                        spacing: 12
                        boundsBehavior: Flickable.StopAtBounds
                        currentIndex: window.listIndex
                        model: window.mainItems
                        delegate: ContentCard {
                            width: mainList.width
                            selected: index === window.listIndex
                            title: modelData.name
                            subtitle: modelData.subtitle
                            description: modelData.description
                            imagePath: modelData.imagePath
                            launchText: "Başlat"
                            onLaunch: window.launchMainItem(index)
                            onEdit: modelData.kind === "game" ? window.openGameForEdit(modelData.sourceIndex) : window.openEmulatorForEdit(modelData.sourceIndex)
                            onRemove: modelData.kind === "game" ? controller.removeGame(modelData.sourceIndex) : controller.removeEmulator(modelData.sourceIndex)
                            onAsset: {
                                window.assetIndex = modelData.sourceIndex
                                window.assetTarget = modelData.kind
                                assetDialog.open()
                            }
                        }
                        onCountChanged: Qt.callLater(window.ensureSelectedVisible)
                        Text {
                            anchors.centerIn: parent
                            visible: window.mainItems.length === 0
                            text: "Henüz kayıt yok. Yukarıdaki ekleme düğmelerinden başlayın."
                            color: controller.secondaryText
                            font.pixelSize: 22
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 18
                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: "← Ana Menü"; onClicked: window.goBack() }
                        Item { Layout.fillWidth: true }
                        Button { text: "+ Oyun Ekle"; onClicked: window.openGameForAdd() }
                        Button { text: ".desktop Ekle"; onClicked: desktopFileDialog.open() }
                    }
                    Label { text: "Steam oyunları"; color: controller.textColor; font.pixelSize: 28; font.bold: true }
                    ListView {
                        id: gameList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 12
                        model: controller.games
                        delegate: ContentCard {
                            width: gameList.width
                            selected: index === window.listIndex
                            title: modelData.name
                            subtitle: (modelData.sourceType || "steam") + "  •  " + (modelData.appId || modelData.launchPath || "") + "  •  " + modelData.category
                            description: modelData.description
                            imagePath: modelData.displayAsset || modelData.cover || modelData.icon || "qrc:/assets/TeknoKonsole.png"
                            launchText: "Başlat"
                            onLaunch: controller.launchGame(index)
                            onEdit: window.openGameForEdit(index)
                            onRemove: controller.removeGame(index)
                            onAsset: { window.assetIndex = index; window.assetTarget = "game"; assetDialog.open() }
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: controller.games.length === 0
                            text: "Henüz oyun eklenmedi.  + Oyun Ekle ile başlayın."
                            color: controller.secondaryText
                            font.pixelSize: 22
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 18
                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: "← Ana Menü"; onClicked: window.goBack() }
                        Item { Layout.fillWidth: true }
                        Button { text: "+ Emülatör Ekle"; onClicked: { window.editIndex = -1; emulatorName.text = ""; emulatorCommand.text = ""; emulatorDialog.open() } }
                        Button { text: "AppImage Ekle"; onClicked: emulatorAppImageFileDialog.open() }
                        Button { text: ".desktop Ekle"; onClicked: emulatorDesktopFileDialog.open() }
                    }
                    Label { text: "Emülatörler"; color: controller.textColor; font.pixelSize: 28; font.bold: true }
                    ListView {
                        id: emulatorList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 12
                        model: controller.emulators
                        delegate: ContentCard {
                            width: emulatorList.width
                            selected: index === window.listIndex
                            title: modelData.name
                            subtitle: (modelData.type || "emulator") + "  •  " + modelData.command
                            description: modelData.type === "appimage" ? "AppImage emülatörü" : modelData.type === "desktop" ? "Linux masaüstü emülatörü" : "Kullanıcı tanımlı komut"
                            imagePath: modelData.displayAsset || modelData.banner || modelData.icon || "qrc:/assets/TeknoKonsole.png"
                            launchText: "Başlat"
                            onLaunch: controller.launchEmulator(index)
                            onEdit: window.openEmulatorForEdit(index)
                            onRemove: controller.removeEmulator(index)
                            onAsset: { window.assetIndex = index; window.assetTarget = "emulator"; assetDialog.open() }
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: controller.emulators.length === 0
                            text: "Henüz emülatör eklenmedi."
                            color: controller.secondaryText
                            font.pixelSize: 22
                        }
                    }
                }
            }

            Item {
                Flickable {
                    anchors.fill: parent
                    contentHeight: settingsColumn.height + 30
                    clip: true
                    ColumnLayout {
                        id: settingsColumn
                        width: parent.width
                        spacing: 22
                        RowLayout {
                            Layout.fillWidth: true
                            Button { text: "← Ana Menü"; onClicked: window.goBack() }
                            Item { Layout.fillWidth: true }
                            Button { text: "Logoyu Değiştir"; onClicked: logoDialog.open() }
                        }
                        Label { text: "Görünüm"; color: controller.textColor; font.pixelSize: 28; font.bold: true }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16
                            SettingField { label: "Vurgu"; value: controller.accent; onCommit: controller.setThemeColor("accent", value) }
                            SettingField { label: "Arka Plan"; value: controller.background; onCommit: controller.setThemeColor("background", value) }
                            SettingField { label: "Yüzey"; value: controller.surface; onCommit: controller.setThemeColor("surface", value) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Button { text: "Temayı Sıfırla"; onClicked: controller.resetTheme() }
                            CheckBox { text: "Tam ekran"; checked: controller.fullscreen; onToggled: controller.fullscreen = checked }
                            CheckBox { text: "Animasyonlar"; checked: controller.animations; onToggled: controller.animations = checked }
                            CheckBox { text: "Steam'i otomatik başlat"; checked: controller.autoStartSteam; onToggled: controller.autoStartSteam = checked }
                        }
                        Label { text: "Kullanıcı verileri"; color: controller.textColor; font.pixelSize: 28; font.bold: true }
                        Label {
                            Layout.fillWidth: true
                            text: "Ayarlar: ~/.config/tekno-konsole/   •   Görseller: ~/.local/share/tekno-konsole/assets/"
                            color: controller.secondaryText
                            font.pixelSize: 18
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Button { text: "Oyunları Yönet"; onClicked: window.screen = 1 }
                            Button { text: "Emülatörleri Yönet"; onClicked: window.screen = 2 }
                            Button { text: "Kategori Ekle"; onClicked: categoryDialog.open() }
                        }
                        Label { text: "TeknoKonsole 1.0.0  •  Native Qt6/QML  •  Wayland uyumlu"; color: controller.secondaryText; font.pixelSize: 16 }
                    }
                }
            }
        }
    }

    component HomeCard: Rectangle {
        id: card
        property string title: ""
        property string subtitle: ""
        property string iconText: ""
        property bool selected: false
        signal clicked()
        implicitHeight: 300
        radius: 26
        color: selected ? Qt.darker(controller.accent, 2.5) : controller.surface
        border.color: selected ? controller.accent : Qt.darker(controller.secondaryText, 1.6)
        border.width: selected ? 4 : 1
        scale: selected ? 1.02 : 1.0
        Behavior on scale { enabled: controller.animations; NumberAnimation { duration: 140 } }
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 90; height: 90; radius: 45
                color: controller.accent
                Text { anchors.centerIn: parent; text: card.iconText; color: "#061006"; font.pixelSize: 48; font.bold: true }
            }
            Text { Layout.alignment: Qt.AlignHCenter; text: card.title; color: controller.textColor; font.pixelSize: 32; font.bold: true }
            Text { Layout.alignment: Qt.AlignHCenter; text: card.subtitle; color: controller.secondaryText; font.pixelSize: 18 }
        }
        MouseArea { anchors.fill: parent; onClicked: card.clicked() }
    }

    component ContentCard: Rectangle {
        id: contentCard
        property bool selected: false
        property string title: ""
        property string subtitle: ""
        property string description: ""
        property string imagePath: ""
        property string launchText: "Başlat"
        signal launch()
        signal edit()
        signal remove()
        signal asset()
        height: 142
        radius: 20
        color: selected ? Qt.darker(controller.accent, 2.6) : controller.surface
        border.color: selected ? controller.accent : "transparent"
        border.width: selected ? 3 : 1
        RowLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 20
            Rectangle {
                Layout.preferredWidth: 160; Layout.preferredHeight: 106; radius: 14
                color: controller.background
                Image { id: coverImage; anchors.fill: parent; anchors.margins: 3; source: window.assetUrl(contentCard.imagePath || "qrc:/assets/TeknoKonsole.png"); fillMode: Image.PreserveAspectCrop; visible: status === Image.Ready }
                Image { anchors.fill: parent; anchors.margins: 3; source: "qrc:/assets/TeknoKonsole.png"; fillMode: Image.PreserveAspectFit; visible: coverImage.status !== Image.Ready }
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 5
                Text { text: contentCard.title; color: controller.textColor; font.pixelSize: 25; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { text: contentCard.subtitle; color: controller.accent; font.pixelSize: 15; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { text: contentCard.description; color: controller.secondaryText; font.pixelSize: 16; elide: Text.ElideRight; Layout.fillWidth: true }
            }
            Button { text: contentCard.launchText; onClicked: contentCard.launch() }
            Button { text: "Düzenle"; onClicked: contentCard.edit() }
            Button { text: "Görsel"; onClicked: contentCard.asset() }
            Button { text: "Sil"; onClicked: contentCard.remove() }
        }
    }

    component SettingField: ColumnLayout {
        property string label: ""
        property string value: ""
        signal commit(string value)
        Layout.fillWidth: true
        Label { text: parent.label; color: controller.secondaryText; font.pixelSize: 15 }
        TextField {
            id: field
            text: parent.value
            Layout.fillWidth: true
            color: controller.textColor
            placeholderText: "#RRGGBB"
            onAccepted: parent.commit(text)
            background: Rectangle { radius: 8; color: controller.surface; border.color: controller.secondaryText }
        }
    }

    Dialog {
        id: gameDialog
        title: window.editIndex < 0 ? "Oyun / Uygulama Ekle" : "Oyunu / Uygulamayı Düzenle"
        modal: true
        width: 640
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ColumnLayout {
            Label { text: "Kaynak türü"; color: controller.secondaryText }
            ComboBox {
                id: gameSourceType
                model: ["Steam", "AppImage", ".desktop", "Komut / yol", "Wine EXE"]
                Layout.fillWidth: true
            }
            TextField { id: gameName; placeholderText: "Oyun veya uygulama adı"; Layout.fillWidth: true }
            TextField { id: gameAppId; placeholderText: "Steam AppID (isteğe bağlı, ör. 730)"; Layout.fillWidth: true }
            TextField { id: gameSteamUri; placeholderText: "Steam URI (isteğe bağlı, ör. steam://rungameid/730)"; Layout.fillWidth: true }
            TextField { id: gameLaunchPath; placeholderText: "Başlatma yolu veya komutu"; Layout.fillWidth: true }
            TextField { id: gameDescription; placeholderText: "Açıklama"; Layout.fillWidth: true }
            TextField { id: gameCategory; placeholderText: "Kategori"; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: gameIconPath; placeholderText: "İkon yolu (isteğe bağlı)"; Layout.fillWidth: true }
                Button { text: "İkon Seç"; onClicked: gameIconFileDialog.open() }
            }
            Label {
                text: "İkon seçmek zorunlu değildir. Bulunamazsa varsayılan TeknoKonsole ikonu kullanılır."
                color: controller.secondaryText
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            var sourceType = window.gameSourceTypes[gameSourceType.currentIndex]
            if (window.editIndex < 0) {
                controller.addGame(gameName.text, gameAppId.text, gameDescription.text, gameCategory.text,
                                   gameLaunchPath.text, gameSteamUri.text, gameIconPath.text, sourceType)
            } else {
                controller.editGame(window.editIndex, gameName.text, gameAppId.text, gameDescription.text, gameCategory.text,
                                    gameLaunchPath.text, gameSteamUri.text, gameIconPath.text, sourceType)
            }
        }
    }

    Dialog {
        id: emulatorDialog
        title: window.editIndex < 0 ? "Emülatör Ekle" : "Emülatörü Düzenle"
        modal: true
        width: 560
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ColumnLayout {
            TextField { id: emulatorName; placeholderText: "Emülatör adı"; Layout.fillWidth: true }
            TextField { id: emulatorCommand; placeholderText: "Komut (ör. duckstation)"; Layout.fillWidth: true }
        }
        onAccepted: {
            if (window.editIndex < 0) controller.addEmulator(emulatorName.text, emulatorCommand.text)
            else controller.editEmulator(window.editIndex, emulatorName.text, emulatorCommand.text)
        }
    }

    Dialog {
        id: categoryDialog
        title: "Kategori Ekle"
        modal: true
        width: 420
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: TextField { id: categoryName; placeholderText: "Kategori adı" }
        onAccepted: controller.addCategory(categoryName.text)
    }

    Dialog {
        id: logoDialog
        title: "Logo Değiştir"
        modal: true
        width: 480
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            Label { text: "PNG, SVG, JPG veya JPEG seçin."; color: controller.secondaryText }
            Button { text: "Dosya Seç"; onClicked: logoFileDialog.open() }
        }
    }

    Dialog {
        id: assetDialog
        title: "Görsel Ekle"
        modal: true
        width: 480
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            ComboBox { id: assetType; model: ["icons", "covers", "banners"]; Layout.fillWidth: true }
            Button { text: "Dosya Seç"; onClicked: assetFileDialog.open() }
        }
    }

    FileDialog {
        id: logoFileDialog
        title: "TeknoKonsole logosu seç"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Görseller (*.png *.svg *.jpg *.jpeg)"]
        onAccepted: { controller.setLogo(selectedFile.toString()); logoDialog.close() }
    }

    FileDialog {
        id: appImageFileDialog
        title: "AppImage uygulaması seç"
        fileMode: FileDialog.OpenFile
        nameFilters: ["AppImage dosyaları (*.AppImage *.appimage)"]
        onAccepted: controller.importAppImage(selectedFile.toString())
    }

    FileDialog {
        id: wineExeFileDialog
        title: "Windows EXE dosyası seç"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Windows uygulamaları (*.exe *.EXE)"]
        onAccepted: controller.importWineExe(selectedFile.toString())
    }

    FileDialog {
        id: desktopFileDialog
        title: "Linux .desktop dosyası seç"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Desktop dosyaları (*.desktop)"]
        onAccepted: controller.importDesktop(selectedFile.toString())
    }

    FileDialog {
        id: emulatorAppImageFileDialog
        title: "Emülatör AppImage dosyası seç"
        fileMode: FileDialog.OpenFile
        nameFilters: ["AppImage dosyaları (*.AppImage *.appimage)"]
        onAccepted: controller.importAppImageAsEmulator(selectedFile.toString())
    }

    FileDialog {
        id: emulatorDesktopFileDialog
        title: "Emülatör .desktop dosyası seç"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Desktop dosyaları (*.desktop)"]
        onAccepted: controller.importDesktopAsEmulator(selectedFile.toString())
    }

    FileDialog {
        id: gameIconFileDialog
        title: "İsteğe bağlı ikon seç"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Görseller (*.png *.svg *.jpg *.jpeg *.xpm)"]
        onAccepted: gameIconPath.text = selectedFile.toString()
    }

    FileDialog {
        id: assetFileDialog
        title: "Görsel seç"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Görseller (*.png *.svg *.jpg *.jpeg)"]
        onAccepted: {
            if (window.assetTarget === "game") controller.setGameAsset(window.assetIndex, assetType.currentText, selectedFile.toString())
            else controller.setEmulatorAsset(window.assetIndex, assetType.currentText, selectedFile.toString())
            assetDialog.close()
        }
    }
}
