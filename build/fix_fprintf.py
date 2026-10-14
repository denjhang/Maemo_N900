p='imgui_gles1.cpp'
s=open(p,encoding='utf-8').read()
bad='wall_mode=%d\nwall_variant=%d\na_win=%.2f\na_bar=%.2f\na_desk=%.2f\ndock_cover=%d\n'
good='wall_mode=%d\\nwall_variant=%d\\na_win=%.2f\\na_bar=%.2f\\na_desk=%.2f\\ndock_cover=%d\\n'
assert s.count(bad)==1, s.count(bad)
s=s.replace(bad,good,1)
open(p,'w',encoding='utf-8',newline='\n').write(s)
print('really fixed')
