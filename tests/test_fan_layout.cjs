// SPDX-License-Identifier: MIT
// Regression tests: one curved Cairo-style fan, regardless of stack size.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root=path.resolve(__dirname,'..');
const code=fs.readFileSync(path.join(root,'qml/DockLogic.js'),'utf8');
const logic=vm.createContext({Math,Number,String,Array});
vm.runInContext(code,logic);
const {fanPlacement,fanLeafDistance,fanStaggerProgress,fanLabelsLeft,
       fanLeafBend,fanIconTilt,fanLabelTilt,fanCurveDirection,
       fanVisibleCapacity,fanScrollWindow,fanBridgeGeometry,stackPresentation,
       fanLeafPosition,fanLabelBounds}=logic;
let n=0;
function test(name,fn){fn();n++;console.log('PASS',name)}

for (const count of [0,1,2,7,8,10,15,32,33,100,512,10000]) {
 test(`count=${count} always remains fan`,()=>assert.equal(stackPresentation(count),'fan'));
}
test('placement follows exact source x in middle and edges',()=>{
 for(const src of [27,300,980,1876]){
  const p=fanPlacement(src,1920,690);
  assert.equal(p.x+p.anchorX,src);
  assert.ok(p.x>=4 && p.x+p.width<=1916);
 }
});
test('invalid screen does not produce a fake visible fan',()=>{
 assert.equal(fanPlacement(100,0,690).width,0);
});
test('classic fan stays compact and curved with any number of items',()=>{
 for(const count of [8,12,32,100,10000]){
  let previous=-Infinity;
  for(let i=0;i<Math.min(count,23);i++){
   const bend=fanLeafBend(i,count,86);
   assert.ok(bend>=previous && bend<=86);
   if(i===0) assert.equal(bend,0);
   previous=bend;
  }
  assert.ok(fanLeafBend(6,count,86)>35 && fanLeafBend(6,count,86)<55);
  assert.ok(fanLeafBend(13,count,86)>fanLeafBend(6,count,86));
  assert.ok(fanLeafBend(13,count,86)<80);
 }
});
test('arc bends left by default, and right at the left monitor edge',()=>{
 assert.equal(fanCurveDirection(370,690,86),-1);
 assert.equal(fanCurveDirection(18,690,86),1);
});
test('labels flip left when the source is near the right edge',()=>{
 assert.equal(fanLabelsLeft(340,690,142,86),false);
 assert.equal(fanLabelsLeft(652,690,142,86),true);
});
test('tilt is readable but visibly curved like old Cairo-Dock',()=>{
 for(let i=0;i<30;i++){
  assert.ok(fanIconTilt(i,100)>=24 && fanIconTilt(i,100)<=29);
  assert.ok(fanLabelTilt(i,100)>=20 && fanLabelTilt(i,100)<=24);
  assert.ok(fanIconTilt(i,100)>fanLabelTilt(i,100));
 }
});
test('leaf zero is positioned directly above its base',()=>{
 assert.equal(fanLeafBend(0,20,86),0);
 assert.equal(fanLeafDistance(0),47);
});
test('subsequent rows increase in vertical distance',()=>{
 for(let i=1;i<30;i++) assert.equal(fanLeafDistance(i)-fanLeafDistance(i-1),44);
});
test('visible capacity depends on height, never stack item threshold',()=>{
 const small=fanVisibleCapacity(430,8),medium=fanVisibleCapacity(650,8),big=fanVisibleCapacity(950,8);
 assert.ok(small>=6 && medium>small && big>medium);
 assert.ok(big>=15);
});
test('window virtualizes 10000 entries to screen-sized number of delegates',()=>{
 const count=10000,cap=fanVisibleCapacity(880,8);
 const w=fanScrollWindow(count,cap,0);
 assert.equal(w.start,0);
 assert.ok(w.shown<30);
 assert.equal(w.shown,cap);
 assert.equal(w.maxStart,count-cap);
});
test('scroll reaches final element and can navigate backward',()=>{
 const first=fanScrollWindow(100,13,0);
 const middle=fanScrollWindow(100,13,54);
 const last=fanScrollWindow(100,13,5000);
 assert.equal(first.start,0);
 assert.equal(middle.start,54);
 assert.equal(last.start+last.shown,100);
 assert.equal(last.maxStart,87);
});
test('scroll never goes negative and handles empty stack',()=>{
 assert.equal(fanScrollWindow(15,8,-33).start,0);
 assert.equal(fanScrollWindow(0,8,13).shown,0);
 assert.equal(fanScrollWindow(0,8,13).start,0);
 assert.equal(fanScrollWindow(2,16,13).shown,2);
});
test('smooth fan entrance and exit includes each visible row',()=>{
 for(let i=0;i<17;i++){
  assert.equal(fanStaggerProgress(0,i,17),0);
  assert.equal(fanStaggerProgress(1,i,17),1);
  let prev=0;
  for(let k=0;k<=100;k++){
   const v=fanStaggerProgress(k/100,i,17);
   assert.ok(v>=prev-1e-12 && v<=1);
   prev=v;
  }
 }
});
test('upper entries start later in cascade',()=>{
 assert.ok(fanStaggerProgress(.30,0,15)>0);
 assert.equal(fanStaggerProgress(.30,12,15),0);
});
test('narrow mouse corridor stays in owner column',()=>{
 const b=fanBridgeGeometry(330,410);
 assert.equal(b.x+b.width/2,330);
 assert.ok(b.width<=50 && b.y+b.height>=410);
});

