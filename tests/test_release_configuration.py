#!/usr/bin/env python3
"""Fail CI if Skelly Master is configured to ship anything but Windows VST3."""
from pathlib import Path
import re
import sys

source = Path(__file__).resolve().parents[1] / "CMakeLists.txt"
text = source.read_text(encoding="utf-8")
matches = re.findall(r"juce_add_plugin\s*\(\s*SkellyMaster\b(.*?)\)", text, flags=re.S)
if len(matches) != 1:
    raise SystemExit(f"Expected exactly one SkellyMaster JUCE target, found {len(matches)}")

block = matches[0]
formats = re.search(r"^\s*FORMATS\s+([^\r\n]+)", block, flags=re.M)
if not formats:
    raise SystemExit("SkellyMaster JUCE target has no explicit FORMATS declaration")
actual = formats.group(1).split()
if actual != ["VST3"]:
    raise SystemExit(f"Release policy requires exactly FORMATS VST3; found: {actual}")

required = {
    'PRODUCT_NAME "Skelly Master"': r'PRODUCT_NAME\s+"Skelly Master"',
    'BUNDLE_ID audio.skelly.master': r'BUNDLE_ID\s+"audio\.skelly\.master"',
    'manufacturer code SkAu': r'PLUGIN_MANUFACTURER_CODE\s+SkAu',
    'plugin code SkM1': r'PLUGIN_CODE\s+SkM1',
}
for label, pattern in required.items():
    if not re.search(pattern, block):
        raise SystemExit(f"Protected VST3 identity missing: {label}")

if not re.search(r"GIT_TAG\s+8\.0\.12\b", text):
    raise SystemExit("Expected pinned JUCE 8.0.12 FetchContent fallback")

print("PASS: Skelly Master builds VST3 only; plugin identity and JUCE 8.0.12 remain pinned")
