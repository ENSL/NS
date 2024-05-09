/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// hud_redraw.cpp
//
#include <math.h>
#include "hud.h"
#include "cl_util.h"
#include "mod/AvHFont.h"

#include "vgui_TeamFortressViewport.h"
#include "common/com_model.h"
//#include "r_studioint.h"

#define MAX_LOGO_FRAMES 56

//extern engine_studio_api_t IEngineStudio;

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};


extern int g_iVisibleMouse;

float HUD_GetFOV( void );

extern cvar_t *sensitivity;
//extern cvar_t *cl_forcedefaultfov;

// Think
void CHud::Think(void)
{
	float newfov;
	HUDLIST *pList = m_pHudList;

	while (pList)
	{
		if (pList->p->m_iFlags & HUD_ACTIVE)
			pList->p->Think();
		pList = pList->pNext;
	}

	newfov = HUD_GetFOV();
	if ( newfov == 0 )
	{
//		m_iFOV = default_fov->value;
		m_iFOV = 90;
	}
	else
	{
		m_iFOV = newfov;
	}

	//if(cl_forcedefaultfov->value)
	//{
	//	m_iFOV = 90;
	//}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	//if ( m_iFOV == default_fov->value || CVAR_GET_FLOAT("senslock") == 1.0f)
	if (m_iFOV == 90 || CVAR_GET_FLOAT("senslock") == 1.0f)
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		//m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)default_fov->value) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
		m_flMouseSensitivity = sensitivity->value * (newfov / 90.0f ) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	// think about default fov
	if ( m_iFOV == 0 )
	{  // only let players adjust up in fov,  and only if they are not overriden by something else
		//m_iFOV = max( default_fov->value, 90 );
		m_iFOV = 90;
	}
}

// Redraw
// step through the local data,  placing the appropriate graphics & text as appropriate
// returns 1 if they've changed, 0 otherwise
int CHud :: Redraw( float flTime, int intermission )
{
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;
	static int  m_flShotTime = 0;
	
	// Clock was reset, reset delta
	if ( m_flTimeDelta < 0 )
		m_flTimeDelta = 0;

	bool bWantWidescreen = m_pCvarWidescreen->value != 0.0F;

	if (bWantWidescreen != m_bIsWidescreen)
	{
		if (bWantWidescreen)
		{
			m_iConWidth = m_iConHeight * (ScreenWidth() / (float)ScreenHeight());
		}
		else
		{
			m_iConWidth = m_iRes;
		}
		m_flOffsetX = (ScreenWidth() - m_iConWidth * m_flScaleX) / 2.0F;
		m_bIsWidescreen = bWantWidescreen;
	}

	// Bring up the scoreboard during intermission
	if (gViewPort)
	{
		if ( m_iIntermission && !intermission )
		{
			// Have to do this here so the scoreboard goes away
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideScoreBoard();
			gViewPort->UpdateSpectatorPanel();
		}
		else if ( !m_iIntermission && intermission )
		{
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideVGUIMenu();
			gViewPort->ShowScoreBoard();
			gViewPort->UpdateSpectatorPanel();

			// Take a screenshot if the client's got the cvar set
			if ( CVAR_GET_FLOAT( "hud_takesshots" ) != 0 )
				m_flShotTime = flTime + 1.0;	// Take a screenshot in a second
		}
	}

	if (m_flShotTime && m_flShotTime < flTime)
	{
		gEngfuncs.pfnClientCmd("snapshot\n");
		m_flShotTime = 0;
	}

	m_iIntermission = intermission;

	// if no redrawing is necessary
	// return 0;

	
	if ( m_pCvarDraw->value)
	{
	HUDLIST *pList = m_pHudList;

	while (pList)
	{
		if ( !intermission )
		{
			if ( (pList->p->m_iFlags & HUD_ACTIVE) && (!(m_iHideHUDDisplay & HIDEHUD_CHAT) && !(m_iHideHUDDisplay & HIDEHUD_ALL)) )
				pList->p->Draw(flTime);
		}
		else
		{  // it's an intermission,  so only draw hud elements that are set to draw during intermissions
			if ( pList->p->m_iFlags & HUD_INTERMISSION )
				pList->p->Draw( flTime );
		}

		pList = pList->pNext;
		}
	}

	// are we in demo mode? do we need to draw the logo in the top corner?
	if (m_iLogo)
	{
		int x, y, i;

		if (m_hsprLogo == 0)
			m_hsprLogo = LoadSprite("sprites/%d_logo.spr");

		SPR_Set(m_hsprLogo, 250, 250, 250 );
		
		x = SPR_Width(m_hsprLogo, 0);
		x = ScreenWidth() - x;
		y = SPR_Height(m_hsprLogo, 0)/2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive(i, x, y, NULL);
	}
	
	/*
	if ( g_iVisibleMouse )
	{
	void IN_GetMousePos( int *mx, int *my );
	int mx, my;
	
	  IN_GetMousePos( &mx, &my );
	  
		if (m_hsprCursor == 0)
		{
		char sz[256];
		sprintf( sz, "sprites/cursor.spr" );
		m_hsprCursor = SPR_Load( sz );
		}
		
		  SPR_Set(m_hsprCursor, 250, 250, 250 );
		  
			// Draw the logo at 20 fps
			SPR_DrawAdditive( 0, mx, my, NULL );
			}
	*/
	
	return 1;
}

