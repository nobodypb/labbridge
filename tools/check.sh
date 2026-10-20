#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/assemble_api.py --check
cmake -S . -B .build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/host -j2
ctest --test-dir .build/host --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py' -v
PYTHONPATH=host/src python3 -m unittest discover -s host/tests -v
if [[ -n "${IDF_PATH:-}" ]]; then
  IDF_PATH="$IDF_PATH" reference/scripts/test.sh
fi
