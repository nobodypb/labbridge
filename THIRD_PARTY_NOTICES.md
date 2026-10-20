# Third-party notices

Generated clients/models: OpenAPI Generator 7.12.0, templates under Apache-2.0.
The Python/TypeScript package READMEs identify generated output.

`firmware/generated/c/external/cJSON.c` and `.h` are cJSON (MIT), with the
upstream notice in `firmware/generated/c/external/cJSON.licence`.

The `reference/` implementation is the project's preserved first-party baseline.
Manufacturer firmware, PC applications and mobile applications are not included.
ESP-IDF and managed USB/1-Wire dependencies are fetched separately under their
own licenses; see their package metadata when building.

`tests/vendor/cjson` contains cJSON 1.7.19 (MIT) for preserved controller tests;
its upstream copyright and permission notice are in that directory's `LICENSE`.


The optional `yoctopuce` Python SDK is fetched as a dependency and retains the
manufacturer's license for interfacing with Yoctopuce products. Its sources and
native USB binaries are not vendored here. The LabBridge adapter is first-party.
