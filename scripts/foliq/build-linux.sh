#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
# Usage: build-linux.sh [build-directory] [ON|OFF (Foliq identity)]
build=${1:-"$root/build-foliq"}
identity=${2:-ON}
cmake -S "$root" -B "$build" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFOLIQ_BUILD="$identity" \
  -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=OFF
cmake --build "$build" --target pot translations --parallel 2
cmake --build "$build" --parallel 2
cmake --build "$build" --target test-units --parallel 2
CI=true "$build/test/test-units" --gtest_output="xml:$build/test-results.xml"
