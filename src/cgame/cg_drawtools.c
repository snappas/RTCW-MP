/*
===========================================================================

Return to Castle Wolfenstein multiplayer GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein multiplayer GPL Source Code (RTCW MP Source Code).  

RTCW MP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW MP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW MP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW MP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW MP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// cg_drawtools.c -- helper functions called by cg_draw, cg_scoreboard, cg_info, etc
#include "cg_local.h"

/*
================
CG_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void CG_AdjustFrom640( float *x, float *y, float *w, float *h ) {
#if 0
	// adjust for wide screens
	if ( cgs.glconfig.vidWidth * 480 > cgs.glconfig.vidHeight * 640 ) {
		*x += 0.5 * ( cgs.glconfig.vidWidth - ( cgs.glconfig.vidHeight * 640 / 480 ) );
	}
#endif

	// NERVE - SMF - hack to make images display properly in small view / limbo mode
	if ( cg.limboMenu && cg.refdef.width ) {
		float xscale = ( ( cg.refdef.width / cgs.screenXScale ) / 640.f );
		float yscale = ( ( cg.refdef.height / cgs.screenYScale ) / 480.f );

		( *x ) = ( *x ) * xscale + ( cg.refdef.x / cgs.screenXScale );
		( *y ) = ( *y ) * yscale + ( cg.refdef.y / cgs.screenYScale );
		( *w ) *= xscale;
		( *h ) *= yscale;
	}
	// -NERVE - SMF

	// scale for screen sizes
	*x *= cgs.screenXScale;
	*y *= cgs.screenYScale;
	*w *= cgs.screenXScale;
	*h *= cgs.screenYScale;
}

/*
================
CG_FillRect

Coordinates are 640*480 virtual values
=================
*/
void CG_FillRect( float x, float y, float width, float height, const float *color ) {
	trap_R_SetColor( color );

	CG_AdjustFrom640( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 0, 1, cgs.media.whiteShader );

	trap_R_SetColor( NULL );
}

/*
==============
CG_FillRectGradient
==============
*/
void CG_FillRectGradient( float x, float y, float width, float height, const float *color, const float *gradcolor, int gradientType ) {
	trap_R_SetColor( color );

	CG_AdjustFrom640( &x, &y, &width, &height );
	trap_R_DrawStretchPicGradient( x, y, width, height, 0, 0, 0, 0, cgs.media.whiteShader, gradcolor, gradientType );

	trap_R_SetColor( NULL );
}


/*
==============
CG_HorizontalPercentBar
	Generic routine for pretty much all status indicators that show a fractional
	value to the palyer by virtue of how full a drawn box is.

flags:
	left		- 1
	center		- 2		// direction is 'right' by default and orientation is 'horizontal'
	vert		- 4
	nohudalpha	- 8		// don't adjust bar's alpha value by the cg_hudalpha value
	bg			- 16	// background contrast box (bg set with bgColor of 'NULL' means use default bg color (1,1,1,0.25)
	spacing		- 32	// some bars use different sorts of spacing when drawing both an inner and outer box

	lerp color	- 256	// use an average of the start and end colors to set the fill color
==============
*/


// TODO: these flags will be shared, but it was easier to work on stuff if I wasn't changing header files a lot
#define BAR_LEFT        0x0001
#define BAR_CENTER      0x0002
#define BAR_VERT        0x0004
#define BAR_NOHUDALPHA  0x0008
#define BAR_BG          0x0010
// different spacing modes for use w/ BAR_BG
#define BAR_BGSPACING_X0Y5  0x0020
#define BAR_BGSPACING_X0Y0  0x0040

#define BAR_LERP_COLOR  0x0100

#define BAR_BORDERSIZE 2

