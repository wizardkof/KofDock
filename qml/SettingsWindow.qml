// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: window
    required property var settings
    required property var controller
    required property var autostart
    required property var hostDock

    title: "KofDock · Preferências"
    width: 640
    height: 640
    minimumWidth: 520
    minimumHeight: 480
    color: "#171a22"
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowCloseButtonHint

    component SectionTitle: Label {
        Layout.fillWidth: true
        Layout.topMargin: 14
        Layout.bottomMargin: 5
        font.pixelSize: 17
        font.bold: true
        color: "#f1f3fa"
    }
    component SettingSlider: ColumnLayout {
        id: setting
        property string caption: ""
        property string displayValue: ""
        property real minimum: 0
        property real maximum: 100
        property real increment: 1
        property real current: 0
        signal applied(real value)
        Layout.fillWidth: true
        spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Label { text: setting.caption; color: "#e0e3ed"; Layout.fillWidth: true }
            Label { text: setting.displayValue; color: "#a7c8ff"; font.bold: true }
        }
        Slider {
            Layout.fillWidth: true
            from: setting.minimum
            to: setting.maximum
            stepSize: setting.increment
            value: setting.current
            onMoved: setting.applied(value)
        }
    }
    component Description: Label {
        Layout.fillWidth: true
        color: "#aeb7c7"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }
    component PreferenceToggle: Switch {
        Layout.fillWidth: true
        font.pixelSize: 13
        palette.windowText: "#e0e3ed"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 2
                Label { text: "KofDock"; font.pixelSize: 27; font.bold: true; color: "white" }
                Label { text: "0.2.9-hf3 · KDE Plasma / Wayland"; color: "#a9afc2"; font.pixelSize: 12 }
            }
            Item { Layout.fillWidth: true }
            Label { text: "PERSONALIZAÇÃO"; color: "#9cbce6"; font.pixelSize: 11 }
        }
        TabBar {
            id: tabs
            Layout.fillWidth: true
            background: Rectangle { color: "#222733"; radius: 10 }
            TabButton { text: "Aparência" }
            TabButton { text: "Animações" }
            TabButton { text: "Comportamento" }
            TabButton { text: "Pilhas" }
        }
        StackLayout {
            id: views
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            ScrollView {
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width - 22
                    spacing: 10
                    SectionTitle { text: "Ícones e painel" }
                    SettingSlider {
                        caption: "Tamanho dos ícones"
                        displayValue: window.settings.iconSize + " px"
                        minimum: 28; maximum: 88; increment: 1; current: window.settings.iconSize
                        onApplied: value => window.settings.iconSize = Math.round(value)
                    }
                    SettingSlider {
                        caption: "Espaçamento"
                        displayValue: window.settings.spacing + " px"
                        minimum: 0; maximum: 24; increment: 1; current: window.settings.spacing
                        onApplied: value => window.settings.spacing = Math.round(value)
                    }
                    SettingSlider {
                        caption: "Transparência do fundo"
                        displayValue: Math.round((1 - window.settings.backgroundOpacity) * 100) + "%"
                        minimum: 0; maximum: 1; increment: 0.01
                        current: 1 - window.settings.backgroundOpacity
                        onApplied: value => window.settings.backgroundOpacity = 1 - value
                    }
                    SettingSlider {
                        caption: "Arredondamento"
                        displayValue: window.settings.cornerRadius + " px"
                        minimum: 0; maximum: 48; increment: 1
                        current: window.settings.cornerRadius
                        onApplied: value => window.settings.cornerRadius = Math.round(value)
                    }
                    SectionTitle { text: "Cores" }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Fundo"; color: "white"; Layout.fillWidth: true }
                        Rectangle { width: 24; height: 24; radius: 5; color: window.settings.backgroundColor }
                        TextField {
                            text: window.settings.backgroundColor
                            Layout.preferredWidth: 135
                            maximumLength: 7
                            validator: RegularExpressionValidator { regularExpression: /^#[0-9a-fA-F]{6}$/ }
                            onEditingFinished: if (acceptableInput) window.settings.backgroundColor = text
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Indicadores"; color: "white"; Layout.fillWidth: true }
                        Rectangle { width: 24; height: 24; radius: 5; color: window.settings.indicatorColor }
                        TextField {
                            text: window.settings.indicatorColor
                            Layout.preferredWidth: 135
                            maximumLength: 7
                            validator: RegularExpressionValidator { regularExpression: /^#[0-9a-fA-F]{6}$/ }
                            onEditingFinished: if (acceptableInput) window.settings.indicatorColor = text
                        }
                    }
                    SectionTitle { text: "Detalhes" }
                    PreferenceToggle {
                        text: "Desenhar borda no painel"
                        checked: window.settings.borderEnabled
                        onToggled: window.settings.borderEnabled = checked
                    }
                    PreferenceToggle {
                        text: "Sombra atrás do painel"
                        checked: window.settings.shadowEnabled
                        onToggled: window.settings.shadowEnabled = checked
                    }
                    PreferenceToggle {
                        text: "Indicadores de aplicativos abertos"
                        checked: window.settings.showIndicators
                        onToggled: window.settings.showIndicators = checked
                    }
                }
            }
            ScrollView {
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width - 22
                    spacing: 10
                    SectionTitle { text: "Ampliação parabólica" }
                    SettingSlider {
                        caption: "Zoom"
                        displayValue: Number(window.settings.zoomScale).toFixed(2) + "×"
                        minimum: 1.0; maximum: 2.1; increment: 0.05
                        current: window.settings.zoomScale
                        onApplied: value => window.settings.zoomScale = value
                    }
                    SettingSlider {
                        caption: "Alcance da ampliação"
                        displayValue: window.settings.zoomSpread + " vizinhos"
                        minimum: 0; maximum: 4; increment: 1
                        current: window.settings.zoomSpread
                        onApplied: value => window.settings.zoomSpread = Math.round(value)
                    }
                    SettingSlider {
                        caption: "Suavização"
                        displayValue: window.settings.animationMs + " ms"
                        minimum: 0; maximum: 450; increment: 15
                        current: window.settings.animationMs
                        onApplied: value => window.settings.animationMs = Math.round(value)
                    }
                    Description {
                        text: "O Dock mantém a geometria da janela fixa durante a ampliação. Os ícones são transformados pela cena gráfica, sem recriar texturas a cada movimento."
                    }
                    SectionTitle { text: "Efeito de minimização" }
                    PreferenceToggle {
                        text: "Publicar geometria dos ícones no KWin (experimental)"
                        checked: window.settings.publishMinimizeGeometry
                        onToggled: window.settings.publishMinimizeGeometry = checked
                    }
                    Description {
                        text: "A publicação é compatível com efeitos que usam a posição da tarefa, mas não reproduz a animação Genie da Apple nem ativa automaticamente o efeito Lâmpada Mágica."
                    }
                }
            }
            ScrollView {
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width - 22
                    spacing: 10
                    SectionTitle { text: "Ocultação" }
                    PreferenceToggle {
                        text: "Ocultar quando uma janela maximizar ou se aproximar"
                        checked: window.settings.smartHide
                        onToggled: window.settings.smartHide = checked
                    }
                    PreferenceToggle {
                        text: "Ocultar sempre que o mouse sair"
                        checked: window.settings.autoHide
                        onToggled: window.settings.autoHide = checked
                    }
                    SettingSlider {
                        caption: "Atraso ao ocultar"
                        displayValue: window.settings.hideDelay + " ms"
                        minimum: 100; maximum: 2500; increment: 50
                        current: window.settings.hideDelay
                        onApplied: value => window.settings.hideDelay = Math.round(value)
                    }
                    SettingSlider {
                        caption: "Distância da borda inferior"
                        displayValue: window.settings.bottomGap + " px"
                        minimum: 0; maximum: 32; increment: 1
                        current: window.settings.bottomGap
                        onApplied: value => window.settings.bottomGap = Math.round(value)
                    }
                    Description { text: "Quando oculto, uma faixa de ativação permanece na borda. Mova o mouse até a borda inferior para revelar o Dock." }
                    SectionTitle { text: "Janelas e notificações" }
                    PreferenceToggle {
                        text: "Mostrar aplicativos abertos não fixados"
                        checked: window.settings.showRunningApps
                        onToggled: window.settings.showRunningApps = checked
                    }
                    PreferenceToggle {
                        text: "Minimizar ao clicar na janela já ativa"
                        checked: window.settings.minimizeOnClick
                        onToggled: window.settings.minimizeOnClick = checked
                    }
                    PreferenceToggle {
                        text: "Mostrar contadores de mensagens (quando disponíveis)"
                        checked: window.settings.showBadges
                        onToggled: window.settings.showBadges = checked
                    }
                    Description { text: "Os contadores dependem do aplicativo enviar informações pelo protocolo Unity LauncherEntry. Nenhuma contagem é fabricada." }
                    SectionTitle { text: "Inicialização automática" }
                    PreferenceToggle {
                        id: autostartToggle
                        text: "Iniciar KofDock ao entrar no KDE Plasma"
                        checked: window.autostart.enabled
                        onToggled: {
                            // Write/delete ~/.config/autostart immediately. On
                            // permission failure the backend keeps the old state.
                            if (!window.autostart.setEnabled(checked)) {
                                Qt.callLater(() => { autostartToggle.checked = window.autostart.enabled; });
                            }
                        }
                    }
                    Description {
                        text: "Ativado por padrão em novas instalações. Desativar remove a entrada da inicialização automática, sem fechar o Dock em execução. A escolha é preservada nas atualizações."
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: window.autostart.lastError.length > 0
                        color: "#ff9999"
                        wrapMode: Text.WordWrap
                        text: window.autostart.lastError
                    }
                    SectionTitle { text: "Monitor do KofDock" }
                    Description {
                        text: "Saída atualmente solicitada: " + window.controller.targetScreenName +
                              ". A alteração abaixo recria apenas a superfície do Dock, sem reiniciar o KDE."
                    }
                    ComboBox {
                        id: monitorSelector
                        Layout.fillWidth: true
                        model: window.controller.screenNames
                        currentIndex: Math.max(0, window.controller.screenNames.indexOf(window.settings.screenName))
                        onActivated: window.controller.selectScreen(currentIndex === 0 ? "" : String(currentText))
                    }
                }
            }
            ScrollView {
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width - 22
                    spacing: 10
                    SectionTitle { text: "Pilhas em leque" }
                    PreferenceToggle {
                        text: "Abrir pilhas quando o mouse estiver sobre o ícone"
                        checked: window.settings.fanEnabled
                        onToggled: window.settings.fanEnabled = checked
                    }
                    SettingSlider {
                        caption: "Tempo para abrir"
                        displayValue: window.settings.fanDelay + " ms"
                        minimum: 120; maximum: 1200; increment: 20
                        current: window.settings.fanDelay
                        onApplied: value => window.settings.fanDelay = Math.round(value)
                    }
                    Description { text: "Leque curvo no estilo Cairo-Dock para qualquer quantidade de itens, sem mudar para grade. Quando faltar altura, role a roda do mouse sobre os ícones da pilha para acessar os demais. Sem miniaturas de janelas." }
                    SectionTitle { text: "Sua pilha de aplicativos" }
                    TextField {
                        id: stackNameField
                        Layout.fillWidth: true
                        placeholderText: "Nome da pilha"
                        text: window.settings.stackName
                        onEditingFinished: if (text.trim().length > 0) window.settings.stackName = text.trim()
                    }
                    Description {
                        text: "Para criar uma pilha, clique com o botão direito em um aplicativo e escolha 'Adicionar à pilha'. Também é possível arrastar um ícone para a pilha já criada."
                    }
                    Repeater {
                        model: window.settings.stackLaunchers
                        delegate: RowLayout {
                            required property int index
                            required property var modelData
                            Layout.fillWidth: true
                            Label {
                                text: dockServices.displayName(String(modelData), "", "", String(modelData))
                                color: "#e5e8ef"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Button {
                                text: "↑"
                                enabled: index > 0
                                onClicked: window.hostDock.moveStackItem(index, -1)
                            }
                            Button {
                                text: "↓"
                                enabled: index < window.settings.stackLaunchers.length - 1
                                onClicked: window.hostDock.moveStackItem(index, 1)
                            }
                            Button {
                                text: "Retirar"
                                onClicked: window.hostDock.removeFromStack(String(modelData))
                            }
                        }
                    }
                    Label {
                        visible: window.settings.stackLaunchers.length === 0
                        Layout.fillWidth: true
                        text: "Nenhuma pilha criada. Seus aplicativos fixados continuam intactos."
                        wrapMode: Text.WordWrap
                        color: "#97a5b7"
                    }
                    Button {
                        text: "Desfazer pilha (preservar aplicativos)"
                        Layout.fillWidth: true
                        enabled: window.settings.stackLaunchers.length > 0
                        onClicked: window.settings.stackLaunchers = []
                    }
                    SectionTitle { text: "Favoritos do Plasma" }
                    Button {
                        text: "Importar aplicativos fixados do painel KDE"
                        Layout.fillWidth: true
                        onClicked: {
                            const count = window.hostDock.importKdeLaunchers();
                            feedback.text = count > 0 ? "Adicionados " + count + " favoritos." : "Nenhum novo favorito disponível.";
                        }
                    }
                    Label { id: feedback; color: "#96bbff"; Layout.fillWidth: true }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "As alterações são salvas automaticamente."
                color: "#99a6b8"
                font.pixelSize: 11
                Layout.fillWidth: true
            }
            Button { text: "Fechar"; onClicked: window.close() }
        }
    }
}
