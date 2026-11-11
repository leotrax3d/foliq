# Windows-Testpaket und manuelle Abnahme

## CI starten und herunterladen

Im Repository leotrax3d/foliq unter Actions `Foliq Windows test package`
öffnen. Der Workflow läuft auf Push/PR und kann über `Run workflow` für einen
gewählten Branch gestartet werden.
Erst ein grüner Lauf mit sichtbarem Artefakt bestätigt Build und Packaging.

Artefakt `foliq-windows-x64-test` herunterladen und vollständig entpacken.
Das Paket enthält den portablen `dist`-Inhalt, exakte Projektquellen desselben
Commits und Prüfsummen. `bin/xournalpp.exe` unterhalb des entpackten Paketbaums
starten; dessen Fenster identifiziert sich als Foliq-Entwicklung auf Xournal++.
Je nach GitHub-Entpackung liegt bin direkt an der Wurzel oder unter dist.
DLLs und share-Verzeichnis müssen neben bin erhalten bleiben. Kein Installer
oder Release-Signatur wird als Foliq-Release angeboten. Der Upstream-Installer
wird intern erzeugt/geprüft, ist aber nicht Teil des Testdownloads.

Foliq verwendet eigene Profilordner und eine eigene GTK-App-ID. Für den
Entwicklungstest eine separate Bibliothek oder VM verwenden; der Binaryname und
die XOPP-Dateiendung bleiben unverändert.
Testversion ist unsigniert; keine Systemschutzfunktionen abschalten.

## Kurzer Testlauf (Windows 10/11 x64)

1. Start ohne Internet/Konto; Foliq-Titel, Werkzeugleisten und Seitenansicht prüfen.
2. Text mit Umlauten, Handschrift mit Druckstufen, Radierer, Auswahl und Bild auf
   derselben Seite erstellen. Undo/Redo mehrfach prüfen.
3. XOPP speichern, schließen, neu öffnen; Text, Striche und Bild vergleichen.
   Kopie in unverändertem Xournal++ 1.2.8 öffnen und Kompatibilität prüfen.
4. Mehrseitiges PDF annotieren, als XOPP speichern und als PDF exportieren;
   zweite Seite, Zoom und Druckvorschau prüfen.
