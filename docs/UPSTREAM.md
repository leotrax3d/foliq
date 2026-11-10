# Herkunft und Lizenzen

Am 2026-11-10 enthielt /workspace kein Projekt und keine AGENTS.md. Xournal++
wurde unverändert nach /workspace/foliq geklont; anschließend wurde der lokale
Branch `foliq` angelegt. Keine vorhandene Arbeit wurde überschrieben.

- Remote: https://github.com/xournalpp/xournalpp.git (`origin`, unverändert)
- Release-Basis: `v1.2.8`
- Commit: `ce753d2e0c48a7026fe1fbe59f132586b2217296`
- Import: zunächst `git clone --branch v1.2.8 --depth 1 …`. Vor dem Push ins
  Foliq-Repository wurde mit `git fetch --unshallow origin` die vollständige
  erreichbare Upstream-Historie bis zu dieser Basis nachgeladen.
- Foliq-Ziel: https://github.com/leotrax3d/foliq (zusätzlicher Remote `foliq`).
  Die separate Historie der ersten UI-Uploads wurde regulär zusammengeführt.
- Originalversion in CMake: 1.2.8. Foliq ist ein Arbeitsname, keine Aussage über
  Markenverfügbarkeit oder Zugehörigkeit zum Upstream-Projekt.

## Tatsächlich geprüfte Bedingungen

`LICENSE` enthält GPL Version 2; `debian/copyright`, `copyright.txt` und die
Quellköpfe bestimmen für den Hauptcode **GPL-2.0-or-later**. Der Baum ist nicht
homogen lizenziert: copyright.txt verzeichnet unter anderem GPL-3+ für farbige
Icons und einzelne weitere Dateien, LGPL-2+, ISC (Lucide), BSD-2/3-Clause,
BSL-1, CC0 und CC-BY-SA-4.0. Die dateispezifischen Angaben und Lizenztexte sind
maßgeblich. Alle Originalhinweise und AUTHORS bleiben erhalten. Ergänzend liegen die
vollständigen GPLv3- und LGPL-2.1-Texte unter docs/licenses/ (unverändert aus
Ubuntu /usr/share/common-licenses).

Foliq-Änderungen am Hauptcode werden unter GPL-2.0-or-later beigetragen.
Mit GPL-3+-Bestandteilen muss die Gesamtverteilung GPLv3-kompatibel erfolgen;
daraus folgt keine Umschreibung der Lizenz jeder Quelldatei. Binärverteilung
muss passende vollständige Quellen, Lizenzhinweise und Build-Anweisungen
bereitstellen. Der CI-Testdownload enthält das Quellarchiv desselben Commits,
die Originalhinweise und die MSYS2-Lizenzverzeichnisse. Vor öffentlicher
Release-Verteilung müssen zusätzlich die exakten transitiven Laufzeitbibliotheken
mit ihren Quellbezugs-/Quellbereitstellungspflichten geprüft und archiviert
werden. Ein Paketmanifest allein erfüllt diese Pflichten nicht.

Es wurden keine neuen Editor- oder Laufzeitbibliotheken eingeführt. CMake,
Ninja, GoogleTest und GitHub/MSYS2 dienen nur dem Build/Test. Builds benötigen
Netzwerkzugriff zur Beschaffung; die Anwendung benötigt für lokale Notizen
keinen Server. Bestehende Lua-Plugins sind separat auf Netzwerkzugriffe zu prüfen.

## Änderungen gegenüber der Basis

AGENTS.md und docs/*, Linux-Buildhelper mit Paket-Lock/Dockerfile, eigener
Windows-Testworkflow mit optional deaktiviertem veraltetem GTK-Downgrade sowie
eine CMake-gesteuerte Foliq-Fensterkennung und README-Einstieg. Binaryname, Konfigurationspfade, Dateizuordnung und XOPP
bleiben im ersten Schritt bei Xournal++. Deshalb nicht parallel mit einer
produktiven Xournal++-Instanz im selben Benutzerprofil testen.

Seit M1 kommen der lokale Katalog und die GTK-Navigation hinzu. Foliq nutzt
eigene Profilordner/App-ID; XOPP und der Binaryname bleiben erhalten.
Speicherpfad und neue Dateien siehe NOTEBOOKS.md und HANDOFF.md.
