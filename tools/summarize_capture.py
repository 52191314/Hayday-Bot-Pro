"""
Create a redacted summary of logs/hayday_capture.json.

This intentionally omits request/response bodies and headers. It is for
triaging capture completeness without spreading session tokens or payload data.
"""

from __future__ import annotations

import json
import re
from collections import Counter
from pathlib import Path
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parents[1]
CAPTURE = ROOT / "logs" / "hayday_capture.json"
SUMMARY = ROOT / "logs" / "hayday_capture_summary.json"
INTERESTING = re.compile(r"(supercell|hayday|game-id|prod-game|game\.hayday)", re.I)


def main() -> None:
    data = json.loads(CAPTURE.read_text(encoding="utf-8"))

    hosts = Counter()
    events = Counter()
    methods = Counter()
    statuses = Counter()
    interesting_hosts = Counter()
    interesting_paths = Counter()

    for entry in data:
        host = str(entry.get("host") or "")
        method = str(entry.get("method") or "")
        event = str(entry.get("event") or "")
        status = entry.get("status")
        url = str(entry.get("url") or "")
        path = urlsplit(url).path or "/"

        hosts[host] += 1
        events[event] += 1
        if method:
            methods[method] += 1
        if status is not None:
            statuses[str(status)] += 1
        if INTERESTING.search(host):
            interesting_hosts[host] += 1
            interesting_paths[f"{method} {host}{path}"] += 1

    summary = {
        "capture": str(CAPTURE),
        "entries": len(data),
        "events": dict(events),
        "methods": dict(methods),
        "statuses": dict(statuses),
        "top_hosts": hosts.most_common(30),
        "interesting_entries": sum(interesting_hosts.values()),
        "interesting_hosts": interesting_hosts.most_common(),
        "interesting_paths": interesting_paths.most_common(50),
        "redaction": "headers and bodies intentionally omitted",
    }

    SUMMARY.write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"wrote {SUMMARY}")
    print(f"entries={summary['entries']} interesting_entries={summary['interesting_entries']}")


if __name__ == "__main__":
    main()
