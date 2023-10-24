#  communication protocol
#
#  cmd 1: [0xAA] [ B1 ] [ B2 ] [0xE1] : set back light   ACK 1:[0xE1] ['\n']
#  cmd 2: [0xAA] [0xE2] : request timer                  ACK 2:[0xE2] [ T1 ] [ T2 ] [ T3 ] [ T4 ] ['\n']
#  cmd 3: [0xAA] [0xE3] : request voltage                ACK 3:[0xE3] [ U1 ] [ U2 ] ['\n']
#  cmd 4: [0xAA] [0xE4] : request die_temp               ACK 4:[0xE4] [ D1 ] [ D2 ] ['\n']
#  cmd 5: [0xAA] [0xE5] : request proximity              ACK 5:[0xE5] [ P1 ] ['\n']
#  cmd 6: [0xAA] [0xE6] : request light                  ACK 6:[0xE6] [ L1 ] [ L2 ] ['\n']

import serial
import time
import random

serialPort = serial.Serial(port = "COM20", baudrate=1000000, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE)

device = 0
password = 0
password_rev = 0

input = ""
timer = 0
voltage = 0
die_temp = 0
proximity = 0
light = 0




cmd0 = bytearray([0xAA, 0, 0xE0])
cmd1 = bytearray([0xAA, 0, 0, 0xE1])
cmd2 = bytearray([0xAA, 0xE2])
cmd3 = bytearray([0xAA, 0xE3])
cmd4 = bytearray([0xAA, 0xE4])
cmd5 = bytearray([0xAA, 0xE5])
cmd6 = bytearray([0xAA, 0xE6])

out =  bytearray([0xAA, 0x3A, 0, 0xE1])
out1 = bytearray([0xAA, 0,    0, 0xE1])
out2 = bytearray([0xAA, 0x7F, 0, 0xE1])

seq = [1,2,3,6,5,8,9,12,11,10,7,4]
#seq = [0,1,2,3,4,5,6,7,8,9,10,11,12]

#update cmd buffer array
def update_cmd(index):
    if(index < 7):
        cmd1[1] = 0
        cmd1[2] = 1 << index
    elif(index < 13):
        cmd1[1] = 1 << (index-7)
        cmd1[2] = 0
    else:
        cmd1[1] = 0
        cmd1[2] = 0

def update_random():
    #data = random.randrange(65536)
    cmd1[1] = random.randrange(128)
    cmd1[2] = random.randrange(128)

def check_reply():
    if(serialPort.in_waiting > 0):
    
        input = serialPort.readline()
        
        if(input[0] == 0xE0) and (len(input) == 3):
            password_rev = input[1]
            #password_rev = ~password_rev
            
            global device
            
            if(password_rev == password):
                device = 1   #device valid
                print(">> device OK !")
            else:
                device = 0   #device invalid
                print(">> device invalid !")
            
        elif(input[0] == 0xE1) and (len(input) == 2):
            #print("   E1 OK")
            pass
            
        elif(input[0] == 0xE2) and (len(input) == 6):
            tmp = bytearray([input[1], input[2], input[3], input[4]])
            timer = int.from_bytes(tmp, 'little')
            print("   E2 timer = " + str(timer))
            
        elif(input[0] == 0xE3) and (len(input) == 4):
            tmp = bytearray([input[1], input[2]])
            voltage = int.from_bytes(tmp, 'little')
            print("   E3 voltage = " + str(voltage))
            
        elif(input[0] == 0xE4) and (len(input) == 4):
            tmp = bytearray([input[1], input[2]])
            die_temp = int.from_bytes(tmp, 'little')
            print("   E4 die_temp = " + str(die_temp))
            
        elif(input[0] == 0xE5) and (len(input) == 3):
            proximity = input[1]
            print("   E5 proximity = " + str(proximity))
            
        elif(input[0] == 0xE6) and (len(input) == 4):
            tmp = bytearray([input[1], input[2]])
            light = int.from_bytes(tmp, 'little')
            print("   E6 light = " + str(light))
            
        else:
            print("   unknown reply")
            print(input)
            

#authenticate device
password = random.randrange(256)
cmd0[1] = password
serialPort.write(cmd0)
time.sleep(0.1)
check_reply()

#serialPort.write(out1)

while(device):
    for x in seq:
        serialPort.write(cmd1)
        check_reply()
        time.sleep(0.1)
        update_cmd(x)

#while(device):
#    for x in range(4):
#        serialPort.write(cmd1)
#        check_reply()
#        time.sleep(0.5)
#        update_random()
    
    print(">> =================")
    serialPort.write(cmd2)
    serialPort.write(cmd3)
    serialPort.write(cmd4)
    serialPort.write(cmd5)
    serialPort.write(cmd6)
    
    check_reply()
    check_reply()
    check_reply()
    check_reply()
    check_reply()
    
    #if(serialPort.in_waiting > 0):
    #    input = serialPort.readline()
    #    print(input.decode('Ascii'))    
  
  