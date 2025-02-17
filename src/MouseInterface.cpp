// Based on Apple in PC's mousecard.cpp
// - Permission given by Kyle Kim to reuse in AppleWin
/* Adaptation for SDL and POSIX (l) by beom beotiger, Nov-Dec 2007 */

#include "stdafx.h"
#include "MouseInterface.h"

// Sets mouse mode
#define MOUSE_SET    0x00
// Reads mouse position
#define MOUSE_READ    0x10
// Services mouse interrupt
#define MOUSE_SERV    0x20
// Clears mouse positions to 0 (for delta mode)
#define MOUSE_CLEAR    0x30
// Sets mouse position to a user-defined pos
#define MOUSE_POS    0x40
// Resets mouse clamps to default values
// Sets mouse position to 0,0
#define MOUSE_INIT    0x50
// Sets mouse bounds in a window
#define MOUSE_CLAMP    0x60
// Sets mouse to upper-left corner of clamp win
#define MOUSE_HOME    0x70

// Set VBL Timing : 0x90 is 60Hz, 0x91 is 50Hz
#define MOUSE_TIME    0x90

#define BIT0    0x01
#define BIT1    0x02
#define BIT2    0x04
#define BIT3    0x08
#define BIT4    0x10
#define BIT5    0x20
#define BIT6    0x40
#define BIT7    0x80


