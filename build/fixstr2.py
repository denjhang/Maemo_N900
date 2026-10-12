import re
p = "/mnt/e/working/zcode-projects/Maemo_N900/build/gldemo2.c"
t = open(p, encoding="utf-8").read()
n = 0
# 形状1: 字符串断行后紧跟 ");（真实换行 → \n 转义）
t, a = re.subn(r'\n"\);', '\\n");', t)
n += a
# 形状2: 字符串断行后紧跟 ", ...（真实换行 → \n 转义）
t, b = re.subn(r'\n", ', '\\n", ', t)
n += b
# 形状3: 字符串断行后紧跟 "); 空格
t, c = re.subn(r'\n"\); ', '\\n"); ', t)
n += c
open(p, "w", encoding="utf-8").write(t)
print("fixed total", n, "(a=%d b=%d c=%d)" % (a, b, c))