void ScaleColors( int &r, int &g, int &b, int a )
{
	float x = (float)a / 255;
	r = (int)(r * x);
	g = (int)(g * x);
	b = (int)(b * x);
}

int CHud::GetHudStringHeight()
{

    /*
	int theHeight = 0;
	theHeight = gHUD.m_scrinfo.iCharHeight;
	return theHeight;
    */

    return mFont.GetStringHeight();

}

int CHud::GetHudStringWidth(const char* szIt)
{
    /*
	int theWidth = 0;

	// draw the string until we hit the null character or a newline character
	for ( ; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		theWidth += gHUD.m_scrinfo.charWidths[ *szIt ];
	}
	
	return theWidth;
    */

    return mFont.GetStringWidth(szIt);

}

int CHud::DrawHudStringCentered(int x, int y, int iMaxX, const char *szString, int r, int g, int b )
{
	int theStringWidth = this->GetHudStringWidth(szString);
	return this->DrawHudString(x - theStringWidth/2, y, iMaxX, szString, r, g, b);
}

int CHud :: DrawHudString(int xpos, int ypos, int iMaxX, const char *szIt, int r, int g, int b )
{
    /*
	// Try to prevent software-mode crash
	int theStringHeight = this->GetHudStringHeight();

	if((ypos + theStringHeight) < ScreenHeight())
	{
		// draw the string until we hit the null character or a newline character
		for ( ; *szIt != 0 && *szIt != '\n'; szIt++ )
		{
			int next = xpos + gHUD.m_scrinfo.charWidths[ *szIt ]; // variable-width fonts look cool
			if ( next > iMaxX )
				return xpos;
		
			TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
			xpos = next;		
		}
	}

	return xpos;
    */

    return mFont.DrawString(xpos, ypos, szIt, r, g, b);

}

int CHud :: DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b )
{
	char szString[32];
	sprintf( szString, "%d", iNumber );
	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );

}

// draws a string from right to left (right-aligned)
int CHud :: DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b )
{
 
    /*
	// find the end of the string
	for ( char *szIt = szString; *szIt != 0; szIt++ )
	{ // we should count the length?		
	}

	// iterate throug the string in reverse
	for ( szIt--;  szIt != (szString-1);  szIt-- )	
	{
		int next = xpos - gHUD.m_scrinfo.charWidths[ *szIt ]; // variable-width fonts look cool
		if ( next < iMinX )
			return xpos;
		xpos = next;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
	}

	return xpos;
    */

    return mFont.DrawStringReverse(xpos, ypos, szString, r, g, b);
    
}

