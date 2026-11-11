# SPDX-License-Identifier: GPL-2.0-or-later
# Run after main-ui-smoke.py in the same isolated test container.
exec(open('/src/docs/validation/main-ui-smoke.py').read().split('\np=start()\n')[0])
# Minimal two-page PDF; all content is synthetic.
objects=[b'<< /Type /Catalog /Pages 2 0 R >>',b'<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>',b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 5 0 R >>',b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 5 0 R >>',b'<< /Length 0 >>\nstream\n\nendstream']
pdf=b'%PDF-1.4\n';offsets=[0]
for i,obj in enumerate(objects,1):offsets.append(len(pdf));pdf+=str(i).encode()+b' 0 obj\n'+obj+b'\nendobj\n'
xref=len(pdf);pdf+=b'xref\n0 6\n0000000000 65535 f \n'+b''.join(f'{n:010d} 00000 n \n'.encode() for n in offsets[1:])+f'trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n'.encode()
pdfpath=root/'reference.pdf';pdfpath.write_bytes(pdf)
(root/'annotated.xopp').unlink(missing_ok=True)
p=subprocess.Popen(['/src/build-codex/xournalpp',str(pdfpath)],cwd='/src',env=env,stdout=log,stderr=log);time.sleep(4)
wid=subprocess.check_output(['xdotool','search','--onlyvisible','--pid',str(p.pid),'--name','Foliq']).decode().splitlines()[-1];xdo('windowsize',wid,1200,800);time.sleep(.5)
try:
 shot('main-ui-pdf-before')
 # Coordinates will be derived from the real document view in the final test.
 xdo('mousemove',850,470,'mousedown',1,'mousemove',920,510,'mouseup',1);time.sleep(.5)
 xdo('key','ctrl+shift+s');time.sleep(1);xdo('key','ctrl+l');xdo('type',str(root/'annotated.xopp'));xdo('key','Return');time.sleep(2)
 content=gzip.decompress((root/'annotated.xopp').read_bytes());assert content.count(b'<page ')==2 and b'type="pdf"' in content and b'<stroke' in content
 print('PASS two-page PDF opens, pen annotation and XOPP save',flush=True)
 quitapp();assert p.returncode==0
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=10)
