// SPDX-License-Identifier: MIT
// Regression tests for pointer ownership, fan dismissal and stable icon order.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '..');
const code = fs.readFileSync(path.join(root, 'qml/DockLogic.js'), 'utf8');
const ctx = vm.createContext({Math, Number, String, Array, Set});
vm.runInContext(code, ctx);
let passed = 0;
function test(name, fn) { fn(); passed += 1; console.log('PASS', name); }
const {fanHoverAction, preservePinnedOrder, stableRowLeft, fanBridgeGeometry,
       fanLeafBend, fanLabelTilt, fanIconTilt} = ctx;
const clean = x=>JSON.parse(JSON.stringify(x));

test('initial hover does not bypass the configured delay',()=>{
  assert.equal(fanHoverAction(false, false, 2), 'none');
});
test('crossing from browser fan to VSCode fan switches immediately',()=>{
  assert.equal(fanHoverAction(true, false, 2), 'switch');
});
test('crossing to a single-window app dismisses the old fan',()=>{
  assert.equal(fanHoverAction(true, false, 0), 'dismiss');
});
test('remaining over current app never restarts the reveal',()=>{
  assert.equal(fanHoverAction(true, true, 5), 'none');
});
test('dropping to no stack never freezes previous window labels',()=>{
  assert.equal(fanHoverAction(true, false, undefined), 'dismiss');
});
test('pinned order preserved when KDE source model reorders on launch',()=>{
  assert.deepEqual(clean(preservePinnedOrder(['applications:vs.desktop','applications:zen.desktop'],
    ['applications:zen.desktop','applications:vs.desktop'])),
    ['applications:vs.desktop','applications:zen.desktop']);
});
test('newly pinned app is appended without moving existing favorites',()=>{
  assert.deepEqual(clean(preservePinnedOrder(['a','b'],['c','b','a'])),['a','b','c']);
});
test('explicit unpin removes only the correct favorite',()=>{
  assert.deepEqual(clean(preservePinnedOrder(['a','b','c'],['c','a'])),['a','c']);
});
test('launcher URL suffix normalization prevents double entry',()=>{
  assert.deepEqual(clean(preservePinnedOrder(['applications:zen.desktop'],
    ['applications:/zen.desktop?icon=zen'])), ['applications:zen.desktop']);
});
test('empty favorites preserve default centered running apps',()=>{
  assert.equal(stableRowLeft(1920,0,58,8,240), 840);
});
test('full icon row stays centered as unpinned running apps appear',()=>{
  const center = 1920 / 2;
  for(const extra of [0,58,130,210,350]){
    const width = 3*58+2*8+extra;
    const left = stableRowLeft(1920,3,58,8,width);
    assert.equal(left + width/2, center);
  }
});
test('long rows stay inside screen bounds',()=>{
  assert.equal(stableRowLeft(800,3,58,8,1100),4);
});
test('curved fan has a small but non-zero offset',()=>{
  assert.ok(fanLeafBend(1,2)>fanLeafBend(0,2));
  assert.ok(fanIconTilt(1,2)>fanLabelTilt(1,2));
  assert.ok(fanLabelTilt(3,7)>=20 && fanLabelTilt(3,7)<=24);
});
test('fan trigger bridge is narrower than neighboring app slot',()=>{
  const b=fanBridgeGeometry(390,290);
  assert.ok(b.width<58);
  assert.equal(b.x+b.width/2,390);
});

const main = fs.readFileSync(path.join(root,'qml/Main.qml'),'utf8');
const cell = fs.readFileSync(path.join(root,'qml/DockCell.qml'),'utf8');
const stack= fs.readFileSync(path.join(root,'qml/StackCell.qml'),'utf8');
const fan = fs.readFileSync(path.join(root,'qml/FanStack.qml'),'utf8');
const proxy= fs.readFileSync(path.join(root,'src/docksectionmodel.cpp'),'utf8');
test('switching is wired both for normal cells and user stacks',()=>{
  assert.match(main,/function switchOpenFan\(owner, entries\)/);
  assert.match(main,/reconcileFanAtPointer/);
  assert.match(cell,/switchOpenFan\(cell, eligible \? cell\.windowEntries\(\) : \[\]\)/);
  assert.match(stack,/switchOpenFan\(cell, eligible \? cell\.entries\(\) : \[\]\)/);
});
test('the fan has NO window-wide hover interceptor',()=>{
  assert.doesNotMatch(fan,/HoverHandler \{ id: hoverZone \}/);
  assert.match(fan,/id: bridge/);
  assert.match(fan,/id: iconHit/);
  assert.match(fan,/id: labelHit/);
  assert.match(fan,/model: fan.visibleRows/);
  assert.doesNotMatch(fan,/gridMode|gridPlate/);
});
test('input mask takes only actual fan item regions, not full 660px rectangle',()=>{
  assert.match(main,/for \(const rect of fanPanel\.inputRegions\(\)\)/);
  assert.doesNotMatch(main,/fanPanel\.width \+ 24, dock\.height - fanPanel\.y/);
  assert.match(fan,/function inputRegions\(\)/);
});
test('fan dismissal and fade never wait the previous 650ms',()=>{
  assert.match(main,/id: fanDismiss\s+interval: 115/);
  assert.match(main,/id: fanClear\s+interval: 165/);
  assert.match(fan,/id: fold[\s\S]{0,90}duration: 135/);
});
test('running programs do not mutate KDE task-manager sort by auto launch',()=>{
  assert.match(main,/runningModel\.moveRunning\(sourceRow, targetRow\)/);
  assert.match(proxy,/bool DockSectionModel::lessThan/);
  assert.match(proxy,/int DockSectionModel::pinnedRank/);
  assert.match(proxy,/void DockSectionModel::captureRunningOrder/);
  assert.doesNotMatch(main,/taskModel\.move\(sourceRow, targetRow\)/);
});
test('pinned row is stable even when total running app count grows',()=>{
  assert.match(main,/x: Logic\.stableRowLeft\(dock\.width, visiblePinnedLaunchers\.length/);
  assert.doesNotMatch(main,/anchors\.horizontalCenter: parent\.horizontalCenter\s+y: plate\.y/);
});
console.log(`${passed} cenários adicionais PASS (hover, ordenação, curva e fechamento).`);
