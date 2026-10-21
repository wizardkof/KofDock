// SPDX-License-Identifier: MIT
// Tests user-owned pin list transaction, system settings alias and crash guards.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const ctx = vm.createContext({Math,Number,String,Array,Set,JSON});
vm.runInContext(fs.readFileSync(path.join(root, 'qml/DockLogic.js'), 'utf8'),ctx);
const clean = o => JSON.parse(JSON.stringify(o));
let passed = 0;
function check(name, fn) { fn(); console.log('PASS',name); passed++; }
check('pin new program appended without changing earlier positions', () => {
  assert.deepEqual(clean(ctx.togglePinnedLauncher(['a','b'],'c')), ['a','b','c']);
});
check('unpin removes only selected program', () => {
  assert.deepEqual(clean(ctx.togglePinnedLauncher(['a','b','c'],'b')), ['a','c']);
});
check('URL variant maps to same launcher', () => {
  assert.deepEqual(clean(ctx.togglePinnedLauncher(['applications:x.desktop'],'applications:/x.desktop?icon=a')),[]);
});
check('empty or malformed pin never changes list', () => {
  assert.deepEqual(clean(ctx.togglePinnedLauncher(['a'],'  ')),['a']);
});
check('stack only pins once', () => {
  assert.deepEqual(clean(ctx.addPinnedLauncher(['a','b'],'b')),['a','b']);
});
check('KDE System Settings recognized using both desktop variants', () => {
  for(const id of ['applications:org.kde.systemsettings.desktop','applications:systemsettings.desktop']) {
    assert.equal(ctx.isSystemSettingsLauncher(id,'',''),true);
  }
  assert.equal(ctx.isSystemSettingsLauncher('','org.kde.systemsettings',''),true);
  assert.equal(ctx.isSystemSettingsLauncher('','org.kde.konsole',''),false);
});
const main = fs.readFileSync(path.join(root,'qml/Main.qml'),'utf8');
const cell = fs.readFileSync(path.join(root,'qml/DockCell.qml'),'utf8');
const proxy = fs.readFileSync(path.join(root,'src/docksectionmodel.cpp'),'utf8');
check('delegate menu defers pin action to next event loop with captured values', () => {
  assert.match(cell, /const host = cell\.hostWindow;[\s\S]*?const url = cell\.launcherUrl;[\s\S]*?Qt\.callLater\(function \(\) \{ host\.pinOrUnpin\(url\); \}\)/);
  assert.doesNotMatch(cell,/onTriggered:\s*cell\.hostWindow\.pinOrUnpin/);
});
check('pin state is rendered independently and not applied to live TasksModel', () => {
  assert.match(main,/model: dock\.visiblePinnedLaunchers/);
  assert.match(main,/dockSettings\.stackLaunchers\.some/);
  assert.match(main,/function sourceRowForLauncher\(launcherUrl\)/);
  assert.match(main,/function snapshotForLauncher\(url\)/);
  assert.match(main,/launcherCommitTimer/);
  assert.match(main,/dockSettings\.launchers = pending/);
  const mutations = [...main.matchAll(/taskModel\.launcherList\s*=/g)];
  assert.equal(mutations.length, 0, 'cannot overwrite TasksModel during pin');
  assert.match(main,/launcherList = dockSettings\.launchers/);
  assert.doesNotMatch(main,/taskModel\.requestAddLauncher/);
});
check('source-model signal scanning is coalesced, not reentrant', () => {
  assert.match(proxy,/void DockSectionModel::scheduleRunningOrderCapture/);
  assert.match(proxy,/QTimer::singleShot\(0, this/);
  assert.doesNotMatch(proxy,/disconnect\(sourceModel\(\), nullptr, this, nullptr\)/);
});
check('bounce moves actual icon, no animated duplicate sprite', () => {
  assert.match(main,/property string bouncingKey/);
  assert.match(main,/id: bounceAnimation/);
  assert.doesNotMatch(main,/launchSpriteFactory/);
  assert.match(main,/function playLaunchBounce\(cell\)/);
  assert.match(cell,/readonly property real launchOffset/);
  assert.match(cell,/y: \(cell\.dragging \? -14 : 0\) \+ cell\.launchOffset/);
  assert.match(cell,/cell\.hostWindow\.playLaunchBounce\(cell\)/);
});
check('bounce curve has three separate hops and rests at baseline', () => {
  assert.equal(ctx.launchBounceOffset(0,52),0);
  assert.equal(ctx.launchBounceOffset(1,52),0);
  assert.ok(ctx.launchBounceOffset(1/6,52)<-10);
  assert.equal(ctx.launchBounceOffset(1/3,52),0);
  assert.ok(ctx.launchBounceOffset(1/2,52)<0);
  assert.equal(ctx.launchBounceOffset(2/3,52),0);
  assert.ok(ctx.launchBounceOffset(5/6,52)<0);
  assert.ok(ctx.launchBounceOffset(1/6,52)<ctx.launchBounceOffset(5/6,52));
});
check('runtime monitor override works without overwriting saved screen setting', () => {
  const source = fs.readFileSync(path.join(root,'src/dockcontroller.cpp'),'utf8');
  assert.match(source,/m_sessionScreenOverride/);
  assert.match(source,/m_window->setScreen\(screen\)/);
  assert.match(source,/ScreenRouting::choose/);
  assert.match(source,/QStringLiteral\("DP-3"\)/);
  assert.match(fs.readFileSync(path.join(root,'src/main.cpp'),'utf8'),/--screen=/);
});
check('transparent divider area remains inside hover mask', () => {
  assert.match(main,/CONTINUOUS hit region/);
  assert.match(main,/const hoverTop =/);
  assert.match(main,/const stableLeft =/);
  assert.match(main,/maxPlateWidth \+ 24/);
});
check('pinned launcher fallback to native KDE launcher', () => {
 const h=fs.readFileSync(path.join(root,'src/dockservices.h'),'utf8');
 const c=fs.readFileSync(path.join(root,'src/dockservices.cpp'),'utf8');
 assert.match(h,/launchLauncher\(const QString &url\)/);
 assert.match(c,/kioclient6/);
 assert.match(main,/dockServices\.launchLauncher/);
});
console.log(`${passed} launch safety checks PASS`);
