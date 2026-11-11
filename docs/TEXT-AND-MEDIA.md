# Text, Tabellen, Bilder und Formeln

Foliq erweitert den vorhandenen GTK3-Editor. Es gibt keinen Webeditor und keine
neue Laufzeitbibliothek. Alle Inhalte bleiben Elemente derselben XOPP-Seite und
werden mit derselben Auswahl und denselben Seitenkoordinaten verschoben.

## Text bearbeiten

Mit **Start → Text** auf einen Container oder eine freie Stelle klicken.
**Text formatieren…**, **F4** oder **Strg+Umschalt+E** öffnet die ausführliche
Bearbeitung. Der Dialog verwendet denselben GtkTextBuffer wie die Leinwand.
Formatierung gilt für die Auswahl, ohne Auswahl für die aktuelle Textzeile.
Auf einer leeren Zeile vorgemerkte Formatierung gilt für anschließend eingefügten
Text. Absätze entstehen durch Enter; Textcontainer wachsen mit ihrem Inhalt.

- Absatz sowie Überschrift 1/2 sind Stilvorgaben (12/24/18 Seitenpunkte); keine
  separate Gliederungs-/Inhaltsverzeichnisstruktur.
- Schriftfamilie und Größe, fett/kursiv/unterstrichen, Text- und Hintergrundfarbe
  lassen sich innerhalb eines Containers mischen. Strg+B/I/U formatiert direkt
  auf der Leinwand; Strg++/− ändert die Größe im aktuellen Bereich.
- Listen, Nummerierung, Ein-/Ausrücken und umschaltbare ☐/☑-Aufgaben wirken auf
  markierte Zeilen. Marker und Einrückung sind bearbeitbarer Unicode-Klartext,
  keine versteckte Aufgaben-Datenbank. Auf der Leinwand setzt Enter Listen fort;
  Enter auf einem leeren Listeneintrag beendet die Liste. Im Dialog können
  weitere Zeilen mit den Listenaktionen formatiert werden.
- **Link / Seite** oder Strg+K fügt einen Link hinzu, ändert oder entfernt ihn.
  Erlaubt: http, https, mailto und `page:2` für die zweite physische Seite des
  aktuellen XOPP-Dokuments. **Link öffnen** bzw. Strg+Enter auf der Leinwand
  folgt dem Verweis; fehlende Seiten melden einen Fehler. Seitenverweise sind
  ausdrücklich Seitenzahlen, keine stabilen Katalog-UUIDs; nach Umsortieren
  gegebenenfalls anpassen. Kein Verweis auf eine andere Notizbuchdatei.
- **Formatierung löschen** entfernt auch Verweise im Bereich.

Strg+C/X/V verwendet GTK-Klartext und das Foliq-GTK-Rich-Text-Zwischenablageformat.
Kopieren zwischen Foliq-Textcontainern erhält die unterstützten Stile. Externe
Programme erhalten Klartext. HTML/RTF aus Browsern/Office wird derzeit als
Klartext eingefügt; es gibt keinen HTML-Importer. **Strg+Umschalt+V** erzwingt
Klartext ohne geerbte Zeichenformatierung und vereinheitlicht CRLF/CR zu LF.
Eingabemethoden bleiben bei GtkIMMulticontext bzw. dem nativen GtkTextView;
keine Verarbeitung von Tastencodes in einem eingebetteten Browser.

## Bearbeitbare Tabellen

**Tabelle…** im Textdialog bzw. Strg+Umschalt+T auf der Leinwand wandelt den
aktuellen Container in eine Tabelle um. Vorhandener Text wird in die erste
Zelle übernommen, Zeilenumbrüche dabei zu Leerzeichen. **Abbrechen** verändert
den Container nicht. Eine Tabelle ist ein eigener Textcontainer; Fließtext und
mehrere Tabellen lassen sich als getrennte Container frei anordnen.

Der Zelleneditor öffnet sich beim erneuten Bearbeiten der Tabelle. Jede Zelle
ist ein GtkTextBuffer, kein Bild. Tab wechselt zwischen Zellen. Auswahlfelder
bestimmen, hinter welcher Zeile/Spalte eingefügt bzw. welche gelöscht wird.
Strukturänderungen bleiben bis **Übernehmen** im Dialog; der gesamte Vorgang
ist ein Undo-Schritt. Bestehende Zeichenformatierungen und formatierte interne
Zwischenablage bleiben bei Zeilen-/Spaltenoperationen in den Zellen erhalten.

