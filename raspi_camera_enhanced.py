#!/usr/bin/python
# python code for interfacing to VC0706 cameras and grabbing a photo
# pretty basic stuff
# written by ladyada. MIT license
# revisions for Raspberrry Pi by Gordon Rush
# revisions to enhanced Raspberry Pi capability by Leif Knag

# Usage: ./raspi_camera_enhanced.py                    <-- Run using motion detection until max # of pictures taken
#        or
#        ./raspi_camera_enhanced.py <high/medium/low>  <-- Set the resolution of pictures taken, use motion detection
#	 or
#	 ./raspi_camera_enhanced.py <something>        <-- Take one picture on demand
#	 or
#        ./raspi_camera_enhanced.py <high/medium/low> <something>  <-- Set the resolution of pictures taken, take one picture on demand

import serial 
import glob
import time
import sys

BAUD = 38400
# this is the port on the Raspberry Pi; it will be different for serial ports on other systems.
PORT = "/dev/ttyAMA0"

TIMEOUT = 0.5    # I needed a longer timeout than ladyada's 0.2 value
SERIALNUM = 0    # start with 0, each camera should have a unique ID.

COMMANDSEND = 0x56
COMMANDREPLY = 0x76
COMMANDEND = 0x00

CMD_GETVERSION = 0x11
CMD_RESET = 0x26
CMD_TAKEPHOTO = 0x36
CMD_READBUFF = 0x32
CMD_GETBUFFLEN = 0x34
CMD_COMM_MOTION_CTRL = 0x37
CMD_COMM_MOTION_STATUS = 0x38
CMD_COMM_MOTION_DETECTED = 0x39
CMD_MOTION_CTRL = 0x42
CMD_MOTION_STATUS = 0x43
CMD_READ_DATA = 0x30
CMD_WRITE_DATA = 0x31

FBUF_CURRENTFRAME = 0x00
FBUF_NEXTFRAME = 0x01

FBUF_STOPCURRENTFRAME = 0x00
FBUF_RESUMEFRAME = 0x02

RES_640x480 = 0x00	# Highest resoluition
RES_320x240 = 0x11	# Medium resolution (default)
RES_160x120 = 0x22	# Low resolution

if len(sys.argv) > 1:
	if sys.argv[1] == "high":
		resSize = RES_640x480
	elif sys.argv[1] == "low":
		resSize = RES_160x120
	else:
		resSize = RES_320x240
else:
	resSize = RES_320x240

resMap = { 	chr(RES_640x480):"640x480",
		chr(RES_320x240):"320x240",
		chr(RES_160x120):"160:120"
}

getversioncommand = [COMMANDSEND, SERIALNUM, CMD_GETVERSION, COMMANDEND]
resetcommand = [COMMANDSEND, SERIALNUM, CMD_RESET, COMMANDEND]
takephotocommand = [COMMANDSEND, SERIALNUM, CMD_TAKEPHOTO, 0x01, FBUF_STOPCURRENTFRAME]
getbufflencommand = [COMMANDSEND, SERIALNUM, CMD_GETBUFFLEN, 0x01, FBUF_CURRENTFRAME]
resumephotocommand = [COMMANDSEND, SERIALNUM, CMD_TAKEPHOTO, 0x01, FBUF_RESUMEFRAME]
startMotionDetectionCommand = [COMMANDSEND, SERIALNUM, CMD_COMM_MOTION_CTRL, 0x01, 0x01]
stopMotionDetectionCommand = [COMMANDSEND, SERIALNUM, CMD_COMM_MOTION_CTRL, 0x01, 0x00]
getImageSizeCommand = [COMMANDSEND, SERIALNUM, CMD_READ_DATA, 0x04, 0x04, 0x01, 0x00, 0x19]


# Change the RES value to something else if desired (see options above)
setImageSizeCommand = [COMMANDSEND, SERIALNUM, CMD_WRITE_DATA, 0x05, 0x04, 0x01, 0x00, 0x19, resSize]

