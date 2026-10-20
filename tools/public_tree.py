#!/usr/bin/env python3
"""Fail on private artifacts accidentally staged for publication."""
import pathlib, re, subprocess

files = subprocess.check_output(["git", "ls-files", "-z"]).decode().split("\0")
errors = []
for name in filter(None, files):
    p = pathlib.Path(name)
    if p.suffix.lower() in (".bin", ".elf", ".apk", ".pem", ".key") or any(
        x in p.parts for x in ("results", ".venv", "node_modules")
    ):
        errors.append(name)
    data = p.read_text(errors="ignore")
    if re.search(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----", data):
        errors.append(name + ": private key")
    if re.search(r"(?:ghp_|github_pat_)[A-Za-z0-9_]{30,}", data):
        errors.append(name + ": GitHub token")
if errors:
    raise SystemExit("\n".join(errors))
print(f"Public-tree artifact checks passed ({len(files)-1} files)")
