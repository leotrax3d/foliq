# SPDX-License-Identifier: GPL-2.0-or-later
# Isolated Linux GTK smoke test; run from the repository root under Xvfb + dbus-run-session.
# Updated for the Fluent shell (2026-11-11): ⋯ menu, segmented theme, Fokusmodus.
import os, subprocess, time, configparser, gzip
from pathlib import Path
import pyatspi
REPO=os.getcwd()
BUILD=os.environ.get('FOLIQ_BUILD',REPO+'/build-codex')
root=Path('/tmp/foliq-main-ui');root.mkdir(exist_ok=True)
env=os.environ.copy();env.update(XDG_CONFIG_HOME=str(root/'config'),XDG_CACHE_HOME=str(root/'cache'),XDG_DATA_HOME=str(root/'data'),FOLIQ_LIBRARY=str(root/'library'),GTK_MODULES='gail:atk-bridge')
log=open(root/'app.log','w')
def xdo(*args): subprocess.run(['xdotool',*map(str,args)],check=True)
def nodes(n=None):
 if n is None:n=pyatspi.Registry.getDesktop(0)
 yield n
 try:
  for c in n:yield from nodes(c)
 except Exception:pass
def find(name=None,role=None):
 for _ in range(40):
  for n in nodes():
   try:
    if (name is None or n.name==name) and (role is None or n.getRoleName()==role) and n.getState().contains(pyatspi.STATE_SHOWING):return n
   except Exception:pass
  time.sleep(.15)
 raise AssertionError(('missing',name,role,[(n.getRoleName(),n.name) for n in nodes()]))
