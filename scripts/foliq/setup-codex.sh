#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Reusable Ubuntu/Debian Codex bootstrap. No accounts, secrets or service ports.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
mode=${1:---setup}
build=${FOLIQ_CODEX_BUILD_DIR:-"$root/build-codex"}
jobs=${FOLIQ_BUILD_JOBS:-2}
case "$mode" in
  --setup|--check|--configure|--build) ;;
  *) echo "Usage: $0 [--setup|--check|--configure|--build]" >&2; exit 2 ;;
esac
if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
  echo 'FOLIQ_BUILD_JOBS must be a positive integer.' >&2
  exit 2
fi
packages=(
  build-essential cmake ninja-build pkg-config git ca-certificates
  librsvg2-dev libgtk-3-dev libpoppler-glib-dev portaudio19-dev
  libsndfile1-dev liblua5.3-dev libzip-dev gettext help2man libgtest-dev
  libgtksourceview-4-dev python3 python3-gi python3-pyatspi
  xvfb xauth xdotool dbus-x11
)
if [[ "$mode" == --setup ]]; then
  if command -v cloud-agent-packages >/dev/null; then
    installer=$(command -v cloud-agent-packages)
    if (( EUID == 0 )); then
      "$installer" update
      "$installer" install "${packages[@]}"
    else
      sudo -n "$installer" update
      sudo -n "$installer" install "${packages[@]}"
    fi
  elif (( EUID == 0 )); then
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${packages[@]}"
  elif command -v sudo >/dev/null && sudo -n true 2>/dev/null; then
    sudo -n apt-get update
    sudo -n env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${packages[@]}"
  else
    echo 'Setup needs the approved package installer or root/passwordless sudo for APT.' >&2
    echo 'Install the packages listed in this script via environment settings, then use --build.' >&2
    exit 1
  fi
  # A root setup phase must not leave a root-owned cache in a non-root checkout.
  owner_uid=$(stat -c '%u' "$root")
  if (( EUID == 0 && owner_uid != 0 )); then
    owner=$(getent passwd "$owner_uid" | cut -d: -f1)
    if [[ -z "$owner" ]]; then
      echo "Repository owner UID $owner_uid has no user entry; run --build as its owner." >&2
      exit 1
    fi
    exec runuser -u "$owner" -- bash "$root/scripts/foliq/setup-codex.sh" --build
  fi
fi
for tool in cmake ninja pkg-config git g++ msgfmt xvfb-run xauth xdotool python3; do
  command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }
done
pkg-config --exists 'glib-2.0 >= 2.66' 'gtk+-3.0 >= 3.18.9' poppler-glib \
  gthread-2.0 libxml-2.0 libzip portaudiocpp sndfile librsvg-2.0 gtksourceview-4 lua5.3
# Use the distro Python for the APT-provided GTK/AT-SPI modules.
/usr/bin/python3 -c 'import gi, pyatspi; gi.require_version("Gtk", "3.0"); from gi.repository import Gtk'
if [[ "$mode" == --check ]]; then
  printf 'Foliq Codex dependencies ready; GTK %s, GLib %s\n' \
    "$(pkg-config --modversion gtk+-3.0)" "$(pkg-config --modversion glib-2.0)"
  exit 0
fi
git -C "$root" config --local user.name leotrax3d
git -C "$root" config --local user.email leotrax3d@gmx.net
cmake -S "$root" -B "$build" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFOLIQ_BUILD=ON \
  -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=OFF
if [[ "$mode" != --configure ]]; then
  # Named targets avoid regenerating tracked translation files during bootstrap.
  cmake --build "$build" --target xournalpp test-units --parallel "$jobs"
  CI=true "$build/test/test-units" --gtest_output="xml:$build/test-results.xml"
  "$build/xournalpp" --version
fi
printf 'Ready: %s\nBuild cache: %s\n' "$root" "$build"
