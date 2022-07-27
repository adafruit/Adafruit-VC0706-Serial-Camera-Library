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
#if defined(__AVR__) || defined(ESP8266)
  swSerial = NULL;
#endif
  hwSerial = NULL;
  frameptr = 0;
  bufferLen = 0;
  serialNum = 0;
}

#if defined(__AVR__) || defined(ESP8266)
/**************************************************************************/
/*!
    @brief Constructor when using SoftwareSerial
    @param ser Software serial connection
*/
/**************************************************************************/
Adafruit_VC0706::Adafruit_VC0706(SoftwareSerial *ser) {
  common_init();  // Set everything to common state, then...
  swSerial = ser; // ...override swSerial with value passed.
}
#endif

/**************************************************************************/
/*!
    @brief Constructor when using HardwareSerial
    @param ser Hardware serial connection
*/
/**************************************************************************/
Adafruit_VC0706::Adafruit_VC0706(HardwareSerial *ser) {
  common_init();  // Set everything to common state, then...
  hwSerial = ser; // ...override hwSerial with value passed.
}

/**************************************************************************/
/*!
    @brief Connect to and reset the camera
    @param baud Camera interface baud rate
    @return True on reset success
*/
/**************************************************************************/
boolean Adafruit_VC0706::begin(uint32_t baud) {
#if defined(__AVR__) || defined(ESP8266)
  if (swSerial)
    swSerial->begin(baud);
  else
#endif
    hwSerial->begin(baud);
  return reset();
}

/**************************************************************************/
/*!
    @brief  Soft reset the camera
    @return True on success
*/
/**************************************************************************/
boolean Adafruit_VC0706::reset() {
  uint8_t args[] = {0x0};

  return runCommand(VC0706_RESET, args, 1, 5);
}

/**************************************************************************/
/*!
    @brief  Check if motion is detected
    @return True on motion detected
*/
/**************************************************************************/
boolean Adafruit_VC0706::motionDetected() {
  if (readResponse(4, 200) != 4) {
    return false;
  }
  if (!verifyResponse(VC0706_COMM_MOTION_DETECTED))
    return false;

  return true;
}

/**************************************************************************/
/*!
    @brief  Set motion detection
    @param x See datasheet for VC0706_MOTION_CTRL command details
    @param d1 See datasheet for VC0706_MOTION_CTRL command details
    @param d2 See datasheet for VC0706_MOTION_CTRL command details
    @return True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::setMotionStatus(uint8_t x, uint8_t d1, uint8_t d2) {
  uint8_t args[] = {0x03, x, d1, d2};

  return runCommand(VC0706_MOTION_CTRL, args, sizeof(args), 5);
}

/**************************************************************************/
/*!
    @brief  Get motion status
    @param x See datasheet for VC0706_MOTION_STATUS command details
    @return True on command success
*/
/**************************************************************************/
uint8_t Adafruit_VC0706::getMotionStatus(uint8_t x) {
  uint8_t args[] = {0x01, x};

  return runCommand(VC0706_MOTION_STATUS, args, sizeof(args), 5);
}

/**************************************************************************/
/*!
    @brief Set motion detection
    @param flag Whether to activate motion detection
    @return True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::setMotionDetect(boolean flag) {
  if (!setMotionStatus(VC0706_MOTIONCONTROL, VC0706_UARTMOTION,
                       VC0706_ACTIVATEMOTION))
    return false;

  uint8_t args[] = {0x01, flag};

  return runCommand(VC0706_COMM_MOTION_CTRL, args, sizeof(args), 5);
}

/**************************************************************************/
/*!
    @brief  Get motion detection status
    @return True if motion detection is on!
*/
/**************************************************************************/
boolean Adafruit_VC0706::getMotionDetect(void) {
  uint8_t args[] = {0x0};

  if (!runCommand(VC0706_COMM_MOTION_STATUS, args, 1, 6))
    return false;

  return camerabuff[5];
}