def click(name,role='push button',double=False):
 n=find(name,role);r=n.queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
 scale=int(env.get('GDK_SCALE','1'))
 xdo('mousemove',(r.x+r.width//2)*scale,(r.y+r.height//2)*scale,'click','--repeat',2 if double else 1,'--delay',100,1);time.sleep(.25)
def menu(action,pages=False):
 click('Seitenaktionen' if pages else 'Notizbuchaktionen');click(action,'menu item')
def drag(source,target):
 a=find(source,'table cell').queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
 b=find(target,'table cell').queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
 xdo('mousemove',a.x+a.width//2,a.y+a.height//2,'mousedown',1)
 xdo('mousemove',a.x+a.width//2+15,a.y+a.height//2);time.sleep(.4)
 xdo('mousemove',b.x+b.width//2,b.y+b.height//2);time.sleep(.5);xdo('mouseup',1);time.sleep(.7)
def create(kind,name,pages=False):
 menu(kind,pages);find(role='text').queryEditableText().setTextContents(name);click('OK')
def cat():
 c=configparser.ConfigParser(interpolation=None);c.read(root/'library/catalog.ini');return c
def named(name):return next(s for s in cat().sections() if s!='catalog' and cat()[s]['name']==name)
def shot(name):
 import gi;gi.require_version('Gdk','3.0');from gi.repository import Gdk
 w=Gdk.get_default_root_window();r=find(role='frame').queryComponent().getExtents(pyatspi.DESKTOP_COORDS);Gdk.pixbuf_get_from_window(w,r.x*int(env.get('GDK_SCALE','1')),r.y*int(env.get('GDK_SCALE','1')),r.width*int(env.get('GDK_SCALE','1')),r.height*int(env.get('GDK_SCALE','1'))).savev(REPO+'/docs/validation/fluent/smoke-'+name+'.png','png',[],[])
def windowid():
 r=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(p.pid),'--name','Foliq'],capture_output=True,text=True)
 return r.stdout.splitlines()[-1] if r.returncode==0 else None
def quitapp():
 xdo('key','ctrl+q');p.wait(timeout=15)
def start():
 proc=subprocess.Popen([BUILD+'/xournalpp'],cwd=REPO,env=env,stdout=log,stderr=log);time.sleep(3)
 r=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(proc.pid),'--name','Foliq'],capture_output=True,text=True)
 if r.returncode==0:
  wid=r.stdout.splitlines()[-1];xdo('windowsize',wid,2400 if env.get('GDK_SCALE')=='2' else 1200,1600 if env.get('GDK_SCALE')=='2' else 800);xdo('windowfocus','--sync',wid)
 time.sleep(.5);return proc

p=start()
try:
 create('Notizbuch','UI-Notizbuch');click('UI-Notizbuch','table cell');xdo('key','Right');time.sleep(.2)
 create('Abschnitt','Gedanken');click('Gedanken','table cell')
 create('Seite','Eine lange Notiz über Ideen, Entwürfe und nächste Schritte',True)
 title='Eine lange Notiz über Ideen, Entwürfe und nächste Schritte';click(title,'table cell')
 create('Seite','Unterseite',True);click(title,'table cell');xdo('key','Right');time.sleep(.2)
 click('Unterseite','table cell',True);page=named('Unterseite');path=root/'library/documents'/f'{page}.xopp'
 # Real editor input, persistence and undo/redo through new buttons.
 click('Zeichnen','page tab');click('Radierer','toggle button');click('Stift','toggle button')
 xdo('mousemove',850,470,'mousedown',1,'mousemove',920,510,'mouseup',1);time.sleep(.5)
 click('Rückgängig');click('Wiederholen');time.sleep(6)
 assert b'<stroke' in gzip.decompress(path.read_bytes())
 print('PASS editor stroke, ribbon undo/redo, autosave',flush=True)
 click('Unterseite','table cell');xdo('key','F2');find(role='text').queryEditableText().setTextContents('Details');click('OK');assert named('Details')==page
 click(title,'table cell');xdo('key','Right');time.sleep(.3)
 drag('Details','Gedanken');assert cat()[page]['parent']==named('Gedanken')
 create('Notizbuch','Zweites Buch');drag('Gedanken','Zweites Buch')
 assert cat()[named('Gedanken')]['parent']==named('Zweites Buch')
 click('Gedanken','table cell')
 print('PASS real page and section drag-and-drop',flush=True)
 click('Details','table cell');xdo('key','alt+Up');time.sleep(.3);assert cat()[page]['order']=='0'
 xdo('key','alt+Down');time.sleep(.3);assert cat()[page]['order']=='1'
 menu('Verschieben',True);combo=find(role='combo box');combo.queryAction().doAction(0);time.sleep(.2)
 xdo('key','Home')
 targets=sorted([named('Gedanken'),named(title)])
 if targets.index(named('Gedanken')):xdo('key','Down')
 xdo('key','Return');click('Verschieben');assert cat()[page]['parent']==named('Gedanken')
 print('PASS keyboard ordering and move menu',flush=True)
 click('Suchen');find('Search','text');xdo('key','Escape');time.sleep(.3)
 shot('main-ui-light')
 click('Ansicht','page tab');click('Darstellung: Design, Eingabe, Akzentfarbe, Bewegung','toggle button');click('Dunkel','radio button');xdo('key','Escape');time.sleep(1);shot('main-ui-dark')
 # Both native pane separators must really resize and survive restart.
 xdo('mousemove',200,500,'mousedown',1,'mousemove',170,500,'mouseup',1);time.sleep(.3)
 xdo('mousemove',420,500,'mousedown',1,'mousemove',360,500,'mouseup',1);time.sleep(.3)
 click('Fokusmodus','toggle button');time.sleep(.5);shot('main-ui-focus')
 quitapp()
 v=configparser.ConfigParser();v.read(root/'library/view.ini');assert v['view']['focus']=='1' and v['view']['theme']=='2'
 assert 120<=int(v['view']['books'])<190 and abs(int(v['view']['width'])-360)<3,dict(v['view'])
 p=start();click('Ansicht','page tab');assert find('Fokusmodus','toggle button').getState().contains(pyatspi.STATE_CHECKED)
 click('Fokusmodus','toggle button');wid=subprocess.check_output(['xdotool','search','--onlyvisible','--pid',str(p.pid),'--name','Foliq']).decode().splitlines()[-1];xdo('windowsize',wid,800,600);time.sleep(1);shot('main-ui-small');r=find(role='frame').queryComponent().getExtents(pyatspi.DESKTOP_COORDS);assert (r.width,r.height)==(800,600),(r.width,r.height)
 print('PASS theme/focus/view restart, long title and 800x600 capture',flush=True)
 quitapp();assert p.returncode==0
 # Native GTK 200% scaling under a separate launch.
 env['GDK_SCALE']='2';p=start();time.sleep(1);shot('main-ui-scale200')
 find('Notizbücher und Abschnitte');click('Ansicht','page tab');click('Darstellung: Design, Eingabe, Akzentfarbe, Bewegung','toggle button');find('Dunkel','radio button');xdo('key','Escape')
 quitapp();assert p.returncode==0
 del env['GDK_SCALE']
 # Unsupported optional view format must never be overwritten.
 view=root/'library/view.ini';original=view.read_bytes();future=original.replace(b'version=2',b'version=99');view.write_bytes(future)
 p=start();find(role='alert');click('OK');quitapp()
 assert view.read_bytes()==future;view.write_bytes(original)
 print('PASS 200% GTK launch and unknown view version preservation',flush=True)
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=10)
