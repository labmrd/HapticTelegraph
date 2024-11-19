# A UDP packet router;  Packets from one IP get routed to another and vice versa
# Alice is one IP, Bob is the other. They can be different IP's or the same for loopback
# Any IP address will work as long as you have betwork permissions
# localhost is optimized, faster than 127.0.0.1 for local inter-process communication (IPC)

import socket
import select 


UDP_IP_ALICE        =  "localhost" # "127.0.0.1"   
UDP_IP_BOB          =  "127.0.0.1" #
UDP_PORT            =  8284


MESSAGE = "B-99.4" # use b"Hello, World!" to encode as bytes type, or .encode() as needed

print("UDP Alice IP: %s" % UDP_IP_ALICE)
print("UDP Bob IP: %s" % UDP_IP_BOB)
print("UDP listening & sending on port: %s" % UDP_PORT)
print("message: %s" % MESSAGE)

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP


sock.bind((UDP_IP_ALICE, UDP_PORT))
sock.setblocking(0)


i = 0
while True:
    packetReady = select.select([sock], [], [], 1) 
    if packetReady[0]:
        data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
        print("received UDP message (%i): %s   & sending it ..." % (i, data.decode()))  # use data.decode()
        sock.sendto(MESSAGE.encode(), (UDP_IP_BOB, UDP_PORT))
        i = i+1
