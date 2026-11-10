# Foliq Produktanforderungen

Freie Windows-Notiz-App, vollständig lokal, ohne Pflichtkonto, Server oder
Telemetrie. Handschrift und umfangreiche Textbearbeitung sind gleichrangig.
Foliq bleibt bis zur Namensprüfung ein Arbeitsname. Kein Import proprietärer
OneNote-Dateien erforderlich.

Ziel: aufgeräumte OneNote-artige Orientierung mit eigenen Gestaltungselementen;
Notizbücher → Abschnittsgruppen (verschachtelbar) → Abschnitte/Fächer → Seiten →
Unterseiten. Tastaturbedienung, zugängliche Beschriftungen, hohe DPI und
stiftfreundliche Ziele gehören zur Abnahme. Keine Kopie fremder Markenassets.

Pro Notiz wählbar: feste Papierseiten oder eine automatisch wachsende
Arbeitsfläche. Bilder, PDF-Anmerkungen, Tabellen, Listen, Formeln, Anhänge,
Audio, Tags, Aufgaben, globale Suche, OCR, Handschrifterkennung und Passwortschutz
sind Anforderungen. Daten müssen auch bei Absturz und Migration erhalten bleiben.
Passwortschutz muss authentifizierte Verschlüsselung bedeuten; kein bloßes
UI-Schloss. Suchindex und Audio dürfen den Schutz nicht umgehen.

## Stand dieser Basis

Vorhanden aus Upstream: Stift-/Maus-/Touch-Werkzeuge, Seiten und Ebenen, Bilder,
PDF-Anmerkungen, einfache Textobjekte, lokale Textsuche im Dokument,
Audiofunktionen und LaTeX-Workflow (externe TeX-Werkzeuge erforderlich).
Diese Funktionen wurden übernommen; ihre vollständige Hardwareabnahme steht aus.

Noch nicht implementiert: Notizbuchhierarchie, Rich Text mit gemischten Stilen,
strukturierte Tabellen/Listen, beliebige Anhänge, Tags/Aufgaben, globale Suche,
OCR, Handschrift-zu-Text, verschlüsselte Notizbücher, automatisch wachsende
Arbeitsfläche und die neue Oberfläche. `HandRecognition` im Upstream ist
Handballen-/Touch-Unterdrückung und keine Handschrifterkennung.

Erster Lieferumfang ist eine nachvollziehbare Entwicklungsbasis mit realem
Editor, Build-Verfahren, Windows-Testpaket-CI und Architekturentscheidungen.
Es gibt keine Attrappen für noch fehlende Funktionen.

## Visuelle Orientierung

Die fünf vom Nutzer bereitgestellten Screenshots und die daraus abgeleitete
Anordnung sind unter [design-references/](design-references/README.md) abgelegt.
Sie dienen der späteren GTK-Oberfläche als Orientierung für Werkzeuggruppen,
Abschnittsreiter, rechte Seitenliste und reduzierten Zeichenmodus.
