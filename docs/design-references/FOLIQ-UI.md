# Foliq Hauptoberfläche

GTK3/C++ und der gemeinsame Xournal++-Editor bleiben erhalten. Links stehen
Notizbücher, Gruppen und Abschnitte; die zweite Spalte zeigt ausschließlich
Seiten/Unterseiten des gewählten Abschnitts. Eingabe/Doppelklick öffnet eine
Seite. Die Dreiecke bzw. Links/Rechts klappen die Hierarchie ein und aus.
Die Trennleisten lassen sich ziehen und über GTK per Tastatur bedienen.

Vorlagen: Repository-Bilder 2731–2735 (Werkzeugreiter/Fokus) sowie der am
2026-11-11 betrachtete [Joplin-Desktop-Screenshot](https://raw.githubusercontent.com/laurent22/joplin/dev/Assets/WebsiteAssets/images/DemoDesktop.png),
verlinkt von https://joplinapp.org/help/apps/desktop/ (Organisation/Seitenliste).
Externe Bilder dienen nur der Betrachtung, keine Übernahme in Programmassets.
Die schon vorhandenen Lucide-Icons und ihre ISC-Hinweise bleiben erhalten.
Keine neue Bibliothek und keine Microsoft-Assets.

## Grundlagen

Zentrale Regeln: `ui/xournalpp.css`, begrenzt auf `foliq-ribbon` und
`foliq-navigation`. GTK-Systemschrift in relativer Größe (1em), Betriebssystem-
Skalierung und native Hell-/Dunkel-Themen. Ein Akzent #5269b8, Fokus #738bdf; dunkle Flächen #20252e/#2b323e,
Text #edf0f7 und Rahmen #455064 sichern auch bei GTK-Themen ohne dunkle Variante
den Kontrast der neuen Oberfläche;
Text/Flächen folgen GTK, Dokumentpapier folgt weiterhin dem Dokument.
Abstände: 4 px kleine Gruppen, 6 px Werkzeugabstände, 8 px Navigationsränder,
9 px vertikaler Zeileninnenraum. Beschriftungen bleiben sichtbar, lange Titel
werden ellipsiert. Auswahl: Akzent mit Weiß; Fokus: 2-px-Ring; deaktivierte
Undo/Redo-Aktionen folgen dem tatsächlichen Undo-Stack (250-ms-Aktualisierung).

Start, Einfügen, Zeichnen und Ansicht rufen bestehende Control-Aktionen auf.
Die bisherigen Menüs bleiben erreichbar. „Zeichnen → Weitere Werkzeuge“ bzw.
„Ansicht → Alle Werkzeuge“ blendet die konfigurierbaren Werkzeugleisten für
Farben, Strichstärken und sämtliche Spezialwerkzeuge ein. Neue Profile starten
mit der kompakten Oberfläche; vorhandene Profile behalten ihre Einstellungen. Suchen durchsucht das aktuelle Dokument; globale
Notizbuch-Volltextsuche, OCR und Rich Text werden nicht angeboten.

Drag-and-drop: obere/untere Zeilenkante sortiert, Zeilenmitte verschiebt in den
Zielknoten. Der Katalog validiert Typen und Zyklen, inklusive Unterseiten.
Gleichwertige Wege: Aktionen → Verschieben, ↑/↓, Alt+↑/↓; F2 benennt um.
Die Aktionsmenüs sind über Tab und Leertaste zugänglich. Namen der beiden
Navigationsbäume und des Farbschemafeldes sind für ATK gesetzt.

## Ansichtsoptionen

`library/view.ini`, Version 1, speichert beide Trennleisten, Fokus, Farbschema,
Werkzeugreiter, Abschnitt und eingeklappte/aufgeklappte Knoten beim regulären
Beenden/Seitenwechsel. Atomare Speicherung und `.bak` über denselben bestehenden
Storage-Pfad wie der Katalog. Keine Änderung am Katalog- oder XOPP-Format.
Fehlende Datei erhält Defaults; fehlerhafte/unbekannte Version wird sichtbar
abgewiesen und nicht überschrieben. Zur Wiederherstellung App beenden und
`view.ini` nach Sicherung aus dem Bibliotheksordner verschieben. Keine Migration
vorhanden: dies ist die erste Version dieses optionalen Formats.
