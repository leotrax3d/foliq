# Git-Identität für Foliq

Verbindliche Vorgabe des Projektinhabers: Für neue Foliq-Commits gilt immer
**leotrax3d@gmx.net**, sowohl als Author- als auch als Committer-E-Mail.
Keine andere Adresse oder GitHub-noreply-Adresse verwenden.

```sh
git config user.name leotrax3d
git config user.email leotrax3d@gmx.net
git show -s --format='%ae%n%ce' HEAD
```

Vor dem Push müssen beide ausgegebenen Zeilen exakt `leotrax3d@gmx.net` sein.
Die Regel gilt auch für Merge-Commits, API-Änderungen und Automatisierungen.
GitHub-Weboberfläche oder APIs nur zum Committen verwenden, wenn beide Felder
korrekt gesetzt werden können; ansonsten lokal committen und pushen.
Die historischen Autorenangaben der unverändert übernommenen Xournal++-Commits
bleiben aus Gründen der Herkunftsnachvollziehbarkeit erhalten.
