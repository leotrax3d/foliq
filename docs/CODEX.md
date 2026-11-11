# Wiederverwendbare Codex-Umgebung für Foliq

Ziel ist eine vorbereitete Linux-Entwicklungsumgebung mit Repository, Compiler,
GTK und Tests. Die Anwendung bleibt lokal; ein laufender Web-/MCP-/SSH-Server
ist dafür nicht erforderlich. Projektwissen und Übergaben stehen im Git-Repository,
nicht ausschließlich im Verlauf eines früheren Chats.

## Einmal in Codex speichern

In Codex → Umgebungen/Einstellungen eine Umgebung **Foliq** für
`leotrax3d/foliq`, Branch `main`, anlegen. Ein Ubuntu-24.04-kompatibles Image mit
APT verwenden. Im Setup-Schritt Paketinstallation mit Root-Rechten oder dem
vorgesehenen Paketinstaller erlauben. Der Build läuft anschließend als Eigentümer
des Checkouts. Setup-Skript (im Repository-Verzeichnis):

```sh
bash scripts/foliq/setup-codex.sh
```

Falls das Setup-Feld im Workspace-Verzeichnis statt im Repository startet,
zuerst in den von Codex angelegten Foliq-Checkout wechseln; keine feste
`/workspace/foliq`-Adresse für alle zukünftigen Umgebungen voraussetzen.

Für Wartung/Wiederaufnahme ist dieser Befehl geeignet, sofern die Oberfläche
ein entsprechendes Feld anbietet:

```sh
bash scripts/foliq/setup-codex.sh --check
```

Umgebung nach erfolgreichem Setup **speichern/veröffentlichen**. In neuen Chats
wieder die gespeicherte Umgebung und das Repository auswählen. Ein Snapshot
beschleunigt den Start; er ersetzt weder Git-Push noch eine aktuelle Übergabe.
Diese Repository-Dateien erstellen keine Plattformressource von allein.

## Inhalt der Einrichtung

- C++17, CMake/Ninja, die vorhandenen GTK3-/GLib-/Poppler-/Audio-/Lua-/ZIP-
  Entwicklungsbibliotheken und System-GoogleTest; kein GTest-Netzdownload.
- Xvfb, AT-SPI, Python-GI und xdotool für echte GTK-Tests ohne physischen Bildschirm.
- Lokaler Git-Author und Committer: leotrax3d@gmx.net gemäß AGENTS.md.
- Inkrementeller Build unter `build-codex/` (Git-ignoriert), Editor und Unit-Tests.
  Ergebnisse unter `build-codex/test-results.xml`. Es werden gezielt die Ziele
  `xournalpp` und `test-units` gebaut; versionierte Übersetzungen bleiben unberührt.
- Keine Zugangsdaten im Repository, keine geöffneten Serverports und keine
  automatische Veröffentlichung von Änderungen oder Nutzernotizen.

Setup benötigt Zugriff auf die für das Image konfigurierten Ubuntu-/Debian-
Paketquellen. GitHub-Zugriff für Checkout/Push über die Codex-GitHub-Verbindung
bereitstellen. Vorhandene Proxy-/CA-Einstellungen beibehalten. Keine zusätzlichen
Laufzeitbibliotheken; die Zusatzwerkzeuge dienen nur Entwicklung und Tests.
Paketversionen werden durch die gewählten APT-Quellen bestimmt; dies ist kein
bitidentisches Image. Der ältere Referenz-Lock/Dockerfile unter scripts/foliq
bleibt ein getrenntes Verfahren.

## Weiterarbeiten in einem neuen Chat

AGENTS.md sowie PRODUCT, ARCHITECTURE, ROADMAP, UPSTREAM und HANDOFF lesen.
Danach tatsächlichen Checkout und Branch prüfen:

```sh
git status -sb
git fetch origin
bash scripts/foliq/setup-codex.sh --check
```

Den Checkout nur mit sauberem Arbeitsbaum passend aktualisieren. Für Änderungen
konfigurieren/bauen/testen, ohne erneut Systempakete zu installieren:

```sh
bash scripts/foliq/setup-codex.sh --build
```

Nur konfigurieren: `--configure`. Parallelität über `FOLIQ_BUILD_JOBS` (Standard 2),
abweichenden Cachepfad über `FOLIQ_CODEX_BUILD_DIR` wählen. Der Cache muss zum
aktuellen Checkout passen. Bei abweichenden Workspacepfaden einen neuen Build-
Ordner wählen statt fremde Caches zu überschreiben. Zum Start im geteilten Desktop:

```sh
FOLIQ_LIBRARY=/tmp/foliq-codex-library build-codex/xournalpp
```

Ein Desktop/Display muss dafür aktiv sein. Headless `--version` prüft das Laden
der Bibliotheken, ersetzt aber keinen GTK-Eingabetest. Vollständige GUI-Testabläufe
und ihre isolierten Testdaten: validation/NOTEBOOK-VALIDATION.md. Windows-Pakete
weiterhin über `.github/workflows/foliq-windows.yml` bauen; Linux ist kein
Windows-Hardwaretest.

Geeigneter Startauftrag für neue Chats:

> Arbeite im Foliq-Repository weiter. Lies AGENTS.md und die dort genannten
> Dokumente, insbesondere den neuesten HANDOFF. Prüfe Code, Git-Stand und
> `scripts/foliq/setup-codex.sh --check`. Nutze den vorhandenen GTK3/C++-Editor,
> teste die konkrete Änderung und aktualisiere HANDOFF. Aufgabe: …
