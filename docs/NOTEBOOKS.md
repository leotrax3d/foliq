# Lokale Notizbücher – Format 1

Die neue linke Navigation arbeitet mit echten XOPP-Dateien im bestehenden Editor.
Zuerst ein Notizbuch anlegen, dort Gruppen oder Abschnitte, im Abschnitt Seiten.
Eine Seite als Elternknoten ergibt eine Unterseite. Doppelklick/Eingabe öffnet
Seiten; Notizbücher lassen sich damit öffnen/schließen. Pfeile ändern die
Reihenfolge, „Verschieben“ bietet Zielknoten an (ungültige Ziele werden abgewiesen).
„Löschen“ legt den gesamten Teilbaum in den Papierkorb; die Papierkorbansicht
zeigt zur Orientierung auch nicht gelöschte Elternpfade. Eltern zuerst
wiederherstellen. Es gibt bewusst noch kein unwiderrufliches Leeren.

## Speicherort und Aufgabenteilung

Standard: GLib-Benutzerdatenverzeichnis / `foliq/library` (Windows typischerweise
`%LOCALAPPDATA%/foliq/library`). Mit `FOLIQ_LIBRARY` lässt sich ein expliziter,
lokaler Bibliotheksordner für Tests wählen. Keine SQLite-Datenbank, keine neue
Bibliothek. GKeyFile kommt aus dem vorhandenen GLib (LGPL-2.1-or-later), zlib
und GTK sind bereits Build-/Laufzeitabhängigkeiten; Windows UCRT64 bietet sie an.

```text
library/
  catalog.ini                 # Schema, Revision, Hierarchie, Namen, Reihenfolge,
                              # Papierkorb, offene Bücher, letzte Seite/Position
  catalog.ini.bak             # letzte vollständige Katalogrevision
  catalog.lock                # OS-Sperre, kein PID-/Zeitstempel-Lock
  documents/
    <UUID>.xopp                # originaler XOPP-Inhalt; unabhängig lesbar
    <UUID>.xopp.bak            # letzte vollständig gespeicherte Revision
    <UUID>.xopp.lock           # OS-Sperrdatei, wird nicht entfernt
    <UUID>.xopp.bg_<UUID>.png   # gegebenenfalls XOPP-Bildhintergrund
```

Namen sind UTF-8-Metadaten (1–1024 Bytes), keine Pfadbestandteile. `CON`, `NUL`,
Doppelpunkte, Schrägstriche und abschließende Punkte sind deshalb als Anzeigenamen
möglich. IDs, Eltern-IDs und explizite Geschwisterreihenfolge sind persistent.
Umbenennen/Verschieben ändern weder ID noch Dokumentpfad. Ein Duplikat bekommt
neue IDs für den ganzen Teilbaum und kopiert angehängte Hintergrundbilder.
Externe PDF-/Audioverweise behalten ihre XOPP-Semantik; externe Medien werden
nicht automatisch in die Bibliothek importiert. Die Bibliothek auf ein anderes
Laufwerk zu kopieren macht externe absolute Medienpfade nicht portabel.

## Transaktionen und Fehler

Metadaten werden bei jeder Aktion sofort gespeichert. Seiten im geöffneten
Katalog werden alle fünf Sekunden bei Änderungen synchron gespeichert. Beim
Wechseln, Schließen und Duplizieren wird zuerst gespeichert; unbenannte externe
Dokumente behalten den bestehenden Speichern-Dialog. Die letzte XOPP-Seitennummer
wird beim Wechsel/Schließen und periodisch festgehalten. Bestehende
Editor-Metadaten ergänzen Zoom/Scrollposition.

Neue/duplizierte Inhalte werden **vor** der Katalogreferenz geschrieben. Erst
danach wird ein validierter Katalog als Ganzes veröffentlicht. Abbruch davor
kann unreferenzierte Dateien hinterlassen, niemals absichtlich einen Katalog auf
halb geschriebene neue Dateien zeigen lassen. Papierkorb ist ein Metadatenflag;
Dateien bleiben erhalten. Es gibt keine automatische Bereinigung verwaister
Dateien, Backups oder älterer Bildhintergründe.

GLib `g_file_set_contents_full(CONSISTENT | DURABLE)` schreibt in demselben
Verzeichnis temporär, synchronisiert und ersetzt atomar. Zuerst wird die bisherige
Revision ebenso in `.bak` gesichert. Schlägt die Sicherung fehl, wird das Original
nicht ersetzt. XOPP wird zuerst vollständig als `.saving` komprimiert und
geschlossen. Neue Bildhintergründe haben neue Namen, damit bestehende Revisionen
währenddessen gültig bleiben. Danach folgt derselbe atomare Ersetzungspfad.
Ein unterbrochenes `.saving` wird nie automatisch geladen. Fehler erscheinen
im Dialog und bei Katalogaktionen zusätzlich in der Statuszeile. Fehlgeschlagenes
Speichern setzt den Änderungsstatus nicht zurück. Nach einem Autosavefehler wird
nicht alle fünf Sekunden ein neuer Dialog erzeugt; explizites Speichern oder
ein Navigationsversuch ermöglicht einen neuen Versuch.

Ein nativer OS-Lock sperrt den Katalog für die gesamte Lebensdauer (POSIX flock,
Windows exklusives CreateFileW). Nach Prozessabbruch gibt das OS ihn frei; die
Lockdatei darf stehenbleiben. Eine zweite Instanz erhält einen sichtbaren Fehler
und keine schreibbare Katalognavigation. Dokument-Schreibzugriffe haben zusätzlich
kurze OS-Locks und SHA-256-Vergleiche seit dem Öffnen/letzten Speichern. Geänderte
Dateien werden nicht still überschrieben. „Speichern unter“ erlaubt das Retten
lokaler Änderungen. Externe Programme, die unsere Locks ignorieren, können
weiterhin selbst Dateien verändern; Netzwerk-/Cloud-Sync-Ordner werden nicht als
verteilte Transaktionsspeicher unterstützt.

Schema 1 ist das erste Foliq-Katalogformat. Es existiert kein altes Schema, das
migriert werden müsste. Version 0, unbekannte neuere Versionen, fehlende
Eltern, Zyklen, ungültige IDs/Typen/Reihenfolgen oder fehlende Seitendateien werden
abgewiesen; kein automatisches Neuanlegen über beschädigten Daten. Das Backup
wird nicht still zurückgespielt (könnte neueres Format zurückstufen): App beenden,
Bibliotheksordner vollständig kopieren und erst dann `catalog.ini.bak` als
`catalog.ini` wiederherstellen. Künftige Migrationen müssen Backup und Tests
mitbringen. Einzelne XOPP-Dateien bleiben ohne Katalog lesbar.

Foliq verwendet jetzt eigene Konfigurations-/Cache-/Datenordner `foliq` und eine
eigene GTK-App-ID. Bestehende Xournal++-Profile werden nicht automatisch migriert.
Das Entwicklungsprogramm und der Windows-Dateiname heißen weiterhin xournalpp.
