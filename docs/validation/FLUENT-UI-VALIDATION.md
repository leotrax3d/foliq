# Fluent-UI – Prüfprotokoll (2026-11-11)

**Plattform:** Linux (Ubuntu 24.04-Container), GTK 3.24.41, Xvfb ohne
Fenstermanager, Maus über xdotool, AT-SPI. Kein Windows, kein Stift, kein Touch,
keine echte DPI-Skalierung außer GTK `GDK_SCALE=2`. Screenshots zeigen die
Linux-GTK-Darstellung, **nicht** das Windows-Erscheinungsbild.

## Reproduzieren

```bash
/usr/bin/python3.12 docs/validation/demo-library.py /tmp/foliq-demo/library
xvfb-run -a -s '-screen 0 2600x1700x24' dbus-run-session -- /usr/bin/python3.12 docs/validation/fluent-shots.py after build-codex collapsed focus navfloat menu accent dialogs touch dark search
xvfb-run -a -s '-screen 0 2600x1700x24' dbus-run-session -- /usr/bin/python3.12 docs/validation/main-ui-smoke.py
CI=true build-codex/test/test-units
```

Demodokument (fiktiv, deterministisch): 3 Notizbücher, Gruppe, Abschnitte,
Unterseite, lange Titel; Handschrift mit Druckprofil, Rich-Text-Überschrift,
Tabelle, Bild, Formel-Bild (Platzhalter-Bitmap, kein TeX-Lauf), PDF-Hintergrund
mit Anmerkungen, freie Arbeitsfläche. **Aufgaben, Tags und Anhänge existieren
in Foliq noch nicht (Roadmap M5)** und sind nur als Text auf der Seite
dargestellt; Verschlüsselung/Passwortdialoge ebenso nicht (M6).

## Ergebnisse

- Unit-Tests: 132/132 bestanden (neu: FoliqThemeTest – Hex-Parser, Kontrast ≥3:1
  für 7 Problemfarben × hell/dunkel, view.ini v1→v2-Migration, Roundtrip,
  Ablehnung unbekannter Version/ungültiger Werte). Darin enthalten die
  bestehenden PDF-Export-, Speicher-, Katalog- und Rich-Text-Tests.
- main-ui-smoke.py (angepasst an neue Bedienelemente): echter Strich +
  Undo/Redo über Befehlsleiste + Autosave; Drag-and-drop; Alt+↑/↓; Theme,
  Fokusmodus und Ansichtsoptionen über Neustart; 800×600; GDK_SCALE=2;
  unbekannte view.ini-Version bleibt unverändert – alle PASS.
- fluent-shots.py: 18 Zustände (hell, dunkel, eingeklappt, temporär schwebend,
  Fokusmodus, schwebende Navigation, ⋯-Menü, Akzent-Popover, Violett-Akzent,
  Aktionsmenü, Eingabedialog, Einstellungen, Berührung, Suche, klein).
- Vorher/Nachher mit identischem Demodokument: `fluent/before-*` vs.
  `fluent/after-*` (main-light, ribbon-insert, ribbon-draw, small). Der alte
  Dunkelmodus-Lauf öffnete nur die Auswahlliste und wurde verworfen; alte
  Dunkelreferenz: `main-ui-dark.png`.
- Tastatur: F6-Bereichswechsel, Strg+F1, Strg+Umschalt+N/F, Escape, F10
  implementiert; im ⋯-Menü per Pfeiltasten bis „Preferences" navigiert
  (Screenshot). Sichtbarer Fokusring in Menüs/Bäumen bestätigt.
  Zugängliche Namen für alle Icon-Befehle gesetzt (AT-SPI-Smoketest findet sie).

## Nicht ausgeführt / offen

Windows 100/150/200 %, Windows-Build dieses Stands, NVDA/Narrator/UIA,
physischer Stift (Druck, Radierer, Palm Rejection, Striche während Panel-
Animation), Touch, IME. Native Windows-Dateidialoge (GtkFileChooserNative)
noch nicht umgestellt: Öffnen-Dialoge nutzen Zusatzwidgets (Vorschau,
„Datei anhängen"), die nativ nicht 1:1 abbildbar sind. Upstream-Dialogtexte
erscheinen in dieser Umgebung englisch (Übersetzungen nicht installiert).
Kontraste nur rechnerisch (Tokens) und visuell geprüft, kein Messwerkzeug.