char MouseInterface_rom[] = "\x2C\x58\xFF\x70\x1B\x38\x90\x18\xB8\x50\x15\x01\x20\xF4\xF4\xF4"
                            "\xF4\x00\xB3\xC4\x9B\xA4\xC0\x8A\xDD\xBC\x48\xF0\x53\xE1\xE6\xEC"
                            "\x08\x78\x8D\xF8\x07\x48\x98\x48\x8A\x48\x20\x58\xFF\xBA\xBD\x00"
                            "\x01\xAA\x08\x0A\x0A\x0A\x0A\x28\xA8\xAD\xF8\x07\x8E\xF8\x07\x48"
                            "\xA9\x08\x70\x67\x90\x4D\xB0\x55\x29\x01\x09\xF0\x9D\x38\x06\xA9"
                            "\x02\xD0\x40\x29\x0F\x09\x90\xD0\x35\xFF\xFF\xB9\x83\xC0\x29\xFB"
                            "\x99\x83\xC0\xA9\x3E\x99\x82\xC0\xB9\x83\xC0\x09\x04\x99\x83\xC0"
                            "\xB9\x82\xC0\x29\xC1\x1D\xB8\x05\x99\x82\xC0\x68\xF0\x0A\x6A\x90"
                            "\x75\x68\xAA\x68\xA8\x68\x28\x60\x18\x60\x29\x01\x09\x60\x9D\x38"
                            "\x06\xA9\x0E\x9D\xB8\x05\xA9\x01\x48\xD0\xC0\xA9\x0C\x9D\xB8\x05"
                            "\xA9\x02\xD0\xF4\xA9\x30\x9D\x38\x06\xA9\x06\x9D\xB8\x05\xA9\x00"
                            "\x48\xF0\xA8\xC9\x10\xB0\xD2\x9D\x38\x07\x90\xEA\xA9\x04\xD0\xEB"
                            "\xA9\x40\xD0\xCA\xA4\x06\xA9\x60\x85\x06\x20\x06\x00\x84\x06\xBA"
                            "\xBD\x00\x01\xAA\x0A\x0A\x0A\x0A\xA8\xA9\x20\xD0\xC9\xA9\x70\xD0"
                            "\xC5\x48\xA9\xA0\xD0\xA8\x29\x0F\x09\xB0\xD0\xBA\xA9\xC0\xD0\xB6"
                            "\xA9\x02\xD0\xB7\xA2\x03\x38\x60\xFF\xFF\xFF\xD6\xFF\xFF\xFF\x01"
                            "\x98\x48\xA5\x06\x48\xA5\x07\x48\x86\x07\xA9\x27\x85\x06\x20\x58"
                            "\xFC\xA0\x00\xB1\x06\xF0\x06\x20\xED\xFD\xC8\xD0\xF6\x68\x85\x07"
                            "\x68\x85\x06\x68\xA8\xD0\x5B\xC1\xF0\xF0\xEC\xE5\xCD\xEF\xF5\xF3"
                            "\xE5\x8D\xC3\xEF\xF0\xF9\xF2\xE9\xE7\xE8\xF4\xA0\xB1\xB9\xB8\xB3"
                            "\xA0\xE2\xF9\xA0\xC1\xF0\xF0\xEC\xE5\xA0\xC3\xEF\xED\xF0\xF5\xF4"
                            "\xE5\xF2\xAC\xA0\xC9\xEE\xE3\xAE\x8D\x8D\xC2\xE1\xE3\xE8\xED\xE1"
                            "\xEE\xAF\xCD\xE1\xF2\xEB\xF3\xAF\xCD\xE1\xE3\xCB\xE1\xF9\x8D\x00"
                            "\xB9\x82\xC0\x29\xF1\x1D\xB8\x05\x99\x82\xC0\x68\x30\x0C\xF0\x80"
                            "\xD0\x09\xA9\x00\x9D\xB8\x05\x48\xF0\xE6\x60\xBD\x38\x07\x29\x0F"
                            "\x09\x20\x9D\x38\x07\x8A\x48\x48\x48\x48\xA9\xAA\x48\xBD\x38\x06"
                            "\x48\xA9\x0C\x9D\xB8\x05\xA9\x00\x48\xF0\xC5\xA9\xB3\x48\xAD\x78"
                            "\x04\x18\x90\xEC\xA9\xBC\x48\xAD\xF8\x04\x18\x90\xE3\xA9\x81\x48"
                            "\x7E\x38\x06\x90\x05\xAD\x78\x05\xB0\xD6\x8A\x48\xA9\xD8\x48\xA9"
                            "\x0C\x9D\xB8\x05\xA9\x01\x48\xD0\x97\xBD\x38\x06\x8D\x78\x05\x60"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xC2"
                            "\xBD\x38\x07\x29\x0F\x09\x40\x9D\x38\x07\x8A\x48\x48\x48\xA9\x11"
                            "\xD0\x27\xA9\x1E\x48\xA9\x0C\x9D\xB8\x05\xA9\x01\x48\xD0\x51\xAD"
                            "\xB3\xFB\xC9\x06\xD0\x21\xAD\x19\xC0\x30\xFB\xAD\x19\xC0\x10\xFB"
                            "\xAD\x19\xC0\x30\xFB\xA9\x7F\xD0\x00\x48\xA9\x50\x48\xA9\x0C\x9D"
                            "\xB8\x05\xA9\x00\x48\xF0\x29\xA5\x06\x48\xA5\x07\x48\x98\x48\xA9"
                            "\x20\x85\x07\xA0\x00\x84\x06\xA9\x00\x91\x06\xC8\xD0\xFB\xE6\x07"
                            "\xA5\x07\xC9\x40\xD0\xF1\x68\xA8\xA5\x08\x48\xA9\x00\xF0\x1C\xFF"
                            "\xB9\x82\xC0\x29\xF1\x1D\xB8\x05\x99\x82\xC0\x68\x30\x0A\xF0\x80"
                            "\xA9\x00\x9D\xB8\x05\x48\xF0\xE8\x60\xD0\xAE\xA9\x01\x8D\xD0\x3F"
                            "\x8D\xE0\x3F\xAD\x57\xC0\xAD\x54\xC0\xAD\x52\xC0\xAD\x50\xC0\xEA"
                            "\x85\x06\x85\x07\x85\x08\xE6\x06\xD0\x0E\xE6\x07\xD0\x0C\xE6\x08"
                            "\xA5\x08\xC9\x01\x90\x0A\xB0\x1F\x08\x28\x08\x28\xA9\x00\xA5\x00"
                            "\xAD\xFF\xCF\xB9\x82\xC0\x4A\xEA\xEA\xB0\xDB\xAD\xFF\xCF\xB9\x82"
                            "\xC0\x4A\xA5\x00\xEA\xB0\xCF\x68\x85\x08\x68\x85\x07\x68\x85\x06"
                            "\xA9\xE3\xD0\xA5\xAD\x51\xC0\xAD\x56\xC0\x18\x90\x93\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xC1"
                            "\xBD\x38\x06\xC9\x20\xD0\x06\xA9\x7F\x69\x01\x70\x01\xB8\xB9\x82"
                            "\xC0\x30\xFB\xB9\x81\xC0\x29\xFB\x99\x81\xC0\xA9\xFF\x99\x80\xC0"
                            "\xB9\x81\xC0\x09\x04\x99\x81\xC0\xBD\x38\x06\x99\x80\xC0\xB9\x82"
                            "\xC0\x09\x20\x99\x82\xC0\xB9\x82\xC0\x10\xFB\x29\xDF\x99\x82\xC0"
                            "\x70\x44\xBD\x38\x06\xC9\x30\xD0\x35\xA9\x00\x9D\xB8\x04\x9D\xB8"
                            "\x03\x9D\x38\x05\x9D\x38\x04\xF0\x25\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xB9\x82\xC0\x29\xF1\x1D\xB8\x05\x99\x82\xC0\x68\xF0\x82\xA9\x00"
                            "\x9D\xB8\x05\x48\xF0\xEA\xB9\x81\xC0\x29\xFB\x99\x81\xC0\xA9\x00"
                            "\x99\x80\xC0\xB9\x81\xC0\x09\x04\x99\x81\xC0\xB9\x82\xC0\x0A\x10"
                            "\xFA\xB9\x80\xC0\x9D\x38\x06\xB9\x82\xC0\x09\x10\x99\x82\xC0\xB9"
                            "\x82\xC0\x0A\x30\xFA\xB9\x82\xC0\x29\xEF\x99\x82\xC0\xBD\xB8\x06"
                            "\x29\xF1\x1D\x38\x06\x9D\xB8\x06\x29\x0E\xD0\xB2\xA9\x00\x9D\xB8"
                            "\x05\xA9\x02\x48\xD0\x9A\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xC3"
                            "\xE4\x37\xD0\x2D\xA9\x07\xC5\x36\xF0\x27\x85\x36\x68\xC9\x8D\xF0"
                            "\x74\x29\x01\x09\x80\x9D\x38\x07\x8A\x48\xA9\x84\x48\xBD\x38\x07"
                            "\x4A\xA9\x80\xB0\x01\x0A\x48\xA9\x0C\x9D\xB8\x05\xA9\x00\x48\xF0"
                            "\x3F\xE4\x39\xD0\xD7\xA9\x05\x85\x38\xBD\x38\x07\x29\x01\xD0\x14"
                            "\x68\x68\x68\x68\xA9\x00\x9D\xB8\x03\x9D\xB8\x04\x9D\x38\x04\x9D"
                            "\x38\x05\xF0\x3C\xBD\x38\x07\x29\x01\x09\x80\x9D\x38\x07\x8A\x48"
                            "\xA9\xA1\x48\xA9\x10\x48\xA9\x0C\xD0\x30\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xB9\x82\xC0\x29\xF1\x1D\xB8\x05\x99\x82\xC0\x68\x30\x11\xF0\x80"
                            "\x6A\xB0\x89\x90\xB4\xA9\x00\x9D\xB8\x05\xA9\x01\x48\xD0\xE1\x60"
                            "\xA9\xC0\x9D\xB8\x06\x8C\x22\x02\xA9\x0A\x9D\xB8\x05\xA9\x00\x48"
                            "\xF0\xCE\x68\x68\x68\x68\xA9\x05\x9D\x38\x06\xB9\x81\xC0\x29\xFB"
                            "\x99\x81\xC0\xA9\x00\x99\x80\xC0\xB9\x81\xC0\x09\x04\x99\x81\xC0"
                            "\xB9\x82\xC0\x0A\x10\xFA\xB9\x80\xC0\x48\xB9\x82\xC0\x09\x10\x99"
                            "\x82\xC0\xB9\x82\xC0\x0A\x30\xFA\xB9\x82\xC0\x29\xEF\x99\x82\xC0"
                            "\xDE\x38\x06\xD0\xDB\x68\x9D\xB8\x06\x68\x9D\x38\x05\x68\x9D\x38"
                            "\x04\x68\x9D\xB8\x04\x68\x9D\xB8\x03\x18\x90\x99\xFF\xFF\xFF\xC8"
                            "\x8A\x48\x48\x48\xA9\x12\x48\xBC\xB8\x03\xBD\xB8\x04\xAA\x98\xA0"
                            "\x05\xD0\x6D\xAE\xF8\x07\xA9\x24\x48\xBC\x38\x04\xBD\x38\x05\xAA"
                            "\x98\xA0\x0C\xD0\x5B\xAE\xF8\x07\xA9\x43\x48\xAD\x00\xC0\x0A\x08"
                            "\xBD\xB8\x06\x2A\x2A\x2A\x29\x03\x49\x03\x38\x69\x00\x28\xA2\x00"
                            "\xA0\x10\xD0\x4D\xA9\x8D\x8D\x11\x02\x48\xA9\x11\x48\x48\xA9\x00"
                            "\xF0\x12\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xAE\xF8\x07\xAC\x22\x02\x9D\xB8\x05\xA9\x01\x48"
                            "\xB9\x82\xC0\x29\xF1\x1D\xB8\x05\x99\x82\xC0\x68\x30\x4E\xF0\x80"
                            "\xE0\x80\x90\x0D\x49\xFF\x69\x00\x48\x8A\x49\xFF\x69\x00\xAA\x68"
                            "\x38\x8D\x21\x02\x8E\x20\x02\xA9\xAB\x90\x02\xA9\xAD\x48\xA9\xAC"
                            "\x99\x01\x02\xA2\x11\xA9\x00\x18\x2A\xC9\x0A\x90\x02\xE9\x0A\x2E"
                            "\x21\x02\x2E\x20\x02\xCA\xD0\xF0\x09\xB0\x99\x00\x02\x88\xF0\x08"
                            "\xC0\x07\xF0\x04\xC0\x0E\xD0\xDB\x68\x99\x00\x02\x60\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xCD"
                            "\xB8\x50\x13\xBD\x38\x07\x29\x01\xF0\x47\xA9\x10\x48\xA9\x05\x9D"
                            "\x38\x06\xA9\x7F\x69\x01\xB9\x82\xC0\x30\xFB\xB9\x81\xC0\x29\xFB"
                            "\x99\x81\xC0\xA9\xFF\x99\x80\xC0\xB9\x81\xC0\x09\x04\x99\x81\xC0"
                            "\x68\x99\x80\xC0\xB9\x82\xC0\x09\x20\x99\x82\xC0\xB9\x82\xC0\x10"
                            "\xFB\x29\xDF\x99\x82\xC0\x70\x3F\x70\x07\xBD\x38\x07\x4A\x4A\x4A"
                            "\x4A\xB8\x9D\xB8\x05\xF0\x02\xA9\x80\x48\x50\x14\xFF\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xB9\x82\xC0\x29\xF1\x1D\xB8\x05\x99\x82\xC0\x68\xF0\x82\xC9\x02"
                            "\xF0\x81\xD0\x02\xF0\xC2\xB8\xB9\x81\xC0\x29\xFB\x99\x81\xC0\xA9"
                            "\x00\x99\x80\xC0\xB9\x81\xC0\x09\x04\x99\x81\xC0\xB9\x82\xC0\x0A"
                            "\x10\xFA\xB9\x80\xC0\x70\x05\x9D\x38\x06\x50\x01\x48\xB9\x82\xC0"
                            "\x09\x10\x99\x82\xC0\xB9\x82\xC0\x0A\x30\xFA\xB9\x82\xC0\x29\xEF"
                            "\x99\x82\xC0\x50\x19\xDE\x38\x06\xD0\xD2\x68\x9D\xB8\x06\x68\x9D"
                            "\x38\x05\x68\x9D\x38\x04\x68\x9D\xB8\x04\x68\x9D\xB8\x03\xA9\x00"
                            "\xF0\xA2\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xC1"
                            "\xBD\x38\x06\xC9\x40\xF0\x22\xC9\x60\xF0\x0D\xC9\x61\xF0\x09\xC9"
                            "\xA0\xD0\x2E\x48\xA9\x02\xD0\x45\xAD\xF8\x05\x48\xAD\x78\x05\x48"
                            "\xAD\xF8\x04\x48\xAD\x78\x04\xB0\x0F\xBD\x38\x05\x48\xBD\x38\x04"
                            "\x48\xBD\xB8\x04\x48\xBD\xB8\x03\x48\xBD\x38\x06\x48\xA9\x05\xD0"
                            "\x1C\x29\x0C\x4A\x4A\x4A\xB0\x3E\x4A\x90\x0C\xAD\x78\x05\x48\xBD"
                            "\x38\x06\x48\xA9\x02\xD0\x06\xBD\x38\x06\x48\xA9\x01\x9D\x38\x06"
                            "\xD0\x4F\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                            "\xB9\x82\xC0\x29\xF1\x1D\xB8\x05\x99\x82\xC0\x68\xD0\x82\xA9\x00"
                            "\x9D\xB8\x05\x48\xF0\xEA\x4A\xB0\x13\xAD\xF8\x04\x48\xAD\x78\x04"
                            "\x48\xBD\x38\x06\x48\xA9\x03\x9D\x38\x06\xD0\x15\xAD\x78\x05\x48"
                            "\xAD\xF8\x04\x48\xAD\x78\x04\x48\xBD\x38\x06\x48\xA9\x04\x9D\x38"
                            "\x06\xB9\x82\xC0\x30\xFB\xB9\x81\xC0\x29\xFB\x99\x81\xC0\xA9\xFF"
                            "\x99\x80\xC0\xB9\x81\xC0\x09\x04\x99\x81\xC0\x68\x99\x80\xC0\xB9"
                            "\x82\xC0\x09\x20\x99\x82\xC0\xB9\x82\xC0\x10\xFB\x29\xDF\x99\x82"
                            "\xC0\xDE\x38\x06\xF0\x98\xB9\x82\xC0\x30\xFB\x10\xD6\xFF\xFF\xFF"
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xCE";


