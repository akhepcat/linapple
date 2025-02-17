/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski
Copyright (C) 2020, Thorsten Brehm

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

/* Description: Debugger
 *
 * Author: Copyright (C) 2006-2010 Michael Pohoreski
 */

#include "stdafx.h"

#include "Debug.h"
#include "Debugger_Console.h"
#include "Debugger_Display.h"
#include "Debugger_Color.h"
#include "Debugger_Assembler.h"
#include "Debugger_Parser.h"
#include "Util_Text.h"

#include "AppleWin.h"
#include "CPU.h"
#include "Frame.h"
//#include "LanguageCard.h"
#include "Memory.h"
#include "Mockingboard.h"
//#include "NTSC.h"
#include "Video.h"
#include <SDL_image.h>
#include "../../build/obj/charset40.xpm" // US/default

// NEW UI debugging - force display ALL meta-info (regs, stack, bp, watches, zp) for debugging purposes
#define DEBUG_FORCE_DISPLAY 0

#define SOFTSWITCH_OLD 0
#define SOFTSWITCH_LANGCARD 1

#if _DEBUG
	#define DEBUG_FONT_NO_BACKGROUND_CHAR      0
	#define DEBUG_FONT_NO_BACKGROUND_TEXT      0
	#define DEBUG_FONT_NO_BACKGROUND_FILL_CON  0
	#define DEBUG_FONT_NO_BACKGROUND_FILL_INFO 0
	#define DEBUG_FONT_NO_BACKGROUND_FILL_MAIN 0

	// no top console line
	#define DEBUG_BACKGROUND 0
#endif

	#define DISPLAY_MEMORY_TITLE     1
//	#define DISPLAY_BREAKPOINT_TITLE 1
//	#define DISPLAY_WATCH_TITLE      1


// Public _________________________________________________________________________________________

// Font
	FontConfig_t g_aFontConfig[ NUM_FONTS  ];

// Private ________________________________________________________________________________________

	char g_aDebuggerVirtualTextScreen[ DEBUG_VIRTUAL_TEXT_HEIGHT ][ DEBUG_VIRTUAL_TEXT_WIDTH ];

// HACK HACK HACK
	//g_nDisasmWinHeight
	WindowSplit_t *g_pDisplayWindow = 0; // HACK
// HACK

	// Display
	SDL_Surface *g_hDebugScreen;
	SDL_Surface *g_hDebugCharset;
	int g_hConsoleBrushFG = 0;
	int g_hConsoleBrushBG = 0;

// Disassembly
	/*
		// Thought about moving MouseText to another location, say high bit, 'A' + 0x80
		// But would like to keep compatibility with existing CHARSET40
		// Since we should be able to display all apple chars 0x00 .. 0xFF with minimal processing
		// Use CONSOLE_COLOR_ESCAPE_CHAR to shift to mouse text
		* Apple Font
		    K      Mouse Text Up Arror
		    H      Mouse Text Left Arrow
		    J      Mouse Text Down Arrow
		* Wingdings
			\xE1   Up Arrow
			\xE2   Down Arrow
		* Webdings // M$ Font
			\x35   Up Arrow
			\x33   Left Arrow  (\x71 recycl is too small to make out details)
			\x36   Down Arrow
		* Symols
			\xAD   Up Arrow
			\xAF   Down Arrow
		* ???
			\x18 Up
			\x19 Down
	*/
#if USE_APPLE_FONT
	char * g_sConfigBranchIndicatorUp   [ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "^", "\x8B" }; // "`K" 0x4B
	char * g_sConfigBranchIndicatorEqual[ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "=", "\x88" }; // "`H" 0x48
	char * g_sConfigBranchIndicatorDown [ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "v", "\x8A" }; // "`J" 0x4A
#else
	char * g_sConfigBranchIndicatorUp   [ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "^", "\x35" };
	char * g_sConfigBranchIndicatorEqual[ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "=", "\x33" };
	char * g_sConfigBranchIndicatorDown [ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "v", "\x36" };
#endif

// Drawing
	// Width
		const int DISPLAY_WIDTH  = 560;
		// New Font = 50.5 char * 7 px/char = 353.5
		const int DISPLAY_DISASM_RIGHT   = 353 ;

#if USE_APPLE_FONT
		// Horizontal Column (pixels) of Stack & Regs
		const int INFO_COL_1 = (51 * CONSOLE_FONT_WIDTH);
		const int DISPLAY_REGS_COLUMN       = INFO_COL_1;
		const int DISPLAY_FLAG_COLUMN       = INFO_COL_1;
		const int DISPLAY_STACK_COLUMN      = INFO_COL_1;
		const int DISPLAY_TARGETS_COLUMN    = INFO_COL_1;
		const int DISPLAY_ZEROPAGE_COLUMN   = INFO_COL_1;
		const int DISPLAY_SOFTSWITCH_COLUMN = INFO_COL_1 - (CONSOLE_FONT_WIDTH/2) + 1; // 1/2 char width padding around soft switches

		// Horizontal Column (pixels) of BPs, Watches
		const int INFO_COL_2 = (62 * 7); // nFontWidth
		const int DISPLAY_BP_COLUMN      = INFO_COL_2;
		const int DISPLAY_WATCHES_COLUMN = INFO_COL_2;

		// Horizontal Column (pixels) of VideoScannerInfo & Mem
		const int INFO_COL_3 = (63 * 7); // nFontWidth
		const int DISPLAY_MINIMEM_COLUMN = INFO_COL_3;
		const int DISPLAY_VIDEO_SCANNER_COLUMN = INFO_COL_3;
#else
		const int DISPLAY_CPU_INFO_LEFT_COLUMN = SCREENSPLIT1	// TC: SCREENSPLIT1 is not defined anywhere in the .sln!

		const int DISPLAY_REGS_COLUMN       = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_FLAG_COLUMN       = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_STACK_COLUMN      = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_TARGETS_COLUMN    = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_ZEROPAGE_COLUMN   = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_SOFTSWITCH_COLUMN = DISPLAY_CPU_INFO_LEFT_COLUMN - (CONSOLE_FONT_WIDTH/2);

		const int SCREENSPLIT2 = SCREENSPLIT1 + (12 * 7); // moved left 3 chars to show B. prefix in breakpoint #, W. prefix in watch #
		const int DISPLAY_BP_COLUMN      = SCREENSPLIT2;
		const int DISPLAY_WATCHES_COLUMN = SCREENSPLIT2;
		const int DISPLAY_MINIMEM_COLUMN = SCREENSPLIT2; // nFontWidth
		const int DISPLAY_VIDEO_SCANNER_COLUMN = SCREENSPLIT2;
#endif

		int MAX_DISPLAY_REGS_LINES        = 7;
		int MAX_DISPLAY_STACK_LINES       = 8;
		int MAX_DISPLAY_TARGET_PTR_LINES  = 2;
		int MAX_DISPLAY_ZEROPAGE_LINES    = 8;

//		int MAX_DISPLAY_BREAKPOINTS_LINES = 7; // 7
//		int MAX_DISPLAY_WATCHES_LINES     = 8; // 8
		int MAX_DISPLAY_MEMORY_LINES_1    = 4; // 4
		int MAX_DISPLAY_MEMORY_LINES_2    = 4; // 4 // 2
		int g_nDisplayMemoryLines;

		VideoScannerDisplayInfo g_videoScannerDisplayInfo;

static char ColorizeSpecialChar( char * sText, unsigned char nData, const MemoryView_e iView,
			const int iTxtBackground  = BG_INFO     , const int iTxtForeground  = FG_DISASM_CHAR,
			const int iHighBackground = BG_INFO_CHAR, const int iHighForeground = FG_INFO_CHAR_HI,
			const int iLowBackground  = BG_INFO_CHAR, const int iLowForeground  = FG_INFO_CHAR_LO );

	char  FormatCharTxtAsci ( const unsigned char b, bool * pWasAsci_ );

	void DrawSubWindow_Code ( int iWindow );
	void DrawSubWindow_IO       (Update_t bUpdate);
	void DrawSubWindow_Source   (Update_t bUpdate);
	void DrawSubWindow_Source2  (Update_t bUpdate);
	void DrawSubWindow_Symbols  (Update_t bUpdate);
	void DrawSubWindow_ZeroPage (Update_t bUpdate);

	void DrawWindowBottom ( Update_t bUpdate, int iWindow );

	char* FormatCharCopy( char *pDst, const char *pSrc, const int nLen );
	char  FormatCharTxtAsci( const unsigned char b, bool * pWasAsci_ = NULL );
	char  FormatCharTxtCtrl( const unsigned char b, bool * pWasCtrl_ = NULL );
	char  FormatCharTxtHigh( const unsigned char b, bool *pWasHi_ = NULL );
	char  FormatChar4Font  ( const unsigned char b, bool *pWasHi_, bool *pWasLo_ );

	void DrawRegister(int line, LPCTSTR name, int bytes, unsigned short value, int iSource = 0);

#define  SOFTSTRECH(SRC, SRC_X, SRC_Y, SRC_W, SRC_H, DST, DST_X, DST_Y, DST_W, DST_H) \
{ \
  srcrect.x = SRC_X; \
  srcrect.y = SRC_Y; \
  srcrect.w = SRC_W; \
  srcrect.h = SRC_H; \
  dstrect.x = DST_X; \
  dstrect.y = DST_Y; \
  dstrect.w = DST_W; \
  dstrect.h = DST_H; \
  SDL_SoftStretch(SRC, &srcrect, DST, &dstrect);\
}

#define  SOFTSTRECH_MONO(SRC, SRC_X, SRC_Y, SRC_W, SRC_H, DST, DST_X, DST_Y, DST_W, DST_H) \
{ \
  srcrect.x = SRC_X; \
  srcrect.y = SRC_Y; \
  srcrect.w = SRC_W; \
  srcrect.h = SRC_H; \
  dstrect.x = DST_X; \
  dstrect.y = DST_Y; \
  dstrect.w = DST_W; \
  dstrect.h = DST_H; \
  SDL_SoftStretchMono8(SRC, &srcrect, DST, &dstrect, hBrush, hBgBrush);\
}

//===========================================================================

void AllocateDebuggerMemDC(void)
{
  if (!g_hDebugScreen)
  {
    g_hDebugScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, 560, 384, 8, 0, 0, 0, 0);

    // character bitmap for IIe and enhanced
    SDL_Surface *tmp = IMG_ReadXPMFromArray(charset40_xpm);
    // convert format
    g_hDebugCharset = SDL_DisplayFormat(tmp);
    SDL_FreeSurface(tmp);

//    ZeroMemory(debugColors, sizeof(debugColors));
//    for (int i=1;i<256;i++)
//    {
//      debugColors[i].r=255;
//      debugColors[i].g=255;
//      debugColors[i].b=255;
//    }

    //SDL_Color *palette = screen->format->palette->colors;
    //palette = debugColors;

    //SDL_SetColors(g_hDebugScreen, palette, 0, 256);
    //SDL_SetColors(g_hDebugCharset, palette, 0, 256);
  }
}

void ReleaseDebuggerMemDC(void)
{
}

void GetDebugViewPortScale(float *x, float *y)
{
	float f = ((float) g_hDebugScreen->w) / screen->w;
	*x = (f>0.01) ? f : 0.01;
	f = ((float) g_hDebugScreen->h) / screen->h;
	*y = (f>0.01) ? f : 0.01;
}

void StretchBltMemToFrameDC(void)
{
	SDL_Rect drect, srect;

	pthread_mutex_lock(&video_draw_mutex);

	drect.x = drect.y = srect.x = srect.y = 0;
	drect.w = screen->w;
	drect.h = screen->h;
	srect.w = g_hDebugScreen->w;
	srect.h = g_hDebugScreen->h;

	SDL_SoftStretch(g_hDebugScreen, &srect, g_origscreen, &drect);
	SDL_BlitSurface(g_origscreen, NULL, screen, NULL);

	SDL_Flip(screen);

	pthread_mutex_unlock(&video_draw_mutex);
}

// Font: Apple Text
void DebuggerSetColorFG( unsigned int nRGB )
{
#ifndef _WIN32
	g_hConsoleBrushFG = nRGB;
#elif USE_APPLE_FONT
	g_hConsoleBrushFG = CreateSolidBrush( nRGB );
#else
	SetTextColor( GetDebuggerMemDC(), nRGB );
#endif
}

void DebuggerSetColorBG( unsigned int nRGB, bool bTransparent )
{
#ifndef _WIN32
	g_hConsoleBrushBG = nRGB;
#elif USE_APPLE_FONT
	if (! bTransparent)
	{
		g_hConsoleBrushBG = CreateSolidBrush( nRGB );
	}
#else
	SetBkColor( GetDebuggerMemDC(), nRGB );
#endif
}

#define CONSOLE_FONT_GRID_X  8
#define CONSOLE_FONT_GRID_Y  8
#define CONSOLE_FONT_WIDTH   7
#define CONSOLE_FONT_HEIGHT  8

//SDL_Color debugColors[256];
extern int g_hConsoleBrushFG;
extern int g_hConsoleBrushBG;

void FillRect(const RECT* r, int Brush)
{
	SDL_Rect sr;

	sr.x = r->left;
	sr.y = r->top;
	sr.w = r->right - sr.x;
	sr.h = r->bottom - sr.y;
	SDL_FillRect(g_hDebugScreen, &sr, Brush);
}

// @param glyph Specifies a native glyph from the 16x16 chars Apple Font Texture.
//===========================================================================
void PrintGlyph( const int x, const int y, const char glyph )
{
	char g = glyph;

	int ySrc = 64;

	if (glyph<32)
	{
		// mouse text
		g -= 32;
		ySrc = 48;
	}
	else
	if ((glyph >= '@')&&(glyph <= '_'))
	{
		g -= '@';
	}
	else
	if ((glyph >= ' ')&&(glyph <= '?'))
	{
		g += 32-' ';
	}
	else
	if ((glyph >= '`')&&(glyph <= 127))
	{
		g += 6*16 - '`';
	}

	// 16x8 chars in bitmap
	int xSrc = (g & 0x0F) * CONSOLE_FONT_GRID_X;
	ySrc += ((g >>   4) * CONSOLE_FONT_GRID_Y);

	// BUG #239 - (Debugger) Save debugger "text screen" to clipboard / file
	//	if( g_bDebuggerVirtualTextCapture )
	//
	{
#if _DEBUG
		if ((x < 0) || (y < 0))
			fprintf(stderr, "X or Y out of bounds: PrintGlyph(x,y)", x, y);
#endif
		int col = x / CONSOLE_FONT_WIDTH ;
		int row = y / CONSOLE_FONT_HEIGHT;

		// if( !g_bDebuggerCopyInfoPane )
		//    if( col < 50
		if (x > DISPLAY_DISASM_RIGHT) // INFO_COL_2 // DISPLAY_CPU_INFO_LEFT_COLUMN
			col++;

		if ((col < DEBUG_VIRTUAL_TEXT_WIDTH)
		&&  (row < DEBUG_VIRTUAL_TEXT_HEIGHT))
			g_aDebuggerVirtualTextScreen[ row ][ col ] = glyph;
	}

	SDL_Rect srcrect, dstrect;

	Uint8 hBrush = g_hConsoleBrushFG;
	Uint8 hBgBrush = g_hConsoleBrushBG;
	SOFTSTRECH_MONO(g_hDebugCharset, xSrc, ySrc, CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT,
		g_hDebugScreen, x, y, CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT);
}

//===========================================================================
void DebuggerPrint ( int x, int y, const char *pText )
{
	const int nLeft = x;

	char c;
	const char *p = pText;

	while ((c = *p))
	{
		if (c == '\n')
		{
			x = nLeft;
			y += CONSOLE_FONT_HEIGHT;
			p++;
			continue;
		}
		c &= 0x7F;
		PrintGlyph( x, y, c );
		x += CONSOLE_FONT_WIDTH;
		p++;
	}
}

