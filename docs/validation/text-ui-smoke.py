# SPDX-License-Identifier: GPL-2.0-or-later
# Real GTK/X11/AT-SPI test. Only synthetic data in a fresh temporary directory.
exec(open('/src/docs/validation/main-ui-smoke.py').read().split('\np=start()\n')[0])
import tempfile, xml.etree.ElementTree as ET, base64
root=Path(tempfile.mkdtemp(prefix='foliq-text-ui-'))
env.update(XDG_CONFIG_HOME=str(root/'config'),XDG_CACHE_HOME=str(root/'cache'),XDG_DATA_HOME=str(root/'data'),FOLIQ_LIBRARY=str(root/'library'))
log=open(root/'app.log','w')
xml='<xournal fileversion="4"><page width="595" height="842"><background type="solid" color="#ffffffff" style="plain"/><layer><text font="Sans" size="16" x="80" y="120" color="#000000ff">Grüße ÄÖÜ ß\nText bearbeiten</text></layer></page></xournal>'
free=os.environ.get('FOLIQ_TEST_FREE')=='1'
textY=330 if free else 312
if free:
 xml=xml.replace('width="595" height="842"','width="595" height="842" foliq-surface-version="1" foliq-camera-x="-500" foliq-camera-y="-600" foliq-export-content="0" foliq-crop-x="-500" foliq-crop-y="-600" foliq-crop-width="700" foliq-crop-height="700" foliq-tiled="1" foliq-tile-width="350" foliq-tile-height="350"').replace('x="80" y="120"','x="-290" y="-465"')
fixture=root/'input.xopp';fixture.write_bytes(gzip.compress(xml.encode()))
def launch():
 proc=subprocess.Popen(['/src/build-codex/xournalpp',str(fixture)],cwd='/src',env=env,stdout=log,stderr=log);time.sleep(3)
 wid=subprocess.check_output(['xdotool','search','--onlyvisible','--pid',str(proc.pid),'--name','Foliq']).decode().splitlines()[-1]
 xdo('windowsize',wid,1280,900);xdo('windowfocus',wid);time.sleep(.5)
 return proc
def body():
 return next(n for n in nodes(find('Text bearbeiten','dialog')) if n.getRoleName()=='text' and n.getState().contains(pyatspi.STATE_EDITABLE))
def txt(n):return n.queryText().getText(0,-1)
def doc():return ET.fromstring(gzip.decompress(fixture.read_bytes()))
def state():
 t=doc().find('.//text');return t,configparser.ConfigParser(interpolation=None)
def save():
 click('Fertig');click('Auswahl','toggle button');click('Speichern');time.sleep(.6)
def clipboard(value):
 code='import gi;gi.require_version("Gtk","3.0");from gi.repository import Gtk,Gdk;Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD).set_text('+repr(value)+',-1);Gtk.main()'
 owner=subprocess.Popen(['python3','-c',code],env=env);time.sleep(.3);return owner