def checkreply(r, b):
	r = map( ord, r )
	if( r[0] == COMMANDREPLY and r[1] == SERIALNUM and r[2] == b and r[3] == 0x00):
		return True
	return False

def reset():
	cmd = ''.join( map( chr, resetcommand ) )
	s.write(cmd)
	reply = s.read(100)
	r = list(reply)
	if checkreply( r, CMD_RESET ):
		return True
	return False

def getversion():
	cmd = ''.join( map( chr, getversioncommand ))
	s.write(cmd)
	reply = s.read(16)
	r = list(reply)
	# print r
	if checkreply( r, CMD_GETVERSION ):
		return True
	return False

def takephoto():
	cmd = ''.join( map( chr, takephotocommand ))
	s.write(cmd)
	reply = s.read(5)
	r = list(reply)
	# print r
	if( checkreply( r, CMD_TAKEPHOTO) and r[3] == chr(0x0)):
		return True
	return False

def resumephoto():
        cmd = ''.join( map( chr, resumephotocommand ))
        s.write(cmd)
        reply = s.read(5)
        r = list(reply)
        # print r
        if( checkreply( r, CMD_TAKEPHOTO) and r[3] == chr(0x0)):
                return True
        return False


def getbufferlength():
	cmd = ''.join( map( chr, getbufflencommand ))
	s.write(cmd)
	reply = s.read(9)
	r = list(reply)
	if( checkreply( r, CMD_GETBUFFLEN) and r[4] == chr(0x4)):
		l = ord(r[5])
		l <<= 8
		l += ord(r[6])
		l <<= 8
		l += ord(r[7])
		l <<= 8
		l += ord(r[8])
		return l
	return 0

readphotocommand = [COMMANDSEND, SERIALNUM, CMD_READBUFF, 0x0c, FBUF_CURRENTFRAME, 0x0a]


def readbuffer(bytes):
	addr = 0   # the initial offset into the frame buffer
	photo = []

	# bytes to read each time (must be a mutiple of 4)
	inc = 8192

	while( addr < bytes ):
 		# on the last read, we may need to read fewer bytes.
                chunk = min( bytes-addr, inc );

		# append 4 bytes that specify the offset into the frame buffer
		command = readphotocommand + [(addr >> 24) & 0xff, 
				(addr>>16) & 0xff, 
				(addr>>8 ) & 0xff, 
				addr & 0xff]

		# append 4 bytes that specify the data length to read
		command += [(chunk >> 24) & 0xff, 
				(chunk>>16) & 0xff, 
				(chunk>>8 ) & 0xff, 
				chunk & 0xff]

		# append the delay
		command += [1,0]

		# print map(hex, command)
		print "Reading", chunk, "bytes at", addr

		# make a string out of the command bytes.
		cmd = ''.join(map(chr, command))
	        s.write(cmd)

		# the reply is a 5-byte header, followed by the image data
		#   followed by the 5-byte header again.
		reply = s.read(5+chunk+5)

 		# convert the tuple reply into a list
		r = list(reply)
		if( len(r) != 5+chunk+5 ):
			# retry the read if we didn't get enough bytes back.
			print "Read", len(r), "Retrying."
			continue

		if( not checkreply(r, CMD_READBUFF)):
			print "ERROR READING PHOTO"
			print "Got this reply", r
			return ""
		
		# append the data between the header data to photo
		photo += r[5:chunk+5]

		# advance the offset into the frame buffer
		addr += chunk

	print addr, "Bytes written"
	return photo

def startMotionDetect():
        cmd = ''.join( map( chr, startMotionDetectionCommand ))
        s.write(cmd)
        reply = s.read(5)
        r = list(reply)
        # print r
        if( checkreply( r, CMD_COMM_MOTION_CTRL) and r[3] == chr(0x0)):
                return True
        return False