//===========================================================================
void DebuggerPrintColor( int x, int y, const conchar_t * pText )
{
	int nLeft = x;

	conchar_t g;
	const conchar_t *pSrc = pText;

	if( !pText)
		return;

	while ((g = (*pSrc)))
	{
		if (g == '\n')
		{
			x = nLeft;
			y += CONSOLE_FONT_HEIGHT;
			pSrc++;
			continue;
		}

		if (ConsoleColor_IsColorOrMouse( g ))
		{
			if (ConsoleColor_IsColor( g ))
			{
				DebuggerSetColorFG( ConsoleColor_GetColor( g ) );
			}

			g = ConsoleChar_GetChar( g );
		}

		PrintGlyph( x, y, (char) (g & _CONSOLE_COLOR_MASK) );
		x += CONSOLE_FONT_WIDTH;
		pSrc++;
	}
}


// Utility ________________________________________________________________________________________


//===========================================================================
bool CanDrawDebugger()
{
#ifdef _WIN32
	if (DebugGetVideoMode(NULL))
		return false;
#endif

	return (g_nAppMode == MODE_DEBUG) || (g_nAppMode == MODE_STEPPING);
}


//===========================================================================
int PrintText ( const char * pText, RECT & rRect )
{
#if _DEBUG
	if (! pText)
		MessageBox( g_hFrameWindow, "pText = NULL!", "DrawText()", MB_OK );
#endif

	int nLen = strlen( pText );

#if !DEBUG_FONT_NO_BACKGROUND_TEXT
	FillRect(&rRect, g_hConsoleBrushBG );
#endif

	DebuggerPrint( rRect.left, rRect.top, pText );
	return nLen;
}

//===========================================================================
void PrintTextColor ( const conchar_t *pText, RECT & rRect )
{
#if !DEBUG_FONT_NO_BACKGROUND_TEXT
	FillRect(&rRect, g_hConsoleBrushBG );
#endif

	DebuggerPrintColor( rRect.left, rRect.top, pText );
}

// Updates the horizontal cursor
//===========================================================================
int PrintTextCursorX ( const char * pText, RECT & rRect )
{
	int nChars = 0;
	if (pText)
	{
		nChars = PrintText( pText, rRect );
		int nFontWidth = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;
		rRect.left += (nFontWidth * nChars);
	}
	return nChars;
}

//===========================================================================
int PrintTextCursorY ( const char * pText, RECT & rRect )
{
	int nChars = PrintText( pText, rRect );
	rRect.top    += g_nFontHeight;
	rRect.bottom += g_nFontHeight;
	return nChars;
}


//===========================================================================
char* FormatCharCopy( char *pDst, const char *pSrc, const int nLen )
{
	for( int i = 0; i < nLen; i++ )
		*pDst++ = FormatCharTxtCtrl( *pSrc++ );
	return pDst;
}

//===========================================================================
char  FormatCharTxtAsci ( const unsigned char b, bool * pWasAsci_ )
{
	if (pWasAsci_)
		*pWasAsci_ = false;

	char c = (b & 0x7F);
	if (b <= 0x7F)
	{
		if (pWasAsci_)
		{
			*pWasAsci_ = true;
		}
	}
	return c;
}

// Note: FormatCharTxtCtrl() and RemapChar()
//===========================================================================
char  FormatCharTxtCtrl ( const unsigned char b, bool * pWasCtrl_ )
{
	if (pWasCtrl_)
		*pWasCtrl_ = false;

	char c = (b & 0x7F); // .32 Changed: Lo now maps High Ascii to printable chars. i.e. ML1 D0D0
	if (b < 0x20) // SPACE
	{
		if (pWasCtrl_)
		{
			*pWasCtrl_ = true;
		}
		c = b + '@'; // map ctrl chars to visible
	}
	return c;
}

//===========================================================================
char  FormatCharTxtHigh ( const unsigned char b, bool *pWasHi_ )
{
	if (pWasHi_)
		*pWasHi_ = false;

	char c = b;
	if (b > 0x7F)
	{
		if (pWasHi_)
		{
			*pWasHi_ = true;
		}
		c = (b & 0x7F);
	}
	return c;
}


//===========================================================================
char FormatChar4Font ( const unsigned char b, bool *pWasHi_, bool *pWasLo_ )
{
	// Most Windows Fonts don't have (printable) glyphs for control chars
	unsigned char b1 = FormatCharTxtHigh( b , pWasHi_ );
	unsigned char b2 = FormatCharTxtCtrl( b1, pWasLo_ );
	return b2;
}




//===========================================================================
void SetupColorsHiLoBits ( bool bHighBit, bool bCtrlBit,
	const int iBackground, const int iForeground,
	const int iColorHiBG , const int iColorHiFG,
	const int iColorLoBG , const int iColorLoFG )
{
	// 4 cases:
	// Hi Lo Background Foreground -> just map Lo -> FG, Hi -> BG
	// 0  0  normal     normal     BG_INFO        FG_DISASM_CHAR   (dark cyan bright cyan)
	// 0  1  normal     LoFG       BG_INFO        FG_DISASM_OPCODE (dark cyan yellow)
	// 1  0  HiBG       normal     BG_INFO_CHAR   FG_DISASM_CHAR   (mid cyan  bright cyan)
	// 1  1  HiBG       LoFG       BG_INFO_CHAR   FG_DISASM_OPCODE (mid cyan  yellow)

	DebuggerSetColorBG( DebuggerGetColor( iBackground ));
	DebuggerSetColorFG( DebuggerGetColor( iForeground ));

	if (bHighBit)
	{
		DebuggerSetColorBG( DebuggerGetColor( iColorHiBG ));
		DebuggerSetColorFG( DebuggerGetColor( iColorHiFG )); // was iForeground
	}

	if (bCtrlBit)
	{
		DebuggerSetColorBG( DebuggerGetColor( iColorLoBG ));
		DebuggerSetColorFG( DebuggerGetColor( iColorLoFG ));
	}
}


// To flush out color bugs... swap: iAsciBackground & iHighBackground
//===========================================================================
char ColorizeSpecialChar( char * sText, unsigned char nData, const MemoryView_e iView,
		const int iAsciBackground /*= 0           */, const int iTextForeground /*= FG_DISASM_CHAR */,
		const int iHighBackground /*= BG_INFO_CHAR*/, const int iHighForeground /*= FG_INFO_CHAR_HI*/,
		const int iCtrlBackground /*= BG_INFO_CHAR*/, const int iCtrlForeground /*= FG_INFO_CHAR_LO*/ )
{
	bool bHighBit = false;
	bool bCtrlBit = false;

	int iTextBG = iAsciBackground;
	int iHighBG = iHighBackground;
	int iCtrlBG = iCtrlBackground;
	int iTextFG = iTextForeground;
	int iHighFG = iHighForeground;
	int iCtrlFG = iCtrlForeground;

	unsigned char nByte = FormatCharTxtHigh( nData, & bHighBit );
	char nChar = FormatCharTxtCtrl( nByte, & bCtrlBit );

	switch (iView)
	{
		case MEM_VIEW_ASCII:
			iHighBG = iTextBG;
			iCtrlBG = iTextBG;
			break;
		case MEM_VIEW_APPLE:
			iHighBG = iTextBG;
			if (!bHighBit)
			{
				iTextBG = iCtrlBG;
			}

			if (bCtrlBit)
			{
				iTextFG = iCtrlFG;
				if (bHighBit)
				{
					iHighFG = iTextFG;
				}
			}
			bCtrlBit = false;
			break;
		default: break;
	}

	if (sText)
		sprintf( sText, "%c", nChar );

#if OLD_CONSOLE_COLOR
	if (sText)
	{
		if (ConsoleColor_IsEscapeMeta( nChar ))
			sprintf( sText, "%c%c", nChar, nChar );
		else
			sprintf( sText, "%c", nChar );
	}
#endif

//	if (hDC)
	{
		SetupColorsHiLoBits( bHighBit, bCtrlBit
			, iTextBG, iTextFG // FG_DISASM_CHAR
			, iHighBG, iHighFG // BG_INFO_CHAR
			, iCtrlBG, iCtrlFG // FG_DISASM_OPCODE
		);
	}
	return nChar;
}

void ColorizeFlags( bool bSet, int bg_default = BG_INFO, int fg_default = FG_INFO_TITLE )
{
	if (bSet)
	{
		DebuggerSetColorBG( DebuggerGetColor( BG_INFO_INVERSE ));
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_INVERSE ));
	}
	else
	{
		DebuggerSetColorBG( DebuggerGetColor( bg_default ));
		DebuggerSetColorFG( DebuggerGetColor( fg_default ));
	}
}


// Main Windows ___________________________________________________________________________________


//===========================================================================
void DrawBreakpoints ( int line )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_BP_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	char sText[16] = "Breakpoints"; // TODO: Move to BP1

#if DISPLAY_BREAKPOINT_TITLE
	DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
	PrintText( sText, rect );
	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;
#endif

	int nBreakpointsDisplayed = 0;

	int iBreakpoint;
	for (iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++ )
	{
		Breakpoint_t *pBP = &g_aBreakpoints[iBreakpoint];
		unsigned int nLength = pBP->nLength;

#if DEBUG_FORCE_DISPLAY
		nLength = 2;
#endif
		if (nLength)
		{
			bool bSet      = pBP->bSet;
			bool bEnabled  = pBP->bEnabled;
			unsigned short nAddress1 = pBP->nAddress;
			unsigned short nAddress2 = nAddress1 + nLength - 1;

#if DEBUG_FORCE_DISPLAY
//			if (iBreakpoint < MAX_DISPLAY_BREAKPOINTS_LINES)
				bSet = true;
#endif
			if (! bSet)
				continue;

			nBreakpointsDisplayed++;

//			if (nBreakpointsDisplayed > MAX_DISPLAY_BREAKPOINTS_LINES)
//				break;

			RECT rect2;
			rect2 = rect;

			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ) );
			sprintf( sText, "B" );
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ) );
			sprintf( sText, "%X ", iBreakpoint );
			PrintTextCursorX( sText, rect2 );

//			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ) );
//			_l_strcpy( sText, "." );
//			PrintTextCursorX( sText, rect2 );

#if DEBUG_FORCE_DISPLAY
	pBP->eSource = (BreakpointSource_t) iBreakpoint;
#endif
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ) );
			int nRegLen = PrintTextCursorX( g_aBreakpointSource[ pBP->eSource ], rect2 );

			// Pad to 2 chars
			if (nRegLen < 2)
				rect2.left += g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ) );
#if DEBUG_FORCE_DISPLAY
	if (iBreakpoint < 3)
		pBP->eOperator = (BreakpointOperator_t)(iBreakpoint * 2);
//	else
//		pBP->eOperator = (BreakpointOperator_t)(iBreakpoint-3 + BP_OP_READ);
#endif
			PrintTextCursorX( g_aBreakpointSymbols [ pBP->eOperator ], rect2 );

			DebugColors_e iForeground;
			DebugColors_e iBackground = BG_INFO;

			if (bSet)
			{
				if (bEnabled)
				{
					iBackground = BG_DISASM_BP_S_C;
//					iForeground = FG_DISASM_BP_S_X;
					iForeground = FG_DISASM_BP_S_C;
				}
				else
				{
					iForeground = FG_DISASM_BP_0_X;
				}
			}
			else
			{
				iForeground = FG_INFO_TITLE;
			}

			DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
			DebuggerSetColorFG( DebuggerGetColor( iForeground ) );

#if DEBUG_FORCE_DISPLAY


	int iColor = R8 + iBreakpoint;
	unsigned int nColor = g_aColorPalette[ iColor ];
	if (iBreakpoint >= 8)
	{
		DebuggerSetColorBG( DebuggerGetColor( BG_DISASM_BP_S_C ) );
		nColor = DebuggerGetColor( FG_DISASM_BP_S_C );
	}
	DebuggerSetColorFG( nColor );
#endif

			sprintf( sText, "%04X", nAddress1 );
			PrintTextCursorX( sText, rect2 );

			if (nLength == 1)
			{
				if (pBP->eSource == BP_SRC_MEM_READ_ONLY)
					PrintTextCursorX("R", rect2);
				else if (pBP->eSource == BP_SRC_MEM_WRITE_ONLY)
					PrintTextCursorX("W", rect2);
			}

			if (nLength > 1)
			{
				DebuggerSetColorBG( DebuggerGetColor( BG_INFO ) );
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ) );

//				if (g_bConfigDisasmOpcodeSpaces)
//				{
//					PrintTextCursorX( " ", rect2 );
//					rect2.left += g_nFontWidthAvg;
//				}

				PrintTextCursorX( ":", rect2 );
//				rect2.left += g_nFontWidthAvg;
//				if (g_bConfigDisasmOpcodeSpaces) // TODO: Might have to remove spaces, for BPIO... addr-addr xx
//				{
//					rect2.left += g_nFontWidthAvg;
//				}

				DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
				DebuggerSetColorFG( DebuggerGetColor( iForeground ) );
#if DEBUG_FORCE_DISPLAY
	unsigned int nColor = g_aColorPalette[ iColor ];
	if (iBreakpoint >= 8)
	{
		nColor = DebuggerGetColor( BG_INFO );
		DebuggerSetColorBG( nColor );
		nColor = DebuggerGetColor( FG_DISASM_BP_S_X );
	}
	DebuggerSetColorFG( nColor );
#endif
				sprintf( sText, "%04X", nAddress2 );
				PrintTextCursorX( sText, rect2 );

				if (pBP->eSource == BP_SRC_MEM_READ_ONLY)
					PrintTextCursorX("R", rect2);
				else if (pBP->eSource == BP_SRC_MEM_WRITE_ONLY)
					PrintTextCursorX("W", rect2);
			}

#if !USE_APPLE_FONT
			// Windows HACK: Bugfix: Rest of line is still breakpoint background color
			DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
			PrintTextCursorX( " ", rect2 );
#endif
			rect.top    += g_nFontHeight;
			rect.bottom += g_nFontHeight;
		}
	}
}


// Console ________________________________________________________________________________________


//===========================================================================
int GetConsoleLineHeightPixels()
{
	int nHeight = g_aFontConfig[ FONT_CONSOLE ]._nFontHeight; // _nLineHeight; // _nFontHeight;
/*
	if (g_iFontSpacing == FONT_SPACING_CLASSIC)
	{
		nHeight++;  // "Classic" Height/Spacing
	}
	else
	if (g_iFontSpacing == FONT_SPACING_CLEAN)
	{
		nHeight++;
	}
	else
	if (g_iFontSpacing == FONT_SPACING_COMPRESSED)
	{
		// default case handled
	}
*/
	return nHeight;
}

//===========================================================================
int GetConsoleTopPixels( int y )
{
	int nLineHeight = GetConsoleLineHeightPixels();
	int nTop = DISPLAY_HEIGHT - ((y + 1) * nLineHeight);
	return nTop;
}

//===========================================================================
void DrawConsoleCursor ()
{
	DebuggerSetColorFG( DebuggerGetColor( FG_CONSOLE_INPUT ));
	DebuggerSetColorBG( DebuggerGetColor( BG_CONSOLE_INPUT ));

	int nWidth = g_aFontConfig[ FONT_CONSOLE ]._nFontWidthAvg;

	int nLineHeight = GetConsoleLineHeightPixels();
	int y = 0;

	RECT rect;
	rect.left   = (g_nConsoleInputChars + g_nConsolePromptLen) * nWidth;
	rect.top    = GetConsoleTopPixels( y );
	rect.bottom = rect.top + nLineHeight; //g_nFontHeight;
	rect.right  = rect.left + nWidth;

	PrintText( g_sConsoleCursor, rect );
}

