from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import json
import os
import shlex
import subprocess
import tempfile
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[1]
UI_DIR = Path(__file__).resolve().parent
TEST_DIR = ROOT / "test"
DOCS_DIR = ROOT / "docs"
INDEX_FILE = UI_DIR / "index.html"
STYLE_FILE = UI_DIR / "styles.css"
APP_FILE = UI_DIR / "app.js"
COMPILER = ROOT / "mini_compiler"


def read_text(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def list_samples():
    return [p.name for p in sorted(TEST_DIR.glob("*.pascal"))]


def split_sections(text: str):
    lines = text.splitlines()
    sections = []
    current_title = None
    current_lines = []
    i = 0
    while i < len(lines):
        line = lines[i].rstrip("\n")
        if line.strip() == "========================================" and i + 2 < len(lines) and lines[i + 2].strip() == "========================================":
            if current_title is not None:
                sections.append({"title": current_title, "content": "\n".join(current_lines).strip()})
                current_lines = []
            current_title = lines[i + 1].strip()
            i += 3
            continue
        if current_title is not None:
            current_lines.append(line)
        i += 1
    if current_title is not None:
        sections.append({"title": current_title, "content": "\n".join(current_lines).strip()})
    return sections


def to_wsl_path(path: Path) -> str:
    resolved = path.resolve()
    drive = resolved.drive
    if not drive:
        raise RuntimeError(f"cannot convert non-Windows path: {resolved}")
    drive_letter = drive.rstrip(":").lower()
    parts = resolved.parts[1:]
    return "/mnt/" + drive_letter + "/" + "/".join(parts)


def run_compiler(source_path: Path) -> dict:
    if os.name == "nt":
        root_wsl = to_wsl_path(ROOT)
        source_wsl = to_wsl_path(source_path)
        command = f"cd {shlex.quote(root_wsl)} && ./mini_compiler --with-lr {shlex.quote(source_wsl)}"
        result = subprocess.run(["wsl", "bash", "-lc", command], capture_output=True, text=True)
    else:
        result = subprocess.run([str(COMPILER), "--with-lr", str(source_path)], cwd=str(ROOT), capture_output=True, text=True)
    output = (result.stdout or "") + ("\n" + result.stderr if result.stderr else "")
    return {
        "returncode": result.returncode,
        "output": output,
        "sections": split_sections(output),
    }


def load_docs():
    return {
        "grammar": read_text(DOCS_DIR / "grammar_bnf.txt"),
        "first_follow": read_text(DOCS_DIR / "first_follow.txt"),
        "ll1": read_text(DOCS_DIR / "ll1_table.txt"),
        "lr": read_text(DOCS_DIR / "lr_table.txt"),

        "integration": "Lexer produces tokens; parsers consume the same token stream; the symbol table validates declarations/usages during parsing; the error handler aggregates diagnostics with line/column info.",
    }


class Handler(BaseHTTPRequestHandler):
    def send_json(self, payload, status=200):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_text(self, text, content_type="text/html; charset=utf-8"):
        body = text.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        query = parse_qs(parsed.query)
        if path == "/":
            self.send_text(read_text(INDEX_FILE))
            return
        if path == "/styles.css":
            self.send_text(read_text(STYLE_FILE), "text/css; charset=utf-8")
            return
        if path == "/app.js":
            self.send_text(read_text(APP_FILE), "application/javascript; charset=utf-8")
            return
        if path == "/api/samples":
            self.send_json({"samples": list_samples()})
            return
        if path == "/api/docs":
            self.send_json(load_docs())
            return
        if path == "/api/sample":
            name = query.get("name", [""])[0]
            sample_path = TEST_DIR / name
            if not sample_path.exists() or sample_path.suffix.lower() != ".pascal":
                self.send_json({"error": "sample not found"}, 404)
                return
            self.send_json({"name": name, "source": read_text(sample_path)})
            return
        self.send_json({"error": "not found"}, 404)

    def do_POST(self):
        if self.path != "/api/run":
            self.send_json({"error": "not found"}, 404)
            return
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(content_length).decode("utf-8"))
        except Exception:
            self.send_json({"error": "invalid json"}, 400)
            return

        mode = payload.get("mode", "sample")
        if mode == "sample":
            sample_name = payload.get("sample", "")
            source_path = TEST_DIR / sample_name
            if not source_path.exists():
                self.send_json({"error": "sample not found"}, 400)
                return
            result = run_compiler(source_path)
            result.update({"sample": sample_name, "source": read_text(source_path)})
            self.send_json(result)
            return

        source = payload.get("source", "")
        if not source.strip():
            self.send_json({"error": "source is empty"}, 400)
            return
        UI_TMP = UI_DIR / "tmp"
        UI_TMP.mkdir(parents=True, exist_ok=True)
        fd, temp_name = tempfile.mkstemp(suffix=".pascal", dir=str(UI_TMP))
        temp_path = Path(temp_name)
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as handle:
                handle.write(source)
            result = run_compiler(temp_path)
            result.update({"sample": "custom", "source": source})
            self.send_json(result)
        finally:
            try:
                temp_path.unlink(missing_ok=True)
            except Exception:
                pass


def main():
    port = int(os.environ.get("PORT", 8000))
    server = HTTPServer(("0.0.0.0", port), Handler)
    print(f"Mini compiler UI running at http://0.0.0.0:{port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
