# Text und Medien: Linux-Validierung, 2026-11-11

Ubuntu 24.04 im Docker-Container `foliq-text-dev`, GCC 13.3, GTK 3.24.41,
GLib 2.80, Pango 1.52, CMake 3.28.3/Ninja; Build als UID 1000 unter `/src`.
Aktuelle integrierte Basis: `81d5201e` (freie Seiten/PDF-Teilung).
Der anfängliche Checkout `e94b6344` wurde nach dem Remote-Vergleich per Fast-forward
aktualisiert; vorhandene Textänderungen und Dokumentation wurden erhalten.
Auf dem Host fehlten CMake/GTK-Entwicklungspakete und sudo. Der vorhandene
verwaltete Docker-Daemon wurde geprüft, Ubuntu 24.04 bezogen und die im
Setup-Skript genannten Entwicklungsabhängigkeiten im Testcontainer installiert.
Zusätzlich ausschließlich für Formel-/Exporttests: `texlive-latex-extra`,
`texlive-fonts-recommended`, `poppler-utils`. Keine neue Foliq-Laufzeitbibliothek. Clang-format 18 dient nur der Quellformatierung.

Kommandos für den aktuellen Checkout:

```sh
cmake -S . -B build-codex -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DFOLIQ_BUILD=ON -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=OFF
cmake --build build-codex --target xournalpp test-units --parallel 4
CI=true xvfb-run -a build-codex/test/test-units \
  --gtest_output=xml:build-codex/text-tests.xml
xvfb-run -a -s '-screen 0 1400x1000x24' dbus-run-session -- \
  python3 docs/validation/text-ui-smoke.py
xvfb-run -a -s '-screen 0 1400x1000x24' dbus-run-session -- \
  python3 docs/validation/media-ui-smoke.py
FOLIQ_TEST_FREE=1 xvfb-run -a -s '-screen 0 1400x1000x24' dbus-run-session -- \
  python3 docs/validation/text-ui-smoke.py
xvfb-run -a -s '-screen 0 1400x1000x24' dbus-run-session -- \
  python3 docs/validation/surfaces-ui-smoke.py
```

Die GUI-Skripte legen pro Lauf eine neue synthetische Testbibliothek mit
`tempfile.mkdtemp` an. Sie testen die echte GTK-App mit X11 und AT-SPI, keinen
Browser-Nachbau. Alle Daten in Screenshots und Fixtures sind synthetisch.

## Prüfungen

Unit-Tests: **128/128 Tests aus 23 Suites**, einschließlich zwölf neuer Tests.
XML-Beleg: `text-media-linux-tests.xml`. Abgedeckt sind:

- Unicode-Codepoint-/UTF-8-Bytegrenzen einschließlich Umlauten, ß, Euro,
  kombinierenden Zeichen und Emoji; gemischte Pango-Stile und Validierung.
- GtkTextBuffer-Änderungen und tatsächlich serialisierte/deserialisierte
  formatierte Zwischenablage; Objektzwischenablage und Klonen/Skalierung.
- rechteckige Tabellen, leere Zellen, Zelltexte, Layout und erneutes Bearbeiten.
- echter XOPP-Roundtrip mit langen mehrblockigen XML-Texten und erhaltenen
  Koordinaten; Version-4-Mischdokument → neue Inhalte in Version 5.
- Zurückweisung neuer/defekter Inhaltsversionen, widersprüchlicher Klartexte und
  neuer Zuschnittversionen, ohne Quelldateien zu ändern.
- mehrfaches Undo/Redo der Objekt-Ersetzung bei unveränderter Stapelreihenfolge.
- Zuschnittvalidierung, Originaldaten im Objekt-/XOPP-Roundtrip und Pixelprüfung
  des gemeinsamen Cairo-Renderers einschließlich Clipping außerhalb des Bildes.
- negative Rich-Text-/Tabellen-/Formelkoordinaten gemeinsam mit PageSurface v1,
  Papierkopie ohne Datenverlust und echter vierseitiger gekachelter PDF-Export.
- lokaler Formelquelltext zusammen mit PDF im XOPP-/Objektroundtrip, ohne TeX als
  Abhängigkeit der Unit-Tests.

GTK-Textprüfung:

