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

/* Description: Hard drive emulation
 *
 * Author: Copyright (c) 2005, Robert Hoem
 */

/* Adaptation for SDL and POSIX (l) by beom beotiger, Nov-Dec 2007 */

#include "stdafx.h"
#include "wwrapper.h"
#include "ftpparse.h"
#include "DiskFTP.h"

/*
Memory map:

    C0F0  (r)   EXECUTE AND RETURN STATUS
  C0F1  (r)   STATUS (or ERROR)
  C0F2  (r/w) COMMAND
  C0F3  (r/w) UNIT NUMBER
  C0F4  (r/w) LOW unsigned char OF MEMORY BUFFER
  C0F5  (r/w) HIGH unsigned char OF MEMORY BUFFER
  C0F6  (r/w) LOW unsigned char OF BLOCK NUMBER
  C0F7  (r/w) HIGH unsigned char OF BLOCK NUMBER
  C0F8    (r)   NEXT unsigned char
*/

/*
Hard drive emulation in Applewin.

Concept
    To emulate a 32mb hard drive connected to an Apple IIe via Applewin.
    Designed to work with Autoboot Rom and Prodos.

Overview
  1. Hard drive image file
      The hard drive image file (.HDV) will be formatted into blocks of 512
      bytes, in a linear fashion. The internal formatting and meaning of each
      block to be decided by the Apple's operating system (ProDos). To create
      an empty .HDV file, just create a 0 byte file (I prefer the debug method).

  2. Emulation code
      There are 4 commands Prodos will send to a block device.
      Listed below are each command and how it's handled:

      1. STATUS
          In the emulation's case, returns only a DEVICE OK (0) or DEVICE I/O ERROR (8).
          DEVICE I/O ERROR only returned if no HDV file is selected.

      2. READ
          Loads requested block into a 512 byte buffer by attempting to seek to
            location in HDV file.
          If seek fails, returns a DEVICE I/O ERROR.  Resets hd_buf_ptr used by HD_NEXTBYTE
          Returns a DEVICE OK if read was successful, or a DEVICE I/O ERROR otherwise.

      3. WRITE
          Copies requested block from the Apple's memory to a 512 byte buffer
            then attempts to seek to requested block.
          If the seek fails (usually because the seek is beyond the EOF for the
            HDV file), the Emulation will attempt to "grow" the HDV file to accomodate.
            Once the file can accomodate, or if the seek did not fail, the buffer is
            written to the HDV file.  NOTE: A2PC will grow *AND* shrink the HDV file.
          I didn't see the point in shrinking the file as this behaviour would require
            patching prodos (to detect DELETE FILE calls).

      4. FORMAT
          Ignored.  This would be used for low level formatting of the device
            (as in the case of a tape or SCSI drive, perhaps).

  3. Bugs
      The only thing I've noticed is that Copy II+ 7.1 seems to crash or stall
      occasionally when trying to calculate how many free block are available
      when running a catalog.  This might be due to the great number of blocks
      available.  Also, DDD pro will not optimise the disk correctally (it's
      doing a disk defragment of some sort, and when it requests a block outside
      the range of the image file, it starts getting I/O errors), so don't
      bother.  Any program that preforms a read before write to an "unwritten"
      block (a block that should be located beyond the EOF of the .HDV, which is
      valid for writing but not for reading until written to) will fail with I/O
      errors (although these are few and far between).

      I'm sure there are programs out there that may try to use the I/O ports in
      ways they weren't designed (like telling Ultima 5 that you have a Phazor
      sound card in slot 7 is a generally bad idea) will cause problems.
*/

char Hddrvr_dat[] = "\xA9\x20\xA9\x00\xA9\x03\xA9\x3C\xA9\x00\x8D\xF2\xC0\xA9\x70\x8D"
                    "\xF3\xC0\xAD\xF0\xC0\x48\xAD\xF1\xC0\x18\xC9\x01\xD0\x01\x38\x68"
                    "\x90\x03\x4C\x00\xC6\xA9\x70\x85\x43\xA9\x00\x85\x44\x85\x46\x85"
                    "\x47\xA9\x08\x85\x45\xA9\x01\x85\x42\x20\x46\xC7\x90\x03\x4C\x00"
                    "\xC6\xA2\x70\x4C\x01\x08\x18\xA5\x42\x8D\xF2\xC0\xA5\x43\x8D\xF3"
                    "\xC0\xA5\x44\x8D\xF4\xC0\xA5\x45\x8D\xF5\xC0\xA5\x46\x8D\xF6\xC0"
                    "\xA5\x47\x8D\xF7\xC0\xAD\xF0\xC0\x48\xA5\x42\xC9\x01\xD0\x03\x20"
                    "\x7D\xC7\xAD\xF1\xC0\x18\xC9\x01\xD0\x01\x38\x68\x60\x98\x48\xA0"
                    "\x00\xAD\xF8\xC0\x91\x44\xC8\xD0\xF8\xE6\x45\xA0\x00\xAD\xF8\xC0"
                    "\x91\x44\xC8\xD0\xF8\x68\xA8\x60\x00\x00\x00\x00\x00\x00\x00\x00"
                    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\x7F\xD7\x46";


