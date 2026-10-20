# Development

Use Python 3.13, CMake 3.30.2, Java 21, Node 20 and ESP-IDF 5.5.2.

```sh
uv venv --python /usr/bin/python3.13 .venv
uv pip install --python .venv/bin/python -r tools/requirements.txt
uv pip install --python .venv/bin/python --no-deps -e host/generated -e host
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
