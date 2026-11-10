# Prüfbelege

Datum: 2026-11-10. Headless Linux-Referenz; kein Windows- oder Hardwarebeleg.

- Basisquellen: git archive v1.2.8, Commit
  ce753d2e0c48a7026fe1fbe59f132586b2217296.
- SHA-256 des unkomprimierten Git-Tararchivs:
  d3d697aadb5f1128e7911c1120e15546fb00934a0d3759fd14c4f2bd92a5e8e8.
- Containerbasis siehe BUILD.md. Ubuntu 24.04 x86_64, GCC 13.3, CMake 3.28.3,
  Ninja 1.11.1, GTK 3.24.41. Paket-/Toolchainmanifeste liegen daneben.
- Statische Prüfung: git diff --check, bash -n des Buildhelpers und aller
  Windows-Workflow-run-Blöcke, YAML-Parsing mit PyYAML 6.0.3 sowie Auflösung
  lokaler Composite-Actions und relativer Dokumentationslinks erfolgreich.

YAML-Parsing überprüft keine Windows-DLLs und ist kein Actions-Lauf. Die lokale
Umgebung hat weder ein Windows-System noch eine Stift-/Mikrofon-Abnahme geliefert.

## Resultate

| Lauf | Build | Unit-Tests | CLI --version |
| --- | --- | --- | --- |
| Unveränderte v1.2.8-Basis | bestanden | 92/92, 20 Suites, 0 deaktiviert | bestanden |
| Foliq ON, Buildhelper | bestanden | 92/92, 20 Suites, 0 deaktiviert | bestanden |
| Foliq OFF | nur Konfiguration bestanden | nicht erneut ausgeführt | nicht gebaut |

Test-XML: upstream-tests.xml und foliq-tests.xml. FOLIQ_WINDOW_NAME nach ON
und OFF jeweils am generierten Header geprüft. HTTP-HEAD des alten Windows-
GTK-Pakets: 404; daraufhin optionalen Downgrade für Foliq deaktiviert.
Windows-CI, Windows-Paket und GUI-/Hardwaretests wurden nicht ausgeführt.
Der vollständige Lock besteht eine apt-get --simulate install-Prüfung mit
0 Änderungen; Dockerfile-Neuaufbau nicht ausgeführt.

## Ausgeführte Build-Kommandos im Container

```sh
cmake -S /tmp/xournalpp-upstream -B /tmp/build-reference -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=OFF
cmake --build /tmp/build-reference --target pot translations --parallel 2
cmake --build /tmp/build-reference --parallel 2
cmake --build /tmp/build-reference --target test-units --parallel 2
CI=true /tmp/build-reference/test/test-units --gtest_output=xml:/tmp/reference-tests.xml
/tmp/build-reference/xournalpp --version
```

Danach CMakeLists.txt, src/config.h.in und src/core/control/Control.cpp aus dem
Foliq-Arbeitsbaum in den temporären Quellbaum kopiert, ebenso den Buildhelper:

```sh
bash /tmp/xournalpp-upstream/scripts/foliq/build-linux.sh /tmp/build-reference ON
/tmp/build-reference/xournalpp --version
cmake -S /tmp/xournalpp-upstream -B /tmp/check-foliq-off -G Ninja \
  -DFOLIQ_BUILD=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

SHA-256 der lokal erzeugten ELF-Binaries (keine Reproduzierbarkeitsgarantie
über andere Pfade/Zeitstempel/Toolchains):

- Upstream: f68451cccdd1710bc2759dcdbe240bef1445acbb540b5c2f29d0aa9d0e2c6663
- Foliq: 1aff711577d8af53d7c5cd492c7b4690c2838f4ea66f351d0a1f1e8db13cb5e0
