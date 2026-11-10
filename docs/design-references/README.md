# UI-Orientierung für Foliq

Vom Nutzer am 2026-11-10 bereitgestellte OneNote-Screenshots. Die Originalbilder
werden unverändert als visuelle Referenzen aufbewahrt. Sie sind keine
Foliq-Oberfläche und keine zur Übernahme bestimmten Programmassets; die
GPL-Lizenz des Foliq-Codes wird diesen Fremdabbildungen nicht zugeschrieben.

| Bild | Orientierung |
| --- | --- |
| [2735.png](2735.png) | Normalansicht und Textwerkzeuge; dunkler Rahmen, helle Schreibfläche |
| [2731.png](2731.png) | Fokusmodus mit kompakter schwebender Zeichenleiste |
| [2732.png](2732.png) | Zeichenwerkzeuge, Stiftfavoriten, Auswahl und Radierer |
| [2734.png](2734.png) | Gruppierte Einfügewerkzeuge für Inhalte und Medien |
| [2733.png](2733.png) | Ansichtsoptionen, Zoom, Papier und Hilfslinien |

Für die spätere GTK-Oberfläche übernehmen wir die räumliche Orientierung:
oben kontextbezogene Werkzeuggruppen für Text, Einfügen, Zeichnen und Ansicht;
darunter Notizbuchauswahl und farbige Abschnittsreiter; rechts Seiten und
Unterseiten; im Zentrum die große Papier- oder wachsende Arbeitsfläche.
Ein reduzierter Fokusmodus soll die Navigation ausblenden und die wichtigsten
Stiftwerkzeuge erreichbar halten. Text und Handschrift bleiben gleichrangig.

Foliq erhält eigene Icons, Farben und Gestaltung. OneNote-Logo, Kontofunktionen,
Freigabe und die Screenshot-Benachrichtigung aus 2735 sind keine Anforderungen.
Nicht implementierte Funktionen werden nicht als Attrappen eingebaut.
Die Referenzen ändern weder den GTK/C++-Stack noch das bestehende Dokumentmodell.
Dies ist eine Gestaltungsgrundlage, keine bereits implementierte Oberfläche.
