# SPDX-License-Identifier: GPL-2.0-or-later
"""Linux/Xvfb screenshot + smoke run for the Fluent UI revision.

Usage (from repo root):
  xvfb-run -a -s '-screen 0 2600x1700x24' dbus-run-session -- \
    /usr/bin/python3.12 docs/validation/fluent-shots.py <prefix> [build-dir]
Uses a fresh demo library from demo-library.py in a temp dir. Screenshots are
Linux/GTK renderings, NOT evidence of the Windows appearance.
"""
import os, subprocess, sys, time, shutil, tempfile
from pathlib import Path
import pyatspi, gi
gi.require_version('Gdk', '3.0')
from gi.repository import Gdk

prefix = sys.argv[1]
build = Path(sys.argv[2] if len(sys.argv) > 2 else 'build-codex')
repo = Path.cwd()
out = repo / 'docs/validation/fluent'
out.mkdir(exist_ok=True)
tmp = Path(tempfile.mkdtemp(prefix='foliq-fluent-'))
subprocess.run(['/usr/bin/python3.12', 'docs/validation/demo-library.py', str(tmp / 'library')], check=True)
env = os.environ.copy()
env.update(XDG_CONFIG_HOME=str(tmp / 'config'), XDG_CACHE_HOME=str(tmp / 'cache'),
           XDG_DATA_HOME=str(tmp / 'data'), FOLIQ_LIBRARY=str(tmp / 'library'), GTK_MODULES='gail:atk-bridge',
           LANG='de_DE.UTF-8', LANGUAGE='de')
extra = [a for a in sys.argv[3:]]


def xdo(*a):
    subprocess.run(['xdotool', *map(str, a)], check=True)


def nodes(n=None):
    if n is None:
        n = pyatspi.Registry.getDesktop(0)
    yield n
    try:
        for c in n:
            yield from nodes(c)
    except Exception:
        pass


def find(name=None, role=None, tries=30):
    for _ in range(tries):
        for n in nodes():
            try:
                if (name is None or n.name == name) and (role is None or n.getRoleName() == role) and \
                        n.getState().contains(pyatspi.STATE_SHOWING):
                    return n
            except Exception:
                pass
        time.sleep(.15)
    return None


