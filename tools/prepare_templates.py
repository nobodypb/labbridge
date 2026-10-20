#!/usr/bin/env python3
"""Derive one template overlay from the checksum-pinned upstream generator.

Our object schemas are closed. Python generator 7.12.0 otherwise silently drops
unknown Pydantic fields, which can hide misspelled instrument settings.
"""
from pathlib import Path
from zipfile import ZipFile

root = Path(__file__).resolve().parents[1]
with ZipFile(root / ".tools/openapi-generator-cli-7.12.0.jar") as archive:
    template = archive.read("python/model_generic.mustache").decode()
needle = "    model_config = ConfigDict(\n"
assert template.count(needle) == 1
replacement = (
    needle
    + """        {{^additionalPropertiesType}}
        extra='forbid',
        {{/additionalPropertiesType}}
"""
)
path = root / ".tools/templates/python/model_generic.mustache"
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(template.replace(needle, replacement))