//===========================================================================
void GetConsoleRect ( const int y, RECT & rect )
{
	int nLineHeight = GetConsoleLineHeightPixels();

	rect.left   = 0;
	rect.top    = GetConsoleTopPixels( y );
	rect.bottom = rect.top + nLineHeight; //g_nFontHeight;

//	int nHeight = WindowGetHeight( g_iWindowThis );

	int nFontWidth = g_aFontConfig[ FONT_CONSOLE ]._nFontWidthAvg;
	int nMiniConsoleRight = g_nConsoleDisplayWidth * nFontWidth;
	int nFullConsoleRight = DISPLAY_WIDTH;
	int nRight = g_bConsoleFullWidth ? nFullConsoleRight : nMiniConsoleRight;
	rect.right = nRight;
}

//===========================================================================
void DrawConsoleLine ( const conchar_t * pText, int y )
{
	if (y < 0)
		return;

	RECT rect;
	GetConsoleRect( y, rect );

	// Console background is drawn in DrawWindowBackground_Info
	PrintTextColor( pText, rect );
}

//===========================================================================
void DrawConsoleInput ()
{
	DebuggerSetColorFG( DebuggerGetColor( FG_CONSOLE_INPUT ));
	DebuggerSetColorBG( DebuggerGetColor( BG_CONSOLE_INPUT ));

	RECT rect;
	GetConsoleRect( 0, rect );

	// Console background is drawn in DrawWindowBackground_Info
//	DrawConsoleLine( g_aConsoleInput, 0 );
	PrintText( g_aConsoleInput, rect );
}


// Disassembly ____________________________________________________________________________________

void GetTargets_IgnoreDirectJSRJMP(const unsigned char iOpcode, int& nTargetPointer)
{
	if (iOpcode == OPCODE_JSR || iOpcode == OPCODE_JMP_A)
		nTargetPointer = NO_6502_TARGET;
}

// Get the data needed to disassemble one line of opcodes. Fills in the DisasmLine info.
// Disassembly formatting flags returned
//	@parama sTargetValue_ indirect/indexed final value
//===========================================================================
int GetDisassemblyLine ( unsigned short nBaseAddress, DisasmLine_t & line_ )
//	char *sAddress_, char *sOpCodes_,
//	char *sTarget_, char *sTargetOffset_, int & nTargetOffset_,
//	char *sTargetPointer_, char *sTargetValue_,
//	char *sImmediate_, char & nImmediate_, char *sBranch_ )
{
	line_.Clear();

	int iOpcode;
	int iOpmode;
	int nOpbyte;

	iOpcode = _6502_GetOpmodeOpbyte( nBaseAddress, iOpmode, nOpbyte, &line_.pDisasmData );
	const DisasmData_t* pData = line_.pDisasmData; // Disassembly_IsDataAddress( nBaseAddress );

	line_.iOpcode = iOpcode;
	line_.iOpmode = iOpmode;
	line_.nOpbyte = nOpbyte;

#if _DEBUG
//	if (iLine != 41)
//		return nOpbytes;
#endif

	if (iOpmode == AM_M)
		line_.bTargetImmediate = true;

	if ((iOpmode >= AM_IZX) && (iOpmode <= AM_NA))
		line_.bTargetIndirect = true; // ()

	if ((iOpmode >= AM_IZX) && (iOpmode <= AM_NZY))
		line_.bTargetIndexed = true; // ()

	if (((iOpmode >= AM_A) && (iOpmode <= AM_ZY)) || line_.bTargetIndirect)
		line_.bTargetValue = true; // #$

	if ((iOpmode == AM_AX) || (iOpmode == AM_ZX) || (iOpmode == AM_IZX) || (iOpmode == AM_IAX))
		line_.bTargetX = true; // ,X

	if ((iOpmode == AM_AY) || (iOpmode == AM_ZY) || (iOpmode == AM_NZY))
		line_.bTargetY = true; // ,Y

	unsigned int nMinBytesLen = (MAX_OPCODES * (2 + g_bConfigDisasmOpcodeSpaces)); // 2 char for byte (or 3 with space)

	int bDisasmFormatFlags = 0;

	// Composite string that has the symbol or target nAddress
	unsigned short nTarget = 0;

	if ((iOpmode != AM_IMPLIED) &&
		(iOpmode != AM_1) &&
		(iOpmode != AM_2) &&
		(iOpmode != AM_3))
	{
		// Assume target address starts after the opcode ...
		// BUT in the Assembler Directive / Data Disassembler case for define addr/word
		// the opcode literally IS the target address!
		if( pData )
		{
			nTarget = pData->nTargetAddress;
		} else {
			nTarget = *(LPWORD)(mem+nBaseAddress+1);
			if (nOpbyte == 2)
				nTarget &= 0xFF;
		}

		if (iOpmode == AM_R) // Relative
		{
			line_.bTargetRelative = true;

			nTarget = nBaseAddress+2+(int)(signed char)nTarget;

			line_.nTarget = nTarget;
			sprintf( line_.sTargetValue, "%04X", nTarget & 0xFFFF );

			// Always show branch indicators
			bDisasmFormatFlags |= DISASM_FORMAT_BRANCH;

			if (nTarget < nBaseAddress)
			{
				sprintf( line_.sBranch, "%s", g_sConfigBranchIndicatorUp[ g_iConfigDisasmBranchType ] );
			}
			else
			if (nTarget > nBaseAddress)
			{
				sprintf( line_.sBranch, "%s", g_sConfigBranchIndicatorDown[ g_iConfigDisasmBranchType ] );
			}
			else
			{
				sprintf( line_.sBranch, "%s", g_sConfigBranchIndicatorEqual[ g_iConfigDisasmBranchType ] );
			}
		}
		// intentional re-test AM_R ...

//		if ((iOpmode >= AM_A) && (iOpmode <= AM_NA))
		if ((iOpmode == AM_A  ) || // Absolute
			(iOpmode == AM_Z  ) || // Zeropage
			(iOpmode == AM_AX ) || // Absolute, X
			(iOpmode == AM_AY ) || // Absolute, Y
			(iOpmode == AM_ZX ) || // Zeropage, X
			(iOpmode == AM_ZY ) || // Zeropage, Y
			(iOpmode == AM_R  ) || // Relative
			(iOpmode == AM_IZX) || // Indexed (Zeropage Indirect, X)
			(iOpmode == AM_IAX) || // Indexed (Absolute Indirect, X)
			(iOpmode == AM_NZY) || // Indirect (Zeropage) Index, Y
			(iOpmode == AM_NZ ) || // Indirect (Zeropage)
			(iOpmode == AM_NA ))   //(Indirect Absolute)
		{
			line_.nTarget  = nTarget;

			const char* pTarget = NULL;
			const char* pSymbol = 0;

			pSymbol = FindSymbolFromAddress( nTarget );

			// Data Assembler
			if (pData && (!pData->bSymbolLookup))
				pSymbol = 0;

			// Try exact match first
			if (pSymbol)
			{
				bDisasmFormatFlags |= DISASM_FORMAT_SYMBOL;
				pTarget = pSymbol;
			}

			if (! (bDisasmFormatFlags & DISASM_FORMAT_SYMBOL))
			{
				pSymbol = FindSymbolFromAddress( nTarget - 1 );
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_FORMAT_SYMBOL;
					bDisasmFormatFlags |= DISASM_FORMAT_OFFSET;
					pTarget = pSymbol;
					line_.nTargetOffset = +1; // U FA82   LDA $3F1 BREAK-1
				}
			}

			// Old Offset search: (Search +1 First) nTarget-1, (Search -1 Second) nTarget+1
			//    Problem: U D038 shows as A.TRACE+1
			// New Offset search: (Search -1 First) nTarget+1, (Search +1 Second) nTarget+1
			//    Problem: U D834, D87E shows as P.MUL-1, instead of P.ADD+1
			// 2.6.2.31 Fixed: address table was bailing on first possible match. U D000 -> da STOP+1, instead of END-1
			// 2.7.0.0: Try to match nTarget-1, nTarget+1, AND if we have both matches
			// Then we need to decide which one to show. If we have pData then pick this one.
			// TODO: Do we need to let the user decide which one they want searched first?
			//    nFirstTarget = g_bDebugConfig_DisasmMatchSymbolOffsetMinus1First ? nTarget-1 : nTarget+1;
			//    nSecondTarget = g_bDebugConfig_DisasmMatchSymbolOffsetMinus1First ? nTarget+1 : nTarget-1;
			if (! (bDisasmFormatFlags & DISASM_FORMAT_SYMBOL) || pData)
			{
				pSymbol = FindSymbolFromAddress( nTarget + 1 );
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_FORMAT_SYMBOL;
					bDisasmFormatFlags |= DISASM_FORMAT_OFFSET;
					pTarget = pSymbol;
					line_.nTargetOffset = -1; // U FA82 LDA $3F3 BREAK+1
				}
			}

			if (! (bDisasmFormatFlags & DISASM_FORMAT_SYMBOL))
			{
				pTarget = FormatAddress( nTarget, (iOpmode != AM_R) ? nOpbyte : 3 );	// GH#587: For Bcc opcodes, pretend it's a 3-byte opcode to print a 16-bit target addr
			}

//			sprintf( sTarget, g_aOpmodes[ iOpmode ]._sFormat, pTarget );
			if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
			{
				int nAbsTargetOffset =  (line_.nTargetOffset > 0) ? line_.nTargetOffset : - line_.nTargetOffset;
				snprintf( line_.sTargetOffset, sizeof(line_.sTargetOffset), "%d", nAbsTargetOffset );
			}
			snprintf( line_.sTarget, sizeof(line_.sTarget), "%s", pTarget );


		// Indirect / Indexed
			int nTargetPartial;
			int nTargetPartial2;
			int nTargetPointer;
			unsigned short nTargetValue = 0; // de-ref
			_6502_GetTargets( nBaseAddress, &nTargetPartial, &nTargetPartial2, &nTargetPointer, NULL );
			GetTargets_IgnoreDirectJSRJMP(iOpcode, nTargetPointer);	// For *direct* JSR/JMP, don't show 'addr16:byte char'

			if (nTargetPointer != NO_6502_TARGET)
			{
				bDisasmFormatFlags |= DISASM_FORMAT_TARGET_POINTER;

				nTargetValue = *(mem + nTargetPointer) | (*(mem + ((nTargetPointer + 1) & 0xffff)) << 8);

//				if (((iOpmode >= AM_A) && (iOpmode <= AM_NZ)) && (iOpmode != AM_R))
//					sprintf( sTargetValue_, "%04X", nTargetValue ); // & 0xFFFF

				if (g_iConfigDisasmTargets & DISASM_TARGET_ADDR)
					sprintf( line_.sTargetPointer, "%04X", nTargetPointer & 0xFFFF );

				if (iOpcode != OPCODE_JMP_NA && iOpcode != OPCODE_JMP_IAX)
				{
					bDisasmFormatFlags |= DISASM_FORMAT_TARGET_VALUE;
					if (g_iConfigDisasmTargets & DISASM_TARGET_VAL)
						sprintf( line_.sTargetValue, "%02X", nTargetValue & 0xFF );

					bDisasmFormatFlags |= DISASM_FORMAT_CHAR;
					line_.nImmediate = (unsigned char) nTargetValue;

					unsigned _char = FormatCharTxtCtrl( FormatCharTxtHigh( line_.nImmediate, NULL ), NULL );
					sprintf( line_.sImmediate, "%c", _char );

//					if (ConsoleColorIsEscapeMeta( nImmediate_ ))
#if OLD_CONSOLE_COLOR
					if (ConsoleColorIsEscapeMeta( _char ))
						sprintf( line_.sImmediate, "%c%c", _char, _char );
					else
						sprintf( line_.sImmediate, "%c", _char );
#endif
				}

//				if (iOpmode == AM_NA ) // Indirect Absolute
//					sprintf( sTargetValue_, "%04X", nTargetPointer & 0xFFFF );
//				else
// //					sprintf( sTargetValue_, "%02X", nTargetValue & 0xFF );
//					sprintf( sTargetValue_, "%04X:%02X", nTargetPointer & 0xFFFF, nTargetValue & 0xFF );
			}
		}
		else
		if (iOpmode == AM_M)
		{
//			sprintf( sTarget, g_aOpmodes[ iOpmode ]._sFormat, (unsigned)nTarget );
			sprintf( line_.sTarget, "%02X", (unsigned)nTarget );

			if (iOpmode == AM_M)
			{
				bDisasmFormatFlags |= DISASM_FORMAT_CHAR;
				line_.nImmediate = (unsigned char) nTarget;
				unsigned _char = FormatCharTxtCtrl( FormatCharTxtHigh( line_.nImmediate, NULL ), NULL );

				sprintf( line_.sImmediate, "%c", _char );
#if OLD_CONSOLE_COLOR
				if (ConsoleColorIsEscapeMeta( _char ))
					sprintf( line_.sImmediate, "%c%c", _char, _char );
				else
					sprintf( line_.sImmediate, "%c", _char );
#endif
			}
		}
	}

	sprintf( line_.sAddress, "%04X", nBaseAddress );

	// Opcode Bytes
	FormatOpcodeBytes( nBaseAddress, line_ );

// Data Disassembler
	if( pData )
	{
		line_.iNoptype = pData->eElementType;
		line_.iNopcode = pData->iDirective;
		_l_strcpy( line_.sMnemonic, g_aAssemblerDirectives[ line_.iNopcode ].m_pMnemonic );

		FormatNopcodeBytes( nBaseAddress, line_ );
	} else { // Regular 6502/65C02 opcode -> mnemonic
		_l_strcpy( line_.sMnemonic, g_aOpcodes[ line_.iOpcode ].sMnemonic );
	}

	int nSpaces = strlen( line_.sOpCodes );
    while (nSpaces < (int)nMinBytesLen)
	{
		_l_strcat( line_.sOpCodes, " " );
		nSpaces++;
	}

	return bDisasmFormatFlags;
}


const char* FormatAddress( unsigned short nAddress, int nBytes )
{
	// There is no symbol for this nAddress
	static char sSymbol[8] = TEXT("");
	switch (nBytes)
	{
		case  2:	snprintf(sSymbol, 8, TEXT("$%02X"),(unsigned)nAddress);  break;
		case  3:	snprintf(sSymbol, 8, TEXT("$%04X"),(unsigned)nAddress);  break;
		// TODO: FIXME: Can we get called with nBytes == 16 ??
		default:	sSymbol[0] = 0; break; // clear since is static
	}
	return sSymbol;
}

void FormatOpcodeBytes ( unsigned short nBaseAddress, DisasmLine_t & line_ )
{
	int nOpbyte = line_.nOpbyte;

	char *pDst = line_.sOpCodes;
	int nMaxOpBytes = nOpbyte;
	if ( nMaxOpBytes > MAX_OPCODES) // 2.8.0.0 fix // TODO: FIX: show max 8 bytes for HEX
		nMaxOpBytes = MAX_OPCODES;

	for( int iByte = 0; iByte < nMaxOpBytes; iByte++ )
	{
		unsigned char nMem = (unsigned)*(mem+nBaseAddress + iByte);
		sprintf( pDst, "%02X", nMem ); // sBytes+strlen(sBytes)
		pDst += 2;

		// TODO: If Disassembly_IsDataAddress() don't show spaces...
		if (g_bConfigDisasmOpcodeSpaces)
		{
			_l_strcat( pDst, " " );
			pDst++; // 2.5.3.3 fix
		}
	}
}

