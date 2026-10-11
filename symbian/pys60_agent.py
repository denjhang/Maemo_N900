# -*- coding: utf-8 -*-
# Symbian PyS60 remote agent - a poor man's SSH for S60 phones.
# Runs on Python for S60 (1.4.x / 2.0.0, Python 2.2-2.5 dialect).
#
# Install once:
#   1. Install PyS60 .sis (official Nokia package) + PythonScriptShell
#   2. Copy this file to E:\\Python\\agent.py (E: = memory card) via USB cable
#   3. Phone: connect WiFi, run "Python" -> Options -> Run script -> agent.py
#   4. PC:  telnet <phone-ip> 8087   (or use client code below)
#
# Protocol: newline-terminated commands over TCP 8087:
#   EXEC|<python code>     -> executes code, returns repr of result
#   SHOT|                  -> returns PNG screen dump, length-prefixed
#   GET|<path>             -> returns file bytes, length-prefixed
#   PUT|<path>|<n>\n<n bytes>  -> writes n bytes to path
#   EXIT|                  -> quits agent

import e32, socket, sys

PORT = 8087

def start_server():
    serv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serv.bind(("", PORT))
    serv.listen(1)
    return serv

def recv_line(conn):
    buf = ""
    while "\n" not in buf:
        d = conn.recv(1)
        if not d:
            return None
        buf += d
    return buf[:-1]

def recv_n(conn, n):
    buf = ""
    while len(buf) < n:
        d = conn.recv(min(4096, n - len(buf)))
        if not d:
            break
        buf += d
    return buf

def try_screenshot():
    # PyS60 >= 1.3.8
    import graphics
    img = graphics.screenshot()
    return img  # has .save()

def main():
    serv = start_server()
    print "agent listening on port", PORT
    while True:
        conn, addr = serv.accept()
        try:
            line = recv_line(conn)
            if line is None:
                conn.close(); continue
            parts = line.split("|", 2)
            op = parts[0]
            if op == "EXEC":
                code = parts[1] if len(parts) > 1 else ""
                g = {"conn_result": None, "e32": e32, "sys": sys}
                try:
                    exec code in g
                    out = repr(g.get("conn_result"))
                except Exception, ex:
                    out = "ERR " + repr(ex)
                conn.send(out + "\n")
            elif op == "SHOT":
                try:
                    img = try_screenshot()
                    img.save("C:\\agent_shot.png")
                    f = open("C:\\agent_shot.png", "rb")
                    data = f.read(); f.close()
                    conn.send("OK %d\n" % len(data))
                    conn.send(data)
                except Exception, ex:
                    conn.send("ERR " + repr(ex) + "\n")
            elif op == "GET":
                try:
                    path = parts[1]
                    f = open(path, "rb")
                    data = f.read(); f.close()
                    conn.send("OK %d\n" % len(data))
                    conn.send(data)
                except Exception, ex:
                    conn.send("ERR " + repr(ex) + "\n")
            elif op == "PUT":
                try:
                    path, n = parts[1], int(parts[2])
                    rest = recv_line(conn)
                    data = rest
                    while len(data) < n:
                        data += recv_n(conn, n - len(data))
                    f = open(path, "wb")
                    f.write(data[:n]); f.close()
                    conn.send("OK\n")
                except Exception, ex:
                    conn.send("ERR " + repr(ex) + "\n")
            elif op == "EXIT":
                conn.send("BYE\n"); conn.close()
                break
            conn.close()
        except Exception, ex:
            try: conn.close()
            except: pass
    serv.close()

main()