WRITE_HANDLER( M6821_Listener_B ) {
  ((CMouseInterface *) objTo)->On6821_B(byData);
}

WRITE_HANDLER( M6821_Listener_A ) {
  ((CMouseInterface *) objTo)->On6821_A(byData);
}

CMouseInterface::CMouseInterface() : m_pSlotRom(NULL) {
  m_6821.SetListenerB(this, M6821_Listener_B);
  m_6821.SetListenerA(this, M6821_Listener_A);
  m_by6821A = 0;
  m_by6821B = 0x40;    // Set PB6
  m_6821.SetPB(m_by6821B);
  m_bVBL = false;

  m_iX = 0;
  m_iMinX = 0;
  m_iMaxX = 1023;
  m_iRangeX = 0;

  m_iY = 0;
  m_iMinY = 0;
  m_iMaxY = 1023;
  m_iRangeY = 0;

  m_bButtons[0] = m_bButtons[1] = false;

  Reset();
  memset(m_byBuff, 0, sizeof(m_byBuff));
  m_bActive = false;
}

CMouseInterface::~CMouseInterface() {
  delete[] m_pSlotRom;
}

void CMouseInterface::Initialize(LPBYTE pCxRomPeripheral, unsigned int uSlot) {
  const unsigned int FW_SIZE = 2 * 1024;
  unsigned char *pData = (unsigned char *) MouseInterface_rom;  // NB. Don't need to unlock resource

  m_uSlot = uSlot;

  if (m_pSlotRom == NULL) {
    m_pSlotRom = new unsigned char[FW_SIZE];
    if (m_pSlotRom != nullptr) {
      memcpy(m_pSlotRom, pData, FW_SIZE);
    }
  }

  SetSlotRom();
  RegisterIoHandler(uSlot, &CMouseInterface::IORead, &CMouseInterface::IOWrite, NULL, NULL, this, NULL);
  m_bActive = true;
  printf("MouseInterface Rom loaded and registered\n");
}

