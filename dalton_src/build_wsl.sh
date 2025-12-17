#!/bin/bash
# Workaround Script: Build Kernel Module in WSL with paths containing spaces
# The kernel build system fails if the path has spaces.
# This script copies the source to /tmp, builds it, and copies the result back.

# Config
SRC_DIR=$(pwd)
TEMP_DIR="/tmp/dalton_build_safe"

echo "[*] Path Safe Builder for DaltonFix"
echo "    Source: $SRC_DIR"
echo "    Build : $TEMP_DIR"

# 1. Prepare Temp Directory
echo "[*] Preparing workspace..."
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR"

# 2. Copy Source Files
cp "$SRC_DIR"/* "$TEMP_DIR/" 2>/dev/null

# 3. Build
echo "[*] Compiling..."
make -C "$TEMP_DIR"

# 4. Check Result
if [ -f "$TEMP_DIR/dalton_drv.ko" ]; then
    echo "[*] Build Success! Copying module back..."
    cp "$TEMP_DIR/dalton_drv.ko" "$SRC_DIR/dalton_drv.ko"
    echo "[!] File available at: $SRC_DIR/dalton_drv.ko"
    ls -lh "$SRC_DIR/dalton_drv.ko"
else
    echo "[!] Build Failed. Check errors above."
    exit 1
fi
