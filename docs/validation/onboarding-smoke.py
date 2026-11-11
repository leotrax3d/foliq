# SPDX-License-Identifier: GPL-2.0-or-later
"""First-run onboarding smoke test (Linux/Xvfb, AT-SPI). Run from repo root:
xvfb-run -a -s '-screen 0 2600x1700x24' dbus-run-session -- /usr/bin/python3.12 docs/validation/onboarding-smoke.py
Fresh HOME/XDG dirs; no FOLIQ_LIBRARY so the real first-run path is exercised."""
import configparser, os, subprocess, tempfile, time
from pathlib import Path
import pyatspi, gi
gi.require_version('Gdk', '3.0')
from gi.repository import Gdk

repo = Path.cwd()
out = repo / 'docs/validation/fluent'
tmp = Path(tempfile.mkdtemp(prefix='foliq-onboarding-'))
env = os.environ.copy()
env.pop('FOLIQ_LIBRARY', None)
env.update(HOME=str(tmp), XDG_CONFIG_HOME=str(tmp / 'config'), XDG_CACHE_HOME=str(tmp / 'cache'),
           XDG_DATA_HOME=str(tmp / 'data'), GTK_MODULES='gail:atk-bridge')
target = tmp / 'Schule Notizen'


def xdo(*a): subprocess.run(['xdotool', *map(str, a)], check=True)


def nodes(n=None):
    if n is None: n = pyatspi.Registry.getDesktop(0)
    yield n
    try:
        for c in n: yield from nodes(c)
    except Exception: pass


def find(name=None, role=None, tries=40):
    for _ in range(tries):
        for n in nodes():
            try:
                if (name is None or n.name == name) and (role is None or n.getRoleName() == role) and \
                        n.getState().contains(pyatspi.STATE_SHOWING): return n
            except Exception: pass
        time.sleep(.15)
    raise AssertionError(('missing', name, role))


def click(name, role=None):
    r = find(name, role).queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
    xdo('mousemove', r.x + r.width // 2, r.y + r.height // 2, 'click', 1); time.sleep(.5)


def shot(name):
    time.sleep(.6)
    g = Gdk.get_default_root_window()
    Gdk.pixbuf_get_from_window(g, 0, 0, 1000, 760).savev(str(out / f'onboarding-{name}.png'), 'png', [], [])


p = subprocess.Popen([str(repo / 'build-codex/xournalpp')], cwd=repo, env=env,
                     stdout=open(tmp / 'app.log', 'w'), stderr=subprocess.STDOUT)
try:
    time.sleep(4)
    wid = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'Foliq einrichten'],
                         capture_output=True, text=True).stdout.split()
    assert wid, 'onboarding did not open on first run'
    xdo('windowmove', wid[-1], 0, 0); time.sleep(.5)
    click('Schüler/in', 'radio button'); shot('1-profil')
    click('Weiter', 'push button')
    # Folder: set via the label's AT-SPI text is not possible; use a pre-created folder through env-less default.
    shot('2-ort')
    click('Weiter', 'push button')
    for subject in ('Deutsch', 'Mathematik', 'Englisch', 'Biologie'):
        click(subject, 'check box')
    click('Mathematik: Farbe', 'toggle button'); click('Grün', 'push button')
    shot('3-faecher')
    click('Weiter', 'push button')
    click('Dunkel', 'radio button'); click('Berührung (Stift und Touch)', 'radio button')
    shot('4-ansicht')
    click('Weiter', 'push button'); shot('5-intro')
    click('Fertig', 'push button'); time.sleep(2)
    shot('6-ergebnis')
    config = tmp / 'config/foliq/library.ini'
    assert config.exists(), 'library.ini missing'
    c = configparser.ConfigParser(interpolation=None); c.read(config)
    root = Path(c['library']['path'])
    cat = configparser.ConfigParser(interpolation=None); cat.read(root / 'catalog.ini')
    names = {cat[s]['name']: s for s in cat.sections() if s != 'catalog'}
    for subject in ('Deutsch', 'Mathematik', 'Englisch', 'Biologie', 'Mitschriften', 'Hausaufgaben'):
        assert subject in names, (subject, names)
    view = configparser.ConfigParser(interpolation=None); view.read(root / 'view.ini')
    assert f"{names['Mathematik']}=#107c10" in view['view']['colors'], view['view']['colors']
    assert view['view']['touch'] == '1' and view['view']['theme'] == '2'
    print('PASS first-run onboarding: library.ini, 4 subject notebooks with sections, colour, theme, touch', flush=True)
    xdo('key', 'ctrl+q'); p.wait(timeout=15)
    # Second start: no wizard again.
    p = subprocess.Popen([str(repo / 'build-codex/xournalpp')], cwd=repo, env=env,
                         stdout=open(tmp / 'app2.log', 'w'), stderr=subprocess.STDOUT)
    time.sleep(5)
    again = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'Foliq einrichten'],
                           capture_output=True, text=True).stdout.split()
    assert not again, 'onboarding reappeared'
    print('PASS onboarding not repeated on second start', flush=True)
    xdo('key', 'ctrl+q'); p.wait(timeout=15)
finally:
    if p.poll() is None: p.terminate(); p.wait(timeout=10)