p=launch()
try:
 click('Text','toggle button');xdo('mousemove',680,textY,'click',1);xdo('key','F4');find('Text bearbeiten','dialog')
 text=body();text.queryComponent().grabFocus();xdo('key','ctrl+a')
 expected='Grüße ÄÖÜ äöü ß €\nMehrzeilig\nAbsatz drei'
 owner=clipboard(expected);xdo('key','ctrl+v');time.sleep(.4);owner.terminate();owner.wait(timeout=5)
 assert txt(body())==expected,repr(txt(body()))
 # Real keyboard input and Return, in addition to Unicode clipboard input.
 xdo('key','ctrl+End','Return');xdo('type','--clearmodifiers','--delay',30,'Eingabe 123')
 expected+='\nEingabe 123';assert txt(body())==expected,repr(txt(body()))
 # Ctrl+Shift+U belongs to the GTK input method, not underline formatting.
 xdo('key','Return')
 for char in 'äöüßÄÖÜ':
  xdo('key','ctrl+shift+u');xdo('type','--clearmodifiers',format(ord(char),'x'));xdo('key','Return');time.sleep(.05)
 expected+='\näöüßÄÖÜ';assert txt(body())==expected,repr(txt(body()))
 for label, marker in [('• Liste','• '),('1. Liste','1. '),('☐ Aufgabe','☐ '),('Einrücken','    ')]:
  text.queryComponent().grabFocus();xdo('key','ctrl+a');click(label);assert txt(body()).startswith(marker),repr(txt(body()))
  text.queryComponent().grabFocus();xdo('key','ctrl+z');assert txt(body())==expected,repr(txt(body()))
 text.queryComponent().grabFocus();xdo('key','ctrl+a');click('Überschrift 2');click('Absatz')
 text.queryComponent().grabFocus();xdo('key','ctrl+a');click('Fett');click('Kursiv');click('Unterstrichen')
 # Native clipboard preserves the selected spans and text.
 text.queryComponent().grabFocus();xdo('key','ctrl+a','ctrl+c','ctrl+End','Return','ctrl+v');time.sleep(.4)
 assert txt(body())==expected+'\n'+expected,repr(txt(body()))
 xdo('key','ctrl+z');time.sleep(.2);assert txt(body())==expected+'\n'
 xdo('key','ctrl+y');time.sleep(.2);assert txt(body())==expected+'\n'+expected
 # Plain-text paste explicitly removes inherited spans.
 owner=clipboard('Klartext Öß\nZweite Zeile')
 text.queryComponent().grabFocus();xdo('key','ctrl+End','Return','ctrl+shift+v');time.sleep(.4);owner.terminate();owner.wait(timeout=5)
 assert txt(body()).endswith('Klartext Öß\nZweite Zeile'),repr(txt(body()))
 text.queryComponent().grabFocus();xdo('key','ctrl+a');click('Link / Seite')
 entry=next(n for n in nodes(find('Link oder Seitenverweis','dialog')) if n.getRoleName()=='text')
 entry.queryEditableText().setTextContents('page:1');click('Übernehmen')
 if not free:shot('text-format-dialog')
 save();t=doc().find('.//text');assert t.text.endswith('Klartext Öß\nZweite Zeile') and 'foliq-rich' in t.attrib
 rich=configparser.ConfigParser(interpolation=None);rich.read_string(base64.b64decode(t.attrib['foliq-rich']).decode());assert rich['rich']['version']=='1'
 kinds={rich[s]['kind'] for s in rich.sections() if s.startswith('span')};assert {'bold','italic','underline','link'}<=kinds,kinds
 print('PASS German Unicode, multiline, formatted/native clipboard, plain paste, local undo/redo and XOPP save',flush=True)
 # Leaving an unchanged session must preserve the original selection pointer.
 click('Text','toggle button');xdo('mousemove',680,textY,'click',1);xdo('key','F4');click('Fertig');click('Auswahl','toggle button')
 xdo('key','ctrl+c','ctrl+v');time.sleep(.3);assert p.poll() is None
 click('Rückgängig');click('Speichern');time.sleep(.4);assert len(doc().findall('.//text'))==1
 if free:
  original=doc().find('.//text').text;camera=doc().find('.//page').attrib['foliq-camera-y']
  click('Text','toggle button');xdo('mousemove',680,textY,'click',1);xdo('key','End');xdo('type','!')
  xdo('mousemove',1000,600,'click',5);time.sleep(.3)
  click('Rückgängig');click('Speichern');time.sleep(.3)
  assert doc().find('.//page').attrib['foliq-camera-y']==camera
  assert doc().find('.//text').text!=original
  click('Rückgängig');click('Speichern');time.sleep(.3);assert doc().find('.//text').text==original
  print('PASS shared text/camera undo ordering on free pages',flush=True)
 # Following an internal link closes the modal before navigation can destroy its editor.
 click('Text','toggle button');xdo('mousemove',680,textY,'click',1);xdo('key','F4');click('Link öffnen');time.sleep(.4);assert p.poll() is None
 # Change the same container to a real table, then operate rows and columns.
 click('Text','toggle button');xdo('mousemove',680,textY,'click',1);xdo('key','F4');click('Tabelle…');find('Tabelle bearbeiten','dialog')
 find('Zelle 1, 1','text').queryEditableText().setTextContents('Käse')
 find('Zelle 1, 2','text').queryEditableText().setTextContents('Größe')
 find('Zelle 2, 1','text').queryEditableText().setTextContents('Äpfel')
 find('Zelle 2, 2','text').queryEditableText().setTextContents('12 €')
 click('Zeile +');find('Zelle 3, 1','text')
 click('Spalte +');find('Zelle 1, 3','text')
 click('Zeile −');find('Zelle 2, 1','text');click('Spalte −');find('Zelle 1, 2','text')
 click('Übernehmen');time.sleep(.3)
 # A table edit is one local undo step; redo restores its structure.
 xdo('key','ctrl+z','ctrl+y','F4');find('Tabelle bearbeiten','dialog')
 find('Zelle 1, 1','text').queryEditableText().setTextContents('Erneut Grüße')
 find('Zelle 1, 2','text').queryEditableText().setTextContents('42')
 click('Übernehmen');click('Auswahl','toggle button');click('Speichern');time.sleep(.5)
 t=doc().find('.//text');rich.read_string(base64.b64decode(t.attrib['foliq-rich']).decode());assert rich['rich']['columns']=='2';assert 'Erneut Grüße' in t.text
 # Restart and reopen the cell editor, proving table persistence beyond rendered output.
 quitapp();assert p.returncode==0;p=launch();click('Text','toggle button');xdo('mousemove',680,textY,'click',1);xdo('key','F4');find('Tabelle bearbeiten','dialog')
 assert txt(find('Zelle 1, 1','text'))=='Erneut Grüße'
 find('Zelle 1, 1','text').queryEditableText().setTextContents('Neustart ÄÖÜ ß')
 click('Übernehmen');click('Auswahl','toggle button');click('Speichern');time.sleep(.5);assert 'Neustart ÄÖÜ ß' in doc().find('.//text').text
 if not free:shot('text-table-reopened')
 quitapp();assert p.returncode==0
 pdf=root/'output.pdf';subprocess.run(['/src/build-codex/xournalpp','--create-pdf='+str(pdf),str(fixture)],env=env,check=True,stdout=log,stderr=log)
 extracted=subprocess.check_output(['pdftotext',str(pdf),'-']).decode();assert 'Neustart ÄÖÜ ß' in extracted,repr(extracted)
 print('PASS table cell edits, insert/delete rows and columns, undo/redo, restart/reedit and PDF Unicode extraction',flush=True)
 if free:
  assert float(doc().find('.//text').attrib['x'])<0 and float(doc().find('.//text').attrib['y'])<0
  assert doc().find('.//page').attrib['foliq-surface-version']=='1'
  print('PASS free-page negative-coordinate text, object clipboard, table persistence and tiled PDF',flush=True)
 print('Synthetic artifacts:',root,flush=True)
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=10)
