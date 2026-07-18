#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# build_mac.sh  —  Build locale Pitch Wrench su macOS
# Compilazione in locale. Mai GitHub Actions. Mai push dei binari.
# ─────────────────────────────────────────────────────────────────────────────

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "╔════════════════════════════════════════════╗"
echo "║   PITCH WRENCH  —  Build macOS             ║"
echo "║   Pulverine Audio                          ║"
echo "╚════════════════════════════════════════════╝"
# Stop on error
set -e

export PATH="/Users/vinz/Library/Python/3.9/bin:/usr/local/bin:/opt/homebrew/bin:$PATH"

# Go to script directory di build
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configura con CMake (Ninja per velocità, Xcode come fallback)
if command -v ninja &> /dev/null; then
    GENERATOR="Ninja"
else
    GENERATOR="Xcode"
fi

echo "→ Configurazione CMake (generator: $GENERATOR)..."
cmake .. \
    -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

echo ""
echo "→ Build..."
cmake --build . --config Release --parallel

echo ""
echo "✓ Build completata."
echo "  Plugin installato in ~/Library/Audio/Plug-Ins/VST3/"