void CG_FilledBar( float x, float y, float w, float h, float *startColor, float *endColor, const float *bgColor, float frac, int flags ) {
	vec4_t backgroundcolor = {1, 1, 1, 0.25f}, colorAtPos;  // colorAtPos is the lerped color if necessary
	int indent = BAR_BORDERSIZE;

	if ( ( flags & BAR_BG ) && bgColor ) { // BAR_BG set, and color specified, use specified bg color
		Vector4Copy( bgColor, backgroundcolor );
	}

	// hud alpha
	if ( !( flags & BAR_NOHUDALPHA ) ) {
		startColor[3] *= cg_hudAlpha.value;
		if ( endColor ) {
			endColor[3] *= cg_hudAlpha.value;
		}
		backgroundcolor[3] *= cg_hudAlpha.value;
		
	}

	if ( flags & BAR_LERP_COLOR ) {
		Vector4Average( startColor, endColor, frac, colorAtPos );
	}

	// background
	if ( ( flags & BAR_BG ) ) {
		// draw background at full size and shrink the remaining box to fit inside with a border.  (alternate border may be specified by a BAR_BGSPACING_xx)
		CG_FillRect(   x,
					   y,
					   w,
					   h,
					   backgroundcolor );

		if ( flags & BAR_BGSPACING_X0Y0 ) {          // fill the whole box (no border)

		} else if ( flags & BAR_BGSPACING_X0Y5 ) {   // spacing created for weapon heat
			indent *= 3;
			y += indent;
			h -= ( 2 * indent );

		} else {                                // default spacing of 2 units on each side
			x += indent;
			y += indent;
			w -= ( 2 * indent );
			h -= ( 2 * indent );
		}
	}


	// adjust for horiz/vertical and draw the fractional box
	if ( flags & BAR_VERT ) {
		if ( flags & BAR_LEFT ) {    // TODO: remember to swap colors on the ends here
			y += ( h * ( 1 - frac ) );
		} else if ( flags & BAR_CENTER ) {
			y += ( h * ( 1 - frac ) / 2 );
		}

		if ( flags & BAR_LERP_COLOR ) {
			CG_FillRect( x, y, w, h * frac, colorAtPos );
		} else {
//			CG_FillRectGradient ( x, y, w, h * frac, startColor, endColor, 0 );
			CG_FillRect( x, y, w, h * frac, startColor );
		}

	} else {

		if ( flags & BAR_LEFT ) {    // TODO: remember to swap colors on the ends here
			x += ( w * ( 1 - frac ) );
		} else if ( flags & BAR_CENTER ) {
			x += ( w * ( 1 - frac ) / 2 );
		}

		if ( flags & BAR_LERP_COLOR ) {
			CG_FillRect( x, y, w * frac, h, colorAtPos );
		} else {
//			CG_FillRectGradient ( x, y, w * frac, h, startColor, endColor, 0 );
			CG_FillRect( x, y, w * frac, h, startColor );
		}
	}

}


/*
=================
CG_HorizontalPercentBar
=================
*/
void CG_HorizontalPercentBar( float x, float y, float width, float height, float percent ) {
	vec4_t bgcolor = {0.5f, 0.5f, 0.5f, 0.3f},
		   color = {1.0f, 1.0f, 1.0f, 0.3f};
	CG_FilledBar( x, y, width, height, color, NULL, bgcolor, percent, BAR_BG | BAR_NOHUDALPHA );
}

