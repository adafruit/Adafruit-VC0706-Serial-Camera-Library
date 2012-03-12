/*************************************************** 
  This is a library for the Adafruit TTL JPEG Camera (VC0706 chipset)

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/397

  These displays use Serial to communicate, 2 pins are required to interface

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/


#include "Adafruit_VC0706.h"

// Initialization code used by all constructor types
void Adafruit_VC0706::common_init(void) {
  swSerial  = NULL;
  hwSerial  = NULL;
  frameptr  = 0;
  bufferLen = 0;
  serialNum = 0;
}

// Constructor when using SoftwareSerial or NewSoftSerial
#if ARDUINO >= 100
Adafruit_VC0706::Adafruit_VC0706(SoftwareSerial *ser) {
#else
Adafruit_VC0706::Adafruit_VC0706(NewSoftSerial *ser) {
#endif
  common_init();  // Set everything to common state, then...
  swSerial = ser; // ...override swSerial with value passed.
}

// Constructor when using HardwareSerial
Adafruit_VC0706::Adafruit_VC0706(HardwareSerial *ser) {
  common_init();  // Set everything to common state, then...
  hwSerial = ser; // ...override hwSerial with value passed.
}

boolean Adafruit_VC0706::begin(uint16_t baud) {
  if(swSerial) swSerial->begin(baud);
  else         hwSerial->begin(baud);
  return reset();
}

boolean Adafruit_VC0706::reset() {
  uint8_t args[] = {0x0};

  return runCommand(VC0706_RESET, args, 1, 5);
}

boolean Adafruit_VC0706::motionDetected() {
  if (readResponse(4, 200) != 4) {
    return false;
  }
  if (! verifyResponse(VC0706_COMM_MOTION_DETECTED))
    return false;

  return true;
}


uint8_t Adafruit_VC0706::setMotionStatus(uint8_t x, uint8_t d1, uint8_t d2) {
  uint8_t args[] = {0x03, x, d1, d2};

  return runCommand(VC0706_MOTION_CTRL, args, sizeof(args), 5);
}


uint8_t Adafruit_VC0706::getMotionStatus(uint8_t x) {
  uint8_t args[] = {0x01, x};

  return runCommand(VC0706_MOTION_STATUS, args, sizeof(args), 5);
}


boolean Adafruit_VC0706::setMotionDetect(boolean flag) {
  if (! setMotionStatus(VC0706_MOTIONCONTROL, 
			VC0706_UARTMOTION, VC0706_ACTIVATEMOTION))
    return false;

  uint8_t args[] = {0x01, flag};
  
  runCommand(VC0706_COMM_MOTION_CTRL, args, sizeof(args), 5);
}



boolean Adafruit_VC0706::getMotionDetect(void) {
  uint8_t args[] = {0x0};

  if (! runCommand(VC0706_COMM_MOTION_STATUS, args, 1, 6))
    return false;

  return camerabuff[5];
}

uint8_t Adafruit_VC0706::getImageSize() {
  uint8_t args[] = {0x4, 0x4, 0x1, 0x00, 0x19};
  if (! runCommand(VC0706_READ_DATA, args, sizeof(args), 6))
    return -1;

  return camerabuff[5];
}

boolean Adafruit_VC0706::setImageSize(uint8_t x) {
  uint8_t args[] = {0x05, 0x04, 0x01, 0x00, 0x19, x};

  return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

/****************** downsize image control */

uint8_t Adafruit_VC0706::getDownsize(void) {
  uint8_t args[] = {0x0};
  if (! runCommand(VC0706_DOWNSIZE_STATUS, args, 1, 6)) 
    return -1;

   return camerabuff[5];
}

boolean Adafruit_VC0706::setDownsize(uint8_t newsize) {
  uint8_t args[] = {0x01, newsize};

  return runCommand(VC0706_DOWNSIZE_CTRL, args, 2, 5);
}

/***************** other high level commands */

