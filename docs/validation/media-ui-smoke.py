# SPDX-License-Identifier: GPL-2.0-or-later
# Real GTK/X11 tests: image crop/size, TeX editing and error recovery, PDF export.
exec(open('/src/docs/validation/main-ui-smoke.py').read().split('\np=start()\n')[0])
import tempfile, base64, xml.etree.ElementTree as ET
import gi;gi.require_version('GdkPixbuf','2.0');from gi.repository import GdkPixbuf
root=Path(tempfile.mkdtemp(prefix='foliq-media-ui-'))
env.update(XDG_CONFIG_HOME=str(root/'config'),XDG_CACHE_HOME=str(root/'cache'),XDG_DATA_HOME=str(root/'data'),FOLIQ_LIBRARY=str(root/'library'))
log=open(root/'app.log','w')
# Actual default template and compiler, not a screenshot standing in for formula data.
template=Path('/src/resources/default_template.tex').read_text().replace('%%XPP_TOOL_INPUT%%',r'x^2').replace('%%XPP_TEXT_COLOR%%','000000')
(root/'tex.tex').write_text(template)
subprocess.run(['pdflatex','-halt-on-error','-interaction=nonstopmode','tex.tex'],cwd=root,stdout=log,stderr=log,check=True)
pix=GdkPixbuf.Pixbuf.new(GdkPixbuf.Colorspace.RGB,False,8,160,80);pix.fill(0x4060d0ff);pix.savev(str(root/'image.png'),'png',[],[])
xml='<xournal fileversion="4"><page width="595" height="842"><background type="solid" color="#ffffffff" style="plain"/><layer><text font="Sans" size="16" x="80" y="80" color="#000000ff">Medien Grüße ÄÖÜ ß</text><image left="80" top="180" right="240" bottom="260">'+base64.b64encode((root/'image.png').read_bytes()).decode()+'</image><teximage left="80" top="320" right="160" bottom="360" text="x^2">'+base64.b64encode((root/'tex.pdf').read_bytes()).decode()+'</teximage><stroke tool="pen" color="#ff0000ff" width="2">80 390 100 410 160 390</stroke></layer></page></xournal>'
fixture=root/'input.xopp';fixture.write_bytes(gzip.compress(xml.encode()))
def doc():return ET.fromstring(gzip.decompress(fixture.read_bytes()))
def launch():
 proc=subprocess.Popen(['/src/build-codex/xournalpp',str(fixture)],cwd='/src',env=env,stdout=log,stderr=log);time.sleep(3)
 wid=subprocess.check_output(['xdotool','search','--onlyvisible','--pid',str(proc.pid),'--name','Foliq']).decode().splitlines()[-1]
 xdo('windowsize',wid,1280,900);xdo('windowfocus',wid);time.sleep(.5);return proc
def select(x1,y1,x2,y2):
 click('Start','page tab');click('Auswahl','toggle button');xdo('mousemove',x1,y1,'mousedown',1,'mousemove',x2,y2,'mouseup',1);time.sleep(.3)
def save():click('Speichern');time.sleep(.5)
def texteditor():
 for n in nodes(find('Insert Latex','dialog')):
  if n.getRoleName()=='text' and n.getState().contains(pyatspi.STATE_EDITABLE):return n
 raise AssertionError('missing TeX editor')
def waitok():
 for _ in range(120):
  n=find('OK');
  if n.getState().contains(pyatspi.STATE_ENABLED):return
  time.sleep(.1)
 raise AssertionError('formula never rendered')
p=launch()
try:
 select(620,350,800,450);click('Einfügen','page tab');click('Bildgröße / Zuschnitt…');find('Bildgröße und Zuschnitt','dialog')
 for name,value in [('Breite (Seitenpunkte)','120'),('Links (%)','25'),('Rechts (%)','75')]:
  find(name,'spin button').queryEditableText().setTextContents(value)
 click('Übernehmen');save();image=doc().find('.//image');assert float(image.attrib['crop-left'])==.25 and float(image.attrib['crop-right'])==.75
 assert abs(float(image.attrib['right'])-float(image.attrib['left'])-120)<.01
 assert abs(float(image.attrib['bottom'])-float(image.attrib['top'])-120)<.01
 assert [e.tag for e in doc().find('.//layer')]==['text','image','teximage','stroke']
 click('Rückgängig');save();assert 'crop-left' not in doc().find('.//image').attrib
 click('Wiederholen');save();assert float(doc().find('.//image').attrib['crop-left'])==.25
 print('PASS image crop/size, original retained, aspect ratio, stacking and one-step undo/redo',flush=True)
 # Existing formula is selected and edited through the same canvas selection.
 select(620,490,720,550);click('Einfügen','page tab');click('Formel…');find('Insert Latex','dialog');editor=texteditor()
 assert editor.queryText().getText(0,-1)=='x^2'
 editor.queryEditableText().setTextContents(r'\frac{a}{b}+y^3');waitok();click('OK');save()
 assert doc().find('.//teximage').attrib['text']==r'\frac{a}{b}+y^3'
 assert [e.tag for e in doc().find('.//layer')]==['text','image','teximage','stroke']
 click('Rückgängig');save();assert doc().find('.//teximage').attrib['text']=='x^2'
 click('Wiederholen');save();assert doc().find('.//teximage').attrib['text']==r'\frac{a}{b}+y^3'
 # Reopen after saving, then reject a real compiler syntax error without touching stored data.
 select(620,490,740,560);click('Einfügen','page tab');click('Formel…');find('Insert Latex','dialog');editor=texteditor()
 assert editor.queryText().getText(0,-1)==r'\frac{a}{b}+y^3'
 editor.queryEditableText().setTextContents(r'\frac{');time.sleep(2)
 assert not find('OK').getState().contains(pyatspi.STATE_ENABLED)
 assert any('could not be rendered' in n.name for n in nodes()),'missing actionable error'
 shot('text-formula-error');click('Cancel');save();assert doc().find('.//teximage').attrib['text']==r'\frac{a}{b}+y^3'
 quitapp();assert p.returncode==0;p=launch();select(620,490,740,560);click('Einfügen','page tab');click('Formel…');find('Insert Latex','dialog')
 assert texteditor().queryText().getText(0,-1)==r'\frac{a}{b}+y^3';click('Cancel');quitapp();assert p.returncode==0
 pdf=root/'output.pdf';subprocess.run(['/src/build-codex/xournalpp','--create-pdf='+str(pdf),str(fixture)],env=env,check=True,stdout=log,stderr=log)
 assert 'Medien Grüße ÄÖÜ ß' in subprocess.check_output(['pdftotext',str(pdf),'-']).decode()
 assert pdf.stat().st_size>1000
 print('PASS real TeX compile, source save/reopen/reedit, syntax error preserving original, one-step undo/redo and mixed PDF export',flush=True)
 print('Synthetic artifacts:',root,flush=True)
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=10)
