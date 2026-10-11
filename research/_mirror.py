# -*- coding: utf-8 -*-
import os, re, sys, time
from urllib.parse import urljoin, urlparse, unquote
import urllib.request, ssl
START, OUT, HOST = sys.argv[1], sys.argv[2], sys.argv[3]
HEADERS={"User-Agent":"Mozilla/5.0 (Windows NT 10.0; Win64; x64)"}
CTX=ssl.create_default_context(); CTX.check_hostname=False; CTX.verify_mode=ssl.CERT_NONE
seen=set()
def local_path(url):
    p=urlparse(url); path=unquote(p.path).lstrip("/")
    if path=="" or path.endswith("/"): path+="index.html"
    if "." not in os.path.basename(path): path=os.path.join(path,"index.html")
    return os.path.join(OUT,path.replace("/",os.path.sep))
def fetch(url,tries=3):
    for i in range(tries):
        try:
            req=urllib.request.Request(url,headers=HEADERS)
            with urllib.request.urlopen(req,timeout=30,context=CTX) as r: return r.read(),r.headers.get("Content-Type","")
        except Exception as e:
            if i==tries-1: print("FAIL",url,e,flush=True); return None,None
            time.sleep(1.5)
def links(html,base):
    urls=[]
    for m in re.finditer(r'(?:href|src|data-[a-z]+)\s*=\s*["\']([^"\'#]+)["\']',html): urls.append(m.group(1))
    for m in re.finditer(r'url\(\s*["\']?([^"\')]+)["\']?\s*\)',html): urls.append(m.group(1))
    out=[]
    for u in urls:
        u=u.strip()
        if u.startswith(("javascript:","mailto:","data:","#")): continue
        full=urljoin(base,u); p=urlparse(full)
        if p.netloc==HOST:
            if p.scheme=="http": full="https://"+p.netloc+p.path+("?"+p.query if p.query else "")
            out.append(full.split("#")[0])
    return out
q=[START]
n=0
while q:
    url=q.pop(0)
    if url in seen: continue
    seen.add(url)
    data,ctype=fetch(url)
    if data is None: continue
    p=local_path(url); os.makedirs(os.path.dirname(p),exist_ok=True)
    open(p,"wb").write(data); n+=1
    if "html" in (ctype or "") or url.endswith("/"):
        try: html=data.decode("utf-8","replace")
        except Exception: continue
        for u in links(html,url):
            if u not in seen: q.append(u)
    if n%20==0: print("fetched",n,"queued",len(q),flush=True)
print("DONE",n,flush=True)
