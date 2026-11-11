# Windows-Testpaket und manuelle Abnahme

## CI starten und herunterladen

Im Repository leotrax3d/foliq unter Actions `Foliq Windows test package`
öffnen. Der Workflow läuft auf Push/PR und kann über `Run workflow` für einen
gewählten Branch gestartet werden.
Erst ein grüner Lauf mit sichtbarem Artefakt bestätigt Build und Packaging.

Artefakt `foliq-windows-x64-test` herunterladen und vollständig entpacken.
Das Paket enthält den portablen `dist`-Inhalt, exakte Projektquellen desselben
Commits und Prüfsummen. `bin/xournalpp.exe` unterhalb des entpackten Paketbaums
starten; dessen Fenster identifiziert sich als Foliq-Entwicklung auf Xournal++.
Je nach GitHub-Entpackung liegt bin direkt an der Wurzel oder unter dist.
DLLs und share-Verzeichnis müssen neben bin erhalten bleiben. Kein Installer
oder Release-Signatur wird als Foliq-Release angeboten. Der Upstream-Installer
wird intern erzeugt/geprüft, ist aber nicht Teil des Testdownloads.

Foliq verwendet eigene Profilordner und eine eigene GTK-App-ID. Für den
Entwicklungstest eine separate Bibliothek oder VM verwenden; der Binaryname und
die XOPP-Dateiendung bleiben unverändert.
Testversion ist unsigniert; keine Systemschutzfunktionen abschalten.

## Kurzer Testlauf (Windows 10/11 x64)

1. Start ohne Internet/Konto; Foliq-Titel, Werkzeugleisten und Seitenansicht prüfen.
2. Text mit Umlauten, Handschrift mit Druckstufen, Radierer, Auswahl und Bild auf
   derselben Seite erstellen. Undo/Redo mehrfach prüfen.
3. XOPP speichern, schließen, neu öffnen; Text, Striche und Bild vergleichen.
   Kopie in unverändertem Xournal++ 1.2.8 öffnen und Kompatibilität prüfen.
4. Mehrseitiges PDF annotieren, als XOPP speichern und als PDF exportieren;
   zweite Seite, Zoom und Druckvorschau prüfen.
