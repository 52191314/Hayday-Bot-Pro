"""
mitmproxy addon that captures all Hay Day traffic.
Saves a JSON log of every request/response to logs/hayday_capture.json.
"""

import json
import re
import time
from pathlib import Path

LOG_DIR = Path(__file__).resolve().parents[1] / "logs"
LOG_DIR.mkdir(exist_ok=True)
JSON_LOG = LOG_DIR / "hayday_capture.json"
TMP_LOG = LOG_DIR / "hayday_capture.json.tmp"
MAX_ENTRIES = 1000

INTERESTING_HOSTS = re.compile(
    r"(supercell|hayday|game-id|prod-game|game\.hayday)", re.I
)

captured = []


def _body_text(message, limit):
    if not message or not getattr(message, "content", None):
        return ""
    try:
        return message.content.decode("utf-8", errors="replace")[:limit]
    except Exception:
        try:
            return message.content.hex()[:limit]
        except Exception:
            return ""


def _headers(message):
    if not message:
        return {}
    try:
        return {str(k): str(v) for k, v in message.headers.items()}
    except Exception:
        return {}


def _write_log():
    try:
        TMP_LOG.write_text(
            json.dumps(captured[-MAX_ENTRIES:], indent=2, ensure_ascii=False, default=str),
            encoding="utf-8",
        )
        TMP_LOG.replace(JSON_LOG)
    except Exception as exc:
        print(f"capture write failed: {exc!r}")


def request(flow):
    host = flow.request.pretty_host
    method = flow.request.method
    url = flow.request.pretty_url

    entry = {
        "ts": time.time(),
        "event": "request",
        "method": method,
        "url": url,
        "host": host,
        "req_headers": _headers(flow.request),
        "req_body": _body_text(flow.request, 4000),
    }

    captured.append(entry)
    _write_log()


def response(flow):
    host = flow.request.pretty_host
    method = flow.request.method
    url = flow.request.pretty_url
    status = flow.response.status_code if flow.response else None

    entry = {
        "ts": time.time(),
        "event": "response",
        "method": method,
        "url": url,
        "host": host,
        "status": status,
        "resp_headers": _headers(flow.response),
        "resp_body": _body_text(flow.response, 8000),
    }

    captured.append(entry)

    if INTERESTING_HOSTS.search(host):
        print(f"\n>>> {method} {url}  [{status}]")

    _write_log()
