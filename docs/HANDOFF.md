# Übergabe – 2026-11-10

## Schritt 1: Repository und Untersuchung

Leeren Arbeitsbereich geprüft; keine vorhandenen Projektdateien/Anweisungen.
Xournal++ v1.2.8 (`ce753d2e0c48a7026fe1fbe59f132586b2217296`) flach geklont,
Branch foliq erstellt; origin zeigt weiterhin auf Upstream. Herkunft, tatsächliche
Mischlizenzen und Verteilungspflichten in UPSTREAM dokumentiert. LICENSE,
AUTHORS, copyright.txt und bestehende Implementierung erhalten.

GTK/Control/Document/InputContext/PageView/Cairo/Pango, Textmodell,
XOPP-Speicherpfad und Windows-Build/Tests am Code untersucht. ADRs halten Stack,
Kataloggrenze, inkrementellen Rich Text und endliches Auto-Grow fest.
Produktumfang und überprüfbare Meilensteine sind dokumentiert.

## Schritt 2: Konkrete Änderungen

- AGENTS.md verankert Erhalt, ehrliche Funktionsangaben, versionierte Formate mit
  Migrationen, Bibliotheksprüfung und fortlaufende Übergabe.
- CMake-Option FOLIQ_BUILD (Standard ON), generierte Fensterkennung und Control-
  Titel identifizieren den laufenden Fork. OFF erhält den Upstream-Titel.
- README verweist auf Foliq-Dokumentation, originale Upstream-Anleitung bleibt.
- Vollständige GPLv3-/LGPL-2.1-Texte als zusätzliche Lizenzbeilage aufgenommen.
- scripts/foliq/build-linux.sh verwendet bestehende CMake-Ziele, lokale GTests
  und XML-Ergebnisse. Keine zusätzliche Laufzeitbibliothek.
- foliq-windows.yml: Push/PR/manuell, Windows-2022/MINGW64, Upstream-Dependency-
  und Build-Actions, Unit-Tests, Upstream-Packaging und portable Testartefakte
  mit passenden Projektquellen, Lizenzen und Build-Metadaten.

## Tatsächlich ausgeführte Prüfungen

Ausgeführt: `git diff --check` erfolgreich; `bash -n` für Buildhelper erfolgreich;
PyYAML 6.0.3 parst neuen Workflow erfolgreich (7 Schritte, windows-2022).
Dies ist keine GitHub-Actions-Laufzeitvalidierung.

Host: CMake fehlt; apt-get update scheitert ohne Installationsrechte. Keine
Berechtigungen geändert. Managed Docker-Daemon geprüft (29.9.2), Ubuntu-24.04-
Container `foliq-baseline` mit festgehaltenem Image-Digest verwendet.
Abhängigkeiten erfolgreich installiert; vollständiger Lock mit 494 Paketen und
Toolchainmanifest unter docs/validation. Installationssimulation des Locks
erfolgreich (0 Änderungen). Dockerfile-Neuaufbau selbst nicht ausgeführt.

1. Unverändertes `git archive v1.2.8` separat nach `/tmp/xournalpp-upstream`
   extrahiert. CMake/Ninja RelWithDebInfo, ENABLE_GTEST=ON,
   DOWNLOAD_GTEST=OFF; Ziele pot, translations, Standardbuild und test-units
   erfolgreich. **92 Tests aus 20 Suites bestanden**, 0 deaktiviert/fehlgeschlagen.
   `xournalpp --version` erfolgreich (1.2.8, GTK 3.24.41).
2. Danach die drei geänderten CMake/C++-Dateien in denselben temporären
   Quellbaum kopiert; Buildhelper dort mit bestehendem Buildcache und
   FOLIQ_BUILD=ON ausgeführt. Editor und Tests erfolgreich gebaut;
   **erneut 92/92 Tests bestanden**, CLI-Start erfolgreich. Generierter
   FOLIQ_WINDOW_NAME enthält die Foliq-Entwicklungskennung.
3. Separater CMake-Konfigurationslauf mit FOLIQ_BUILD=OFF erfolgreich;
   generierter Titel exakt Xournal++. Dieser OFF-Lauf wurde nicht kompiliert.

XML-Belege: docs/validation/upstream-tests.xml und foliq-tests.xml. Toolchain,
Paketversionen, genaue Kommandos und Hashes: docs/validation/README.md.
Compilerwarnungen im unveränderten Upstream (u.a. Konversionen, deprecated
LegacyRedrawable); keine Buildfehler. Keine neue Warnungsfreiheit behauptet.
Temporäre vollständige Logs unter /workspace/scratch/foliq-*.log; Build liegt
im laufenden Container unter /tmp/build-reference. Repository-Quellen wurden
nicht durch Übersetzungs-Buildziele verändert, weil im temporären Baum gebaut.

## Schritt 3: Windows-Buildpfad nachgeprüft

HTTP-Prüfung des Upstream-GTK-3.24.43-1-Paketlinks ergibt 404. Issue #6315
(Tastaturlayoutwechsel) ist offen; aktuelles Upstream-master installiert GTK ohne
Downgrade. Optionalen gtk_compatibility_downgrade-Schalter zur bestehenden
Composite-Action ergänzt (Standard true für unveränderte alte Aufrufer), für
Foliq auf false gesetzt. Foliq verwendet verfügbares GTK3; Keyboard-Regression
ist nicht als behoben bewertet und explizit Teil der Windows-Testanleitung.

## Offene Grenzen

Beim ersten Buildschritt war kein eigenes authentifiziertes GitHub-Ziel verfügbar.
Im Folgeschritt wurde leotrax3d/foliq gefunden und der Zugriff bestätigt; dort
liegen bisher die UI-Referenzen, noch nicht der lokale Code/Workflow.
Windows-CI wurde nicht gestartet, kein Windows-Artefakt erzeugt.
Kein Windows-Runner lokal verfügbar; Workflow erst nach Code-Push ausführbar.
Keine Windows-GUI-/DPI-/Stift-/Touch-/Audio-Hardwaretests ausgeführt.
Bewegliche MSYS2-/APT-/Actions-Versionen verhindern derzeit bitidentische Builds.
Vor öffentlichem Release vollständige transitive Lizenz-/Quellbereitstellung
und Dependency-Snapshots prüfen. Konfiguration/App-ID noch nicht isoliert.

Die neue Notizbuchhierarchie, Rich Text, Auto-Grow, Tabellen/Listen, Anhänge,
Tags/Aufgaben, globale Suche, OCR, Handschrift-zu-Text und Passwortschutz sind
noch nicht implementiert. Übernommene Upstream-Funktionen sind keine Abnahme
auf dieser Hardware. Kein Frameworkwechsel und kein zweiter Editor.

## Nächster Schritt

M0: Windows-Workflow im eigenen GitHub-Repository ausführen; Artefakt nach WINDOWS.md auf isoliertem
Windows-Profil testen. Anschließend M1: versionierten Katalogkern mit Roundtrip,
Zyklus-/Pfadvalidierung und atomarer Speicherung implementieren, dann in die
bestehende GTK-Sidebar integrieren. Vor Installation App-ID/Profile isolieren.

## Veränderte Dateien gegenüber v1.2.8

Bestehend: CMakeLists.txt, src/config.h.in, src/core/control/Control.cpp,
README.md, .github/actions/install_deps_windows/action.yml.
Neu: AGENTS.md, .github/workflows/foliq-windows.yml,
scripts/foliq/build-linux.sh, scripts/foliq/Dockerfile;
docs/PRODUCT.md, ARCHITECTURE.md, ROADMAP.md, HANDOFF.md, UPSTREAM.md,
BUILD.md, WINDOWS.md; docs/licenses/{GPL-3.0,LGPL-2.1}.txt;
docs/validation/README.md, linux-packages.lock, linux-toolchain.txt und beide
Test-XML-Dateien. Keine XOPP-Formatänderung; keine Migration erforderlich.