Version 1 unterstützt rechteckige Tabellen mit einem Absatz je Zelle, höchstens
32 Spalten und 1000 Zeilen. Keine verbundenen Zellen, Formeln zur Berechnung oder
mehrzeiligen Zellen. Ungültige Einfügungen werden mit einer Meldung abgewiesen.
Alle Spalten sind gleich breit; Breite und Zeilenhöhe wachsen mit dem Inhalt.
Das Raster verwendet dieselben Pango-Maße auf Leinwand und im PDF. Es wird
nicht lediglich gerendert: Zellen und Struktur sind nach Speichern/Neustart
wieder bearbeitbar. Das Löschen einzelner Trennzeichen auf der Leinwand wird
verhindert; Strukturänderungen erfolgen im Zelleneditor.

## Bilder

Bilder mit dem bestehenden Bildwerkzeug oder der Zwischenablage einfügen und
mit der gemeinsamen Auswahl verschieben/vergrößern. Einzelbilder werden
proportional skaliert. **Einfügen → Bildgröße / Zuschnitt…** bearbeitet genau
ein ausgewähltes Bild. Breite in Seitenpunkten und vier prozentuale Grenzen
beziehen sich auf das Original. Rechts/unten müssen hinter links/oben liegen;
der Ausschnitt muss je Richtung mindestens 0,1 % des Originals groß sein.

Die linke obere Seitenposition bleibt bestehen, die Höhe folgt dem
Seitenverhältnis des Ausschnitts. Das Original bleibt vollständig eingebettet;
0/0/100/100 stellt es wieder her. Bei alten bereits verzerrten Bildern dient
das bisherige Verhältnis als Ausgangspunkt. Kein destruktives Abschneiden von
Quelldaten. Zuschnitt und Größe sind gemeinsam ein Undo-Schritt.

## Lokale, erneut bearbeitbare Formeln

**Einfügen → Formel…** nutzt den bestehenden LaTeX-Dialog. Zum Ändern zuerst die
Formel mit dem Auswahlwerkzeug auswählen und denselben Befehl aufrufen. XOPP
speichert den LaTeX-Quelltext und das erzeugte PDF gemeinsam. Anzeigen, Bewegen,
Skalieren und Exportieren benötigen nach dem Speichern kein installiertes TeX.
Eine Änderung ersetzt die bisherige Formel erst nach erfolgreichem Rendern;
Syntax-/Paketfehler zeigen Status und Compilerprotokoll, OK bleibt gesperrt.
Abbrechen erhält das Original. Ersetzen ist eine gemeinsame Undo-Aktion mit
erhaltener Ebenenposition, keine getrennte Löschung und Einfügung.

Zum **Erstellen/Ändern** benötigt die Standardkonfiguration lokal `pdflatex`
und die Pakete der Vorlage `resources/default_template.tex`: standalone,
varwidth, amsmath, amssymb, scontents, ifthen und xcolor samt TeX-Basispaketen
und Fonts. Unter Ubuntu 24.04 wurden `texlive-latex-extra` und
`texlive-fonts-recommended` verwendet. Die Legacy-Vorlage ist für ältere
TeX-Installationen weiterhin auswählbar. Einstellungen → LaTeX verwaltet
Generatorbefehl und Vorlage; Pfade mit Leerzeichen müssen korrekt zitiert sein.

Windows: TeX Live oder MiKTeX vorab lokal installieren, benötigte Pakete/Fonts
bereitstellen und den TeX-Programmordner in PATH aufnehmen; Foliq anschließend
neu starten. Automatische Paketnachinstallation in MiKTeX deaktivieren bzw.
alle benötigten Pakete vor dem Offline-Test installieren. Foliq lädt keine
TeX-Pakete nach. Diese Änderung bündelt keine TeX-Distribution. TeX Live ist
eine Sammlung mit unterschiedlichen freien Lizenzen; LaTeX-Komponenten stehen
u.a. unter LPPL, MiKTeX besitzt eigene GPL-/Komponentenhinweise. Eine spätere
Bündelung erfordert eine paketgenaue Lizenz-/Quellprüfung. Bestehende GTK/Pango/
Cairo/Poppler-Lizenzen und Verteilungspflichten bleiben unverändert.

## Koordinaten, Überlagerung, Größe und Undo

Document → XojPage → Layer → Element bleibt das einzige Seitenmodell.
Elementpositionen/-größen sind Seiten-/Weltpunkte; PageView übernimmt Zoom,
Kamera und Fenstertransformation. Freie Seiten behalten auch beim Einfügen
negative Koordinaten. Bildimport begrenzt die Größe dort nicht an Papierkanten. Text, Tabellen, Bilder, Formeln und Stiftstriche lassen
sich gemeinsam auswählen, verschieben, speichern und über DocumentView
exportieren. Es gibt keinen automatischen Textfluss um überlagerte Objekte.

Die Reihenfolge der Ebenen und ihrer Elemente bestimmt die Überlagerung.
Textbearbeitung trifft das oberste Textobjekt der aktiven Ebene. Bearbeitung,
Zuschnitt und Formelersetzung behalten den bisherigen Index; Undo/Redo stellt
denselben Index wieder her. Vorhandene Anordnungsbefehle bleiben verfügbar.
Hintergrundfarbe gehört zu Zeichenbereichen, nicht zu einem opaken Container.

