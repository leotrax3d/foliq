# Seitentypen – ausgeführte Prüfungen am 2026-11-11

Basis: e94b6344 einschließlich aktueller GTK-Hauptoberfläche; neue Seitentypänderungen
im Arbeitsbaum. Keine Windows-/Stift-/Touch-/Latenz-Hardwareprüfung. Alle Notizen
und Bilder sind synthetisch. Keine neue Laufzeitbibliothek.

## Umgebung und reproduzierbare Kommandos

Host Debian 13, UID 1000, keine lokalen CMake-/GTK-Dev-Pakete und kein sudo.
Managed Docker 29.9.2 verfügbar. Frischer Ubuntu-24.04-Container `foliq-pages`,
Image-Digest `sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852`.
Vorhandenes `scripts/foliq/setup-codex.sh --setup` installiert die bestehenden
Abhängigkeiten und wechselt auf UID 1000. GTK 3.24.41, GLib 2.80.0, GCC 13.3.
Clang-format 18 wurde ausschließlich als Entwicklungswerkzeug nachinstalliert.

```sh
docker run -d --name foliq-pages -v "$PWD:/src" -w /src ubuntu:24.04 sleep infinity
docker exec foliq-pages bash scripts/foliq/setup-codex.sh --setup
docker exec -u 1000 foliq-pages bash scripts/foliq/setup-codex.sh --build

docker exec -u 1000 foliq-pages xvfb-run -a \
  -s '-screen 0 1400x1000x24' dbus-run-session \
  /usr/bin/python3 /src/docs/validation/surfaces-ui-smoke.py
```

Der GTK-Test erstellt einen eigenen zufälligen `/tmp/foliq-surfaces-ui-*`-Ordner
und startet die echte Anwendung mit getrenntem Profil/Katalog. Er arbeitet über
AT-SPI und xdotool. `save()` liest anschließend die tatsächlich geschriebene
XOPP-Datei unabhängig mit Python/Gzip/XML. Normales Beenden über Strg+Q,
Neustart mit demselben Profil und erneute Inhaltsprüfung. Es werden weder echte
Stiftgeräte noch echte Druckereignisse simuliert.

## Unit- und Exporttests

**116/116 Tests aus 22 Suites bestanden**, darunter 16 PageSurface-Tests.
Vollständiger Editorbuild und `xournalpp --version` erfolgreich. Test-XML:
[surfaces-linux-tests.xml](surfaces-linux-tests.xml). Vorhandene Upstream-
Compilerwarnungen bleiben; keine pauschale Warnungsfreiheit behauptet.

Neue Tests prüfen:

- Welt-/Bildschirmkoordinaten und Zoomanker in positiven/negativen Quadranten,
  unterschiedliche Zoomstufen sowie ungültigen Zoom und numerische Grenzen.
- Ausschließlich negative Stroke-Bounding-Boxes, verlustarme kopierende
  Umwandlung mit Überlauf und erhaltenem Segmentdruck, vollständigen Settings-
  Undo-Swap und Pan-History-Zusammenfassung.
- XOPP-Roundtrip inklusive Kamera, Ausschnitt und Drucksegmenten, Legacy-Papier
  ohne Erweiterung, unbekannte Versionen, fehlende Attribute, NaN, Zahlen mit
  Restzeichen und ungültige Bool-Werte. Fehlerfälle erhalten die Eingabebytes.
- Lazy PDF-Seitenteilung mit negativem Ursprung, flächentreue Kachelabdeckung,
  Größen-/Seitenlimits und unveränderte bestehende Ausgabe bei Vorprüfungsfehler.
- Echter Cairo-PDF-Export: Poppler prüft sechs Teilseiten einschließlich
  verkürzter Randseite und sichtbarer negativer Striche; gemischte Papier-/Frei-
  Ausgabe mit ungeteiltem Ausschnitt und korrekten Seitengrößen.
- Bounded Rendering einschließlich eines gefüllten Textmarkers und eines
  Strichs, die von −900.000 bis +900.000 pt reichen. Sichtbare Pixel werden in
  einer 64×64-Ausgabe bei Weltursprung (500.000,500.000) geprüft. Zusätzlich
  wird die Unterdrückung des Hintergrunds beim Export geprüft.

