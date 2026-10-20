# Development

Use Python 3.13, CMake 3.30.2, Java 21, Node 20 and ESP-IDF 5.5.2.

```sh
uv venv --python /usr/bin/python3.13 .venv
uv pip install --python .venv/bin/python --require-hashes -r tools/requirements.lock
uv pip install --python .venv/bin/python --no-build-isolation --no-deps -e host/generated -e host
export PATH="$PWD/.venv/bin:$PATH"
tools/generate.sh
tools/check.sh
```

OpenAPI Generator 7.12.0 is downloaded from Maven Central with a pinned SHA256.
Generated Python uses Pydantic, TypeScript uses fetch, C models use cJSON.
The SDK explicitly imports the generator's transport configuration class to
avoid confusing it with an API configuration model.

Start a story with a failing acceptance test, implement the smallest behavior,
run checks, then refactor. Keep physical hardware evidence separate from portable
tests. Follow `AGENTS.md`; reviewable story PRs link the canonical backlog ID.
Generated source and vendor support code are reviewed as generated changes.
No credentials, binary dumps, APKs or manufacturer firmware belong in Git.

## Reproducible checks

Use `tools/requirements.lock` with `--require-hashes` in place of the un-hashed
input list above. Install editable packages with `--no-build-isolation --no-deps`
after the lock. CI uses these commands. `tools/requirements.txt` is the direct
input; regenerate the lock deliberately when upgrading a dependency.

```sh
. .venv/bin/activate
tools/generate.sh
git diff --exit-code -- api/openapi.json firmware/generated host/generated web/generated
tools/check.sh
black --check tools host/src host/tests tests/test_*.py
cd web
npm ci --ignore-scripts
npm test
npm run build
npx playwright install --with-deps chromium
npm run test:browser
```

The browser fixture serves the real bundled UI and intercepts API calls with
sanitized scenarios. Playwright manages the fixture server through its
[webServer configuration](https://playwright.dev/docs/test-webserver).

## Board and resource configuration

`tools/build_firmware.sh sticks3|usb-otg|esp32` uses separate build directories
and sdkconfig files. StickS3 preserves native USB for management; USB-OTG enables
host serial adapters and uses a UART console. Classic ESP32 currently uses BLE
and UART; SPP is not enabled in the common runtime. Build-time board capabilities
bound the runtime device list. Enabling GPIO/1-Wire/UART pins requires an explicit
approved pin list in Kconfig; validate against the actual board wiring first.
Runtime configuration rejects duplicate identities and conflicting resources
before saving. Both intentional OFF work and unconfirmed shutdown/restoration
block unsafe reconfiguration.

The IDF task delay wrapper rounds positive millisecond waits up to at least one
RTOS tick. A short high-priority worker delay must never become a zero-tick busy
loop on a 100 Hz scheduler.

The generator template overlay is derived from the pinned upstream JAR and
configures closed Pydantic models to reject unknown request fields. Firmware
validation constraints are generated from request-reachable schemas. Do not
hand-edit any generated route/model. `tests/vendor/cjson` is a separately licensed
MIT test dependency for the preserved controller; see its license.

Document operations, parameters, responses, schemas and inline fields in the API
fragments, including units, prerequisites and known limitations. OpenAPI 3.0
Reference Objects cannot carry description siblings; document shared object
types in their referenced schema instead. Keep examples free of credentials.
The assembler retains documentation in the public contract/clients and strips
prose/examples from the ESP validation schemas, preserving constraints and
property names. `tests/test_api_annotations.py` covers that boundary.


Component contracts live in `api/modules/components.json`; meter configuration
schemas and manifests live with their individual module fragments. A driver can
inherit the standard component adapter or override discovery, readings, actor
state/command and component-stop behavior. Legacy routes are optional; a new
source, sink or sensor can serve the standard routes without adding core paths.
Root and ESP builds include portable meter codecs; ESP worker tasks own bounded
I²C/HTTP calls. Do not publish manufactured sensor data as hardware evidence.
The optional native Yocto package is version-pinned in the development lock and
host extra; fake SDK/transport fixtures keep CI independent of attached USB.
