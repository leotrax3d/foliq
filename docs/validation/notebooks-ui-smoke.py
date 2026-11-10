# SPDX-License-Identifier: GPL-2.0-or-later
# Linux GTK regression evidence; run in the documented disposable Docker environment.
# Uses only the synthetic /tmp/foliq-ui-smoke library. See NOTEBOOK-VALIDATION.md.
import os, subprocess, time, pyatspi, configparser, gzip, signal
from pathlib import Path
root=Path('/tmp/foliq-ui-smoke');root.mkdir(exist_ok=True)
env=os.environ.copy();env.update(XDG_CONFIG_HOME=str(root/'config'),XDG_CACHE_HOME=str(root/'cache'),XDG_DATA_HOME=str(root/'data'),FOLIQ_LIBRARY=str(root/'library'),GTK_MODULES='gail:atk-bridge')
log=open(str(root/'app.log'),'w')
def start():
 p=subprocess.Popen(['/tmp/build-foliq/xournalpp'],cwd='/src',env=env,stdout=log,stderr=log);time.sleep(3);return p
def allnodes(n=None):
 if n is None: n=pyatspi.Registry.getDesktop(0)
 yield n
 try:
  for c in n:
   yield from allnodes(c)
 except Exception: pass
def find(name=None,role=None):
 deadline=time.monotonic()+8
 while time.monotonic()<deadline:
  for n in allnodes():
   try:
    if (name is None or n.name==name) and (role is None or n.getRoleName()==role) and n.getState().contains(pyatspi.STATE_SHOWING): return n
   except Exception: pass
  time.sleep(.2)
 print([(n.getRoleName(),n.name) for n in allnodes()],flush=True);raise AssertionError(('UI element missing',name,role))
def click(name):
 n=find(name,'push button');r=n.queryComponent().getExtents(pyatspi.DESKTOP_COORDS);print('CLICK',name,r,flush=True);xdo('mousemove',r.x+r.width//2,r.y+r.height//2,'click',1);time.sleep(.5)
def xdo(*args): subprocess.run(['xdotool',*map(str,args)],check=True)
def select(name,double=False):
 n=find(name,'table cell');r=n.queryComponent().getExtents(pyatspi.DESKTOP_COORDS);xdo('mousemove',r.x+max(20,r.width//2),r.y+r.height//2,'click','--repeat',2 if double else 1,'--delay',100,1);time.sleep(.4)
def create(button,name):
 click(button);n=find(role='text');n.queryEditableText().setTextContents(name);click('OK')
def catalog():
 c=configparser.ConfigParser(interpolation=None);c.read(root/'library/catalog.ini');return c
def idnamed(name):
 c=catalog();return next(s for s in c.sections() if s!='catalog' and c[s]['name']==name)
def screenshot():
 import gi;gi.require_version('Gdk','3.0');from gi.repository import Gdk
 w=Gdk.get_default_root_window();Gdk.pixbuf_get_from_window(w,0,0,w.get_width(),w.get_height()).savev('/src/docs/validation/notebooks-navigation.png','png',[],[])
p=start()
try:
 create('Notizbuch','UI-Buch');select('UI-Buch');create('Gruppe','UI-Gruppe');select('UI-Gruppe');create('Abschnitt','UI-Abschnitt');select('UI-Abschnitt');create('Seite','UI-Seite');select('UI-Seite',True)
 page=idnamed('UI-Seite');path=root/'library/documents'/f'{page}.xopp';assert path.exists()
 # Make a real pen stroke in the existing editor and wait for autosave.
 xdo('mousemove',650,300,'mousedown',1,'mousemove',700,350,'mouseup',1);time.sleep(7)
 xml=gzip.decompress(path.read_bytes());assert b'<stroke' in xml,xml[:300]
 print('PASS: hierarchy UI, real XOPP open, pen input and autosave',flush=True)
 select('UI-Seite');click('Duplizieren');assert idnamed('UI-Seite (Kopie)')!=page
 click('Löschen');assert catalog()[page]['trash']=='1'
 click('Papierkorb');select('UI-Seite [Papierkorb]');click('Wiederherstellen');assert catalog()[page]['trash']=='0'
 screenshot()
 print('PASS: duplicate, trash and restore through GTK',flush=True)
 # Simulate a real filesystem backup failure; original bytes must remain intact.
 backup=Path(str(path)+'.bak');backup.unlink(missing_ok=True);backup.mkdir();before=path.read_bytes()
 select('UI-Seite',True);xdo('mousemove',660,320,'mousedown',1,'mousemove',710,370,'mouseup',1);time.sleep(7)
 find('Error','alert');assert path.read_bytes()==before;click('OK');backup.rmdir();xdo('key','ctrl+s');time.sleep(2)
 assert path.read_bytes()!=before
 print('PASS: visible save failure keeps original; retry persists edit',flush=True)
 # Graceful quit and restart must restore the same document.
 xdo('key','ctrl+q');p.wait(timeout=10);p=start()
 windows=[n.name for n in allnodes() if n.getRoleName()=='frame'];assert any(page in n for n in windows),windows
 assert b'<stroke' in gzip.decompress(path.read_bytes())
 print('PASS: restart restores managed document and saved strokes',flush=True)
 screenshot();xdo('key','ctrl+q');p.wait(timeout=10)
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=10)
