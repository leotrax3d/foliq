# Build-Basis

Quellbasis und Commit: [UPSTREAM.md](UPSTREAM.md). Build verwendet weiterhin
Upstream-CMake, Ninja und dieselben Bibliotheken; FOLIQ_BUILD aktiviert die
Notizbuchnavigation, Fensterkennung und getrennten Profilordner. `-DFOLIQ_BUILD=OFF` erhält den Xournal++-Titel. Exe und XOPP-Format
werden nicht umbenannt. Keine Laufzeitbibliothek wurde ergänzt.

## Linux-Referenz (Ubuntu 24.04, x86_64)

```sh
apt-get update
apt-get install -y --no-install-recommends build-essential cmake ninja-build \
  pkg-config librsvg2-dev libgtk-3-dev libpoppler-glib-dev portaudio19-dev \
  libsndfile1-dev liblua5.3-dev libzip-dev gettext help2man libgtest-dev \
  libgtksourceview-4-dev git ca-certificates
./scripts/foliq/build-linux.sh
```

System-GoogleTest vermeidet Downloads durch CMake. Der Helper baut Übersetzungen,
Editor und Tests, und bricht bei Fehler ab. Headless-Tests sind keine GUI-Abnahme.
Eine unveränderte Basis separat mit `git archive v1.2.8` auspacken und dieselben
CMake-Kommandos ohne FOLIQ_BUILD verwenden. Ein Linux-Erfolg beweist keinen
Windows-Erfolg.

Der erste Referenzcontainer verwendet
`ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852`.
Tatsächlich ermittelte Paketversionen/Toolchain und Ergebnisse stehen unter
`docs/validation/` und HANDOFF. Paketquellen sind beweglich: dieses Verfahren ist
wiederholbar, aber noch kein bitidentischer Build mit dauerhaft archiviertem
Dependency-Snapshot. Vor Release Paketarchive, Compiler, Actions und MSYS2-
Repository auf geprüfte unveränderliche Versionen festschreiben.

## Windows

Siehe [WINDOWS.md](WINDOWS.md). `.github/workflows/foliq-windows.yml` läuft auf
Push/PR aller Branches und manuell. Er verwendet die vorhandenen Composite-
Actions und `windows-setup/package.sh`. Die alten Upstream-Workflows bleiben
erhalten. Der alte GTK-Downgrade auf 3.24.43-1 wegen Upstream #6315 ist
für Foliq deaktiviert: sein Paket-URL liefert am 2026-11-10 HTTP 404. Die
Composite-Action hat dafür einen optionalen Schalter; bestehende Upstream-Aufrufer
behalten ihr Verhalten. Auch aktuelles Upstream-master verwendet keinen solchen
Downgrade mehr. Foliq baut mit dem aktuell angebotenen GTK3; #6315 ist weiterhin
offen, ein behobener Tastaturlayout-Wechsel wird nicht behauptet und muss getestet
werden. Verfügbarkeit und Kompatibilität sind erst im Windows-Lauf bestätigt. MSYS2-Paketmanifest, CMakeCache, Toolchainversion und Commit werden
mitgeliefert. CI nutzt derzeit bewegliche Actions/MSYS2-Quellen; kein hermetischer
oder bitreproduzierbarer Windows-Build zugesagt.

## Fixierte Linux-Paketversionen

`docs/validation/linux-packages.lock` enthält den tatsächlich installierten
vollständigen dpkg-Versionssatz. `scripts/foliq/Dockerfile` verwendet den festen
Ubuntu-Image-Digest und verlangt genau diese Versionen (kein stiller Wechsel
auf neuere Pakete):

```sh
docker build -f scripts/foliq/Dockerfile -t foliq-build .
docker run --rm -v "$PWD:/src" foliq-build
```

Der Referenzlauf wurde durch Installation im Container durchgeführt; der
Dockerfile-Neuaufbau selbst ist separat noch zu verifizieren. Solange die
Paketversionen in den Ubuntu-Repositories verfügbar sind, lässt sich derselbe
Versionssatz erneut installieren. Zur langfristigen Reproduktion müssen die
DEB-Dateien oder ein Repository-Snapshot zusätzlich archiviert werden. Der
Lock dokumentiert auch Basispakete, nicht nur direkte Build-Abhängigkeiten.

## Erster tatsächlicher Windows-CI-Lauf

Run 42897015289 scheiterte bei MSYS2 vor dem Compiler: Das MINGW64-Paket
mingw-w64-x86_64-gtksourceview4 ist nicht mehr verfügbar, und MSYS2 markiert
MINGW64 als veraltet. Der Foliq-Workflow verwendet deshalb UCRT64 mit
ucrt-x86_64-Paketen. Das benötigte GtkSourceView4-Paket ist für UCRT64 im
MSYS2-Katalog vorhanden (4.8.4-3, am 2026-11-10 geprüft). Bibliothek und
GTK3-Editor bleiben gleich; Paketierung nutzt das MSYSTEM_PREFIX des bestehenden
Upstream-Skripts. Lizenzkopien werden aus /ucrt64 übernommen.
Der vollständige UCRT64-Build ist erst nach tatsächlichem CI-Erfolg bestätigt.