5. Audio aufnehmen/abspielen mit Berechtigung, wechselndem Mikrofon und Kopfhörer.
6. Bei 100/150/200% DPI sowie Stift/Touch: Latenz, Handballenunterdrückung,
   Radierertaste, Fensterwechsel und Scrollen prüfen. Während Texteingabe und
   Dateinamen-Eingabe mehrfach per Alt+Shift das Tastaturlayout wechseln
   (bekanntes Upstream-Risiko #6315).
7. Offline-Neustart, Autosave/Wiederherstellung mit Testdaten prüfen.

Ergebnis mit Windows-Version, Hardware, Stift/Grafiktreiber, Paket-Commit,
Reproduktionsschritten und erwartetem/tatsächlichem Verhalten protokollieren.
Notizbücher sind implementiert (Testablauf unten). Rich Text, OCR,
Verschlüsselung und Auto-Grow werden in diesem Paket noch nicht erwartet. Neue Features erst nach ROADMAP-Abnahme freigeben.

## Lokal bauen

MSYS2 UCRT64 und dieselben Pakete wie in
`.github/actions/install_deps_windows/action.yml` installieren (einschließlich
des in BUILD.md beschriebenen deaktivierten Legacy-GTK-Downgrades). Im UCRT64-Terminal:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DFOLIQ_BUILD=ON -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=ON
cmake --build build --target pot translations
cmake --build build
cmake --build build --target test-units
CI=true build/test/test-units.exe
(cd build && bash ../windows-setup/package.sh)
```

Packaging benötigt NSIS am im Upstream-Skript angegebenen Windows-Pfad.
Heruntergeladene Build-Abhängigkeiten sind vom Offline-Betrieb der App zu trennen.

## Lokale Notizbuchverwaltung testen

Portable Paket entpacken und `bin/xournalpp.exe` starten. Es gibt keine
vorinstallierten Beispielnotizen. Optional in PowerShell vor dem Start:

```powershell
$env:FOLIQ_LIBRARY = "$env:USERPROFILE\Foliq-Testbibliothek"
.\bin\xournalpp.exe
```

1. Notizbuch, zwei verschachtelte Gruppen, Abschnitt, Seite und Unterseite
   anlegen. Namen wie `CON: Test?`, Unicode und abschließenden Punkt versuchen.
2. Seite mit Doppelklick öffnen, zeichnen, fünf Sekunden warten. App beenden,
   neu starten: Dokumentinhalt und letzte Position müssen wieder erscheinen.
3. Abschnitt verschieben, Einträge mit Pfeilen sortieren, Seite umbenennen.
   UUID-Dateiname und bestehende Unterseitenverweise müssen gleich bleiben.
4. Teilbaum duplizieren (auch mit Bildhintergrund), Inhalt prüfen. Original
   löschen: nur im Papierkorb sichtbar; wiederherstellen und öffnen.
5. Aktives Notizbuch schließen, neu starten: es bleibt geschlossen. Öffnen und
   Seite erneut aufrufen. Eine zweite App-Instanz muss einen sichtbaren
   Katalog-Sperrfehler anzeigen; die erste darf weiter speichern.
6. App beenden, eine Kopie der Bibliothek anlegen. Für einen echten Fehlerfall
   einen Ordner `documents/<Seiten-ID>.xopp.bak` erzeugen (vorherige Sicherung
   dafür außerhalb aufbewahren). Seite bearbeiten: Autosave muss einen Fehler
   melden, Original bleibt unverändert, Editor bleibt geändert. Ordner entfernen
   und explizit speichern: Inhalt wird erfolgreich übernommen. Anschließend
   denselben Test mit einem Ordner `<Seiten-ID>.xopp.saving` wiederholen; schon
   das Öffnen des Kompressionsziels muss als Fehler behandelt werden.
7. Separat Datei mit exklusivem Windows-Handle öffnen und Speichern versuchen.
   Der Windows-GTest `WindowsLockedTargetKeepsOriginal` prüft diesen Fall ohne UI.
8. Mit verwalteter Seite im Editor die XOPP-Datei extern durch eine andere gültige
   Revision ersetzen: Speichern muss einen Konflikt melden. Änderungen über
   „Speichern unter“ retten, Original nicht überschreiben.

Automatische Tests sind kein Windows-Stift-/DPI-/Screenreader-/Hardwaretest.

## Neue Hauptoberfläche prüfen

Diese Matrix gilt für den neuen Quellstand, nicht für das ältere verlinkte Paket.
Nach einem neuen Windows-Build Paket-Commit und Windows-/GTK-Version protokollieren.

1. Zwei Abschnitte mit Seiten/Unterseiten erstellen. Abschnitt links wählen;
   daneben nur dessen Seiten. Links/Rechts klappt Unterseiten ein/aus;
   Eingabe öffnet sie. F2, Tab, Shift+Tab und sichtbaren Fokus prüfen.
2. Seiten und Abschnitte an Zeilenkanten sortieren, in Zeilenmitte in andere
   gültige Eltern verschieben. Unterseiten bleiben erhalten. Zyklus und falscher
   Knotentyp müssen abgewiesen werden. Dieselben Ziele über Aktionen → Verschieben
   und Reihenfolge über Alt+↑/↓ prüfen; Neustart muss die Änderung behalten.
3. Beide Spaltengrenzen ändern. Fokus ein-/ausschalten; Hell, Dunkel, System
   wählen. Werkzeugreiter und eingeklappte Unterseiten ändern, regulär beenden
   und neu starten: Ansichtsoptionen wiederhergestellt. Unbekannte view.ini-
   Version nur in einer Testkopie prüfen: sichtbarer Fehler, kein Überschreiben.
4. Fenster bei 800×600 und 1280×800 sowie 100/150/200 % Windows-Skalierung
   prüfen. Sehr lange Unicode-Titel dürfen die Schreibfläche nicht verdrängen.
   Werkzeuggruppen müssen horizontal erreichbar bleiben. Fokusmodus bei kleinem
   Fenster prüfen. Beide Trennleisten auch mit Tastatur bedienen.
5. Alle vier Werkzeugreiter bedienen; „Zeichnen → Weitere Werkzeuge“ und
   „Ansicht → Alle Werkzeuge“ müssen die bisherigen Werkzeugleisten mit Farben,
   Strichstärken und Spezialwerkzeugen ein-/ausblenden; Dokument-Suche, Undo/Redo und tatsächlichen
   Speicherstatus prüfen. Stift/Radierer/Druck, PDF-Anmerkungen, Spezialwerkzeuge,
   Export und Autosave erneut mit der bestehenden Matrix oben prüfen.
6. NVDA: Namen der Bäume, Farbschema, Aktionsmenüs, Auswahl und Fokus prüfen.
   Linux-ATK-Inspektion ersetzt diese Windows-Screenreader-Abnahme nicht.

## Seitentypen: konkrete Stift- und Touchabnahme (noch auszuführen)

Diese Matrix ist eine Anleitung, kein Hardwareprüfbericht. In dieser Änderung
wurden keine Windows-Geräte, echten Druckwerte oder physische Eingabelatenzen
geprüft. Pro Durchlauf Windows-Build, Gerät/Stift, Treiber/Firmware, GTK-Version,
Bildschirm-Hz, DPI (100/150/200 %), Touch-Einstellungen, Energieprofil und
Git-Commit protokollieren. Dieselbe synthetische Notiz im Papier- und freien
Modus verwenden; freie Tests in allen vier Quadranten und nach Neustart.

| Prüfung | Konkreter Ablauf | Erwartung / Beleg |
|---|---|---|
| Druck | Je 20 langsame diagonale Linien mit leicht → kräftig → leichtem Druck bei 25/100/400 % Zoom; Kreise und schnelle Schleifen ergänzen. Im freien Modus nach links/oben über den Ursprung schwenken und wiederholen. | Kontinuierliche Strichbreiten ohne Nullstellen/Spitzen; Drucksegmentwerte im gespeicherten XOPP vor/nach Neustart vergleichen. Der letzte Punkt besitzt upstreamgemäß keinen eigenen Segmentdruck. Bildschirmaufnahme und Gerät angeben. |
| Textmarker | Mehrfach über Text/Handschrift zeichnen, gefülltes Rechteck/Polygon ergänzen; bei negativem Ursprung zoomen und radieren. | Transparenz/Füllung unverändert; keine schwarzen Kacheln, kein Flackern durch flächenproportionale Masken. PDF-Ausschnitt vergleichen. |
| Handballen | Stift zunächst schwebend, dann schreibend; Handballen auflegen, verschieben, abheben, erneut auflegen. 20 Wiederholungen je Zuordnung, links-/rechtshändige Lage. Dazu Stift aus Erkennungsbereich entfernen und Touch-Geste bewusst ausführen. | Während Schreiben keine zusätzlichen Striche, Lasso-Aktionen oder Kamerasprünge durch die Hand. Danach Touch wieder nutzbar. Unterschied zwischen gerätegemeldetem Proximity und verfügbaren Softwarefiltern festhalten. Ohne passende Gerätedaten keine universelle Palm-Rejection behaupten. |
| Seitentasten | In Einstellungen beide Stifttasten getrennt z. B. auf Radierer und Lasso legen. Je 20-mal vor Kontakt, während Kontakt und schwebend drücken/loslassen; falls vorhanden Stift umdrehen. Danach neu starten. Windows-Option für Tablet-PC-Button-Ereignisse mitprotokollieren. | Gewähltes Werkzeug wirkt nur entsprechend konfigurierter Zuordnung; Loslassen stellt Werkzeug zurück, kein hängender Modus und kein Zusatzpunkt. Einstellungen über Neustart erhalten. |
| Maus/Touch-Zuordnung | Mausmitte/-rechts je Hand/Lasso, Touch auf Schwenken bzw. Zeichnen sperren; Stift schreibt. Während schreibendem Stift einen zweiten Touchkontakt setzen, danach einzeln und per Pinch navigieren. | Bestehende Gerätekonfiguration bleibt wirksam; kein Stift-/Maus-Doppelstrich. Schwenken/Zoom bewegen den Ausschnitt, nicht die gespeicherten Striche. |
| Auswahl/Formen | In jedem Quadranten Strich und Rechteck/Ellipse/Pfeil erzeugen; Lasso, Verschieben, Skalieren, Spiegeln und Ebenenwechsel verwenden. Bei 400 % vergrößert und bei 25 % wiederholen. | Griffe treffen die sichtbaren Elemente; Druck/Geometrie bleiben nach Speichern/Laden. Undo/Redo für jeden Bearbeitungsschritt, Schwenk, Zoom und Seiteneinstellung durchführen. |
| Umwandlung | Freie Notiz mit Inhalt in allen Quadranten und ausgeblendeter Ebene zu A4 quer kopieren. Typwechsel rückgängig/wiederholen. Papierkopie zurück zu Frei kopieren. | Originalseite unverändert vorhanden. Papierkopie nur übersetzt, ohne Skalierung; Überlauf bleibt gespeichert, ist aber auf Papier abgeschnitten. Dialoggrenzen vor Bestätigung lesbar. |
| PDF | Freien Ausschnitt mit negativem X/Y setzen und 2×3 Teilseiten exportieren; anschließend ungeteilten Ausschnitt und gemischtes Papier/Frei-Dokument exportieren. Datei in unabhängigem PDF-Leser öffnen. | Seitenzahl, Kantenanschlüsse, Inhalt und Druckbreiten vergleichen; Randseiten können verkürzt sein. Übermäßig großen Export anfordern: sichtbarer Fehler und vorherige PDF unverändert. |
| Latenz | Mit mindestens 240-fps-Kamera gleichzeitig Stiftspitze und Anzeige filmen. Je 30 kurze Starts und Richtungswechsel pro Modus bei 100/400 %; dann dieselben Abläufe bei weit entferntem negativem/positivem Ursprung. Leere Fläche und Notiz mit 10.000 synthetischen Strichen getrennt messen. | Zeit von sichtbarem Kontakt/Bewegungsbeginn zum ersten passenden Pixel in Frames zählen; Median, p95 und Messunsicherheit (bei 240 fps mindestens ±4,17 ms pro Framegrenze) berichten. Gegen denselben Papier-/Vorversionslauf vergleichen; zusätzliche Verzögerung >1 Bildschirmframe oder Aussetzer untersuchen. Keine aus Maus-/Softwaretests abgeleitete Millisekundenbehauptung. |

Zusätzlich während 100 wechselseitiger Schwenks zwischen weit entfernten leeren
Ausschnitten privaten Arbeitssatz und CPU beobachten: der Aufwand darf nicht mit
der besuchten leeren Weltfläche anwachsen. Erst normale Cache-/Initialisierung
abwarten, dann Anfang/Maximum/Ende messen. Bei großen Auswahlbereichen kann die
auf 2.048 Pixel begrenzte temporäre Vorschau weich erscheinen; die Vektoren und
der finale PDF-Export müssen scharf bleiben. Direkten Druck freier Dokumente
über den angezeigten PDF-Weg durchführen. Audio ist hiervon nicht abgenommen.

## Text-/Medienregression nach M3

Zusätzlich [TEXT-AND-MEDIA.md](TEXT-AND-MEDIA.md) und die Linux-Belege unter
[TEXT-MEDIA-VALIDATION.md](validation/TEXT-MEDIA-VALIDATION.md) beachten. Auf
Windows noch auszuführen: deutsche Umlaut-/ß-/AltGr-/Tottasteneingabe, native
IME-Komposition/Kandidatenfenster, Text-/Elementzwischenablage, Klartext-Fallback
aus Office/Browser, Tabellen nach Neustart weiterbearbeiten, Text- und Dokument-
Undo, proportionaler Bildzuschnitt, Offline-TeX einschließlich Fehler und
Formel-Undo sowie gemischte PDF-Ausgabe. Mit Stift prüfen, dass Textdialog/
Tabellenzellen den Canvas nicht zeichnen lassen und nach dem Schließen dieselbe
Seitenposition/Zoom-Auswahl gilt. DPI/NVDA nicht aus den Linux-Screenshots ableiten.
TeX/Pakete lokal vorinstallieren; gespeicherte Formeln müssen auch ohne TeX
angezeigt und exportiert werden. Kein neuer Windows-Testnachweis in diesem Schritt.
