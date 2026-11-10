# Notizbuchverwaltung – tatsächlich ausgeführte Prüfungen

Datum: 2026-11-10. C++-Stand des fertigen Entwicklungszweigs: `038bf789`.
Linux: Ubuntu 24.04 im Container `foliq-build`, GCC 13.3, GTK 3.24.41, GLib 2.80.
Keine Stift-/Audiohardware im Container. Windows-Abnahme unten separat.

## Linux-Build und Unit-Tests

Frischen Ubuntu-24.04-Container mit Repository als `/src` gestartet;
Abhängigkeiten aus BUILD.md installiert, zusätzliche Testwerkzeuge:
`xvfb xauth xdotool python3-gi python3-pyatspi dbus-x11`.
Build als UID 1000, nicht als root:

```sh
cd /src
scripts/foliq/build-linux.sh /tmp/build-foliq
```

Beim ersten Build C++-Fehler am neuen Bildhintergrund-UUID-Bezeichner korrigiert
(Name hatte vorhandene Seiten-ID verdeckt). Anschließend vollständiger
Editorbuild erfolgreich. Nach Änderungen inkrementell:

```sh
cmake --build /tmp/build-foliq --parallel 3
cmake --build /tmp/build-foliq --target test-units --parallel 3
CI=true /tmp/build-foliq/test/test-units \
  --gtest_output=xml:/src/docs/validation/notebooks-linux-tests.xml
```

**100 Tests aus 21 Suites bestanden**, einschließlich acht Katalog-/Speichertests.
Letzter vollständiger Lauf vor reinem Fensterparent-/Streamöffnungs-Guard;
diese zusätzlichen Fehlerpfade anschließend in der tatsächlichen GUI geprüft.
CMake-Konfiguration mit explizitem GLib-2.66-Minimum danach erfolgreich.
Bestehende Upstream-Compilerwarnungen bleiben; keine Warnungsfreiheit behauptet.
`git diff --check` erfolgreich. Generierte POT-Datei zurückgesetzt.

## GTK-Eingabe und Fehlerpfade

Die drei beigefügten Python-Skripte entsprechen den tatsächlich ausgeführten
Testabläufen (zusätzlich nur Lizenz-/Umgebungskommentar). Sie sind **Linux-
Testwerkzeuge**, keine Produktabhängigkeit. Sie verwenden ein ausschließlich
synthetisches Testprofil unter `/tmp/foliq-ui-smoke` im Wegwerfcontainer.
`smoke` einmal auf leerem Testprofil ausführen, danach `locks` und `stage`.
Bei Wiederholung zuerst ausschließlich dieses Testprofil entfernen.
Da der Build außerhalb des Quellbaums liegt, wurden vor dem Start die Ressourcen
im Buildverzeichnis verlinkt:

```sh
ln -s /src/ui /tmp/build-foliq/ui
ln -s /src/resources /tmp/build-foliq/resources
xvfb-run -a -s '-screen 0 1600x1000x24' dbus-run-session -- \
  python3 /src/docs/validation/notebooks-ui-smoke.py
xvfb-run -a -s '-screen 0 1600x1000x24' dbus-run-session -- \
  python3 /src/docs/validation/notebooks-ui-locks.py
xvfb-run -a -s '-screen 0 1600x1000x24' dbus-run-session -- \
  python3 /src/docs/validation/notebooks-ui-stage.py
```

Beobachtet und per Assertion geprüft:

- Hierarchie über GTK-Buttons angelegt, echte XOPP geöffnet, Stiftstrich durch
  Mausereignisse gezeichnet, nach Autosave Strich im dekomprimierten XOPP.
- Duplizieren, Papierkorb und Wiederherstellen über GTK; Katalogzustände geprüft.
- Ordner statt `.bak`: Fehlerdialog, Original bytesgleich, App bleibt lauffähig;
  nach Entfernen und Strg+S geänderte Datei.
- Neustart stellt dieselbe UUID-Datei mit gespeicherten Strichen wieder her.
- Notizbuch schließen, Neustart bleibt geschlossen, Öffnen stellt Seite wieder her.
- Zweite echte App-Instanz zeigt Fehlerdialog und verändert Katalog nicht;
  kein Crash beim Fehler während des Fensteraufbaus.
