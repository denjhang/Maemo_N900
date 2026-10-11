# -*- coding: utf-8 -*-
import sys, os, zlib
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "sisinfo-master", "sis"))
import sisinfo, sisfields

def all_fields(root):
    out=[]; stack=[root]
    while stack:
        f=stack.pop(); out.append(f); stack.extend(getattr(f,"subFields",[]))
    return out

def raw_bytes(f):
    for obj in [f]+getattr(f,"subFields",[]):
        for a in ("rawdata","data"):
            v=getattr(obj,a,None)
            if isinstance(v,(bytes,bytearray)) and v: return bytes(v)
    return b""

def name_of(df):
    for sub in df.subFields:
        try:
            s=sub.readableStr()
            if s and len(s)>3: return s
        except Exception: pass
    return "unk_%d"%df.fileIndex

def extract(sisfile, outdir):
    info=sisinfo.SISInfo(); info.parse(sisfile)
    fl=all_fields(info)
    descs=[f for f in fl if type(f).__name__=="SISFileDescriptionField" and f.operation!=8]
    units=[raw_bytes(f) for f in fl if type(f).__name__=="SISFileDataField"]
    units=[(u, len(zlib.decompress(u)) if (len(u)>2 and u[0]==0x78) else len(u)) for u in units]
    used=set(); ok=0
    for d in sorted(descs, key=lambda d:-d.uncompressedLength):
        name=name_of(d).replace("\\","/").lstrip("/")
        if name.startswith("!:"): name=name[2:].lstrip("/")
        hit=None
        for i,(u,ulen) in enumerate(units):
            if i in used: continue
            if ulen==d.uncompressedLength: hit=i; break
        if hit is None:
            print("NO DATA FOR", name, d.uncompressedLength); continue
        used.add(hit)
        u=units[hit][0]
        try: data=zlib.decompress(u)
        except Exception: data=u
        out=os.path.join(outdir,name)
        os.makedirs(os.path.dirname(out) or ".",exist_ok=True)
        open(out,"wb").write(data)
        ok+=1
        print("OK %8d %s"%(len(data),name))
    print("extracted",ok,"of",len(descs))
    return ok

if __name__=="__main__":
    extract(sys.argv[1], sys.argv[2])