- mehrzeilige deutsche Zwischenablage, native Tastatureingabe/Enter, GTK-Unicode-
  Komposition; Formatierung, Listen-/Aufgaben-/Einrückaktionen und deren Undo.
- native formatierte Copy/Paste innerhalb des Editors sowie erzwungenes
  Klartext-Paste; lokale Undo/Redo-Schritte und gespeicherte Stilbereiche.
- Seitenlink speichern und über die echte UI öffnen; Navigation erst nach
  Schließen des Bearbeitungsdialogs.
- unveränderte Bearbeitung verlassen, resultierende Auswahl kopieren/einfügen
  und wieder rückgängig machen (Schutz vor ungültigem Auswahlzeiger).
- Tabellenzellen bearbeiten, Zeilen/Spalten einfügen und löschen, Übernehmen als
  ein Undo-Schritt, speichern, App beenden, neu starten, Zelle erneut bearbeiten.
- tatsächlicher PDF-Export und Prüfung deutscher Zeichen mit `pdftotext`.

Die gesamte Textprüfung wird zusätzlich auf einer freien Seite mit negativer
Kamera und negativen Textkoordinaten ausgeführt; sie prüft auch die Reihenfolge
von Text-/Kamera-Undo. Der vorhandene Oberflächen-GTK-Test wird im integrierten
Stand erneut ausgeführt.

GTK-Medienprüfung:

- Bild auf derselben Seite wie Text, Formel und Stiftstrich auswählen; Breite
  und Zuschnitt ändern, erwartetes Seitenverhältnis/Position im XOPP prüfen,
  Undo/Redo und unveränderte Elementreihenfolge.
- LaTeX mit der echten Standardvorlage und lokalem `pdflatex` kompilieren;
  vorhandenen Quelltext ändern, speichern, mit einem Undo-Schritt wiederherstellen
  und mit Redo erneut anwenden.
- echten LaTeX-Syntaxfehler eingeben: verständlicher Status und Compilerprotokoll,
  gesperrtes OK, Abbrechen erhält die gespeicherte gültige Formel.
- App neu starten, denselben Formelquelltext erneut öffnen, gemeinsame Seite als
  PDF exportieren und Text extrahieren.

Screenshots: `text-format-dialog.png`, `text-table-reopened.png`,
`text-formula-error.png`. Konsolidierte Ergebnisse: `text-media-results.txt`.

## Abgrenzung und offene Abnahme

Während der Entwicklung zeigten Assertions zwei Fehler, die behoben wurden:
XML-Helferfehler mussten sofort an GMarkup propagiert werden (sonst wurde im
selben Parserblock trotz ungültiger Zuschnittversion weitergearbeitet); bei
unverändertem Textabschluss muss die Auswahl das ursprüngliche Objekt statt
eines verworfenen Bearbeitungsklons erhalten. GTK-Unicode-Komposition darf nicht
vom Unterstreichen-Shortcut Strg+Umschalt+U abgefangen werden. Die Prüfung von
GTK-Logs unterscheidet Bibliotheksfehler von den erwarteten ALSA/JACK-Meldungen
im hardwarelosen Container.

`xdotool type` allein lieferte Unicode bei dynamischer X11-Tastaturbelegung nicht
zuverlässig. Die finale Prüfung nutzt echte GTK-Zwischenablage sowie GTK-Unicode-
Komposition und prüft die Inhalte anschließend in Buffer, XOPP und PDF.
Das ersetzt keinen Test deutscher Windows-Tastaturbelegung.

Kein Windows-Compiler/Runner/Hardware in diesem Auftrag, kein neuer Windows-CI-
Lauf und kein Paket-/Release-/Push-Nachweis. Windows 10/11 mit UCRT64, deutsche
Tastatur/AltGr/Tottasten, japanische/chinesische IME mit Kandidatenfenster,
Clipboard aus Office/Browser (Klartext-Fallback), 100/150/200 % DPI, NVDA,
physischer Stift/Touch/Palm-Rejection und Offline-TeX müssen noch geprüft werden.
GTK3- und TeX-Pfade werden erhalten; eine erfolgreiche Linux-Prüfung ist keine
Windows-Abnahme. Produktgrenzen: TEXT-AND-MEDIA.md (insbesondere ein Absatz je
Tabellenzelle, einfache Listenmarker, kein HTML/RTF-Import und Seitenzahlverweise).