/**************************************************************************/
/*!
    @brief  Get image size with VC0706_READ_DATA
    @return VC0706_640x480, VC0706_320x240, VC0706_160x120, etc.
*/
/**************************************************************************/
uint8_t Adafruit_VC0706::getImageSize() {
  uint8_t args[] = {0x4, 0x4, 0x1, 0x00, 0x19};
  if (!runCommand(VC0706_READ_DATA, args, sizeof(args), 6))
    return -1;

  return camerabuff[5];
}

/**************************************************************************/
/*!
    @brief  Set image size with VC0706_WRITE_DATA
    @param x VC0706_640x480, VC0706_320x240, VC0706_160x120, etc.
    @return True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::setImageSize(uint8_t x) {
  uint8_t args[] = {0x05, 0x04, 0x01, 0x00, 0x19, x};

  if (x < VC0706_1024x768)
    // standard image resolution
    args[1] = 0x04;
  else
    // extended image resolution
    args[1] = 0x05;

  return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

/****************** downsize image control */

/**************************************************************************/
/*!
    @brief  Get downsize Status
    @return Camera reply byte
*/
/**************************************************************************/
uint8_t Adafruit_VC0706::getDownsize(void) {
  uint8_t args[] = {0x0};
  if (!runCommand(VC0706_DOWNSIZE_STATUS, args, 1, 6))
    return -1;

  return camerabuff[5];
}

/**************************************************************************/
/*!
    @brief  Set downsize size
    @param newsize See datasheet for VC0706_DOWNSIZE_CTRL parameters
    @return True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::setDownsize(uint8_t newsize) {
  uint8_t args[] = {0x01, newsize};

  return runCommand(VC0706_DOWNSIZE_CTRL, args, 2, 5);
}

/***************** other high level commands */

/**************************************************************************/
/*!
    @brief Get firmware version
    @returns Pointer to buffer containing camera response
*/
/**************************************************************************/
char *Adafruit_VC0706::getVersion(void) {
  uint8_t args[] = {0x01};

  sendCommand(VC0706_GEN_VERSION, args, 1);
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200))
    return 0;
  camerabuff[bufferLen] = 0; // end it!
  return (char *)camerabuff; // return it!
}

/**************************************************************************/
/*!
    @brief  Set the baud rate to 9600
    @return The response as a character buffer
*/
/**************************************************************************/
char *Adafruit_VC0706::setBaud9600() {
  uint8_t args[] = {0x03, 0x01, 0xAE, 0xC8};

  sendCommand(VC0706_SET_PORT, args, sizeof(args));
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200))
    return 0;
  camerabuff[bufferLen] = 0; // end it!
  return (char *)camerabuff; // return it!
}

/**************************************************************************/
/*!
    @brief  Set the baud rate to 19200
    @return The response as a character buffer
*/
/**************************************************************************/
char *Adafruit_VC0706::setBaud19200() {
  uint8_t args[] = {0x03, 0x01, 0x56, 0xE4};

  sendCommand(VC0706_SET_PORT, args, sizeof(args));
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200))
    return 0;
  camerabuff[bufferLen] = 0; // end it!
  return (char *)camerabuff; // return it!
}

/**************************************************************************/
/*!
    @brief  Set the baud rate to 38400
    @return The response as a character buffer
*/
/**************************************************************************/
char *Adafruit_VC0706::setBaud38400() {
  uint8_t args[] = {0x03, 0x01, 0x2A, 0xF2};

  sendCommand(VC0706_SET_PORT, args, sizeof(args));
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200))
    return 0;
  camerabuff[bufferLen] = 0; // end it!
  return (char *)camerabuff; // return it!
}

/**************************************************************************/
/*!
    @brief  Set the baud rate to 57600
    @return The response as a character buffer
*/
/**************************************************************************/
char *Adafruit_VC0706::setBaud57600() {
  uint8_t args[] = {0x03, 0x01, 0x1C, 0x1C};

  sendCommand(VC0706_SET_PORT, args, sizeof(args));
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200))
    return 0;
  camerabuff[bufferLen] = 0; // end it!
  return (char *)camerabuff; // return it!
}