typedef struct {
  char hd_imagename[16];
  char hd_fullname[128];
  unsigned char hd_error;
  unsigned short hd_memblock;
  unsigned short hd_diskblock;
  unsigned short hd_buf_ptr;
  bool hd_imageloaded;
  HANDLE hd_file;
  unsigned char hd_buf[513];
} HDD, *PHDD;

static bool g_bHD_RomLoaded = false;
bool g_bHD_Enabled = false;

static unsigned char g_nHD_UnitNum = DRIVE_1;

// The HDD interface has a single Command register for both drives:
// . ProDOS will write to Command before switching drives
static unsigned char g_nHD_Command;

static HDD g_HardDrive[2] = {0};

static unsigned int g_uSlot = 7;

static int HDDStatus = DISK_STATUS_OFF;  // status: 0 - none, 1 - read, 2 - write

int HD_GetStatus(void)
{
  int result = HDDStatus;
  return result;
}

void HD_ResetStatus(void)
{
  HDDStatus = DISK_STATUS_OFF;
}


static void GetImageTitle(LPCTSTR imageFileName, PHDD pHardDrive)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
  char imagetitle[128];
  LPCTSTR startpos = imageFileName;

  if (_tcsrchr(startpos, FILE_SEPARATOR)) {
    startpos = _tcsrchr(startpos, FILE_SEPARATOR) + 1;
  }
  _tcsncpy(imagetitle, startpos, 127);
  imagetitle[127] = 0;

  bool found = false;
  int loop = 0;
  while ((imagetitle[loop] != 0) && !found) {
    if (IsCharLower(imagetitle[loop])) {
      found = true;
    } else {
      loop++;
    }
  }

  _tcsncpy(pHardDrive->hd_fullname, imagetitle, 127);
  pHardDrive->hd_fullname[127] = 0;

  if (imagetitle[0] != 0) {
    LPTSTR dot = imagetitle;
    if (_tcsrchr(dot, TEXT('.'))) {
      dot = _tcsrchr(dot, TEXT('.'));
    }
    if (dot > imagetitle) {
      *dot = 0;
    }
  }

  _tcsncpy(pHardDrive->hd_imagename, imagetitle, 15);
  pHardDrive->hd_imagename[15] = 0;
#pragma GCC diagnostic pop
}

static void NotifyInvalidImage(char *filename)
{
  printf("HDD: Could not load %s\n", filename);
}

static void HD_CleanupDrive(int nDrive)
{
  if (g_HardDrive[nDrive].hd_file != nullptr) {
    CloseHandle(g_HardDrive[nDrive].hd_file);
  }
  g_HardDrive[nDrive].hd_imageloaded = false;
  g_HardDrive[nDrive].hd_imagename[0] = 0;
  g_HardDrive[nDrive].hd_fullname[0] = 0;
}

static bool HD_Load_Image(int nDrive, LPCSTR filename)
{
  g_HardDrive[nDrive].hd_file = fopen(filename, "r+b");

  g_HardDrive[nDrive].hd_imageloaded = g_HardDrive[nDrive].hd_file != INVALID_HANDLE_VALUE;

  return g_HardDrive[nDrive].hd_imageloaded;
}

#if 0
static LPCTSTR HD_DiskGetName (int nDrive)
{
  return g_HardDrive[nDrive].hd_imagename;
}
#endif

// Everything below is global

static unsigned char HD_IO_EMUL(unsigned short pc, unsigned short addr, unsigned char bWrite, unsigned char d, ULONG nCyclesLeft);

static const unsigned int HDDRVR_SIZE = 0x100;

