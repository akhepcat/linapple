/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2004 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/


/* This a stretch blit implementation based on ideas given to me by
   Tomasz Cejner - thanks! :)

   April 27, 2000 - Sam Lantinga
*/


/* This isn't ready for general consumption yet - it should be folded
   into the general blitting mechanism.
*/

// For embedded XPMs.
#include <SDL_image.h>

#include "stdafx.h"
#include "asset.h"

#define DEFINE_COPY_ROW(name, type)      \
void name(type *src, int src_w, type *dst, int dst_w)  \
{              \
  int i;            \
  int pos, inc;          \
  type pixel = 0;          \
              \
  pos = 0x10000;          \
  inc = (src_w << 16) / dst_w;      \
  for ( i=dst_w; i>0; --i ) {      \
    while ( pos >= 0x10000L ) {    \
      pixel = *src++;      \
      pos -= 0x10000L;    \
    }          \
    *dst++ = pixel;        \
    pos += inc;        \
  }            \
}
// the same as above, but pixels ORed with the destination
#define DEFINE_COPY_ROW_OR(name, type)      \
void name(type *src, int src_w, type *dst, int dst_w)  \
{              \
  int i;            \
  int pos, inc;          \
  type pixel = 0;          \
              \
  pos = 0x10000;          \
  inc = (src_w << 16) / dst_w;      \
  for ( i=dst_w; i>0; --i ) {      \
    while ( pos >= 0x10000L ) {    \
      pixel = *src++;      \
      pos -= 0x10000L;    \
}          \
    *dst++ |= pixel;      \
    pos += inc;        \
}            \
}

DEFINE_COPY_ROW(copy_row1, Uint8)
DEFINE_COPY_ROW(copy_row2, Uint16)
DEFINE_COPY_ROW(copy_row4, Uint32)
DEFINE_COPY_ROW_OR(copy_row_or1, Uint8)
DEFINE_COPY_ROW_OR(copy_row_or2, Uint16)
DEFINE_COPY_ROW_OR(copy_row_or4, Uint32)

/* The ASM code doesn't handle 24-bpp stretch blits */
void copy_row3(Uint8 *src, int src_w, Uint8 *dst, int dst_w) {
  int i;
  int pos, inc;
  Uint8 pixel[3];

  pos = 0x10000;
  inc = (src_w << 16) / dst_w;
  for (i = dst_w; i > 0; --i) {
    while (pos >= 0x10000L) {
      pixel[0] = *src++;
      pixel[1] = *src++;
      pixel[2] = *src++;
      pos -= 0x10000L;
    }
    *dst++ = pixel[0];
    *dst++ = pixel[1];
    *dst++ = pixel[2];
    pos += inc;
  }
}

/* Perform a stretch blit between two surfaces of the same format.
   NOTE:  This function is not safe to call from multiple threads!
*/
int SDL_SoftStretchMy(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
  int src_locked;
  int dst_locked;
  int pos, inc;
  int dst_maxrow;
  int src_row, dst_row;
  Uint8 *srcp = NULL;
  Uint8 *dstp;
  SDL_Rect full_src;
  SDL_Rect full_dst;
  const int bpp = dst->format->BytesPerPixel;

  if (src->format->BitsPerPixel != dst->format->BitsPerPixel) {
    SDL_SetError("Only works with same format surfaces");
    return (-1);
  }

  /* Verify the blit rectangles */
  if (srcrect == nullptr) {
    full_src.x = 0;
    full_src.y = 0;
    full_src.w = src->w;
    full_src.h = src->h;
    srcrect = &full_src;
  }
  if (dstrect == nullptr) {
    full_dst.x = 0;
    full_dst.y = 0;
    full_dst.w = dst->w;
    full_dst.h = dst->h;
    dstrect = &full_dst;
  }

  /* Lock the destination if it's in hardware */
  dst_locked = 0;
  if (SDL_MUSTLOCK(dst)) {
    if (SDL_LockSurface(dst) < 0) {
      return -1;
    }
    dst_locked = 1;
  }
  /* Lock the source if it's in hardware */
  src_locked = 0;
  if (SDL_MUSTLOCK(src)) {
    if (SDL_LockSurface(src) < 0) {
      if (dst_locked != 0) {
        SDL_UnlockSurface(dst);
      }
      return (-1);
    }
    src_locked = 1;
  }

  /* Set up the data... */
  pos = 0x10000;
  inc = (srcrect->h << 16) / dstrect->h;
  src_row = srcrect->y;
  dst_row = dstrect->y;


  /* Perform the stretch blit */
  for (dst_maxrow = dst_row + dstrect->h; dst_row < dst_maxrow; ++dst_row) {
    dstp = (Uint8 *) dst->pixels + (dst_row * dst->pitch) + (dstrect->x * bpp);
    while (pos >= 0x10000L) {
      srcp = (Uint8 *) src->pixels + (src_row * src->pitch) + (srcrect->x * bpp);
      ++src_row;
      pos -= 0x10000L;
    }
    switch (bpp) {
      case 1:
        copy_row1(srcp, srcrect->w, dstp, dstrect->w);
        break;
      case 2:
        copy_row2((Uint16 *) srcp, srcrect->w, (Uint16 *) dstp, dstrect->w);
        break;
      case 3:
        copy_row3(srcp, srcrect->w, dstp, dstrect->w);
        break;
      case 4:
        copy_row4((Uint32 *) srcp, srcrect->w, (Uint32 *) dstp, dstrect->w);
        break;
    }
    pos += inc;
  }

  /* We need to unlock the surfaces if they're locked */
  if (dst_locked != 0) {
    SDL_UnlockSurface(dst);
  }
  if (src_locked != 0) {
    SDL_UnlockSurface(src);
  }
  return (0);
}