/*
=================
CG_DrawMotd
=================
*/
void CG_DrawMotd() {
	const char *s;
	vec4_t color = { 0.5f, 0.5f, 0.5f, 0.3f };
	int len;

	s = CG_ConfigString( CS_MOTD );
	if ( s[0] ) {
		CG_FillRect( 0, 448, 640, 14, color );
		len = (int)( (float)UI_ProportionalStringWidth( s ) * UI_ProportionalSizeScale( UI_EXSMALLFONT ) / 2 );
		CG_DrawStringExt( 320 - len, 445, s, colorWhite, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
	}
}

/*
================
CG_DrawSides

Coords are virtual 640x480
================
*/
void CG_DrawSides( float x, float y, float w, float h, float size ) {
	CG_AdjustFrom640( &x, &y, &w, &h );
	size *= cgs.screenXScale;
	trap_R_DrawStretchPic( x, y, size, h, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_DrawStretchPic( x + w - size, y, size, h, 0, 0, 0, 0, cgs.media.whiteShader );
}

void CG_DrawTopBottom( float x, float y, float w, float h, float size ) {
	CG_AdjustFrom640( &x, &y, &w, &h );
	size *= cgs.screenYScale;
	trap_R_DrawStretchPic( x, y, w, size, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_DrawStretchPic( x, y + h - size, w, size, 0, 0, 0, 0, cgs.media.whiteShader );
}

/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void CG_DrawRect( float x, float y, float width, float height, float size, const float *color ) {
	vec4_t hudAlphaColor;

	Vector4Copy( color, hudAlphaColor );
	hudAlphaColor[3] *= cg_hudAlpha.value;

	trap_R_SetColor( hudAlphaColor );

	CG_DrawTopBottom( x, y, width, height, size );
	CG_DrawSides( x, y, width, height, size );

	trap_R_SetColor( NULL );
}



/*
================
CG_DrawPic

Coordinates are 640*480 virtual values
=================
*/
void CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	CG_AdjustFrom640( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}

// NERVE - SMF
/*
================
CG_DrawRotatedPic

Coordinates are 640*480 virtual values
=================
*/
void CG_DrawRotatedPic( float x, float y, float width, float height, qhandle_t hShader, float angle ) {

	CG_AdjustFrom640( &x, &y, &width, &height );

	trap_R_DrawRotatedPic( x, y, width, height, 0, 0, 1, 1, hShader, angle );
}
// -NERVE - SMF

/*
===============
CG_DrawChar

Coordinates and size in 640*480 virtual screen size
===============
*/
void CG_DrawChar( float x, float y, int width, int height, int ch ) {
	int row, col;
	float frow, fcol;
	float size;
	float ax, ay, aw, ah;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	ax = x;
	ay = y;
	aw = width;
	ah = height;
	CG_AdjustFrom640( &ax, &ay, &aw, &ah );

	row = ch >> 4;
	col = ch & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	trap_R_DrawStretchPic( ax, ay, aw, ah,
						   fcol, frow,
						   fcol + size, frow + size,
						   cgs.media.charsetShader );
}

/*
===============
CG_DrawChar2

Coordinates and size in 640*480 virtual screen size
===============
*/
void CG_DrawChar2( float x, float y, int width, int height, int ch ) {
	int row, col;
	float frow, fcol;
	float size;
	float ax, ay, aw, ah;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	ax = x;
	ay = y;
	aw = width;
	ah = height;
	CG_AdjustFrom640( &ax, &ay, &aw, &ah );

	row = ch >> 4;
	col = ch & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	trap_R_DrawStretchPic( ax, ay, aw, ah,
						   fcol, frow,
						   fcol + size, frow + size,
						   cgs.media.menucharsetShader );
}

// JOSEPH 4-25-00
/*
==================
CG_DrawStringExt

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void CG_DrawStringExt( int x, int y, const char *string, const float *setColor,
					   qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars ) {
	vec4_t color;
	const char  *s;
	int xx;
	int cnt;

	if ( maxChars <= 0 ) {
		maxChars = 32767; // do them all!

	}
	// draw the drop shadow
	if ( shadow ) {
		color[0] = color[1] = color[2] = 0;
		color[3] = setColor[3];
		trap_R_SetColor( color );
		s = string;
		xx = x;
		cnt = 0;
		while ( *s && cnt < maxChars ) {
			if ( Q_IsColorString( s ) ) {
				s += 2;
				continue;
			}
			CG_DrawChar( xx + 3 / cgs.screenXScale, y + 3 / cgs.screenYScale, charWidth, charHeight, *s );
			cnt++;
			xx += charWidth;
			s++;
		}
	}

	// draw the colored text
	s = string;
	xx = x;
	cnt = 0;
	trap_R_SetColor( setColor );
	while ( *s && cnt < maxChars ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( color ) );
				color[3] = setColor[3];
				trap_R_SetColor( color );
			}
			s += 2;
			continue;
		}
		CG_DrawChar( xx, y, charWidth, charHeight, *s );
		xx += charWidth;
		cnt++;
		s++;
	}
	trap_R_SetColor( NULL );
}

/*==================
CG_DrawStringExt2

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void CG_DrawStringExt2( int x, int y, const char *string, const float *setColor,
						qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars ) {
	vec4_t color;
	const char  *s;
	int xx;
	int cnt;

	if ( maxChars <= 0 ) {
		maxChars = 32767; // do them all!

	}
	// draw the drop shadow
	if ( shadow ) {
		color[0] = color[1] = color[2] = 0;
		color[3] = setColor[3];
		trap_R_SetColor( color );
		s = string;
		xx = x;
		cnt = 0;
		while ( *s && cnt < maxChars ) {
			if ( Q_IsColorString( s ) ) {
				s += 2;
				continue;
			}
			CG_DrawChar2( xx + 2 / cgs.screenXScale , y + 2 / cgs.screenYScale, charWidth, charHeight, *s );
			cnt++;
			xx += charWidth;
			s++;
		}
	}

	// draw the colored text
	s = string;
	xx = x;
	cnt = 0;
	trap_R_SetColor( setColor );
	while ( *s && cnt < maxChars ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( color ) );
				color[3] = setColor[3];
				trap_R_SetColor( color );
			}
			s += 2;
			continue;
		}
		CG_DrawChar2( xx, y, charWidth, charHeight, *s );
		xx += charWidth;
		cnt++;
		s++;
	}
	trap_R_SetColor( NULL );
}

/*==================
CG_DrawStringExt3

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void CG_DrawStringExt3( int x, int y, const char *string, const float *setColor,
						qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars ) {
	vec4_t color;
	const char  *s;
	int xx;
	int cnt;

	if ( maxChars <= 0 ) {
		maxChars = 32767; // do them all!

	}
	s = string;
	xx = 0;

	while ( *s ) {
		xx += charWidth;
		s++;
	}

	x -= xx;

	s = string;
	xx = x;

	// draw the drop shadow
	if ( shadow ) {
		color[0] = color[1] = color[2] = 0;
		color[3] = setColor[3];
		trap_R_SetColor( color );
		s = string;
		xx = x;
		cnt = 0;
		while ( *s && cnt < maxChars ) {
			if ( Q_IsColorString( s ) ) {
				s += 2;
				continue;
			}
			CG_DrawChar2( xx + 2, y + 2, charWidth, charHeight, *s );
			cnt++;
			xx += charWidth;
			s++;
		}
	}

	// draw the colored text
	s = string;
	xx = x;
	cnt = 0;
	trap_R_SetColor( setColor );
	while ( *s && cnt < maxChars ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( color ) );
				color[3] = setColor[3];
				trap_R_SetColor( color );
			}
			s += 2;
			continue;
		}
		CG_DrawChar2( xx, y, charWidth, charHeight, *s );
		xx += charWidth;
		cnt++;
		s++;
	}
	trap_R_SetColor( NULL );
}

/*
==================
CG_DrawStringExt2

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
/*void CG_DrawStringExt2( int x, int y, const char *string, const float *setColor,
		qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars ) {
	vec4_t		color;
	const char	*s;
	int			xx;
	int			cnt;

	if (maxChars <= 0)
		maxChars = 32767; // do them all!

	// draw the drop shadow
	if (shadow) {
		color[0] = color[1] = color[2] = 0;
		color[3] = setColor[3];
		trap_R_SetColor( color );
		s = string;
		xx = x;
		cnt = 0;
		while ( *s && cnt < maxChars) {
			if ( Q_IsColorString( s ) ) {
				s += 2;
				continue;
			}
			CG_DrawChar2( xx + 2, y + 2, charWidth, charHeight, *s );
			cnt++;
			xx += charWidth;
			s++;
		}
	}

	// draw the colored text
	s = string;
	xx = x;
	cnt = 0;
	trap_R_SetColor( setColor );
	while ( *s && cnt < maxChars) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
				color[3] = setColor[3];
				trap_R_SetColor( color );
			}
			s += 2;
			continue;
		}
		CG_DrawChar2( xx, y, charWidth, charHeight, *s );
		xx += charWidth;
		cnt++;
		s++;
	}
	trap_R_SetColor( NULL );
}*/

void CG_DrawBigString( int x, int y, const char *s, float alpha ) {
	float color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	//CG_DrawStringExt( x, y, s, color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
	CG_DrawStringExt2( x, y, s, color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
}

void CG_DrawBigStringColor( int x, int y, const char *s, vec4_t color ) {
	//CG_DrawStringExt( x, y, s, color, qtrue, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
	CG_DrawStringExt2( x, y, s, color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
}
// END JOSEPH

// JOSEPH 4-25-00
void CG_DrawBigString2( int x, int y, const char *s, float alpha ) {
	float color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	CG_DrawStringExt3( x, y, s, color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
}

void CG_DrawBigStringColor2( int x, int y, const char *s, vec4_t color ) {
	CG_DrawStringExt3( x, y, s, color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
}
// END JOSEPH

void CG_DrawSmallString( int x, int y, const char *s, float alpha ) {
	float color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	CG_DrawStringExt( x, y, s, color, qfalse, qfalse, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
}

void CG_DrawSmallStringColor( int x, int y, const char *s, vec4_t color ) {
	CG_DrawStringExt( x, y, s, color, qtrue, qfalse, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
}

/*
=================
CG_DrawStrlen

Returns character count, skiping color escape codes
=================
*/
int CG_DrawStrlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/*
=============
CG_TileClearBox

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
static void CG_TileClearBox( int x, int y, int w, int h, qhandle_t hShader ) {
	float s1, t1, s2, t2;
	s1 = x / 64.0;
	t1 = y / 64.0;
	s2 = ( x + w ) / 64.0;
	t2 = ( y + h ) / 64.0;
	trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, hShader );
}



/*
==============
CG_TileClear

Clear around a sized down screen
==============
*/
void CG_TileClear( void ) {
	int top, bottom, left, right;
	int w, h;

	w = cgs.glconfig.vidWidth;
	h = cgs.glconfig.vidHeight;

	if ( cg.refdef.x == 0 && cg.refdef.y == 0 &&
		 cg.refdef.width == w && cg.refdef.height == h ) {
		return;     // full screen rendering
	}

	top = cg.refdef.y;
	bottom = top + cg.refdef.height - 1;
	left = cg.refdef.x;
	right = left + cg.refdef.width - 1;

	// clear above view screen
	CG_TileClearBox( 0, 0, w, top, cgs.media.backTileShader );

	// clear below view screen
	CG_TileClearBox( 0, bottom, w, h - bottom, cgs.media.backTileShader );

	// clear left of view screen
	CG_TileClearBox( 0, top, left, bottom - top + 1, cgs.media.backTileShader );

	// clear right of view screen
	CG_TileClearBox( right, top, w - right, bottom - top + 1, cgs.media.backTileShader );
}



/*
================
CG_FadeColor
================
*/
float *CG_FadeColor( int startMsec, int totalMsec ) {
	static vec4_t color;
	int t;

	if ( startMsec == 0 ) {
		return NULL;
	}

	t = cg.time - startMsec;

	if ( t >= totalMsec ) {
		return NULL;
	}

	// fade out
	if ( totalMsec - t < FADE_TIME ) {
		color[3] = ( totalMsec - t ) * 1.0 / FADE_TIME;
	} else {
		color[3] = 1.0;
	}
	color[0] = color[1] = color[2] = 1;

	color[3] *= cg_hudAlpha.value;          // NERVE - SMF - make this work like everything else

	return color;
}


/*
================
CG_TeamColor
================
*/
float *CG_TeamColor( int team ) {
	static vec4_t red = {1, 0.2, 0.2, 1};
	static vec4_t blue = {0.2, 0.2, 1, 1};
	static vec4_t other = {1, 1, 1, 1};
	static vec4_t spectator = {0.7, 0.7, 0.7, 1};

	switch ( team ) {
	case TEAM_RED:
		return red;
	case TEAM_BLUE:
		return blue;
	case TEAM_SPECTATOR:
		return spectator;
	default:
		return other;
	}
}


/*
=================
CG_GetColorForHealth
=================
*/
void CG_GetColorForHealth( int health, int armor, vec4_t hcolor ) {
	int count;
	int max;

	// calculate the total points of damage that can
	// be sustained at the current health / armor level
	if ( health <= 0 ) {
		VectorClear( hcolor );  // black
		hcolor[3] = 1;
		return;
	}
	count = armor;
	max = health * ARMOR_PROTECTION / ( 1.0 - ARMOR_PROTECTION );
	if ( max < count ) {
		count = max;
	}
	health += count;

	// set the color based on health
	hcolor[0] = 1.0;
	hcolor[3] = 1.0;
	if ( health >= 100 ) {
		hcolor[2] = 1.0;
	} else if ( health < 66 ) {
		hcolor[2] = 0;
	} else {
		hcolor[2] = ( health - 66 ) / 33.0;
	}

	if ( health > 60 ) {
		hcolor[1] = 1.0;
	} else if ( health < 30 ) {
		hcolor[1] = 0;
	} else {
		hcolor[1] = ( health - 30 ) / 30.0;
	}
}

/*
=================
CG_ColorForHealth
=================
*/
void CG_ColorForHealth( vec4_t hcolor ) {
	int health;
	int count;
	int max;

	// calculate the total points of damage that can
	// be sustained at the current health / armor level
	health = cg.snap->ps.stats[STAT_HEALTH];
	if ( health <= 0 ) {
		VectorClear( hcolor );  // black
		hcolor[3] = 1;
		return;
	}
	count = cg.snap->ps.stats[STAT_ARMOR];
	max = health * ARMOR_PROTECTION / ( 1.0 - ARMOR_PROTECTION );
	if ( max < count ) {
		count = max;
	}
	health += count;


	// set the color based on health
	hcolor[0] = 1.0;
	hcolor[3] = 1.0;
	if ( health >= 100 ) {
		hcolor[2] = 1.0;
	} else if ( health < 66 ) {
		hcolor[2] = 0;
	} else {
		hcolor[2] = ( health - 66 ) / 33.0;
	}

	if ( health > 60 ) {
		hcolor[1] = 1.0;
	} else if ( health < 30 ) {
		hcolor[1] = 0;
	} else {
		hcolor[1] = ( health - 30 ) / 30.0;
	}
}




/*
=================
UI_DrawProportionalString2
=================
*/
static int propMap[128][3] = {
	{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
	{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},

	{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
	{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},

	{0, 0, PROP_SPACE_WIDTH},   // SPACE
	{11, 122, 7}, // !
	{154, 181, 14}, // "
	{55, 122, 17}, // #
	{79, 122, 18}, // $
	{101, 122, 23}, // %
	{153, 122, 18}, // &
	{9, 93, 7}, // '
	{207, 122, 8}, // (
	{230, 122, 9}, // )
	{177, 122, 18}, // *
	{30, 152, 18}, // +
	{85, 181, 7}, // ,
	{34, 93, 11}, // -
	{110, 181, 6}, // .
	{130, 152, 14}, // /

	{22, 64, 17}, // 0
	{41, 64, 12}, // 1
	{58, 64, 17}, // 2
	{78, 64, 18}, // 3
	{98, 64, 19}, // 4
	{120, 64, 18}, // 5
	{141, 64, 18}, // 6
	{204, 64, 16}, // 7
	{162, 64, 17}, // 8
	{182, 64, 18}, // 9
	{59, 181, 7}, // :
	{35,181, 7}, // ;
	{203, 152, 14}, // <
	{56, 93, 14}, // =
	{228, 152, 14}, // >
	{177, 181, 18}, // ?

	{28, 122, 22}, // @
	{5, 4, 18}, // A
	{27, 4, 18}, // B
	{48, 4, 18}, // C
	{69, 4, 17}, // D
	{90, 4, 13}, // E
	{106, 4, 13}, // F
	{121, 4, 18}, // G
	{143, 4, 17}, // H
	{164, 4, 8}, // I
	{175, 4, 16}, // J
	{195, 4, 18}, // K
	{216, 4, 12}, // L
	{230, 4, 23}, // M
	{6, 34, 18}, // N
	{27, 34, 18}, // O

	{48, 34, 18}, // P
	{68, 34, 18}, // Q
	{90, 34, 17}, // R
	{110, 34, 18}, // S
	{130, 34, 14}, // T
	{146, 34, 18}, // U
	{166, 34, 19}, // V
	{185, 34, 29}, // W
	{215, 34, 18}, // X
	{234, 34, 18}, // Y
	{5, 64, 14}, // Z
	{60, 152, 7}, // [
	{106, 151, 13}, // '\'
	{83, 152, 7}, // ]
	{128, 122, 17}, // ^
	{4, 152, 21}, // _

	{134, 181, 5}, // '
	{5, 4, 18}, // A
	{27, 4, 18}, // B
	{48, 4, 18}, // C
	{69, 4, 17}, // D
	{90, 4, 13}, // E
	{106, 4, 13}, // F
	{121, 4, 18}, // G
	{143, 4, 17}, // H
	{164, 4, 8}, // I
	{175, 4, 16}, // J
	{195, 4, 18}, // K
	{216, 4, 12}, // L
	{230, 4, 23}, // M
	{6, 34, 18}, // N
	{27, 34, 18}, // O

	{48, 34, 18}, // P
	{68, 34, 18}, // Q
	{90, 34, 17}, // R
	{110, 34, 18}, // S
	{130, 34, 14}, // T
	{146, 34, 18}, // U
	{166, 34, 19}, // V
	{185, 34, 29}, // W
	{215, 34, 18}, // X
	{234, 34, 18}, // Y
	{5, 64, 14}, // Z
	{153, 152, 13}, // {
	{11, 181, 5}, // |
	{180, 152, 13}, // }
	{79, 93, 17}, // ~
	{0, 0, -1}  // DEL
};

static int propMapB[26][3] = {
	{11, 12, 33},
	{49, 12, 31},
	{85, 12, 31},
	{120, 12, 30},
	{156, 12, 21},
	{183, 12, 21},
	{207, 12, 32},

	{13, 55, 30},
	{49, 55, 13},
	{66, 55, 29},
	{101, 55, 31},
	{135, 55, 21},
	{158, 55, 40},
	{204, 55, 32},

	{12, 97, 31},
	{48, 97, 31},
	{82, 97, 30},
	{118, 97, 30},
	{153, 97, 30},
	{185, 97, 25},
	{213, 97, 30},

	{11, 139, 32},
	{42, 139, 51},
	{93, 139, 32},
	{126, 139, 31},
	{158, 139, 25},
};

#define PROPB_GAP_WIDTH     4
#define PROPB_SPACE_WIDTH   12
#define PROPB_HEIGHT        36

/*
=================
UI_DrawBannerString
=================
*/
static void UI_DrawBannerString2( int x, int y, const char* str, vec4_t color ) {
	const char* s;
	unsigned char ch;
	float ax;
	float ay;
	float aw;
	float ah;
	float frow;
	float fcol;
	float fwidth;
	float fheight;

	// draw the colored text
	trap_R_SetColor( color );

	ax = x * cgs.screenXScale + cgs.screenXBias;
	ay = y * cgs.screenXScale;

	s = str;
	while ( *s )
	{
		ch = *s & 127;
		if ( ch == ' ' ) {
			ax += ( (float)PROPB_SPACE_WIDTH + (float)PROPB_GAP_WIDTH ) * cgs.screenXScale;
		} else if ( ch >= 'A' && ch <= 'Z' )     {
			ch -= 'A';
			fcol = (float)propMapB[ch][0] / 256.0f;
			frow = (float)propMapB[ch][1] / 256.0f;
			fwidth = (float)propMapB[ch][2] / 256.0f;
			fheight = (float)PROPB_HEIGHT / 256.0f;
			aw = (float)propMapB[ch][2] * cgs.screenXScale;
			ah = (float)PROPB_HEIGHT * cgs.screenXScale;
			trap_R_DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol + fwidth, frow + fheight, cgs.media.charsetPropB );
			ax += ( aw + (float)PROPB_GAP_WIDTH * cgs.screenXScale );
		}
		s++;
	}

	trap_R_SetColor( NULL );
}

void UI_DrawBannerString( int x, int y, const char* str, int style, vec4_t color ) {
	const char *    s;
	int ch;
	int width;
	vec4_t drawcolor;

	// find the width of the drawn text
	s = str;
	width = 0;
	while ( *s ) {
		ch = *s;
		if ( ch == ' ' ) {
			width += PROPB_SPACE_WIDTH;
		} else if ( ch >= 'A' && ch <= 'Z' )     {
			width += propMapB[ch - 'A'][2] + PROPB_GAP_WIDTH;
		}
		s++;
	}
	width -= PROPB_GAP_WIDTH;

	switch ( style & UI_FORMATMASK ) {
	case UI_CENTER:
		x -= width / 2;
		break;

	case UI_RIGHT:
		x -= width;
		break;

	case UI_LEFT:
	default:
		break;
	}

	if ( style & UI_DROPSHADOW ) {
		drawcolor[0] = drawcolor[1] = drawcolor[2] = 0;
		drawcolor[3] = color[3];
		UI_DrawBannerString2( x + 2, y + 2, str, drawcolor );
	}

	UI_DrawBannerString2( x, y, str, color );
}


int UI_ProportionalStringWidth( const char* str ) {
	const char *    s;
	int ch;
	int charWidth;
	int width;

	s = str;
	width = 0;
	while ( *s ) {
		ch = *s & 127;
		charWidth = propMap[ch][2];
		if ( charWidth != -1 ) {
			width += charWidth;
			width += PROP_GAP_WIDTH;
		}
		s++;
	}

	width -= PROP_GAP_WIDTH;
	return width;
}

static void UI_DrawProportionalString2( int x, int y, const char* str, vec4_t color, float sizeScale, qhandle_t charset ) {
	const char* s;
	unsigned char ch;
	float ax;
	float ay;
	float aw;
	float ah;
	float frow;
	float fcol;
	float fwidth;
	float fheight;

	// draw the colored text
	trap_R_SetColor( color );

	ax = x * cgs.screenXScale + cgs.screenXBias;
	ay = y * cgs.screenXScale;

	s = str;
	while ( *s )
	{
		ch = *s & 127;
		if ( ch == ' ' ) {
			aw = (float)PROP_SPACE_WIDTH * cgs.screenXScale * sizeScale;
		} else if ( propMap[ch][2] != -1 ) {
			fcol = (float)propMap[ch][0] / 256.0f;
			frow = (float)propMap[ch][1] / 256.0f;
			fwidth = (float)propMap[ch][2] / 256.0f;
			fheight = (float)PROP_HEIGHT / 256.0f;
			aw = (float)propMap[ch][2] * cgs.screenXScale * sizeScale;
			ah = (float)PROP_HEIGHT * cgs.screenXScale * sizeScale;
			trap_R_DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol + fwidth, frow + fheight, charset );
		} else {
			aw = 0;
		}

		ax += ( aw + (float)PROP_GAP_WIDTH * cgs.screenXScale * sizeScale );
		s++;
	}

	trap_R_SetColor( NULL );
}

/*
=================
UI_ProportionalSizeScale
=================
*/
float UI_ProportionalSizeScale( int style ) {
	if (  style & UI_SMALLFONT ) {
		return 0.75;
	}
	if (  style & UI_EXSMALLFONT ) {
		return 0.4;
	}

	return 1.00;
}


/*
=================
UI_DrawProportionalString
=================
*/
void UI_DrawProportionalString( int x, int y, const char* str, int style, vec4_t color ) {
	vec4_t drawcolor;
	int width;
	float sizeScale;

	sizeScale = UI_ProportionalSizeScale( style );

	switch ( style & UI_FORMATMASK ) {
	case UI_CENTER:
		width = UI_ProportionalStringWidth( str ) * sizeScale;
		x -= width / 2;
		break;

	case UI_RIGHT:
		width = UI_ProportionalStringWidth( str ) * sizeScale;
		x -= width;
		break;

	case UI_LEFT:
	default:
		break;
	}

	if ( style & UI_DROPSHADOW ) {
		drawcolor[0] = drawcolor[1] = drawcolor[2] = 0;
		drawcolor[3] = color[3];
		UI_DrawProportionalString2( x + 2, y + 2, str, drawcolor, sizeScale, cgs.media.charsetProp );
	}

	if ( style & UI_INVERSE ) {
		drawcolor[0] = color[0] * 0.8;
		drawcolor[1] = color[1] * 0.8;
		drawcolor[2] = color[2] * 0.8;
		drawcolor[3] = color[3];
		UI_DrawProportionalString2( x, y, str, drawcolor, sizeScale, cgs.media.charsetProp );
		return;
	}

	// JOSEPH 12-29-99
	if ( style & UI_PULSE ) {
		//drawcolor[0] = color[0] * 0.8;
		//drawcolor[1] = color[1] * 0.8;
		//drawcolor[2] = color[2] * 0.8;
		drawcolor[3] = color[3];
		UI_DrawProportionalString2( x, y, str, color, sizeScale, cgs.media.charsetProp );

		drawcolor[0] = color[0];
		drawcolor[1] = color[1];
		drawcolor[2] = color[2];
		drawcolor[3] = 0.5 + 0.5 * sin( cg.time / PULSE_DIVISOR );
		UI_DrawProportionalString2( x, y, str, drawcolor, sizeScale, cgs.media.charsetPropGlow );
		return;
	}
	// END JOSEPH

	UI_DrawProportionalString2( x, y, str, color, sizeScale, cgs.media.charsetProp );
}

char* CG_TranslateString( const char *string ) {
	// dont even make the call if we're in english
	return trap_TranslateString( string );
}
