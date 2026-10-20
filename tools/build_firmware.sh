#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
board=${1:-sticks3}
case "$board" in
 sticks3) target=esp32s3; defaults='sdkconfig.defaults;sdkconfig.sticks3.defaults';;
 usb-otg) target=esp32s3; defaults='sdkconfig.defaults';;
 esp32) target=esp32; defaults='sdkconfig.esp32.defaults';;
 *) echo 'Usage: tools/build_firmware.sh sticks3|usb-otg|esp32' >&2;exit 2;;
esac
: "${IDF_PATH:?Set IDF_PATH to ESP-IDF v5.5.2}"
cd web
npm ci --ignore-scripts
npm run build
cd ../firmware/platform/esp_idf
idf.py -B "../../../.build/$board" -D "IDF_TARGET=$target" -D "SDKCONFIG=sdkconfig.$board" -D "SDKCONFIG_DEFAULTS=$defaults" build
