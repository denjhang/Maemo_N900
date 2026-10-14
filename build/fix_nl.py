p='gldemo_imgui.c'
s=open(p).read()
bad="    fputc('\n', alogf);"
good="    fputc('\\n', alogf);"
assert s.count(bad)==1
s=s.replace(bad,good,1)
open(p,'w',newline='\n').write(s)
print('ok')
