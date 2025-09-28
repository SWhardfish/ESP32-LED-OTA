name: Build ESP32-S3 (single merged factory image)

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-latest
    env:
      PIO_ENV: esp32-s3-zero   # <-- must match your [env:...] name in platformio.ini

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0  # show real HEAD

      # --- DIAGNOSTICS: See exactly what CI is building ---
      - name: Diagnostics: show repo state and main.cpp
        shell: bash
        run: |
          set -euo pipefail
          echo "===== HEAD ====="
          git rev-parse --abbrev-ref HEAD
          git --no-pager log -1 --oneline
          echo

          echo "===== env ====="
          echo "GITHUB_REF=$GITHUB_REF"
          echo "GITHUB_SHA=$GITHUB_SHA"
          echo "PIO_ENV=${PIO_ENV}"
          echo

          echo "===== file listing ====="
          ls -la
          echo

          echo "===== platformio.ini (first 200 lines) ====="
          sed -n '1,200p' platformio.ini || true
          echo

          echo "===== find any main.cpp in repo ====="
          find . -type f -name 'main.cpp' -print
          echo

          echo "===== src/main.cpp (first 120 lines, numbered) ====="
          nl -ba src/main.cpp | sed -n '1,120p' || true
          echo

          echo "===== grep: onStatusChange in src/main.cpp ====="
          grep -n "onStatusChange" src/main.cpp || true
          echo

          echo "===== grep: HTML entities in src ( &lt;  &gt;  &amp; ) ====="
          grep -R --line-number -E '&lt;|&gt;|&amp;' src || echo "No HTML entities found in src/"
          echo

          echo "===== file type and md5 of src/main.cpp ====="
          file src/main.cpp || true
          md5sum src/main.cpp || true
          echo

      # --- GUARD: fail immediately if the lambda is missing and it's a brace block ---
      - name: Guard check: ensure onStatusChange is a lambda
        shell: bash
        run: |
          set -euo pipefail
          if grep -nE 'onStatusChange\s*=\s*\{' src/main.cpp; then
            echo "::error ::Found 'onStatusChange = {' which is not a lambda. It must be 'onStatusChange =  {'"
            exit 1
          fi
          echo "Guard OK: no brace-block assignment found."

      # --- OPTIONAL HOTFIX: auto-rewrite the line to a lambda (uncomment to try) ---
      # - name: HOTFIX: rewrite brace block to lambda
      #   if: always()
      #   shell: bash
      #   run: |
      #     set -euo pipefail
      #     # Show before
      #     echo "Before rewrite:"
      #     nl -ba src/main.cpp | sed -n '1,80p'
      #     # Replace any 'onStatusChange =  {' with a correct lambda
      #     perl -0777 -i -pe 's/onStatusChange\s*=\s*\{([^\n]*)/onStatusChange =  {\1/' src/main.cpp
      #     echo "After rewrite:"
      #     nl -ba src/main.cpp | sed -n '1,80p'

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: "3.x"

      - name: Install PlatformIO + esptool
        run: |
          pip install --upgrade platformio esptool
          esptool version   # v5 syntax; don't use --version

      - name: Build (PlatformIO)
        run: pio run -e $PIO_ENV
      - name: Merge images -> one firmware file at 0x0
        shell: bash
        run: |
          set -euo pipefail
          BUILD_DIR=".pio/build/${PIO_ENV}"

          BOOTLOADER="${BUILD_DIR}/bootloader.bin"
          PARTITIONS="${BUILD_DIR}/partitions.bin"
          APPBIN="${BUILD_DIR}/firmware.bin"

          # boot_app0.bin may not be produced in BUILD_DIR by PIO; find a fallback in packages
          BOOTAPP0="${BUILD_DIR}/boot_app0.bin"
          if [[ ! -f "$BOOTAPP0" ]]; then
            BOOTAPP0="$(find ~/.platformio/packages -type f -name 'boot_app0.bin' | head -n 1 || true)"
          fi

          echo "Resolved files:"
          ls -l "$BOOTLOADER" "$PARTITIONS" "$APPBIN"
          if [[ -n "${BOOTAPP0:-}" && -f "$BOOTAPP0" ]]; then
            echo "Using boot_app0.bin: $BOOTAPP0"
          else
            echo "ERROR: boot_app0.bin not found"; exit 1
          fi

          # Create single merged image with standard ESP32-S3 offsets
          python -m esptool --chip esp32s3 merge-bin \
            --flash-mode qio --flash-freq 80m --flash-size 4MB \
            -o "${BUILD_DIR}/firmware-merged.bin" \
            0x0     "$BOOTLOADER" \
            0x8000  "$PARTITIONS" \
            0xE000  "$BOOTAPP0" \
            0x10000 "$APPBIN"

      - name: Upload build artifacts
        uses: actions/upload-artifact@v4
        with:
          name: esp32-s3-zero-images
          path: |
            .pio/build/${{ env.PIO_ENV }}/bootloader.bin
            .pio/build/${{ env.PIO_ENV }}/partitions.bin
            .pio/build/${{ env.PIO_ENV }}/boot_app0.bin
            .pio/build/${{ env.PIO_ENV }}/firmware.bin
            .pio/build/${{ env.PIO_ENV }}/firmware-merged.bin