// Formats Target string with bytes,words, string, etc...
//===========================================================================
void FormatNopcodeBytes ( unsigned short nBaseAddress, DisasmLine_t & line_ )
{
		char *pDst = line_.sTarget;
		const	char *pSrc = 0;
		unsigned int nStartAddress = line_.pDisasmData->nStartAddress;
		unsigned int nEndAddress   = line_.pDisasmData->nEndAddress  ;
		int   nDisplayLen   = nEndAddress - nBaseAddress  + 1 ; // *inclusive* KEEP IN SYNC: _CmdDefineByteRange() CmdDisasmDataList() _6502_GetOpmodeOpbyte() FormatNopcodeBytes()
		int   len           = nDisplayLen;

	for( int iByte = 0; iByte < line_.nOpbyte; )
	{
		unsigned char nTarget8  = *(LPBYTE)(mem + nBaseAddress + iByte);
		unsigned short nTarget16 = *(LPWORD)(mem + nBaseAddress + iByte);

		switch( line_.iNoptype )
		{
			case NOP_BYTE_1:
			case NOP_BYTE_2:
			case NOP_BYTE_4:
			case NOP_BYTE_8:
				sprintf( pDst, "%02X", nTarget8 ); // sBytes+strlen(sBytes)
				pDst += 2;
				iByte++;
				if( line_.iNoptype == NOP_BYTE_1)
					if( iByte < line_.nOpbyte )
					{
						*pDst++ = ',';
					}
				break;
			case NOP_WORD_1:
			case NOP_WORD_2:
			case NOP_WORD_4:
				sprintf( pDst, "%04X", nTarget16 ); // sBytes+strlen(sBytes)
				pDst += 4;
				iByte+= 2;
				if( iByte < line_.nOpbyte )
				{
					*pDst++ = ',';
				}
				break;
			case NOP_ADDRESS:
				// Nothing to do, already handled :-)
				iByte += 2;
				break;
			case NOP_STRING_APPLESOFT:
				iByte = line_.nOpbyte;
				strncpy( pDst, (const char*)(mem + nBaseAddress), iByte );
				pDst += iByte;
				*pDst = 0;
			case NOP_STRING_APPLE:
				iByte = line_.nOpbyte; // handle all bytes of text
				pSrc = (const char*)mem + nStartAddress;

				if (len > (MAX_IMMEDIATE_LEN - 2)) // does "text" fit?
				{
					if (len > MAX_IMMEDIATE_LEN) // no; need extra characters for ellipsis?
						len = (MAX_IMMEDIATE_LEN - 3); // ellipsis = true

					// DISPLAY: text_longer_18...
					FormatCharCopy( pDst, pSrc, len ); // BUG: #251 v2.8.0.7: ASC #:# with null byte doesn't mark up properly

					if( nDisplayLen > len ) // ellipsis
					{
						*pDst++ = '.';
						*pDst++ = '.';
						*pDst++ = '.';
					}
				} else { // DISPLAY: "max_18_char"
					*pDst++ = '"';
					pDst = FormatCharCopy( pDst, pSrc, len ); // BUG: #251 v2.8.0.7: ASC #:# with null byte doesn't mark up properly
					*pDst++ = '"';
				}

				*pDst = 0;
				break;
			default:
#if _DEBUG // Unhandled data disassembly!
	int *FATAL = 0;
	*FATAL = 0xDEADC0DE;
#endif
				iByte++;
				break;
		}
	}
}

//===========================================================================
void FormatDisassemblyLine( const DisasmLine_t & line, char * sDisassembly, const int nBufferSize )
{
	//> Address Seperator Opcodes   Label Mnemonic Target [Immediate] [Branch]
	//
	// Data Disassembler
	//                              Label Directive       [Immediate]
	const char * pMnemonic = g_aOpcodes[ line.iOpcode ].sMnemonic;

	sprintf( sDisassembly, "%s:%s %s "
		, line.sAddress
		, line.sOpCodes
		, pMnemonic
	);

/*
	if (line.bTargetIndexed || line.bTargetIndirect)
	{
		_l_strcat( sDisassembly, "(" );
	}

	if (line.bTargetImmediate)
		_l_strcat( sDisassembly, "#$" );

	if (line.bTargetValue)
		_l_strcat( sDisassembly, line.sTarget );

	if (line.bTargetIndirect)
	{
		if (line.bTargetX)
	 		_l_strcat( sDisassembly, ", X" );
		if (line.bTargetY)
 			_l_strcat( sDisassembly, ", Y" );
	}

	if (line.bTargetIndexed || line.bTargetIndirect)
	{
		_l_strcat( sDisassembly, ")" );
	}

	if (line.bTargetIndirect)
	{
		if (line.bTargetY)
 			_l_strcat( sDisassembly, ", Y" );
	}
*/
	char sTarget[ 32 ];

	if (line.bTargetValue || line.bTargetRelative || line.bTargetImmediate)
	{
		if (line.bTargetRelative)
			_l_strcpy( sTarget, line.sTargetValue );
		else
		if (line.bTargetImmediate)
		{
			_l_strcat( sDisassembly, "#" );
			strncpy( sTarget, line.sTarget, sizeof(sTarget) );
			sTarget[sizeof(sTarget)-1] = 0;
		}
		else
			sprintf( sTarget, g_aOpmodes[ line.iOpmode ].m_sFormat, line.nTarget );

		_l_strcat( sDisassembly, "$" );
		_l_strcat( sDisassembly, sTarget );
	}
}

//===========================================================================
unsigned short DrawDisassemblyLine ( int iLine, const unsigned short nBaseAddress )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return 0;

	int iOpmode;
	int nOpbyte;
	DisasmLine_t line;
	const char* pSymbol = FindSymbolFromAddress( nBaseAddress );
	const char* pMnemonic = NULL;

	// Data Disassembler
	int bDisasmFormatFlags = GetDisassemblyLine( nBaseAddress, line );
	const DisasmData_t *pData = line.pDisasmData;

	iOpmode = line.iOpmode;
	nOpbyte = line.nOpbyte;

	// sAddress, sOpcodes, sTarget, sTargetOffset, nTargetOffset, sTargetPointer, sTargetValue, sImmediate, nImmediate, sBranch );
	//> Address Seperator Opcodes   Label Mnemonic Target [Immediate] [Branch]
	//
	//> xxxx: xx xx xx   LABEL    MNEMONIC    'E' =
	//>       ^          ^        ^           ^   ^
	//>       6          17       27          41  46
	const int nDefaultFontWidth = 7; // g_aFontConfig[FONT_DISASM_DEFAULT]._nFontWidth or g_nFontWidthAvg

	enum TabStop_e
	{
		  TS_OPCODE
		, TS_LABEL
		, TS_INSTRUCTION
		, TS_IMMEDIATE
		, TS_BRANCH
		, TS_CHAR
		, _NUM_TAB_STOPS
	};

//	int X_OPCODE      =  6 * nDefaultFontWidth;
//	int X_LABEL       = 17 * nDefaultFontWidth;
//	int X_INSTRUCTION = 26 * nDefaultFontWidth; // 27
//	int X_IMMEDIATE   = 40 * nDefaultFontWidth; // 41
//	int X_BRANCH      = 46 * nDefaultFontWidth;

	float aTabs[ _NUM_TAB_STOPS ] =
//	{ 6, 16, 26, 41, 46, 49 }; // 6, 17, 26, 40, 46
#if USE_APPLE_FONT
//	{ 5, 14, 20, 40, 46, 49 };
      // xxxx:xx xx xx LABELxxxxxx MNEMONIC    'E' =
      // 0   45        14          26
	{ 5, 14, 26, 41, 48, 49 };
#else
	{ 5.75, 15.5, 25, 40.5, 45.5, 48.5 };
#endif

#if !USE_APPLE_FONT
	if (! g_bConfigDisasmAddressColon)
	{
		aTabs[ TS_OPCODE ] -= 1;
	}

	if ((g_bConfigDisasmOpcodesView) && (! g_bConfigDisasmOpcodeSpaces))
	{
		aTabs[ TS_LABEL       ] -= 3;
		aTabs[ TS_INSTRUCTION ] -= 2;
		aTabs[ TS_IMMEDIATE   ] -= 1;
	}
#endif

	int iTab = 0;
	int nSpacer = 11; // 9
	for (iTab = 0; iTab < _NUM_TAB_STOPS; iTab++ )
	{
		if (! g_bConfigDisasmAddressView )
		{
			if (iTab < TS_IMMEDIATE) // TS_BRANCH)
			{
				aTabs[ iTab ] -= 4;
			}
		}
		if (! g_bConfigDisasmOpcodesView)
		{
			if (iTab < TS_IMMEDIATE) // TS_BRANCH)
			{
				aTabs[ iTab ] -= nSpacer;
				if (nSpacer > 0)
					nSpacer -= 2;
			}
		}

		aTabs[ iTab ] *= nDefaultFontWidth;
	}

	int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight; // _nFontHeight; // g_nFontHeight

	RECT linerect;
	linerect.left   = 0;
	linerect.top    = iLine * nFontHeight;
	linerect.right  = DISPLAY_DISASM_RIGHT;
	linerect.bottom = linerect.top + nFontHeight;

	bool bBreakpointActive;
	bool bBreakpointEnable;
	GetBreakpointInfo( nBaseAddress, bBreakpointActive, bBreakpointEnable );
	bool bAddressAtPC = (nBaseAddress == regs.pc);
	bool bAddressIsBookmark = Bookmark_Find( nBaseAddress );

	DebugColors_e iBackground = BG_DISASM_1;
	DebugColors_e iForeground = FG_DISASM_MNEMONIC; // FG_DISASM_TEXT;
	bool bCursorLine = false;

	if (((! g_bDisasmCurBad) && (iLine == g_nDisasmCurLine))
		|| (g_bDisasmCurBad && (iLine == 0)))
	{
		bCursorLine = true;

		// Breakpoint
		if (bBreakpointActive)
		{
			if (bBreakpointEnable)
			{
				iBackground = BG_DISASM_BP_S_C; iForeground = FG_DISASM_BP_S_C;
			}
			else
			{
				iBackground = BG_DISASM_BP_0_C; iForeground = FG_DISASM_BP_0_C;
			}
		}
		else
		if (bAddressAtPC)
		{
			iBackground = BG_DISASM_PC_C; iForeground = FG_DISASM_PC_C;
		}
		else
		{
			iBackground = BG_DISASM_C; iForeground = FG_DISASM_C;
			// HACK?  Sync Cursor back up to Address
			// The cursor line may of had to be been moved, due to Disasm Singularity.
			g_nDisasmCurAddress = nBaseAddress;
		}
	}
	else
	{
		if (iLine & 1)
		{
			iBackground = BG_DISASM_1;
		}
		else
		{
			iBackground = BG_DISASM_2;
		}

		// This address has a breakpoint, but the cursor is not on it (atm)
		if (bBreakpointActive)
		{
			if (bBreakpointEnable)
			{
				iForeground = FG_DISASM_BP_S_X; // Red (old Yellow)
			}
			else
			{
				iForeground = FG_DISASM_BP_0_X; // Yellow
			}
		}
		else
		if (bAddressAtPC)
		{
			iBackground = BG_DISASM_PC_X; iForeground = FG_DISASM_PC_X;
		}
		else
		{
			iForeground = FG_DISASM_MNEMONIC;
		}
	}

	if (bAddressIsBookmark)
	{
		DebuggerSetColorBG( DebuggerGetColor( BG_DISASM_BOOKMARK ) );
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_BOOKMARK ) );
	}
	else
	{
		DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
		DebuggerSetColorFG( DebuggerGetColor( iForeground ) );
	}

	// Address
	if (! bCursorLine)
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ) );
//	else
//	{
//		DebuggerSetColorBG( GetDebuggerMemDC(), DebuggerGetColor( FG_DISASM_BOOKMARK ) ); // swapped
//		DebuggerSetColorFG( GetDebuggerMemDC(), DebuggerGetColor( BG_DISASM_BOOKMARK ) ); // swapped
//	}

	if( g_bConfigDisasmAddressView )
	{
		PrintTextCursorX( (LPCTSTR) line.sAddress, linerect );
	}

	if (bAddressIsBookmark)
	{
		DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
		DebuggerSetColorFG( DebuggerGetColor( iForeground ) );
	}

	// Address Seperator
	if (! bCursorLine)
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );

	if (g_bConfigDisasmAddressColon)
		PrintTextCursorX( ":", linerect );
	else
		PrintTextCursorX( " ", linerect ); // bugfix, not showing "addr:" doesn't alternate color lines

	// Opcodes
	linerect.left = (int) aTabs[ TS_OPCODE ];

	if (! bCursorLine)
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE ) );

	if (g_bConfigDisasmOpcodesView)
		PrintTextCursorX( (LPCTSTR) line.sOpCodes, linerect );

	// Label
	linerect.left = (int) aTabs[ TS_LABEL ];

	if (pSymbol)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SYMBOL ) );
		PrintTextCursorX( pSymbol, linerect );
	}

	// Instruction / Mnemonic
	linerect.left = (int) aTabs[ TS_INSTRUCTION ];

	if (! bCursorLine)
	{
		if( pData ) // Assembler Data Directive / Data Disassembler
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_DIRECTIVE ) ); // TODO: FIXME: HACK? Is the color fine?
		else
			DebuggerSetColorFG( DebuggerGetColor( iForeground ) );
	}

	pMnemonic = line.sMnemonic;
	PrintTextCursorX( pMnemonic, linerect );
	PrintTextCursorX( " ", linerect );

	// Target
	if (line.bTargetImmediate)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "#$", linerect );
	}

	if (line.bTargetIndexed || line.bTargetIndirect)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "(", linerect );
	}

	char *pTarget = line.sTarget;
	int nLen = strlen( pTarget );

	if (*pTarget == '$') // BUG? if ASC #:# starts with '$' ? // && (iOpcode != OPCODE_NOP)
	{
		pTarget++;
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "$", linerect );
	}

	if (! bCursorLine)
	{
		if (bDisasmFormatFlags & DISASM_FORMAT_SYMBOL)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SYMBOL ) );
		}
		else
		{
			if (iOpmode == AM_M)
//			if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
			{
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE ) );
			}
			else
			{
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_TARGET ) );
			}
		}
	}

	// https://github.com/AppleWin/AppleWin/issues/227
	// (Debugger)[1.25] AppleSoft symbol: COPY.FAC.TO.ARG.ROUNDED overflows into registers
	// Repro:
	//    UEA39
	// 2.8.0.1 Clamp excessive symbol target to not overflow
	//    SYM COPY.FAC.TO.ARG.ROUNDED = EB63
	// If opcodes aren't showing then length can be longer!
	// FormatOpcodeBytes() uses 3 chars/MAX_OPCODES. i.e. "## "
	int nMaxLen = MAX_TARGET_LEN;

	// 2.8.0.8: Bug #227: AppleSoft symbol: COPY.FAC.TO.ARG.ROUNDED overflows into registers
	if ( !g_bConfigDisasmAddressView )
	    nMaxLen += 4;
	if ( !g_bConfigDisasmOpcodesView )
	    nMaxLen += (MAX_OPCODES*3);

	// 2.9.0.9 Continuation of 2.8.0.8: Fix overflowing disassembly pane for long symbols
	int nOverflow = 0;
	if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
	{
		if (line.nTargetOffset != 0)
			nOverflow++;

		nOverflow += strlen( line.sTargetOffset );
	}

	if (line.bTargetIndirect || line.bTargetX || line.bTargetY)
	{
		if (line.bTargetX)
				nOverflow += 2;
		else
		if ((line.bTargetY) && (! line.bTargetIndirect))
				nOverflow += 2;
	}

	if (line.bTargetIndexed || line.bTargetIndirect)
		nOverflow++;

	if (line.bTargetIndexed)
	{
		if (line.bTargetY)
			nOverflow += 2;
	}

	if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER)
	{
		nOverflow += strlen( line.sTargetPointer ); // '####'
		nOverflow ++  ;                             //     ':'
		nOverflow += 2;                             //      '##'
		nOverflow ++  ;                             //         ' '
	}

	if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
	{
		nOverflow += strlen( line.sImmediate );
	}

	if (nLen >=  (nMaxLen - nOverflow))
	{
#if _DEBUG
		// TODO: Warn on import about long symbol/target names
#endif
		pTarget[ nMaxLen - nOverflow ] = 0;
	}

	// TODO: FIXME: 2.8.0.7: Allow ctrl characters to show as inverse; i.e. ASC 400:40F
	PrintTextCursorX( pTarget, linerect );

	// Target Offset +/-
	if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));

		if (line.nTargetOffset > 0)
			PrintTextCursorX( "+", linerect );
		else
		if (line.nTargetOffset < 0)
			PrintTextCursorX( "-", linerect );

		if (! bCursorLine)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE )); // Technically, not a hex number, but decimal
		}
		PrintTextCursorX( line.sTargetOffset, linerect );
	}
	// Inside parenthesis = Indirect Target Regs
	if (line.bTargetIndirect || line.bTargetX || line.bTargetY)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));

		if (line.bTargetX)
		{
			PrintTextCursorX( ",", linerect );
			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ) );
			PrintTextCursorX( "X", linerect );
		}
		else
		if ((line.bTargetY) && (! line.bTargetIndirect))
		{
			PrintTextCursorX( ",", linerect );
			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ) );
			PrintTextCursorX( "Y", linerect );
		}
	}

	if (line.bTargetIndexed || line.bTargetIndirect)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));

		PrintTextCursorX( ")", linerect );
	}

	if (line.bTargetIndexed)
	{
		if (line.bTargetY)
		{
			PrintTextCursorX( ",", linerect );
			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ) );
			PrintTextCursorX( "Y", linerect );
		}
	}

	// BUGFIX: 2.6.2.30:  DA $target --> show right paren
	if( pData )
	{
		return nOpbyte;
	}

	// Memory Pointer and Value
	if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER) // (bTargetValue)
	{
		linerect.left = (int) aTabs[ TS_IMMEDIATE ]; // TS_IMMEDIATE ];

		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));

		PrintTextCursorX( line.sTargetPointer, linerect );

		if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_VALUE)
		{
			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));

			if (g_iConfigDisasmTargets & DISASM_TARGET_BOTH)
				PrintTextCursorX( ":", linerect );

			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE ));

			PrintTextCursorX( line.sTargetValue, linerect );
			PrintTextCursorX( " ", linerect );
		}
	}

	// Immediate Char
	if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
	{
		linerect.left = (int) aTabs[ TS_CHAR ]; // TS_IMMEDIATE ];

		if (! bCursorLine)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
		}

		if (! bCursorLine)
		{
			ColorizeSpecialChar( NULL, line.nImmediate, MEM_VIEW_ASCII, iBackground );
		}
		PrintTextCursorX( line.sImmediate, linerect );

		DebuggerSetColorBG( DebuggerGetColor( iBackground ) ); // Hack: Colorize can "color bleed to EOL"
		if (! bCursorLine)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
		}
	}

	// Branch Indicator
	if (bDisasmFormatFlags & DISASM_FORMAT_BRANCH)
	{
		linerect.left = (int) aTabs[ TS_BRANCH ];

		if (! bCursorLine)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_BRANCH ) );
		}

