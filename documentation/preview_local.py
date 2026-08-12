"""Build a local HTML preview of documentation/index.md (no Jekyll/Ruby needed).

Usage:
  python documentation/preview_local.py
Then open http://127.0.0.1:4000/index.preview.html
"""

from __future__ import annotations

import http.server
import socketserver
import threading
import webbrowser
from pathlib import Path

import markdown

ROOT = Path(__file__).resolve().parent
PORT = 5500


def build() -> Path:
    md_text = (ROOT / "index.md").read_text(encoding="utf-8")
    body = markdown.markdown(
        md_text,
        extensions=["tables", "fenced_code", "toc", "sane_lists"],
    )
    head_custom = (ROOT / "_includes" / "head-custom.html").read_text(encoding="utf-8")
    css_href = "assets/css/style.css"
    if not (ROOT / "assets" / "css" / "style.css").exists():
        css_href = "https://imperialchepi.github.io/healthgps/assets/css/style.css"

    html = f"""<!DOCTYPE html>
<html lang="en-US">
  <head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Health-GPS (local preview)</title>
    <link rel="stylesheet" href="{css_href}">
    {head_custom}
    <style>
      /* Make it obvious this is local preview */
      body::before {{
        content: "LOCAL PREVIEW — not GitHub Pages";
        display: block;
        background: #fff3cd;
        color: #664d03;
        text-align: center;
        padding: 0.4rem 0.75rem;
        font: 600 13px/1.4 Calibri, Arial, sans-serif;
        border-bottom: 1px solid #ffecb5;
      }}
    </style>
  </head>
  <body>
    <div class="wrapper">
      <header>
        <h1><a href="index.preview.html">Health-GPS</a></h1>
        <img src="images/logo.png" alt="Logo" />
        <p>Global Health Policy Simulation model</p>
        <p class="view"><a href="https://github.com/imperialCHEPI/healthgps">View the Project on GitHub <small>imperialCHEPI/healthgps</small></a></p>
        <ul class="downloads">
          <li><a href="#">Download <strong>ZIP File</strong></a></li>
          <li><a href="#">Download <strong>TAR Ball</strong></a></li>
          <li><a href="https://github.com/imperialCHEPI/healthgps">View On <strong>GitHub</strong></a></li>
        </ul>
      </header>
      <section>
{body}
      </section>
      <footer>
        <p>This project is maintained by <a href="https://github.com/imperialCHEPI">imperialCHEPI</a></p>
        <p><small>Documentation by <a href="https://profiles.imperial.ac.uk/mahima.ghosh23">Mahima Ghosh</a></small></p>
        <p><small>Local preview (Jekyll not required)</small></p>
      </footer>
    </div>
  </body>
</html>
"""
    out = ROOT / "index.preview.html"
    out.write_text(html, encoding="utf-8")
    return out


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def log_message(self, fmt: str, *args) -> None:
        print(f"[preview] {args[0]}")


def main() -> None:
    out = build()
    print(f"Built {out}")
    print(f"Serving {ROOT}")
    print(f"Open http://127.0.0.1:{PORT}/index.preview.html")

    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", PORT), Handler) as httpd:
        threading.Timer(
            0.8,
            lambda: webbrowser.open(f"http://127.0.0.1:{PORT}/index.preview.html"),
        ).start()
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nStopped.")


if __name__ == "__main__":
    main()
