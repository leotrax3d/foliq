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
