import re
p = "/mnt/e/working/zcode-projects/Maemo_N900/build/gldemo2.c"
t = open(p, encoding="utf-8").read()
# 修复被真实换行打断的 C 字符串：'...\n");'（真实换行）→ '...\n"）转义形式
fixed, n1 = re.subn(r'\n"\);', '\\n");', t)
open(p, "w", encoding="utf-8").write(fixed)
print("fixed", n1, "broken string literals")