// MONO8
/* Perform a monochrome stretch blit between two surfaces of the 8bpp format,
   using the given foreground and background color (index from palette).
   NOTE:  This function is not safe to call from multiple threads!
*/
void copy8mono(Uint8 *src, int src_w, Uint8 *dst, int dst_w, Uint8 fgbrush, Uint8 bgbrush) {
  int i;
  int pos, inc;
  Uint8 pixel = 0;
  pos = 0x10000;
  inc = (src_w << 16) / dst_w;
  for (i = dst_w; i > 0; --i) {
    while (pos >= 0x10000L) {
      pixel = *src++;
      pos -= 0x10000L;
    }
    if (pixel != 0u) {
      *dst++ = fgbrush;
    } else {
      *dst++ = bgbrush;
    }
    pos += inc;
  }
}

int SDL_SoftStretchMono8(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect, Uint8 fgbrush, Uint8 bgbrush)
{
  //fgbrush/bgbrush - monochrome color (index from palette)
  int src_locked;
  int dst_locked;
  int pos, inc;
  int dst_maxrow;
  int src_row, dst_row;
  Uint8 *srcp = NULL;
  Uint8 *dstp;
  SDL_Rect full_src;
  SDL_Rect full_dst;
  const int bpp = dst->format->BytesPerPixel;

  /* Verify the blit rectangles */
  if (srcrect == nullptr) {
    full_src.x = 0;
    full_src.y = 0;
    full_src.w = src->w;
    full_src.h = src->h;
    srcrect = &full_src;
  }
  if (dstrect == nullptr) {
    full_dst.x = 0;
    full_dst.y = 0;
    full_dst.w = dst->w;
    full_dst.h = dst->h;
    dstrect = &full_dst;
  }

  /* Lock the destination if it's in hardware */
  dst_locked = 0;
  if (SDL_MUSTLOCK(dst)) {
    if (SDL_LockSurface(dst) < 0) {
      return -1;
    }
    dst_locked = 1;
  }
  /* Lock the source if it's in hardware */
  src_locked = 0;
  if (SDL_MUSTLOCK(src)) {
    if (SDL_LockSurface(src) < 0) {
      if (dst_locked != 0) {
        SDL_UnlockSurface(dst);
      }
      return (-1);
    }
    src_locked = 1;
  }

  /* Set up the data... */
  pos = 0x10000;
  inc = (srcrect->h << 16) / dstrect->h;
  src_row = srcrect->y;
  dst_row = dstrect->y;


  /* Perform the stretch blit */
  for (dst_maxrow = dst_row + dstrect->h; dst_row < dst_maxrow; ++dst_row) {
    dstp = (Uint8 *) dst->pixels + (dst_row * dst->pitch) + (dstrect->x * bpp);
    while (pos >= 0x10000L) {
      srcp = (Uint8 *) src->pixels + (src_row * src->pitch) + (srcrect->x * bpp);
      ++src_row;
      pos -= 0x10000L;
    }
    switch (bpp) {
      case 1:
        copy8mono(srcp, srcrect->w, dstp, dstrect->w, fgbrush, bgbrush);
        break;
      default:
        break;
    }
    pos += inc;
  }

  /* We need to unlock the surfaces if they're locked */
  if (dst_locked != 0) {
    SDL_UnlockSurface(dst);
  }
  if (src_locked != 0) {
    SDL_UnlockSurface(src);
  }
  return (0);
}

/*  SDL_SoftStretchOr  - the same as SDL_SoftStretch, but ORed with destination
  NOTE: 24bpp does not support  -- beom beotiger 2007 November
*/

