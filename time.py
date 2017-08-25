import socket
import time
import sys
import fcntl, os
import errno
from time import sleep

address = ('192.168.168.2', 26706)
address1 = ('192.168.168.1', 26706)
# address = ('172.16.78.1', 26706)
# address1 = ('172.16.78.129', 26706)

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(address1)
fl = fcntl.fcntl(s, fcntl.F_GETFL)
fcntl.fcntl(s, fcntl.F_SETFL, os.O_NONBLOCK)
s.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 2)

fd = open('input')
con = fd.read()
i=0

sa = "abcdefghijklmnopqrstuvwxyz"
con = ""
while 1:
    con += sa[i%26];
    i += 1
    if i == 1000:
        break


fuck_len = 0
def  generate():
    sss = ""
    global fuck_len
    for i in range(0, 1000):
        sss += sa[(fuck_len + i)%26]
    fuck_len += 1000
    return sss

# for i in range(0, 1000):

current_len = 0

def verity(msg):
    global current_len
    for i in range(0, len(msg)):
        assert msg[i] == sa[(current_len + i) % 26]
    current_len += len(msg)

i = 0;
while True:
    # msg = raw_input()
    tt = time.strftime('%Y-%m-%d %I:%M:%S',time.localtime(time.time()))
    tmp = 'time#' + tt + '\n'
    print tt
    i += len(tmp);
    fcntl.fcntl(s, fcntl.F_SETFL,  fl)
    assert len(tmp) == s.sendto(tmp, address)
    fcntl.fcntl(s, fcntl.F_SETFL,  (os.O_NONBLOCK))

    sleep(1)
    try:
        while 1:
            msg = s.recv(10000)
            print msg
    except socket.error, e:
        err = e.args[0]
        if err == errno.EAGAIN or err == errno.EWOULDBLOCK:
            continue
        else:
            pass

s.close()
