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
