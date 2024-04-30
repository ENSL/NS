#ifndef CHUD_H
#define CHUD_H

#include "chudmisc.h"
#include "hud_spectator.h"
#include "AvHFont.h"
#include "hud_crosshairs.h"


class CHud
{
private:
	HUDLIST						*m_pHudList;
	AVHHSPRITE						m_hsprLogo;
	int							m_iLogo;
	client_sprite_t				*m_pSpriteList;
	int							m_iSpriteCount;
	int							m_iSpriteCountAllRes;
	float						m_flMouseSensitivity;
	bool						wstoggle;

	float m_flScaleX;
	float m_flScaleY;
	unsigned int m_iConWidth;
	unsigned int m_iConHeight;
	float m_flOffsetX;
	float m_flOffsetY;
	bool m_bIsWidescreen;

public:

	AVHHSPRITE					m_hsprCursor;
	float m_flTime;	   // the current client time
	float m_fOldTime;  // the time at which the HUD was last redrawn
	//float m_wsFOV; //fov recalc corrected for widescreen
	double m_flTimeDelta; // the difference between flTime and fOldTime
	Vector	m_vecOrigin;
	Vector	m_vecAngles;
	int		m_iKeyBits;
	int		m_iHideHUDDisplay;
	int		m_iFOV;
	int		m_iRes;
	cvar_t  *m_pCvarStealMouse;
	cvar_t	*m_pCvarDraw;
	bool	m_bWindowed;
	cvar_t* m_pCvarWidescreen;

	int m_iFontHeight;
	//int DrawHudNumber(int x, int y, int iFlags, int iNumber, int r, int g, int b );
	int DrawHudStringCentered(int x, int y, int iMaxX, const char *szString, int r, int g, int b );
	int DrawHudString(int x, int y, int iMaxX, const char *szString, int r, int g, int b );
	int GetHudStringHeight();
	int GetHudStringWidth(const char* szIt);
	int DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b );
	//int DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b );
	int GetNumWidth(int iNumber, int iFlags);

	// New stuff
	typedef enum {
		COLOR_DEFAULT = 0,
		COLOR_PRIMARY,
		COLOR_SECONDARY,
		COLOR_WARNING,
		COLOR_COUNT
	} hudcolor_e;

	typedef enum {
		a_northwest = 0,
		a_north,
		a_northeast,
		a_west,
		a_center,
		a_east,
		a_southwest,
		a_south,
		a_southeast,
	} hudalign_e;

	void DrawHudSprite(AVHHSPRITE pic, int frame, wrect_t* rect, int x, int y, int r, int g, int b, int a = 255, float scale = 1.0f, hudalign_e alignment = a_northwest);
	//void DrawHudSprite(AVHHSPRITE pic, int frame, wrect_t* rect, int x, int y, hudcolor_e color, int a = 255, hudalign_e alignment = a_northwest);
	//void DrawHudSpriteIndex(int index, int x, int y, hudcolor_e color, int a = 255, hudalign_e alignment = a_northwest);
	void DrawHudSpriteIndex(int index, int x, int y, int r, int g, int b, int a = 255, float scale = 1.0f, hudalign_e alignment = a_northwest);
	void DrawHudFill(int x, int y, int w, int h, int r, int g, int b, int a, float scale = 1.0f);
	//void DrawHudFill(int x, int y, int w, int h, hudcolor_e color, int a);
	int DrawHudNumber(int x, int y, int iFlags, int iNumber, int r, int g, int b, int a = 255, float scale = 1.0f, hudalign_e alignment = a_northwest);
	//int DrawHudNumber(int x, int y, int iFlags, int iNumber, hudcolor_e color, int a = 255, hudalign_e alignment = a_northwest);
	
	//int DrawHudStringReverse(int xpos, int ypos, int iMinX, const char* szString, int r, int g, int b);
	int DrawHudNumberString(int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b);

	int GetHudNumberWidth(int number, int width, int flags, float scale = 1.0f);
	int DrawHudNumberReverse(int x, int y, int number, int flags, int r, int g, int b, int a = 255, float scale = 1.0f, hudalign_e alignment = a_northwest);
	//int DrawHudNumberReverse(int x, int y, int number, int flags, hudcolor_e color, int a = 255, hudalign_e alignment = a_northwest);

	//int DrawHudString(const char* string, int x, int y);
	//void GetHudStringSize(const char* string, int& width, int& height);
	//int HudStringLen(const char* string);

	//void GetChatInputPosition(int& x, int& y);

	inline unsigned int GetWidth() const
	{
		return m_iConWidth;
	}

	inline unsigned int GetHeight() const
	{
		return m_iConHeight;
	}

	inline float GetScaleX() const
	{
		return  m_flScaleX;
	}

	inline float GetScaleY() const
	{
		return  m_flScaleY;
	}

	//inline void GetColor(int& r, int& g, int& b, hudcolor_e color) const
	//{
	//	r = m_cColors[color].r;
	//	g = m_cColors[color].g;
	//	b = m_cColors[color].b;
	//}

private:
	// the memory for these arrays are allocated in the first call to CHud::VidInit(), when the hud.txt and associated sprites are loaded.
	// freed in ~CHud()
	AVHHSPRITE *m_rghSprites;	/*[HUD_SPRITE_COUNT]*/			// the sprites loaded from hud.txt
	wrect_t *m_rgrcRects;	/*[HUD_SPRITE_COUNT]*/
	char *m_rgszSpriteNames; /*[HUD_SPRITE_COUNT][MAX_SPRITE_NAME_LENGTH]*/

	//struct cvar_s *default_fov;


public:
	AVHHSPRITE GetSprite( int index )
	{
		return (index < 0) ? 0 : m_rghSprites[index];
	}

	wrect_t& GetSpriteRect( int index )
	{
		return m_rgrcRects[index];
	}

	
	int GetSpriteIndex( const char *SpriteName );	// gets a sprite index, for use in the m_rghSprites[] array

	CHudAmmo		m_Ammo;
	CHudHealth		m_Health;
	CHudSpectator	m_Spectator;
	CHudGeiger		m_Geiger;
	CHudBattery		m_Battery;
	CHudTrain		m_Train;
	CHudFlashlight	m_Flash;
	CHudMessage		m_Message;
	CHudStatusBar   m_StatusBar;
	CHudDeathNotice m_DeathNotice;
	CHudSayText		m_SayText;
	CHudMenu		m_Menu;
	CHudAmmoSecondary	m_AmmoSecondary;
	CHudTextMessage m_TextMessage;
	CHudStatusIcons m_StatusIcons;
	CHudCrosshairs	m_Crosshairs;

    AvHFont mFont;
    AvHFont mSmallFont;

	void Init( void );
	void VidInit( void );
	void Think(void);
	int Redraw( float flTime, int intermission );
	int UpdateClientData( client_data_t *cdata, float time );

	CHud() : m_iSpriteCount(0), m_pHudList(NULL) {}  
	~CHud();			// destructor, frees allocated memory

	// user messages
	int _cdecl MsgFunc_ResetHUD(const char *pszName,  int iSize, void *pbuf);
	void _cdecl MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf );
	void _cdecl MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf);

	// Screen information
	SCREENINFO	m_scrinfo;

	int	m_iWeaponBits;
	int	m_fPlayerDead;
	int m_iIntermission;

	// sprite indexes
	int m_HUD_number_0;

	void AddHudElem(CHudBase *p);

	float GetSensitivity();

};



#endif
