# Architektur und Entscheidungen

## ADR-001: GTK3/C++-Editor erhalten (angenommen, 2026-11-10)

Basis ist Xournal++ v1.2.8, C++17, CMake/Ninja, GTK3/GDK, Cairo/Pango,
Poppler und bestehende Audio-/ZIP-/Lua-Integration. Kein Frameworkwechsel,
kein eingebetteter Webeditor. Die enge Verbindung zwischen Stift-Latenz,
Undo, Selektion, PDF und Rendering macht einen Austausch derzeit riskant;
es gibt keinen belegten technischen Grund dafür.

### Kopplung am tatsächlichen Code

- `src/core/control/Control.cpp` verbindet Fenster, aktives Document, Aktionen,
  Werkzeuge, Speichern und Titel. UI und Dokumentsteuerung sind eng gekoppelt.
- `src/core/gui/MainWindow.cpp`, `ui/` und `src/core/gui/sidebar/` bilden GTK-UI,
  Menüs und aktuelle Seiten-/Ebenenvorschau. Die Sidebar ist dokumentbezogen,
  kein bestehendes Notizbuchmodell.
- `src/core/gui/inputdevices/InputContext.*` verteilt normalisierte Eingaben an
  Stylus/Pen/Mouse/Touch-Handler. `PageView` vermittelt Koordinaten, Zoom,
  Seitenauswahl und Werkzeugaktionen. GDK-Eingaben nicht separat nachbauen.
- `src/core/model/Document.*` hält Seiten; `XojPage.*` hält endliche Breite/Höhe,
  Ebenen und Hintergründe; Elemente umfassen Stroke, Text, Image usw.
  Änderungen müssen die bestehenden Notifications und Undo-Pfade durchlaufen.
- `src/core/view/DocumentView.*`, `LayerView.*`, `StrokeView.*`, `TextView.*`
  zeichnen über Cairo/Pango; `PageView` koordiniert die Bildschirmdarstellung.
  PDF- und Exportpfade müssen dieselben Elemente verstehen.
- `src/core/control/tools/TextEditor.*` nutzt GtkTextBuffer für interaktive
  Bearbeitung; `model/Text.h` speichert einen String und ein XojFont pro Objekt.
  GtkTextBuffer-Tags allein würden beim Speichern verloren gehen: Rich Text
  benötigt Modell, Serialisierung, Renderer, Clipboard und Undo gemeinsam.
- `src/core/control/xojfile/LoadHandler.*`/`SaveHandler.*` speichern XOPP;
  `src/config.h.in` definiert FILE_FORMAT_VERSION (Basis: 4 aus CMake). Objektserialisierung für
  Undo/Clipboard ist ebenfalls zu berücksichtigen.

## ADR-002: Navigation getrennt vom Seiteninhalt (angenommen)

Nächster Schnitt ist ein lokaler Katalogdienst ohne GTK-Abhängigkeit plus
GTK-Navigationsadapter; er öffnet XOPP im bestehenden Control. Stabile IDs,
relative Pfade und explizite Sortierung statt Dateinamen als Identität.
Geplantes versioniertes Manifest außerhalb XOPP enthält Notizbücher,
Abschnittsgruppen, Abschnitte, Seiten-/Unterseitenbeziehungen und Metadaten.
Format und Speicherbibliothek sind noch nicht festgelegt. Vor Einführung:
Schema v1, Zyklus-/Pfadvalidierung, atomare Speicherung, Backup und Migrationstest.
Ein fehlender Katalog darf vorhandene XOPP-Dateien nicht unlesbar machen.

## ADR-003: Rich Text inkrementell (angenommen)

Zunächst versionierte Text-Runs mit klaren Unicode-Offsets, danach Absätze,
Listen und Tabellen. Rendering mit Pango und Bearbeitung im bestehenden
TextEditor. Gemischte Stile müssen Speichern/Laden, Undo/Redo, Copy/Paste,
PDF-Export und Suche überstehen. Ältere Leser: explizite Warnung und/oder
verlustarmer Plaintext-Export, niemals stillschweigender Datenverlust.
Kein zweiter unabhängiger Texteditor.

## ADR-004: Wachsende endliche Seite vor unendlichem Raum (angenommen)

Auto-Grow als kontrollierte Vergrößerung einer XojPage über Document::setPageSize
und vorhandene Größenänderungs-/Undo-Pfade (`PageSizeChangeUndoAction`) untersuchen. Nicht nur den View
strecken: Hintergründe, Scrollgrenzen, Cache, Selektion und PDF-Seitenteilung
müssen folgen. Wachstum in begrenzten Schritten und Größenlimits; Negative
Koordinaten und vollständig unendlicher Raum bleiben vorerst außerhalb des
ersten Prototyps. Modus später versioniert pro Notiz speichern.

## Grenzen und Risiken