#if !USE_APPLE_FONT
		if (g_iConfigDisasmBranchType == DISASM_BRANCH_FANCY)
			SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_DISASM_BRANCH ]._hFont );  // g_hFontWebDings
#endif

		PrintText( line.sBranch, linerect );

#if !USE_APPLE_FONT
		if (g_iConfigDisasmBranchType)
			SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont ); // g_hFontDisasm
#endif
	}

	return nOpbyte;
}


// Optionally copy the flags to pText_
//===========================================================================
void DrawFlags ( int line, unsigned short nRegFlags, LPTSTR pFlagNames_)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	char sFlagNames[ _6502_NUM_FLAGS+1 ] = ""; // = "NVRBDIZC"; // copy from g_aFlagNames
	char sText[8] = "?";
	RECT rect;

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	// Regs are 10 chars across
	// Flags are 8 chars across -- scale "up"
	int nSpacerWidth = nFontWidth;

#if OLD_FLAGS_SPACING
	const int nScaledWidth = 10;
	if (nFontWidth)
		nSpacerWidth = (nScaledWidth * nFontWidth) / 8;
	nSpacerWidth++;
#endif

	//
	rect.top    = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left   = DISPLAY_FLAG_COLUMN;
	rect.right  = rect.left + (10 * nFontWidth);

	DebuggerSetColorBG( DebuggerGetColor( BG_DATA_1 )); // BG_INFO
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ));
	PrintText( "P ", rect );

	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;

	snprintf( sText, sizeof(sText), "%02X", nRegFlags );

	DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
	PrintText( sText, rect );

	rect.top    -= g_nFontHeight;
	rect.bottom -= g_nFontHeight;
	sText[1] = 0;

	rect.left += ((2 + _6502_NUM_FLAGS) * nSpacerWidth);
	rect.right = rect.left + nFontWidth;

	//

	int iFlag = 0;
	int nFlag = _6502_NUM_FLAGS;
	while (nFlag--)
	{
		iFlag = (_6502_NUM_FLAGS - nFlag - 1);

		bool bSet = (nRegFlags & 1);

		sText[0] = g_aBreakpointSource[ BP_SRC_FLAG_C + iFlag ][0];

		if (bSet)
		{
			DebuggerSetColorBG( DebuggerGetColor( BG_INFO_INVERSE ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_INVERSE ));
		}
		else
		{
			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
		}

		rect.left  -= nSpacerWidth;
		rect.right -= nSpacerWidth;
		PrintText( sText, rect );

		// Print Binary value
		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
		DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));

		sText[0] = '0' + static_cast<int>(bSet);
		PrintText( sText, rect );
		rect.top    -= g_nFontHeight;
		rect.bottom -= g_nFontHeight;

		if (pFlagNames_)
		{
			if (!bSet)
				sFlagNames[nFlag] = '.';
			else
				sFlagNames[nFlag] = g_aBreakpointSource[ BP_SRC_FLAG_C + iFlag ][0];
		}

		nRegFlags >>= 1;
	}

	if (pFlagNames_)
		_l_strcpy(pFlagNames_,sFlagNames);
}

//===========================================================================
void DrawMemory ( int line, int iMemDump )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	MemoryDump_t* pMD = &g_aMemDump[ iMemDump ];
	bool bActive = pMD->bActive;
#if DEBUG_FORCE_DISPLAY
	bActive = true;
#endif
	if( !bActive )
		return;

	USHORT       nAddr   = pMD->nAddress;
	DEVICE_e     eDevice = pMD->eDevice;
	MemoryView_e iView   = pMD->eView;

	SS_CARD_MOCKINGBOARD SS_MB;

	if ((eDevice == DEV_SY6522) || (eDevice == DEV_AY8910))
		MB_GetSnapshot(&SS_MB, 4+(nAddr>>1));		// Slot4 or Slot5

	RECT rect;
	rect.left   = DISPLAY_MINIMEM_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	RECT rect2;
	rect2 = rect;

	const int MAX_MEM_VIEW_TXT = 16;
	char sText[ MAX_MEM_VIEW_TXT * 2 ];

	char sType   [ 8 ] = "Mem";
	char sAddress[ 12 ] = "";

	int iForeground = FG_INFO_OPCODE;
	int iBackground = BG_INFO;

#if DISPLAY_MEMORY_TITLE
	if (eDevice == DEV_SY6522)
	{
		snprintf( sAddress, sizeof(sAddress), "SY#%d", nAddr );
	}
	else if(eDevice == DEV_AY8910)
	{
		snprintf( sAddress, sizeof(sAddress),"AY#%d", nAddr );
	}
	else
	{
		snprintf( sAddress, sizeof(sAddress),"%04X",(unsigned)nAddr);

		if (iView == MEM_VIEW_HEX)
			snprintf( sType, sizeof(sType), "HEX" );
		else
		if (iView == MEM_VIEW_ASCII)
			snprintf( sType, sizeof(sType), "ASCII" );
		else
			snprintf( sType, sizeof(sType), "TEXT" );
	}

	rect2 = rect;
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
	DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
	PrintTextCursorX( sType, rect2 );

	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
	PrintTextCursorX( " at ", rect2 );

	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
	PrintTextCursorY( sAddress, rect2 );
#endif

	rect.top    = rect2.top;
	rect.bottom = rect2.bottom;

	unsigned short iAddress = nAddr;

	int nLines = g_nDisplayMemoryLines;
	int nCols = 4;

	if (iView != MEM_VIEW_HEX)
	{
		nCols = MAX_MEM_VIEW_TXT;
	}

	if( (eDevice == DEV_SY6522) || (eDevice == DEV_AY8910) )
	{
		iAddress = 0;
		nCols = 6;
	}

	rect.right = DISPLAY_WIDTH - 1;

	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));

	for (int iLine = 0; iLine < nLines; iLine++ )
	{
		rect2 = rect;

		if (iView == MEM_VIEW_HEX)
		{
			sprintf( sText, "%04X", iAddress );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );
		}

		for (int iCol = 0; iCol < nCols; iCol++)
		{
			DebuggerSetColorBG( DebuggerGetColor( iBackground ));
			DebuggerSetColorFG( DebuggerGetColor( iForeground ));

// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
//			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
//			{
//				sprintf( sText, "IO " );
//			}
//			else
			if (eDevice == DEV_SY6522)
			{
				sprintf( sText, "%02X", (unsigned) ((unsigned char*)&SS_MB.Unit[nAddr & 1].RegsSY6522)[iAddress] );
				if (iCol & 1)
					DebuggerSetColorFG( DebuggerGetColor( iForeground ));
				else
					DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			}
			else
			if (eDevice == DEV_AY8910)
			{
				sprintf( sText, "%02X", (unsigned)SS_MB.Unit[nAddr & 1].RegsAY8910[iAddress] );
				if (iCol & 1)
					DebuggerSetColorFG( DebuggerGetColor( iForeground ));
				else
					DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			}
			else
			{
				unsigned char nData = (unsigned)*(LPBYTE)(mem+iAddress);
				sText[0] = 0;

				if (iView == MEM_VIEW_HEX)
				{
					if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
					{
						DebuggerSetColorFG( DebuggerGetColor( FG_INFO_IO_BYTE ));
					}

					sprintf(sText, "%02X ", nData );
				}
				else
				{
// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
					if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
						iBackground = BG_INFO_IO_BYTE;

					ColorizeSpecialChar( sText, nData, iView, iBackground );
				}
			}
			int nChars = PrintTextCursorX( sText, rect2 ); // PrintTextCursorX()
			(void) nChars;
			iAddress++;
		}
		// Windows HACK: Bugfix: Rest of line is still background color
//		DebuggerSetColorBG(  hDC, DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
//		DebuggerSetColorFG(hDC, DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
//		PrintTextCursorX( " ", rect2 );

		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
	}
}

//===========================================================================
void DrawRegister ( int line, LPCTSTR name, const int nBytes, const unsigned short nValue, int iSource )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	RECT rect;
	rect.top    = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left   = DISPLAY_REGS_COLUMN;
	rect.right  = rect.left + (10 * nFontWidth); // + 1;

	if ((PARAM_REG_A  == iSource) ||
		(PARAM_REG_X  == iSource) ||
		(PARAM_REG_Y  == iSource) ||
		(PARAM_REG_PC == iSource) ||
		(PARAM_REG_SP == iSource))
	{
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ));
	}
	else
	{
//		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
		rect.left += nFontWidth;
	}

	// 2.6.0.0 Alpha - Regs not "boxed"
	int iBackground = BG_DATA_1;  // BG_INFO

	DebuggerSetColorBG( DebuggerGetColor( iBackground ));
	PrintText( name, rect );

	unsigned int nData = nValue;
	int nOffset = 6;

	char sValue[8];

	if (PARAM_REG_SP == iSource)
	{
		unsigned short nStackDepth = _6502_STACK_END - nValue;
		sprintf( sValue, "%02X", nStackDepth );
		int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;
		rect.left += (2 * nFontWidth) + (nFontWidth >> 1); // 2.5 looks a tad nicer then 2

		// ## = Stack Depth (in bytes)
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR )); // FG_INFO_OPCODE, FG_INFO_TITLE
		PrintText( sValue, rect );
	}

	if (nBytes == 2)
	{
		sprintf(sValue,"%04X", nData);
	}
	else
	{
		rect.left  = DISPLAY_REGS_COLUMN + (3 * nFontWidth);
//		rect.right = SCREENSPLIT2;

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
		PrintTextCursorX( "'", rect ); // PrintTextCursorX

		ColorizeSpecialChar( sValue, nData, MEM_VIEW_ASCII ); // MEM_VIEW_APPLE for inverse background little hard on the eyes

		DebuggerSetColorBG( DebuggerGetColor( iBackground ));
		PrintTextCursorX( sValue, rect ); // PrintTextCursorX()

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
		PrintTextCursorX( "'", rect ); // PrintTextCursorX()

		sprintf(sValue,"  %02X", nData );
	}

	// Needs to be far enough over, since 4 chars of ZeroPage symbol also calls us
	rect.left  = DISPLAY_REGS_COLUMN + (nOffset * nFontWidth);

	if ((PARAM_REG_PC == iSource) || (PARAM_REG_SP == iSource)) // Stack Pointer is target address, but doesn't look as good.
	{
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
	}
	else
	{
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE )); // FG_DISASM_OPCODE
	}
	PrintText( sValue, rect );
}

//===========================================================================
void DrawRegisters ( int line )
{
	const char **sReg = g_aBreakpointSource;

	DrawRegister( line++, sReg[ BP_SRC_REG_A ] , 1, regs.a , PARAM_REG_A  );
	DrawRegister( line++, sReg[ BP_SRC_REG_X ] , 1, regs.x , PARAM_REG_X  );
	DrawRegister( line++, sReg[ BP_SRC_REG_Y ] , 1, regs.y , PARAM_REG_Y  );
	DrawRegister( line++, sReg[ BP_SRC_REG_PC] , 2, regs.pc, PARAM_REG_PC );
	DrawFlags   ( line  , regs.ps, NULL);
	line += 2;
	DrawRegister( line++, sReg[ BP_SRC_REG_S ] , 2, regs.sp, PARAM_REG_SP );
}


// 2.9.0.3
//===========================================================================
void _DrawSoftSwitchHighlight( RECT & temp, bool bSet, const char *sOn, const char *sOff, int bg = BG_INFO )
{
//	DebuggerSetColorBG( DebuggerGetColor( bg                 ) ); // BG_INFO

	ColorizeFlags( bSet, bg );
	PrintTextCursorX( sOn, temp );

	DebuggerSetColorBG( DebuggerGetColor( bg                 ) ); // BG_INFO
	DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
	PrintTextCursorX( "/", temp );

	ColorizeFlags( !bSet, bg );
	PrintTextCursorX( sOff, temp );
}


// 2.9.0.8
//===========================================================================
void _DrawSoftSwitchAddress( RECT & rect, int nAddress, int bg_default = BG_INFO )
{
	char sText[ 4 ] = "";

	DebuggerSetColorBG( DebuggerGetColor( bg_default ));
	DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_TARGET ));
	sprintf( sText, "%02X", (nAddress & 0xFF) );
	PrintTextCursorX( sText, rect );

	DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
	PrintTextCursorX( ":", rect );
}