void CMouseInterface::SetSlotRom() {
  LPBYTE pCxRomPeripheral = MemGetCxRomPeripheral();
  if (pCxRomPeripheral == NULL) {
    return;
  }

  unsigned int uOffset = (m_by6821B << 7) & 0x0700;
  memcpy(pCxRomPeripheral + m_uSlot * 256, m_pSlotRom + uOffset, 256);
  if (mem != nullptr) {
    memcpy(mem + 0xC000 + m_uSlot * 256, m_pSlotRom + uOffset, 256);
  }
}

unsigned char CMouseInterface::IORead(unsigned short PC, unsigned short uAddr, unsigned char bWrite, unsigned char uValue, ULONG nCyclesLeft) {
  unsigned int uSlot = ((uAddr & 0xff) >> 4) - 8;
  CMouseInterface *pMouseIF = (CMouseInterface *) MemGetSlotParameters(uSlot);

  unsigned char byRS;
  byRS = uAddr & 3;
  return pMouseIF->m_6821.Read(byRS);
}

unsigned char CMouseInterface::IOWrite(unsigned short PC, unsigned short uAddr, unsigned char bWrite, unsigned char uValue, ULONG nCyclesLeft) {
  unsigned int uSlot = ((uAddr & 0xff) >> 4) - 8;
  CMouseInterface *pMouseIF = (CMouseInterface *) MemGetSlotParameters(uSlot);

  unsigned char byRS;
  byRS = uAddr & 3;
  pMouseIF->m_6821.Write(byRS, uValue);

  return 0;
}

