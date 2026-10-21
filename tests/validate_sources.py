#!/usr/bin/env python3
"""Static source/integration contracts; NOT a replacement for Qt compilation."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
required = [
    'CMakeLists.txt', 'src/main.cpp', 'src/docksettings.h', 'src/docksettings.cpp',
    'src/dockautostart.h', 'src/dockautostart.cpp',
    'src/dockcontroller.h', 'src/dockcontroller.cpp', 'src/docksectionmodel.h',
    'src/docksectionmodel.cpp', 'src/dockservices.cpp', 'src/dockservices.h',
    'qml/Main.qml', 'qml/DockCell.qml', 'qml/StackCell.qml', 'qml/FanStack.qml',
    'qml/DockLogic.js', 'qml/SettingsWindow.qml', 'scripts/install.sh',
    'scripts/uninstall.sh', 'tests/test_dock_logic.cjs', 'tests/test_hover_order.cjs',
    'tests/test_fan_layout.cjs', 'tests/test_launch_safety.cjs', 'tests/test_docksectionmodel.cpp',
    'tests/test_dockautostart.cpp',
]
for rel in required:
    assert (root / rel).is_file(), f'Missing {rel}'
for script in (root / 'scripts').glob('*.sh'):
    subprocess.run(['bash', '-n', str(script)], check=True)
main = (root/'qml/Main.qml').read_text()
cell = (root/'qml/DockCell.qml').read_text()
fan = (root/'qml/FanStack.qml').read_text()
stack = (root/'qml/StackCell.qml').read_text()
ui = (root/'qml/SettingsWindow.qml').read_text()
controller = (root/'src/dockcontroller.cpp').read_text()
controller_h = (root/'src/dockcontroller.h').read_text()
settings = (root/'src/docksettings.cpp').read_text()
sections = (root/'src/docksectionmodel.cpp').read_text()
installer = (root/'scripts/install.sh').read_text()
autostart = (root/'src/dockautostart.cpp').read_text()
autostart_h = (root/'src/dockautostart.h').read_text()
app_main = (root/'src/main.cpp').read_text()
uninstaller = (root/'scripts/uninstall.sh').read_text()
cmake = (root/'CMakeLists.txt').read_text()
logic = (root/'qml/DockLogic.js').read_text()

contracts = {
    'autostart toggle added under behavior preferences':
        'Iniciar KofDock ao entrar no KDE Plasma' in ui and
        'window.autostart.setEnabled(checked)' in ui and
        'required property var autostart' in ui and
        'autostart: dockAutostart' in main,
    'new installs default to autostart enabled':
        'startOnLogin' in autostart and
        'value(QStringLiteral("startOnLogin"), true)' in autostart and
        'startup_mode=--sync-autostart' in installer,
    'user disable persists and removes file without deleting permission desktop':
        'QFile::remove(m_entryPath)' in autostart and
        'm_preferences->setValue(QStringLiteral("startOnLogin"), desired)' in autostart and
        'm_entryPath' in autostart and
        'org.kof.KofDock.desktop' in installer and
        'X-KDE-Wayland-Interfaces=' in installer,
    'autostart CLI available without compositor or instance lock':
        '--sync-autostart' in app_main and
        '--disable-autostart' in app_main and
        'QCoreApplication cli(argc, argv)' in app_main and
        app_main.index('QCoreApplication cli(argc, argv)') < app_main.index('QGuiApplication app(argc, argv)'),
    'autostart writer is atomic with limited KDE session scope':
        'QSaveFile out(m_entryPath)' in autostart and
        'OnlyShowIn=KDE;' in autostart and
        'StartupNotify=false' in autostart and
        'QStandardPaths::ConfigLocation' in autostart,
    'installer honors explicitly disabled settings on upgrades':
        'startup_mode=--sync-autostart' in installer and
        '"$bin_dir/kofdock" "$startup_mode"' in installer and
        'cp "$app_dir/org.kof.KofDock.desktop"' not in installer,
    'uninstall removes autostart even if custom XDG_CONFIG_HOME':
        'XDG_CONFIG_HOME' in uninstaller and
        'org.kof.KofDock.desktop' in uninstaller,
    'real C++ filesystem regression tests wired to CTest':
        'add_test(NAME dock_autostart' in cmake and
        'tests/test_dockautostart.cpp' in cmake and
        'void togglingOffRemovesOnlyAutostartAndSurvivesReinstall()' in (root/'tests/test_dockautostart.cpp').read_text(),

    'monitor geometry exported directly from QScreen':
        'm_selectedScreen->geometry()' in controller and
        'Q_PROPERTY(QRect screenGeometry' in controller_h and
        'onScreenGeometryChanged' in main,
    'monitor selected before constructing native LayerShell integration':
        controller.index('chooseScreen();\n    if (!m_selectedScreen)') < controller.index('    configureLayer();') and
        'if (window->handle())' in controller and
        'window->setScreen(m_selectedScreen);' in controller,
    'changing output reconstructs Wayland wl_surface asynchronously':
        'QTimer::singleShot(0, this' in controller and
        'm_window->destroy();' in controller and
        'm_window->show();' in controller and
        'LayerShellQt::Window::get(m_window)' in controller,
    'gui selection beats command line preference in same session':
        'm_sessionScreenOverride.clear();' in controller and
        'Q_INVOKABLE void selectScreen(' in controller_h and
        'window.controller.selectScreen(' in ui,
    'settings panel displays the actual selected output':
        'window.controller.targetScreenName' in ui and
        'Q_PROPERTY(QString targetScreenName' in controller_h,
    'QML dock window dimensions follow selected output':
        'dockController.screenGeometry.width > 0' in main and
        'dockController.screenGeometry.height' in main,
    'screen selection unit tests are installed for host':
        'add_test(NAME dock_screen_routing' in cmake and
        (root/'tests/test_screenrouting.cpp').exists() and
        'ScreenRouting::choose(' in controller,
    'Qt task filtering uses valid C++ QRect':
        'screenGeometry: dockController.screenGeometry' in main and
        'dock.screen.geometry' not in main,
    'real maximized and proximity window detection':
        all(x in main for x in ('visibilityModel', 'IsMaximized', 'IsMinimized',
                                'dockProximityZone', 'rectanglesOverlap')),
    'edge reveal trigger and proper hidden mask':
        all(x in main for x in ('edgeTriggerHeight: 12', 'height - edgeTriggerHeight',
                               'onRevealedChanged', 'hideTimer')),
    'fan presented inside existing layer surface rather than new window':
        'FanStack {' in main and 'popupType: Popup.Window' not in fan and 'Window {' not in fan,
    'fan starts after hover, never on clicking normal icon':
        'fanTimer.restart()' in cell and 'onTriggered' in cell and
        'showStackForCell(cell, cell.windowEntries())' in cell,
    'fan stays available through pointer corridor':
        'pointerInFanCorridor' in main and 'fanPanel.panelHovered' in main and
        'dockController.setInputRegions(regions)' in main,
    'fan uses real KDE child model indices':
        'makeModelIndex(sourceRow, child)' in cell and
        'activateGroupWindow(entry.sourceRow, entry.childRow)' in main,
    'persistent multi-app stack managed by settings':
        'QStringList DockSettings::stackLaunchers() const' in settings and
        'dockSettings.stackLaunchers = current' in main and
        'visiblePinnedLaunchers' in main,
    'apps in stacks still in underlying task model':
        'm_stackLaunchers.contains(url)' in sections and
        'queueLauncherCommit(Logic.addPinnedLauncher' in main and 'taskModel.requestActivate(idx)' in main,
    'single curved fan regardless of item count':
        'readonly property int visibleCapacity:' in fan and
        'readonly property var scrollWindow:' in fan and
        'model: fan.visibleRows' in fan and
        'fanScrollWindow' in fan and 'function scrollBy(delta)' in fan and
        'gridMode' not in fan and 'gridPlate' not in fan and
        'entries.slice(0, 32)' not in main and
        'Math.min(32, countWindows)' not in cell and 'Kirigami.Icon' in fan,
    'icon count notifications preserved':
        'dockServices.badgeCount' in stack and 'dockServices.badgeRevision' in stack and
        'cell.unreadCount' in cell,
    'right click app action to create a stack':
        'Adicionar à pilha de aplicativos' in cell and
        'hostWindow.moveToStack' in cell,
    'stack can be undone without unpinning':
        'Desfazer pilha (preservar aplicativos)' in stack and
        'stackLaunchers = []' in ui,
    'mac-like layout separation':
        'id: stackSeparatorCell' in main and 'showStackDivider' in main and
        'id: separatorCell' in main,
    'settings grouped into four pages':
        'TabBar {' in ui and 'StackLayout {' in ui and
        all(n in ui for n in ('Aparência', 'Animações', 'Comportamento', 'Pilhas')),
    'CMake includes new QML components and no thumbnail':
        'qml/StackCell.qml' in cmake and 'qml/FanStack.qml' in cmake and
        'WindowThumbnail' not in cmake,
    'PipeWire preview and KWin capture permission removed':
        'PipeWire' not in main+cell+stack+fan and
        'zkde_screencast_unstable_v1' not in installer and
        not (root/'qml/WindowThumbnail.qml').exists(),
    'no forced shadow and fullscreen dock anchor kept':
        'setExclusiveZone(0)' in controller and
        'BOOL_SETTING(shadowEnabled, ShadowEnabled, false)' in settings and
        'surface->setAnchors(anchors)' in controller,
    'no auto-modification of Plasma panels':
        'kwriteconfig6' not in installer and 'plasma-org.kde.plasma.desktop-appletsrc' not in installer,
    'C++ model can be registered in QML':
        'class DockSectionModel : public' in (root/'src/docksectionmodel.h').read_text(),
    'fan does not override QtQuick Item.data':
        re.search(r'\b(?:required\s+|readonly\s+)?property\s+var\s+data\s*:', fan) is None and
        fan.count('readonly property var fanEntry: fan.entries[entryIndex]') == 1 and
        'leaf.fanEntry.icon' in fan,
    'wayland QML smoke-check prevents installing a broken executable':
        'KofDock: verificacao QML de inicializacao PASS' in (root/'src/main.cpp').read_text() and
        'if (checkQmlOnly)' in (root/'src/main.cpp').read_text() and
        '"$build_dir/kofdock" --check-qml' in installer and
        installer.index('"$build_dir/kofdock" --check-qml') < installer.index('mv -f "$tmp"'),
    'classic fan label anchored to icon rather than rotating around label center':
        'transformOrigin: fan.labelsLeft ? Item.Right : Item.Left' in fan and
        'height: 21' in fan and 'Math.min(142,' in fan,
    'classic fan offset below 90px, no oversized 185px arc':
        'const FAN_MAX_SWEEP = 86' in logic and
        'readonly property real curveLimit: Math.min(86' in fan,
    'input geometry follows rotated labels not invisible fan panel':
        'function fanLabelBounds(' in logic and
        'Logic.fanLabelBounds(' in fan and
        'id: iconHit' in fan and 'id: labelHit' in fan,
    'QML smoke test instantiates fan leaves before installation':
        'objectName: "kofdock-fan"' in fan and
        'findChild<QObject *>' in app_main and
        'entry.insert(QStringLiteral("title")' in app_main and
        'fan->setProperty("entries", examples)' in app_main and
        'fan->property("visibleRows")' in app_main,
    'version set for code and installer':
        'VERSION 0.2.10' in cmake and 'build-makefiles-0.2.10-beta2' in installer and '0.2.10-beta2' in (root/'src/main.cpp').read_text(),
    'fan opens at its base but bends progressively outward':
        'Logic.fanLeafPosition(' in fan and
        'function fanLeafBend(' in logic and
        'anchorX: dock.liveFanAnchorX - x' in main and
        'Logic.fanPlacement(dock.liveFanAnchorX, dock.width, width).x' in main,
    'stack origin tracks scaled dock icon position':
        'fanOwner.width / 2 + fanOwner.renderedShift' in main and
        'readonly property real renderedShift: slideTransform.x' in cell and
        'readonly property real renderedShift: stackSlide.x' in stack and
        'fanBaseY - y' in main,
    'fan entrance and exit are separately animated':
        all(token in fan for token in ('id: unfold', 'id: fold', 'property: "phase"',
                                      'Logic.fanStaggerProgress', 'phase > 0.001')) and
        'fanClear.restart()' in main and 'fanEntries = []' in main,
    'stack labels tilted legibly on dark plaques along the arc':
        'rotation: -10' not in fan and
        'Logic.fanIconTilt' in fan and 'Logic.fanLabelTilt' in fan and
        'Text.QtRendering' in fan and 'font.weight: Font.DemiBold' in fan and
        'elide: Text.ElideRight' in fan and 'font.family: Qt.application.font.family' in fan,
    'every running indicator shares its icon transformations':
        cell.index('id: runningIndicator') < cell.index('    function publishGeometry()') and
        'id: applicationIcon' in cell and 'x: cell.iconShift' in cell and
        stack.index('id: stackRunningIndicator') < stack.index('    Timer {\n        id: fanTimer'),
    'stack badges and indicators use visual scale for the whole stack':
        'id: stackBadge' in stack and 'id: stackRunningIndicator' in stack and
        'xScale: cell.zoomAmount / tile.maxZoom' in stack and
        'x: cell.iconShift' in stack,
    'Qt system QML plugins remain runtime-loaded rather than false link warnings':
        'add_executable(kofdock' in cmake and 'qt_add_executable(kofdock' not in cmake and
        'qt_add_resources(kofdock' in cmake and 'Qt6::Quick' in cmake,
    'animated fan is graphical not PipeWire preview':
        'PipeWire' not in fan and 'Screencasting' not in fan and
        'model: fan.visibleRows' in fan and 'Logic.fanLabelTilt' in fan,
    'no full-panel hover capture or click region':
        'id: hoverZone' not in fan and 'id: bridge' in fan and
        'id: iconHit' in fan and 'id: labelHit' in fan and 'function inputRegions()' in fan and
        'fanPanel.inputRegions()' in main and
        'fanPanel.width + 24, dock.height - fanPanel.y' not in main and
        'id: maskSchedule' in fan and 'interval: 80' in fan and 'Logic.fanLabelBounds(' in fan,
    'fast fold and old fan dismissal on new hover':
        'function switchOpenFan(owner, entries)' in main and
        'reconcileFanAtPointer' in main and 'interval: 115' in main and
        'interval: 165' in main and 'duration: 135' in fan and
        'switchOpenFan(cell, eligible ? cell.entries() : [])' in stack and
        'switchOpenFan(cell, eligible ? cell.windowEntries() : [])' in cell,
    'preferred alias is matched by exact resolved launcher-list slot':
        'sourceModel()->property("launcherList")' in sections and
        'launcherPositionForUrl(primary) == aliasPosition' in sections and
        'bool DockSectionModel::matchesLauncher' in sections and
        'runningModel.sourceRowForLauncher(String(launcherUrl))' in main and
        'preferredBrowserNeverClaimsOtherKdeLauncherSlots' in (root/'tests/test_docksectionmodel.cpp').read_text(),
    'pinned and running app order independent from KDE model mutation':
        'Logic.stableRowLeft(dock.width, visiblePinnedLaunchers.length' in main and
        'function queueLauncherCommit(next)' in main and
        'runningModel.moveRunning(sourceRow, targetRow)' in main and
        'bool DockSectionModel::lessThan' in sections and
        'captureRunningOrder' in sections and 'model: dock.visiblePinnedLaunchers' in main,
    'pinning source-model and bounce regressions are wired to CTest':
        'dock_launch_safety' in cmake and
        'tests/test_launch_safety.cjs' in cmake and
        'void DockSectionModel::scheduleRunningOrderCapture()' in sections and
        'function playLaunchBounce(cell)' in main and
        'function queueLauncherCommit(next)' in main,
    'pin/unpin cannot mutate running KDE TasksModel launcherList':
        'taskModel.launcherList = newList' not in main and
        'launcherList = dockSettings.launchers' in main and
        'dockSettings.launchers = pending' in main and
        'model: dock.visiblePinnedLaunchers' in main,
    'actual icon bounce without duplicate overlay':
        'launchSpriteFactory' not in main and 'property string bouncingKey' in main and
        'readonly property real launchOffset' in cell and
        'Logic.launchBounceOffset' in cell,
    'primary screen uses DP-3 if present, with explicit override preserved':
        'QStringLiteral("DP-3")' in controller and 'm_sessionScreenOverride' in controller,
    'pointer mask includes transparent divider and plate':
        'CONTINUOUS hit region' in main and 'const hoverTop' in main,
    'hover and order regression suite is wired to CTest':
        'dock_hover_order_stability' in cmake and
        'tests/test_hover_order.cjs' in cmake,
}
for label, passed in contracts.items():
    print(('PASS' if passed else 'FAIL')+': '+label)
assert all(contracts.values()), 'Static source contract failure'

# Basic delimiter balance is a guard, not a QML parser or a runtime proof.
for qml in (root/'qml').glob('*.qml'):
    content = qml.read_text()
    assert content.startswith('// SPDX-License-Identifier: MIT')
    assert content.count('{') == content.count('}'), str(qml)
    assert content.count('(') == content.count(')'), str(qml)
print(f'PASS: {len(contracts)} static contracts, shell syntax and QML marker checks.')
print('NOT RUN: Qt6 compiler, qmllint, Wayland interaction, 60/180Hz rendering.')