//// Old metod before scaling.
//int CHud :: DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b)
//{
//	int iWidth = GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left;
//	int k;
//	
//	if (iNumber > 0)
//	{
//		// SPR_Draw 100's
//		if (iNumber >= 100)
//		{
//			 k = iNumber/100;
//			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
//			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
//			x += iWidth;
//		}
//		else if (iFlags & (DHN_3DIGITS))
//		{
//			//SPR_DrawAdditive( 0, x, y, &rc );
//			x += iWidth;
//		}
//
//		// SPR_Draw 10's
//		if (iNumber >= 10)
//		{
//			k = (iNumber % 100)/10;
//			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
//			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
//			x += iWidth;
//		}
//		else if (iFlags & (DHN_3DIGITS | DHN_2DIGITS))
//		{
//			//SPR_DrawAdditive( 0, x, y, &rc );
//			x += iWidth;
//		}
//
//		// SPR_Draw ones
//		k = iNumber % 10;
//		SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
//		SPR_DrawAdditive(0,  x, y, &GetSpriteRect(m_HUD_number_0 + k));
//		x += iWidth;
//	} 
//	else if (iFlags & DHN_DRAWZERO) 
//	{
//		SPR_Set(GetSprite(m_HUD_number_0), r, g, b );
//
//		// SPR_Draw 100's
//		if (iFlags & (DHN_3DIGITS))
//		{
//			//SPR_DrawAdditive( 0, x, y, &rc );
//			x += iWidth;
//		}
//
//		if (iFlags & (DHN_3DIGITS | DHN_2DIGITS))
//		{
//			//SPR_DrawAdditive( 0, x, y, &rc );
//			x += iWidth;
//		}
//
//		// SPR_Draw ones
//		
//		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect(m_HUD_number_0));
//		x += iWidth;
//	}
//
//	return x;
//}

// Toodles' sprite scaling with additional scaling option added.
void CHud::DrawHudSprite(AVHHSPRITE pic, int frame, wrect_t * rect, int x, int y, int r, int g, int b, int a, float scale, hudalign_e alignment)
{
	auto sprw = gEngfuncs.pfnSPR_Width(pic, frame);
	auto sprh = gEngfuncs.pfnSPR_Height(pic, frame);

	if (!rect)
	{
		static wrect_t rc;
		rc.left = 0;
		rc.right = sprw;
		rc.top = 0;
		rc.bottom = sprh;
		rect = &rc;
	}

	float xf = x;
	float yf = y;
	auto width = (rect->right - rect->left) * scale;
	auto height = (rect->bottom - rect->top) * scale;

	switch (alignment)
	{
	case a_north:
	case a_center:
	case a_south:
		xf -= width / 2.0F - 0.5F;
		break;
	case a_northeast:
	case a_east:
	case a_southeast:
		xf -= width;
		break;
	}

	switch (alignment)
	{
	case a_west:
	case a_center:
	case a_east:
		yf -= height / 2.0F - 0.5F;
		break;
	case a_southwest:
	case a_south:
	case a_southeast:
		yf -= height;
		break;
	}

	//// No software mode in NS.
	//if (!IEngineStudio.IsHardware())
	//{
	//	x += m_flOffsetX;
	//	y += m_flOffsetY;

	//	ScaleColors(r, g, b, a);
	//	gEngfuncs.pfnSPR_Set(pic, r, g, b);

	//	// Toodles FIXME: Hack for the crosshair.
	//	if (alignment == a_center)
	//	{
	//		gEngfuncs.pfnSPR_DrawHoles(frame, x, y, rect);
	//	}
	//	else
	//	{
	//		gEngfuncs.pfnSPR_DrawAdditive(frame, x, y, rect);
	//	}
	//	return;
	//}

	auto pSprite = const_cast<model_t*>(gEngfuncs.GetSpritePointer(pic));

	auto x1 = roundf(m_flOffsetX + xf * m_flScaleX);
	auto y1 = roundf(m_flOffsetY + yf * m_flScaleY);
	auto x2 = roundf(m_flOffsetX + (xf + width) * m_flScaleX);
	auto y2 = roundf(m_flOffsetY + (yf + height) * m_flScaleY);

	auto left = rect->left / (float)sprw;
	auto right = rect->right / (float)sprw;
	auto top = rect->top / (float)sprh;
	auto bottom = rect->bottom / (float)sprh;

	gEngfuncs.pTriAPI->SpriteTexture(pSprite, frame);

	auto rendermode = kRenderTransAdd;

	// Toodles FIXME: Hack for the crosshair.
	if (alignment == a_center)
	{
		rendermode = kRenderTransAlpha;
	}

	gEngfuncs.pTriAPI->Color4fRendermode(r / 255.0F, g / 255.0F, b / 255.0F, a / 255.0F, rendermode);
	gEngfuncs.pTriAPI->RenderMode(rendermode);

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);

	gEngfuncs.pTriAPI->TexCoord2f(left, top);
	gEngfuncs.pTriAPI->Vertex3f(x1, y1, 0);

	gEngfuncs.pTriAPI->TexCoord2f(right, top);
	gEngfuncs.pTriAPI->Vertex3f(x2, y1, 0);

	gEngfuncs.pTriAPI->TexCoord2f(right, bottom);
	gEngfuncs.pTriAPI->Vertex3f(x2, y2, 0);

	gEngfuncs.pTriAPI->TexCoord2f(left, bottom);
	gEngfuncs.pTriAPI->Vertex3f(x1, y2, 0);

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

