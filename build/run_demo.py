"""Deploy gldemo2bin to N900 and run it with a given /home/user/gmode mode char.
Usage: python run_demo.py <mode_char>   (mode char optional; omitted = leave file)
"""
import sys, io, time
import paramiko

mode = sys.argv[1] if len(sys.argv) > 1 else None

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect('192.168.101.111', username='root', password='123456',
          allow_agent=False, look_for_keys=False)
s = c.open_sftp()
s.put('gldemo2bin', '/home/user/gldemo2')
s.chmod('/home/user/gldemo2', 0o755)

if mode:
    f = s.open('/home/user/gmode', 'w')
    f.write(mode)
    f.close()
s.close()

i, o, e = c.exec_command(
    "cd /home/user && DISPLAY=:0 ./gldemo2 > /home/user/o.log 2> /home/user/e.log; "
    "echo EXIT=$?; echo ---STDERR---; cat /home/user/e.log; echo ---STDOUT---; cat /home/user/o.log")
o.channel.settimeout(120)
print(o.read().decode('utf-8', 'replace'))
c.close()
