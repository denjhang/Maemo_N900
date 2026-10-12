import re
p = "/mnt/e/working/zcode-projects/Maemo_N900/build/gldemo2.c"
t = open(p, encoding="utf-8").read()
n0 = t.count("\r\n")
t = t.replace("\r\n", "\n")
n = 0
t, a = re.subn(r'\n"\);', '\\n");', t)
n += a
t, b = re.subn(r'\n", ', '\\n", ', t)
n += b
open(p, "w", encoding="utf-8", newline="\n").write(t)
print("crlf normalized:", n0, "| strings fixed:", n, "(a=%d b=%d)" % (a, b))
