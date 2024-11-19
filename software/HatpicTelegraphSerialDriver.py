##################################################################################
# Haptic Telegraph Serial Driver   
##################################################################################
#
# Controls Haptic Telegraph over serial port.  Has following features:
#   + reads in any and all serial traffic, can dump to a local file:
#       "hapticTelegraphDataYYMMDD.HH.MM.txt"
#
#   + drives haptic telegraph by sending commands over serial like:  
#        "B12.3" which the haptic telegraph maps to cmdB  (drives actuator B)
#                
#   + consumes and sends UDP packets b/w a desired IP address and PORT, 
#        will forward B commands directly to serial; will forward serial traffic
#        over UDP
#    
#   + TODO: has option to run webcam via openCV and read ArUco markers that may be
#        in scence; e.g., to track end-effector, base, and target of haptic tele.
# 
# BEFORE RUNNING:
#   + set SERIAL_PORT & BAUDRATE for your teensy seriel (comm info in arduino IDE)
#   + set UDP_IP and UDP_PORT if desired.
#  
#
# Please install *and* activate te HatpicTelegraphSerialDriver.py conda environment
# before running the script.  You can use alternatives to conda if you prefer. 
# For basic instructions on conda see:
# https://docs.conda.io/projects/conda/en/latest/user-guide/getting-started.html
# For additional tips/walthroughs see our guides:
# https://drive.google.com/drive/folders/10JO5NEnbRlizAJYX1gss3WAvmlMQl0se?usp=sharing 
#
# In a terminal/command prompt on Windows, MacOS, or Linux opened to the folder
# containting your files, run the following.  (You may need to run Anaconda Prompt
# vs just a native prompt/terminal if you have Anaconda installed). 
#
#   conda env create -f HapticTelegraph.yml
#
# Once complete:
#   conda info --envs                <-- lists environments that already exist
#   conda activate HapticTelegraph   <-- activates our environment.  You can also 
#                                        select this in VScode, PyCharm, etc.
#    
# You may need to reactivate this environment when re-opening your editor or 
# python instance/interpreter. 
#
# For OpenCV-Python documentation, see: 
#   https://docs.opencv.org/4.x/d6/d00/tutorial_py_root.html
#
# When done running, press 'q' in the openCV window for it to shut down nicely. 
###################################################################################
import serial
import socket
import select 
import time 
from datetime import datetime
import numpy as np
import cv2
import keyboard 

# Serial port settings must match arduino/teensy.  See arduino IDE for com port info
HW_EMULATION = False   # True: never open or use serial port, just fake the data. 
SERIAL_PORT  = "COM5"  # 'COM4'  '/dev/ttyUSB0'   etc.
BAUDRATE     = 115200  # must match in ardio Serial.begin(###) command

# Networking/UDP;  Use None for no UDP networking; "localhost" or "127.0.0.1" IP's
# If you cannot successfully "ping" the remote IP address, chances are there are 
# network permissions not in place.  Use a VPN to overcome this. See the Haptic
# Telegraph Software Guide for details: https://github.com/labmrd/HapticTelegraph
UDP_IP      = "127.0.0.1" # "localhost" # "127.0.0.1"  # None  # 128.95.215.232
UDP_PORT    = 8284 # remote machine uses this port; firewalls must allow this

# Option to dump data to a file; will show up in present working directory os.pwd()
# i.e., the folder you are running your python script in. 
writeToFile  = False  # if True this will dump all data from arduino serial to a 
                      # a local file named: filenameStubYYYY-mm-dd_HH-MM-SS 
filenameStub = "myHapticTelegraphData" 

# Generate a filename with the current date and time
if writeToFile:
    filename = filenameStub + datetime.now().strftime("%Y-%m-%d_%H-%M-%S") + ".txt"
    file = open(filename, 'w')


