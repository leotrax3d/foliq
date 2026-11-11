# SPDX-License-Identifier: GPL-2.0-or-later
"""Reproducible fictional German demo library for Foliq UI before/after screenshots.

Usage: /usr/bin/python3.12 docs/validation/demo-library.py <target-library-dir>
Creates catalog.ini (v1) and XOPP pages. Deterministic IDs and contents; no
private data. Only uses the Python standard library plus PyGObject/cairo, which
the Codex setup already installs (python3-gi, python3-gi-cairo).
"""
import base64, gzip, math, struct, sys, zlib
from pathlib import Path

root = Path(sys.argv[1])
(root / 'documents').mkdir(parents=True, exist_ok=True)


def uid(n):
    return f'00000000-0000-4000-8000-{n:012d}'


def png(w, h, fn):
    rows = b''.join(b'\0' + b''.join(bytes(fn(x, y)) for x in range(w)) for y in range(h))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def sky(x, y):  # fictional landscape photo substitute
    if y > 70 + 12 * math.sin(x / 18):
        return (74, 128 - y // 4, 82)
    return (150 + y // 2, 190 + y // 4, 235)


def rich(text, columns=0, spans=()):
    lines = ['[rich]', 'version=1', 'text=' + text.replace('\\', '\\\\').replace('\n', '\\n').replace('\t', '\\t'),
             f'columns={columns}', f'count={len(spans)}']
    for i, (s, e, k, v) in enumerate(spans):
        lines += [f'[span{i}]', f'start={s}', f'end={e}', f'kind={k}', f'value={v}']
    return base64.b64encode(('\n'.join(lines) + '\n').encode()).decode()


def esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;').replace('"', '&quot;')


def handwriting(x0, y0, word_count=3):
    out = []
    for w in range(word_count):
        pts, widths = [], []
        for i in range(60):
            t = i / 59
            x = x0 + w * 95 + t * 80
            y = y0 + 9 * math.sin(t * 13 + w) - 6 * math.cos(t * 5)
            pts.append(f'{x:.2f} {y:.2f}')
            widths.append(f'{1.0 + 0.9 * math.sin(t * math.pi):.2f}')  # pressure profile
        out.append(f'<stroke tool="pen" color="#1f3a8aff" width="{" ".join(widths[:1] + widths[1:])}">' +
                   ' '.join(pts) + '</stroke>')
    return ''.join(out)


def page(body, bg='<background type="solid" color="#ffffffff" style="lined"/>', w=595.28, h=841.89, extra=''):
    return f'<page width="{w}" height="{h}"{extra}>{bg}<layer>{body}</layer></page>'


def xopp(pages, rich_used=False):
    attrs = ' fileversion="5" foliq-content-version="1"' if rich_used else ' fileversion="4"'
    return gzip.compress(f'<?xml version="1.0" standalone="no"?><xournal creator="Foliq demo"{attrs}>'
                         f'<title>Foliq Demo</title>{"".join(pages)}</xournal>'.encode(), mtime=0)


def text(x, y, s, size=12, color='#202124ff', r=None):
    a = f' foliq-rich="{r}"' if r else ''
    return f'<text font="Sans" size="{size}" x="{x}" y="{y}" color="{color}"{a}>{esc(s)}</text>'


def pdf_file(path):
    """Minimal hand-written single-page PDF (Helvetica, standard 14 font) without extra libraries."""
    lines = ['Fiktiver Projektbericht Q3', 'Dieser Bericht ist frei erfunden und dient nur als Demo.',
             'Abschnitt 1: Ausgangslage und Ziele.', 'Abschnitt 2: Ergebnisse der Pilotphase.',
             'Abschnitt 3: Offene Punkte und naechste Schritte.']
    ops = ['BT', '/F1 20 Tf', '60 750 Td', f'({lines[0]}) Tj', '/F1 11 Tf']
    for line in lines[1:]:
        ops += ['0 -24 Td', f'({line}) Tj']
    stream = '\n'.join(ops + ['ET']).encode('latin-1')
    objs = [b'<< /Type /Catalog /Pages 2 0 R >>', b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
            b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595.28 841.89] /Contents 4 0 R '
            b'/Resources << /Font << /F1 5 0 R >> >> >>',
            b'<< /Length ' + str(len(stream)).encode() + b' >>\nstream\n' + stream + b'\nendstream',
            b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>']
    out, offsets = bytearray(b'%PDF-1.4\n'), []
    for i, o in enumerate(objs, 1):
        offsets.append(len(out))
        out += f'{i} 0 obj\n'.encode() + o + b'\nendobj\n'
    xref = len(out)
    out += f'xref\n0 {len(objs) + 1}\n0000000000 65535 f \n'.encode()
    for off in offsets:
        out += f'{off:010d} 00000 n \n'.encode()
    out += f'trailer\n<< /Size {len(objs) + 1} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n'.encode()
    path.write_bytes(bytes(out))


img = base64.b64encode(png(160, 110, sky)).decode()
formula = base64.b64encode(png(120, 30, lambda x, y: (255, 255, 255) if (x + y) % 23 else (40, 40, 40))).decode()
TABLE = ('Aufgabe\tVerantwortlich\tStatus\nAngebot prüfen\tMara\terledigt\nTermin abstimmen\tJonas\toffen\n'
             'Unterlagen sammeln\tLeyla\tin Arbeit')
table = rich(TABLE, columns=3)
heading = rich('Projekt Lindenhof – Besprechung', spans=[(0, 31, 'bold', '1')])

docs = {
    1: xopp([page(text(60, 60, 'Projekt Lindenhof – Besprechung', 20, r=heading) +
                  text(60, 100, '☐ Angebot der Schreinerei prüfen  #Aufgabe\n☑ Raumplan verschicken  #erledigt\n'
                       'Tags: #Projekt #Lindenhof\nAnhang: Raumplan_Entwurf.pdf (Verweis)') +
                  handwriting(60, 220) +
                  text(60, 270, 'Tabelle', 14) + text(60, 295, TABLE, r=table) +
                  f'<image left="330" top="390" right="530" bottom="527.5">{img}</image>' +
                  f'<teximage text="E = mc^2" left="70" top="420" right="250" bottom="465">{formula}</teximage>')],
            True),
    2: xopp([page(handwriting(70, 120, 4) + text(70, 80, 'Skizzen und Gedanken'),
                  bg='<background type="solid" color="#ffffffff" style="graph"/>')]),
    3: xopp([page(text(60, 60, 'Einkaufsliste für das Wochenende'))]),
    4: None,  # PDF annotation page, written below
    5: xopp([page(text(80, 80, 'Freie Arbeitsfläche: Mindmap Umzug') + handwriting(120, 200, 5),
                  bg='<background type="solid" color="#ffffffff" style="plain"/>', w=1600, h=1200,
                  extra=(' foliq-surface-version="1" foliq-camera-x="0" foliq-camera-y="0" foliq-export-content="1"'
                         ' foliq-crop-x="0" foliq-crop-y="0" foliq-crop-width="1600" foliq-crop-height="1200"'
                         ' foliq-tiled="1" foliq-tile-width="595.276" foliq-tile-height="841.89"'))]),
}
pdf = root / 'documents' / 'Projektbericht_fiktiv.pdf'
pdf_file(pdf)
docs[4] = xopp([page(handwriting(330, 175, 2) +
                     '<stroke tool="highlighter" color="#ffff0080" width="12">60 162 330 162</stroke>',
                     bg=f'<background type="pdf" domain="absolute" filename="{esc(str(pdf))}" pageno="1"/>')])

nodes = [  # id, parent, kind(0 nb,1 group,2 section,3 page), name, order
    (101, '', 0, 'Arbeit', 0),
    (102, uid(101), 1, 'Kundenprojekte', 0),
    (103, uid(102), 2, 'Projekt Lindenhof mit einem bewusst sehr langen Abschnittstitel', 0),
    (1, uid(103), 3, 'Besprechung mit Bauherrin und Planungsbüro am Donnerstagvormittag', 0),
    (4, uid(103), 3, 'Projektbericht (PDF-Anmerkungen)', 1),
    (2, uid(1), 3, 'Skizzen', 0),
    (104, uid(101), 2, 'Teamorganisation', 1),
    (201, '', 0, 'Privat', 1),
    (202, uid(201), 2, 'Haushalt', 0),
    (3, uid(202), 3, 'Einkauf', 0),
    (5, uid(202), 3, 'Umzug planen', 1),
    (301, '', 0, 'Studium Archiv', 2),
]
for n, data in docs.items():
    (root / 'documents' / f'{uid(n)}.xopp').write_bytes(data)
out = ['[catalog]', 'version=1', 'revision=1', f'last={uid(1)}', 'page=0', '']
for i, parent, kind, name, order in nodes:
    out += [f'[{uid(i)}]', f'name={name}', f'parent={parent}', f'kind={kind}', f'order={order}', 'trash=0',
            'open=1', '']
(root / 'catalog.ini').write_text('\n'.join(out))
print('demo library at', root)