//void CHud::DrawHudSprite(AVHHSPRITE pic, int frame, wrect_t* rect, int x, int y, hudcolor_e color, int a, hudalign_e alignment)
//{
//	int r, g, b;
//	GetColor(r, g, b, color);
//	DrawHudSprite(pic, frame, rect, x, y, r, g, b, a, alignment);
//}

void CHud::DrawHudSpriteIndex(int index, int x, int y, int r, int g, int b, int a, float scale, hudalign_e alignment)
{
	DrawHudSprite(GetSprite(index), 0, &GetSpriteRect(index), x, y, r, g, b, a, scale, alignment);
}

//void CHud::DrawHudSpriteIndex(int index, int x, int y, hudcolor_e color, int a, hudalign_e alignment)
//{
//	int r, g, b;
//	GetColor(r, g, b, color);
//	DrawHudSprite(GetSprite(index), 0, &GetSpriteRect(index), x, y, r, g, b, a, alignment);
//}

void CHud::DrawHudFill(int x, int y, int w, int h, int r, int g, int b, int a, float scale)
{
	x = roundf(m_flOffsetX + x * m_flScaleX);
	y = roundf(m_flOffsetY + y * m_flScaleY);
	w = roundf(w * m_flScaleX * scale);
	h = roundf(h * m_flScaleY * scale);

	gEngfuncs.pfnFillRGBA(x, y, w, h, r, g, b, a);
}

//void CHud::DrawHudFill(int x, int y, int w, int h, hudcolor_e color, int a)
//{
//	int r, g, b;
//	GetColor(r, g, b, color);
//	DrawHudFill(x, y, w, h, r, g, b, a);
//}

//int CHud::DrawHudString(int xpos, int ypos, int iMaxX, const char* szIt, int r, int g, int b)
//{
//	auto x1 = roundf(m_flOffsetX + xpos * m_flScaleX);
//	auto y1 = roundf(m_flOffsetY + ypos * m_flScaleY);
//	return (gEngfuncs.pfnDrawString(x1, y1, szIt, r, g, b) - m_flOffsetX) / m_flScaleX;
//}

//// draws a string from right to left (right-aligned)
//int CHud::DrawHudStringReverse(int xpos, int ypos, int iMinX, const char* szString, int r, int g, int b)
//{
//	auto x1 = roundf(m_flOffsetX + xpos * m_flScaleX);
//	auto y1 = roundf(m_flOffsetY + ypos * m_flScaleY);
//	return (gEngfuncs.pfnDrawStringReverse(x1, y1, szString, r, g, b) - m_flOffsetX) / m_flScaleX;
//}

