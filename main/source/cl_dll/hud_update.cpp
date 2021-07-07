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
//  hud_update.cpp
//

#include <math.h>
#include "hud.h"
#include "cl_util.h"
#include <stdlib.h>
#include <memory.h>

int CL_ButtonBits( int );
void CL_ResetButtonBits( int bits );

extern float v_idlescale;
float in_fov;
extern void HUD_SetCmdBits( int bits );

int CHud::UpdateClientData(client_data_t *cdata, float time)
{
	memcpy(m_vecOrigin, cdata->origin, sizeof(vec3_t));
	memcpy(m_vecAngles, cdata->viewangles, sizeof(vec3_t));
	
	m_iKeyBits = CL_ButtonBits( 0 );
	m_iWeaponBits = cdata->iWeaponBits;

	in_fov = cdata->fov;

	Think();

	//cdata->fov = m_iFOV;

	float width = ScreenWidth();
	float height = ScreenHeight();

	// Horizontal+ widescreen view correction - engine uses vertical- cropping

	// Cvar to let players use old widescreen. Only allow it to change when not alive so it can't be used as a zoom toggle.
	if (!gHUD.GetIsAlive(false))
	{
		wstoggle = CVAR_GET_FLOAT("cl_widescreen") != 0;
	}

	if (wstoggle)
	{
		m_wsFOV = atanf(tan(m_iFOV * M_PI / 360) * 0.75 * width / height) * 360 / M_PI;

		//clamp for game balance
		if (m_iFOV == 105 && m_wsFOV > 121)
		{
			m_wsFOV = 120;
		}
		else if (m_iFOV == 100 && m_wsFOV > 117)
		{
			m_wsFOV = 116;
		}
		else if (m_iFOV == 90 && m_wsFOV > 107)
		{
			m_wsFOV = 106;
		}
		else if (m_wsFOV < 90)
		{
			m_wsFOV = 90;
		}
	}
	else
	{
		m_wsFOV = m_iFOV;
	}

	cdata->fov = m_wsFOV;

	CL_ResetButtonBits( m_iKeyBits );

	// return 1 if in anything in the client_data struct has been changed, 0 otherwise
	return 1;
}


