// SPDX-License-Identifier: MIT
// Regression tests for the exact JS library loaded by Qt QML.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const js = fs.readFileSync(path.join(__dirname, '../qml/DockLogic.js'), 'utf8');
const ctx = vm.createContext({ Math, Number, String, Array });
vm.runInContext(js, ctx, { filename: 'DockLogic.js' });
const { cleanUrl, zoomAt, reorderLaunchers, chooseDropTarget, totalZoomExtra, visualShift, lastWindowIndex, compactTitle, zoomLayout, maximumZoomExtra, hoverBetweenCenters, dividerShift, rectanglesOverlap, reorderOrPinLauncher, routeDrop } = ctx;
let cases = 0;
function check(label, fn) { fn(); ++cases; console.log('PASS', label); }
function eqArray(actual, expected) { assert.deepEqual(Array.from(actual), expected); }
check('zoom sem hover', () => assert.equal(zoomAt(2,-1,1.65,2),1));
check('zoom no centro', () => assert.equal(zoomAt(2,2,1.65,2),1.65));
check('zoom fora da onda', () => assert.equal(zoomAt(8,2,1.65,2),1));
check('zoom suave no meio', () => assert.ok(zoomAt(2,2.3,1.65,2) > zoomAt(2,2.7,1.65,2)));
check('zoom simétrico', () => assert.ok(Math.abs(zoomAt(1,2,1.7,2)-zoomAt(3,2,1.7,2)) < 1e-10));
check('zoom escalonado é limitado', () => assert.equal(zoomAt(0,0,8,1),3));
check('zoom com zero vizinhos', () => assert.equal(zoomAt(1,0,2,0),1));
check('URLs de ícones equivalentes', () => assert.equal(cleanUrl('applications:foo.desktop?iconData=1'),'applications:foo.desktop'));
check('URLs de aplicativos normalizam uma barra opcional', () => assert.equal(cleanUrl('applications:///org.kde.systemsettings.desktop'), 'applications:org.kde.systemsettings.desktop'));
check('URLs das aplicações não alteram preferred://', () => assert.equal(cleanUrl('preferred://browser'),'preferred://browser'));

