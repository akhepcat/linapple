#pragma once

#define USE_SPEECH_API

const double M14 = (157500000.0 / 11.0); // 14.3181818... * 10^6
const double CLOCK_6502 = ((M14 * 65.0) / 912.0); // 65 cycles per 912 14M clocks

// The effective Z-80 clock rate is 2.041MHz
const double CLK_Z80 = (CLOCK_6502 * 2);

const unsigned int uCyclesPerLine = 65; // 25 cycles of HBL & 40 cycles of HBL
const unsigned int uVisibleLinesPerFrame = 64 * 3; // 192
const unsigned int uLinesPerFrame = 262; // 64 in each third of the screen & 70 in VBL
const unsigned int dwClksPerFrame = uCyclesPerLine * uLinesPerFrame; // 17030

#define NUM_SLOTS 8

#ifndef MIN
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define RAMWORKS // 8MB RamWorks III support
#define MOCKINGBOARD // Mockingboard support

// Use a base freq so that DirectX (or sound h/w) doesn't have to up/down-sample. Assume base freqs are 44.1KHz & 48KHz.
const unsigned int SPKR_SAMPLE_RATE = 44100;  // that is for Apple][ speakers
const unsigned int SAMPLE_RATE = 44100;  // that is for Phasor/Mockingboard?

enum AppMode_e {
  MODE_LOGO = 0,
  MODE_PAUSED,
  MODE_RUNNING, // 6502 is running at normal speed (Debugger breakpoints may or may not be active)
  MODE_DEBUG, // 6502 is paused
  MODE_STEPPING, // 6502 is running at full speed (Debugger breakpoints always active)
};

#define SPEED_MIN         0
#define SPEED_NORMAL      10
#define SPEED_MAX         40

#define DRAW_BACKGROUND   1
#define DRAW_LEDS         2
#define DRAW_TITLE        4
#define DRAW_BUTTON_DRIVES 8

// Function Keys F1 - F12
#define BTN_HELP          0
#define BTN_RUN           1
#define BTN_DRIVE1        2
#define BTN_DRIVE2        3
#define BTN_DRIVESWAP     4
#define BTN_FULLSCR       5
#define BTN_DEBUG         6
#define BTN_SETUP         7
#define BTN_CYCLE         8
#define BTN_QUIT          11
#define BTN_SAVEST        10
#define BTN_LOADST        9

// TODO: Move to StringTable.h
#define TITLE_APPLE_2      TEXT("Apple ][ Emulator")
#define TITLE_APPLE_2_PLUS    TEXT("Apple ][+ Emulator")
#define TITLE_APPLE_2E      TEXT("Apple //e Emulator")
#define TITLE_APPLE_2E_ENHANCED  TEXT("Enhanced Apple //e Emulator")

#define TITLE_PAUSED       TEXT(" Paused ")
#define TITLE_STEPPING     TEXT("Stepping")

#define LOAD(a, b) RegLoadValue(TEXT("Configuration"),a,1,b)
#define SAVE(a, b) RegSaveValue(TEXT("Configuration"),a,1,b)

// Configuration
#define REGVALUE_APPLE2_TYPE        "Apple2 Type"
#define REGVALUE_SPKR_VOLUME        "Speaker Volume"
#define REGVALUE_MB_VOLUME          "Mockingboard Volume"
#define REGVALUE_SOUNDCARD_TYPE     "Soundcard Type"
#define REGVALUE_KEYB_TYPE          "Keyboard Type"
#define REGVALUE_KEYB_CHARSET_SWITCH "Keyboard Rocker Switch"
#define REGVALUE_SAVESTATE_FILENAME "Save State Filename"
#define REGVALUE_SAVE_STATE_ON_EXIT "Save State On Exit"
#define REGVALUE_HDD_ENABLED        "Harddisk Enable"
#define REGVALUE_HDD_IMAGE1         "Harddisk Image 1"
#define REGVALUE_HDD_IMAGE2         "Harddisk Image 2"
#define REGVALUE_DISK_IMAGE1         "Disk Image 1"
#define REGVALUE_DISK_IMAGE2         "Disk Image 2"
#define REGVALUE_CLOCK_SLOT         "Clock Enable"

