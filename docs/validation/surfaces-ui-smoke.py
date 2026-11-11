# SPDX-License-Identifier: GPL-2.0-or-later
# Run in a disposable Linux GTK/X11 test environment, see SURFACES-VALIDATION.md.
import os, subprocess, time, gzip, tempfile, xml.etree.ElementTree as ET, pyatspi
from pathlib import Path
root=Path(tempfile.mkdtemp(prefix='foliq-surfaces-ui-')); print('Synthetic test directory:',root,flush=True)
path=root/'free.xopp'
path.write_text('''<?xml version="1.0"?><xournal creator="Foliq" fileversion="4"><page width="595.276" height="841.89" foliq-surface-version="1" foliq-camera-x="-500" foliq-camera-y="-600" foliq-export-content="0" foliq-crop-x="-500" foliq-crop-y="-600" foliq-crop-width="700" foliq-crop-height="700" foliq-tiled="1" foliq-tile-width="350" foliq-tile-height="350"><background type="solid" color="#ffffffff" style="graph" config="r1=20"/><layer/></page></xournal>''')
env=os.environ.copy();env.update(XDG_CONFIG_HOME=str(root/'config'),XDG_CACHE_HOME=str(root/'cache'),XDG_DATA_HOME=str(root/'data'),FOLIQ_LIBRARY=str(root/'library'),GTK_MODULES='gail:atk-bridge',LANG='C.UTF-8')
log=open(root/'app.log','w')
def start():
 p=subprocess.Popen(['/src/build-codex/xournalpp',str(path)],cwd='/src',env=env,stdout=log,stderr=log); time.sleep(3); return p
def nodes(n=None):
 if n is None: n=pyatspi.Registry.getDesktop(0)
 yield n
 try:
  for c in n: yield from nodes(c)
 except: pass
def find(name=None, role=None):
 deadline=time.monotonic()+6
 while time.monotonic()<deadline:
  for n in nodes():
   try:
    if (name is None or n.name==name) and (role is None or n.getRoleName()==role) and n.getState().contains(pyatspi.STATE_SHOWING): return n
   except: pass
  time.sleep(.15)
 raise AssertionError(('Missing widget',name,role))
