#!/usr/bin/env python3
import argparse
import base64
import json
import os
import secrets
import subprocess
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path


HERE = Path(__file__).resolve().parent
AUTH = os.environ.get("QUALITY_LAB_AUTH", "meleys:meleys")


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(HERE / "static"), **kwargs)

    def _authorized(self):
        if not AUTH:
            return True
        header = self.headers.get("Authorization", "")
        if not header.startswith("Basic "):
            return False
        try:
            value = base64.b64decode(header[6:]).decode("utf-8")
        except Exception:
            return False
        return secrets.compare_digest(value, AUTH)

    def _require_auth(self):
        self.send_response(401)
        self.send_header("WWW-Authenticate", 'Basic realm="AI Six Sigma Quality Lab"')
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        if not self._authorized():
            self._require_auth()
            return
        super().do_GET()

    def do_POST(self):
        if not self._authorized():
            self._require_auth()
            return
        if self.path != "/api/consult":
            self.send_error(404)
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(min(length, 65536))
        try:
            payload = json.loads(raw.decode("utf-8"))
        except Exception:
            self.send_error(400, "Invalid JSON")
            return

        prompt = build_prompt(payload)
        result = run_hermes(prompt)
        data = json.dumps(result, ensure_ascii=False).encode("utf-8")

        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def build_prompt(payload):
    metrics = payload.get("metrics", {})
    pareto = payload.get("pareto", [])
    notes = payload.get("notes", "")
    return (
        "Jsi Six Sigma konzultant. Odpovez cesky, vecne a prakticky. "
        "Na zaklade metrik procesu priprav kratky komentar pro management: "
        "1) stav procesu, 2) hlavni rizika, 3) doporucene kroky DMAIC, "
        "4) co zmerit priste. Nevyzaduj dalsi data, pokud lze neco rict z dodanych metrik.\n\n"
        f"Metriky JSON:\n{json.dumps(metrics, ensure_ascii=False, indent=2)}\n\n"
        f"Pareto vad JSON:\n{json.dumps(pareto, ensure_ascii=False, indent=2)}\n\n"
        f"Poznamky uzivatele:\n{notes[:1200]}"
    )


def run_hermes(prompt):
    cmd = [
        "hermes",
        "-z",
        prompt,
        "--cli",
        "--safe-mode",
    ]
    try:
        proc = subprocess.run(
            cmd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=180,
            cwd=str(HERE),
        )
    except subprocess.TimeoutExpired:
        return {"ok": False, "text": "Hermes timeout: konzultant neodpovedel do 180 s."}
    except FileNotFoundError:
        return {"ok": False, "text": "Hermes CLI neni dostupne v PATH."}

    text = proc.stdout.strip()
    if proc.returncode == 0 and text:
        return {"ok": True, "text": text}

    err = proc.stderr.strip() or "Hermes nevratil finalni odpoved."
    return {"ok": False, "text": err[-2000:]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8090)
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"AI Six Sigma Quality Lab: http://{args.host}:{args.port}/", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