Text und Tabellen wachsen beim Tippen; eine Auswahl-Größenänderung skaliert
Schriftgrößen proportional statt den Text neu umzubrechen. Tabellenraster und
Schrift wachsen zusammen. Bilder/Formeln behalten bei Einzelauswahl ihr
Seitenverhältnis. Im Papiermodus wächst die Seite nicht automatisch: Inhalte außerhalb des
Papiers sind im Papierexport abgeschnitten. Freie Seiten verwenden den bereits
implementierten Kamera-/Inhaltsausschnitt und dessen gekachelten PDF-Export
(siehe [SURFACES.md](SURFACES.md)). Alle neuen Elemente bleiben in beiden
Seitentypen im selben Modell.

Während einer Textsitzung führen Strg+Z/Y und die Undo-Schaltflächen den lokalen
Verlauf einschließlich Formatierung/Tabellen aus (bis 100 Zustände, ungefähr
32 MiB Text-/Spanbudget). Beim Verlassen wird das Ergebnis als eine Aktion in
den bestehenden Dokument-Undo-Verlauf übernommen. Unverändertes Öffnen erzeugt
keine Aktion. Ein rückgängig machbarer Kamera-Schwenk/Zoom auf einer freien
Seite schließt die laufende Textsitzung vorher ab; so folgen Text und Kamera
im gemeinsamen Dokumentverlauf in der richtigen Reihenfolge. Speichern beendet die aktive Textsitzung wie im bestehenden Editor.
Aktive Eingabe ist vor deren Abschluss noch kein separat gespeicherter Zustand.

## Dateiformat und Export

Neue Inhalte werden innerhalb des vorhandenen atomaren XOPP-Speicherwegs samt
Backup gespeichert; kein Sidecar und kein zweites Dokumentformat. Dateien ohne
Rich Text/Zuschnitt bleiben bei XOPP-Version 4. Neue Inhalte setzen Version 5
und `foliq-content-version="1"`. Ältere Xournal++-Leser zeigen damit ihre Warnung
vor einer neueren Version; dort nicht überschreiben, sonst können Erweiterungen
verloren gehen. Foliq lehnt unbekannte Inhalts-/Zuschnittversionen ab.

`<text>` behält Klartext, Grundschrift/-farbe und Koordinaten. Das zusätzliche
Attribut `foliq-rich` enthält Base64-kodierte UTF-8-GKeyFile-Daten: Gruppe `rich`
mit `version=1`, `text`, `columns` (0 für normalen Text), `count`; je `spanN`
`start`, `end`, `kind`, `value`. Grenzen sind Unicode-Codepoints, exklusives Ende;
Pango erhält erst beim Rendern daraus UTF-8-Bytegrenzen. Unterstützte Stile sind
bold/italic/underline (0/1), font, size, foreground/background und link.
Klartext und strukturierter Inhalt müssen übereinstimmen. Textlimit 8 MiB,
100000 Stilbereiche; ungültige/neue Daten werden nicht stillschweigend verworfen.
Ein reines Version-4-Textobjekt migriert ohne Stile beim Bearbeiten; Migration,
Unicode, lange XML-Parserblöcke und erneuter Roundtrip werden getestet.

Tabellen verwenden dieselben Stilbereiche; TAB trennt Zellen, LF trennt Zeilen,
`columns` validiert die rechteckige Struktur. Bildattribute sind
`foliq-crop-version="1"`, `crop-left/top/right/bottom` im Bereich 0–1, zusätzlich
zum vollständigen Bild. Objektzwischenablage verwendet `TextRichV1` bzw.
`ImageCropV1`; alte Klartext-/Bildobjekte bleiben lesbar. TeX benutzt weiterhin
das vorhandene `teximage` mit Quelltext und PDF.

PDF, SVG und PNG benutzen den gemeinsamen Renderer. PDF bewahrt auswählbaren
Unicode-Text und das sichtbare Tabellenraster/Zuschnitt/Formelbild; es ist kein
Foliq-Bearbeitungsformat. Eigene Textlinks werden derzeit visuell exportiert,
aber nicht als klickbare PDF-Annotation. Den alten XOJ-Export verweigert Foliq
bei Rich Text, Tabellen und Zuschnitten vor dem Schreiben, mit Hinweis auf
XOPP/PDF/SVG/PNG, um unbeabsichtigten Datenverlust zu verhindern.

Ausgeführte Linux-Prüfungen und offene Windows-/IME-/Stiftabnahme:
[TEXT-MEDIA-VALIDATION.md](validation/TEXT-MEDIA-VALIDATION.md).
