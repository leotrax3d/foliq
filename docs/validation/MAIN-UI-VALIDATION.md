# Hauptoberfläche: Linux-Abnahme am 2026-11-11

Plattform: Ubuntu 24.04 x86_64 im verwalteten Docker-Container `foliq-ui`,
Host Debian 13. GCC 13.3, GTK 3.24.41, GLib 2.80, CMake 3.28.3/Ninja.
Native GTK-App unter Xvfb/X11 und DBus/AT-SPI; kein Browser-Nachbau.
Alle Notizen und das PDF sind synthetische Wegwerfdaten.

Build als UID 1000, Repository unter /src:

```sh
FOLIQ_BUILD_JOBS=4 bash scripts/foliq/setup-codex.sh --build
xvfb-run -a -s '-screen 0 3200x2000x24' dbus-run-session -- \
  python3 /src/docs/validation/main-ui-smoke.py
xvfb-run -a -s '-screen 0 1600x1000x24' dbus-run-session -- \
  python3 /src/docs/validation/main-ui-pdf.py
```

Der Smoke-Test erwartet eine frische, ausschließlich synthetische Bibliothek
`/tmp/foliq-main-ui`. Bei Wiederholung diese Testbibliothek vorher separat
verschieben; der Test löscht keine beliebigen Benutzerdaten. Der PDF-Test folgt
auf den erfolgreichen Smoke-Test. Keine Laufzeitabhängigkeit der Anwendung.

Der Buildhelper baut Editor und test-units und führt alle 100 Linux-Unit-Tests
(21 Suites) aus. Beleg: main-ui-linux-tests.xml. Windows besitzt zusätzlich einen
Win32-spezifischen Test; hier kein neuer Windows-Build.

Geprüfte GUI-Pfade werden durch Assertions mit dem realen Katalog bzw. dem
dekomprimierten XOPP verglichen: Hierarchie anlegen, Unterseite öffnen,
Werkzeugwechsel, Strich, Undo/Redo, 5-s-Autosave, F2, tatsächliches Ziehen einer
Seite und eines Abschnitts, Alt+↑/↓, Verschieben-Dialog und Dokumentsuche.
Spaltenbreiten werden mit echten Mausereignissen verändert, dann Fokus, Thema,
Reiter und Abschnitt gespeichert und nach Neustart geprüft. Kleine Fenster:
800×600 (Fenstergröße ausdrücklich geprüft), normale Ansicht 1200×800.
Skalierung: GDK_SCALE=2, 2400×1600 physische Pixel für 1200×800 logisch.
AT-SPI-Koordinaten müssen für Mausereignisse und Aufnahmen umgerechnet werden.
Unbekannte view.ini-Version 99 wird sichtbar abgewiesen und bytesgleich erhalten.

Bei der Prüfung gefunden und korrigiert: Baum-Refresh unterbrach das Ziehen von
Abschnitten; Quellbaum wird beim Auswahlwechsel nicht mehr neu aufgebaut.
Der alte Vorschau-Umschalter benötigt seinen eigenen Editorcontainer innerhalb
der neuen Aufteilung. Neues Ziel wird nach Drag-and-drop sichtbar aufgeklappt.
Die Ubuntu-GTK-Voreinstellung Yaru ohne installierte dunkle Variante machte den
ThemeVariant-Status allein visuell unwirksam; eigene dunkle Shell-Farben ergänzen
jetzt den bestehenden Theme-Pfad. Das Dokumentpapier behält seine eigene Farbe.

Der separate PDF-Test ist bestanden: zweiseitiges synthetisches PDF per CLI
geöffnet, mit dem echten Stiftwerkzeug annotiert und als XOPP gespeichert;
Assertions prüfen zwei Seiten, PDF-Hintergrund und Strich im dekomprimierten XOPP.
Dabei gefunden und korrigiert: Die alte Notizwiederherstellung konnte eine explizit
geöffnete PDF ohne XOPP-Dateinamen verdrängen. Vor Wiederherstellung wird jetzt
zusätzlich geprüft, dass noch kein PDF im Dokument geladen ist.

Screenshots: main-ui-light.png, main-ui-dark.png, main-ui-focus.png,
main-ui-small.png und main-ui-scale200.png. Es sind echte GTK-Aufnahmen.

Grenzen: keine Windows-GUI-/NVDA-/Stiftdruck-/Palm-Rejection-/Touch-/Audio-
Hardwareabnahme. Mausstriche prüfen den vorhandenen Editorpfad, keinen physischen
Stiftdruck. Windows-Matrix in WINDOWS.md ausführen. Der GTK-3.18-Menüfallback ist
nicht separat auf einem alten GTK getestet; der ausgeführte Build nutzt 3.24.
