/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Disk Image
 *
 * Author: Various
 */

/* Adaptation for SDL and POSIX (l) by beom beotiger, Nov-Dec 2007 */

#include <assert.h>
#include "stdafx.h"
#include "wwrapper.h"

/* DO logical order  0 1 2 3 4 5 6 7 8 9 A B C D E F */
/*    physical order 0 D B 9 7 5 3 1 E C A 8 6 4 2 F */

/* PO logical order  0 E D C B A 9 8 7 6 5 4 3 2 1 F */
/*    physical order 0 2 4 6 8 A C E 1 3 5 7 9 B D F */

typedef struct _imageinforec {
  char filename[MAX_PATH];
  unsigned int format;
  HANDLE file;
  unsigned int offset;
  bool writeProtected;
  unsigned int headerSize;
  LPBYTE header;
  bool validTrack[TRACKS];
} imageinforec, *imageinfoptr;

typedef bool (*boottype  )(imageinfoptr);

typedef unsigned int(*detecttype)(LPBYTE, unsigned int);

typedef void (*readtype  )(imageinfoptr, int, int, LPBYTE, int *);

typedef void (*writetype )(imageinfoptr, int, int, LPBYTE, int);

bool AplBoot(imageinfoptr ptr);

unsigned int AplDetect(LPBYTE imageptr, unsigned int imagesize);

unsigned int DoDetect(LPBYTE imageptr, unsigned int imagesize);

void DoRead(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles);

void DoWrite(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles);

unsigned int IieDetect(LPBYTE imageptr, unsigned int imagesize);

void IieRead(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles);

void IieWrite(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles);

unsigned int Nib1Detect(LPBYTE imageptr, unsigned int imagesize);

void Nib1Read(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles);

void Nib1Write(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles);

unsigned int Nib2Detect(LPBYTE imageptr, unsigned int imagesize);

void Nib2Read(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles);

void Nib2Write(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles);

unsigned int PoDetect(LPBYTE imageptr, unsigned int imagesize);

void PoRead(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles);

void PoWrite(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles);

bool PrgBoot(imageinfoptr ptr);

unsigned int PrgDetect(LPBYTE imageptr, unsigned int imagesize);

unsigned int Woz2Detect(LPBYTE imageptr, unsigned int imagesize);

void Woz2Read(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles);


typedef struct _imagetyperec {
  LPCTSTR createExts;
  LPCTSTR rejectExts;
  detecttype detect;
  boottype boot;
  readtype read;
  writetype write;
} imagetyperec, *imagetypeptr;

static imagetyperec imagetype[IMAGETYPES] = {{TEXT(".prg"),     TEXT(
  ".do;.dsk;.iie;.nib;.po"),                                                                      PrgDetect,  PrgBoot, NULL,     NULL},
                                             {TEXT(".do;.dsk"), TEXT(
                                               ".nib;.iie;.po;.prg"),                           DoDetect,   NULL,    DoRead,   DoWrite},
                                             {TEXT(".po"),      TEXT(
                                               ".do;.iie;.nib;.prg"),                           PoDetect,   NULL,    PoRead,   PoWrite},
                                             {TEXT(".apl"),     TEXT(
                                               ".do;.dsk;.iie;.nib;.po"),                       AplDetect,  AplBoot, NULL,     NULL},
                                             {TEXT(".nib"),     TEXT(
                                               ".do;.iie;.po;.prg"),                            Nib1Detect, NULL,    Nib1Read, Nib1Write},
                                             {TEXT(".nb2"),     TEXT(
                                               ".do;.iie;.po;.prg"),                            Nib2Detect, NULL,    Nib2Read, Nib2Write},
                                             {TEXT(".iie"),     TEXT(
                                               ".do.;.nib;.po;.prg"),                           IieDetect,  NULL,    IieRead,  IieWrite},
                                             {TEXT(".woz"),     TEXT(
                                     ".do;.dsk;.iie;.nib;.po;.prg"),                            Woz2Detect, NULL,    Woz2Read, NULL}};

static unsigned char diskbyte[0x40] = {0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2,
                              0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE,
                              0xCF, 0xD3, 0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE5, 0xE6, 0xE7, 0xE9,
                              0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB,
                              0xFC, 0xFD, 0xFE, 0xFF};

