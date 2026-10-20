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
    + "{{#vendorExtensions.x-labbridge-secret-model}}\n"
    + "        hide_input_in_errors=True,\n"
    + "{{/vendorExtensions.x-labbridge-secret-model}}\n"
    + """        {{^additionalPropertiesType}}
        extra='forbid',
        {{/additionalPropertiesType}}
"""
)
template = template.replace(
    "from typing_extensions import Self",
    "from typing_extensions import Self\n"
    "{{#vars}}{{#vendorExtensions.x-labbridge-secret}}\n"
    "from pydantic import SecretStr, Field\n"
    "{{/vendorExtensions.x-labbridge-secret}}{{/vars}}",
)
# Upstream loses SecretStr when a password has min/maxLength. Keep constrained
# credentials redacted in repr/model dumps while ApiClient serializes their
# explicit secret values onto the trusted transport.
declaration = "    {{name}}: {{{vendorExtensions.x-py-typing}}}"
assert template.count(declaration) == 1
template = template.replace(
    declaration,
    "{{#vendorExtensions.x-labbridge-secret}}\n"
    "    {{name}}: {{^required}}Optional[{{/required}}SecretStr{{^required}}]{{/required}} = Field("
    "{{^required}}default=None, {{/required}}"
    "{{#minLength}}min_length={{minLength}}, {{/minLength}}"
    '{{#maxLength}}max_length={{maxLength}}, {{/maxLength}}description="""{{{description}}}""", repr=False)\n'
    "{{/vendorExtensions.x-labbridge-secret}}"
    "{{^vendorExtensions.x-labbridge-secret}}\n" + declaration + "\n"
    "{{/vendorExtensions.x-labbridge-secret}}",
)
path = root / ".tools/templates/python/model_generic.mustache"
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(template.replace(needle, replacement))