/* Perform a stretch blit between two surfaces of the same format.
   NOTE:  This function is not safe to call from multiple threads!
*/
int SDL_SoftStretchOr(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
  int src_locked;
  int dst_locked;
  int pos, inc;
  int dst_maxrow;
  int src_row, dst_row;
  Uint8 *srcp = NULL;
  Uint8 *dstp;
  SDL_Rect full_src;
  SDL_Rect full_dst;
  const int bpp = dst->format->BytesPerPixel;

  if (src->format->BitsPerPixel != dst->format->BitsPerPixel) {
    SDL_SetError("Only works with same format surfaces");
    return (-1);
  }

  /* Verify the blit rectangles */
  if (srcrect == nullptr) {
    full_src.x = 0;
    full_src.y = 0;
    full_src.w = src->w;
    full_src.h = src->h;
    srcrect = &full_src;
  }
  if (dstrect == nullptr) {
    full_dst.x = 0;
    full_dst.y = 0;
    full_dst.w = dst->w;
    full_dst.h = dst->h;
    dstrect = &full_dst;
  }

  /* Lock the destination if it's in hardware */
  dst_locked = 0;
  if (SDL_MUSTLOCK(dst)) {
    if (SDL_LockSurface(dst) < 0) {
      return -1;
    }
    dst_locked = 1;
  }
  /* Lock the source if it's in hardware */
  src_locked = 0;
  if (SDL_MUSTLOCK(src)) {
    if (SDL_LockSurface(src) < 0) {
      if (dst_locked != 0) {
        SDL_UnlockSurface(dst);
      }
      return (-1);
    }
    src_locked = 1;
  }

  /* Set up the data... */
  pos = 0x10000;
  inc = (srcrect->h << 16) / dstrect->h;
  src_row = srcrect->y;
  dst_row = dstrect->y;


  /* Perform the stretch blit */
  for (dst_maxrow = dst_row + dstrect->h; dst_row < dst_maxrow; ++dst_row) {
    dstp = (Uint8 *) dst->pixels + (dst_row * dst->pitch) + (dstrect->x * bpp);
    while (pos >= 0x10000L) {
      srcp = (Uint8 *) src->pixels + (src_row * src->pitch) + (srcrect->x * bpp);
      ++src_row;
      pos -= 0x10000L;
    }
    switch (bpp) {
      case 1:
        copy_row_or1(srcp, srcrect->w, dstp, dstrect->w);
        break;
      case 2:
        copy_row_or2((Uint16 *) srcp, srcrect->w, (Uint16 *) dstp, dstrect->w);
        break;
      case 3:
        copy_row3(srcp, srcrect->w, dstp, dstrect->w);
        break;
      case 4:
        copy_row_or4((Uint32 *) srcp, srcrect->w, (Uint32 *) dstp, dstrect->w);
        break;
    }
    pos += inc;
  }

  /* We need to unlock the surfaces if they're locked */
  if (dst_locked != 0) {
    SDL_UnlockSurface(dst);
  }
  if (src_locked != 0) {
    SDL_UnlockSurface(src);
  }
  return (0);
}

// Font routines
SDL_Surface *font_sfc = NULL;  // used for font

bool fonts_initialization(void) {
  font_sfc = SDL_DisplayFormat(assets->font);
  /* Transparant color is BLACK: */
  SDL_SetColorKey(font_sfc, SDL_SRCCOLORKEY, SDL_MapRGB(font_sfc->format, 0, 0, 0));

  return true;
}

void fonts_termination(void) {
  SDL_FreeSurface(font_sfc);
  font_sfc = NULL;
}

void font_print(int x, int y, const char *text, SDL_Surface *surface, double kx, double ky)
{
  // kx, ky - stretching koefficients for width and height of destination chars respectively
  int i, c;
  SDL_Rect s, d;

  for (i = 0; text[i] != 0 && x < surface->w; i++) {
    int row;
    c = int(text[i]);

    if (c > 127 || c < 0) {
      c = '?';  // cut-off non-ASCII chars
    }

    row = c / CHARS_IN_ROW;

    s.x = (c - (row * CHARS_IN_ROW)) * (FONT_SIZE_X + 1) + 1;
    s.y = (row) * (FONT_SIZE_Y + 1) + 1;
    s.h = FONT_SIZE_Y;
    s.w = FONT_SIZE_X;

    d.x = x + i * FONT_SIZE_X * kx;
    d.y = y;
    d.w = s.w * kx;
    d.h = s.h * ky;
    SDL_SoftStretchOr(font_sfc, &s, surface, &d);
  }
}

