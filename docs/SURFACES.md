# Papier und freie Arbeitsfläche

Unter **Einfügen → Seitentyp / Papier** oder **Journal → Page Surface / Paper
Format** wird die aktuelle Dokumentseite eingestellt. Seitentypen können im
selben XOPP-Dokument gemischt werden; der Notizbuchkatalog bleibt unverändert.

## Papier

A3/A4/A5/Letter oder eigene Breite/Höhe in PDF-Punkten (72 pt = 1 Zoll), Hoch-
oder Querformat, Lineatur/Raster, Abstand und Hintergrundfarbe. Vorhandene
Spezialhintergründe lassen sich beibehalten; die bisherigen Papierhintergrund-
und Farbaktionen bleiben erreichbar. Papiermaße im neuen Dialog: 1–14.400 pt.
PDF-/Bildhintergründe bleiben beim Typwechsel am Weltursprung verankert.

## Frei

Das vorhandene GTK-Zeichenfenster zeigt einen verschiebbaren Ausschnitt eines
freien Koordinatenraums, auch links/oberhalb von (0,0). Es wird keine große
Papierseite angelegt. Handwerkzeug oder konfigurierte Touch-Gesten verschieben;
Mausrad bzw. Umschalt+Mausrad verschieben vertikal/horizontal. Strg+Mausrad,
Zoomknöpfe und Pinch verwenden weiterhin die vorhandene Zoomsteuerung.
Der Ausschnitt bleibt beim Zoomen bildschirmfüllend. Zwischen Dokumentseiten
über Seitenvorschau/Navigation wechseln; das Rad navigiert innerhalb der aktiven
freien Fläche. Es sind keine Hintergrundkacheln für die gesamte leere Welt nötig.

Druck, Textmarker, Radierer, Lasso, Verschieben, Skalieren, Formwerkzeuge und
vorhandene Gerätezuordnungen bleiben im gemeinsamen Editor. Handballen- und
Eingabefilter bleiben die vorhandenen geräteabhängigen Funktionen; keine neue
plattformübergreifende Hardwareerkennung wird versprochen. Freie Hintergründe:
Blanko, Linien und Raster. Andere Lineaturen werden beim Kopieren von Papier in
Frei nach Hinweis im Dialog zum Raster. Die bisherigen Hintergrundmenüs weisen
nicht unterstützte freie Lineaturen sichtbar zurück. PDF/Bilder bleiben als
endliche Hintergründe erhalten. Farbe und Raster lassen sich anschließend ändern.

Kamera und Exportoptionen werden mit der Seite gespeichert. Kamera-Schwenks,
freies Zoomen und Seiteneinstellungen sind rückgängig/wiederholbar; aufeinander-
folgende Navigationsereignisse werden für 350 ms zusammengefasst. Bestehende
Werkzeugoperationen behalten ihre Undo-Aktionen. Die numerische Kameragrenze
±985.600 pt lässt Raum bis ±1.000.000 pt. Vektoren/Elemente bleiben unabhängig
von Kamera und leerem Abstand. Es ist keine mathematisch grenzenlose Genauigkeit
behauptet. Elemente außerhalb des sichtbaren Ausschnitts bleiben gespeichert.

## Umwandlung

Ein Wechsel des Seitentyps erstellt **immer eine neue Dokumentseite direkt nach
dem Original**. Alle Ebenen und Elemente werden kopiert, auch ausgeblendete;
Stiftsegmentdruck, Text, Bilder, Formen und Stilwerte bleiben im Modell erhalten.
Die Originalseite wird weder gelöscht noch verschoben. Vor dem Kopieren werden
laufende Text-/Auswahlbearbeitungen über den bestehenden Editor abgeschlossen.

- Papier → Frei: unveränderte Koordinaten und Originalgröße. Die Kamera beginnt
  bei (0,0). Unterstützte Lineaturen bleiben; andere werden nach Dialoghinweis
  als Raster dargestellt. PDF-/Bildhintergrund bleibt an (0,0).
- Frei → Papier: Kopie ohne Skalierung um die negative Kameraposition verschieben;
  damit liegt der aktuelle Ansichtsursprung bei (0,0). Elemente außerhalb des
  gewählten Formats bleiben in der Kopie gespeichert, sind aber auf Papier und
  im Papier-PDF abgeschnitten. **Das vollständige freie Original bleibt erhalten.**
  Der Dialog erklärt diese Grenze vor Anwenden.