const apps=['applications:a.desktop','applications:b.desktop','applications:c.desktop'];
check('mover o primeiro depois do segundo',()=>eqArray(reorderLaunchers(apps,apps[0],apps[1],true),[apps[1],apps[0],apps[2]]));
check('mover o último antes do primeiro',()=>eqArray(reorderLaunchers(apps,apps[2],apps[0],false),[apps[2],apps[0],apps[1]]));
check('mover o meio para o fim',()=>eqArray(reorderLaunchers(apps,apps[1],apps[2],true),[apps[0],apps[2],apps[1]]));
check('não move item sobre ele mesmo',()=>eqArray(reorderLaunchers(apps,apps[1],apps[1],true),apps));
check('não move programa não fixado',()=>eqArray(reorderLaunchers(apps,'applications:x.desktop',apps[0],true),apps));
check('não move sobre alvo inexistente',()=>eqArray(reorderLaunchers(apps,apps[0],'applications:x.desktop',false),apps));
check('reordenar URL com query mantém forma original',()=>eqArray(reorderLaunchers([apps[0]+'?i=x',apps[1],apps[2]],apps[0],apps[2],true),[apps[1],apps[2],apps[0]+'?i=x']));
const c=[{url:'a',left:0,right:30},{url:'b',left:30,right:60},{url:'c',left:60,right:90}];
check('drop antes de B',()=>assert.equal(chooseDropTarget(c,34).url,'b'));
check('drop próximo do início',()=>assert.equal(chooseDropTarget(c,-25).after,false));
check('drop após o último',()=>{const d=chooseDropTarget(c,180);assert.equal(d.url,'c');assert.equal(d.after,true)});
check('drop com lista vazia é seguro',()=>assert.equal(chooseDropTarget([],25),null));
check('drop com NaN é seguro',()=>assert.equal(chooseDropTarget(c,NaN),null));
check('sem expansão, sem deslocamento',()=>assert.equal(visualShift(1,3,-1,1.65,2,52),0));
check('sem zoom, região extra zero',()=>assert.equal(totalZoomExtra(4,-1,1.65,2,52),0));
check('tamanho total expandido positivo',()=>assert.ok(totalZoomExtra(5,2,1.65,2,52)>0));
check('deslocamentos simétricos',()=>assert.ok(Math.abs(visualShift(0,5,2,1.65,2,52)+visualShift(4,5,2,1.65,2,52))<1e-8));
check('ícone central não sai do eixo',()=>assert.ok(Math.abs(visualShift(2,5,2,1.65,2,52))<1e-8));
check('deslocamento varia continuamente',()=>assert.notEqual(visualShift(0,5,2.1,1.65,2,52),visualShift(0,5,2.3,1.65,2,52)));
check('sem itens retorna expansão zero',()=>assert.equal(totalZoomExtra(0,0,1.65,2,52),0));
check('deslocamento inválido é zero',()=>assert.equal(visualShift(-1,5,2,1.65,2,52),0));
check('última janela ativa tem preferência',()=>assert.equal(lastWindowIndex([{lastUsed:10},{lastUsed:3,active:true}]),1));
check('janela recente não minimizada',()=>assert.equal(lastWindowIndex([{lastUsed:22,minimized:true},{lastUsed:9,minimized:false},{lastUsed:11,minimized:false}]),2));
check('grupo com todas minimizadas',()=>assert.equal(lastWindowIndex([{lastUsed:11,minimized:true},{lastUsed:17,minimized:true}]),1));
check('grupo sem timestamp escolhe fallback',()=>assert.equal(lastWindowIndex([{minimized:true},{minimized:true}]),0));
check('grupo vazio não ativa pai',()=>assert.equal(lastWindowIndex([]),-1));
check('título compacto',()=>assert.equal(compactTitle('  Título    com  espaços   ',48),'Título com espaços'));
check('título longo truncado',()=>assert.equal(compactTitle('a'.repeat(100),15),'a'.repeat(14)+'…'));
check('layout único equivale ao algoritmo anterior', () => {
  for (const h of [-1,0,0.2,2.55,4,7]) {
    const layout = zoomLayout(8,h,1.8,3,48);
    for (let i=0;i<8;++i) {
      assert.ok(Math.abs(layout.scales[i] - zoomAt(i,h,1.8,3)) < 1e-9);
      assert.ok(Math.abs(layout.shifts[i] - visualShift(i,8,h,1.8,3,48)) < 1e-9);
    }
  }
});
check('ampliação sempre dentro da máscara estática', () => {
  for (const count of [1,2,4,8,30]) {
    for (let h=0;h<count;h+=0.25)
      assert.ok(zoomLayout(count,h,1.9,2,52).extra <= maximumZoomExtra(count,1.9,2,52)+0.001);
  }
});
check('hover atravessa divisória sem salto', () => {
  const centers=[25,82,139,218,275];
  assert.equal(hoverBetweenCenters(centers,139,62),2);
  assert.equal(hoverBetweenCenters(centers,218,62),3);
  assert.ok(Math.abs(hoverBetweenCenters(centers,178.5,62)-2.5)<1e-9);
  assert.ok(Math.abs(hoverBetweenCenters(centers,177.5,62)-2.4873417721518987)<1e-8);
});
check('hover sai da área com segurança', () => {
  assert.equal(hoverBetweenCenters([25,82,139],-50,62),-1);
  assert.equal(hoverBetweenCenters([25,82,139],500,62),-1);
  assert.equal(hoverBetweenCenters([],20,62),-1);
  assert.equal(hoverBetweenCenters([10],NaN,62),-1);
});
check('zoom não depende de FPS', () => {
  const early = zoomLayout(5,0.5,1.65,2,52);
  const late = zoomLayout(5,0.5,1.65,2,52);
  assert.deepEqual(Array.from(early.scales), Array.from(late.scales));
});
check('divisória não desloca quando não há ampliação', () => assert.equal(dividerShift([1,1,1,1],2,52),0));
check('divisória acompanha a onda na seção esquerda', () => assert.ok(dividerShift([1.5,1.2,1,1],2,52)>0));
check('divisória acompanha a onda na seção direita', () => assert.ok(dividerShift([1,1,1.5,1.2],2,52)<0));
check('divisória no eixo ao ampliar simetricamente', () => assert.ok(Math.abs(dividerShift([1.5,1.2,1.2,1.5],2,52))<1e-9));
check('divisória inativa quando só há uma seção', () => assert.equal(dividerShift([1.2,1.5],2,52),0));
check('divisória é contínua durante o movimento', () => {
  const a=zoomLayout(8,2.1,1.65,2,52), b=zoomLayout(8,2.11,1.65,2,52);
  assert.ok(Math.abs(dividerShift(a.scales,3,52)-dividerShift(b.scales,3,52))<4);
});
check('sobreposição maximizada perto do dock', () => assert.equal(rectanglesOverlap({x:700,y:850,width:500,height:65},{x:0,y:0,width:1920,height:1080}),true));
check('janela distante não encobre dock', () => assert.equal(rectanglesOverlap({x:700,y:850,width:500,height:65},{x:40,y:45,width:600,height:500}),false));
check('janela tangente à área de dock não conta como sobreposição', () => assert.equal(rectanglesOverlap({x:700,y:850,width:500,height:65},{x:600,y:0,width:100,height:850}),false));
check('janela em monitor vizinho não cobre o dock', () => assert.equal(rectanglesOverlap({x:0,y:850,width:500,height:65},{x:1920,y:0,width:1920,height:1080}),false));
check('geometria inválida não causa ocultação', () => assert.equal(rectanglesOverlap({x:0,y:0,width:0,height:10},{x:0,y:0,width:20,height:10}),false));
check('janela em monitor à esquerda, coordenadas negativas', () => assert.equal(rectanglesOverlap({x:-1920,y:900,width:500,height:60},{x:-1800,y:880,width:350,height:200}),true));
check('arrastar app aberto para os fixados', () => eqArray(reorderOrPinLauncher(['a.desktop','b.desktop'],'discord.desktop','a.desktop',true),['a.desktop','discord.desktop','b.desktop']));
check('arrastar app fixado preserva ordenação', () => eqArray(reorderOrPinLauncher(['a.desktop','b.desktop','c.desktop'],'a.desktop','b.desktop',true),['b.desktop','a.desktop','c.desktop']));
check('arrastar app para destino inválido não muda ícones', () => eqArray(reorderOrPinLauncher(['a.desktop','b.desktop'],'new.desktop','zzz.desktop',true),['a.desktop','b.desktop']));
check('não duplica app fixado durante drop', () => eqArray(reorderOrPinLauncher(['a.desktop','b.desktop'],'a.desktop','b.desktop',false),['a.desktop','b.desktop']));
check('drop de fixado não o transforma em app em execução',()=> assert.equal(routeDrop(true,500,250,4,3),'pin'));
check('drop de aberto sobre os fixados fixa o app',()=> assert.equal(routeDrop(false,160,250,4,3),'pin'));
check('drop de aberto sobre os abertos reorganiza',()=> assert.equal(routeDrop(false,310,250,4,3),'reorder-running'));
check('drop com zero apps abertos mantém seção fixada',()=> assert.equal(routeDrop(false,310,250,4,0),'pin'));
check('drop com posição inválida cancela',()=> assert.equal(routeDrop(false,NaN,250,3,4),'cancel'));
check('drop com zero fixados pode criar a primeira entrada',()=> assert.equal(routeDrop(false,20,100,0,4),'pin'));
check('cursor percorre divisa linearmente sem reset',()=> {
  const centers=[50,110,170,250,310];
  assert.ok(hoverBetweenCenters(centers,205,62)>2 && hoverBetweenCenters(centers,205,62)<3);
  assert.ok(hoverBetweenCenters(centers,206,62)>hoverBetweenCenters(centers,205,62));
});
check('guia de favoritos não depende do zoom gráfico',()=> {
  assert.equal(chooseDropTarget(c,34).url,'b');
  assert.equal(chooseDropTarget(c,35).url,'b');
});
// v0.2.5: verify real screen-coordinate auto-hide and hover-stack mode.
const { dockProximityZone, stackPresentation } = ctx;
check('dentro da tela de teste HDMI-A-2, zona de proximidade termina na borda', () => {
  const z=dockProximityZone({x:1920,y:0,width:1920,height:1080},440,74,0,36);
  assert.equal(z.y+z.height,1080);
  assert.equal(z.x+z.width/2,2880);
  assert.ok(z.height>74 && z.width>440);
});
check('monitor à esquerda com coordenadas negativas mantém centro correto', () => {
  const z=dockProximityZone({x:-1920,y:0,width:1920,height:1080},400,74,0,40);
  assert.equal(z.x+z.width/2,-960);
});
check('janela perto da borda entra na região inteligente', () => {
  const zone=dockProximityZone({x:0,y:0,width:1920,height:1080},440,74,0,36);
  assert.equal(rectanglesOverlap(zone,{x:800,y:970,width:320,height:95}),true);
});
check('janela distante não dispara ocultação inteligente', () => {
  const zone=dockProximityZone({x:0,y:0,width:1920,height:1080},440,74,0,36);
  assert.equal(rectanglesOverlap(zone,{x:0,y:0,width:600,height:600}),false);
});
check('tela inválida não cria região enganosa', () => {
  assert.equal(dockProximityZone({x:0,y:0,width:0,height:0},440,74,0,36),null);
});
check('pilha pequena usa leque, não preview',()=> {
 assert.equal(stackPresentation(3),'fan');
 assert.equal(stackPresentation(7),'fan');
});
check('pilha grande continua em leque, sem grade',()=> assert.equal(stackPresentation(8),'fan'));
check('pilha com 100 elementos continua em leque',()=> assert.equal(stackPresentation(100),'fan'));
console.log(`${cases} cenários PASS (DockLogic.js, execução Node).`);

// Regression for the runtime-only Qt failure in KofDock 0.2.5.
check('FanStack não redefine a propriedade nativa Item.data', () => {
  const fan = fs.readFileSync(path.join(__dirname, '../qml/FanStack.qml'), 'utf8');
  assert.equal(/\b(?:readonly\s+)?property\s+var\s+data\s*:/.test(fan), false);
  assert.equal((fan.match(/readonly property var fanEntry:/g) || []).length, 1);
});