/**************************************************************************/
/*!
    @brief  Set the baud rate to 115200
    @return The response as a character buffer
*/
/**************************************************************************/
char *Adafruit_VC0706::setBaud115200() {
  uint8_t args[] = {0x03, 0x01, 0x0D, 0xA6};

  sendCommand(VC0706_SET_PORT, args, sizeof(args));
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200))
    return 0;
  camerabuff[bufferLen] = 0; // end it!
  return (char *)camerabuff; // return it!
}

/**************************************************************************/
/*!
    @brief  Add a character to on screen display
    @param x X offset
    @param y Y offset
    @param str The string to display
*/
/**************************************************************************/
void Adafruit_VC0706::OSD(uint8_t x, uint8_t y, char *str) {
  if (strlen(str) > 14) {
    str[13] = 0;
  }

  uint8_t args[17] = {(uint8_t)strlen(str), (uint8_t)(strlen(str) - 1),
                      (uint8_t)((y & 0xF) | ((x & 0x3) << 4))};

  for (uint8_t i = 0; i < strlen(str); i++) {
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

    args[3 + i] = str[i];
  }

  runCommand(VC0706_OSD_ADD_CHAR, args, strlen(str) + 3, 5);
  printBuff();
}

/**************************************************************************/
/*!
    @brief Set compression rate
    @param c See datasheet for compression settings
    @returns True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::setCompression(uint8_t c) {
  uint8_t args[] = {0x5, 0x1, 0x1, 0x12, 0x04, c};
  return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

/**************************************************************************/
/*!
    @brief Get compression rate
    @returns The character reply
*/
/**************************************************************************/
uint8_t Adafruit_VC0706::getCompression(void) {
  uint8_t args[] = {0x4, 0x1, 0x1, 0x12, 0x04};
  runCommand(VC0706_READ_DATA, args, sizeof(args), 6);
  printBuff();
  return camerabuff[5];
}

/**************************************************************************/
/*!
    @brief Set PTZ (orientation), see datasheet for SET_ZOOM command
    @param wz Rotation
    @param hz Horizontal
    @param pan Pan
    @param tilt Tilt

    @returns True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::setPTZ(uint16_t wz, uint16_t hz, uint16_t pan,
                                uint16_t tilt) {
  uint8_t args[] = {
      0x08,         (uint8_t)(wz >> 8),  (uint8_t)wz,  (uint8_t)(hz >> 8),
      (uint8_t)hz,  (uint8_t)(pan >> 8), (uint8_t)pan, (uint8_t)(tilt >> 8),
      (uint8_t)tilt};

  return (!runCommand(VC0706_SET_ZOOM, args, sizeof(args), 5));
}

/**************************************************************************/
/*!
    @brief Get PTZ (orientation), see datasheet for GET_ZOOM command
    @param w Pointer to variable to store width
    @param h Pointer to variable to store height
    @param wz Pointer to variable to store rotation
    @param hz Pointer to variable to store horizontal
    @param pan Pointer to variable to store pan
    @param tilt Pointer to variable to store tilt
    @returns True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::getPTZ(uint16_t &w, uint16_t &h, uint16_t &wz,
                                uint16_t &hz, uint16_t &pan, uint16_t &tilt) {
  uint8_t args[] = {0x0};

  if (!runCommand(VC0706_GET_ZOOM, args, sizeof(args), 16))
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

/**************************************************************************/
/*!
    @brief Send STOPCURRENTFRAME command
    @returns True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::takePicture() {
  frameptr = 0;
  return cameraFrameBuffCtrl(VC0706_STOPCURRENTFRAME);
}

/**************************************************************************/
/*!
    @brief Send RESUMEFRAME command
    @returns True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::resumeVideo() {
  return cameraFrameBuffCtrl(VC0706_RESUMEFRAME);
}

/**************************************************************************/
/*!
    @brief TV output ON
    @returns True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::TVon() {
  uint8_t args[] = {0x1, 0x1};
  return runCommand(VC0706_TVOUT_CTRL, args, sizeof(args), 5);
}

/**************************************************************************/
/*!
    @brief TV output OFF
    @returns True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::TVoff() {
  uint8_t args[] = {0x1, 0x0};
  return runCommand(VC0706_TVOUT_CTRL, args, sizeof(args), 5);
}

/**************************************************************************/
/*!
    @brief Send FBUF_CTRL with command
    @param command The FBUF control command (see datasheet)
    @returns True on command success
*/
/**************************************************************************/
boolean Adafruit_VC0706::cameraFrameBuffCtrl(uint8_t command) {
  uint8_t args[] = {0x1, command};
  return runCommand(VC0706_FBUF_CTRL, args, sizeof(args), 5);
}

