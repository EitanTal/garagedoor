import subprocess
import os
from time import sleep
import threading, queue
import signal
import re
import time

q = queue.Queue()

def millis():
    return int(round(time.time() * 1000))

def tx(p,data):
    p.stdin.write( data.encode() + b'\n')
    p.stdin.flush() # after sending to stdin...

def worker(p):
    p.stdout.flush() # before reading from stdout...
    while True:
        line = p.stdout.readline()
        if len(line) == 0: break
        q.put(b'O>'+line)

def worker_err(p):
    p.stderr.flush() # before reading from stdout...
    while True:
        line = p.stderr.readline()
        if len(line) == 0: break
        q.put(b'E>'+line)

def rampup_thread_out(p):
    threading.Thread(target=worker, args=[p]).start()

def rampup_thread_outerr(p):
    threading.Thread(target=worker_err, args=[p]).start()

def printall_thread(q):
    while (not q.empty()):
        line = q.get()
        print("GDB> " + line.decode().strip())
        q.task_done()

def pexit(p):
    #p.send_signal(signal.CTRL_C_EVENT)
    p.stdin.write( b'q\n' )
    p.stdin.flush()
    p.wait()

def setup_write_watchpoint(p):
    tx(p, 'gdi icdbreak -set CR 0xA 0' )
    tx(p, 'gdi icdbreak -set BK1 0x00005231 0' )
    tx(p, 'gdi icdbreak -set BK2 0x00005231 0' )

def connect_and_reset(p):
    tx(p, 'emulator-reset-port-mcu usb://usb STM8S003F3')

def interactive(p):
    while (1):
        a = input()
        if (a == 'q'): break
        if (a == 'xxx'):
            setup_write_watchpoint(p)
            continue
        if (a == 'spy'):
            spy(p)
        p.stdin.write( a.encode()+b'\n' )
        p.stdin.flush()
        sleep(0.05)
        printall_thread(q)

def spy(p):
    setup_write_watchpoint(p)
    tx(p,'run')
    pattern = re.compile(rb'O>\(gdb\) \$\d+ = (\d+)')

    timeCapture = millis()
    while (True):
        line = q.get()
        q.task_done()
        uart_char_match = pattern.findall(line)
        if (millis() - timeCapture > 500):
            print('')
            timeCapture = millis() + 1000000        
        if (b'warning: Break on advanced breakpoint BK1 of Debug Module 0') in line:
            tx(p,'p $A')
            tx(p,'c')
        elif (uart_char_match):
            uart_char = int(uart_char_match[0])
            timeCapture = millis()
            print('{:02X} '.format(uart_char),end='')
            #print('{:02X} '.format(uart_char))



def inout_thread():
    cmd = [r'gdb7.exe', r'C:\garagedoor\Debug\firmware.elf', r'--command=swim\gdbswim_stlink.ini']
    os.chdir(r'C:\Program Files (x86)\STMicroelectronics\st_toolset\stvd')
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    rampup_thread_out(p)
    rampup_thread_outerr(p)
    connect_and_reset(p)
    tx(p,'asdfasdf')
    #spy(p)
    interactive(p)
    pexit(p)

inout_thread()