test('all leaves emerge from the SAME base icon, then unfold on a short arc',()=>{
 for(const count of [2,7,8,100]) {
  for(let i=0;i<Math.min(16,count);i++){
   const collapsed=fanLeafPosition(i,count,340,450,-1,86,100,false,0);
   const expanded=fanLeafPosition(i,count,340,450,-1,86,100,false,1);
   assert.equal(collapsed.iconCenterX,340);
   assert.equal(collapsed.iconCenterY,450);
   assert.ok(expanded.iconCenterY<450);
   assert.ok(expanded.iconCenterX<=340 && expanded.iconCenterX>=254);
  }
 }
});
test('compact Cairo leaf geometry physically joins each label to its icon',()=>{
 for(const labelsLeft of [false,true]){
  for(let i=0;i<8;i++){
   const p=fanLeafPosition(i,8,345,510,-1,86,92,labelsLeft,1);
   const pivot=p.labelPivotX;
   assert.equal(p.width,34+3+92);
   assert.ok(Math.abs(pivot-p.iconCenterX)-(34/2+3)<1e-7);
   if(labelsLeft) assert.equal(p.labelX,0);
   else assert.equal(p.labelX,37);
   const bounds=fanLabelBounds(p.iconCenterX,p.iconCenterY,labelsLeft,92,fanLabelTilt(i,8),34,3,21);
   assert.ok(bounds.width>0 && bounds.height>0);
   assert.ok(bounds.y<p.iconCenterY-15); // inclined label rises toward its free tip
  }
 }
});
test('curve never creates the 185px horizontal spread seen in beta1',()=>{
 const p0=fanLeafPosition(0,100,350,830,-1,86,100,false,1);
 const p12=fanLeafPosition(12,100,350,830,-1,86,100,false,1);
 assert.ok(p0.iconCenterX-p12.iconCenterX<72);
 assert.ok(p0.iconCenterX-p12.iconCenterX>60);
});
test('last visible label has a safe margin above the viewport',()=>{
 for(const y of [430,650,880,1080]){
  const cap=fanVisibleCapacity(y,8);
  const last=fanLeafPosition(cap-1,cap,345,y,-1,86,142,false,1);
  const bounds=fanLabelBounds(last.iconCenterX,last.iconCenterY,false,142,24,34,3,21);
  assert.ok(bounds.y>=8-1e-7,`top clipped for y=${y}, cap=${cap}, got=${bounds.y}`);
 }
});
const fan=fs.readFileSync(path.join(root,'qml/FanStack.qml'),'utf8');
const main=fs.readFileSync(path.join(root,'qml/Main.qml'),'utf8');
const cell=fs.readFileSync(path.join(root,'qml/DockCell.qml'),'utf8');
test('no grid layout, no 7-entry threshold, no 32-entry truncation',()=>{
 assert.doesNotMatch(fan,/gridMode|gridPlate|Grid\s*\{|id:\s*gridHover/);
 assert.doesNotMatch(fan,/entries\.length\s*>\s*7/);
 assert.doesNotMatch(main,/entries\.slice\(0,\s*32\)/);
 assert.doesNotMatch(cell,/Math\.min\(32,\s*countWindows\)/);
});
test('visible screen-sized window and wheel navigation are wired',()=>{
 assert.match(fan,/model: fan\.visibleRows/);
 assert.match(fan,/fan\.scrollWindow\.start \+ index/);
 assert.match(fan,/function scrollBy\(delta\)/);
 assert.match(fan,/onWheel: wheel =>/);
 assert.match(fan,/onScrollRowsChanged: inputChanged\(\)/);
});
test('fan opens in existing Wayland window from the exact icon position',()=>{
 assert.match(main,/anchorX: dock\.liveFanAnchorX - x/);
 assert.match(main,/Logic\.fanPlacement\(dock\.liveFanAnchorX, dock\.width, width\)\.x/);
 assert.match(fan,/Logic\.fanLeafPosition/);
 assert.match(fan,/Logic\.fanLabelBounds/);
});
test('labels are tilted dark plaques, icon and text arc animated',()=>{
 assert.match(fan,/Logic\.fanIconTilt/);
 assert.match(fan,/Logic\.fanLabelTilt/);
 assert.match(fan,/Logic\.fanStaggerProgress/);
 assert.match(fan,/Text\.QtRendering/);
 assert.match(fan,/font\.weight: Font\.DemiBold/);
 assert.match(fan,/elide: Text\.ElideRight/);
 assert.match(fan,/"#ed05060a"/);
 assert.match(fan,/transformOrigin: fan\.labelsLeft \? Item\.Right : Item\.Left/);
});
test('only actual leaves and short bridge enter Wayland input mask',()=>{
 assert.match(fan,/function inputRegions\(\)/);
 assert.match(fan,/id: leafViewport/);
 assert.match(fan,/id: iconHit/);
 assert.match(fan,/id: labelHit/);
 assert.match(fan,/clip: true/);
 assert.match(main,/fanPanel\.inputRegions\(\)/);
 assert.doesNotMatch(fan,/Window\s*\{|Popup\.Window/);
});
test('visible leaves accept hover before the cascade finishes',()=>{
 assert.match(fan,/enabled: fan.open/);
 assert.doesNotMatch(fan,/enabled: fan.open && leaf.progress/);
});
test('FanStack cannot override QtQuick Item.data',()=>{
 assert.doesNotMatch(fan,/\b(?:readonly\s+)?property\s+var\s+data\s*:/);
 assert.equal((fan.match(/readonly property var fanEntry:/g)||[]).length,1);
});
console.log(`${n} testes PASS (leque Cairo-Dock ilimitado por quantidade).`);