## Schritt 4: UI-Referenzen übernommen und auf GitHub hochgeladen

Auf Nutzeranweisung alle fünf PNG-Anhänge unverändert unter
`docs/design-references/{2731,2732,2733,2734,2735}.png` abgelegt. README beschreibt
Werkzeuggruppen, farbige Abschnittsreiter, rechte Seitenliste und Fokusmodus als
Orientierung für die spätere GTK-UI. PRODUCT verweist auf die Referenzen.
Keine Oberfläche oder neue Funktion wurde in diesem Schritt implementiert.

Über den verbundenen GitHub-Zugang das private Repository leotrax3d/foliq mit
Schreibrecht gefunden. Alle fünf PNGs und das Referenz-README auf dessen main
hochgeladen: https://github.com/leotrax3d/foliq/tree/main/docs/design-references
Der lokale Upstream-Remote wurde nicht verändert. Der lokale Implementierungs-
und Dokumentationsstand aus Schritt 1–3 wurde dabei nicht mit hochgeladen.

Prüfungen: fünf lokale Kopien per SHA-256 mit Anhängen verglichen; alle sechs
hochgeladenen Dateien anhand der zurückgegebenen Git-Blob-Hashes mit den lokalen
Bytes verglichen. Dokumentationslinks und git diff --check erfolgreich.
Kein neuer Build erforderlich: nur Referenzbilder und Dokumentation geändert.
Nächster Schritt: Code/Workflow in dasselbe Repository integrieren, bestehende
Remote-Referenzen erhalten; danach Windows-CI und M1/M2 gemäß Roadmap.

## Schritt 5: Vollständigen Stand für GitHub integrieren

Auf ausdrückliche Nutzeranweisung werden Xournal++-Basis, Foliq-Code,
Dokumentation, Build-Verfahren, Windows-CI und Testbelege gemeinsam nach
leotrax3d/foliq übernommen. Zusätzlicher Remote `foliq` zeigt auf dieses private
Repository; `origin` bleibt unverändert beim Upstream. Die bestehende main-
Historie der UI-Uploads wird durch einen regulären Merge erhalten, kein
Force-Push. Sämtliche sechs Remote-Referenzdateien wurden vorab per Git-Blob-Hash
mit den lokalen Dateien verglichen und sind identisch.

Vor Veröffentlichung erneut geprüft: git diff --check, Shellsyntax des
Buildhelpers, YAML-Parsing und gespeicherte Test-XMLs (je 92 Tests ohne Fehler).
Keine C++-Änderung seit den erfolgreichen Linux-Builds; kein erneuter Hardwaretest.
Vollständige erreichbare Upstream-Historie mit git fetch --unshallow origin
nachgeladen. Lokale Basis als d92cabd2 committed; UI-Upload-Historie mit
5595b6a4 konfliktfrei zusammengeführt. Die Referenzdateien blieben unverändert.
Push-Ergebnis und erster CI-Status werden nach dem Upload ergänzt.

## Schritt 6: GitHub-Push und verbindliche E-Mail

Vollständigen Projektstand inklusive Upstream-Historie und v1.2.8-Tag erfolgreich
nach leotrax3d/foliq gepusht; Bilder und ihre bisherigen Commits erhalten.
Der Windows-Workflow startete tatsächlich (Run 42897015289, zunächst laufend).
Noch kein erfolgreicher Windows-Build oder Testpaket behauptet.

Neue Nutzeranweisung: ausschließlich leotrax3d@gmx.net für Author/Committer aller
neuen Foliq-Commits. Global und lokal konfiguriert und verbindlich in AGENTS,
docs/CONTRIBUTING sowie README hinterlegt. Drei in diesem Schritt zunächst mit
der vorherigen Umgebungsidentität angelegte lokale Commits werden vor Abschluss
in beiden Identitätsfeldern korrigiert; Inhalt, ursprüngliche UI-Upload-Commits
und Upstream-Historie bleiben erhalten. Dafür ist ein auf den zuvor selbst
gepushten main-Stand begrenzter Force-with-lease-Push erforderlich.
Prüfung: alle neu angelegten Commit-Identitäten kontrollieren, Remote-HEAD mit
lokalem HEAD vergleichen und Arbeitsbaum auf ausstehende Änderungen prüfen.

## Schritt 7: Ersten Windows-CI-Fehler konkret behoben

Run 42897015289 ist bei der Dependency-Installation fehlgeschlagen:
mingw-w64-x86_64-gtksourceview4 nicht gefunden, MINGW64 inzwischen deprecated.
Im MSYS2-Katalog ist dieselbe Bibliothek für UCRT64 als Version 4.8.4-3
nachgewiesen. Foliq-Workflow auf UCRT64/ucrt-x86_64 umgestellt; fest codierten
Lizenzpfad und lokale Windows-Buildanleitung angepasst. Kein Frameworkwechsel,
keine neue Laufzeitbibliothek und kein Abschalten von TeX-Syntaxhervorhebung.
Neuer vollständiger Windows-Lauf weiterhin ausstehend; kein Paket zugesagt.
GitHub-Push und E-Mail-Korrektur sind erfolgt; Remote-HEAD stimmte mit lokalem
HEAD überein. Die vier korrigierten/neuen Foliq-Commits hatten jeweils
leotrax3d@gmx.net als Author und Committer, Arbeitsbaum danach sauber.

## Schritt 8: Lokale Notizbuchverwaltung (laufende Validierung)

Tatsächlichen Remote-main `1ff0784d` sauber geklont und Quellcode geprüft;
die Dokumentation der früheren Arbeitsumgebung wurde nicht als Buildnachweis
übernommen. Neuer Katalogkern `src/core/folio/{Catalog,Storage}` und GTK-Adapter
`Navigation` implementiert. Enthält Notizbücher, beliebig verschachtelte Gruppen,
Abschnitte, Seiten/Unterseiten, Anlegen/Umbenennen, persistente Reihenfolge,
Verschieben, rekursive Duplikate, Papierkorb/Wiederherstellen, offene Bücher und
letzte Position. Echte XOPP-Dateien werden über Control geöffnet, keine Fixtures
oder Beispielnotizbücher in der Anwendung. Format/Ordner/Recovery: NOTEBOOKS.md.

Katalogschema v1 mit Validierung, Revision und Backup; UUID-basierte Dateinamen
vermeiden Windows-Namensprobleme. Inhalte vor Katalogreferenz, atomare durable
GLib-Ersetzung, OS-Locks und Erkennung externer Änderungen. SaveJob schreibt
zunächst vollständig nach .saving, behält .bak und ersetzt erst nach Erfolg.
Bildhintergründe erhalten neue Namen; Duplikate kopieren Sidecars. Control setzt
synchronen Dirty-Status bei Fehler nicht mehr zurück. Automatisches Speichern
verwalteter Notizen alle 5 s, Fehlerdialog/Status und Wiederholungsbremse.
Foliq verwendet eigene Profilordner und GTK-App-ID.

Vorherigen Windows-Lauf 42897347723 am Log geprüft: Editor, Tests und NSIS-Paket
wurden gebaut; anschließende Quellbeilage scheiterte an `git: command not found`.
MSYS2-Dependency-Action installiert jetzt ausdrücklich git. Keine neue
Laufzeitbibliothek. Eigener Entwicklungsbranch für Windows-Testpaket vorgesehen;
keine Änderung direkt auf main. Neue Commits verwenden verbindliche E-Mail.