bool HD_CardIsEnabled() {
  return g_bHD_RomLoaded && g_bHD_Enabled;
}

void HD_SetEnabled(bool bEnabled) {
  if (g_bHD_Enabled == bEnabled)
    return;

  g_bHD_Enabled = bEnabled;

  LPBYTE pCxRomPeripheral = MemGetCxRomPeripheral();
  if (pCxRomPeripheral == NULL) { // This will be NULL when called after loading value from Registry
    return;
  }

  if (g_bHD_Enabled) {
    HD_Load_Rom(pCxRomPeripheral, g_uSlot);
  } else {
    memset(pCxRomPeripheral + g_uSlot * 256, 0, HDDRVR_SIZE);
  }

  RegisterIoHandler(g_uSlot, HD_IO_EMUL, HD_IO_EMUL, NULL, NULL, NULL, NULL);
}

LPCTSTR HD_GetFullName(int nDrive) {
  return g_HardDrive[nDrive].hd_fullname;
}

void HD_Load_Rom(LPBYTE pCxRomPeripheral, unsigned int uSlot) {
  if (!g_bHD_Enabled)
    return;

  unsigned char *pData = (unsigned char *) Hddrvr_dat;  // NB. Don't need to unlock resource - hmmmmmm....... i love linux

  g_uSlot = uSlot;
  memcpy(pCxRomPeripheral + uSlot * 256, pData, HDDRVR_SIZE);
  g_bHD_RomLoaded = true;
}

void HD_Cleanup() {
  for (int i = DRIVE_1; i < DRIVE_2; i++) {
    HD_CleanupDrive(i);
  }
}

// pszFilename is not qualified with path
bool HD_InsertDisk2(int nDrive, LPCTSTR pszFilename) {
  if (*pszFilename == 0x00) {
    return false;
  }
  return HD_InsertDisk(nDrive, pszFilename);
}

// imageFileName is qualified with path
bool HD_InsertDisk(int nDrive, LPCTSTR imageFileName) {
  if (*imageFileName == 0x00) {
    return false;
  }

  if (g_HardDrive[nDrive].hd_imageloaded) {
    HD_CleanupDrive(nDrive);
  }

  bool result = HD_Load_Image(nDrive, imageFileName);

  if (result) {
    GetImageTitle(imageFileName, &g_HardDrive[nDrive]);
  } else {
    NotifyInvalidImage((char *) imageFileName);  // could not load hd image
  }

  return result;
}

void HD_FTP_Select(int nDrive)
{
  // Selects HDrive from FTP directory
  static size_t fileIndex = 0; // file index will be remembered for current dir
  static size_t backdx = 0;  //reserve
  static size_t dirdx = 0;  // reserve for dirs

  std::string filename;      // given filename
  std::string fullPath;  // full path for it
  bool isDirectory = true;      // if given filename is a directory?

  fileIndex = backdx;
  fullPath = g_sFTPServerHDD;  // global var for FTP path for HDD

  while (isDirectory) {
    if (!ChooseAnImageFTP(g_ScreenWidth, g_ScreenHeight, fullPath, 7,
                          filename, isDirectory, fileIndex)) {
      DrawFrameWindow();
      return;  // if ESC was pressed, just leave
    }
    // --
    if (isDirectory) {
      if (filename == "..") {
        // go to the upper directory
        auto r = fullPath.find_last_of(FTP_SEPARATOR);
        if (r == fullPath.size()-1) {
          r = fullPath.find_last_of(FTP_SEPARATOR, r-1);
        }
        if (r != std::string::npos) {
          fullPath = fullPath.substr(0, 1+r);
        }
        if (fullPath == "") {
          fullPath = "/";  //we don't want fullPath to be empty
        }
        fileIndex = dirdx;  // restore
      } else {
        if (fullPath != "/") {
          fullPath += filename + "/";
        } else {
          fullPath = "/" + filename + "/";
        }
        printf("HD_FTP_Select: we build %s\n", fullPath.c_str());
        dirdx = fileIndex; // store it
        fileIndex = 0;  // start with beginning of dir
      }
    }
  }
  // we chose some file
  _l_strcpy(g_sFTPServerHDD, fullPath.c_str());
  RegSaveString(TEXT("Preferences"), REGVALUE_FTP_HDD_DIR, true, g_sFTPServerHDD);// save it

  fullPath += "/" + filename;

  std::string localPath = std::string(g_sFTPLocalDir) + "/" + filename; // local path for file

  int error = ftp_get(fullPath.c_str(), localPath.c_str());
  if (error == 0) {
    if (HD_InsertDisk2(nDrive, localPath.c_str())) {
      // save file names for HDD disk 1 or 2
      if (nDrive != 0) {
        RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE2, true, localPath.c_str());
      } else {
        RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE1, true, localPath.c_str());
      }
    }
  }
  backdx = fileIndex;  //store cursor position
  DrawFrameWindow();
}