void CMouseInterface::On6821_A(unsigned char byData) {
  m_by6821A = byData;
}

void CMouseInterface::On6821_B(unsigned char byData) {
  unsigned char byDiff = (m_by6821B ^ byData) & 0x3E;

  if (byDiff != 0u) {
    m_by6821B &= ~0x3E;
    m_by6821B |= byData & 0x3E;
    if ((byDiff & BIT5) != 0)      // Write to 0285 chip
    {
      if ((byData & BIT5) != 0) {
        m_by6821B |= BIT7;    // OK, I'm ready to read from MC6821
      }
      else {// Clock Activate for read
        m_byBuff[m_nBuffPos++] = m_by6821A;
        if (m_nBuffPos == 1) {
          OnCommand();
        }
        if (m_nBuffPos == m_nDataLen || m_nBuffPos > 7) {
          OnWrite();      // Have written all, Commit the command.
          m_nBuffPos = 0;
        }
        m_by6821B &= ~BIT7;    // for next reading
        m_6821.SetPB(m_by6821B);
      }
    }
    if ((byDiff & BIT4) != 0) {// Read from 0285 chip ?
      if ((byData & BIT4) != 0) {
        m_by6821B &= ~BIT6;    // OK, I'll prepare next value
      } else { // Clock Activate for write
        if (m_nBuffPos != 0) { // if m_nBuffPos is 0, something goes wrong!
          m_nBuffPos++;
        }
        if (m_nBuffPos == m_nDataLen || m_nBuffPos > 7) {
          m_nBuffPos = 0; // Have read all, ready for next command.
        } else {
          m_6821.SetPA(m_byBuff[m_nBuffPos]);
        }
        m_by6821B |= BIT6;    // for next writing
      }
    }
    m_6821.SetPB(m_by6821B);

    SetSlotRom();  // Update Cn00 ROM page
  }
}


