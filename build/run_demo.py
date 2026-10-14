"""Deploy a demo binary to N900 and run it with a given /home/user/gmode mode char.
Usage: python run_demo.py [bin_name] [mode_char]
  bin_name : gldemo2bin (default) or gldemo_scenebin -> deploys to /home/user/<name>
  mode_char: written to /home/user/gmode ('u'=UI, 'g'=game3d, 'b'=both; omit = keep)
"""
import sys
import paramiko

binname = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].endswith('mode') else 'gldemo2bin'
mode = None
for a in sys.argv[1:]:
    if a != binname:
        mode = a
if len(sys.argv) > 2:
    mode = sys.argv[2]

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect('192.168.101.108', username='root', password='123456',
          allow_agent=False, look_for_keys=False)
s = c.open_sftp()
s.put(binname, '/home/user/' + binname)
s.chmod('/home/user/' + binname, 0o755)
if mode:
    f = s.open('/home/user/gmode', 'w')
    f.write(mode)
    f.close()
s.close()

i, o, e = c.exec_command(
    "cd /home/user && DISPLAY=:0 ./%s > /home/user/o.log 2> /home/user/e.log; "
    "echo EXIT=$?; echo ---STDERR---; cat /home/user/e.log" % binname)
o.channel.settimeout(120)
print(o.read().decode('utf-8', 'replace'))
c.close()