void HD_Select(int nDrive)
{
  // Selects HDrive from file list
  static size_t fileIndex = 0; // file index will be remembered for current dir
  static size_t backdx = 0;  //reserve
  static size_t dirdx = 0;  // reserve for dirs

  std::string filename;      // given filename
  std::string fullPath;  // full path for it
  bool isDirectory;      // if given filename is a directory?

  fileIndex = backdx;
  isDirectory = true;
  fullPath = g_sHDDDir;  // global var for disk selecting directory

  while (isDirectory) {
    if (!ChooseAnImage(g_ScreenWidth, g_ScreenHeight, fullPath, 7,
                       filename, isDirectory, fileIndex)) {
      DrawFrameWindow();
      return;  // if ESC was pressed, just leave
    }
    if (isDirectory) {
      if (filename == "..") { // go to the upper directory
        const auto last_sep_pos = fullPath.find_last_of(FILE_SEPARATOR);
        if (last_sep_pos != std::string::npos) {
          fullPath = fullPath.substr(0, last_sep_pos);
        }
        if (fullPath == "") {
          fullPath = "/";  //we don't want fullPath to be empty
        }
        fileIndex = dirdx;  // restore
      } else {
        if (fullPath != "/") {
          fullPath += "/" + filename;
        } else {
          fullPath = "/" + filename;
        }
        dirdx = fileIndex; // store it
        fileIndex = 0;  // start with beginning of dir
      }
    }
  }
  // we chose some file
  _l_strcpy(g_sHDDDir, fullPath.c_str());
  RegSaveString(TEXT("Preferences"), REGVALUE_PREF_HDD_START_DIR, true, g_sHDDDir); // Save it

  fullPath += "/" + filename;

  // in future: save file name in registry for future fetching
  // for one drive will be one reg parameter
  if (HD_InsertDisk2(nDrive, fullPath.c_str())) {
    // save file names for HDD disk 1 or 2
    if (nDrive != 0) {
      RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE2, true, fullPath.c_str());
    } else {
      RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE1, true, fullPath.c_str());
    }
    printf("HDD disk image %s inserted\n", fullPath.c_str());
  }
  backdx = fileIndex; // Store cursor position
  DrawFrameWindow();
}

#define DEVICE_OK        0x00
#define DEVICE_UNKNOWN_ERROR  0x03
#define DEVICE_IO_ERROR      0x08