static unsigned char sectornumber[3][0x10] = {{0x00, 0x08, 0x01, 0x09, 0x02, 0x0A, 0x03, 0x0B, 0x04, 0x0C, 0x05, 0x0D, 0x06, 0x0E, 0x07, 0x0F},
                                     {0x00, 0x07, 0x0E, 0x06, 0x0D, 0x05, 0x0C, 0x04, 0x0B, 0x03, 0x0A, 0x02, 0x09, 0x01, 0x08, 0x0F},
                                     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

static LPBYTE workbuffer = NULL;

// Nibblization functions
LPBYTE Code62(int sector)
{

  // Convert the 256 8-bit bytes into 342 6-bit bytes, which we store
  // Starting at 4k into the work buffer.
  {
    LPBYTE sectorBase = workbuffer + (sector << 8);
    LPBYTE resultptr = workbuffer + 0x1000;
    unsigned char offset = 0xAC;
    while (offset != 0x02) {
      unsigned char value = 0;
      #define ADDVALUE(a) value = (value << 2) |        \
                            (((a) & 0x01) << 1) | \
                            (((a) & 0x02) >> 1)
      ADDVALUE(*(sectorBase + offset));
      offset -= 0x56;
      ADDVALUE(*(sectorBase + offset));
      offset -= 0x56;
      ADDVALUE(*(sectorBase + offset));
      offset -= 0x53;
      #undef ADDVALUE
      *(resultptr++) = value << 2;
    }
    *(resultptr - 2) &= 0x3F;
    *(resultptr - 1) &= 0x3F;
    int loop = 0;
    while (loop < 0x100) {
      *(resultptr++) = *(sectorBase + (loop++));
    }
  }

  // Exclusive-or the entire data block with itself offset by one byte,
  // Creating a 343rd byte which is used as a checksum. Store the new
  // Block of 343 bytes starting at 5k into the work buffer.
  {
    unsigned char savedval = 0;
    LPBYTE sourceptr = workbuffer + 0x1000;
    LPBYTE resultptr = workbuffer + 0x1400;
    int loop = 342;
    while ((loop--) != 0) {
      *(resultptr++) = savedval ^ *sourceptr;
      savedval = *(sourceptr++);
    }
    *resultptr = savedval;
  }

  // Using a lookup table, convert the 6-bit bytes into disk bytes. A valid
  // disk byte is a byte that has the high bit set, at least two adjacent
  // bits set (excluding the high bit), and at most one pair of consecutive
  // zero bits. The converted block of 343 bytes is stored starting at 4k
  // into the work buffer.
  {
    LPBYTE sourceptr = workbuffer + 0x1400;
    LPBYTE resultptr = workbuffer + 0x1000;
    int loop = 343;
    while ((loop--) != 0) {
      *(resultptr++) = diskbyte[(*(sourceptr++)) >> 2];
    }
  }

  return workbuffer + 0x1000;
}

void Decode62(LPBYTE imageptr)
{

  // If we haven't already done so, generate a table for converting
  // disk bytes back into 6-bit bytes
  static bool tablegenerated = false;
  static unsigned char sixbitbyte[0x80];
  if (!tablegenerated) {
    ZeroMemory(sixbitbyte, 0x80);
    int loop = 0;
    while (loop < 0x40) {
      sixbitbyte[diskbyte[loop] - 0x80] = loop << 2;
      loop++;
    }
    tablegenerated = true;
  }

  // Using our table, convert the disk bytes back into 6-bit bytes
  {
    LPBYTE sourceptr = workbuffer + 0x1000;
    LPBYTE resultptr = workbuffer + 0x1400;
    int loop = 343;
    while ((loop--) != 0) {
      *(resultptr++) = sixbitbyte[*(sourceptr++) & 0x7F];
    }
  }

  // Exclusive-or the entire data block with itself offset by one byte
  // to undo the effects of the checksumming process
  {
    unsigned char savedval = 0;
    LPBYTE sourceptr = workbuffer + 0x1400;
    LPBYTE resultptr = workbuffer + 0x1000;
    int loop = 342;
    while ((loop--) != 0) {
      *resultptr = savedval ^ *(sourceptr++);
      savedval = *(resultptr++);
    }
  }

  // Convert the 342 6-bit bytes into 256 8-bit bytes
  {
    LPBYTE lowbitsptr = workbuffer + 0x1000;
    LPBYTE sectorBase = workbuffer + 0x1056;
    unsigned char offset = 0xAC;
    while (offset != 0x02) {
      if (offset >= 0xAC) {
        *(imageptr + offset) =
          (*(sectorBase + offset) & 0xFC) | (((*lowbitsptr) & 0x80) >> 7) | (((*lowbitsptr) & 0x40) >> 5);
      }
      offset -= 0x56;
      *(imageptr + offset) =
        (*(sectorBase + offset) & 0xFC) | (((*lowbitsptr) & 0x20) >> 5) | (((*lowbitsptr) & 0x10) >> 3);
      offset -= 0x56;
      *(imageptr + offset) =
        (*(sectorBase + offset) & 0xFC) | (((*lowbitsptr) & 0x08) >> 3) | (((*lowbitsptr) & 0x04) >> 1);
      offset -= 0x53;
      lowbitsptr++;
    }
  }
}

void DenibblizeTrack(LPBYTE trackimage, bool dosorder, int nibbles)
{
  ZeroMemory(workbuffer, 0x1000);

  // Search through the track image for each sector. For every sector
  // we find, copy the nibblized data for that sector into the work
  // buffer at offset 4k. Then call decode62() to denibblize the data
  // in the buffer and write it into the first part of the work buffer
  // offset by the sector number.
  {
    int offset = 0;
    int partsleft = 33;
    int sector = 0;
    while ((partsleft--) != 0) {
      unsigned char byteval[3] = {0, 0, 0};
      int bytenum = 0;
      int loop = nibbles;
      while (((loop--) != 0) && (bytenum < 3)) {
        if (bytenum != 0) {
          byteval[bytenum++] = *(trackimage + offset++);
        } else if (*(trackimage + offset++) == 0xD5) {
          bytenum = 1;
        }
        if (offset >= nibbles) {
          offset = 0;
        }
      }
      if ((bytenum == 3) && ((byteval[1] = 0xAA) != 0u)) {
        int loop = 0;
        int tempoffset = offset;
        while (loop < 384) {
          *(workbuffer + 0x1000 + loop++) = *(trackimage + tempoffset++);
          if (tempoffset >= nibbles) {
            tempoffset = 0;
          }
        }
        if (byteval[2] == 0x96) {
          sector = ((*(workbuffer + 0x1004) & 0x55) << 1) | (*(workbuffer + 0x1005) & 0x55);
        } else if (byteval[2] == 0xAD) {
          Decode62(workbuffer + (sectornumber[dosorder][sector] << 8));
          sector = 0;
        }
      }
    }
  }
}

unsigned int NibblizeTrack(LPBYTE trackImageBuffer, bool dosorder, int track)
{
  ZeroMemory(workbuffer + 4096, 4096);
  LPBYTE imageptr = trackImageBuffer;
  unsigned char sector = 0;

  // Write gap one, which contains 48 self-sync bytes
  int loop;
  for (loop = 0; loop < 48; loop++) {
    *(imageptr++) = 0xFF;
  }

  while (sector < 16) {

    // Write the address field, which contains:
    //   - PROLOGUE (D5AA96)
    //   - VOLUME NUMBER ("4 AND 4" ENCODED)
    //   - TRACK NUMBER ("4 AND 4" ENCODED)
    //   - SECTOR NUMBER ("4 AND 4" ENCODED)
    //   - CHECKSUM ("4 AND 4" ENCODED)
    //   - EPILOGUE (DEAAEB)
    *(imageptr++) = 0xD5;
    *(imageptr++) = 0xAA;
    *(imageptr++) = 0x96;
    #define VOLUME 0xFE
    #define CODE44A(a) ((((a) >> 1) & 0x55) | 0xAA)
    #define CODE44B(a) (((a) & 0x55) | 0xAA)
    *(imageptr++) = CODE44A(VOLUME);
    *(imageptr++) = CODE44B(VOLUME);
    *(imageptr++) = CODE44A((unsigned char) track);
    *(imageptr++) = CODE44B((unsigned char) track);
    *(imageptr++) = CODE44A(sector);
    *(imageptr++) = CODE44B(sector);
    *(imageptr++) = CODE44A(VOLUME ^ ((unsigned char) track) ^ sector);
    *(imageptr++) = CODE44B(VOLUME ^ ((unsigned char) track) ^ sector);
    #undef CODE44A
    #undef CODE44B
    *(imageptr++) = 0xDE;
    *(imageptr++) = 0xAA;
    *(imageptr++) = 0xEB;

    // Write gap two, which contains six self-sync bytes
    for (loop = 0; loop < 6; loop++) {
      *(imageptr++) = 0xFF;
    }

    // Write the data field, which contains:
    //   - PROLOGUE (D5AAAD)
    //   - 343 6-BIT BYTES OF NIBBLIZED DATA, INCLUDING A 6-BIT CHECKSUM
    //   - EPILOGUE (DEAAEB)
    *(imageptr++) = 0xD5;
    *(imageptr++) = 0xAA;
    *(imageptr++) = 0xAD;
    CopyMemory(imageptr, Code62(sectornumber[dosorder][sector]), 343);
    imageptr += 343;
    *(imageptr++) = 0xDE;
    *(imageptr++) = 0xAA;
    *(imageptr++) = 0xEB;

    // Write gap three, which contains 27 self-sync bytes
    for (loop = 0; loop < 27; loop++) {
      *(imageptr++) = 0xFF;
    }

    sector++;
  }
  return imageptr - trackImageBuffer;
}

void SkewTrack(int track, int nibbles, LPBYTE trackImageBuffer)
{
  int skewbytes = (track * 768) % nibbles;
  CopyMemory(workbuffer, trackImageBuffer, nibbles);
  CopyMemory(trackImageBuffer, workbuffer + skewbytes, nibbles - skewbytes);
  CopyMemory(trackImageBuffer + nibbles - skewbytes, workbuffer, skewbytes);
}

// RAW PROGRAM IMAGE (APL) FORMAT IMPLEMENTATION

bool AplBoot(imageinfoptr ptr) {
  SetFilePointer(ptr->file, 0, NULL, FILE_BEGIN);
  unsigned short address = 0;
  unsigned short length = 0;
  unsigned int bytesRead;
  ReadFile(ptr->file, &address, sizeof(unsigned short), &bytesRead, NULL);
  ReadFile(ptr->file, &length, sizeof(unsigned short), &bytesRead, NULL);
  if ((((unsigned short)(address + length)) <= address) || (address >= 0xC000) || (address + length - 1 >= 0xC000)) {
    return false;
  }
  ReadFile(ptr->file, mem + address, length, &bytesRead, NULL);
  int loop = 192;
  while ((loop--) != 0) {
    *(memdirty + loop) = 0xFF;
  }
  regs.pc = address;
  return true;
}

unsigned int AplDetect(LPBYTE imageptr, unsigned int imagesize) {
  unsigned int length = *(LPWORD)(imageptr + 2);
  return static_cast<unsigned int>(((length + 4) == imagesize) || ((length + 4 + ((256 - ((length + 4) & 255)) & 255)) == imagesize));
}

// DOS ORDER (DO) FORMAT IMPLEMENTATION
unsigned int DoDetect(LPBYTE imageptr, unsigned int imagesize) {
  if (((imagesize < 143105) || (imagesize > 143364)) && (imagesize != 143403) && (imagesize != 143488)) {
    return 0;
  }

  // Check for a DOS order image of a DOS diskette
  {
    int loop = 0;
    bool mismatch = false;
    while ((loop++ < 15) && !mismatch) {
      if (*(imageptr + 0x11002 + (loop << 8)) != loop - 1) {
        mismatch = true;
      }
    }
    if (!mismatch) {
      return 2;
    }
  }

  // Check for a DOS order image of a PRODOS diskette
  {
    int loop = 1;
    bool mismatch = false;
    while ((loop++ < 5) && !mismatch) {
      if ((*(LPWORD)(imageptr + (loop << 9) + 0x100) != ((loop == 5) ? 0 : 6 - loop)) ||
          (*(LPWORD)(imageptr + (loop << 9) + 0x102) != ((loop == 2) ? 0 : 8 - loop))) {
        mismatch = true;
      }
    }
    if (!mismatch) {
      return 2;
    }
  }
  return 1;
}

void DoRead(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles) {
  SetFilePointer(ptr->file, ptr->offset + (track << 12), NULL, FILE_BEGIN);
  ZeroMemory(workbuffer, 4096);
  unsigned int bytesRead;
  ReadFile(ptr->file, workbuffer, 4096, &bytesRead, NULL);
  *nibbles = NibblizeTrack(trackImageBuffer, true, track);
  if (!enhancedisk) {
    SkewTrack(track, *nibbles, trackImageBuffer);
  }
}

void DoWrite(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles)
{
  ZeroMemory(workbuffer, 4096);
  DenibblizeTrack(trackimage, true, nibbles);
  SetFilePointer(ptr->file, ptr->offset + (track << 12), NULL, FILE_BEGIN);
  unsigned int bytesWritten;
  WriteFile(ptr->file, workbuffer, 4096, &bytesWritten, NULL);
}

// SIMSYSTEM IIE (IIE) format implementation

void IieConvertSectorOrder(LPBYTE sourceorder) {
  int loop = 16;
  while ((loop--) != 0) {
    unsigned char found = 0xFF;
    int loop2 = 16;
    while (((loop2--) != 0) && (found == 0xFF)) {
      if (*(sourceorder + loop2) == loop) {
        found = loop2;
      }
    }
    if (found == 0xFF) {
      found = 0;
    }
    sectornumber[2][loop] = found;
  }
}

unsigned int IieDetect(LPBYTE imageptr, unsigned int imagesize)
{
  if ((strncmp((const char *) imageptr, "SIMSYSTEM_IIE", 13) != 0) || (*(imageptr + 13) > 3)) {
    return 0;
  }
  return 2;
}

void IieRead(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles)
{
  // If we haven't already done so, read the image file header
  if (ptr->header == nullptr) {
    ptr->header = (LPBYTE) VirtualAlloc(NULL, 88, 0x1000, 0);
    if (ptr->header == nullptr) {
      *nibbles = 0;
      return;
    }
    ZeroMemory(ptr->header, 88);
    unsigned int bytesRead;
    SetFilePointer(ptr->file, 0, NULL, FILE_BEGIN);
    ReadFile(ptr->file, ptr->header, 88, &bytesRead, NULL);
  }

  if (*(ptr->header + 13) <= 2) {
    // If this image contains user data, read the track and nibblize it
    IieConvertSectorOrder(ptr->header + 14);
    SetFilePointer(ptr->file, (track << 12) + 30, NULL, FILE_BEGIN);
    ZeroMemory(workbuffer, 4096);
    unsigned int bytesRead;
    ReadFile(ptr->file, workbuffer, 4096, &bytesRead, NULL);
    *nibbles = NibblizeTrack(trackImageBuffer, true, track);
  } else {
    // Otherwise, if this image contains nibble information, read it directly into the track buffer
    *nibbles = *(LPWORD)(ptr->header + (track << 1) + 14);
    unsigned int offset = 88;
    while ((track--) != 0) {
      offset += *(LPWORD)(ptr->header + (track << 1) + 14);
    }
    SetFilePointer(ptr->file, offset, NULL, FILE_BEGIN);
    ZeroMemory(trackImageBuffer, *nibbles);
    unsigned int bytesRead;
    ReadFile(ptr->file, trackImageBuffer, *nibbles, &bytesRead, NULL);
  }
}

void IieWrite(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles)
{
}

// Nibblized 6656-nibble (nib) format implementation

unsigned int Nib1Detect(LPBYTE imageptr, unsigned int imagesize)
{
  return (imagesize == 232960) ? 2 : 0;
}

void Nib1Read(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles)
{
  SetFilePointer(ptr->file, ptr->offset + track * NIBBLES, NULL, FILE_BEGIN);
  ReadFile(ptr->file, trackImageBuffer, NIBBLES, (unsigned int *) nibbles, NULL);
}

void Nib1Write(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles)
{
  SetFilePointer(ptr->file, ptr->offset + track * NIBBLES, NULL, FILE_BEGIN);
  unsigned int bytesWritten;
  WriteFile(ptr->file, trackimage, nibbles, &bytesWritten, NULL);
}

// NIBBLIZED 6384-NIBBLE (NB2) FORMAT IMPLEMENTATION

unsigned int Nib2Detect(LPBYTE imageptr, unsigned int imagesize)
{
  return (imagesize == 223440) ? 2 : 0;
}

void Nib2Read(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles)
{
  SetFilePointer(ptr->file, ptr->offset + track * 6384, NULL, FILE_BEGIN);
  ReadFile(ptr->file, trackImageBuffer, 6384, (unsigned int *) nibbles, NULL);
}

void Nib2Write(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles)
{
  SetFilePointer(ptr->file, ptr->offset + track * 6384, NULL, FILE_BEGIN);
  unsigned int bytesWritten;
  WriteFile(ptr->file, trackimage, nibbles, &bytesWritten, NULL);
}

// PRODOS order (po) format implementation

unsigned int PoDetect(LPBYTE imageptr, unsigned int imagesize) {
  if (((imagesize < 143105) || (imagesize > 143364)) && (imagesize != 143488)) {
    return 0;
  }

  // Check for a PRODOS order image of a dos diskette
  {
    int loop = 4;
    bool mismatch = false;
    while ((loop++ < 13) && !mismatch) {
      if (*(imageptr + 0x11002 + (loop << 8)) != 14 - loop) {
        mismatch = true;
      }
    }
    if (!mismatch) {
      return 2;
    }
  }

  // Check for a PRODOS order image of a prodos diskette
  {
    int loop = 1;
    bool mismatch = false;
    while ((loop++ < 5) && !mismatch) {
      if ((*(LPWORD)(imageptr + (loop << 9)) != ((loop == 2) ? 0 : loop - 1)) ||
          (*(LPWORD)(imageptr + (loop << 9) + 2) != ((loop == 5) ? 0 : loop + 1))) {
        mismatch = true;
      }
    }
    if (!mismatch) {
      return 2;
    }
  }

  return 1;
}

void PoRead(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles)
{
  SetFilePointer(ptr->file, ptr->offset + (track << 12), NULL, FILE_BEGIN);
  ZeroMemory(workbuffer, 4096);
  unsigned int bytesRead;
  ReadFile(ptr->file, workbuffer, 4096, &bytesRead, NULL);
  *nibbles = NibblizeTrack(trackImageBuffer, false, track);
  if (!enhancedisk) {
    SkewTrack(track, *nibbles, trackImageBuffer);
  }
}

void PoWrite(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles)
{
  ZeroMemory(workbuffer, 4096);
  DenibblizeTrack(trackimage, false, nibbles);
  SetFilePointer(ptr->file, ptr->offset + (track << 12), NULL, FILE_BEGIN);
  unsigned int bytesWritten;
  WriteFile(ptr->file, workbuffer, 4096, &bytesWritten, NULL);
}

// PRODOS PROGRAM IMAGE (PRG) FORMAT IMPLEMENTATION

bool PrgBoot(imageinfoptr ptr)
{
  SetFilePointer(ptr->file, 5, NULL, FILE_BEGIN);
  unsigned short address = 0;
  unsigned short length = 0;
  unsigned int bytesRead;
  ReadFile(ptr->file, &address, sizeof(unsigned short), &bytesRead, NULL);
  ReadFile(ptr->file, &length, sizeof(unsigned short), &bytesRead, NULL);
  length <<= 1;
  if ((((unsigned short)(address + length)) <= address) || (address >= 0xC000) || (address + length - 1 >= 0xC000)) {
    return false;
  }
  SetFilePointer(ptr->file, 128, NULL, FILE_BEGIN);
  ReadFile(ptr->file, mem + address, length, &bytesRead, NULL);
  int loop = 192;
  while ((loop--) != 0) {
    *(memdirty + loop) = 0xFF;
  }
  regs.pc = address;
  return true;
}

unsigned int PrgDetect(LPBYTE imageptr, unsigned int imagesize)
{
  return (*(LPDWORD) imageptr == 0x214C470A) ? 2 : 0;
}

// WOZ2 (woz) format implementation
// see: https://applesaucefdc.com/woz/reference2/

unsigned int Woz2Detect(LPBYTE imageptr, unsigned int imagesize)
{
  if (strncmp((const char *) imageptr, "WOZ2\xFF\n\r\n", 8) != 0) {
    return 0;
  }
  if (imagesize < WOZ2_HEADER_SIZE) {
    return 0;
  }
  return 2;
}

static unsigned int woz2_scan_sync_bytes(const uint8_t* buffer,
                                         const unsigned int bit_count,
                                         const unsigned int sync_bytes_needed,
                                         const unsigned int trailing_0_count);

void Woz2Read(imageinfoptr ptr, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles)
{
  unsigned int bytesRead;
  if (ptr->header == nullptr) {
    ptr->header = (LPBYTE) VirtualAlloc(NULL, WOZ2_HEADER_SIZE, 0x1000, 0);
    if (ptr->header == nullptr) {
      *nibbles = 0;
      return;
    }
    SetFilePointer(ptr->file, ptr->offset, NULL, FILE_BEGIN);
    ReadFile(ptr->file, ptr->header, WOZ2_HEADER_SIZE, &bytesRead, NULL);
    assert(bytesRead == WOZ2_HEADER_SIZE);
  }

  uint8_t* tmap = ptr->header + WOZ2_TMAP_OFFSET;
  uint8_t* trks = ptr->header + WOZ2_TRKS_OFFSET;
  uint8_t* trk = NULL;

  // Sorry, only integral tracks supported.
  // This is because the Disk II emulation does not properly handle
  // track data for half-/quarter- tracks.
  const unsigned int tmap_index = 4 * track;
  assert(tmap_index <= WOZ2_TMAP_SIZE);

  const unsigned int trks_index = tmap[tmap_index];

  if (trks_index == 0xFF) { // unrecorded track ==> random data
    for (unsigned int i = 0; i < NIBBLES; ++i) {
      trackImageBuffer[i] = (uint8_t)(rand() & 0xFF);
    }
    *nibbles = NIBBLES;
    return;
  }

  assert(trks_index <= WOZ2_TRKS_MAX_SIZE);
  trk = trks + trks_index * WOZ2_TRK_SIZE;

  const uint16_t starting_block = trk[0] | ((uint16_t) trk[1])<<8;
  const uint16_t block_count = trk[2] | ((uint16_t) trk[3])<<8;
  const uint32_t bit_count =  trk[4] | ((uint16_t) trk[5])<<8
    | ((uint16_t) trk[6])<<16 | ((uint16_t) trk[7])<<24;

  SetFilePointer(ptr->file, ptr->offset + starting_block * WOZ2_DATA_BLOCK_SIZE, NULL, FILE_BEGIN);

  const unsigned int byte_count = block_count * WOZ2_DATA_BLOCK_SIZE;
  LPBYTE buffer = (LPBYTE) VirtualAlloc(NULL, byte_count, 0x1000, 0);
  if (buffer == nullptr) {
    *nibbles = 0;
    return;
  }

  ReadFile(ptr->file, buffer, byte_count, &bytesRead, NULL);
  assert(bytesRead == byte_count);


  // scan for sync bytes
  // try 16-sector format first...
  unsigned int i = woz2_scan_sync_bytes(buffer, bit_count,
                                        4, // 4 sync bytes are enough
                                        2);
  if (i == 0) { // sync bytes not found: try 13-sector format...
    i = woz2_scan_sync_bytes(buffer, bit_count,
                             7, // need 7 sync bytes
                             1);
  }
  if (i == 0) { // sync bytes (still) not found
    VirtualFree(buffer, 0,/*MEM_RELEASE*/0);
    *nibbles = 0;
    return;
  }


  // now, scan for nibbles
#define FETCH_BIT(i) ((buffer[(i)/8] & (0x80 >> ((i)%8))) == 0? 0 : 1)
#define WRAP_BIT_INDEX(i)  do{ if ((i) >= bit_count) (i) -= bit_count; }while(0)
  unsigned int bits_left = bit_count;
  unsigned int nibbles_done = 0;
  WRAP_BIT_INDEX(i);
  while (nibbles_done < NIBBLES && bits_left > 0) {
    while (bits_left > 0) {
      if (FETCH_BIT(i) == 1)
        break;
      ++i;
      WRAP_BIT_INDEX(i);
      --bits_left;
    }
    if (bits_left < 8) {
      break;
    }

    uint8_t nibble = 0;
    for (int b=0; b<8; ++b) {
      nibble = (nibble << 1) | FETCH_BIT(i);
      ++i;
      WRAP_BIT_INDEX(i);
    }
    bits_left -= 8;

    trackImageBuffer[nibbles_done++] = nibble;
  }

  VirtualFree(buffer, 0,/*MEM_RELEASE*/0);

  *nibbles = nibbles_done;
}

static unsigned int woz2_scan_sync_bytes(const uint8_t* buffer,
                                         const unsigned int bit_count,
                                         const unsigned int sync_bytes_needed,
                                         const unsigned int trailing_0_count)
{
#define FETCH_BIT(i) ((buffer[(i)/8] & (0x80 >> ((i)%8))) == 0? 0 : 1)

  unsigned int i = 0;
  bool found_sync_FFs = false;

 rescan: for (;;) {
    while (i < bit_count && FETCH_BIT(i)==0) {
      ++i;
    }
    if (i >= bit_count) {
      break;
    }

    unsigned int nr_FFs = 0;
    for (;;) {
      unsigned int nr_1s = 0;
      while (i < bit_count && FETCH_BIT(i)==1) {
        ++nr_1s;
        ++i;
      }
      if (nr_1s < 8) {
        goto rescan;
      }
      if (nr_1s > 8) {
        nr_FFs = 1; // treat it as first FF
      }

      unsigned int nr_0s = 0;
      while (i < bit_count && FETCH_BIT(i)==0) {
        ++nr_0s;
        ++i;
      }
      if (nr_0s != trailing_0_count) {
        goto rescan;
      }

      // we've found 1111_1111_0[0].  This is an autosync byte
      ++nr_FFs;
      if (nr_FFs == sync_bytes_needed) {
        found_sync_FFs = true;
        break;
      }
    }
    if (found_sync_FFs) {
      break;
    }
  }

  if (!found_sync_FFs) {
    return 0; // 0 means not found
  }

  return i;
}

// All globally accessible functions are below this line

bool ImageBoot(HIMAGE imageHandle) {
  imageinfoptr ptr = (imageinfoptr) imageHandle;
  bool result = false;
  if (imagetype[ptr->format].boot != nullptr) {
    result = imagetype[ptr->format].boot(ptr);
  }
  if (result) {
    ptr->writeProtected = true;
  }
  return result;
}

void ImageClose(HIMAGE imageHandle)
{
  imageinfoptr ptr = (imageinfoptr) imageHandle;
  if (ptr->file != INVALID_HANDLE_VALUE) {
    CloseHandle(ptr->file);
  }
  for (int track = 0; track < TRACKS; track++) {
    if (!ptr->validTrack[track]) {
      DeleteFile(ptr->filename);
      break;
    }
  }
  if (ptr->header != nullptr) {
    VirtualFree(ptr->header, 0,/*MEM_RELEASE*/0);
  }
  VirtualFree(ptr, 0,/*MEM_RELEASE*/0);
}

void ImageDestroy()
{
  VirtualFree(workbuffer, 0,/*MEM_RELEASE*/0);
  workbuffer = NULL;
}

void ImageInitialize()
{
  workbuffer = (LPBYTE) VirtualAlloc(NULL, 0x2000, 0x1000,/*PAGE_READWRITE*/0);
}

int ImageOpen(LPCTSTR imagefilename, HIMAGE *hDiskImage_, bool *pWriteProtected_, bool bCreateIfNecessary)
{
  if (!((imagefilename != nullptr) && (hDiskImage_ != nullptr) && (!*pWriteProtected_) && (workbuffer != nullptr))) {
    return IMAGE_ERROR_BAD_POINTER;
  } // HACK: MAGIC # -1

  // Try to open the image file
  HANDLE file = INVALID_HANDLE_VALUE;

  if (!*pWriteProtected_)
    file = fopen(imagefilename, "r+b"); // open file in r/w mode
  // File may have read-only attribute set, so try to open as read-only.
  if (file == INVALID_HANDLE_VALUE) {
    file = fopen(imagefilename, "rb"); // open file just for reading

    if (file != INVALID_HANDLE_VALUE)
      *pWriteProtected_ = true;
  }

  if ((file == INVALID_HANDLE_VALUE) && bCreateIfNecessary) {
    file = fopen(imagefilename, "a+b"); // create file
  }

  // If we aren't able to open the file, return
  if (file == INVALID_HANDLE_VALUE) {
    return IMAGE_ERROR_UNABLE_TO_OPEN; // HACK: MAGIC # 1
  }

  // Determine the file's extension and convert it to lowercase
  LPCTSTR imagefileext = imagefilename;
  if (_tcsrchr(imagefileext, FILE_SEPARATOR)) {
    imagefileext = _tcsrchr(imagefileext, FILE_SEPARATOR) + 1;
  }
  if (_tcsrchr(imagefileext, TEXT('.'))) {
    imagefileext = _tcsrchr(imagefileext, TEXT('.'));
  }

  #define _MAX_EXT  5
  char ext[_MAX_EXT];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
  _tcsncpy(ext, imagefileext, _MAX_EXT);
#pragma GCC diagnostic pop

  CharLowerBuff(ext, _tcslen(ext));

  unsigned int size = GetFileSize(file, NULL);
  LPBYTE view = NULL;
  LPBYTE pImage = NULL;

  const unsigned int UNKNOWN_FORMAT = 0xFFFFFFFF;
  unsigned int format = UNKNOWN_FORMAT;

  if (size > 0) {
    view = (LPBYTE) malloc(size);
    size_t bytesRead = fread(view, 1, size, (FILE *) file);
    if (bytesRead > 0) {
      fseek((FILE *) file, 0, FILE_BEGIN); // I just got accustomed to mrsftish FILE_BEGIN, FILE_END, etc. Hmm. ^_^
    }
    pImage = view;

    if (pImage != nullptr) {
      // Determine whether the file has a 128-byte macbinary header
      if ((size > 128) && (*pImage == 0u) && (*(pImage + 1) < 120) && (*(pImage + *(pImage + 1) + 2) == 0u) &&
          (*(pImage + 0x7A) == 0x81) && (*(pImage + 0x7B) == 0x81)) {
        pImage += 128;
        size -= 128;
      }

      // Call the detection functions in order, looking for a match
      unsigned int possibleformat = UNKNOWN_FORMAT;
      int loop = 0;
      while ((loop < IMAGETYPES) && (format == UNKNOWN_FORMAT)) {
        if ((*ext != 0) && _tcsstr(imagetype[loop].rejectExts, ext)) {
          ++loop;
        } else {
          unsigned int result = imagetype[loop].detect(pImage, size);
          if (result == 2) {
            format = loop;
          } else if ((result == 1) && (possibleformat == UNKNOWN_FORMAT)) {
            possibleformat = loop++;
          } else {
            ++loop;
          }
        }
      }

      if (format == UNKNOWN_FORMAT)
        format = possibleformat;

      free(view); // free memory block, allocated before
    }
  } else {
    // We create only DOS order (do) or 6656-nibble (nib) format files
    for (int loop = 1; loop <= 4; loop += 3) {
      if ((*ext != 0) && _tcsstr(imagetype[loop].createExts, ext)) {
        format = loop;
        break;
      }

    }
  }

  // If the file does match a known format
  if (format != UNKNOWN_FORMAT) {
    // Create a record for the file, and return an image handle
    *hDiskImage_ = (HIMAGE) VirtualAlloc(NULL, sizeof(imageinforec), 0x1000, 0);
    if (*hDiskImage_ != nullptr) {
      ZeroMemory(*hDiskImage_, sizeof(imageinforec));
      // Do this in DiskInsert
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
      _tcsncpy(((imageinfoptr) *hDiskImage_)->filename, imagefilename, MAX_PATH);
#pragma GCC diagnostic pop
      ((imageinfoptr) *hDiskImage_)->format = format;
      ((imageinfoptr) *hDiskImage_)->file = file;
      ((imageinfoptr) *hDiskImage_)->offset = pImage - view;
      ((imageinfoptr) *hDiskImage_)->writeProtected = *pWriteProtected_;

      for (int track = 0; track < TRACKS; track++) {
        ((imageinfoptr) *hDiskImage_)->validTrack[track] = (size > 0);
      }

      return IMAGE_ERROR_NONE; // HACK: MAGIC # 0
    }
  }

  CloseHandle(file);
  if (size <= 0) {
    DeleteFile(imagefilename);
  }

  return IMAGE_ERROR_BAD_SIZE; // HACK: MAGIC # 2
}

void ImageReadTrack(HIMAGE imageHandle, int track, int quartertrack, LPBYTE trackImageBuffer, int *nibbles) {
  imageinfoptr ptr = (imageinfoptr) imageHandle;
  if ((imagetype[ptr->format].read != nullptr) && ptr->validTrack[track]) {
    imagetype[ptr->format].read(ptr, track, quartertrack, trackImageBuffer, nibbles);
  } else {
    for (*nibbles = 0; *nibbles < NIBBLES; (*nibbles)++) {
      trackImageBuffer[*nibbles] = (unsigned char)(rand() & 0xFF);
    }
  }
}

void ImageWriteTrack(HIMAGE imageHandle, int track, int quartertrack, LPBYTE trackimage, int nibbles) {
  imageinfoptr ptr = (imageinfoptr) imageHandle;
  if ((imagetype[ptr->format].write != nullptr) && !ptr->writeProtected) {
    imagetype[ptr->format].write(ptr, track, quartertrack, trackimage, nibbles);
    ptr->validTrack[track] = true;
  }
}
