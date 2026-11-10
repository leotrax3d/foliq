# Überprüfbare Meilensteine

Jede Stufe aktualisiert HANDOFF und erhält bestehende Funktionen.

1. **M0 – Basis (dieser Schritt):** feste Upstream-Herkunft/Lizenzen,
   dokumentierter reproduzierbarer Buildweg, ausgeführter Linux-Ausgangsbuild
   mit Upstream-Tests soweit Umgebung ermöglicht; Windows-CI mit Testpaket.
   Windows-Lauf erst nach tatsächlichem CI-Erfolg als bestanden werten.
2. **M1 – Lokaler Katalog:** Modell/Schema v1, CRUD für gesamte Hierarchie,
   persistente Reihenfolge und Unterseiten; Tests für Roundtrip, Zyklen,
   Unicode/Windows-Pfade, beschädigte/neue Versionen, atomare Speicherung und
   Migration. UI öffnet echte XOPP und fragt bei ungespeicherten Änderungen.
3. **M2 – Oberfläche:** GTK-Navigation und eigene Gestaltung; App-ID und
   Profilisolation; echte Aktionen, Keyboard/Screenreader/200%-DPI-Abnahme.
   Bestehende Werkzeuge, PDF, Undo und Speichern bleiben erreichbar.
4. **M3 – Text:** zunächst gemischte Stile, dann Absätze, Listen, Tabellen.
   Fixtures für Laden/Speichern/Migration, Undo, Clipboard, Suche und PDF;
   identischer Inhalt nach Neustart. Handschrift und Text gemeinsam editierbar.
5. **M4 – Arbeitsfläche:** Papier-/Auto-Grow-Modus, Undo, Stressmessung,
   definierte Export-Seitenteilung, kein Verlust von Elementen außerhalb des
   ursprünglichen Papiers; Stift bei Zoom/DPI und an Wachstumsgrenzen testen.
6. **M5 – Organisation/Medien:** Anhänge, Tags, Aufgaben, globale lokale Suche;
   Audio stabilisieren und Formeln offline einrichten; beschädigte/fehlende
   Medien und Wiederherstellung testen. Index ist aus Quelldaten rekonstruierbar.
7. **M6 – Erkennung/Schutz:** geprüfte FOSS-Offline-OCR/Handschriftmodelle,
   Versions-/Sprachmodellangaben, korrigierbare Erkennung; anschließend
   authentifizierter Passwortschutz inklusive Autosave/Index/Medien und
   dokumentierter Wiederherstellungsgrenzen. Kryptografie extern prüfen lassen.
8. **M7 – Windows-Release:** signierte oder klar als unsigniert gekennzeichnete
   Pakete, vollständige Lizenz-/Quellpakete, reproduzierbare Dependency-Snapshots,
   Upgrade-/Uninstall-/Offline- und Hardwarematrix. Erst dann Release-Bezeichnung.

M0 ist kein Abschluss des Produktumfangs. Keine der späteren Stufen ist allein
durch ihre Dokumentation implementiert.