def checkMotion():
        reply = s.read(5)
        r = list(reply)
        # print r
	if len(r) > 0:
        	if( checkreply( r, CMD_COMM_MOTION_DETECTED) and r[3] == chr(0x0)):
                	return True
        	return False
	return False

def stopMotionDetect():
        cmd = ''.join( map( chr, stopMotionDetectionCommand ))
        s.write(cmd)
        reply = s.read(5)
        r = list(reply)
        # print r
        if( checkreply( r, CMD_COMM_MOTION_CTRL) and r[3] == chr(0x0)):
                return True
        return False

def getImageSize():	
        cmd = ''.join( map( chr, getImageSizeCommand ))
        s.write(cmd)
        reply = s.read(6)
        r = list(reply)
        # print r
        if( checkreply( r, CMD_READ_DATA)):
                return resMap[r[5]]
        return "Error reading image size"

def setImageSize():
        cmd = ''.join( map( chr, setImageSizeCommand ))
        s.write(cmd)
        reply = s.read(5)
        r = list(reply)
        # print r
        if( checkreply( r, CMD_WRITE_DATA) and r[3] == chr(0x0)):
                return True
        return False


######## main

s = serial.Serial( PORT, baudrate=BAUD, timeout = TIMEOUT )

reset()

if( not getversion() ):
	print "Camera not found"
	exit(0)

print "VC0706 Camera found"

# Build the outupt file name
photoName = "photo"  # Make this whatever you want
oldPhotos = glob.glob(photoName + "*.jpg")
oldPhotos.sort(key=str.lower)

# Add 1 to the number of the last picture taken, or start with 0
if (len(oldPhotos) > 0):
	# Make 5 something bigger if you are taking more than 100000 pictures
	# Add 1 to the number of the last picture taken, or start with 0
	if (len(oldPhotos) > 0):
		# Make 5 something bigger if you are taking more than 100000 pictures
		number = int(oldPhotos[-1][len(photoName):len(photoName)+5]) + 1
else:
	number = 0

# Run until max number of pictures are taken or it's killed
runFlag = True
maxPics = 10	# Make this however many you want, affects when motion detection picture taking stops

# Take one picture if more than one input argument are passed in or one argument that isn't a resolution
takeOnePic = len(sys.argv) > 2
if len(sys.argv) == 2:
	if sys.argv[1] != "high" and sys.argv[1] != "medium" and sys.argv[1] != "low":
		takeOnePic = True

if setImageSize():
        print "The image size is", getImageSize()
	reset()

if not takeOnePic:
	if startMotionDetect():
		print "Motion Detection Started"
		print "Waiting for something to move...\n"
else:
	print "Taking just one picture"

while (runFlag):
	if checkMotion() or takeOnePic:
		if not takeOnePic:
			print "I see you, you moving thing!"
			if stopMotionDetect():
				print "Motion detection suspended"
		
		if takephoto():
			print "Snap!"

		bytes = getbufferlength()

		print bytes, "bytes to read"

		photo = readbuffer( bytes )

		# Build the output file name
		curPhotoName = photoName + "0"*(5-len(str(number))) + str(number) + ".jpg"
		
		# Save the picture
		if len(photo) > 0:
			# Open output file
			f = open( curPhotoName, 'w' )

			photodata = ''.join( photo )

			f.write( photodata )

			f.close()
			
                	# Optional timer to slow down rate of picture taking                	
			#time.sleep(5)

			oldPhotos.append(curPhotoName)
        	        number += 1
		else:
			print "Picture corrupted, resetting"
			reset()
			startMotionDetect()
		
		# Break out of the loop if only taking one pic
		if takeOnePic:
			break

		# Resume photo buffer stream
		if resumephoto():
			print "Photo stream resuming"
		if startMotionDetect():
			print "Motion detection resuming"
		print "Waiting for something to move...\n"
	# Check to see if the max number of pictures has been reached
	if len(oldPhotos) >= maxPics:
		runFlag = False