- Rückgängig entfernt die Kopie, Wiederholen fügt dieselbe Kopie erneut ein.
  Änderungen am selben Seitentyp tauschen Maße, Hintergrund, Farbe, Kamera und
  Exportoptionen gemeinsam. Ein Abbruch des Dialogs verändert diese Werte nicht.

## PDF-Ausschnitt und Seitenteilung

Im selben Dialog für jede freie Seite einstellen:

- **PDF: entire content** umfasst die Begrenzung aller Ebenen einschließlich
  ausgeblendeter Elemente; deren Sichtbarkeit wird bei der Zeichnung respektiert.
  PDF-/Bildhintergründe gehören ebenfalls zum Inhaltsbereich. Bei leerer Fläche
  dienen die bisherigen Papiermaße als endlicher Exportausschnitt.
- Alternativ X/Y/Breite/Höhe in Weltkoordinaten angeben, auch mit negativen X/Y.
- Mit **split into tiles** Teilseitenbreite/-höhe angeben. Reihenfolge: links
  nach rechts, dann oben nach unten, ohne Überlappung. Randseiten werden auf
  den verbleibenden Ausschnitt verkürzt. Ohne Teilung genau eine Ausschnittseite.

**File → Export as PDF** und der bestehende benutzerdefinierte PDF-Export nutzen
beide diese Einstellungen. Papierseiten behalten ihre Maße. Ausgabe als Vektoren,
keine flächenproportionale Gesamtbitmap. Vor Öffnen der Ausgabedatei werden
Ausschnitte geprüft: maximal 10.000 Teilseiten pro freier Seite; ungeteilte
Seiten maximal 14.400 pt pro Kante. Zu große Ausschnitte erzeugen einen Fehler,
keine implizite Skalierung/Abschneidung. Bestehende Ausgabe bleibt bei diesem
Validierungsfehler erhalten. Die Prüfung läuft derzeit über alle Dokumentseiten,
auch wenn ein engerer Bereich exportiert wird.

PDF-Outlines werden bei Dokumenten mit freien Flächen weggelassen, weil durch
die Teilung andere Seitenzahlen entstehen; das Original behält seine Daten.
Der Dialog weist darauf hin. Direkter GTK-Druck verweist für solche Dokumente
auf PDF-Export und anschließenden PDF-Druck. PNG/SVG verwenden denselben
Inhalts-/Ausschnittbereich, ohne Seitenteilung; PNG wird vor der Allokation auf
16 Megapixel und 16.384 Pixel pro Kante begrenzt. Für größere Ausgaben PDF teilen.

## Speicherung und Kompatibilität

Kein separates Notizformat: bestehendes XOPP plus explizite Attribute an `page`.
Papier ohne Erweiterung bleibt unverändert lesbar und wird als Papier geladen.
Freie Seiten schreiben `foliq-surface-version="1"`, `foliq-camera-x/y`,
`foliq-export-content`, `foliq-crop-x/y/width/height`, `foliq-tiled` und
`foliq-tile-width/height`. Booleans sind exakt 0/1; Zahlen müssen vollständig
parsebar und endlich sein. Fehlende/ungültige Attribute und unbekannte Versionen
brechen das Laden ab. Der Leser korrigiert und überschreibt diese Dateien nicht.
Legacy-Dateien benötigen keine Umschreibung; die fehlende Erweiterung ist Papier.

Elemente bleiben im bisherigen XOPP-Schema. Die `.saving`-/`.bak`- und atomaren
Replace-Pfade, Konflikterkennung und Notizbuch-Autosave werden weiterverwendet;
kein zweiter Schreibpfad für Oberflächendaten. Kopien bleiben im selben Dokument.
Älteres Xournal++ ignoriert freie Metadaten und zeigt nur einen Papierbereich;
beim Zurückspeichern dort gehen die Metadaten verloren. Daher das Foliq-Original
bewahren und PDF bzw. explizite Papierkopien zum Austausch verwenden.

Tests und tatsächlich ausgeführte Prüfungen: [SURFACES-VALIDATION.md](validation/SURFACES-VALIDATION.md).
Konkrete noch ausstehende Windows-Stifttests: [WINDOWS.md](WINDOWS.md).