def click(name, role=None):
    n = find(name, role)
    if not n:
        print('MISSING', name, role, flush=True)
        return False
    r = n.queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
    s = int(env.get('GDK_SCALE', '1'))
    xdo('mousemove', (r.x + r.width // 2) * s, (r.y + r.height // 2) * s, 'click', 1)
    time.sleep(.6)
    return True


def shot(name):
    time.sleep(.8)
    s = int(env.get('GDK_SCALE', '1'))
    r = find(role='frame').queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
    w = Gdk.get_default_root_window()
    Gdk.pixbuf_get_from_window(w, r.x * s, r.y * s, r.width * s, r.height * s).savev(
        str(out / f'{prefix}-{name}.png'), 'png', [], [])
    print('SHOT', name, flush=True)


def start(w=1280, h=800):
    p = subprocess.Popen([str(build / 'xournalpp')], cwd=repo, env=env,
                         stdout=open(tmp / 'app.log', 'a'), stderr=subprocess.STDOUT)
    time.sleep(4)
    wid = subprocess.run(['xdotool', 'search', '--onlyvisible', '--pid', str(p.pid), '--name', '.'],
                         capture_output=True, text=True).stdout.split()[-1]
    s = int(env.get('GDK_SCALE', '1'))
    xdo('windowmove', wid, 0, 0)
    xdo('windowsize', wid, w * s, h * s)
    time.sleep(1.2)
    return p, wid


def stop(p):
    xdo('key', 'ctrl+q')
    try:
        p.wait(timeout=15)
    except subprocess.TimeoutExpired:
        p.terminate()
        p.wait(timeout=10)


p, wid = start()
try:
    shot('main-light')
    click('Einfügen', None)
    shot('ribbon-insert')
    click('Zeichnen', None)
    shot('ribbon-draw')
    for step in extra:
        if step == 'dark':
            click('Ansicht', None)
            click('Darstellung: Design, Eingabe, Akzentfarbe, Bewegung', None)
            shot('look-popover')
            click('Dunkel', None)
            xdo('key', 'Escape')
            time.sleep(1)
            shot('main-dark')
        elif step == 'touch':
            click('Ansicht', None)
            click('Darstellung: Design, Eingabe, Akzentfarbe, Bewegung', None)
            click('Berührung', None)
            xdo('key', 'Escape')
            shot('touch')
            click('Darstellung: Design, Eingabe, Akzentfarbe, Bewegung', None)
            click('Kompakt', None)
            xdo('key', 'Escape')
        elif step == 'focus':
            click('Ansicht', None)
            click('Fokusmodus', None)
            shot('focus')
            click('Ansicht', None)  # temporarily opened ribbon floats over the canvas
            shot('focus-ribbon-temporary')
            xdo('key', 'Escape')
            xdo('key', 'ctrl+shift+f')
            time.sleep(.6)
        elif step == 'navfloat':
            click('Navigation anheften', None)
            click('Navigation', None)
            shot('nav-floating')
            xdo('key', 'Escape')
            time.sleep(.4)
            click('Navigation', None)
            click('Navigation anheften', None)
        elif step == 'menu':
            click('Menü', None)
            shot('app-menu')
            xdo('key', 'Escape')
        elif step == 'accent':
            click('Ansicht', None)
            click('Darstellung: Design, Eingabe, Akzentfarbe, Bewegung', None)
            click('Akzentfarbe wählen …', None)
            shot('accent-popover')
            click('Violett', None)
            xdo('key', 'Escape')
            shot('accent-violet')
        elif step == 'collapsed':
            click('Menüband einklappen', None)
            shot('ribbon-collapsed')
            click('Zeichnen', None)
            shot('ribbon-temporary')
            xdo('key', 'Escape')
            time.sleep(.4)
            click('Menüband anheften', None)
        elif step == 'settings':
            xdo('key', 'ctrl+comma')
            time.sleep(2)
            r = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'Einstellungen|Preferences'],
                               capture_output=True, text=True).stdout.split()
            if r:
                time.sleep(.5)
                g = Gdk.get_default_root_window()
                Gdk.pixbuf_get_from_window(g, 0, 0, 1400, 1000).savev(str(out / f'{prefix}-settings.png'), 'png',
                                                                       [], [])
                print('SHOT settings', flush=True)
                xdo('key', 'Escape')
        elif step == 'dialogs':
            def screen(name, w=1300, h=820):
                time.sleep(1.2)
                g = Gdk.get_default_root_window()
                Gdk.pixbuf_get_from_window(g, 0, 0, w, h).savev(str(out / f'{prefix}-{name}.png'), 'png', [], [])
                print('SHOT', name, flush=True)
            click('Notizbuchaktionen', None)
            shot('nav-actions-menu')
            click('Notizbuch', 'menu item')
            screen('dialog-prompt')
            xdo('key', 'Escape')
            time.sleep(.5)
            click('Menü', None)
            xdo('key', 'Down', 'Down', 'Return')  # File → Edit submenu, keyboard only
            time.sleep(.6)
            xdo('key', 'Up', 'Return')  # Preferences is the last Edit entry
            time.sleep(.6)
            n = find('Preferences', 'menu item') or find('Einstellungen', 'menu item')
            if n:
                r = n.queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
                xdo('mousemove', r.x + r.width // 2, r.y + r.height // 2, 'click', 1)
                time.sleep(3)
            ids = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'Preferences|Einstellungen'],
                                 capture_output=True, text=True).stdout.split()
            if ids:
                xdo('windowmove', ids[-1], 0, 0)
                xdo('windowsize', ids[-1], 1100, 760)
            screen('dialog-settings', 1100, 760)
            xdo('key', 'Escape')
            time.sleep(.5)
        elif step == 'fullscreen':
            click('Vollbild (F11)', None)
            time.sleep(1.5)
            shot('fullscreen')
            xdo('key', 'Escape')
            time.sleep(1)
            shot('fullscreen-left')
        elif step == 'shapes':
            click('Zeichnen', None)
            click('Rechteck', None)
            shot('draw-shapes')
            xdo('mousemove', 700, 300, 'mousedown', 1, 'mousemove', 820, 380, 'mouseup', 1)
            time.sleep(.6)
            click('Geodreieck / Lineal (erneut klicken zum Entfernen)', None)
            shot('draw-setsquare')
            click('Geodreieck / Lineal (erneut klicken zum Entfernen)', None)
            click('Rechteck', None)
        elif step == 'search':
            xdo('key', 'ctrl+f')
            shot('search')
            xdo('key', 'Escape')
    xdo('windowsize', wid, 820, 600)
    time.sleep(1.2)
    shot('small')
finally:
    stop(p)
print('log tail:', (tmp / 'app.log').read_text()[-600:])
shutil.rmtree(tmp, ignore_errors=True)
