# Foliq – verbindliche Arbeitsregeln

Foliq ist ein vorläufiger Arbeitsname für eine freie, lokale Windows-Notiz-App auf
Xournal++-Basis. Vor Änderungen docs/PRODUCT.md, ARCHITECTURE.md, ROADMAP.md,
UPSTREAM.md und den aktuellen HANDOFF.md lesen.

- Vorhandene Arbeit und Xournal++-Funktionen erhalten. Keine Platzhalter als
  fertige Funktionen ausgeben; sichtbare Bedienelemente müssen funktionieren.
- GTK3/C++ und den gemeinsamen Editor schrittweise erweitern. Ein Frameworkwechsel
  oder zweiter Editor verlangt einen belegten technischen Grund und ein ADR.
- Keine Pflichtkonten, Server oder Telemetrie. Nutzerdaten bleiben lokal.
- Jedes neue Datenformat erhält eine explizite Version, Validierung, atomare
  Schreibvorgänge, Sicherungen und getestete Migrationen. Unbekannte neuere
  Versionen nicht stillschweigend überschreiben. XOPP-Kompatibilität bewahren.
- Neue Bibliotheken vor Aufnahme auf konkrete FOSS-Lizenz, transitive Lizenzen,
  Windows-Build, Offline-Betrieb und Verteilung prüfen und dokumentieren.
- LICENSE, AUTHORS, copyright.txt und dateispezifische Hinweise bewahren.
  Herkunft und Änderungen dokumentieren; Binärpakete mit Hinweisen und passendem
  Quellcode verteilen. Keine pauschale Relizenzierung der Upstream-Dateien.
- Bestehende Tests passend zur Änderung ausführen. Tatsächlich ausgeführte Tests,
  statische Prüfung und offene Windows-/Stift-/Audio-/Hardwaretests klar trennen.
- Nach jedem abgeschlossenen Schritt docs/HANDOFF.md mit Änderungen, Kommandos,
  Ergebnissen, Problemen und nächstem überprüfbaren Schritt aktualisieren.
- Keine Secrets oder private Notizen in Logs, Fixtures oder CI-Artefakte aufnehmen.
- CI-Paket ist ein Entwicklungspaket, kein fertiggestelltes Foliq-Produkt.

## Verbindliche Git-Identität

Für alle neu erstellten Foliq-Commits und GitHub-Änderungen ausschließlich
`leotrax3d@gmx.net` als Author- und Committer-E-Mail verwenden. Keine andere
Adresse und keine noreply-Adresse einsetzen. Vor Commit und Push beide Felder
prüfen; explizit konfigurieren statt Umgebungsstandards zu übernehmen.
Auch API-, Merge- und Automatisierungswege müssen diese Identität gewährleisten;
ist das nicht möglich, den Commit lokal mit korrekter Identität erstellen.
Unveränderte historische Upstream-Autorenangaben erhalten.

## Codex-Umgebung und Einstieg für neue Chats

- Wiederverwendbares Setup und Startbefehle: docs/CODEX.md, .codex/README.md.
- Nach Snapshot-/Chatwechsel `git status -sb`, aktuellen Remote-Stand und
  `bash scripts/foliq/setup-codex.sh --check` prüfen. Cache oder frühere
  Testergebnisse sind kein Nachweis für den aktuell ausgecheckten Code.
- Für inkrementellen Build plus Tests `bash scripts/foliq/setup-codex.sh --build`.
  Kein laufender Chat-Server erforderlich; Arbeit über Git und HANDOFF übergeben.
