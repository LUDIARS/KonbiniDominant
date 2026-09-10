"""Local Cc reservation only; no model or external service calls."""
import json
import os
import subprocess
import urllib.parse
import urllib.request


class NoRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise RuntimeError("local coordination endpoint attempted a redirect")


def local_http_base_url(value, name):
    try:
        parsed = urllib.parse.urlsplit(value)
        port = parsed.port
    except ValueError as error:
        raise ValueError(f"invalid {name}") from error
    if (parsed.scheme != "http" or parsed.hostname != "127.0.0.1" or
            port is None or parsed.username or parsed.password or
            parsed.path not in ("", "/") or parsed.query or parsed.fragment):
        raise ValueError(f"{name} must be an explicit 127.0.0.1 HTTP origin")
    return f"http://127.0.0.1:{port}"


def request(url, body=None):
    encoded = None if body is None else json.dumps(body, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(url, data=encoded, headers={"content-type": "application/json"})
    opener = urllib.request.build_opener(NoRedirectHandler())
    with opener.open(req, timeout=15) as response:
        value = json.load(response)
    if value.get("ok") is False:
        raise RuntimeError("Cc rejected the testing operation")
    return value


class TestingClaim:
    def __init__(self, evidence, body):
        self.evidence = evidence
        self.body = body
        self.session = None
        self.cc_url = None

    def __enter__(self):
        with (self.evidence / "cc-task.log").open("wb") as output:
            command = ["lictor", "cli", "task", "set", "--branch", "main", "--desc",
                       "[KD] No-LLM BT native 100x playtest from project body"]
            if os.name == "nt":
                # The installed Windows CLI is a PowerShell script. All command
                # text is fixed here; no endpoint or user input enters the shell.
                command = ["powershell", "-NoProfile", "-Command",
                           "& lictor cli task set --branch main --desc "
                           "'[KD] No-LLM BT native 100x playtest from project body'"]
            subprocess.run(command, cwd=self.body, stdout=output,
                           stderr=subprocess.STDOUT, check=True, timeout=20)
        port = int(os.environ["LICTOR_PORT"])
        if not 1 <= port <= 65535:
            raise ValueError("invalid Lictor sidecar port")
        own = request(f"http://127.0.0.1:{port}/v1/concordia/session")
        self.cc_url = local_http_base_url(
            os.environ.get("CONCORDIA_URL", ""), "CONCORDIA_URL")
        body = {"session_id": own["session_id"], "service": "konbini-dominant-app",
                "note": "Local deterministic BT playtest, direct native EXE from project body, 100x normal rules. No LLM."}
        path = self.evidence / "claim-request.json"
        path.write_text(json.dumps(body, ensure_ascii=False), encoding="utf-8")
        request(self.cc_url + "/v1/testing/claim", json.loads(path.read_text(encoding="utf-8")))
        self.session = own["session_id"]
        return self

    def __exit__(self, kind, value, traceback):
        if self.session:
            request(self.cc_url + "/v1/testing/release", {"session_id": self.session})
            (self.evidence / "claim-released.txt").write_text("released\n", encoding="utf-8")
