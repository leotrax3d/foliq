# Windows-Testpaket und manuelle Abnahme

## CI starten und herunterladen

Dieses lokale Repository hat zunächst nur den Upstream-Remote. Keine Schreibrechte
auf Upstream annehmen. In ein eigenes GitHub-Repository pushen; Actions aktivieren,
`Foliq Windows test package` öffnen und gegebenenfalls `Run workflow` ausführen.
Erst ein grüner Lauf mit sichtbarem Artefakt bestätigt Build und Packaging.

Artefakt `foliq-windows-x64-test` herunterladen und vollständig entpacken.
Das Paket enthält den portablen `dist`-Inhalt, exakte Projektquellen desselben
Commits und Prüfsummen. `bin/xournalpp.exe` unterhalb des entpackten Paketbaums
starten; dessen Fenster identifiziert sich als Foliq-Entwicklung auf Xournal++.
Je nach GitHub-Entpackung liegt bin direkt an der Wurzel oder unter dist.
DLLs und share-Verzeichnis müssen neben bin erhalten bleiben. Kein Installer
oder Release-Signatur wird als Foliq-Release angeboten. Der Upstream-Installer
wird intern erzeugt/geprüft, ist aber nicht Teil des Testdownloads.

Nur in einer VM oder einem separaten Windows-Benutzerprofil ohne produktive
Xournal++-Daten testen: Konfiguration und Dateitypen sind noch nicht isoliert.
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
Notizbücher, Rich Text, OCR, Verschlüsselung und Auto-Grow werden in diesem
Paket noch nicht erwartet. Neue Features erst nach ROADMAP-Abnahme freigeben.

## Lokal bauen

MSYS2 MINGW64 und dieselben Pakete wie in
`.github/actions/install_deps_windows/action.yml` installieren (einschließlich
des in BUILD.md beschriebenen deaktivierten Legacy-GTK-Downgrades). Im MINGW64-Terminal:

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