void CMouseInterface::OnCommand() {
  switch (m_byBuff[0] & 0xF0) {
    case MOUSE_SET:
      m_nDataLen = 1;
      m_byMode = m_byBuff[0] & 0x0F;
      break;
    case MOUSE_READ:
      m_nDataLen = 6;
      m_byState &= 0x20;
      m_nX = m_iX;
      m_nY = m_iY;
      if (m_bBtn0) {
        m_byState |= 0x40;      // Previous Button 0
      }
      if (m_bBtn1) {
        m_byState |= 0x01;      // Previous Button 1
      }
      m_bBtn0 = m_bButtons[0];
      m_bBtn1 = m_bButtons[1];
      if (m_bBtn0) {
        m_byState |= 0x80;      // Current Button 0
      }
      if (m_bBtn1) {
        m_byState |= 0x10;      // Current Button 1
      }
      m_byBuff[1] = m_nX & 0xFF;
      m_byBuff[2] = (m_nX >> 8) & 0xFF;
      m_byBuff[3] = m_nY & 0xFF;
      m_byBuff[4] = (m_nY >> 8) & 0xFF;
      m_byBuff[5] = m_byState;      // button 0/1 interrupt status
      m_byState &= ~0x20;
      break;
    case MOUSE_SERV:
      m_nDataLen = 2;
      m_byBuff[1] = m_byState & ~0x20;      // reason of interrupt
      CpuIrqDeassert(IS_MOUSE);
      break;
    case MOUSE_CLEAR:
      Reset();
      m_nDataLen = 1;
      break;
    case MOUSE_POS:
      m_nDataLen = 5;
      break;
    case MOUSE_INIT:
      m_nDataLen = 3;
      m_byBuff[1] = 0xFF;      // I don't know what it is
      break;
    case MOUSE_CLAMP:
      m_nDataLen = 5;
      break;
    case MOUSE_HOME:
      m_nDataLen = 1;
      SetPosition(0, 0);
      break;
    case MOUSE_TIME:    // 0x90
      switch (m_byBuff[0] & 0x0C) {
        case 0x00:
          m_nDataLen = 1;
          break;  // write cmd ( #$90 is DATATIME 60Hz, #$91 is 50Hz )
        case 0x04:
          m_nDataLen = 3;
          break;  // write cmd, $0478, $04F8
        case 0x08:
          m_nDataLen = 2;
          break;  // write cmd, $0578
        case 0x0C:
          m_nDataLen = 4;
          break;  // write cmd, $0478, $04F8, $0578
      }
      break;
    case 0xA0:
      m_nDataLen = 2;
      break;
    case 0xB0:
    case 0xC0:
      m_nDataLen = 1;
      break;
    default:
      m_nDataLen = 1;
      break;
  }
  m_6821.SetPA(m_byBuff[1]);
}