char * Adafruit_VC0706::getVersion(void) {
  uint8_t args[] = {0x01};
  
  sendCommand(VC0706_GEN_VERSION, args, 1);
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200)) 
    return 0;
  camerabuff[bufferLen] = 0;  // end it!
  return (char *)camerabuff;  // return it!
}


/****************** high level photo comamnds */

void Adafruit_VC0706::OSD(uint8_t x, uint8_t y, char *str) {
  if (strlen(str) > 14) { str[13] = 0; }

  uint8_t args[17] = {strlen(str), strlen(str)-1, (y & 0xF) | ((x & 0x3) << 4)};

  for (uint8_t i=0; i<strlen(str); i++) {
    char c = str[i];
    if ((c >= '0') && (c <= '9')) {
      str[i] -= '0';
    } else if ((c >= 'A') && (c <= 'Z')) {
      str[i] -= 'A';
      str[i] += 10;
    } else if ((c >= 'a') && (c <= 'z')) {
      str[i] -= 'a';
      str[i] += 36;
    }

    args[3+i] = str[i];
  }

   runCommand(VC0706_OSD_ADD_CHAR, args, strlen(str)+3, 5);
   printBuff();
}

boolean Adafruit_VC0706::setCompression(uint8_t c) {
  uint8_t args[] = {0x5, 0x1, 0x1, 0x12, 0x04, c};
  return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

uint8_t Adafruit_VC0706::getCompression(void) {
  uint8_t args[] = {0x4, 0x1, 0x1, 0x12, 0x04};
  runCommand(VC0706_READ_DATA, args, sizeof(args), 6);
  printBuff();
  return camerabuff[5];
}

boolean Adafruit_VC0706::setPTZ(uint16_t wz, uint16_t hz, uint16_t pan, uint16_t tilt) {
  uint8_t args[] = {0x08, wz >> 8, wz, 
		    hz >> 8, wz, 
		    pan>>8, pan, 
		    tilt>>8, tilt};

  return (! runCommand(VC0706_SET_ZOOM, args, sizeof(args), 5));
}


boolean Adafruit_VC0706::getPTZ(uint16_t &w, uint16_t &h, uint16_t &wz, uint16_t &hz, uint16_t &pan, uint16_t &tilt) {
  uint8_t args[] = {0x0};
  
  if (! runCommand(VC0706_GET_ZOOM, args, sizeof(args), 16))
    return false;
  printBuff();

  w = camerabuff[5];
  w <<= 8;
  w |= camerabuff[6];

  h = camerabuff[7];
  h <<= 8;
  h |= camerabuff[8];

  wz = camerabuff[9];
  wz <<= 8;
  wz |= camerabuff[10];

  hz = camerabuff[11];
  hz <<= 8;
  hz |= camerabuff[12];

  pan = camerabuff[13];
  pan <<= 8;
  pan |= camerabuff[14];

  tilt = camerabuff[15];
  tilt <<= 8;
  tilt |= camerabuff[16];

  return true;
}


boolean Adafruit_VC0706::takePicture() {
  frameptr = 0;
  return cameraFrameBuffCtrl(VC0706_STOPCURRENTFRAME); 
}

boolean Adafruit_VC0706::resumeVideo() {
  return cameraFrameBuffCtrl(VC0706_RESUMEFRAME); 
}

boolean Adafruit_VC0706::TVon() {
  uint8_t args[] = {0x1, 0x1};
  return runCommand(VC0706_TVOUT_CTRL, args, sizeof(args), 5);
}
boolean Adafruit_VC0706::TVoff() {
  uint8_t args[] = {0x1, 0x0};
  return runCommand(VC0706_TVOUT_CTRL, args, sizeof(args), 5);
}

boolean Adafruit_VC0706::cameraFrameBuffCtrl(uint8_t command) {
  uint8_t args[] = {0x1, command};
  return runCommand(VC0706_FBUF_CTRL, args, sizeof(args), 5);
}

uint32_t Adafruit_VC0706::frameLength(void) {
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


uint8_t Adafruit_VC0706::available(void) {
  return bufferLen;
}


uint8_t * Adafruit_VC0706::readPicture(uint8_t n) {
  uint8_t args[] = {0x0C, 0x0, 0x0A, 
                    0, 0, frameptr >> 8, frameptr & 0xFF, 
                    0, 0, 0, n, 
                    CAMERADELAY >> 8, CAMERADELAY & 0xFF};

  if (! runCommand(VC0706_READ_FBUF, args, sizeof(args), 5, false))
    return 0;


  // read into the buffer PACKETLEN!
  if (readResponse(n+5, CAMERADELAY) == 0) 
      return 0;


  frameptr += n;

  return camerabuff;
}

/**************** low level commands */


boolean Adafruit_VC0706::runCommand(uint8_t cmd, uint8_t *args, uint8_t argn, 
			   uint8_t resplen, boolean flushflag) {
  // flush out anything in the buffer?
  if (flushflag) {
    readResponse(100, 10); 
  }

  sendCommand(cmd, args, argn);
  if (readResponse(resplen, 200) != resplen) 
    return false;
  if (! verifyResponse(cmd))
    return false;
  return true;
}

void Adafruit_VC0706::sendCommand(uint8_t cmd, uint8_t args[] = 0, uint8_t argn = 0) {
  if(swSerial) {
#if ARDUINO >= 100
    swSerial->write((byte)0x56);
    swSerial->write((byte)serialNum);
    swSerial->write((byte)cmd);

    for (uint8_t i=0; i<argn; i++) {
      swSerial->write((byte)args[i]);
      //Serial.print(" 0x");
      //Serial.print(args[i], HEX);
    }
#else
    swSerial->print(0x56, BYTE);
    swSerial->print(serialNum, BYTE);
    swSerial->print(cmd, BYTE);

    for (uint8_t i=0; i<argn; i++) {
      swSerial->print(args[i], BYTE);
      //Serial.print(" 0x");
      //Serial.print(args[i], HEX);
    }
#endif
  } else {
#if ARDUINO >= 100
    hwSerial->write((byte)0x56);
    hwSerial->write((byte)serialNum);
    hwSerial->write((byte)cmd);

    for (uint8_t i=0; i<argn; i++) {
      hwSerial->write((byte)args[i]);
      //Serial.print(" 0x");
      //Serial.print(args[i], HEX);
    }
#else
    hwSerial->print(0x56, BYTE);
    hwSerial->print(serialNum, BYTE);
    hwSerial->print(cmd, BYTE);

    for (uint8_t i=0; i<argn; i++) {
      hwSerial->print(args[i], BYTE);
      //Serial.print(" 0x");
      //Serial.print(args[i], HEX);
    }
#endif
  }
}

uint8_t Adafruit_VC0706::readResponse(uint8_t numbytes, uint8_t timeout) {
  uint8_t counter = 0;
  bufferLen = 0;
  int avail;
 
  while ((timeout != counter) && (bufferLen != numbytes)){
    avail = swSerial ? swSerial->available() : hwSerial->available();
    if (avail <= 0) {
      delay(1);
      counter++;
      continue;
    }
    counter = 0;
    // there's a byte!
    camerabuff[bufferLen++] = swSerial ? swSerial->read() : hwSerial->read();
  }
  return bufferLen;
}

boolean Adafruit_VC0706::verifyResponse(uint8_t command) {
  if ((camerabuff[0] != 0x76) ||
      (camerabuff[1] != serialNum) ||
      (camerabuff[2] != command) ||
      (camerabuff[3] != 0x0))
      return false;
  return true;
  
}

void Adafruit_VC0706::printBuff() {
  for (uint8_t i = 0; i< bufferLen; i++) {
    Serial.print(" 0x");
    Serial.print(camerabuff[i], HEX); 
  }
  Serial.println();
}
