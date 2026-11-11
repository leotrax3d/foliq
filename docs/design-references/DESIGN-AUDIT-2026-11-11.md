# Design-Audit (Windows-11-Ästhetik, Konsistenz, Navigation) – 2026-11-11

Plattform: Linux/Xvfb-Screenshots (`docs/validation/fluent/after-*`, `onboarding-*`).
Kein Windows-Rendering geprüft.

| # | Befund | Änderung |
|---|---|---|
| 1 | Fokusmodus, Vollbild und „Vollbild beenden" nutzten fast dasselbe Symbol | Lucide maximize / minimize / eye-off getrennt |
| 2 | Akzentfarbe mit Sonne/Mond-Symbol, Einrichtung teilte Symbol mit Seitentyp | palette-Symbol; Einrichtung in „Darstellung" |
| 3 | Ansicht-Register überladen (Zoom, Fokus, Design, Dichte, Akzent, Bewegung, Leisten, Einrichtung) | Ansicht = Zoom + Fokus; alles Weitere in einem Popover „Darstellung" |
| 4 | Lasso doppelt (Start und Zeichnen) | nur noch unter Zeichnen |
| 5 | Unterstrichene Registerkarten passten nicht zu Win 11 (Nutzerhinweis) | aktive Karte als gefüllte, abgerundete Fläche |
| 6 | „Speichern" + Text „Gespeichert" (abgeschnitten bei schmalem Fenster) | ein Icon: Häkchen = gespeichert, Diskette = Änderungen (Klick speichert), Warnung = Fehler; Text nur bei Fehler |
| 7 | Vollflächig gefärbte Navigationszeilen wirkten schwer, nicht Win 11 (Nutzerhinweis) | neutrale Liste, abgerundete Farbmarke links; Auswahl neutral + fett + Marke |
| 8 | Ausgewählte farbige Zeile verlor ihre Farbe (GTK zeichnet bei Auswahl keine Zellfarben) | Marke als Bild, behält die Farbe |
| 9 | Leere Seitenliste ohne Erklärung | Leerzustände für Notizbücher und Seiten |

Offen: Seiten erben die Abschnittsfarbe noch nicht; Popover-/Dialog-
Erscheinung unter Windows (Schatten, Ecken) ungeprüft; Upstream-Dialoge
(z. B. Einstellungen) haben noch englische Texte ohne installierte Übersetzung.
