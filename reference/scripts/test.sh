#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .build
g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/core firmware/core/protocol.cpp firmware/tests/protocol_test.cpp -o .build/protocol-test
.build/protocol-test
g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/core firmware/core/captive_dns.cpp firmware/tests/captive_dns_test.cpp -o .build/captive-dns-test
.build/captive-dns-test
PYTHONPATH=python/src python3 -m unittest discover -s python/tests -v
if [[ -n "${IDF_PATH:-}" ]]; then
  cjson_dir="$IDF_PATH/components/json/cJSON"
  gcc -c "$cjson_dir/cJSON.c" -o .build/cjson.o
  g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/tests/stubs -Ifirmware/core -I"$cjson_dir" \
    firmware/core/protocol.cpp firmware/tests/controller_test.cpp .build/cjson.o -o .build/controller-test
  .build/controller-test
  g++ -std=c++17 -Wall -Wextra -Werror -DCONFIG_IDF_TARGET_ESP32=1 \
    -Ifirmware/tests/stubs -Ifirmware/core -I"$cjson_dir" \
    firmware/core/protocol.cpp firmware/tests/spp_controller_test.cpp .build/cjson.o \
    -o .build/spp-controller-test
  .build/spp-controller-test
else
  echo 'Controller tests need IDF_PATH (for the real cJSON parser); not run.'
fi
