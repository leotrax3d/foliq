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
