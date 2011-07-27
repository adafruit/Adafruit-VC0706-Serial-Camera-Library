// This is a simple VC0706 library that doesnt suck
// (c) adafruit - MIT license - https://github.com/adafruit/

#include "VC0706.h"


VC0706::VC0706(NewSoftSerial *ser) {
  camera = ser;
  frameptr = 0;
  bufferLen = 0;
}

boolean VC0706::begin(uint16_t baud) {
  camera->begin(baud);
  return reset();
}


boolean VC0706::reset() {
  uint8_t args[] = {0x0};

  return runCommand(VC0706_RESET, args, 1, 5);
}

boolean VC0706::motionDetected() {
  if (readResponse(4, 200) != 4) {
    return false;
  }
  if (! verifyResponse(VC0706_COMM_MOTION_DETECTED))
    return false;

  return true;
}


uint8_t VC0706::setMotionStatus(uint8_t x, uint8_t d1, uint8_t d2) {
  uint8_t args[] = {0x03, x, d1, d2};

  return runCommand(VC0706_MOTION_CTRL, args, sizeof(args), 5);
}


uint8_t VC0706::getMotionStatus(uint8_t x) {
  uint8_t args[] = {0x01, x};

  return runCommand(VC0706_MOTION_STATUS, args, sizeof(args), 5);
}


boolean VC0706::setMotionDetect(boolean flag) {
  if (! setMotionStatus(VC0706_MOTIONCONTROL, 
			VC0706_UARTMOTION, VC0706_ACTIVATEMOTION))
    return false;

  uint8_t args[] = {0x01, flag};
  
  runCommand(VC0706_COMM_MOTION_CTRL, args, sizeof(args), 5);
}



boolean VC0706::getMotionDetect(void) {
  uint8_t args[] = {0x0};

  if (! runCommand(VC0706_COMM_MOTION_STATUS, args, 1, 6))
    return false;

  return camerabuff[5];
}

uint8_t VC0706::getImageSize() {
  uint8_t args[] = {0x4, 0x4, 0x1, 0x00, 0x19};
  if (! runCommand(VC0706_READ_DATA, args, sizeof(args), 6))
    return -1;

  return camerabuff[5];
}

boolean VC0706::setImageSize(uint8_t x) {
  uint8_t args[] = {0x05, 0x04, 0x01, 0x00, 0x19, x};

  return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

/****************** downsize image control */

uint8_t VC0706::getDownsize(void) {
  uint8_t args[] = {0x0};
  if (! runCommand(VC0706_DOWNSIZE_STATUS, args, 1, 6)) 
    return -1;

   return camerabuff[5];
}

boolean VC0706::setDownsize(uint8_t newsize) {
  uint8_t args[] = {0x01, newsize};

  return runCommand(VC0706_DOWNSIZE_CTRL, args, 2, 5);
}

/***************** other high level commands */

char * VC0706::getVersion(void) {
  uint8_t args[] = {0x01};
  
  sendCommand(VC0706_GEN_VERSION, args, 1);
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200)) 
    return 0;
  camerabuff[bufferLen] = 0;  // end it!
  return (char *)camerabuff;  // return it!
}


/****************** high level photo comamnds */

boolean VC0706::takePicture() {
  frameptr = 0;
  return cameraFrameBuffCtrl(VC0706_STOPCURRENTFRAME); 
}


boolean VC0706::cameraFrameBuffCtrl(uint8_t command) {
  uint8_t args[] = {0x1, command};
  return runCommand(VC0706_FBUF_CTRL, args, sizeof(args), 5);
}

uint32_t VC0706::frameLength(void) {
  uint8_t args[] = {0x01, 0x00};
  if (!runCommand(VC0706_GET_FBUF_LEN, args, sizeof(args), 9))
    return 0;

  uint32_t len;
  len = camerabuff[5];
  len <<= 8;
  len |= camerabuff[6];
  len <<= 8;
  len |= camerabuff[7];
  len <<= 8;
  len |= camerabuff[8];
  return len;
}


uint8_t VC0706::available(void) {
  return bufferLen;
}


uint8_t * VC0706::readPicture(uint8_t n) {
  uint8_t args[] = {0x0C, 0x0, 0x0A, 
                    0, 0, frameptr >> 8, frameptr & 0xFF, 
                    0, 0, 0, n, 
                    CAMERADELAY >> 8, CAMERADELAY & 0xFF};

  if (! runCommand(VC0706_READ_FBUF, args, sizeof(args), 5))
    return 0;

  // read into the buffer PACKETLEN!
  if (readResponse(n+5, 250) == 0) 
      return 0;
  frameptr += n;

  return camerabuff;
}

/**************** low level commands */


boolean VC0706::runCommand(uint8_t cmd, uint8_t *args, uint8_t argn, 
			   uint8_t resplen) {
  // flush out anything in the buffer?
  readResponse(100, 200); 

  sendCommand(cmd, args, argn);
  if (readResponse(resplen, 200) != resplen) 
    return false;
  if (! verifyResponse(cmd))
    return false;
  return true;
}

void VC0706::sendCommand(uint8_t cmd, uint8_t args[] = 0, uint8_t argn = 0) {
  camera->print(0x56, BYTE);
  camera->print(cameraSerial, BYTE);
  camera->print(cmd, BYTE);

  for (uint8_t i=0; i<argn; i++) {
    camera->print(args[i], BYTE);
  }
}


uint8_t VC0706::readResponse(uint8_t numbytes, uint8_t timeout) {
  uint8_t counter = 0;
  bufferLen=0;
 
 while ((timeout != counter) && (bufferLen != numbytes)){
   if (! camera->available()) {
     delay(1);
     counter++;
     continue;
   }
   counter = 0;
   // there's a byte!
   camerabuff[bufferLen++] = camera->read();
 }
 return bufferLen;
}

boolean VC0706::verifyResponse(uint8_t command) {
  if ((camerabuff[0] != 0x76) ||
      (camerabuff[1] != cameraSerial) ||
      (camerabuff[2] != command) ||
      (camerabuff[3] != 0x0))
      return false;
  return true;
  
}

void VC0706::printBuff() {
  for (uint8_t i = 0; i< bufferLen; i++) {
    Serial.print(" 0x");
    Serial.print(camerabuff[i], HEX); 
  }
  Serial.println();
}
