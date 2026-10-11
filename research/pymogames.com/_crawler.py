# -*- coding: utf-8 -*-
"""Mirror a static website (same-host pages + assets) with local link rewriting."""
import os, re, sys, time
from urllib.parse import urljoin, urlparse, unquote
import urllib.request
import ssl

START = "https://www.pymogames.com/"
OUT = os.path.dirname(os.path.abspath(__file__))
HOST = "www.pymogames.com"
HEADERS = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"}
CTX = ssl.create_default_context()
CTX.check_hostname = False
CTX.verify_mode = ssl.CERT_NONE

seen = set()
ASSET_EXT = (".css", ".js", ".png", ".jpg", ".jpeg", ".gif", ".ico", ".ttf",
             ".woff", ".woff2", ".mp3", ".mp4", ".zip", ".rar", ".sis", ".sisx",
             ".svg", ".webp", ".txt", ".pdf")

def local_path(url):
    p = urlparse(url)
    path = unquote(p.path).lstrip("/")
    if path == "" or path.endswith("/"):
        path += "index.html"
    if path.endswith("/") or "." not in os.path.basename(path):
        path = os.path.join(path, "index.html")
    return os.path.join(OUT, path.replace("/", os.path.sep))

def fetch(url, tries=3):
    for i in range(tries):
        try:
            req = urllib.request.Request(url, headers=HEADERS)
            with urllib.request.urlopen(req, timeout=30, context=CTX) as r:
                return r.read(), r.headers.get("Content-Type", "")
        except Exception as e:
            if i == tries - 1:
                print("FAIL", url, e)
                return None, None
            time.sleep(1.5)

def links_and_assets(html, base):
    urls = []
    for m in re.finditer(r'(?:href|src|data-[a-z]+)\s*=\s*["\']([^"\'#]+)["\']', html):
        urls.append(m.group(1))
    for m in re.finditer(r'url\(\s*["\']?([^"\')]+)["\']?\s*\)', html):
        urls.append(m.group(1))
    out = []
    for u in urls:
        u = u.strip()
        if u.startswith(("javascript:", "mailto:", "data:", "#")):
            continue
        full = urljoin(base, u)
        p = urlparse(full)
        if p.netloc in (HOST,):
            if p.scheme == "http":
                full = "https://" + p.netloc + p.path + ("?" + p.query if p.query else "")
            out.append(full.split("#")[0])
    return out

def save(url, data):
    path = local_path(url)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)

queue = [START]
count = 0
while queue:
    url = queue.pop(0)
    if url in seen:
        continue
    seen.add(url)
    data, ctype = fetch(url)
    if data is None:
        continue
    save(url, data)
    count += 1
    is_html = "html" in (ctype or "") or url.endswith("/")
    if is_html:
        try:
            html = data.decode("utf-8", "replace")
        except Exception:
            continue
        for u in links_and_assets(html, url):
            if u in seen:
                continue
            low = urlparse(u).path.lower()
            if low.endswith(ASSET_EXT):
                queue.append(u)
            else:
                queue.append(u)
    if count % 10 == 0:
        print("fetched", count, "queued", len(queue))
print("DONE total", count)