static unsigned char HD_IO_EMUL(unsigned short pc, unsigned short addr, unsigned char bWrite, unsigned char d, ULONG nCyclesLeft) {
  unsigned char r = DEVICE_OK;
  addr &= 0xFF;

  if (!HD_CardIsEnabled()) {
    return r;
  }

  PHDD pHDD = &g_HardDrive[g_nHD_UnitNum >> 7];  // bit7 = drive select

  if (bWrite == 0) { // read
    switch (addr) {
      case 0xF0: {
        if (pHDD->hd_imageloaded) {
          // based on loaded data block request, load block into memory
          // returns status
          switch (g_nHD_Command) {
            default:
            case 0x00: //status
              if (GetFileSize(pHDD->hd_file, NULL) == 0) {
                pHDD->hd_error = 1;
                r = DEVICE_IO_ERROR;
              }
              break;
            case 0x01: //read
            {
              HDDStatus = DISK_STATUS_READ;
              unsigned int br = GetFileSize(pHDD->hd_file, NULL);
              if ((unsigned int)(pHDD->hd_diskblock * 512) <= br) { // seek to block
                SetFilePointer(pHDD->hd_file, pHDD->hd_diskblock * 512, NULL, FILE_BEGIN);  // seek to block
                if (ReadFile(pHDD->hd_file, pHDD->hd_buf, 512, &br, NULL)) { // read block into buffer
                  pHDD->hd_error = 0;
                  r = 0;
                  pHDD->hd_buf_ptr = 0;
                } else {
                  pHDD->hd_error = 1;
                  r = DEVICE_IO_ERROR;
                }
              } else {
                pHDD->hd_error = 1;
                r = DEVICE_IO_ERROR;
              }
            }
              break;
            case 0x02: //write
            {
              HDDStatus = DISK_STATUS_WRITE;
              unsigned int bw = GetFileSize(pHDD->hd_file, NULL);
              if ((unsigned int)(pHDD->hd_diskblock * 512) <= bw) {
                MoveMemory(pHDD->hd_buf, mem + pHDD->hd_memblock, 512);
                SetFilePointer(pHDD->hd_file, pHDD->hd_diskblock * 512, NULL, FILE_BEGIN);  // seek to block
                if (WriteFile(pHDD->hd_file, pHDD->hd_buf, 512, &bw, NULL)) { // write buffer to file
                  pHDD->hd_error = 0;
                  r = 0;
                } else {
                  pHDD->hd_error = 1;
                  r = DEVICE_IO_ERROR;
                }
              } else {
                unsigned int fsize = SetFilePointer(pHDD->hd_file, 0, NULL, FILE_END);
                unsigned int addblocks = pHDD->hd_diskblock - (fsize / 512);
                FillMemory(pHDD->hd_buf, 512, 0);
                while ((addblocks--) != 0u) {
                  unsigned int bw;
                  WriteFile(pHDD->hd_file, pHDD->hd_buf, 512, &bw, NULL);
                }
                if (SetFilePointer(pHDD->hd_file, pHDD->hd_diskblock * 512, NULL, FILE_BEGIN) != 0xFFFFFFFF) {
                  // seek to block
                  MoveMemory(pHDD->hd_buf, mem + pHDD->hd_memblock, 512);
                  if (WriteFile(pHDD->hd_file, pHDD->hd_buf, 512, &bw, NULL)) { // write buffer to file
                    pHDD->hd_error = 0;
                    r = 0;
                  } else {
                    pHDD->hd_error = 1;
                    r = DEVICE_IO_ERROR;
                  }
                }
              }
            }
              break;
            case 0x03: //format
              HDDStatus = DISK_STATUS_WRITE;
              break;
          }
        } else {
          HDDStatus = DISK_STATUS_OFF;
          pHDD->hd_error = 1;
          r = DEVICE_UNKNOWN_ERROR;
        }
      }
        break;
      case 0xF1: // hd_error
      {
        r = pHDD->hd_error;
      }
        break;
      case 0xF2: {
        r = g_nHD_Command;
      }
        break;
      case 0xF3: {
        r = g_nHD_UnitNum;
      }
        break;
      case 0xF4: {
        r = (unsigned char)(pHDD->hd_memblock & 0x00FF);
      }
        break;
      case 0xF5: {
        r = (unsigned char)(pHDD->hd_memblock & 0xFF00 >> 8);
      }
        break;
      case 0xF6: {
        r = (unsigned char)(pHDD->hd_diskblock & 0x00FF);
      }
        break;
      case 0xF7: {
        r = (unsigned char)(pHDD->hd_diskblock & 0xFF00 >> 8);
      }
        break;
      case 0xF8: {
        r = pHDD->hd_buf[pHDD->hd_buf_ptr];
        pHDD->hd_buf_ptr++;
      }
        break;
      default:
        printf("Unknow coomand: bWrite=0\n");
        return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
    }
  } else { // write
    switch (addr) {
      case 0xF2: {
        g_nHD_Command = d;
      }
        break;
      case 0xF3: {
        // b7    = drive#
        // b6..4 = slot#
        // b3..0 = ?
        g_nHD_UnitNum = d;
      }
        break;
      case 0xF4: {
        pHDD->hd_memblock = (pHDD->hd_memblock & 0xFF00) | d;
      }
        break;
      case 0xF5: {
        pHDD->hd_memblock = (pHDD->hd_memblock & 0x00FF) | (d << 8);
      }
        break;
      case 0xF6: {
        pHDD->hd_diskblock = (pHDD->hd_diskblock & 0xFF00) | d;
      }
        break;
      case 0xF7: {
        pHDD->hd_diskblock = (pHDD->hd_diskblock & 0x00FF) | (d << 8);
      }
        break;
      default:
        printf("Unknow coomand: bWrite=1\n");
        return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
    }
  }
  FrameRefreshStatus(DRAW_LEDS);  // update status area
  return r;
}