## Native GTK-Prüfung

`surfaces-ui-smoke.py` wurde mit Exit 0 ausgeführt. Ergebnisprotokoll:
[surfaces-ui-results.txt](surfaces-ui-results.txt). Bestanden:

- Echter Mausstrich mit negativen X/Y, Speichern und Undo/Redo.
- Lasso, Verschieben und Skalieren in negativen Koordinaten, jeweiliges Undo/Redo
  anhand gespeicherter Vektoren. Speichern finalisiert die aktive Auswahl; für
  den folgenden Skalierungsschritt wird deshalb erneut ausgewählt.
- Textmarker und Radierer, gespeicherte Änderungen und exakte Wiederherstellung
  über Undo/Redo.
- Hand-Schwenken in negativen Raum mit Kamera-Undo/Redo.
- Reales Strg+Mausrad: Zoom/Kamera-Undo/Redo und gleicher Weltpunkt unter dem
  Mauszeiger (Toleranz unter zwei Dokumentpunkten wegen ganzzahliger GTK-Projektion).
- GTK-Dialog: freie Seite in Papierkopie umwandeln; Originalkoordinaten erhalten,
  Kopie übersetzt, Kopie per Undo entfernen und per Redo wiederherstellen.
- Papiermaße ändern, Undo/Redo, regulär beenden (Code 0), neu starten und beide
  Seitentypen, Kamera und Strichdaten erneut prüfen.

Screenshots: [freie Fläche](surfaces-free.png), [Dialog](surfaces-dialog.png),
[Auswahl](surfaces-selection.png). Keine Hardwareabnahme aus diesen Bildern ableiten.

Die Prüfungen fanden echte Fehler: rekursive Werkzeugumschaltung beim
Synchronisieren der neuen Ribbon-Knöpfe, falsche negative Stroke-Maxima,
flächenproportionale Masken/Selektionsvorschauen und Cairos interne Rastergrenze
bei sehr breiten gefüllten Pfaden. Außerdem wurden Kameraverschiebung beim
Wiederherstellen gespeicherter Zoom-Metadaten und der Lockübergang bei Zoom-Undo
korrigiert. Die entsprechenden Regressionen sind abgedeckt.

## Begrenzter Rendering-Stresstest

Zwei oben beschriebene große Renderfälle jeweils 200-mal im selben Testprozess
wiederholt; insgesamt **400 erfolgreiche Testausführungen**. Ausgabefläche immer
64×64 Pixel, Weltkoordinaten bis ±900.000 pt. Rohresultat:
[surfaces-render-stress.json](surfaces-render-stress.json).

Gemessen im Cloud-Container: **0,068 s**, maximaler Prozess-RSS **33.608 KiB**
über `resource.getrusage(RUSAGE_CHILDREN)`. Das ist ein enger synthetischer
Renderingtest samt Prozessinitialisierung, kein Benchmark für die komplette GUI,
10.000 reale Striche, physische Stiftlatenz oder Windows-Speicherverwaltung.
Der Code allokiert keine Puffer proportional zu den leeren Weltgrenzen; die
Traversal-Kosten bleiben proportional zur Zahl gespeicherter Elemente.

## Verbleibende Grenzen

Windows-/DPI-/NVDA-/Treiber-/Druck-/Palm-/Touch-/Seitentasten-/Latenzabnahme
nach WINDOWS.md ist offen. Bestehende Formwerkzeuge bleiben im gemeinsamen
Stroke-/Undo-Pfad; nicht jede einzelne Form und jedes Geräte-Mapping wurde
per GTK-Automation durchgespielt. PDF-/Bildhintergründe bleiben endlich am Ursprung;
freie Hintergründe sind Blanko/Linien/Raster. Große temporäre Auswahlvorschauen
können weich werden. Kein räumlicher Elementindex; große Inhaltsmengen benötigen
weiter O(Elementzahl) Traversierung. Numerische Grenzen, PDF-Outlines, direkte
Druckumleitung und Altleser-Kompatibilität sind in SURFACES.md und im Dialog erklärt.

Kein neuer Windows-CI-Lauf und kein neues Windows-Paket in diesem Auftrag.
