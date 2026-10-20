#!/usr/bin/env bash
# Run the frozen controller tests against the generated client's real cJSON.
set -euo pipefail
cd "$(dirname "$0")/../reference"
mkdir -p .build
g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/core firmware/core/protocol.cpp firmware/tests/protocol_test.cpp -o .build/protocol-test
.build/protocol-test
g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/core firmware/core/captive_dns.cpp firmware/tests/captive_dns_test.cpp -o .build/captive-dns-test
.build/captive-dns-test
PYTHONPATH=python/src python3 -m unittest discover -s python/tests -v
cjson=../tests/vendor/cjson
gcc -c "$cjson/cJSON.c" -o .build/cjson.o
for name in controller spp_controller; do
  flags=()
  [[ "$name" == spp_controller ]] && flags+=(-DCONFIG_IDF_TARGET_ESP32=1)
  g++ -std=c++17 -Wall -Wextra -Werror "${flags[@]}" -Ifirmware/tests/stubs -Ifirmware/core -I"$cjson" firmware/core/protocol.cpp "firmware/tests/${name}_test.cpp" .build/cjson.o -o ".build/$name"
  ".build/$name"
done
