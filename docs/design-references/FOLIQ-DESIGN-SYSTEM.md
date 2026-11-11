# Foliq Designsystem (Fluent-orientiert, GTK3) – Stand 2026-11-11

Eigene Umsetzung nach den öffentlichen Prinzipien von Microsoft Fluent 2 /
Windows 11 (ruhige Flächen, Ebenen, 4-/8-px-Raster, deutliche Rundungen,
gezielte Schatten). Keine Microsoft-Assets, Schriften oder Icons übernommen.
Icons: Lucide (ISC, siehe copyright.txt), 24er Raster, Strich 2 px, angezeigt
mit 16 px (kompakt) bzw. 20 px (Berührung). Neue Foliq-Icons heißen `foliq-*`.
Rechtfertigung für OneNote-ähnliche Struktur: Menüband mit Registerkarten,
Notizbuch-/Abschnitts-/Seitenspalten; nur Strukturidee, keine Gestaltungskopie.

## Analyse der alten Oberfläche (Screenshots `docs/validation/fluent/before-*`)

Der „KDE/klassisch"-Eindruck entstand durch: klassische Menüleiste ganz oben;
GtkNotebook-Reiter mit grauem Kopfbalken und Rahmen; gewölbte Schaltflächen mit
Rahmen und Verlauf, alle mit Text; Großbuchstaben-Spaltenköpfe und breite
„+ Neu"-Knöpfe; Kästen in Kästen (weiße Listen auf grauem Grund); schwarze
Dreiecks-Expander; 4-px-Rechteck in Signalrot um die aktuelle Seite; graue
Paned-Griffe; Statuszeile ohne Hierarchie.

## Tokens (`ui/xournalpp.css`, Laufzeit-Overrides in `src/core/folio/Theme.cpp`)

| Token | Hell | Dunkel | Zweck |
|---|---|---|---|
| foliq_bg | #f3f3f3 | #202020 | Shell (deckende Mica-Alternative) |
| foliq_layer | #fbfbfb | #272727 | Menüband-Ebene |
| foliq_card | #ffffff | #2c2c2c | Schwebende Ebenen, Dialoge, Popover |
| foliq_text / _secondary / _disabled | #1b1b1b / #5f5f5f / #a0a0a0 | #fff / #c8c8c8 / #787878 | Hierarchie |
| foliq_stroke / _strong | 8 % / 16 % Schwarz | 8 % / 16 % Weiß | nur wo nötig |
| foliq_accent | Nutzerwahl, Standard #005fb8 | aufgehellt bis ≥ 3:1 | Auswahl-/Reiterindikator |
| foliq_on_accent | Schwarz/Weiß nach höherem Kontrast | | Text auf Akzentflächen |
| foliq_focus | #1a1a1a | #ffffff | Fokusring, akzentunabhängig |
| foliq_danger | #c42b1c auf #fde7e9 | #ff99a4 auf #442726 | Speicherfehler |
| Arbeitsfläche | #e7e7e9 | #191919 | nur wenn Upstream-Standard nicht geändert |

Kontrastregel (getestet in `FoliqThemeTest`): beliebige Akzentfarbe wird für
Indikatoren so lange abgedunkelt/aufgehellt, bis ≥ 3:1 gegen die Shell (WCAG
1.4.11); Text auf Akzentflächen ≥ 3:1 (für große/fette Schrift; normaler Text
liegt nie auf freier Akzentfläche – ausgewählte Zeilen nutzen 13 %/26 % Tönung
mit normaler Textfarbe). Abschnittsfarben sind nur ein Punkt-Marker; die
Auswahl daneben bleibt Text-/Fokusfarbe.

Typografie: Systemschrift (Windows: Segoe UI über GTK), 1em Basis, Titel 600,
Sekundärtext 0.92em. Abstände 2/4/6/8/12/16 px. Radien: 4 px (Menüeinträge,
Checkbox), 6 px (Buttons, Felder, Zeilen), 8 px (Menüband-Ebene, Popover,
Menüs). Schatten nur für schwebende Ebenen (0 8 16 14 %). Übergänge 83 ms
(Hover) / 120 ms (Registerwechsel); „Weniger Bewegung" schaltet alle aus.

