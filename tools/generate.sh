#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
version=7.12.0
jar=".tools/openapi-generator-cli-$version.jar"
expected=33e7dfa7a1f04d58405ee12ae19e2c6fc2a91497cf2e56fa68f1875a95cbf220
mkdir -p .tools
if [[ ! -f "$jar" ]]; then
  curl -fsSL "https://repo.maven.apache.org/maven2/org/openapitools/openapi-generator-cli/$version/openapi-generator-cli-$version.jar" -o "$jar"
fi
actual=$(sha256sum "$jar" | cut -d' ' -f1)
[[ "$actual" == "$expected" ]] || { echo 'Generator checksum mismatch' >&2; exit 1; }
python3 tools/prepare_templates.py
python3 tools/assemble_api.py
python3 - <<'CLEAN'
from pathlib import Path
import shutil
for name in ('host/generated','web/generated','firmware/generated/c'):
    p=Path(name)
    if p.exists():shutil.rmtree(p)
CLEAN
java -jar "$jar" validate -i api/openapi.json
java -jar "$jar" generate --git-user-id nobodypb --git-repo-id labbridge -i api/openapi.json -g python -t .tools/templates/python -o host/generated --additional-properties=packageName=labbridge_generated,projectName=labbridge-generated,packageVersion=0.1.0,hideGenerationTimestamp=true --global-property apiTests=false,modelTests=false,apiDocs=false,modelDocs=false
java -jar "$jar" generate --git-user-id nobodypb --git-repo-id labbridge -i api/openapi.json -g typescript-fetch -o web/generated --additional-properties=hideGenerationTimestamp=true,npmName=@labbridge/api,npmVersion=0.1.0,supportsES6=true --global-property apiTests=false,modelTests=false,apiDocs=false,modelDocs=false
java -jar "$jar" generate --git-user-id nobodypb --git-repo-id labbridge -i api/openapi.json -g c -o firmware/generated/c --additional-properties=hideGenerationTimestamp=true --global-property models,supportingFiles,modelDocs=false,modelTests=false