void font_print_right(int x, int y, const char *text, SDL_Surface *surface, double kx, double ky)
{
  // kx, ky - stretching koefficients for width and height of destination chars respectively
  int i, c;
  SDL_Rect s, d;

  x -= strlen(text) * FONT_SIZE_X * kx;

  for (i = 0; text[i] != 0 && x < surface->w; i++) {
    int row;
    c = int(text[i]);
    if (c > 127 || c < 0) {
      c = '?';  // cut-off non-ASCII chars
    }

    row = c / CHARS_IN_ROW;
    s.x = (c - (row * CHARS_IN_ROW)) * (FONT_SIZE_X + 1) + 1;
    s.y = (row) * (FONT_SIZE_Y + 1) + 1;
    s.h = FONT_SIZE_Y;
    s.w = FONT_SIZE_X;

    d.x = x + i * FONT_SIZE_X * kx;
    d.y = y;
    d.w = s.w * kx;
    d.h = s.h * ky;
    SDL_SoftStretchOr(font_sfc, &s, surface, &d);
  }
}

void font_print_centered(int x, int y, const char *text, SDL_Surface *surface, double kx, double ky)
{
  // kx, ky - stretching koefficients for width and height of destination chars respectively
  int i, c;
  SDL_Rect s, d;

  x -= strlen(text) * FONT_SIZE_X * kx / 2;  // centered position
  if (x < 0) {
    x = 0;
  }

  for (i = 0; text[i] != 0 && ((x * kx) < surface->w); i++) {
    int row;
    c = int(text[i]);
    if (c > 127 || c < 0) {
      c = '?';  // cut-off non-ASCII chars
    }

    row = c / CHARS_IN_ROW;
    s.x = (c - (row * CHARS_IN_ROW)) * (FONT_SIZE_X + 1) + 1;
    s.y = (row) * (FONT_SIZE_Y + 1) + 1;
    s.h = FONT_SIZE_Y;
    s.w = FONT_SIZE_X;

    d.x = x + i * FONT_SIZE_X * kx;
    d.y = y;
    d.w = s.w * kx;
    d.h = s.h * ky;
    SDL_SoftStretchOr(font_sfc, &s, surface, &d);
  }
}

// Some auxiliary functions
void surface_fader(SDL_Surface *surface, float r_factor, float g_factor, float b_factor, float a_factor, SDL_Rect *r) {
  // my rebiuld for 8BPP palettized surfaces! --bb
  SDL_Rect r2;
  int i;
  SDL_Color mycolors[256];  // faded colors
  SDL_Color *colors;

  if (r == 0) {
    r2.x = 0;
    r2.y = 0;
    r2.w = surface->w;
    r2.h = surface->h;
    r = &r2;
  }

  // fading just for 8BPP surfaces!
  if (surface->format->BytesPerPixel != 1) {
    return;
  }

  colors = (SDL_Color *) surface->format->palette->colors; // get pointer to origin colors
  for (i = 0; i < 256; i++) {
    mycolors[i].r = (Uint8)(colors[i].r * r_factor);
    mycolors[i].g = (Uint8)(colors[i].g * g_factor);
    mycolors[i].b = (Uint8)(colors[i].b * b_factor);
  }

  SDL_SetColors(surface, mycolors, 0, 256);
}

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel) {
  SDL_Rect clip;
  int bpp = surface->format->BytesPerPixel;

  SDL_GetClipRect(surface, &clip);

  if (x < clip.x || x >= clip.x + clip.w || y < clip.y || y >= clip.y + clip.h) {
    return;
  }

  if (x < 0 || x >= surface->w || y < 0 || y >= surface->h) {
    return;
  }

  Uint8 *p = (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;

  switch (bpp) {
    case 1:
      *p = pixel;
      break;

    case 2:
      *(Uint16 *) p = pixel;
      break;

    case 3:
      if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
        p[0] = (pixel >> 16) & 0xff;
        p[1] = (pixel >> 8) & 0xff;
        p[2] = pixel & 0xff;
      } else {
        p[0] = pixel & 0xff;
        p[1] = (pixel >> 8) & 0xff;
        p[2] = (pixel >> 16) & 0xff;
      }
      break;

    case 4:
      *(Uint32 *) p = pixel;
      break;
  }
}

void rectangle(SDL_Surface *surface, int x, int y, int w, int h, Uint32 pixel) {
  int i;

  for (i = 0; i < w; i++) {
    putpixel(surface, x + i, y, pixel);
    putpixel(surface, x + i, y + h, pixel);
  } /* for */
  for (i = 0; i <= h; i++) {
    putpixel(surface, x, y + i, pixel);
    putpixel(surface, x + w, y + i, pixel);
  }
}