void CMouseInterface::OnWrite() {
  int nMin, nMax;
  switch (m_byBuff[0] & 0xF0) {
    case MOUSE_CLAMP:
      nMin = (m_byBuff[3] << 8) | m_byBuff[1];
      nMax = (m_byBuff[4] << 8) | m_byBuff[2];
      if ((m_byBuff[0] & 1) != 0)  // Clamp Y
        ClampY(nMin, nMax);
      else          // Clamp X
        ClampX(nMin, nMax);
      break;
    case MOUSE_POS:
      m_nX = (m_byBuff[2] << 8) | m_byBuff[1];
      m_nY = (m_byBuff[4] << 8) | m_byBuff[3];
      SetPosition(m_nX, m_nY);
      break;
    case MOUSE_INIT:
      m_nX = 0;
      m_nY = 0;
      ClampX(0, 1023);
      ClampY(0, 1023);
      SetPosition(0, 0);
      break;
  }
}

void CMouseInterface::OnMouseEvent() {
  int byState = 0;
  if ((m_byMode & 1) == 0) { // Mouse Off
    return;
  }

  bool bBtn0 = m_bButtons[0];
  bool bBtn1 = m_bButtons[1];
  if ((unsigned int) m_nX != m_iX || (unsigned int) m_nY != m_iY) {
    byState |= 0x22;        // X/Y moved since last READMOUSE | Movement interrupt
  }
  if (m_bBtn0 != bBtn0 || m_bBtn1 != bBtn1) {
    byState |= 0x04;        // Button 0/1 interrupt
  }
  if (m_bVBL) {
    byState |= 0x08;
  }
  byState &= ((m_byMode & 0x0E) |
              0x20);  // Keep "X/Y moved since last READMOUSE" for next MOUSE_READ (Contiki v1.3 uses this)
  if ((byState & 0x0E) != 0) {
    m_byState |= byState;
    CpuIrqAssert(IS_MOUSE);
  }
}