Linux-Validierung in frischem Ubuntu-24.04-Container mit existierendem Buildhelper
läuft; neue GTests decken Roundtrip/Neustart, Hierarchie, Reihenfolge, Zyklen,
Duplikate, Sidecars, Papierkorb, Position, Versionsablehnung, fehlenden/beschädigten
Katalog, Locks, fremde Änderungen und simulierte Schreibfehler ab. Ergebnisse
und tatsächlicher Windows-Artefaktlink werden nach Abschluss ergänzt.

Linux-Gesamtbuild erfolgreich; erster vollständiger Testlauf **100/100 Tests aus
21 Suites bestanden** (XML: validation/notebooks-linux-tests.xml). GTK unter
Xvfb/DBus tatsächlich gestartet; echter UI-Durchlauf hat Anlegen der Hierarchie,
Öffnen einer XOPP-Seite, Maus-Stiftstrich und persistierendes 5-s-Autosave bestätigt.
Der anschließende Ablauf deckte verlorene Baumauswahl nach Refresh auf; Auswahl
wird nun erst nach Expand wieder gesetzt. Zweispaltige Aktionsleiste schafft
Platz für die eigentliche Navigation. Wiederholungsdurchlauf läuft.
Zusätzlich neue Dokumentziele und explizit bestätigte Speichern-unter-Ziele mit
Konflikt-Snapshot erfasst; unbekannte vorhandene Ziele werden nicht überschrieben.

Zweiter Linux-Testlauf nach Konflikt-/Auswahlkorrekturen erneut **100/100**.
GTK-Durchlauf anschließend vollständig bestanden: echte XOPP-Eingabe und Autosave,
rekursives Duplizieren, Papierkorb/Wiederherstellen, echter Backup-Dateisystemfehler
(Ordner statt .bak), unverändertes Original bei Fehler, erfolgreicher expliziter
Retry und Neustart mit gleichem Dokument und Stiftstrichen. Windows-Lauf
42899676301 für Zwischenstand 0145ef49 erfolgreich, inkl. portablem Testartefakt;
der neuere Stand wird separat gebaut. Bei Prüfung des zweiten Instanzstarts
wurde der Fehlerdialog vom noch nicht initialisierten Control-Fenster entkoppelt:
Navigation erhält das bereits existierende GtkWindow direkt.

Weitere echte GTK-Prüfungen bestanden: Notizbuch schließen → Neustart bleibt
geschlossen → Öffnen stellt Dokument wieder her; zweite Instanz zeigt Sperrfehler,
bleibt lauffähig und verändert den Katalog nicht; Schema 99 zeigt sichtbaren
Startfehler ohne Crash/Überschreiben. Normaler GUI-Exit explizit mit Rückgabecode 0
geprüft. Screenshot: validation/notebooks-navigation.png (synthetische Testnotiz).
CMake-/DEB-Mindestanforderung auf GLib 2.66 angehoben, passend zur verwendeten
atomaren/durablen API; keine zusätzliche Bibliothek. Historische Einstiegsangaben
in README/WINDOWS/UPSTREAM an den implementierten Stand angepasst.

Zusätzlicher Dateisperrfall geprüft: blockierter `.saving`-Pfad (Ordner statt
Kompressionsdatei). SaveJob prüft nun schon den Öffnungsfehler des Gzip-Streams,
bevor XML geschrieben wird. Echter GTK-Test bestätigt sichtbaren Fehler bei
lauffähiger App, unverändertes Original und erfolgreichen Retry nach Entfernen
des Hindernisses. Dieser Test deckt einen anderen Pfad als die .bak-Simulation ab.

## Paketprüfung: Quellbeilage vor Auslieferung korrigiert

Windows-Lauf 42902030779 für 038bf789 erfolgreich: **101/101 Tests aus 21 Suites**,
davon neun Foliq-Tests einschließlich echtem Win32-Lockfall. Portable EXE gebaut
und im Runner per --version gestartet. ZIP tatsächlich heruntergeladen; SHA-256,
ZIP-CRC, AMD64-PE-Header und beide enthaltenen Manifest-Prüfsummen erfolgreich.
Die zusätzliche Inhaltsprüfung fand jedoch eine leere Quellbeilage (171 Byte,
nur Git-PAX-Header): `git archive` wurde aus build/ aufgerufen und beschränkte
sich damit auf das ignorierte Build-Unterverzeichnis. Dieses Paket wird nicht
als vollständiges Testpaket ausgeliefert. Workflow verwendet nun ausdrücklich
`git -C .. archive` und prüft per tar, dass Katalog, SaveJob und LICENSE enthalten
sind. Neuer Windows-Paketlauf erforderlich; Produktcode unverändert.

## Abschluss: lokale Notizbuchverwaltung und verifiziertes Windows-Testpaket

Fertiger Produktcode und korrigierter Buildweg auf `feat/local-notebooks`,
Paketquellstand **c1e5c012e3289c0b202f7d23ea9aee3f2880c0b7**. Main unverändert.
Der nachfolgende reine Dokumentationscommit enthält die abschließenden Belege;
keine weitere Änderung an Produktcode oder Workflow.

- Linux: vollständiger Editorbuild, **100/100 Unit-Tests**. GTK-Prüfskripte auf
  finalem Produktcode erneut vollständig bestanden: echte Eingabe/Autosave,
  Duplikat, Papierkorb/Wiederherstellung, Neustart, Buch schließen/öffnen,
  zweite App-Instanz, unbekannte Version und beide realen Speicherfehlerpfade
  (.bak und .saving) mit erfolgreichem Retry. Normaler Exit mit Code 0 geprüft.