/**************************************************************************/
/*!
    @brief Get frame buffer length
    @returns Length reply
*/
/**************************************************************************/
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

/**************************************************************************/
/*!
    @brief Get available bytes to read
    @returns Internal buffer length
*/
/**************************************************************************/
uint8_t Adafruit_VC0706::available(void) { return bufferLen; }

/**************************************************************************/
/*!
    @brief Read in picture data
    @param n Number of bytes
    @returns Pointer to buffer containing n bytes of picture data
*/
/**************************************************************************/
uint8_t *Adafruit_VC0706::readPicture(uint8_t n) {
  uint8_t args[] = {0x0C,
                    0x0,
                    0x0A,
                    (uint8_t)((frameptr >> 24) & 0xFF),
                    (uint8_t)((frameptr >> 16) & 0xFF),
                    (uint8_t)((frameptr >> 8) & 0xFF),
                    (uint8_t)(frameptr & 0xFF),
                    0,
                    0,
                    0,
                    n,
                    CAMERADELAY >> 8,
                    CAMERADELAY & 0xFF};

  if (!runCommand(VC0706_READ_FBUF, args, sizeof(args), 5, false))
    return 0;

  // read into the buffer PACKETLEN!
  if (readResponse(n + 5, CAMERADELAY) == 0)
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
  if (!verifyResponse(cmd))
    return false;
  return true;
}

void Adafruit_VC0706::sendCommand(uint8_t cmd, uint8_t args[] = 0,
                                  uint8_t argn = 0) {
#if defined(__AVR__) || defined(ESP8266)
  if (swSerial) {

    swSerial->write((byte)0x56);
    swSerial->write((byte)serialNum);
    swSerial->write((byte)cmd);

    for (uint8_t i = 0; i < argn; i++) {
      swSerial->write((byte)args[i]);
      // Serial.print(" 0x");
      // Serial.print(args[i], HEX);
    }
  } else
#endif
  {
    hwSerial->write((byte)0x56);
    hwSerial->write((byte)serialNum);
    hwSerial->write((byte)cmd);

    for (uint8_t i = 0; i < argn; i++) {
      hwSerial->write((byte)args[i]);
      // Serial.print(" 0x");
      // Serial.print(args[i], HEX);
    }
  }
  // Serial.println();
}

uint8_t Adafruit_VC0706::readResponse(uint8_t numbytes, uint8_t timeout) {
  uint8_t counter = 0;
  bufferLen = 0;
  int avail;

  while ((timeout != counter) && (bufferLen != numbytes)) {
#if defined(__AVR__) || defined(ESP8266)
    avail = swSerial ? swSerial->available() : hwSerial->available();
#else
    avail = hwSerial->available();
#endif
    if (avail <= 0) {
      delay(1);
      counter++;
      continue;
    }
    counter = 0;
    // there's a byte!
#if defined(__AVR__) || defined(ESP8266)
    camerabuff[bufferLen++] = swSerial ? swSerial->read() : hwSerial->read();
#else
    camerabuff[bufferLen++] = hwSerial->read();
#endif
  }
  // printBuff();
  // camerabuff[bufferLen] = 0;
  // Serial.println((char*)camerabuff);
  return bufferLen;
}

boolean Adafruit_VC0706::verifyResponse(uint8_t command) {
  if ((camerabuff[0] != 0x76) || (camerabuff[1] != serialNum) ||
      (camerabuff[2] != command) || (camerabuff[3] != 0x0))
    return false;
  return true;
}

void Adafruit_VC0706::printBuff() {
  for (uint8_t i = 0; i < bufferLen; i++) {
    Serial.print(" 0x");
    Serial.print(camerabuff[i], HEX);
  }
  Serial.println();
}
