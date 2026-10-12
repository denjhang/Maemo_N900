p = "/mnt/e/working/zcode-projects/Maemo_N900/build/gldemo2.c"
t = open(p, encoding="utf-8").read()
n = t.count(chr(10) + '");')
t = t.replace(chr(10) + '");', chr(92) + 'n");')
open(p, "w", encoding="utf-8", newline="\n").write(t)
print("pattern sites:", n)
idx = t.find("XOpenDisplay FAIL")
print(repr(t[idx:idx + 40]))