# OpenCV setup: TODO: make this an option, if runing, use mouse input to drive robot
cap =[]
# cap = cv2.VideoCapture(0)   # select other cameras if connected.  e.g. 0, 1, 2 etc.


# UDP / Networking stuff ... If an IP address is provided, do stuff with it, otherwise skip
if UDP_IP is not None:
    MESSAGE = "Hello, I'm Haptic Telegraph"
    print("UDP IP: %s" % UDP_IP)
    print("UDP PORT: %s" % UDP_PORT)
    print("message: %s" % MESSAGE)

    sock = socket.socket(socket.AF_INET,    # Internet
                        socket.SOCK_DGRAM)  # UDP
    sock.bind((UDP_IP, UDP_PORT))
    sock.setblocking(0)
    print("Listening for UDP packets on port %s ..." % UDP_PORT)


# Open serial port and pass through keyboard input
if not HW_EMULATION:
    print("Opening Serial port: " + SERIAL_PORT)
    hapticTelegraphSerial = serial.Serial(SERIAL_PORT,BAUDRATE)
    hapticTelegraphSerial.write(b"P\n")  # toggles printing of all data

## Take any strings that come through keyboard and pass them through
# Buffer to store the current line of input
input_buffer = []
def on_key_event(e):
    global input_buffer
    # Ignore Shift key events
    if e.name in ['shift', 'left shift', 'right shift']:
        return

    if e.name == 'enter':
        # Join the buffer into a single string and send it
        text_to_send = ''.join(input_buffer)
        try:
            hapticTelegraphSerial.write((text_to_send + '\n').encode('utf-8'))
            print(f"Sent: {text_to_send}")
        except Exception as ex:
            print(f"Error sending data: {ex}")
        # Clear the buffer
        input_buffer = []
    else:
        # Add character to buffer
        if len(e.name) == 1:
            input_buffer.append(e.name)
        elif e.name == 'space':
            input_buffer.append(' ')
        elif e.name == 'backspace' and input_buffer:
            input_buffer.pop()

# Set up the keyboard event listener
keyboard.on_press(on_key_event)


######################################################################
## The main loop ....
######################################################################

try: 
    while True:
        
        if not HW_EMULATION and not hapticTelegraphSerial.is_open:
            print("ERROR: serial port is no longer open, quitting ... ")
            exit 
        
        # TELEOPERATION ...
        # If there are incoming UDP packets, write them to the telegraph
        packetReady = select.select([sock], [], [], 1) 
        if packetReady[0]:
            data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
            print("received UDP message: %s " %  data.decode())  # use data.decode()
              
            cmd = data.decode()
            if not HW_EMULATION:
                hapticTelegraphSerial.write(cmd.encode())  # write packet data to serial      
                hapticTelegraphSerial.flush()
            else:
                print("EMULATION MODE: wrote %s to serial", cmd)
       
        # If there is incoming serial data, display it, log it, and dump to UDP       
        if not HW_EMULATION and hapticTelegraphSerial.in_waiting > 0:
            
            # print all lines that are available
            while hapticTelegraphSerial.in_waiting :    
                line = hapticTelegraphSerial.readline().decode('utf-8').strip()
                print( line )

                if writeToFile:
                    file.write(line + '\n')
                
            # parse serial and write UDP packets
            if (line[0]=='B'):
                print("received B command")
                MESSAGE =  line
                sock.sendto(MESSAGE.encode(), (UDP_IP, UDP_PORT))

        elif HW_EMULATION:
            line = "B123.4  EMULATED DATA"
            print( line )
            if writeToFile:
                file.write(line + '\n')
            sock.sendto(line.encode(), (UDP_IP, UDP_PORT))

    
        time.sleep(.01)



finally:
    
    # When everything done, release the and close stuff ...
    print ("Closing serial port ..." )
    hapticTelegraphSerial.close()

    if writeToFile:
        file.close()
        
    # cap.release()
    # cv2.destroyAllWindows()