Windows-GDK/Stiftdruck und Palm-Rejection müssen auf Hardware getestet werden.
Große Seiten können Speicher und Renderzeit sprengen. OCR und Handschriftmodelle
brauchen getrennte Offline-, Lizenz-, Downloadgrößen- und Windows-Prüfung.
Verschlüsselung braucht Bedrohungsmodell, Schlüsselableitung und Behandlung
von Autosave, Anhängen, Suchindex und temporären Dateien. Noch keine Auswahl
neuer Bibliotheken. Konfigurations-/App-ID-Isolation vor paralleler Installation
mit Xournal++ umsetzen. Upstream-Netzwerkaktionen/Plugins vor Release auditieren.

## ADR-005: Lokaler Katalog v1 und Dokumenttransaktionen

Implementiert unter `src/core/folio/`: GTK-unabhängiger Katalogkern mit GKeyFile,
OS-Sperre, validierten UUID-Beziehungen und atomarem Replace; GTK-Adapter öffnet
die XOPP-Dateien über Control. Keine SQLite-Abhängigkeit. Details, Konsistenzregel
„Inhalt vor Referenz“, Wiederherstellung und Grenzen: [NOTEBOOKS.md](NOTEBOOKS.md).
SaveJob behält Original und Backup bis zum erfolgreichen Commit; Bildhintergründe
werden vorab unter neuen Namen geschrieben. Der synchrone Fehlerpfad in Control
setzt den Dirty-Zustand jetzt nur nach Erfolg zurück. Foliq-Profil und GTK-App-ID
sind von Xournal++ getrennt.

## ADR-006: Freie Weltkoordinaten und begrenzte Ansichtsfenster (2026-11-11)

Ersetzt ADR-004 für die jetzt geforderte freie Arbeitsfläche. Kein Auto-Grow einer
riesigen Papierseite: `PageSurface` trennt Papier von freiem Koordinatenraum.
`XojPage::width/height` bleiben Papier-/Fallbackmaße, niemals Weltgrenzen.
Freie PageViews belegen ein Ansichtsfenster in der vorhandenen Dokumentanordnung;
dessen Bildschirmgröße bleibt beim Zoomen konstant. Die Kamera verschiebt die
Projektion, ohne Elemente umzuschreiben. `getViewportX/Y` liefern Layoutkoordinaten,
`getX/Y` den projizierten Weltursprung für die bisherigen Werkzeugpfade. GTK-
Integergrenzen begrenzen die Kameraposition auf ±985.600 pt; der deklarierte
Koordinatenbereich ist ±1.000.000 pt einschließlich des sichtbaren Ausschnitts.
Das ist eine numerische Sicherheitsgrenze, keine vorab angelegte Fläche.

Der freie Bildschirmrenderer zeichnet direkt in Cairos sichtbaren Clip. Kein
seitergroßer Bitmapcache, keine Allokation von Weltkacheln für leere Bereiche.
LayerView prüft Element-Bounding-Boxes gegen den Clip; die Traversierung bleibt
O(Anzahl gespeicherter Elemente), unabhängig vom Abstand zwischen ihnen.
Textmarker- und Radiermasken werden auf den Clip begrenzt. Großflächige Pfade
werden vor dem Rasterizer mit Double-Koordinaten zugeschnitten (Polygon- bzw.
Segment-Clipping), um dessen interne Rastergrenzen nicht zu überschreiten.
Temporäre Auswahl-Bitmaps sind auf 2.048 Pixel pro Kante beschränkt; die endgültige
Transformation verwendet weiterhin die originalen Vektoren. Vorschauen erhalten
begrenzte Ausgabeflächen und passen den Inhaltsbereich ein. Keine zusätzliche
Bibliothek und kein zweiter Editor.

`AbstractInputHandler` und bestehende Druck-/Gerätefilter bleiben erhalten.
Gemeinsame Ursprungstransformation gilt für Schreiben, Radieren, Lasso, Formen,
Verschieben und Skalieren. Papier-Clamps entfallen nur für freie Seiten.
Hand/Touch/Mausrad laufen über Layout::scrollRelative; Zoom hält den Weltpunkt
unter dem Zeiger fest. Freies Schwenken und Zoomen besitzen gemeinsam mit
Seiteneinstellungen Undo/Redo; zusammenhängende Navigationsereignisse werden
innerhalb 350 ms zusammengefasst, niemals über einen Speicherpunkt hinweg.
Programmgesteuertes Wiederherstellen der Zoom-Metadaten verändert keine Kamera
und erzeugt keinen Bearbeitungseintrag.

XOPP-Erweiterung v1, Validierung, Kompatibilität, kopierende Umwandlung und
Exportgrenzen stehen in [SURFACES.md](SURFACES.md). Der bestehende SaveJob behält
atomaren Staging-/Backup-/Konfliktschutz. PDF-Ausgabe verwendet dieselben Elemente,
clippt wählbare Ausschnitte und adressiert Teilseiten einzeln; keine große
Zwischenbitmap. Direkter GTK-Druck freier Dokumente verweist ausdrücklich auf
diesen PDF-Pfad, statt unbemerkt bei (0,0) abzuschneiden.
