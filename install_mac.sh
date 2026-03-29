#!/usr/bin/env bash

set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "install_mac.sh must be run on macOS." >&2
  exit 1
fi

build_dir="${BUILD_DIR:-build}"
config="${CONFIG:-Release}"
artefacts_dir="$build_dir/NinjamNext_artefacts/$config"
vst3_src="$artefacts_dir/VST3/NinjamNext.vst3"
au_src="$artefacts_dir/AU/NinjamNext.component"
vst3_dest_dir="${VST3_DIR:-$HOME/Library/Audio/Plug-Ins/VST3}"
au_dest_dir="${AU_DIR:-$HOME/Library/Audio/Plug-Ins/Components}"

install_bundle() {
  local src="$1"
  local dest_dir="$2"
  local label="$3"

  if [[ ! -e "$src" ]]; then
    echo "Skipping $label: $src was not found."
    return 1
  fi

  mkdir -p "$dest_dir"

  local dest="$dest_dir/$(basename "$src")"
  rm -rf "$dest"
  ditto "$src" "$dest"

  xattr -cr "$dest" 2>/dev/null || true

  if command -v codesign >/dev/null 2>&1; then
    if ! codesign --force --deep --sign - "$dest" >/dev/null 2>&1; then
      echo "Warning: ad-hoc codesign failed for $dest." >&2
    fi
  fi

  echo "Installed $label to $dest"
  return 0
}

installed_any=0

if install_bundle "$vst3_src" "$vst3_dest_dir" "VST3"; then
  installed_any=1
fi

if install_bundle "$au_src" "$au_dest_dir" "AU"; then
  installed_any=1
fi

if [[ "$installed_any" -eq 0 ]]; then
  echo "No built plugin bundles were found under $artefacts_dir." >&2
  echo "Run ./build_mac.sh first." >&2
  exit 1
fi

echo "Rescan plugins in your DAW after installation."
