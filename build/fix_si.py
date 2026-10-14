p='gldemo_imgui.c'
s=open(p).read()
bad='printf("imrun already running (pid %d)\n", oldpid);'
good='printf("imrun already running (pid %d)\\n", oldpid);'
assert s.count(bad)==1
s=s.replace(bad,good,1)
bad2='fprintf(wl, "%d\n", (int)getpid());'
good2='fprintf(wl, "%d\\n", (int)getpid());'
assert s.count(bad2)==1
s=s.replace(bad2,good2,1)
open(p,'w',newline='\n').write(s)
print('fixed')