// 2.7.0.7 Cleaned up display of soft-switches to show address.
//===========================================================================
void _DrawSoftSwitch( RECT & rect, int nAddress, bool bSet, char *sPrefix, char *sOn, char *sOff, const char *sSuffix = NULL, int bg_default = BG_INFO )
{
	RECT temp = rect;

	_DrawSoftSwitchAddress( temp, nAddress, bg_default );

	if( sPrefix )
	{
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG )); // light blue
		PrintTextCursorX( sPrefix, temp );
	}

	// 2.9.0.3
	_DrawSoftSwitchHighlight( temp, bSet, sOn, sOff, bg_default );

	DebuggerSetColorBG( DebuggerGetColor( bg_default    ));
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
	if ( sSuffix )
		PrintTextCursorX( sSuffix, temp );

	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;
}

// 2.9.0.8
//===========================================================================
void _DrawTriStateSoftSwitch( RECT & rect, int nAddress, const int iBankDisplay, int iActive, char *sPrefix, char *sOn, char *sOff, const char *sSuffix = NULL, int bg_default = BG_INFO )
{
//	if ((iActive == 0) || (iBankDisplay == iActive))
	bool bSet = (iBankDisplay == iActive);

	if ( bSet )
		_DrawSoftSwitch( rect, nAddress, bSet, NULL, sOn, sOff, " ", bg_default );
	else // Main Memory is active, or Bank # is not active
	{
		RECT temp = rect;
		int iBank = (GetMemMode() & MF_HRAM_BANK2)
			? 2
			: 1
			;
		bool bDisabled = ((iActive == 0) && (iBank == iBankDisplay));


		_DrawSoftSwitchAddress( temp, nAddress, bg_default );

		// TODO: Q. Show which bank we are writing to in red?
		//       A. No, since we highlight bank 2 or 1, along with R/W
		DebuggerSetColorBG( DebuggerGetColor( bg_default    ));
		if( bDisabled )
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE  ) );
		else
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );

		PrintTextCursorX( sOn , temp );
		PrintTextCursorX( "/" , temp );

		ColorizeFlags( bDisabled, bg_default, FG_DISASM_OPERATOR );
		PrintTextCursorX( sOff, temp );

		DebuggerSetColorBG( DebuggerGetColor( bg_default    ));
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
		PrintTextCursorX( " " , temp );

		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
	}
}

/*
	Debugger: Support LC status and memory
	https://github.com/AppleWin/AppleWin/issues/406

	Bank2       Bank1       First Access, Second Access
	-----------------------------------------------
	C080        C088        Read RAM,     Write protect    MF_HIGHRAM   ~MF_WRITERAM
	C081        C089        Read ROM,     Write enable    ~MF_HIGHRAM    MF_WRITERAM
	C082        C08A        Read ROM,     Write protect   ~MF_HIGHRAM   ~MF_WRITERAM
	C083        C08B        Read RAM,     Write enable     MF_HIGHRAM    MF_WRITERAM
	c084        C08C        same as C080/C088
	c085        C08D        same as C081/C089
	c086        C08E        same as C082/C08A
	c087        C08F        same as C083/C08B
	MF_BANK2   ~MF_BANK2

	NOTE: Saturn 128K uses C084 .. C087 and C08C .. C08F to select Banks 0 .. 7 !
*/
// 2.9.0.4 Draw Language Card Bank Usage
// @param iBankDisplay Either 1 or 2
//===========================================================================
void _DrawSoftSwitchLanguageCardBank( RECT & rect, const int iBankDisplay, int bg_default = BG_INFO )
{
	const int w  = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;
	const int dx80 = 7 * w; // "80:B2/MxR/W"
	//                          ^------^
	const int dx88 = 8 * w; // "88:B1/M    "
	//                          ^-------^

#ifdef _DEBUG
	const int finalRectRight = rect.left + 11 * w;
#endif

	rect.right = rect.left + dx80;

	// 0 = RAM
	// 1 = Bank 1
	// 2 = Bank 2
	bool bBankWritable = (GetMemMode() & MF_HRAM_WRITE) != 0;
	int iBankActive    = (GetMemMode() & MF_HIGHRAM)
		? (GetMemMode() & MF_HRAM_BANK2)
			? 2
			: 1
		: 0
		;
//	bool bBankOn = (iBankActive == iBankDisplay);

	// B#/[M]
	char sOn [ 4 ] = "B#"; // LC# but one char too wide :-/
	char sOff[ 4 ] = "M";
	// C080 LC2
	// C088 LC1
	int nAddress = 0xC080 + (8 * (2 - iBankDisplay));
	sOn[1] = '0' + iBankDisplay;

	// if off then ONLY highlight 'M' but only for the appropiate bank
	// if on  then do NOT highlight 'M'
	// if on  then also only highly the ACTIVE bank
	_DrawTriStateSoftSwitch( rect, nAddress, iBankDisplay, iBankActive, NULL, sOn, sOff, " ", bg_default );

	rect.top    -= g_nFontHeight;
	rect.bottom -= g_nFontHeight;

	if (iBankDisplay == 2)
	{
		rect.left   += dx80;
		rect.right  += 4*w;

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_BP_S_X )); // Red
		DebuggerSetColorBG( DebuggerGetColor( bg_default       ));
		PrintTextCursorX( (GetMemMode() & MF_ALTZP) ? "x" : " ", rect );

		// [B2]/M R/[W]
		// [B2]/M [R]/W
		const char *pOn  = "R";
		const char *pOff = "W";

		_DrawSoftSwitchHighlight( rect, !bBankWritable, pOn, pOff, bg_default );
	}
	else
	{
		_ASSERT(iBankDisplay == 1);

		rect.left   += dx88;
		rect.right  += 4*w;

		int iActiveBank = -1;
		char sText[ 4 ] = "?"; // Default to RAMWORKS

#ifdef RAMWORKS
		/*if (GetCurrentExpansionMemType() == CT_RamWorksIII)*/
		{ sText[0] = 'r'; iActiveBank = GetRamWorksActiveBank(); }
#endif
#ifdef TODO // Not supported for Linux yet...
		if (GetCurrentExpansionMemType() == CT_Saturn128K)  { sText[0] = 's'; iActiveBank = GetLanguageCard()->GetActiveBank(); }
#endif

		if (iActiveBank >= 0)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG )); // light blue
			PrintTextCursorX( sText, rect );

			sprintf( sText, "%02X", (iActiveBank & 0x7F) );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));	// orange
			PrintTextCursorX( sText, rect );
		}
		else
		{
			PrintTextCursorX("   ", rect);
		}
	}

	_ASSERT(rect.right == finalRectRight);

	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;
}


/*
	$C002 W RAMRDOFF  Read enable  main memory from $0200-$BFFF
	$C003 W RAMDRON   Read enable  aux  memory from $0200-$BFFF
	$C004 W RAMWRTOFF Write enable main memory from $0200-$BFFF
	$C005 W RAMWRTON  Write enable aux  memory from $0200-$BFFF
*/
// 2.9.0.6
// GH#406 https://github.com/AppleWin/AppleWin/issues/406
//===========================================================================
void _DrawSoftSwitchMainAuxBanks( RECT & rect, int bg_default = BG_INFO )
{
	RECT temp = rect;
	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;


	/*
		Ideally, we'd show Read/Write for Main/Aux
			02:RM/X
			04:WM/X
		But we only have 1 line:
			02:Rm/x Wm/x
			00:ASC/MOUS
		Which is one character too much.
			02:Rm/x Wm/a
		But we abbreaviate it without the space and color code the Read and Write:
			02:Rm/xWm/x
	*/

	int w  = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;
	int dx = 7 * w;

	int  nAddress  = 0xC002;
	bool bMainRead = (GetMemMode() & MF_AUXREAD) != 0;
	bool bAuxWrite = (GetMemMode() & MF_AUXWRITE) != 0;

	temp.right = rect.left + dx;
	_DrawSoftSwitch( temp, nAddress, !bMainRead, "R", "m", "x", NULL, BG_DATA_2 );

	temp.top    -= g_nFontHeight;
	temp.bottom -= g_nFontHeight;
	temp.left   += dx;
	temp.right  += 3*w;

	DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_BP_S_X )); // Red
	DebuggerSetColorBG( DebuggerGetColor( bg_default       ));
	PrintTextCursorX( "W", temp );
	_DrawSoftSwitchHighlight( temp, !bAuxWrite, "m", "x", BG_DATA_2 );
}


// 2.7.0.1 Display state of soft switches
//===========================================================================
void DrawSoftSwitches( int iSoftSwitch )
{
	RECT rect;
	RECT temp;
	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

		rect.left   = DISPLAY_SOFTSWITCH_COLUMN;
		rect.top    = iSoftSwitch * g_nFontHeight;
		rect.right = rect.left + (10 * nFontWidth) + 1;
		rect.bottom = rect.top + g_nFontHeight;
		temp = rect;

		DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));

#if SOFTSWITCH_OLD
		char sText[16] = "";
		// $C050 / $C051 = TEXTOFF/TEXTON = SW.TXTCLR/SW.TXTSET
		// GR  / TEXT
		// GRAPH/TEXT
		// TEXT ON/OFF
		sprintf( sText, !VideoGetSWTEXT() ? "GR  / ----" : "--  / TEXT" );
		PrintTextCursorY( sText, rect );

		// $C052 / $C053 = MIXEDOFF/MIXEDON = SW.MIXCLR/SW.MIXSET
		// FULL/MIXED
		// MIX OFF/ON
		sprintf( sText, !VideoGetSWMIXED() ? "FULL/-----" : "----/MIXED" );
		PrintTextCursorY( sText, rect );

		// $C054 / $C055 = PAGE1/PAGE2 = PAGE2OFF/PAGE2ON = SW.LOWSCR/SW.HISCR
		// PAGE 1 / 2
		sprintf( sText, !VideoGetSWPAGE2() ? "PAGE 1 / -" : "PAGE - / 2" );
		PrintTextCursorY( sText, rect );

		// $C056 / $C057 LORES/HIRES = HIRESOFF/HIRESON = SW.LORES/SW.HIRES
		// LO / HIRES
		// LO / -----
		// -- / HIRES
		sprintf( sText, !VideoGetSWHIRES() ? "LO /-- RES" : "---/HI RES" );
		PrintTextCursorY( sText, rect );

		PrintTextCursorY( "", rect );

		// Extended soft switches
		sprintf( sText, !VideoGetSW80COL() ? "40 / -- COL" : "-- / 80 COL" );
		PrintTextCursorY( sText, rect );

		sprintf(sText, VideoGetSWAltCharSet() ? "ASCII/-----" : "-----/MOUSE" );
		PrintTextCursorY( sText, rect );

		// 280/560 HGR
		sprintf(sText, !VideoGetSWDHIRES() ? "HGR / ----" : "--- / DHGR" );
		PrintTextCursorY( sText, rect );
#else //SOFTSWITCH_OLD
		// See: VideoSetMode()

		// $C050 / $C051 = TEXTOFF/TEXTON = SW.TXTCLR/SW.TXTSET
		// GR  / TEXT
		// GRAPH/TEXT
		// TEXT ON/OFF
		bool bSet;

		// $C050 / $C051 = TEXTOFF/TEXTON = SW.TXTCLR/SW.TXTSET
		bSet = !VideoGetSWTEXT();
		_DrawSoftSwitch( rect, 0xC050, bSet, NULL, "GR.", "TEXT" );

		// $C052 / $C053 = MIXEDOFF/MIXEDON = SW.MIXCLR/SW.MIXSET
		// FULL/MIXED
		// MIX OFF/ON
		bSet = !VideoGetSWMIXED();
		_DrawSoftSwitch( rect, 0xC052, bSet, NULL, "FULL", "MIX" );

		// $C054 / $C055 = PAGE1/PAGE2 = PAGE2OFF/PAGE2ON = SW.LOWSCR/SW.HISCR
		// PAGE 1 / 2
		bSet = !VideoGetSWPAGE2();
		_DrawSoftSwitch( rect, 0xC054, bSet, "PAGE ", "1", "2" );

		// $C056 / $C057 LORES/HIRES = HIRESOFF/HIRESON = SW.LORES/SW.HIRES
		// LO / HIRES
		// LO / -----
		// -- / HIRES
		bSet = !VideoGetSWHIRES();
		_DrawSoftSwitch( rect, 0xC056, bSet, NULL, "LO", "HI", "RES" );

		DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));

		// 280/560 HGR
		// C05E = ON, C05F = OFF
		bSet = VideoGetSWDHIRES();
		_DrawSoftSwitch( rect, 0xC05E, bSet, NULL, "DHGR", "HGR" );


		// Extended soft switches
		int bgMemory = BG_DATA_2;

		// C000 = 80STOREOFF, C001 = 80STOREON
		bSet = !VideoGetSW80STORE();
		_DrawSoftSwitch( rect, 0xC000, bSet, "80Sto", "0", "1", NULL, bgMemory );

		// C002 .. C005
		_DrawSoftSwitchMainAuxBanks( rect, bgMemory );

		// C00C = off, C00D = on
		bSet = !VideoGetSW80COL();
		_DrawSoftSwitch( rect, 0xC00C, bSet, "Col", "40", "80", NULL, bgMemory );

		// C00E = off, C00F = on
		bSet = !VideoGetSWAltCharSet();
		_DrawSoftSwitch( rect, 0xC00E, bSet, NULL, "ASC", "MOUS", NULL, bgMemory ); // ASCII/MouseText

#if SOFTSWITCH_LANGCARD
		// GH#406 https://github.com/AppleWin/AppleWin/issues/406
		// 2.9.0.4
		// Language Card Bank 1/2
		// See: MemSetPaging()

// LC2 & C008/C009 (ALTZP & ALT-LC)
		DebuggerSetColorBG( DebuggerGetColor( bgMemory )); // BG_INFO_2 -> BG_DATA_2
		_DrawSoftSwitchLanguageCardBank( rect, 2, bgMemory );

// LC1
		rect.left = DISPLAY_SOFTSWITCH_COLUMN; // INFO_COL_2;
		_DrawSoftSwitchLanguageCardBank( rect, 1, bgMemory );
#endif

#endif // SOFTSWITCH_OLD
}


//===========================================================================
void DrawSourceLine( int iSourceLine, RECT &rect )
{
	char sLine[ CONSOLE_WIDTH ];
	ZeroMemory( sLine, CONSOLE_WIDTH );

	if ((iSourceLine >=0) && (iSourceLine < g_AssemblerSourceBuffer.GetNumLines() ))
	{
		char * pSource = g_AssemblerSourceBuffer.GetLine( iSourceLine );

//		int nLenSrc = strlen( pSource );
//		if (nLenSrc >= CONSOLE_WIDTH)
//			bool bStop = true;

		TextConvertTabsToSpaces( sLine, pSource, CONSOLE_WIDTH-1 ); // bugfix 2,3,1,15: fence-post error, buffer over-run

//		int nLenTab = strlen( sLine );
	}
	else
	{
		_l_strcpy( sLine, " " );
	}

	PrintText( sLine, rect );
	rect.top += g_nFontHeight;
}


//===========================================================================
void DrawStack ( int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	unsigned nAddress = regs.sp;
#if DEBUG_FORCE_DISPLAY // Stack
	nAddress = 0x100;
#endif

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	// 2.6.0.0 Alpha - Stack was dark cyan BG_DATA_1
	DebuggerSetColorBG( DebuggerGetColor( BG_DATA_1 )); // BG_INFO // recessed 3d look

	int    iStack = 0;
	while (iStack < MAX_DISPLAY_STACK_LINES)
	{
		nAddress++;

		RECT rect;
		rect.left   = DISPLAY_STACK_COLUMN;
		rect.top    = (iStack+line) * g_nFontHeight;
		rect.right = rect.left + (10 * nFontWidth) + 1;
		rect.bottom = rect.top + g_nFontHeight;

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE )); // [COLOR_STATIC

		char sText[8] = "";
		if (nAddress <= _6502_STACK_END)
		{
			sprintf( sText,"%04X: ", nAddress );
			PrintTextCursorX( sText, rect );
		}

		if (nAddress <= _6502_STACK_END)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE )); // COLOR_FG_DATA_TEXT
			sprintf(sText, "  %02X",(unsigned)*(LPBYTE)(mem+nAddress));
			PrintTextCursorX( sText, rect );
		}
		iStack++;
	}
}


