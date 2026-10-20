# DL24 Bench upstream snapshot

Source: https://crimson-grass-5279.binman-cloudflare.workers.dev/dl24
Retrieved: 2026-10-20. No upstream repository was identified.
SHA-256 (`upstream.html`): d2e0a9b876f9e0a042abc0d5110813f97dec874479662df32229a6c145392fd6

The upstream file is unchanged and identifies itself as GPL-3.0-or-later,
Copyright © 2026 DL24 reverse-engineering project contributors. `COPYING`
contains GPL version 3. LabBridge adaptations are in `web/bench` and
`web/src/bench`, with the same GPL-3.0-or-later designation. The build script
`web/build-bench.mjs` packages the preferred source, build inputs and MIT
API-client dependencies for download from the device at `/dl24-source.tar.gz`.
No HID transport from this snapshot is executed by the LabBridge page.
