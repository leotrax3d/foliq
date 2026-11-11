# Foliq in Codex Cloud

Die versionierte Einrichtung liegt in [docs/CODEX.md](../docs/CODEX.md).
Setup-Befehl in den Codex-Umgebungseinstellungen:

```sh
bash scripts/foliq/setup-codex.sh
```

Nach dem Laden eines Snapshots Abhängigkeiten prüfen:

```sh
bash scripts/foliq/setup-codex.sh --check
```

Diese Datei ist ein Einstieg für Menschen und Agenten, keine automatisch
importierte Codex-Serverkonfiguration. Eine gespeicherte Cloud-Umgebung muss in
der Codex-Oberfläche einmal angelegt/veröffentlicht und für neue Chats ausgewählt
werden. AGENTS.md bleibt die verbindliche Arbeitsanweisung.