//===========================================================================
void DrawTargets ( int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	int aTarget[3];
	_6502_GetTargets( regs.pc, &aTarget[0],&aTarget[1],&aTarget[2], NULL );
	GetTargets_IgnoreDirectJSRJMP(mem[regs.pc], aTarget[2]);

	aTarget[1] = aTarget[2];	// Move down as we only have 2 lines

	RECT rect;
	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	int iAddress = MAX_DISPLAY_TARGET_PTR_LINES;
	while (iAddress--)
	{
		// .6 Bugfix: DrawTargets() should draw target byte for IO address: R PC FB33
//		if ((aTarget[iAddress] >= _6502_IO_BEGIN) && (aTarget[iAddress] <= _6502_IO_END))
//			aTarget[iAddress] = NO_6502_TARGET;

		char sAddress[8] = "-none-";
		char sData[8]    = "";

#if DEBUG_FORCE_DISPLAY // Targets
		if (aTarget[iAddress] == NO_6502_TARGET)
			aTarget[iAddress] = 0;
#endif
		if (aTarget[iAddress] != NO_6502_TARGET)
		{
			sprintf(sAddress,"%04X",aTarget[iAddress]);
			if (iAddress)
				sprintf(sData,"%02X",*(LPBYTE)(mem+aTarget[iAddress]));
			else
				sprintf(sData,"%04X",*(LPWORD)(mem+aTarget[iAddress]));
		}

		rect.left   = DISPLAY_TARGETS_COLUMN;
		rect.top    = (line+iAddress) * g_nFontHeight;
		int nColumn = rect.left + (7 * nFontWidth);
		rect.right  = nColumn;
		rect.bottom = rect.top + g_nFontHeight;

		if (iAddress == 0)
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE )); // Temp Address
		else
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS )); // Target Address

		DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
		PrintText( sAddress, rect );

		rect.left  = nColumn;
		rect.right = rect.left + (10 * nFontWidth); // SCREENSPLIT2

		if (iAddress == 0)
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS )); // Temp Target
		else
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE )); // Target Bytes

		PrintText( sData, rect );
	}
}

//===========================================================================
void DrawWatches (int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_WATCHES_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	char sText[16] = "Watches";

	DebuggerSetColorBG(DebuggerGetColor( BG_INFO_WATCH ));

#if DISPLAY_WATCH_TITLE
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
	PrintTextCursorY( sText, rect );
#endif

	int iWatch;
	for (iWatch = 0; iWatch < MAX_WATCHES; iWatch++ )
	{
#if DEBUG_FORCE_DISPLAY // Watch
		if (true)
#else
		if (g_aWatches[iWatch].bEnabled)
#endif
		{
			RECT rect2 = rect;

			DebuggerSetColorBG( DebuggerGetColor( BG_INFO_WATCH ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ) );
			PrintTextCursorX( "W", rect2 );

			sprintf( sText, "%X ",iWatch );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ));
			PrintTextCursorX( sText, rect2 );

//			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
//			PrintTextCursorX( ".", rect2 );

			sprintf( sText,"%04X", g_aWatches[iWatch].nAddress );
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			unsigned char nTarget8 = 0;

			nTarget8 = (unsigned)*(LPBYTE)(mem+g_aWatches[iWatch].nAddress);
			sprintf(sText,"%02X", nTarget8 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			PrintTextCursorX( sText, rect2 );

			nTarget8 = (unsigned)*(LPBYTE)(mem+g_aWatches[iWatch].nAddress + 1);
			sprintf(sText,"%02X", nTarget8 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			PrintTextCursorX( sText, rect2 );

			sprintf( sText,"(" );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( sText, rect2 );

			unsigned short nTarget16 = (unsigned)*(LPWORD)(mem+g_aWatches[iWatch].nAddress);
			sprintf( sText,"%04X", nTarget16 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
//			PrintTextCursorX( ":", rect2 );
			PrintTextCursorX( ")", rect2 );

//			unsigned char nValue8 = (unsigned)*(LPBYTE)(mem + nTarget16);
//			sprintf(sText,"%02X", nValue8 );
//			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
//			PrintTextCursorX( sText, rect2 );

			rect.top    += g_nFontHeight;
			rect.bottom += g_nFontHeight;

// 1.19.4 Added: Watch show (dynamic) raw hex bytes
			rect2 = rect;

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			for( int iByte = 0; iByte < 8; iByte++ )
			{
				if  ((iByte & 3) == 0) {
					DebuggerSetColorBG( DebuggerGetColor( BG_INFO_WATCH ));
					PrintTextCursorX( " ", rect2 );
				}

				if ((iByte & 1) == 1)
					DebuggerSetColorBG( DebuggerGetColor( BG_INFO_WATCH ));
				else
					DebuggerSetColorBG( DebuggerGetColor( BG_DATA_2 ));

				unsigned char nValue8 = (unsigned)*(LPBYTE)(mem + nTarget16 + iByte );
				sprintf(sText,"%02X", nValue8 );
				PrintTextCursorX( sText, rect2 );
			}
		}
		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
	}
}


//===========================================================================
void DrawZeroPagePointers ( int line )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	RECT rect;
	rect.top    = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left   = DISPLAY_ZEROPAGE_COLUMN;
	rect.right  = rect.left + (10 * nFontWidth);

	DebuggerSetColorBG( DebuggerGetColor( BG_INFO_ZEROPAGE ));

	const int nMaxSymbolLen = 7;
	char sText[nMaxSymbolLen+1] = "";

	for(int iZP = 0; iZP < MAX_ZEROPAGE_POINTERS; iZP++)
	{
		RECT rect2 = rect;

		Breakpoint_t *pZP = &g_aZeroPagePointers[iZP];
		bool bEnabled = pZP->bEnabled;

#if DEBUG_FORCE_DISPLAY // Zero-Page
		bEnabled = true;
#endif
		if (bEnabled)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ) );
			PrintTextCursorX( "Z", rect2 );

			sprintf( sText, "%X ", iZP );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ));
			PrintTextCursorX( sText, rect2 );

//			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
//			PrintTextCursorX( " ", rect2 );

			unsigned char nZPAddr1 = (g_aZeroPagePointers[iZP].nAddress  ) & 0xFF; // +MJP missig: "& 0xFF", or "(unsigned char) ..."
			unsigned char nZPAddr2 = (g_aZeroPagePointers[iZP].nAddress+1) & 0xFF;

			// Get nZPAddr1 last (for when neither symbol is not found - GetSymbol() return ptr to static buffer)
			const char* pSymbol2 = GetSymbol(nZPAddr2, 2);		// 2:8-bit value (if symbol not found)
			const char* pSymbol1 = GetSymbol(nZPAddr1, 2);		// 2:8-bit value (if symbol not found)

			int nLen1 = strlen( pSymbol1 );
			int nLen2 = strlen( pSymbol2 );


//			if ((nLen1 == 1) && (nLen2 == 1))
//				sprintf( sText, "%s%s", pszSymbol1, pszSymbol2);

			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));

			int x;
			for( x = 0; x < nMaxSymbolLen; x++ )
			{
				sText[ x ] = CHAR_SPACE;
			}
			sText[nMaxSymbolLen] = 0;

			if ((nLen1) && (pSymbol1[0] == '$'))
			{
//				sprintf( sText, "%s%s", pSymbol1 );
//				sprintf( sText, "%04X", nZPAddr1 );
			}
			else
			if ((nLen2) && (pSymbol2[0] == '$'))
			{
//				sprintf( sText, "%s%s", pSymbol2 );
//				sprintf( sText, "%04X", nZPAddr2 );
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));
			}
			else
			{
				int nMin = min( nLen1, nMaxSymbolLen );
				memcpy(sText, pSymbol1, nMin);
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SYMBOL ) );
			}
//			DrawRegister( line+iZP, szZP, 2, nTarget16);
//			PrintText( szZP, rect2 );
			PrintText( sText, rect2);

			rect2.left    = rect.left;
			rect2.top    += g_nFontHeight;
			rect2.bottom += g_nFontHeight;

			sprintf( sText, "%02X", nZPAddr1 );
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			unsigned short nTarget16 = (unsigned short)mem[ nZPAddr1 ] | ((unsigned short)mem[ nZPAddr2 ]<< 8);
			sprintf( sText, "%04X", nTarget16 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			unsigned char nValue8 = (unsigned)*(LPBYTE)(mem + nTarget16);
			sprintf(sText, "%02X", nValue8 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			PrintTextCursorX( sText, rect2 );
		}
		rect.top    += (g_nFontHeight * 2);
		rect.bottom += (g_nFontHeight * 2);
	}
}


// Sub Windows ____________________________________________________________________________________

//===========================================================================
void DrawSubWindow_Console (Update_t bUpdate)
{
	if (! CanDrawDebugger())
		return;

#if !USE_APPLE_FONT
	SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_CONSOLE ]._hFont );
#endif

	if ((bUpdate & UPDATE_CONSOLE_DISPLAY)
		|| (bUpdate & UPDATE_CONSOLE_INPUT))
	{
		DebuggerSetColorBG( DebuggerGetColor( BG_CONSOLE_OUTPUT ));

//		int nLines = MIN(g_nConsoleDisplayTotal - g_iConsoleDisplayStart, g_nConsoleDisplayHeight);
		int iLine = g_iConsoleDisplayStart + CONSOLE_FIRST_LINE;
		for (int y = 1; y < g_nConsoleDisplayLines ; y++ )
		{
			if (iLine <= (g_nConsoleDisplayTotal + CONSOLE_FIRST_LINE))
			{
				DebuggerSetColorFG( DebuggerGetColor( FG_CONSOLE_OUTPUT ));
				DrawConsoleLine( g_aConsoleDisplay[ iLine  ], y );
			}
			else
			{
				// bugfix: 2.6.1.34
				// scrolled past top of console... Draw blank line
				DrawConsoleLine( NULL, y );
			}
			iLine++;
		}

		DrawConsoleInput();
	}

//	if (bUpdate & UPDATE_CONSOLE_INPUT)
	{
//		DrawConsoleInput();
	}
}

//===========================================================================
void DrawSubWindow_Data (Update_t bUpdate)
{
//	HDC hDC = g_hDC;
	int iBackground;

	const int nMaxOpcodes = WINDOW_DATA_BYTES_PER_LINE;
	char  sAddress[ 5 ];

	_ASSERT( CONSOLE_WIDTH > WINDOW_DATA_BYTES_PER_LINE );

	char sOpcodes  [ CONSOLE_WIDTH ] = "";
	char sImmediate[ 4 ]; // 'c'

	const int nDefaultFontWidth = 7; // g_aFontConfig[FONT_DISASM_DEFAULT]._nFontWidth or g_nFontWidthAvg
	int X_OPCODE      =  6                    * nDefaultFontWidth;
	int X_CHAR        = (6 + (nMaxOpcodes*3)) * nDefaultFontWidth;

	int iMemDump = 0;

	MemoryDump_t* pMD = &g_aMemDump[ iMemDump ];
	USHORT       nAddress = pMD->nAddress;

//	if (!pMD->bActive)
//		return;

//	int iWindows = g_iThisWindow;
//	WindowSplit_t * pWindow = &g_aWindowConfig[ iWindow ];

	RECT rect;
	rect.top = 0 + 0;

	int  iByte;
	unsigned short iAddress = nAddress;

	int iLine;
	int nLines = g_nDisasmWinHeight;

	for (iLine = 0; iLine < nLines; iLine++ )
	{
		iAddress = nAddress;

	// Format
		sprintf( sAddress, "%04X", iAddress );

		sOpcodes[0] = 0;
		for ( iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			unsigned char nData = (unsigned)*(LPBYTE)(mem + iAddress + iByte);
			sprintf( &sOpcodes[ iByte * 3 ], "%02X ", nData );
		}
		sOpcodes[ nMaxOpcodes * 3 ] = 0;

		int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight;

	// Draw
		rect.left   = 0;
		rect.right  = DISPLAY_DISASM_RIGHT;
		rect.bottom = rect.top + nFontHeight;

		if (iLine & 1)
		{
			iBackground = BG_DATA_1;
		}
		else
		{
			iBackground = BG_DATA_2;
		}
		DebuggerSetColorBG( DebuggerGetColor( iBackground ) );

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ) );
		PrintTextCursorX( (LPCTSTR) sAddress, rect );

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
		if (g_bConfigDisasmAddressColon)
			PrintTextCursorX( ":", rect );

		rect.left = X_OPCODE;

		DebuggerSetColorFG( DebuggerGetColor( FG_DATA_BYTE ) );
		PrintTextCursorX( (LPCTSTR) sOpcodes, rect );

		rect.left = X_CHAR;

	// Separator
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "  |  ", rect );


	// Plain Text
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_CHAR ) );

		MemoryView_e eView = pMD->eView;
		if ((eView != MEM_VIEW_ASCII) && (eView != MEM_VIEW_APPLE))
			eView = MEM_VIEW_ASCII;

		iAddress = nAddress;
		for (iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			unsigned char nImmediate = (unsigned)*(LPBYTE)(mem + iAddress);

			ColorizeSpecialChar( sImmediate, (BYTE) nImmediate, eView, iBackground );
			PrintTextCursorX( (LPCSTR) sImmediate, rect );

			iAddress++;
		}
		DebuggerSetColorBG( DebuggerGetColor( iBackground ) ); // HACK: Colorize() can "spill over" to EOL

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "  |  ", rect );

		nAddress += nMaxOpcodes;

		rect.top    += nFontHeight;
	}
}