- Windows: [Run 42903859940](https://github.com/leotrax3d/foliq/actions/runs/42903859940)
  vollständig erfolgreich, **101/101 Tests** inklusive Win32-Lockfall;
  Upstream-Paketierung und portable --version-Prüfung erfolgreich.
- [Windows-ZIP herunterladen](https://github.com/leotrax3d/foliq/actions/runs/42903859940/artifacts/13803863342),
  entpacken, `dist/bin/xournalpp.exe` starten. Quellarchiv, Lizenzen, Toolchain-
  und Paketmanifest sowie Prüfsummen enthalten. Artefakt bis 2026-11-24 verfügbar;
  lokale Kopie `/workspace/artifacts/foliq-windows-c1e5c012.zip`.
- ZIP vollständig heruntergeladen und geprüft: GitHub-Digest, alle ZIP-CRCs,
  beide internen Manifest-Hashes, AMD64-PE, 1.476 Quellarchiv-Einträge und
  Kernquellen bytegenau gegen Paketcommit. Details und Hashes in
  validation/NOTEBOOK-VALIDATION.md und validation/notebooks-package.json.
- Reproduzierbare GTK-Testabläufe: validation/notebooks-ui-{smoke,locks,stage}.py;
  Resultate, Screenshot und Linux-/Windows-Test-XML daneben. Nur synthetische
  Testdaten, keine Beispielnotizbücher im Programm.

Offene Abnahme: physische Windows-GUI-/DPI-/Screenreader-/Stift-/Touch-/Audio-
Tests nach WINDOWS.md. Große Dokumente können synchrones Autosave spürbar machen.
Backup-Auswahl bei beschädigtem Katalog erfolgt bewusst manuell; kein GC für
Backups/verwaiste Inhalte; externe Medien behalten ihre XOPP-Pfadsemantik.
Kein verteilter Schreibschutz für Netzwerk-/Cloud-Sync-Ordner. Entwicklungs-
paket bleibt unsigniert und ist kein M7-Release. Nächster überprüfbarer Schritt:
ZIP auf Windows 10/11 entpacken und die Notizbuch-Testmatrix in WINDOWS.md mit
Hardware-/OS-Angaben ausführen, anschließend M2-Navigation/Accessibility ausbauen.

Alle neu erstellten Commits mit Author und Committer leotrax3d@gmx.net geprüft.
`git diff --check` erfolgreich; keine neuen Laufzeitbibliotheken. GLib ≥ 2.66
ist jetzt eine explizite Mindestversion für den atomaren/durablen Schreibpfad.

## Übernahme nach main – 2026-11-11

Auf ausdrückliche Nutzeranweisung den vollständigen Stand von
`feat/local-notebooks` nach `main` übernommen. Vorher `git fetch origin`, sauberen
Arbeitsbaum und gemeinsame Historie geprüft: Remote-main ist Vorfahr des
Feature-Stands; neun vorhandene Commits per Fast-forward übernommen, keine
Konflikte oder Historienumschreibung. Produktcode unverändert gegenüber dem
verifizierten Windows-Paket c1e5c012; danach kamen ausschließlich Testbelege und
Dokumentation hinzu. Author-/Committer-E-Mail aller übernommenen Foliq-Commits
und dieses Übergabecommits geprüft: leotrax3d@gmx.net. `git diff --check`
erfolgreich; kein erneuter Editorbuild für die reine Git-/Dokumentationsübernahme.
Push regulär nach origin/main; abschließend Remote-HEAD und Arbeitsbaum prüfen.

## Codex-Umgebung vorbereiten – 2026-11-11

Auf Nutzerauftrag eine wiederverwendbare Einrichtung für nachfolgende Codex-Chats
im Repository hinterlegt: `scripts/foliq/setup-codex.sh`, `.codex/README.md` und
`docs/CODEX.md`; Einstieg in README und AGENTS ergänzt. Setup installiert die
vorhandenen Build-Abhängigkeiten sowie GTK-Testwerkzeuge, konfiguriert den echten
Editor und System-GTest ohne Download, baut und führt die Tests aus. Der lokale
Git-Author/Committer wird auf leotrax3d@gmx.net gesetzt. `--check` prüft nur die
Voraussetzungen, `--configure` konfiguriert ohne Installation, `--build` nutzt
installierte Pakete und den inkrementellen Cache. Standardcache `build-codex/`
ist ignoriert. Root-Paketinstallation wechselt vor dem Build zum Checkout-Eigner.
Gezielte CMake-Ziele vermeiden Änderungen an versionierten Übersetzungen.

Tatsächlich ausgeführt im vorhandenen Ubuntu-24.04-Container `foliq-build`:

- `bash -n scripts/foliq/setup-codex.sh`: erfolgreich.
- Unprivilegiertes `--check`: GTK 3.24.41 und GLib 2.80.0 bereit; Python-GI,
  AT-SPI, Xvfb und übrige Werkzeuge geprüft.
- Vollständiges `--setup` als root mit `FOLIQ_CODEX_BUILD_DIR=/tmp/build-foliq`:
  APT-Update/Installation erfolgreich; Wechsel auf ubuntu/UID 1000, Editor und
  Tests gebaut, **100/100 Tests bestanden**, `xournalpp --version` erfolgreich.
- Danach `--build` als UID 1000 erneut: Ninja ohne ausstehende Arbeit,
  **erneut 100/100 Tests bestanden**, Version erfolgreich.
- `--configure` am Standardpfad `/src/build-codex`: erfolgreich; beide Cache-
  Konfigurationen gehören UID 1000. Keine Änderungen unter po/.
- Keine Änderung am C++-Produktcode; kein neuer Windows-Build für diese Linux-
  Setup-/Dokumentationsänderung. `git diff --check` erfolgreich.

Plattformgrenze: Die aktuelle Cloud-Laufzeit ist erreichbar, aber nicht mit einer
veröffentlichten Quellkonfiguration verbunden (`source_config_id: null`). In
dieser Sitzung stehen keine Werkzeuge zum Erstellen/Speichern/Veröffentlichen
von Codex-Umgebungsvorlagen zur Verfügung; auch keine Browser-Steuerung für den
Einstellungsdialog. Es wurde daher keine dauerhafte Codex-Plattformressource
angelegt und kein öffentlich erreichbarer Server gestartet. Die konkrete,
getestete Einrichtung liegt auf main bereit. Einmal in den Codex-Umgebungseinstellungen
Foliq für leotrax3d/foliq/main anlegen, Setup-Befehl gemäß CODEX.md eintragen,
ausführen und speichern/veröffentlichen; anschließend in neuen Chats auswählen.
Git und HANDOFF bleiben die dauerhafte Wissens-/Arbeitsübergabe.

## M2 Hauptoberfläche – 2026-11-11 (Validierung läuft)

Frischen Checkout von origin/main geprüft, AGENTS und Projektdokumentation gelesen,
`git fetch origin` ausgeführt. Tatsächlich vorhanden: lokaler Katalog v1 mit
Notizbuch-/Gruppen-/Abschnitts-/Seitenhierarchie und Unterseiten, persistente
Verschiebe-/Duplikat-/Papierkorbpfade, XOPP-Navigation und Autosave; bestehende
Xournal++-Werkzeuge einschließlich PDF. Noch kein Rich Text, globale Volltextsuche,
OCR oder Auto-Grow. Keine solchen Attrappen in der neuen Oberfläche.

GTK3/C++ beibehalten. Navigation hat jetzt zwei native GtkTreeViews in verschiebbaren
GtkPaned-Spalten; daneben bleibt derselbe Editor. Notizbuch/Abschnitte getrennt
von Seiten/Unterseiten, einklappbare Hierarchie, ellipsierte Titel, ATK-Namen,
F2 zum Umbenennen und Alt+↑/↓ zum Sortieren. Kompakte Aktionsmenüs bewahren alle
bisherigen Katalogaktionen. Drag-and-drop ist kein reines Widget-Reordering:
Zeilenkanten sortieren, Zeilenmitte verschiebt in den Knoten über Catalog::move,
inklusive bestehender Typ-/Zyklusvalidierung und sofortiger Persistenz.

Oben Start/Einfügen/Zeichnen/Ansicht mit bestehenden Control-Aktionen, echte
Dokumentsuche, Undo/Redo, Speicherstatus und Fokusumschalter. Spezialwerkzeuge
bleiben in den bestehenden Menüs/Werkzeugleisten. Neue Foliq-Profile starten
mit Lucide-Icons, 1200×800 und ohne die zusätzliche Dokumentvorschauspalte bzw. die doppelten Werkzeugleisten;
„Weitere Werkzeuge“ unter Zeichnen und „Alle Werkzeuge“ unter Ansicht blenden
die bestehenden Leisten wieder ein;
vorhandene gespeicherte Einstellungen bleiben maßgeblich. Hell/Dunkel/System
benutzen den vorhandenen ThemeVariant-Pfad. Gestaltungsgrundlagen/Referenzen:
design-references/FOLIQ-UI.md und ui/xournalpp.css. Betrachtet: Repository-2735
und öffentlicher Joplin-Desktop-Screenshot; keine Fremdassets übernommen.

Optionale view.ini v1 im Bibliotheksordner: beide Spaltenbreiten, Fokus,
Farbschema, Werkzeugreiter, Abschnitt, aufgeklappte Knoten. Bestehendes atomicWrite
mit Backup; unbekannte Versionen werden nicht überschrieben. Fehlerhafte
Ansichtsoptionen sperren nicht den gültigen Katalog. Kein Katalog-/XOPP-Formatwechsel.
Windows-Matrix erweitert. Neuer GTK-Smoke-Test: validation/main-ui-smoke.py.

Host: Debian 13 x86_64, UID 1000; --check meldete fehlendes CMake. Kein sudo oder
Pakethelper auf dem Host. Managed Docker 29.9.2 geprüft; Wegwerfcontainer foliq-ui
mit Ubuntu 24.04 (Digest fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852)
und vorhandenen Build-/GTK-Testpaketen eingerichtet. Build läuft als UID 1000
über setup-codex.sh --build, Cache build-codex. Finale Build-/Test-/GUI-Ergebnisse
werden unten ergänzt; keine neue Windows-/Stift-/Audio-Hardwareabnahme behauptet.

### M2: endgültige Prüfungen und Ergebnis

Vollständiger Editorbuild unter Ubuntu 24.04 x86_64/GTK 3.24.41/GCC 13.3
als UID 1000 erfolgreich. Nach den letzten Produktkorrekturen erneut über
`FOLIQ_BUILD_JOBS=4 bash scripts/foliq/setup-codex.sh --build` gebaut:
**100/100 Tests aus 21 Suites bestanden**, --version erfolgreich.
XML: validation/main-ui-linux-tests.xml. Vorhandene Upstream-Warnungen im
Gesamtbuild; keine pauschale Warnungsfreiheit behauptet. GTK-3.18-Menüfallback
per Versionsguard erhalten; alter GTK-Fallback nicht separat ausgeführt.

Native GTK-Prüfung mit Xvfb/X11, DBus/AT-SPI und xdotool, beide Skripte mit
Exit 0 abgeschlossen. Kommandos/Plattform/Grenzen in
validation/MAIN-UI-VALIDATION.md. Auf finalem Produktcode erneut bestanden:

- Notizbuch/Abschnitt/Seite/Unterseite über echte GTK-Menüs anlegen, XOPP öffnen,
  Stift-/Radiererwechsel, echter Mausstrich, Undo/Redo über neue Knöpfe und Autosave.
- Echte Maus-Drag-and-drop-Vorgänge für Seite und Abschnitt; Elternbeziehungen im
  gespeicherten Katalog geprüft. F2, Alt+↑/↓ und Verschieben-Dialog ebenfalls
  durch die echte Oberfläche geprüft. Suche öffnet den vorhandenen Dokumentpfad.
- Beide Spaltengrenzen tatsächlich gezogen; geänderte Breiten gespeichert.
  GTK verteilt die innere Breite beim Ändern der äußeren proportional weiter.
- Dunkles Farbschema, Fokus, Abschnitt/Reiter und Breiten nach Neustart erhalten;
  sichtbare echte Aufnahmen bei 1200×800, exakt 800×600 und GDK_SCALE=2
  (2400×1600 physisch). Neue dunkle Shell-Farben sichern den Kontrast unabhängig
  von fehlenden dunklen Systemthemen. Papierfarbe bleibt dokumentbezogen.
- view.ini-Version 99 zeigt Fehler und bleibt bytesgleich erhalten; kein
  stillschweigendes Zurückschreiben. Reguläres Beenden über GTK-Menü mit Code 0.
- Separater PDF-Test: explizites zweiseitiges PDF geöffnet, echte Stiftanmerkung,
  XOPP gespeichert; unabhängig im Ergebnis **2 Seiten, 2 PDF-Hintergründe,
  1 Strich** gezählt. Dabei vorhandenen Startfehler behoben: zuletzt verwendete
  Notiz darf ein explizit geöffnetes PDF ohne XOPP-Dateinamen nicht verdrängen.
  Wiederherstellung prüft jetzt zusätzlich getPdfPageCount() == 0.

Die GUI-Prüfung korrigierte außerdem den Quellbaum-Refresh beim Abschnittsziehen
und die Einbettung des alten Vorschau-Umschalters. Letzterer behält einen eigenen
Editorcontainer; vorhandene Umschaltlogik und Eingabepfade bleiben erhalten.
Neue Zwischenablageknöpfe spiegeln den tatsächlichen Aktivierungszustand der
bestehenden Menüaktionen, Werkzeugknöpfe den aktiven Werkzeugzustand.

Screenshots unter validation/: main-ui-light.png, main-ui-dark.png,
main-ui-focus.png, main-ui-small.png, main-ui-scale200.png und main-ui-pdf-before.png.
Ausschließlich synthetische Testdaten; keine Microsoft-Assets übernommen.
Tests lagen im laufenden Container unter /tmp/foliq-main-ui bzw.
/tmp/foliq-main-ui-pdf-passed, Build unter /src/build-codex. Keine neue
Laufzeitbibliothek, kein zweiter Editor und keine XOPP-/Katalogmigration.

`git diff --check` erfolgreich. Änderungen lokal im Checkout; kein Commit/Push
und kein neuer Windows-CI-Lauf in diesem Auftrag. Nächster überprüfbarer Schritt:
Quelländerungen reviewen, neues Windows-Testpaket bauen und die aktualisierte
WINDOWS.md-Matrix auf Windows 10/11 einschließlich NVDA, 100/150/200 % DPI und
physischem Stift/Touch/PDF/Audio abnehmen. Linux/ATK/Mausprüfung ersetzt diese
Plattform-/Hardwareabnahme nicht.

## Hauptoberfläche nach main veröffentlichen – 2026-11-11

Auf ausdrücklichen Nutzerauftrag alle oben dokumentierten Oberflächenänderungen,
Testskripte, Testbelege und Screenshots für main übernommen. Vor Commit/Push
origin aktualisiert: lokaler HEAD und origin/main identisch (0/0), keine
Integration oder Historienumschreibung erforderlich. `git diff --check`
erfolgreich. Lokale Git-Identität ausdrücklich auf leotrax3d@gmx.net gesetzt;
Author und Committer vor Push prüfen. Regulärer Push nach origin/main; abschließend
Remote-HEAD und sauberen Arbeitsbaum prüfen. Kein erneuter Produktcode geändert
und kein neuer Hardwaretest für diesen Veröffentlichungsschritt.

## M4 Seitentypen – 2026-11-11 (Integration und Abnahme)

Zunächst AGENTS, PRODUCT/ARCHITECTURE/ROADMAP/UPSTREAM/HANDOFF sowie das tatsächliche
XojPage-/PageView-/Input-/Render-/XOPP-/Undo-/PDF-Modell untersucht. Vorbereiteter
Checkout lag bei a80e1e90; `git ls-remote origin HEAD` zeigte e94b6344 und lokale
origin/main enthielt bereits die neuere Hauptoberfläche. Nicht überschneidende
Änderungen per `git merge --ff-only origin/main` erhalten. Auf aktuellem M2-Stand
weitergearbeitet; kein Push/Commit für diesen Auftrag.

Papier und freier Raum sind nun explizit im bestehenden Editor modelliert.
PageSurface v1 speichert Kamera und PDF-Ausschnitt/-Teilung als validierte
XOPP-Seitenattribute; alte Dateien bleiben Papier. Unbekannte Versionen,
unvollständige Metadaten und ungültige Zahlen werden beim Laden abgewiesen.
SaveJob-Staging, Backup, atomarer Replace und Konflikterkennung bleiben erhalten.
Kein neues externes Format, keine Laufzeitbibliothek. ADR-006 ersetzt den zuvor
bewusst beschränkten endlichen Auto-Grow-Ansatz aus ADR-004.

Keine vorab riesige Papierseite: freie PageViews zeichnen direkt in den sichtbaren
Clip, Bildschirmfenster bleibt beim Zoomen konstant. Weltursprung getrennt von
Layoutposition; alle bestehenden Werkzeuge verwenden dieselbe Projektion.
Hand/Touch/Mausrad verschieben Kamera. Zoomanker, Pan/Zoom-Undo mit 350-ms-
Zusammenfassung und vollständiges Einstellungs-Undo ergänzt. Neue GTK-Oberfläche:
Einfügen → Seitentyp / Papier; vorhandene Journal-Aktion öffnet denselben Dialog.
Papierformat/Orientierung, unterstützte Lineatur/Rasterabstand/Farbe sowie
PDF-Ausschnitt und Kachelgrößen sind dort erreichbar. Typwechsel erzeugt eine
Kopie im selben Dokument; Original bleibt erhalten, Überlauf wird nicht gelöscht.
Grenzen vor Anwenden im Dialog. Details: SURFACES.md.

Tests fanden und korrigierten übernommene Annahmen: negative-only Stroke-Bounds
initialisierten Maxima mit kleinstem positivem Double statt lowest(); aktive
Werkzeugknöpfe lösten beim programmgesteuerten Synchronisieren erneut Klickaktionen
aus und schalteten zurück zum Stift. Beide Fehler sind korrigiert. Große
Textmarker-/Radiermasken werden auf den Clip begrenzt; breite Polygone/Segmente
werden vor Cairos intern begrenztem Rasterizer geometrisch zugeschnitten.
Auswahlvorschauen erhalten maximal 2.048 Pixel pro Kante; originale Vektoren
bleiben erhalten. Vorschauen passen den Inhaltsbereich in eine kleine Ausgabe.

PDF nutzt pro freier Seite wählbaren Inhalt oder numerischen Ausschnitt inklusive
negativer Koordinaten und iteriert Teilseiten ohne Gesamtbitmap. Papier bleibt
unverändert. Vorprüfung begrenzt Teilseitenzahl/Größe; Outline-Ziele werden bei
freien Dokumenten ausdrücklich weggelassen. Direkter GTK-Druck verweist auf
PDF-Export; PNG/SVG nutzen Ausschnitt ohne Teilung, PNG mit Pixelbudget.

Bisher ausgeführt: voller Linux-Editorbuild im frischen Ubuntu-24.04-Dockercontainer
`foliq-pages`, Host Debian 13 ohne sudo/CMake. Setup über vorhandenes Skript,
Build als UID 1000. Zwischenstand **115/115 Tests aus 22 Suites bestanden**.
Neue GTests prüfen Koordinaten/Zoomanker, Bounds, Validierung, kopierende Umwandlung,
Drucksegment-Roundtrip, Legacy-Papier, Settings-Undo-Swap, History-Merge, gekachelte
und gemischte PDFs samt Poppler-Pixel-/Seitengrößenprüfung sowie geschützte
Zieldatei bei Exportfehler. Tatsächliche finale Belege und GTK-/Stressresultate
werden nach Abschluss darunter ergänzt. WINDOWS.md enthält konkrete, ausdrücklich
noch offene Hardwaretests für Druck, Handballen, Tasten und Latenz.

### M4: endgültige Belege und verbleibende Grenzen

Nach allen Produktkorrekturen und Formatierung erneut gebaut:
`docker exec -u 1000 foliq-pages bash scripts/foliq/setup-codex.sh --build`.
**116/116 Tests aus 22 Suites bestanden**, darunter 16 neue PageSurface-Tests;
Editor und --version erfolgreich. XML: validation/surfaces-linux-tests.xml.
Letzter inkrementeller Lauf ohne neue Compilerwarnung; Gesamtbuild enthält
weiterhin bestehende Upstream-Warnungen. Zusätzlich Hintergrundunterdrückung
für freie Exporte geprüft, damit die vorhandene Exportoption wirksam bleibt.

Native GTK-Prüfung auf finalem Code mit Xvfb/DBus/AT-SPI/xdotool, Exit 0:
**10 Prüfschritte bestanden**. Negative Maus-Stiftkoordinaten, Lasso-Verschieben,
Skalieren, Textmarker, Radierer, Hand-Schwenken, Zeiger-Zoom, Papierkopie und
Papiermaße jeweils mit relevanter Undo/Redo-Prüfung anhand der tatsächlich
abgespeicherten XOPP-Daten. Neustart erhält beide Seitentypen, Kameraposition und
Strichdaten; reguläre Programmenden mit Code 0. Reproduzierbares Skript und
Kurzprotokoll: validation/surfaces-ui-smoke.py, surfaces-ui-results.txt.
Screenshots freie Fläche, Dialog und Auswahl daneben. Nur synthetische Daten.

Enger Cairo-Stresstest: zwei breit gespannte Renderfälle jeweils 200-mal,
insgesamt **400 bestandene Testausführungen**, feste Ausgabe 64×64 Pixel,
Weltkoordinaten ±900.000 pt, Ausschnitt bei (500.000,500.000). Gemessen 0,068 s
und maximaler Prozess-RSS 33.608 KiB. Kein GUI-/Windows-/Stiftlatenz-Benchmark.
Rohdaten: validation/surfaces-render-stress.json. Einzelheiten, Kommandos und
Nachweisgrenzen: validation/SURFACES-VALIDATION.md. `git diff --check` und
Python-Syntaxprüfung erfolgreich.

Bekannte, offen ausgewiesene Grenzen: numerischer Kamerabereich, O(Elementzahl)-
Traversal statt räumlichem Index, global vereinfachte freie Lineatur, begrenzte
Auswahlvorschau, endliche PDF-/Bildhintergründe, maximal 10.000 PDF-Teilseiten,
gekürzte Randseiten und keine PDF-Outlines in freien Dokumenten. Direkter Druck
über PDF-Export. PNG mit Pixelbudget. Älteres Xournal++ ignoriert die freien
Metadaten; Original in Foliq bewahren. Der Umwandlungsdialog erklärt Beschneidung,
Originalerhalt, Lineatur- und Exportgrenzen vor Anwenden. Nicht jede einzelne
Form und Gerätezuordnung wurde automatisiert bedient; die vorhandenen Modell-,
Input-, Filter- und Undo-Pfade wurden erhalten. Keine Hardware- oder Windows-CI-
Abnahme behauptet; kein neues Windows-Testpaket und kein Commit/Push in diesem
Auftrag. Nächster überprüfbarer Schritt: Quellreview, Windows-Build und physische
Stift-/Palm-/Seitentasten-/Latenzmatrix aus WINDOWS.md auf realer Hardware.

## Seitentypen auf main veröffentlichen – 2026-11-11

Auf ausdrücklichen Nutzerauftrag den vollständigen Seitentyp-Stand einschließlich
Quellen, Dokumentation, GTK-Prüfskript, Screenshots und Testbelegen für origin/main
vorbereitet. Vor Commit `git fetch origin`: HEAD und origin/main identisch (0/0),
keine Konflikte oder Historienumschreibung erforderlich. `git diff --check`
erfolgreich; gespeicherte XML-/GTK-Belege erneut geprüft (116 Unit-Tests ohne
Fehler, zehn erfolgreiche GTK-Prüfschritte). Keine Produktänderung seit der
abschließenden Validierung; kein neuer Hardwaretest behauptet.
Git-Identität lokal explizit auf leotrax3d@gmx.net gesetzt, Author und Committer
geprüft. Regulärer Push von HEAD nach origin/main; Remote-HEAD und Arbeitsbaum
anschließend kontrollieren. Windows-CI-Ergebnis ist separat abzuwarten.

## M3 Text und Medien – 2026-11-11

AGENTS und Projektdokumentation sowie TextEditor/GtkTextBuffer, Text/Layer/XojPage,
Objektzwischenablage, XOPP, gemeinsame Auswahl/Koordinaten, Undo und Cairo/Pango-
PDF-Pfad untersucht. Anfänglicher Checkout e94b6344; der Remote-Vergleich ergab
zwischenzeitlich den neueren Seitentyp-Stand 81d5201e. Diesen regulär per
Fast-forward übernommen, lokale Textänderungen erhalten und Dokumentationskonflikte
unter Erhalt beider Arbeiten aufgelöst. Kein Frameworkwechsel, keine neue
Editor-/Laufzeitbibliothek. ADR-007 dokumentiert den nativen Erweiterungspfad.

Umgesetzt:

- RichText v1 mit Unicode-Codepoint-Bereichen, gemischter Schrift/Größe,
  fett/kursiv/unterstrichen, Text-/Hintergrundfarbe und Link-/Seitenverweisen.
  Gemeinsamer GtkTextBuffer für Canvas und ausführlichen GtkTextView-Dialog;
  Start → Text formatieren…, F4 und dokumentierte Tastenkürzel.
- Absätze/Überschriftvorgaben, Listen-/Nummerierungs-/Einrück-/Checklistenaktionen.
  Listenmarker bleiben bearbeitbarer Unicode; kein separates Aufgabenmodell.
  Native interne formatierte Zwischenablage, externer Klartext-Fallback und
  explizites Klartext-Paste mit CRLF/CR-Normalisierung.
- Strukturierte rechteckige Tabellen mit editierbaren Zellen, Zeilen-/Spalten-
  operationen und erneutem Bearbeiten nach Neustart. Gemeinsames Pango-Layout
  und Cairo-Raster für Bildschirm und PDF. Ein Absatz je Zelle; keine gerenderten
  Tabellenbilder als vermeintlich bearbeitbare Daten.
- Proportionale Bilder und reversibler normalisierter Zuschnitt bei vollständig
  erhaltenem Original. Größen-/Zuschnittdialog auf der gemeinsamen Auswahl;
  ein Undo-Schritt, unveränderte Ebenenposition.
- Bestehende TexImage-Quelle/PDF-Speicherung erhalten und im Einfügen-Reiter
  erreichbar. Verständlicher umbrochener Fehlerstatus samt Compilerprotokoll;
  fehlender Generator mit Installationshinweis. Formelersetzung erst nach gültigem
  Rendern, eine Undo-Aktion, alte Formel bei Fehler/Abbruch erhalten. Asynchrones
  Render-Ergebnis erhält die tatsächlich gerenderte Quelle. PNG-Leseposition bei
  erneutem Laden zurücksetzen; unbekannte/defekte Bilddaten nicht als Erfolg melden.
- Begrenzter lokaler Textverlauf für Eingabe/Formatierung/Tabellen, danach eine
  Aktion im bestehenden Dokumentverlauf. Text-/Medienersetzung und Undo/Redo
  behalten Layer-Indizes. Oberstes Textobjekt wird bearbeitet. Unverändertes
  Öffnen erzeugt keine Aktion; resultierende Auswahl behält den gültigen Original-
  zeiger statt eines verworfenen Bearbeitungsklons.
- Gemeinsame freie Koordinaten geprüft: Clipboard-Einfügen klemmt nur im
  Papiermodus auf positive Werte; Bildimport auf freien Seiten skaliert nicht
  gegen entfernte Papierkanten. Rückgängig machbarer Kamera-Pan/Zoom beendet
  vorher die Textsitzung, damit die Reihenfolge im Dokument-Undo stimmt.

Dateien mit neuen Inhalten verwenden XOPP-Version 5, Inhaltsversion 1; Klartext
bleibt lesbar. Bisherige reine Inhalte bleiben Version 4. Validierte strukturierte
Daten und Zuschnitt werden im bestehenden `.saving`-/Backup-/atomaren Replace-
Pfad gespeichert. Unbekannte Versionen und widersprüchliche Daten werden
abgewiesen. XML-Textblöcke werden vollständig zusammengesetzt statt überschrieben;
CR wird als XML-Entity erhalten, ungültige XML-Zeichen im Rich-Text-Modell
zurückgewiesen. Helferfehler werden sofort an GMarkup weitergegeben, damit der
aktuelle Eingabeblock nicht trotz Fehler weiterverarbeitet wird. Neue
Objektzwischenablagen sind versioniert; alte Text-/Bildobjekte bleiben lesbar.
Legacy-XOJ-Export verweigert nicht darstellbare Erweiterungen vor dem Schreiben.

Bedienung, Format, Migration, Überlagerung, Größenänderung und Abhängigkeiten:
TEXT-AND-MEDIA.md. Architektur, Produkt, Roadmap, UPSTREAM und Windows-Matrix
aktualisiert. TeX bleibt optional/external: lokal pdflatex und die in der Vorlage
benutzten Pakete; Lesen/Export gespeicherter Formeln benötigt kein TeX. Keine
TeX-Distribution gebündelt. Offline-/MiKTeX-/TeX-Live-/Lizenzhinweise dokumentiert.

### Tatsächlich ausgeführte Prüfungen

Frischer Ubuntu-24.04-Container `foliq-text-dev`, Build als UID 1000, Debug,
CMake/Ninja, GTK 3.24.41, GLib 2.80, Pango 1.52/GCC 13.3. Host ohne CMake/sudo;
vorhandenen verwalteten Docker-Daemon verwendet. Bestehende Entwicklungsabhängigkeiten
sowie TeX Live und poppler-utils für Tests installiert; clang-format 18 nur als
Entwicklungswerkzeug. Keine neue Laufzeitbibliothek der App.

- Vollständiger Editor-/Unit-Build erfolgreich; integrierter Stand **128/128 Tests
  aus 23 Suites**, darunter zwölf neue Rich-Text-/Medientests. XML:
  validation/text-media-linux-tests.xml. Originale Oberflächentests bleiben dabei
  enthalten. Unicode, gemischte Stile, Formatversionen, lange XML-Blöcke,
  Tabellenpersistenz, CRLF/CR, Objekt-/GTK-Zwischenablage, Klonen/Skalieren,
  Stapelreihenfolge, Zuschnittpixel und Formelquelle/PDF tatsächlich geprüft.
- GTK-Texttest auf Papier: native deutsche Zwischenablage und GTK-Unicode-
  Komposition, Tastatur/Enter, Stile, Listen-/Aufgaben-/Einrück-Undo, interne
  formatierte und erzwungene Klartext-Zwischenablage, Seitenlink, unveränderte
  Bearbeitung → Auswahl → Objekt-Copy/Paste/Undo, Tabellenoperationen,
  Neustart/erneute Zelledition und PDF-Textextraktion bestanden.
- Derselbe GTK-Texttest auf freier Seite mit negativen Koordinaten bestanden,
  einschließlich Text-/Kamera-Undo-Reihenfolge, Elementzwischenablage,
  Tabellenpersistenz und gekacheltem PDF. Zusätzlicher Unit-Test verbindet
  negative Rich-Text-/Tabellen-/Formelkoordinaten, Papierkopie und vier PDF-Kacheln.
- GTK-Medientest bestanden: Bild auswählen, Breite/Zuschnitt/Verhältnis,
  Undo/Redo und Layer-Reihenfolge; reale Kompilierung mit Standard-TeX-Vorlage,
  Formel ändern, Quelle speichern/neustarten/erneut editieren, Syntaxfehler mit
  gesperrtem OK und erhaltenem Original; gemischte Text/Bild/Formel/Handschrift-
  Seite als PDF exportieren und deutsche Zeichen extrahieren.
- Bestehendes surfaces-ui-smoke.py erneut erfolgreich: **zehn PASS-Schritte**
  für Stift/Lasso/Skalieren/Textmarker/Radierer, Pan/Zoom samt Undo, Umwandlung,
  Papiereinstellungen und Neustart. Ursprüngliche M4-Screenshots erhalten,
  neue Durchlaufbilder als text-media-surfaces-*.png abgelegt.
- Keine Critical-/Assertion-Meldungen in den App-Logs der finalen vier GTK-Läufe.
  Erwartete ALSA/JACK-Meldungen im hardwarelosen Container sind kein Audiotest.
  `xdotool type` mit dynamischen Unicode-Tastenbelegungen war unzuverlässig;
  stattdessen echte GTK-Zwischenablage und GTK-Unicode-Komposition geprüft.
  Strg+Umschalt+U darf deshalb nicht vom Unterstreichen-Shortcut abgefangen werden.

Reproduzierbare Kommandos und Grenzen: validation/TEXT-MEDIA-VALIDATION.md;
Ergebnisse: validation/text-media-results.txt; ausführbare Text-/Medien-GTK-Skripte
und synthetische Screenshots liegen daneben. Scratch-Logs/Build unter work/ bzw.
build-codex/ sind nicht versioniert. `git diff --check` erfolgreich; keine
Konfliktmarker, HEAD und zuletzt aktualisiertes origin/main identisch (0/0),
Änderungen lokal uncommitted. Git-Identität lokal auf leotrax3d@gmx.net gesetzt.
Kein Commit/Push, kein neuer Windows-CI-Lauf und kein Windows-Paket in diesem Auftrag.

### Bekannte Grenzen und nächster überprüfbarer Schritt

HTML/RTF aus Office/Browser fällt auf Klartext zurück; keine verbundenen oder
mehrzeiligen Tabellenzellen. Überschriften sind Stilvorgaben, Listen einfache
Marker. Interne Verweise sind physische XOPP-Seitenzahlen, keine stabilen
Notizbuch-UUIDs; PDF exportiert sie visuell ohne eigene klickbare Annotation.
Der lokale Textverlauf ist begrenzt und wird beim Abschluss zu einer gemeinsamen
Dokumentaktion; noch aktive Eingabe vor Abschluss ist kein eigener gespeicherter
Zustand. Diese Grenzen sind in der Bedienungsdokumentation ausdrücklich benannt.

Nächster überprüfbarer Schritt: Änderungen reviewen und Windows-UCRT64-Paket
bauen; deutsche Tastatur/AltGr/Tottasten, japanische/chinesische IME-Kandidaten,
NVDA und 100/150/200 % DPI, Offline-TeX sowie physischen Stift/Touch/Palm-Rejection
auf Papier und freien Seiten gemäß WINDOWS.md abnehmen. Linux-/Maus-/AT-SPI-
Prüfungen ersetzen diese Plattform-/Hardwareabnahme nicht.

## Text und Medien auf main veröffentlichen – 2026-11-11

Auf ausdrücklichen Nutzerauftrag den vollständigen oben geprüften Text-/Medien-
Stand samt Dokumentation, Testskripten, XML-Beleg und synthetischen Screenshots
für origin/main übernommen. Vor Commit `git fetch origin`: HEAD und origin/main
identisch (0/0), keine weitere Integration oder Historienumschreibung nötig.
`git diff --check` erfolgreich; Test-XML erneut geprüft: 128 Tests, keine Fehler
oder deaktivierten Tests. Keine Produktänderung seit der abschließenden Prüfung.
Author und Committer ausdrücklich leotrax3d@gmx.net. Regulärer Push nach main;
Remote-HEAD und Arbeitsbaum anschließend kontrollieren. Ein neuer Windows-CI-
Lauf ist separat zu bewerten und ersetzt keine Hardwareabnahme.

## Windows-CI #14: Test-Portabilität korrigieren – 2026-11-11

Fehllauf 43011642471 (Commit 7bb5db0e) und Job 130492443747 anhand der echten
GitHub-Actions-Logs untersucht. Windows/UCRT64 baut den Editor erfolgreich;
129 Tests laufen, 124 bestehen. Fünf Tests scheitern vor dem Packaging:

- Drei PDF-Rücklesetests bilden `file://` plus einen Windows-Pfad mit Laufwerk
  und Backslashes. Stattdessen die vorhandene plattformgerechte Funktion
  Util::toUri verwenden; Parsefehler erhalten die konkrete Poppler-Diagnose.
  Zwei Testdateinamen enthalten nun Leerzeichen und `#`, um URI-Escaping zu prüfen.
- Zwei Dateierhaltungstests rufen fs::remove auf, während ihr eigener
  std::ifstream noch geöffnet ist. Die Lesestreams werden jetzt per Scope vor
  dem Entfernen geschlossen. Die Inhalts-/Nichtüberschreiben-Assertions bleiben.

Nur Testcode geändert, keine Produktfunktion und kein Abschalten von Tests.
Linux: test-units neu gebaut, alle **128/128 Tests aus 23 Suites** bestanden.
Beleg: validation/windows-ci-portability-linux-tests.xml. `git diff --check`
erfolgreich. Auf Nutzerauftrag regulären Fix-Push nach main vorbereitet;
Author/Committer leotrax3d@gmx.net. Neuer Windows-Lauf und Packaging werden nach
Push separat bis zum Abschluss geprüft; Hardwareabnahme bleibt davon getrennt.

## Fluent-Oberfläche (Windows-11-Stil) – 2026-11-11

Branch `claude/jolly-cray-vn7ad3`. Entscheidung ADR-006: GTK3 bleibt, kein
Frameworkwechsel. Umgesetzt: Designsystem mit Tokens (`ui/xournalpp.css`,
`src/core/folio/Theme.*`), neue Befehlsleiste mit Pivot-Registerkarten,
Icon-Menüband (auf Nutzerwunsch Icons statt Textbeschriftung, Namen als
Tooltip/ATK), einklappbar mit temporär schwebendem Öffnen, ⋯-Menü mit allen
Upstream-Menüs (Accelerators erhalten), schwebende/anheftbare Navigation,
Fokusmodus, Kompakt/Berührung, Hell/Dunkel/System, Akzentfarbe mit
Kontrastsicherung, Abschnittsfarben, Weniger Bewegung, Pfadanzeige, dezenter
Status/rote Fehlerpille, ruhige Arbeitsfläche (Akzentkontur statt rotem
Rahmen; freie Fläche ohne Kanten), Dialog-Styling für alle Toplevels,
Wiederherstellung der Seitenliste beim Start, view.ini v2 mit v1-Migration.
Doku: design-references/FOLIQ-DESIGN-SYSTEM.md, WINDOWS-PEN-TEST-FLUENT.md,
validation/FLUENT-UI-VALIDATION.md (Kommandos, Ergebnisse, Grenzen).

Ausgeführt (Linux/Xvfb): test-units 132/132; main-ui-smoke.py alle PASS;
fluent-shots.py 18 Zustände. Nicht ausgeführt: Windows-Build/DPI, Stift,
Touch, NVDA/Narrator. Offen: native Windows-Dateidialoge, Tags/Aufgaben/
Anhänge/Verschlüsselung existieren noch nicht (keine Fake-UI gebaut),
englische Upstream-Texte in Dialogen ohne installierte Übersetzung.

Nächster überprüfbarer Schritt: Windows-CI-Paket dieses Branches bauen und
WINDOWS-PEN-TEST-FLUENT.md auf dem Stiftrechner abarbeiten.