#define  REGVALUE_PPRINTER_FILENAME  "Parallel Printer Filename"
#define  REGVALUE_PRINTER_APPEND     "Append to printer file"
#define  REGVALUE_PRINTER_IDLE_LIMIT "Printer idle limit"

#define REGVALUE_PDL_XTRIM          "PDL X-Trim"
#define REGVALUE_PDL_YTRIM          "PDL Y-Trim"
#define REGVALUE_SCROLLLOCK_TOGGLE  "ScrollLock Toggle"
#define REGVALUE_MOUSE_IN_SLOT4     "Mouse in slot 4"

// Preferences
#define REGVALUE_PREF_START_DIR TEXT("Slot 6 Directory")
#define REGVALUE_PREF_HDD_START_DIR TEXT("HDV Starting Directory")
#define REGVALUE_PREF_SAVESTATE_DIR TEXT("Save State Directory")

#define REGVALUE_SHOW_LEDS TEXT("Show Leds")

// For FTP access
#define REGVALUE_FTP_DIR TEXT("FTP Server")
#define REGVALUE_FTP_HDD_DIR TEXT("FTP ServerHDD")

#define REGVALUE_FTP_LOCAL_DIR TEXT("FTP Local Dir")
#define REGVALUE_FTP_USERPASS TEXT("FTP UserPass")

#define WM_USER_BENCHMARK  WM_USER+1
#define WM_USER_RESTART    WM_USER+2
#define WM_USER_SAVESTATE  WM_USER+3
#define WM_USER_LOADSTATE  WM_USER+4

enum eSOUNDCARDTYPE {
  SC_UNINIT = 0, SC_NONE, SC_MOCKINGBOARD, SC_PHASOR
};  // Apple soundcard type

typedef unsigned char (*iofunction)(unsigned short nPC, unsigned short nAddr, unsigned char nWriteFlag, unsigned char nWriteValue, ULONG nCyclesLeft);

typedef struct _IMAGE__ {
  int unused;
} *HIMAGE;

enum eIRQSRC {
  IS_6522 = 0, IS_SPEECH, IS_SSC, IS_MOUSE
};

#define APPLE2E_MASK  0x10
#define APPLE2C_MASK  0x20

#define IS_APPLE2    ((g_Apple2Type & (APPLE2E_MASK|APPLE2C_MASK)) == 0)
#define IS_APPLE2E    (g_Apple2Type & APPLE2E_MASK)
#define IS_APPLE2C    (g_Apple2Type & APPLE2C_MASK)

// NB. These get persisted to the Registry, so don't change the values for these enums!
enum eApple2Type {
  A2TYPE_APPLE2 = 0,
  A2TYPE_APPLE2PLUS,
  A2TYPE_APPLE2E = APPLE2E_MASK,
  A2TYPE_APPLE2EEHANCED,
  A2TYPE_MAX
};

enum eBUTTON {
  BUTTON0 = 0, BUTTON1
};
enum eBUTTONSTATE {
  BUTTON_UP = 0, BUTTON_DOWN
};

// sizes of status panel
#define STATUS_PANEL_W    100
#define STATUS_PANEL_H    48


// This should be defined for Windows systems maybe?
// or we can just always use internal version...
#if( (!HAS_STRLCPY) || (!HAS_STRLCAT) )
#include <cstring>
#endif

#if(!HAS_STRLCPY)
extern std::size_t __l_strlcpy(char *, const char *, size_t);
# define _l_strcpy(d,s) __l_strlcpy((d),(s),strlen((s)))
#else
# define _l_strcpy(d,s) strlcpy((d),(s),strlen((s)))
#endif

#if(! HAS_STRLCAT)
extern std::size_t __l_strlcat(char *, const char *, size_t);
# define _l_strcat(d,s) __l_strlcat((d),(s),sizeof((d)))

#else
# define _l_strcat(d,s) strlcat((d),(s),sizeof((d)))
#endif