int CHud::DrawHudNumber(int x, int y, int iFlags, int iNumber, int r, int g, int b, int a, float scale, hudalign_e alignment)
{
	int iWidth = (GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left) * scale;
	int k;

	if (iNumber > 0)
	{
		// Draw 1000's
		if (iNumber >= 1000)
		{
			k = iNumber / 1000;
			DrawHudSpriteIndex(m_HUD_number_0 + k, x, y, r, g, b, a, scale, alignment);
			x += iWidth;
		}

		// SPR_Draw 100's
		if (iNumber >= 100)
		{
			k = (iNumber % 1000) / 100;
			DrawHudSpriteIndex(m_HUD_number_0 + k, x, y, r, g, b, a, scale, alignment);
			x += iWidth;
		}
		else if ((iFlags & DHN_3DIGITS) != 0)
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw 10's
		if (iNumber >= 10)
		{
			k = (iNumber % 100) / 10;
			DrawHudSpriteIndex(m_HUD_number_0 + k, x, y, r, g, b, a, scale, alignment);
			x += iWidth;
		}
		else if ((iFlags & (DHN_3DIGITS | DHN_2DIGITS)) != 0)
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		DrawHudSpriteIndex(m_HUD_number_0 + k, x, y, r, g, b, a, scale, alignment);
		x += iWidth;
	}
	else if ((iFlags & DHN_DRAWZERO) != 0)
	{
		// SPR_Draw 100's
		if ((iFlags & DHN_3DIGITS) != 0)
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		if ((iFlags & (DHN_3DIGITS | DHN_2DIGITS)) != 0)
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones

		DrawHudSpriteIndex(m_HUD_number_0, x, y, r, g, b, a, scale, alignment);
		x += iWidth;
	}

	return x;
}

//int CHud::DrawHudNumber(int x, int y, int iFlags, int iNumber, hudcolor_e color, int a, hudalign_e alignment)
//{
//	int r, g, b;
//	GetColor(r, g, b, color);
//	return DrawHudNumber(x, y, iFlags, iNumber, r, g, b, a, alignment);
//}

int CHud::GetNumWidth( int iNumber, int iFlags )
{
	if (iFlags & (DHN_3DIGITS))
		return 3;

	if (iFlags & (DHN_2DIGITS))
		return 2;

	if (iNumber <= 0)
	{
		if (iFlags & (DHN_DRAWZERO))
			return 1;
		else
			return 0;
	}

	if (iNumber < 10)
		return 1;

	if (iNumber < 100)
		return 2;

	return 3;

}	

int CHud::GetHudNumberWidth(int number, int width, int flags, float scale)
{
	const int digitWidth = (GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left) * scale;

	int totalDigits = 0;

	if (number > 0)
	{
		totalDigits = static_cast<int>(log10(number)) + 1;
	}
	else if ((flags & DHN_DRAWZERO) != 0)
	{
		totalDigits = 1;
	}

	totalDigits = max(totalDigits, width);

	return totalDigits * digitWidth;
}

int CHud::DrawHudNumberReverse(int x, int y, int number, int flags, int r, int g, int b, int a, float scale, hudalign_e alignment)
{
	if (number > 0 || (flags & DHN_DRAWZERO) != 0)
	{
		const int digitWidth = (GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left) * scale;

		int remainder = number;

		do
		{
			const int digit = remainder % 10;
			const int digitSpriteIndex = m_HUD_number_0 + digit;

			//This has to happen *before* drawing because we're drawing in reverse
			x -= digitWidth;

			DrawHudSpriteIndex(digitSpriteIndex, x, y, r, g, b, a, scale, alignment);

			remainder /= 10;
		} while (remainder > 0);
	}

	return x;
}

//int CHud::DrawHudNumberReverse(int x, int y, int number, int flags, hudcolor_e color, int a, hudalign_e alignment)
//{
//	int r, g, b;
//	GetColor(r, g, b, color);
//	return DrawHudNumberReverse(x, y, number, flags, r, g, b, a, alignment);
//}

//int CHud::DrawHudString(const char* string, int x, int y)
//{
//	auto x1 = roundf (m_flOffsetX + x * m_flScaleX);
//	auto y1 = roundf (m_flOffsetY + y * m_flScaleY);
//	return (gEngfuncs.pfnDrawConsoleString(x1, y1, (char*)string) - m_flOffsetX) / m_flScaleX;
//}
//
//void CHud::GetHudStringSize(const char* string, int& width, int& height)
//{
//	gEngfuncs.pfnDrawConsoleStringLen(string, &width, &height);
//	width /= m_flScaleX;
//	height /= m_flScaleY;
//}
//
//int CHud::HudStringLen(const char* string)
//{
//	int width, height;
//	GetHudStringSize(string, width, height);
//	return width;
//}
//
//void CHud::GetChatInputPosition(int& x, int& y)
//{
//	x = roundf (m_flOffsetX + m_SayText.m_iBaseX * m_flScaleX);
//	y = roundf (m_flOffsetY + (m_SayText.m_iBaseY + m_SayText.m_iLineHeight) * m_flScaleY);
//}
