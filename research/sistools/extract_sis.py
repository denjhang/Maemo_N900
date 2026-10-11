# -*- coding: utf-8 -*-
import sys, os, zlib
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "sisinfo-master", "sis"))
import sisinfo, sisfields

def all_fields(root):
    stack=[root]
    while stack:
        f=stack.pop()
        yield f
        stack.extend(getattr(f,"subFields",[]))

def raw_bytes(f):
    # SISFileDataField: try rawdata attr, else first subfield's data/rawdata
    for obj in [f]+getattr(f,"subFields",[]):
        for a in ("rawdata","data"):
            v=getattr(obj,a,None)
            if isinstance(v,(bytes,bytearray)) and len(v)>0: return bytes(v)
    return b""

def name_of(desc_field):
    for sub in getattr(desc_field,"subFields",[]):
        s=sub.readableStr() if hasattr(sub,"readableStr") else ""
        if s and ("!" in s or "\\" in s or "/" in s or s.lower().endswith((".exe",".dll",".rsc",".rss",".mbm",".hlp",".sis",".dat",".ini"))):
            return s
        if s and len(s)>3: return s
    return None

def extract(sisfile, outdir):
    info=sisinfo.SISInfo(); info.parse(sisfile)
    fl=list(all_fields(info))
    desc=[f for f in fl if type(f).__name__=="SISFileDescriptionField"]
    names=[name_of(f) for f in desc]
    datas=[f for f in fl if type(f).__name__=="SISFileDataField"]
    print("files listed in controller:", len(names), "| data units:", len(datas))
    for i,(n,d) in enumerate(zip(names,datas)):
        data=raw_bytes(d)
        if not n:
            n="UNKNOWN_%03d.bin"%i
        n=n.replace("\\","/").lstrip("/")
        if n.startswith("!:"): n=n[2:].lstrip("\/")
        out=os.path.join(outdir,n)
        os.makedirs(os.path.dirname(out) or ".",exist_ok=True)
        open(out,"wb").write(data)
        print("%6d  %s"%(len(data),n))

if __name__=="__main__":
    extract(sys.argv[1], sys.argv[2])
