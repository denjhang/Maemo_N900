# -*- coding: utf-8 -*-
"""Extract files from Symbian 9.x SIS packages using the sisinfo library."""
import sys, os, zlib
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "sisinfo-master", "sis"))
import sisinfo, sisfields, sisreader

def walk(field, depth=0, out=None):
    if out is None: out = []
    out.append((field, depth))
    for sub in getattr(field, "subFields", []):
        walk(sub, depth+1, out)
    return out

def extract(sisfile, outdir):
    info = sisinfo.SISInfo()
    info.parse(sisfile)
    fields = walk(info)
    count = 0
    for f, d in fields:
        t = type(f).__name__
        if t == "SISCompressedField":
            pass  # data handled via raw fields below
        if t == "SISFileDataField":
            raw = getattr(f, "rawdata", None)
            if raw:
                try:
                    data = zlib.decompress(raw)
                except Exception:
                    data = raw
                # find target name from sibling SISFileDescriptionField in same DataUnit
                name = None
                if hasattr(f, "subFields") and f.subFields:
                    blob = f.subFields[0]
                os.makedirs(outdir, exist_ok=True)
                out = os.path.join(outdir, "blob_%03d.bin" % count)
                open(out, "wb").write(data)
                print("dumped", out, len(data), "bytes")
                count += 1
    return count

if __name__ == "__main__":
    n = extract(sys.argv[1], sys.argv[2])
    print("total", n)
