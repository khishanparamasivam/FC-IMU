#!/bin/sh
# Read-only STM32F405 main-flash backup. Never erase or disable protection.
set -eu
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DFU_TOOL="$PROJECT_DIR/.tools/dfu-util/extracted/usr/bin/dfu-util"
BACKUP_FILE="$PROJECT_DIR/backups/betaflight-original-flash.bin"
PARTIAL_FILE="$BACKUP_FILE.partial"

if [ -e "$BACKUP_FILE" ] || [ -e "$PARTIAL_FILE" ]; then
    echo "A backup or partial read already exists. Inspect it before retrying." >&2
    exit 1
fi
mkdir -p "$PROJECT_DIR/backups"

# dfu-util -U reads from the FC. No -D, erase, unprotect, or reset options.
"$DFU_TOOL" -d ,0483:df11 -a 0 -s 0x08000000:1048576 -U "$PARTIAL_FILE"
if [ "$(wc -c < "$PARTIAL_FILE")" -ne 1048576 ]; then
    echo "Incomplete read: retained as .partial; not a verified backup." >&2
    exit 1
fi
mv -- "$PARTIAL_FILE" "$BACKUP_FILE"
echo "Read completed: 1,048,576 bytes of main flash saved to $BACKUP_FILE"
sha256sum "$BACKUP_FILE"