- Schema 99 gibt sichtbaren Startfehler, Originalversion bleibt unverändert.
- Ordner statt `.saving`: Öffnungsfehler vor Gzip-Schreibzugriff, Original
  bytesgleich, danach erfolgreicher Retry.
- Normaler App-Exit separat explizit mit Rückgabecode 0 geprüft.

Ein im Ablauf gefundener Auswahlverlust nach Tree-Refresh wurde im Produktcode
behoben und der Ablauf wiederholt. Screenshot `notebooks-navigation.png` ist eine
reale Aufnahme nach Neustart mit ausschließlich synthetischer Testnotiz.
Mausereignisse beweisen keine Stiftdruck-/Palm-Rejection-Hardwarefunktion.

## Grenzen

Noch keine physische Windows-GUI-/DPI-/Screenreader-/Stift-/Touch-/Audioabnahme.
Große Dokumente können durch synchrones 5-s-Autosave die UI vorübergehend blockieren.
Kein verteiltes Lockprotokoll für Cloud-Sync-/Netzwerkordner. Backups und verwaiste
Dateien werden bewusst nicht automatisch entfernt. Externe Medienpfade behalten
XOPP-Semantik. Wiederherstellung beschädigter Kataloge erfordert die explizite
Backup-Auswahl gemäß NOTEBOOKS.md. Kein vorangegangenes Katalogschema vorhanden;
Version 0 und neuere Versionen werden abgewiesen statt erraten migriert.

## Windows – abgeschlossener Paketlauf

[Foliq Windows test package, Run 42903859940](https://github.com/leotrax3d/foliq/actions/runs/42903859940)
für `c1e5c012e3289c0b202f7d23ea9aee3f2880c0b7`: **erfolgreich**.
Der Produktcode ist gegenüber 038bf789 identisch; geändert wurde zuletzt der
Quellarchiv-Aufruf im Workflow. **101/101 Tests aus 21 Suites bestanden**,
einschließlich neun Foliq-Tests und echtem Win32-Dateisperrtest. Editor, Tests,
Upstream-NSIS-Paketierung und portable --version-Prüfung erfolgreich.

[Portables Windows-Testpaket](https://github.com/leotrax3d/foliq/actions/runs/42903859940/artifacts/13803863342)
(97.986.430 Bytes; CI-Aufbewahrung bis 2026-11-24). Lokal zusätzlich unter
`/workspace/artifacts/foliq-windows-c1e5c012.zip`. Entpacken und
`dist/bin/xournalpp.exe` starten; DLLs und share-Verzeichnis zusammen lassen.

Das ZIP wurde tatsächlich heruntergeladen und unabhängig geprüft:

- SHA-256 entspricht dem von GitHub veröffentlichten Artefaktdigest:
  `67b2428772948fabad6d168615c7fdfbfc313bac24b640f79aad82a993133de6`.
- ZIP-CRC aller 3.914 Einträge erfolgreich; EXE ist AMD64-PE.
- Beide internen SHA256SUMS-Einträge (EXE und Quellarchiv) stimmen.
- Quellbeilage enthält 1.476 Tar-Einträge; Catalog.cpp, Storage.cpp, SaveJob.cpp,
  LICENSE und AUTHORS bytegenau gegen den gebauten Git-Commit verglichen.
- Lizenzbeilagen, Paket-/Toolchainmanifest und CMakeCache vorhanden.
- Paket-Quellcommit korrekt; Windows-Test-XML enthält 101 Tests ohne Fehler,
  deaktivierte Tests oder Ausnahmen.

Maschinenlesbare Belege: `notebooks-package.json`, `notebooks-windows-tests.xml`.
Der vorangegangene Lauf 42902030779 hatte eine leere Quellbeilage trotz grünem
Workflow. Die unabhängige Inhaltsprüfung hat dies erkannt; korrigierter Workflow
archiviert vom Repository-Root und prüft drei erforderliche Quelldateien per tar.
Das alte Artefakt ist nicht der ausgelieferte Download.

Die Build-Schritte generieren Übersetzungskataloge und können die Versionskennung
als `-dirty` markieren. Maßgeblich sind der beigelegte source-commit und die
überprüften Quellinhalte; kein bitidentischer Release-Build behauptet.

Das Windows-Test-XML ist im Repository auf LF-Zeilenenden normalisiert.
Das unveränderte Original bleibt im heruntergeladenen Diagnostik-ZIP erhalten.