def xdo(*args): subprocess.run(['xdotool',*map(str,args)],check=True)
def click(name,role=None):
 n=find(name,role); r=n.queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
 print('CLICK',name,n.getRoleName(),r,flush=True)
 xdo('mousemove',r.x+r.width//2,r.y+r.height//2,'click',1); time.sleep(.35)
def xml():
 data=path.read_bytes(); return ET.fromstring(gzip.decompress(data) if data[:2]==b'\x1f\x8b' else data)
def save(): xdo('key','ctrl+s'); time.sleep(.5); return xml()
def strokes(page): return page.findall('.//stroke')
def draw(x,y,dx=60,dy=40): xdo('mousemove',x,y,'mousedown',1,'sleep',.08,'mousemove',x+dx,y+dy,'sleep',.1,'mouseup',1); time.sleep(.3)
def spin(name,value):
 n=find(name,'spin button'); n.queryValue().currentValue=value; time.sleep(.1)
def settings():
 click('Einfügen','page tab');click('Seitentyp / Papier','push button');find('Page surface and PDF crop','dialog')
def choose(name,index):
 n=find(name,'combo box');r=n.queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
 xdo('mousemove',r.x+r.width//2,r.y+r.height//2,'click',1,'key','Home')
 if index: xdo('key','--repeat',index,'Down')
 xdo('key','Return');time.sleep(.2)
def shot(name):
 import gi;gi.require_version('Gdk','3.0');from gi.repository import Gdk
 w=Gdk.get_default_root_window();Gdk.pixbuf_get_from_window(w,0,0,w.get_width(),w.get_height()).savev('/src/docs/validation/'+name,'png',[],[])
p=start()
try:
 click('Zeichnen','page tab');click('Stift');draw(650,350)
 doc=save();page=doc.find('page');assert len(strokes(page))==1
 points=list(map(float,strokes(page)[0].text.split())); assert points[0]<0 and points[1]<0,points
 print('PASS GTK negative-coordinate pen input and save',flush=True)
 click('Rückgängig');assert len(strokes(save().find('page')))==0
 click('Wiederholen');assert len(strokes(save().find('page')))==1
 print('PASS GTK stroke undo/redo',flush=True)
 # Lasso and move use the same negative world coordinates as drawing.
 click('Lasso','toggle button')
 xdo('mousemove',630,330,'mousedown',1,'sleep',.08)
 for px,py in [(735,330),(735,415),(630,415),(630,330)]:
  xdo('mousemove',px,py,'sleep',.06)
 xdo('mouseup',1);time.sleep(.4)
 draw(680,370,40,20)
 moved=strokes(save().find('page'))[0].text
 assert moved!=strokes(doc.find('page'))[0].text
 click('Rückgängig');assert strokes(save().find('page'))[0].text==strokes(doc.find('page'))[0].text
 click('Wiederholen');assert strokes(save().find('page'))[0].text==moved
 print('PASS GTK lasso move and undo/redo at negative coordinates',flush=True)
 # Saving finalizes selection, so select it again before dragging the scale handle.
 xdo('mousemove',670,350,'mousedown',1,'sleep',.08)
 for px,py in [(775,350),(775,435),(670,435),(670,350)]:
  xdo('mousemove',px,py,'sleep',.06)
 xdo('mouseup',1);time.sleep(.4)
 shot('surfaces-selection.png')
 draw(769,429,35,25)
 scaled=strokes(save().find('page'))[0].text
 assert scaled!=moved,(scaled,moved)
 click('Rückgängig');assert strokes(save().find('page'))[0].text==moved
 click('Wiederholen');assert strokes(save().find('page'))[0].text==scaled
 print('PASS GTK selection scaling and undo/redo',flush=True)
 click('Textmarker','toggle button');draw(850,350,60,40)
 markedPage=save().find('page');assert strokes(markedPage)[-1].attrib['tool']=='highlighter'
 marked=[ET.tostring(s) for s in strokes(markedPage)]
 click('Radierer','toggle button');draw(865,350,30,30)
 erased=[ET.tostring(s) for s in strokes(save().find('page'))];assert erased!=marked
 click('Rückgängig');assert [ET.tostring(s) for s in strokes(save().find('page'))]==marked
 click('Wiederholen');assert [ET.tostring(s) for s in strokes(save().find('page'))]==erased
 click('Rückgängig')
 print('PASS GTK highlighter and eraser with undo/redo',flush=True)


 click('Hand','toggle button');draw(650,350,120,80);page=save().find('page');cx=float(page.attrib['foliq-camera-x']);cy=float(page.attrib['foliq-camera-y'])
 assert cx < -500 and cy < -600,(cx,cy)
 click('Rückgängig');page=save().find('page');assert float(page.attrib['foliq-camera-x'])==-500
 click('Wiederholen');page=save().find('page');assert float(page.attrib['foliq-camera-x'])==cx
 print('PASS GTK Hand panning into negative space and undo/redo',flush=True)
 # Pointer anchor must stay at the same world point after real Ctrl+wheel zoom.
 click('Stift');draw(850,450,2,2);page=save().find('page');anchor=list(map(float,strokes(page)[-1].text.split()))[:2]
 beforeZoomCamera=(page.attrib['foliq-camera-x'],page.attrib['foliq-camera-y'])
 xdo('mousemove',850,450,'keydown','ctrl','click',4,'keyup','ctrl');time.sleep(.5)
 zpage=save().find('page');zoomedCamera=(zpage.attrib['foliq-camera-x'],zpage.attrib['foliq-camera-y'])
 click('Rückgängig');undoZoom=save().find('page');assert (undoZoom.attrib['foliq-camera-x'],undoZoom.attrib['foliq-camera-y'])==beforeZoomCamera
 click('Wiederholen');redoZoom=save().find('page');assert (redoZoom.attrib['foliq-camera-x'],redoZoom.attrib['foliq-camera-y'])==zoomedCamera
 draw(850,450,2,2);page=save().find('page');after=list(map(float,strokes(page)[-1].text.split()))[:2]
 assert max(abs(a-b) for a,b in zip(anchor,after))<2,(anchor,after)
 print('PASS GTK real zoom keeps pointer world anchor',flush=True)
 shot('surfaces-free.png')
 original=[s.text for s in strokes(page)];originalCamera=(page.attrib['foliq-camera-x'],page.attrib['foliq-camera-y'])
 settings();shot('surfaces-dialog.png');choose('Page type',0);spin('Width (pt)',300);spin('Height (pt)',200);click('Apply')
 doc=save();assert len(doc.findall('page'))==2
 first,second=doc.findall('page');assert [s.text for s in strokes(first)]==original
 assert 'foliq-surface-version' not in second.attrib
 assert len(strokes(second))==len(original)
 firstPoint=list(map(float,strokes(first)[0].text.split()));secondPoint=list(map(float,strokes(second)[0].text.split()))
 assert abs(secondPoint[0]-(firstPoint[0]-float(originalCamera[0])))<.01
 click('Rückgängig');assert len(save().findall('page'))==1
 click('Wiederholen');assert len(save().findall('page'))==2
 print('PASS GTK conversion preserves original, translates copy, undo/redo',flush=True)
 settings();spin('Width (pt)',400);spin('Height (pt)',250);click('Apply');assert float(save().findall('page')[1].attrib['width'])==400
 click('Rückgängig');assert float(save().findall('page')[1].attrib['width'])==300
 click('Wiederholen');assert float(save().findall('page')[1].attrib['width'])==400
 print('PASS GTK paper settings undo/redo',flush=True)
 xdo('key','ctrl+q');p.wait(timeout=8);assert p.returncode==0
 p=start();doc=save();assert len(doc.findall('page'))==2
 assert [s.text for s in strokes(doc.find('page'))]==original
 assert doc.find('page').attrib['foliq-camera-x']==originalCamera[0]
 print('PASS GTK restart preserves page modes, camera and negative content',flush=True)
 xdo('key','ctrl+q');p.wait(timeout=8);assert p.returncode==0
except:
 shot('surfaces-ui-failure.png');raise
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=5)