void DrawVideoScannerValue(int line, int vert, int horz, bool isVisible)
{
	if (!((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	const int nFontWidth = g_aFontConfig[FONT_INFO]._nFontWidthAvg;

	const int nameWidth = 2;	// 2 chars
	const int numberWidth = 3;	// 3 chars
	const int gapWidth = 1;		// 1 space
	const int totalWidth = (nameWidth + numberWidth) * 2 + gapWidth;

	RECT rect;
	rect.top = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left = DISPLAY_VIDEO_SCANNER_COLUMN;
	rect.right = rect.left + (totalWidth * nFontWidth);

	for (int i = 0; i < 2; i++)
	{
		DebuggerSetColorBG(DebuggerGetColor(BG_VIDEOSCANNER_TITLE));
		DebuggerSetColorFG(DebuggerGetColor(FG_VIDEOSCANNER_TITLE));

		const int nValue = (i == 0) ? vert : horz;

		if (i == 0) PrintText("v:", rect);
		else        PrintText("h:", rect);
		rect.left += nameWidth * nFontWidth;

		char sValue[8];
		if (g_videoScannerDisplayInfo.isDecimal)
			snprintf(sValue, sizeof(sValue), "%03u", nValue);
		else
			snprintf(sValue, sizeof(sValue), "%03X", nValue);

		if (!isVisible)
			DebuggerSetColorFG(DebuggerGetColor(FG_VIDEOSCANNER_INVISIBLE));	// red
		else
			DebuggerSetColorFG(DebuggerGetColor(FG_VIDEOSCANNER_VISIBLE));		// green
		PrintText(sValue, rect);
		rect.left += (numberWidth+gapWidth) * nFontWidth;
	}
}

//===========================================================================

void DrawVideoScannerInfo (int line)
{
#ifdef TODO // Not supported for Linux yet
	NTSC_VideoGetScannerAddressForDebugger();		// update g_nVideoClockHorz/g_nVideoClockVert

	int v = g_nVideoClockVert;
	int h = g_nVideoClockHorz;

	if (g_videoScannerDisplayInfo.isHorzReal)
	{
		h -= 13;	// UTA2e ref?

		if (h < 0)
		{
			h = h + NTSC_GetCyclesPerLine();
			v = v - 1;
			if (v < 0)
				v = v + NTSC_GetVideoLines();
		}
	}

	if (g_nCumulativeCycles != g_videoScannerDisplayInfo.lastCumulativeCycles)
	{
		g_videoScannerDisplayInfo.cycleDelta = (unsigned int) (g_nCumulativeCycles - g_videoScannerDisplayInfo.lastCumulativeCycles);
		g_videoScannerDisplayInfo.lastCumulativeCycles = g_nCumulativeCycles;
	}

	DrawVideoScannerValue(line, v, h, NTSC_IsVisible());
	line++;

	//

	const int nFontWidth = g_aFontConfig[FONT_INFO]._nFontWidthAvg;
	const int nameWidth = 7;
	const int numberWidth = 8;
	const int totalWidth = nameWidth + numberWidth;

	RECT rect;
	rect.top = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left = DISPLAY_VIDEO_SCANNER_COLUMN;
	rect.right = rect.left + (totalWidth * nFontWidth);

	DebuggerSetColorBG(DebuggerGetColor(BG_VIDEOSCANNER_TITLE));
	DebuggerSetColorFG(DebuggerGetColor(FG_VIDEOSCANNER_TITLE));

	PrintText("cycles:", rect);
	rect.left += nameWidth * nFontWidth;

	char sValue[10];
	const unsigned int cycles = g_videoScannerDisplayInfo.isAbsCycle ? (unsigned int)g_nCumulativeCycles : g_videoScannerDisplayInfo.cycleDelta;
	snprintf(sValue, sizeof(sValue), "%08X", cycles);
	PrintText(sValue, rect);
#endif
}

//===========================================================================
void DrawSubWindow_Info ( Update_t bUpdate, int iWindow )
{
	if (g_iWindowThis == WINDOW_CONSOLE)
		return;

	// Left Side
		int yRegs     = 0; // 12
		int yStack    = yRegs  + MAX_DISPLAY_REGS_LINES  + 0; // 0
		int yTarget   = yStack + MAX_DISPLAY_STACK_LINES - 1; // 9
		int yZeroPage = 16; // yTarget
		int ySoft = yZeroPage + (2 * MAX_DISPLAY_ZEROPAGE_LINES) + !SOFTSWITCH_LANGCARD;
		int yBeam = ySoft - 3;

		if (bUpdate & UPDATE_VIDEOSCANNER)
			DrawVideoScannerInfo(yBeam);

		if ((bUpdate & UPDATE_REGS) || (bUpdate & UPDATE_FLAGS))
			DrawRegisters( yRegs );

		if (bUpdate & UPDATE_STACK)
			DrawStack( yStack );

		// 2.7.0.2 Fixed: Debug build of debugger force display all CPU info window wasn't calling DrawTargets()
		bool bForceDisplayTargetPtr = DEBUG_FORCE_DISPLAY || (g_bConfigInfoTargetPointer);
		if (bForceDisplayTargetPtr || (bUpdate & UPDATE_TARGETS))
			DrawTargets( yTarget );

		if (bUpdate & UPDATE_ZERO_PAGE)
			DrawZeroPagePointers( yZeroPage );

		DrawSoftSwitches( ySoft );

	#if defined(SUPPORT_Z80_EMU) && defined(OUTPUT_Z80_REGS)
		DrawRegister( 19,"AF",2,*(unsigned short*)(membank+REG_AF));
		DrawRegister( 20,"BC",2,*(unsigned short*)(membank+REG_BC));
		DrawRegister( 21,"DE",2,*(unsigned short*)(membank+REG_DE));
		DrawRegister( 22,"HL",2,*(unsigned short*)(membank+REG_HL));
		DrawRegister( 23,"IX",2,*(unsigned short*)(membank+REG_IX));
	#endif

	// Right Side
		int yBreakpoints = 0;
		int yWatches     = yBreakpoints + MAX_BREAKPOINTS; // MAX_DISPLAY_BREAKPOINTS_LINES; // 7
		const unsigned int numVideoScannerInfoLines = 4;		// There used to be 2 extra watches (and each watch is 2 lines)
		int yMemory      = yWatches + numVideoScannerInfoLines + (MAX_WATCHES*2); // MAX_DISPLAY_WATCHES_LINES    ; // 14 // 2.7.0.15 Fixed: Memory Dump was over-writing watches

	//	if ((MAX_DISPLAY_BREAKPOINTS_LINES + MAX_DISPLAY_WATCHES_LINES) < 12)
	//		yWatches++;

		bool bForceDisplayBreakpoints = DEBUG_FORCE_DISPLAY || (g_nBreakpoints > 0); // 2.7.0.11 Fixed: Breakpoints and Watches no longer disappear.
		if ( bForceDisplayBreakpoints || (bUpdate & UPDATE_BREAKPOINTS))
			DrawBreakpoints( yBreakpoints );

		bool bForceDisplayWatches = DEBUG_FORCE_DISPLAY || (g_nWatches > 0); // 2.7.0.11 Fixed: Breakpoints and Watches no longer disappear.
		if ( bForceDisplayWatches || (bUpdate & UPDATE_WATCH))
			DrawWatches( yWatches );

		g_nDisplayMemoryLines = MAX_DISPLAY_MEMORY_LINES_1;

		bool bForceDisplayMemory1 = DEBUG_FORCE_DISPLAY || (g_aMemDump[0].bActive);
		if (bForceDisplayMemory1 || (bUpdate & UPDATE_MEM_DUMP))
			DrawMemory( yMemory, 0 ); // g_aMemDump[0].nAddress, g_aMemDump[0].eDevice);

		yMemory += (g_nDisplayMemoryLines + 1);
		g_nDisplayMemoryLines = MAX_DISPLAY_MEMORY_LINES_2;

		bool bForceDisplayMemory2 = DEBUG_FORCE_DISPLAY || (g_aMemDump[1].bActive);
		if (bForceDisplayMemory2 || (bUpdate & UPDATE_MEM_DUMP))
			DrawMemory( yMemory, 1 ); // g_aMemDump[1].nAddress, g_aMemDump[1].eDevice);
}

//===========================================================================
void DrawSubWindow_IO (Update_t bUpdate)
{
}

//===========================================================================
void DrawSubWindow_Source (Update_t bUpdate)
{
}


//===========================================================================
void DrawSubWindow_Source2 (Update_t bUpdate)
{
//	if (! g_bSourceLevelDebugging)
//		return;

	DebuggerSetColorFG( DebuggerGetColor( FG_SOURCE ));

	int nLines  = g_nDisasmWinHeight;

	int y = g_nDisasmWinHeight;
	int nHeight = g_nDisasmWinHeight;

	if (g_aWindowConfig[ g_iWindowThis ].bSplit) // HACK: Split Window Height is odd, so bottom window gets +1 height
		nHeight++;

	RECT rect;
	rect.top    = (y * g_nFontHeight);
	rect.bottom = rect.top + (nHeight * g_nFontHeight);
	rect.left = 0;
	rect.right = DISPLAY_DISASM_RIGHT; // HACK: MAGIC #: 7

// Draw Title
	std::string sTitle = "   Source: ";
	sTitle += g_aSourceFileName;
	sTitle.resize(min(sTitle.size(), size_t(g_nConsoleDisplayWidth)));

	DebuggerSetColorBG( DebuggerGetColor( BG_SOURCE_TITLE ));
	DebuggerSetColorFG( DebuggerGetColor( FG_SOURCE_TITLE ));
	PrintText( sTitle.c_str(), rect );
	rect.top += g_nFontHeight;

// Draw Source Lines
	int iBackground;
	int iForeground;

	int iSourceCursor = 2; // (g_nDisasmWinHeight / 2);
	int iSourceLine = FindSourceLine( regs.pc );

	if (iSourceLine == NO_SOURCE_LINE)
	{
		iSourceCursor = NO_SOURCE_LINE;
	}
	else
	{
		iSourceLine -= iSourceCursor;
		if (iSourceLine < 0)
			iSourceLine = 0;
	}

	for( int iLine = 0; iLine < nLines; iLine++ )
	{
		if (iLine != iSourceCursor)
		{
			iBackground = BG_SOURCE_1;
			if (iLine & 1)
				iBackground = BG_SOURCE_2;
			iForeground = FG_SOURCE;
		}
		else
		{
			// Hilighted cursor line
			iBackground = BG_DISASM_PC_X; // _C
			iForeground = FG_DISASM_PC_X; // _C
		}
		DebuggerSetColorBG( DebuggerGetColor( iBackground ));
		DebuggerSetColorFG( DebuggerGetColor( iForeground ));

		DrawSourceLine( iSourceLine, rect );
		iSourceLine++;
	}
}

//===========================================================================
void DrawSubWindow_Symbols (Update_t bUpdate)
{
}

//===========================================================================
void DrawSubWindow_ZeroPage (Update_t bUpdate)
{
}


//===========================================================================
void DrawWindow_Code( Update_t bUpdate )
{
	DrawSubWindow_Code( g_iWindowThis );

//	DrawWindowTop( g_iWindowThis );
	DrawWindowBottom( bUpdate, g_iWindowThis );

	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

// Full Screen console
//===========================================================================
void DrawWindow_Console( Update_t bUpdate )
{
	// Nothing to do, since text and draw background handled by DrawSubWindow_Console()
	// If the full screen console is only showing partial lines
	// don't erase the background

	//		FillRect(&rect, g_hConsoleBrushBG );
}

//===========================================================================
void DrawWindow_Data( Update_t bUpdate )
{
	DrawSubWindow_Data( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

//===========================================================================
void DrawWindow_IO( Update_t bUpdate )
{
	DrawSubWindow_IO( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

//===========================================================================
void DrawWindow_Source( Update_t bUpdate )
{
	DrawSubWindow_Source( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

//===========================================================================
void DrawWindow_Symbols( Update_t bUpdate )
{
	DrawSubWindow_Symbols( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

void DrawWindow_ZeroPage( Update_t bUpdate )
{
	DrawSubWindow_ZeroPage( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

//===========================================================================
void DrawWindowBackground_Main( int g_iWindowThis )
{
	// TODO/FIXME: COLOR_BG_CODE -> g_iWindowThis, once all tab backgrounds are listed first in g_aColors !
	DebuggerSetColorBG( DebuggerGetColor( BG_DISASM_1 )); // COLOR_BG_CODE

#if !DEBUG_FONT_NO_BACKGROUND_FILL_MAIN
	RECT rect;
	rect.left   = 0;
	rect.top    = 0;
	rect.right  = DISPLAY_DISASM_RIGHT;
	int nTop = GetConsoleTopPixels( g_nConsoleDisplayLines - 1 );
	rect.bottom = nTop;
	FillRect(&rect, g_hConsoleBrushBG );
#endif
}

//===========================================================================
void DrawWindowBackground_Info( int g_iWindowThis )
{
	DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA

#if !DEBUG_FONT_NO_BACKGROUND_FILL_INFO
    RECT rect;
    rect.top    = 0;
    rect.left   = DISPLAY_DISASM_RIGHT;
    rect.right  = DISPLAY_WIDTH;
	int nTop = GetConsoleTopPixels( g_nConsoleDisplayLines - 1 );
	rect.bottom = nTop;
	FillRect(&rect, g_hConsoleBrushBG );
#endif
}

//===========================================================================
void UpdateDisplay (Update_t bUpdate)
{
	static int spDrawMutex = false;

	if (spDrawMutex)
	{
#if DEBUG
		MessageBox( g_hFrameWindow, "Already drawing!", "!", MB_OK );
#endif
	}
	spDrawMutex = true;

	// Hack: Full screen console scrolled, "erase" left over console lines
	if (g_iWindowThis == WINDOW_CONSOLE)
		bUpdate |= UPDATE_BACKGROUND;

	if (bUpdate & UPDATE_BACKGROUND)
	{
#ifndef _WIN32
	  // linux
#elif USE_APPLE_FONT
		SetBkMode( GetDebuggerMemDC(), OPAQUE);
		SetBkColor(GetDebuggerMemDC(), RGB(0,0,0));
#else
		SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_INFO ]._hFont ); // g_hFontDebugger
#endif
	}

#ifdef _WIN32
	SetTextAlign( GetDebuggerMemDC(), TA_TOP | TA_LEFT);
#endif

	if ((bUpdate & UPDATE_BREAKPOINTS)
//		|| (bUpdate & UPDATE_DISASM)
		|| (bUpdate & UPDATE_FLAGS)
		|| (bUpdate & UPDATE_MEM_DUMP)
		|| (bUpdate & UPDATE_REGS)
		|| (bUpdate & UPDATE_STACK)
		|| (bUpdate & UPDATE_SYMBOLS)
		|| (bUpdate & UPDATE_TARGETS)
		|| (bUpdate & UPDATE_WATCH)
		|| (bUpdate & UPDATE_ZERO_PAGE))
	{
		bUpdate |= UPDATE_BACKGROUND;
		bUpdate |= UPDATE_CONSOLE_INPUT;
	}

	if (bUpdate & UPDATE_BACKGROUND)
	{
		if (g_iWindowThis != WINDOW_CONSOLE)
		{
			DrawWindowBackground_Main( g_iWindowThis );
			DrawWindowBackground_Info( g_iWindowThis );
		}
	}

	switch( g_iWindowThis )
	{
		case WINDOW_CODE:
			DrawWindow_Code( bUpdate );
			break;

		case WINDOW_CONSOLE:
			DrawWindow_Console( bUpdate );
			break;

		case WINDOW_DATA:
			DrawWindow_Data( bUpdate );
			break;

		case WINDOW_IO:
			DrawWindow_IO( bUpdate );
			break;

		case WINDOW_SOURCE:
			DrawWindow_Source( bUpdate );
			break;

		case WINDOW_SYMBOLS:
			DrawWindow_Symbols( bUpdate );
			break;

		case WINDOW_ZEROPAGE:
			DrawWindow_ZeroPage( bUpdate );
			break;

		default:
			break;
	}

	if ((bUpdate & UPDATE_CONSOLE_DISPLAY) || (bUpdate & UPDATE_CONSOLE_INPUT))
		DrawSubWindow_Console( bUpdate );

	StretchBltMemToFrameDC();

	spDrawMutex = false;
}

//===========================================================================
void DrawWindowBottom ( Update_t bUpdate, int iWindow )
{
	if (! g_aWindowConfig[ iWindow ].bSplit)
		return;

	WindowSplit_t * pWindow = &g_aWindowConfig[ iWindow ];
	g_pDisplayWindow = pWindow;

	if (pWindow->eBot == WINDOW_DATA)
		DrawWindow_Data( bUpdate ); // false
	else
	if (pWindow->eBot == WINDOW_SOURCE)
		DrawSubWindow_Source2( bUpdate );
}

//===========================================================================
void DrawSubWindow_Code ( int iWindow )
{
	int nLines = g_nDisasmWinHeight;

//	WindowSplit_t * pWindow = &g_aWindowConfig[ iWindow ];

	// Check if we have a bad disasm
	// BUG: This still doesn't catch all cases
	// G FB53, SPACE, PgDn *
	// Note: DrawDisassemblyLine() has kludge.
//		DisasmCalcTopFromCurAddress( false );
	// These should be functionally equivalent.
	//	DisasmCalcTopFromCurAddress();
	//	DisasmCalcBotFromTopAddress();
#if !USE_APPLE_FONT
	SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont );
#endif

	unsigned short nAddress = g_nDisasmTopAddress;
	for (int iLine = 0; iLine < nLines; iLine++ ) {
		nAddress += DrawDisassemblyLine( iLine, nAddress );
	}

#if !USE_APPLE_FONT
	SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_INFO ]._hFont );
#endif
}