5. Audio aufnehmen/abspielen mit Berechtigung, wechselndem Mikrofon und Kopfhörer.
6. Bei 100/150/200% DPI sowie Stift/Touch: Latenz, Handballenunterdrückung,
   Radierertaste, Fensterwechsel und Scrollen prüfen. Während Texteingabe und
   Dateinamen-Eingabe mehrfach per Alt+Shift das Tastaturlayout wechseln
   (bekanntes Upstream-Risiko #6315).
7. Offline-Neustart, Autosave/Wiederherstellung mit Testdaten prüfen.

Ergebnis mit Windows-Version, Hardware, Stift/Grafiktreiber, Paket-Commit,
Reproduktionsschritten und erwartetem/tatsächlichem Verhalten protokollieren.
Notizbücher sind implementiert (Testablauf unten). Rich Text, OCR,
Verschlüsselung und Auto-Grow werden in diesem Paket noch nicht erwartet. Neue Features erst nach ROADMAP-Abnahme freigeben.

## Lokal bauen

MSYS2 UCRT64 und dieselben Pakete wie in
`.github/actions/install_deps_windows/action.yml` installieren (einschließlich
des in BUILD.md beschriebenen deaktivierten Legacy-GTK-Downgrades). Im UCRT64-Terminal:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DFOLIQ_BUILD=ON -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=ON
cmake --build build --target pot translations
cmake --build build
cmake --build build --target test-units
CI=true build/test/test-units.exe
(cd build && bash ../windows-setup/package.sh)
```

Packaging benötigt NSIS am im Upstream-Skript angegebenen Windows-Pfad.
Heruntergeladene Build-Abhängigkeiten sind vom Offline-Betrieb der App zu trennen.

## Lokale Notizbuchverwaltung testen

Portable Paket entpacken und `bin/xournalpp.exe` starten. Es gibt keine
vorinstallierten Beispielnotizen. Optional in PowerShell vor dem Start:

```powershell
$env:FOLIQ_LIBRARY = "$env:USERPROFILE\Foliq-Testbibliothek"
.\bin\xournalpp.exe
```

1. Notizbuch, zwei verschachtelte Gruppen, Abschnitt, Seite und Unterseite
   anlegen. Namen wie `CON: Test?`, Unicode und abschließenden Punkt versuchen.
2. Seite mit Doppelklick öffnen, zeichnen, fünf Sekunden warten. App beenden,
   neu starten: Dokumentinhalt und letzte Position müssen wieder erscheinen.
3. Abschnitt verschieben, Einträge mit Pfeilen sortieren, Seite umbenennen.
   UUID-Dateiname und bestehende Unterseitenverweise müssen gleich bleiben.
4. Teilbaum duplizieren (auch mit Bildhintergrund), Inhalt prüfen. Original
   löschen: nur im Papierkorb sichtbar; wiederherstellen und öffnen.
5. Aktives Notizbuch schließen, neu starten: es bleibt geschlossen. Öffnen und
   Seite erneut aufrufen. Eine zweite App-Instanz muss einen sichtbaren
   Katalog-Sperrfehler anzeigen; die erste darf weiter speichern.
6. App beenden, eine Kopie der Bibliothek anlegen. Für einen echten Fehlerfall
   einen Ordner `documents/<Seiten-ID>.xopp.bak` erzeugen (vorherige Sicherung
   dafür außerhalb aufbewahren). Seite bearbeiten: Autosave muss einen Fehler
   melden, Original bleibt unverändert, Editor bleibt geändert. Ordner entfernen
   und explizit speichern: Inhalt wird erfolgreich übernommen. Anschließend
   denselben Test mit einem Ordner `<Seiten-ID>.xopp.saving` wiederholen; schon
   das Öffnen des Kompressionsziels muss als Fehler behandelt werden.
7. Separat Datei mit exklusivem Windows-Handle öffnen und Speichern versuchen.
   Der Windows-GTest `WindowsLockedTargetKeepsOriginal` prüft diesen Fall ohne UI.
8. Mit verwalteter Seite im Editor die XOPP-Datei extern durch eine andere gültige
   Revision ersetzen: Speichern muss einen Konflikt melden. Änderungen über
   „Speichern unter“ retten, Original nicht überschreiben.

Automatische Tests sind kein Windows-Stift-/DPI-/Screenreader-/Hardwaretest.

## Neue Hauptoberfläche prüfen

Diese Matrix gilt für den neuen Quellstand, nicht für das ältere verlinkte Paket.
Nach einem neuen Windows-Build Paket-Commit und Windows-/GTK-Version protokollieren.

1. Zwei Abschnitte mit Seiten/Unterseiten erstellen. Abschnitt links wählen;
   daneben nur dessen Seiten. Links/Rechts klappt Unterseiten ein/aus;
   Eingabe öffnet sie. F2, Tab, Shift+Tab und sichtbaren Fokus prüfen.
2. Seiten und Abschnitte an Zeilenkanten sortieren, in Zeilenmitte in andere
   gültige Eltern verschieben. Unterseiten bleiben erhalten. Zyklus und falscher
   Knotentyp müssen abgewiesen werden. Dieselben Ziele über Aktionen → Verschieben
   und Reihenfolge über Alt+↑/↓ prüfen; Neustart muss die Änderung behalten.
3. Beide Spaltengrenzen ändern. Fokus ein-/ausschalten; Hell, Dunkel, System
   wählen. Werkzeugreiter und eingeklappte Unterseiten ändern, regulär beenden
   und neu starten: Ansichtsoptionen wiederhergestellt. Unbekannte view.ini-
   Version nur in einer Testkopie prüfen: sichtbarer Fehler, kein Überschreiben.
4. Fenster bei 800×600 und 1280×800 sowie 100/150/200 % Windows-Skalierung
   prüfen. Sehr lange Unicode-Titel dürfen die Schreibfläche nicht verdrängen.
   Werkzeuggruppen müssen horizontal erreichbar bleiben. Fokusmodus bei kleinem
   Fenster prüfen. Beide Trennleisten auch mit Tastatur bedienen.
5. Alle vier Werkzeugreiter bedienen; „Zeichnen → Weitere Werkzeuge“ und
   „Ansicht → Alle Werkzeuge“ müssen die bisherigen Werkzeugleisten mit Farben,
   Strichstärken und Spezialwerkzeugen ein-/ausblenden; Dokument-Suche, Undo/Redo und tatsächlichen
   Speicherstatus prüfen. Stift/Radierer/Druck, PDF-Anmerkungen, Spezialwerkzeuge,
   Export und Autosave erneut mit der bestehenden Matrix oben prüfen.
6. NVDA: Namen der Bäume, Farbschema, Aktionsmenüs, Auswahl und Fokus prüfen.
   Linux-ATK-Inspektion ersetzt diese Windows-Screenreader-Abnahme nicht.