void CMouseInterface::SetVBlank(bool bVBL) {
  if (m_bVBL != bVBL) {
    m_bVBL = bVBL;
    if (m_bVBL) { // Rising edge
      OnMouseEvent();
    }
  }
}

void CMouseInterface::Reset() {
  m_nBuffPos = 0;
  m_nDataLen = 1;

  m_byMode = 0;
  m_byState = 0;
  m_nX = 0;
  m_nY = 0;
  m_bBtn0 = false;
  m_bBtn1 = false;
  ClampX(0, 1023);
  ClampY(0, 1023);
  SetPosition(0, 0);
}

void CMouseInterface::ClampX(int iMinX, int iMaxX) {
  if (iMinX < 0 || iMinX > iMaxX) {
    return;
  }
  m_iMaxX = iMaxX;
  m_iMinX = iMinX;
  if (m_iX > m_iMaxX) {
    m_iX = m_iMaxX;
  } else if (m_iX < m_iMinX) {
    m_iX = m_iMinX;
  }
}

void CMouseInterface::ClampY(int iMinY, int iMaxY) {
  if (iMinY < 0 || iMinY > iMaxY) {
    return;
  }
  m_iMaxY = iMaxY;
  m_iMinY = iMinY;
  if (m_iY > m_iMaxY) {
    m_iY = m_iMaxY;
  } else if (m_iY < m_iMinX) {
    m_iY = m_iMinY;
  }
}

void CMouseInterface::SetPosition(int xvalue, int yvalue) {
  if ((m_iRangeX == 0) || (m_iRangeY == 0)) {
    m_nX = m_iX = m_iMinX;
    m_nY = m_iY = m_iMinY;
    return;
  }

  m_iX = (unsigned int)((xvalue * m_iMaxX) / m_iRangeX);
  m_iY = (unsigned int)((yvalue * m_iMaxY) / m_iRangeY);
}

void CMouseInterface::SetPosition(int xvalue, int xrange, int yvalue, int yrange) {
  m_iRangeX = (unsigned int) xrange;
  m_iRangeY = (unsigned int) yrange;

  SetPosition(xvalue, yvalue);
  OnMouseEvent();
}

void CMouseInterface::SetButton(eBUTTON Button, eBUTTONSTATE State) {
  m_bButtons[Button] = State == BUTTON_DOWN;
  OnMouseEvent();
}
