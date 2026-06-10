#!/usr/bin/env python3

from html.parser import HTMLParser
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
GUIDE = ROOT / "docs" / "reactive-performance-mode" / "ui-guide"
INDEX = GUIDE / "index.html"
DEMO = ROOT / "docs" / "reactive-performance-mode" / "demo-session.md"

REQUIRED_ASSETS = {
    "assets/audio-midi-setup.svg",
    "assets/new-session-template.svg",
    "assets/reactive-session-open.svg",
    "assets/midi-map.svg",
    "assets/action-status.svg",
}

REQUIRED_SECTIONS = {
    "audio-midi",
    "template",
    "session",
    "midi",
    "status",
}

REQUIRED_CONTROLS = {
    "guide-progress",
    "reset-checklist",
}


class GuideParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.local_refs = []
        self.ids = set()
        self.image_alts = {}
        self.step_targets = set()

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.add(attrs["id"])
        if "data-step-target" in attrs:
            self.step_targets.add(attrs["data-step-target"])
        for key in ("href", "src"):
            value = attrs.get(key)
            if value and value.startswith("./"):
                self.local_refs.append(value[2:])
        if tag == "img" and attrs.get("src"):
            self.image_alts[attrs["src"].removeprefix("./")] = attrs.get("alt", "")


def fail(message):
    print(f"ui-guide validation failed: {message}", file=sys.stderr)
    return 1


def main():
    if not INDEX.exists():
        return fail(f"missing {INDEX.relative_to(ROOT)}")
    text = INDEX.read_text(encoding="utf-8")
    parser = GuideParser()
    parser.feed(text)

    missing_sections = sorted(REQUIRED_SECTIONS - parser.ids)
    if missing_sections:
        return fail(f"missing guide sections: {', '.join(missing_sections)}")

    missing_controls = sorted(REQUIRED_CONTROLS - parser.ids)
    if missing_controls:
        return fail(f"missing guide controls: {', '.join(missing_controls)}")

    missing_step_targets = sorted(REQUIRED_SECTIONS - parser.step_targets)
    if missing_step_targets:
        return fail(f"missing step navigation targets: {', '.join(missing_step_targets)}")

    missing_assets = sorted(asset for asset in REQUIRED_ASSETS if not (GUIDE / asset).exists())
    if missing_assets:
        return fail(f"missing required assets: {', '.join(missing_assets)}")

    missing_refs = sorted(ref for ref in parser.local_refs if not (GUIDE / ref).exists())
    if missing_refs:
        return fail(f"broken local references: {', '.join(missing_refs)}")

    missing_alt = sorted(asset for asset in REQUIRED_ASSETS if not parser.image_alts.get(asset))
    if missing_alt:
        return fail(f"missing image alt text: {', '.join(missing_alt)}")

    demo_text = DEMO.read_text(encoding="utf-8")
    if "ui-guide/index.html" not in demo_text:
        return fail("demo-session.md does not link to ui-guide/index.html")

    print("ui-guide validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