Zustände: Hover 4 %/6 % Tönung · gedrückt 7 %/10 % · ausgewählt Akzent-Tönung
+ Akzent-Symbol · Fokus 2-px-Ring mit 1 px Abstand · deaktiviert 40 % Symbol,
Sekundärtext · Fehler Rot-Pille / roter Feldrahmen.

## Komponenten und Verhalten

- **Befehlsleiste**: Navigation-Umschalter · Registerkarten (Pivot mit 3-px-
  Akzentindikator) · Pfad Notizbuch › Abschnitt › Seite · dezenter Status
  („Gespeichert"; Speicherfehler als rote Pille) · Suche · Rückgängig ·
  Wiederholen · Speichern · ⋯ (alle Upstream-Menüs, F10) · Menüband anheften.
  Unter 1100 px werden Marke, Pfad und Suchtext ausgeblendet.
- **Menüband**: eine abgerundete Ebene, Icon-Befehle mit Tooltip und
  zugänglichem Namen, Gruppen durch dünne Trenner. Eingeklappt (Strg+F1):
  Registerkarte öffnet es temporär **schwebend über** der Seite (kein Umbruch
  der Arbeitsfläche); schließt nach Befehl, Escape oder Klick daneben. Der
  Klick daneben wird nicht verbraucht – ein dort begonnener Strich läuft weiter.
  Befehlsschaltflächen nehmen beim Klick keinen Fokus (Texteingabe bleibt aktiv).
- **Navigation**: eine Ebene ohne innere Karten; Titel statt Großbuchstaben;
  „+"/Pin als Icons; Chevron-Expander; Abschnittsfarben über Aktionen → Farbe…
  Nicht angeheftet schwebt sie über der Seite und schließt nach dem Öffnen
  einer Seite. Fokusmodus (Strg+Umschalt+F) blendet Navigation aus und klappt
  das Menüband ein. Strg+Umschalt+N Navigation, F6/Umschalt+F6 Bereichswechsel.
- **Dichte**: Kompakt / Berührung (größere Trefferflächen, Icons 20 px,
  Zeilenhöhe +8 px, Menüeinträge 11 px Innenabstand). Gleiche Befehle.
- **Design**: System / Hell / Dunkel, Akzentfarbe (8 eigene Swatches),
  Weniger Bewegung. Keine Transparenz/Mica: GTK3 unter Windows bietet dafür
  keinen unterstützten Weg; deckende Flächen sind die bewusste Alternative.
- **Arbeitsfläche**: Papierseiten mit dezentem Schatten auf ruhiger Fläche;
  aktuelle Seite mit 2-px-Akzentkontur (Radius 4) statt rotem Rahmen. Eine in
  den Einstellungen geänderte Rahmen-/Hintergrundfarbe hat Vorrang. Freie
  Arbeitsfläche: Canvas nimmt die Papierfarbe an, kein Schatten, keine Kante.
- **Dialoge/Menüs/Popover**: jedes Toplevel erhält Klasse `foliq-app`
  (Realize-Hook), damit Einstellungen, Eingabe-, Fehler- und Upstream-Dialoge
  dieselben Buttons, Felder, Pill-Reiter, Karten und Primärbutton nutzen.

## Persistenz `library/view.ini` (Version 2)

Neue Schlüssel: `ribbon`, `navpin`, `touch`, `motion`, `accent`, `colors`.
Version 1 wird beim Lesen mit Standardwerten migriert und beim nächsten
regulären Speichern als v2 geschrieben (atomar, `.bak` wie bisher). Unbekannte
Versionen oder ungültige Werte: sichtbare Fehlermeldung, Datei unverändert.
Abschnittsfarben gelöschter Knoten werden beim Speichern entfernt. Katalog-
und XOPP-Format unverändert.

## App-Icon (2026-11-11, vom Nutzer bestätigt)

`ui/pixmaps/org.foliq.Foliq.svg` (+ PNG 256 px, Vorschau `foliq-icon-preview.png`):
flache abgerundete Kachel (Radius 56/256) in Akzentblau #005fb8, Glyphe Lucide
„notebook-pen" (ISC) in Weiß, Stift in #ffd166. Keine Verläufe, keine Schatten.
Fenster-, Dialog-, EXE- (über `FOLIQ_APP_ICON` → .ico) und Installer-Icon.
