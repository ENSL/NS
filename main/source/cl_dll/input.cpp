//======== (C) Copyright 2002 Charles G. Cleveland All rights reserved. =========
//
// The copyright to the contents herein is the property of Charles G. Cleveland.
// The contents may be used and/or copied only with the written permission of
// Charles G. Cleveland, or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile: input.cpp $
// $Date: 2002/10/16 02:12:21 $
//
//-------------------------------------------------------------------------------
// $Log: input.cpp,v $
// Revision 1.28  2002/10/16 02:12:21  Flayra
// - Valve anti-cheat integrated!
//
// Revision 1.27  2002/08/09 00:13:04  Flayra
// - Removed explicitly allowing specific commands.  I can't remember why this was needed, but it doesn't appear to be anymore.
//
// Revision 1.26  2002/08/02 21:39:03  Flayra
// - Refactored variable names
//
// Revision 1.25  2002/07/08 16:13:31  Flayra
// - Fixed bug where command was able to switch weapons via mousewheel (bug #239)
//
//===============================================================================
// cl.input.c  -- builds an intended movement command to send to the server

//xxxxxx Move bob and pitch drifting code here and other stuff from view if needed

// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.
#include "hud.h"
#include "cl_util.h"
#include "camera.h"
extern "C"
{
#include "kbutton.h"
}
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "view.h"
#include "bench.h"
#include <string.h>
#include <ctype.h>
#include "engine/keydefs.h"
#include "Exports.h"

#include "vgui_TeamFortressViewport.h"
#include "mod/AvHServerVariables.h"// should we go ahead and just make a SharedVariables.h instead?
#include "mod/AvHClientVariables.h"
#include "mod/AvHMessage.h"
#include "winsani_in.h"
#include "fmod.h"
#include "winsani_out.h"
#include "mod/AvHScriptManager.h"
#include "mod/AvHScrollHandler.h"
#include "mod/AvHCommanderModeHandler.h"
#include "util/Mat3.h"
#include "mod/AvHBasePlayerWeaponConstants.h"

#include "engine/APIProxy.h"
#include "Exports.h"

// : duck toggle
extern bool g_bDuckToggled;
// :

extern int g_iAlive;

extern int g_weaponselect;
extern cl_enginefunc_t gEngfuncs;
bool gResetViewAngles = false;
vec3_t gViewAngles;
extern AvHCommanderModeHandler	gCommanderHandler;

///////////////////////////////
// Begin Max's Code
///////////////////////////////

float gPlayerAngles[3]       = { 0, 0, 0 };
float gTargetPlayerAngles[3] = { 0, 0, 0 };
float gWorldViewAngles[3] = { 0, 0, 0 };

///////////////////////////////
// End Max's Code
///////////////////////////////

// Defined in pm_math.c
//extern "C" float anglemod( float a );
float anglemod( float a );

void IN_Init (void);
void IN_Move ( float frametime, usercmd_t *cmd);
void IN_Shutdown( void );
void V_Init( void );
void VectorAngles( const float *forward, float *angles );
int CL_ButtonBits( int );

// xxx need client dll function to get and clear impuse
extern cvar_t *in_joystick;

int	in_impulse	= 0;
int	in_cancel	= 0;

bool pistolHandlerDown = false;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*lookstrafe;
cvar_t	*lookspring;
cvar_t	*cl_pitchup;
cvar_t	*cl_pitchdown;
cvar_t	*cl_upspeed;
//cvar_t	*cl_forwardspeed;
//cvar_t	*cl_backspeed;
//cvar_t	*cl_sidespeed;
cvar_t	*cl_movespeedkey;
cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;
cvar_t	*cl_anglespeedkey;
cvar_t	*cl_vsmoothing;
cvar_t  *cl_autohelp;
cvar_t  *cl_centerentityid;
cvar_t  *cl_musicenabled;
cvar_t	*cl_musicdelay;
cvar_t  *cl_musicvolume;
cvar_t  *cl_musicdir;
cvar_t	*cl_quickselecttime;
cvar_t	*cl_highdetail;
cvar_t	*cl_cmhotkeys;
//cvar_t	*cl_forcedefaultfov;
cvar_t	*cl_dynamiclights;
cvar_t	*cl_buildmessages;
cvar_t	*cl_particleinfo;
//cvar_t	*cl_widescreen;
cvar_t	*cl_ambientsound;
cvar_t	*senslock;
cvar_t	*cl_chatbeep;
cvar_t	*cl_mutemenu;
cvar_t	*cl_weaponcfgs;
cvar_t	*cl_pistoltrigger;
cvar_t	*cl_cmcancellast;
cvar_t	*cl_teamcfgs;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/


kbutton_t	in_mlook;
kbutton_t	in_klook;
kbutton_t	in_jlook;
kbutton_t	in_left;
kbutton_t	in_right;
kbutton_t	in_forward;
kbutton_t	in_back;
kbutton_t	in_lookup;
kbutton_t	in_lookdown;
kbutton_t	in_moveleft;
kbutton_t	in_moveright;
kbutton_t	in_strafe;
kbutton_t	in_speed;
kbutton_t	in_use;
kbutton_t	in_jump;
kbutton_t	in_attack;
kbutton_t	in_attack2;
kbutton_t	in_up;
kbutton_t	in_down;
kbutton_t	in_duck;
kbutton_t	in_reload;
kbutton_t	in_alt1;
kbutton_t	in_score;
kbutton_t	in_break;
kbutton_t	in_graph;  // Display the netgraph
kbutton_t	in_scrollup;
kbutton_t	in_scrolldown;
kbutton_t	in_scrollleft;
kbutton_t	in_scrollright;

typedef struct kblist_s
{
	struct kblist_s *next;
	kbutton_t *pkey;
	char name[32];
} kblist_t;

kblist_t *g_kbkeys = NULL;

vector< pair <int, string> > g_PrevCmds; 

/*
============
KB_ConvertString

Removes references to +use and replaces them with the keyname in the output string.  If
 a binding is unfound, then the original text is retained.
NOTE:  Only works for text with +word in it.
============
*/
int KB_ConvertString( char *in, char **ppout )
{
	char sz[ 4096 ];
	char binding[ 64 ];
	char *p;
	char *pOut;
	char *pEnd;
	const char *pBinding;

	if ( !ppout )
		return 0;

	*ppout = NULL;
	p = in;
	pOut = sz;
	while ( *p )
	{
		if ( *p == '+' )
		{
			pEnd = binding;
			while ( *p && ( isalnum( *p ) || ( pEnd == binding ) ) && ( ( pEnd - binding ) < 63 ) )
			{
				*pEnd++ = *p++;
			}

			*pEnd =  '\0';

			pBinding = NULL;
			if ( strlen( binding + 1 ) > 0 )
			{
				// See if there is a binding for binding?
				pBinding = gEngfuncs.Key_LookupBinding( binding + 1 );
			}

			if ( pBinding )
			{
				*pOut++ = '[';
				pEnd = (char *)pBinding;
			}
			else
			{
				pEnd = binding;
			}

			while ( *pEnd )
			{
				*pOut++ = *pEnd++;
			}

			if ( pBinding )
			{
				*pOut++ = ']';
			}
		}
		else
		{
			*pOut++ = *p++;
		}
	}

	*pOut = '\0';

	pOut = ( char * )malloc( strlen( sz ) + 1 );
	strcpy( pOut, sz );
	*ppout = pOut;

	return 1;
}

/*
============
KB_Find

Allows the engine to get a kbutton_t directly ( so it can check +mlook state, etc ) for saving out to .cfg files
============
*/
struct kbutton_s CL_DLLEXPORT *KB_Find( const char *name )
{
//	RecClFindKey(name);

	kblist_t *p;
	p = g_kbkeys;
	while ( p )
	{
		if ( !stricmp( name, p->name ) )
			return p->pkey;

		p = p->next;
	}
	return NULL;
}

/*
============
KB_Add

Add a kbutton_t * to the list of pointers the engine can retrieve via KB_Find
============
*/
void KB_Add( const char *name, kbutton_t *pkb )
{
	kblist_t *p;	
	kbutton_t *kb;

	kb = KB_Find( name );
	
	if ( kb )
		return;

	p = ( kblist_t * )malloc( sizeof( kblist_t ) );
	memset( p, 0, sizeof( *p ) );

	strcpy( p->name, name );
	p->pkey = pkb;

	p->next = g_kbkeys;
	g_kbkeys = p;
}

/*
============
KB_Init

Add kbutton_t definitions that the engine can query if needed
============
*/
void KB_Init( void )
{
	g_kbkeys = NULL;

	KB_Add( "in_graph", &in_graph );
	KB_Add( "in_mlook", &in_mlook );
	KB_Add( "in_jlook", &in_jlook );
}

/*
============
KB_Shutdown

Clear kblist
============
*/
void KB_Shutdown( void )
{
	kblist_t *p, *n;
	p = g_kbkeys;
	while ( p )
	{
		n = p->next;
		free( p );
		p = n;
	}
	g_kbkeys = NULL;
}

/*
============
KeyDown
============
*/
void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;

	c = gEngfuncs.Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	int theBlockScripts = (int)gHUD.GetServerVariableFloat(kvBlockScripts);

	char *pCmd = gEngfuncs.Cmd_Argv(0);

	if(theBlockScripts && pCmd)
	{
		bool bFound = false;

		//Check thier last few commands (this prevents false positives if a player is hits several keys real fast)
		for (int i = 0; i < g_PrevCmds.size(); i++) 
		{
			//Check both the key pressed and the command it executed.
			if(k == g_PrevCmds[i].first && !strcmp(pCmd, g_PrevCmds[i].second.c_str()))
			{
				bFound = true;
				break;
			}
		}
		

		if(!bFound 
			&& strcmp(pCmd, "+mlook") 
			&& strcmp(pCmd, "+jlook")
			&& strcmp(pCmd, "+showscores")
			&& strcmp(pCmd, "+use"))
		{
			gEngfuncs.pfnCenterPrint("Scripting is not allowed on this server.");
			b->down[0] = b->down[1] = 0;
			b->state = 4;	// impulse up
			return;
		}
	}

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key
	
	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		gEngfuncs.Con_DPrintf ("Three keys down for a button '%c' '%c' '%c'!\n", b->down[0], b->down[1], c);
		return;
	}
	
	if (b->state & 1)
		return;		// still down
	b->state |= 1 + 2;	// down + impulse down
}

/*
============
KeyDownForced
============
*/
void KeyDownForced (kbutton_t *b)
{
	b->down[0] = 0;
	b->down[1] = 0;
	
	if (b->state & 1)
		return;		// still down
	b->state |= 1 + 2;	// down + impulse down
}

/*
============
KeyUp
============
*/
void KeyUp (kbutton_t *b)
{
	int		k;
	char	*c;
	
	c = gEngfuncs.Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
	{
		//Con_Printf ("Keys down for button: '%c' '%c' '%c' (%d,%d,%d)!\n", b->down[0], b->down[1], c, b->down[0], b->down[1], c);
		return;		// some other key is still holding it down
	}

	if (!(b->state & 1))
		return;		// still up (this should not happen)

	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

/*
============
KeyUpForced
============
*/
void KeyUpForced (kbutton_t *b)
{
	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

bool AvHContainsBlockedCommands(const char* inInput)
{
    if (inInput == NULL)
    {
        return false;
    }

    const char* kBlockedCommand[] =
        {
            "exec",
            "wait",
            "special",
            "_special"
        };

    // Check for a ; indicating multiple commands.

    if (strchr(inInput, ';') != NULL)
    {
        return true;
    }

    // Check if any of the blocked commands are being used.

    const char* theCommandEnd = strpbrk(inInput, " \t");
    
    int theCommandLength;
    
    if (theCommandEnd == NULL)
    {
        theCommandLength = (int)strlen(inInput);
    }
    else
    {
        theCommandLength = theCommandEnd - inInput;
    }

    for (int i = 0; i < sizeof(kBlockedCommand) / sizeof(const char*); ++i)
    {
        if ((int)strlen(kBlockedCommand[i]) == theCommandLength &&
            strncmp(inInput, kBlockedCommand[i], theCommandLength) == 0)
        {
            return true;
        }
	}
    return false;
}

/*
============
HUD_Key_Event

Return 1 to allow engine to process the key, otherwise, act on it as needed
============
*/
int CL_DLLEXPORT HUD_Key_Event( int down, int keynum, const char *pszCurrentBinding )
{
//    RecClKeyEvent(down, keynum, pszCurrentBinding);

	// Check to see if the event has any outlawed commands in it.
	float theBlockScripts = gHUD.GetServerVariableFloat(kvBlockScripts);

	if (theBlockScripts && AvHContainsBlockedCommands(pszCurrentBinding))
	{
		if(down)//: only show when going down.
			gEngfuncs.pfnCenterPrint("Scripting is not allowed on this server.\n");
		return 0;
	}

	if(pszCurrentBinding)
	{
		if(g_PrevCmds.size() >= 5)
			g_PrevCmds.erase(g_PrevCmds.begin());//remove the oldest command

		g_PrevCmds.push_back(make_pair(keynum, (string)pszCurrentBinding));
	}

	int theProcessKeyBinding = 1;

	//	char theKeyBinding[256] = "none";
	//	if(pszCurrentBinding)
	//	{
	//		sprintf(theKeyBinding, pszCurrentBinding);
	//	}
	//
	//	char theMessage[512];
	//	sprintf(theMessage, "%s (down: %d, keynum %d)", theKeyBinding, down, keynum);
	//	CenterPrint(theMessage);

	if(gViewPort /*&& gViewPort->IsOptionsMenuVisible()*/)
	{
		
        theProcessKeyBinding = gViewPort->KeyInput(down, keynum, pszCurrentBinding);

		if(pszCurrentBinding && (!strcmp(pszCurrentBinding, "toggleconsole") || !strcmp(pszCurrentBinding, "cancelselect")))
		{
			theProcessKeyBinding = 1;
		}
	}


	// Process topdown commands with precedence first
	if(theProcessKeyBinding && gHUD.GetInTopDownMode())
	{
		if((keynum != 0) && down/*&& pszCurrentBinding*/)
		{
			AvHMessageID theMessageID = gHUD.HotKeyHit(keynum);
			if((theMessageID != MESSAGE_NULL) || (keynum == K_ESCAPE))
			{
				// If ESC or cancel was hit, cancel ghost building
				if((keynum == K_ESCAPE) || (theMessageID == MESSAGE_CANCEL))
				{
					gHUD.SetGhostBuildingMode(MESSAGE_NULL);
				}

				theProcessKeyBinding = 0;
			}
		////Commander arrow key scroll hardcoding so keys can be used out of comm chair without config change. Add as cvar option?
		//	if (keynum == K_UPARROW){KeyDown(&in_scrollup); theProcessKeyBinding = 0;}
		//	if (keynum == K_DOWNARROW){KeyDown(&in_scrolldown); theProcessKeyBinding = 0;}
		//	if (keynum == K_LEFTARROW){KeyDown(&in_scrollleft);	theProcessKeyBinding = 0;}
		//	if (keynum == K_RIGHTARROW){KeyDown(&in_scrollright);theProcessKeyBinding = 0;}
		//}
		//else if((keynum != 0) && !(down))
		//{
		//	if (keynum == K_UPARROW){KeyUp(&in_scrollup);theProcessKeyBinding = 0;}
		//	if (keynum == K_DOWNARROW){KeyUp(&in_scrolldown);theProcessKeyBinding = 0;}
		//	if (keynum == K_LEFTARROW){KeyUp(&in_scrollleft);theProcessKeyBinding = 0;}
		//	if (keynum == K_RIGHTARROW){KeyUp(&in_scrollright); theProcessKeyBinding = 0;}
		}
	}

	if(theProcessKeyBinding)
	{
		// Process only a couple keybindings in top down mode
//		if(!gHUD.GetInTopDownMode() || (pszCurrentBinding && 
//
//			// Misc. commands
//			(!strcmp(pszCurrentBinding, "toggleconsole") || !strcmp(pszCurrentBinding, "cancelselect") || !strcmp(pszCurrentBinding, "+showscores") || !strcmp(pszCurrentBinding, "-showscores") || !strcmp(pszCurrentBinding, "messagemode") || !strcmp(pszCurrentBinding, "messagemode2") || !strcmp(pszCurrentBinding, "snapshot") || !strcmp(pszCurrentBinding, "screenshot") || !strcmp(pszCurrentBinding, "+jump") || !strcmp(pszCurrentBinding, "addbot") || !strcmp(pszCurrentBinding, "+voicerecord") || !strcmp(pszCurrentBinding, "-voicerecord") || 
//
//			// Movement commands
//			!strcmp(pszCurrentBinding, "testevent") /*|| !strcmp(pszCurrentBinding, "invnext") || !strcmp(pszCurrentBinding, "invprev")*/ || !strcmp(pszCurrentBinding, "+moveleft") || !strcmp(pszCurrentBinding, "-moveleft") || !strcmp(pszCurrentBinding, "+moveright") || !strcmp(pszCurrentBinding, "-moveright") || !strcmp(pszCurrentBinding, "+moveup")  || !strcmp(pszCurrentBinding, "-moveup") || !strcmp(pszCurrentBinding, "+movedown") || !strcmp(pszCurrentBinding, "-movedown") ||
//		
//			// For selecting groups
//			!strcmp(pszCurrentBinding, "slot1") || !strcmp(pszCurrentBinding, "slot2") || !strcmp(pszCurrentBinding, "slot3") || !strcmp(pszCurrentBinding, "slot4") || !strcmp(pszCurrentBinding, "slot5") || 
//		
//			// For creating groups
//			!strcmp(pszCurrentBinding, "+duck") || !strcmp(pszCurrentBinding, "-duck") ||
//		
//			// For testing ease
//			!strcmp(pszCurrentBinding, "givepoints")
//		
//			)))
//		{
			if (gViewPort)
				theProcessKeyBinding = gViewPort->KeyInput(down, keynum, pszCurrentBinding);
		
//			// Don't
//			if(!strcmp(pszCurrentBinding, "+jump") && gHUD.GetInTopDownMode())
//			{
//				gHUD.GotoAlert();
//			}

//		}
	}
	
	return theProcessKeyBinding;
}

void IN_BreakDown( void ) { KeyDown( &in_break );};
void IN_BreakUp( void ) { KeyUp( &in_break ); };
void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_JLookDown (void) {KeyDown(&in_jlook);}
void IN_JLookUp (void) {KeyUp(&in_jlook);}
void IN_MLookDown (void) {KeyDown(&in_mlook);}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}
void IN_ScrollUpDown(void) { KeyDown(&in_scrollup);}
void IN_ScrollUpUp(void) { KeyUp(&in_scrollup); }
void IN_ScrollDownDown(void) { KeyDown(&in_scrolldown); }
void IN_ScrollDownUp(void) { KeyUp(&in_scrolldown); }
void IN_ScrollLeftDown(void) { KeyDown(&in_scrollleft); }
void IN_ScrollLeftUp(void) { KeyUp(&in_scrollleft); }
void IN_ScrollRightDown(void) { KeyDown(&in_scrollright); }
void IN_ScrollRightUp(void) { KeyUp(&in_scrollright); }

void IN_ForwardDown(void)
{
	KeyDown(&in_forward);
	gHUD.m_Spectator.HandleButtonsDown( IN_FORWARD );
}

void IN_ForwardUp(void)
{
	KeyUp(&in_forward);
	gHUD.m_Spectator.HandleButtonsUp( IN_FORWARD );
}

void IN_BackDown(void)
{
	KeyDown(&in_back);
	gHUD.m_Spectator.HandleButtonsDown( IN_BACK );
}

void IN_BackUp(void)
{
	KeyUp(&in_back);
	gHUD.m_Spectator.HandleButtonsUp( IN_BACK );
}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void)
{
	KeyDown(&in_moveleft);
	gHUD.m_Spectator.HandleButtonsDown( IN_MOVELEFT );
}

void IN_MoveleftUp(void)
{
	KeyUp(&in_moveleft);
	gHUD.m_Spectator.HandleButtonsUp( IN_MOVELEFT );
}

void IN_MoverightDown(void)
{
	KeyDown(&in_moveright);
	gHUD.m_Spectator.HandleButtonsDown( IN_MOVERIGHT );
}

void IN_MoverightUp(void)
{
	KeyUp(&in_moveright);
	gHUD.m_Spectator.HandleButtonsUp( IN_MOVERIGHT );
}
void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

// needs capture by hud/vgui also
extern void __CmdFunc_InputPlayerSpecial(void);
void IN_Attack2Down(void) {KeyDownForced(&in_attack2);}
void IN_Attack2Up(void) {KeyUpForced(&in_attack2);}
void IN_UseDown (void)
{
	KeyDown(&in_use);
	gHUD.m_Spectator.HandleButtonsDown( IN_USE );
}
void IN_UseUp (void) {KeyUp(&in_use);}
void IN_JumpDown (void)
{
	KeyDown(&in_jump);
	gHUD.m_Spectator.HandleButtonsDown( IN_JUMP );

}

void IN_JumpUp (void) 
{
    KeyUp(&in_jump);
    gHUD.m_Spectator.HandleButtonsUp( IN_JUMP );
}

void IN_DuckDown(void)
{
	KeyDown(&in_duck);
	gHUD.m_Spectator.HandleButtonsDown( IN_DUCK );
}

void IN_DuckUp(void) {KeyUp(&in_duck);}
// : duck toggle
void IN_DuckToggle(void) 
{
	g_bDuckToggled = !g_bDuckToggled;
}
// :
void IN_ReloadDown(void) {KeyDownForced(&in_reload);}
void IN_ReloadUp(void) {KeyUpForced(&in_reload);}
void IN_Alt1Down(void) {KeyDown(&in_alt1);}
void IN_Alt1Up(void) {KeyUp(&in_alt1);}
void IN_GraphDown(void) {KeyDown(&in_graph);}
void IN_GraphUp(void) {KeyUp(&in_graph);}

void IN_AttackDown(void)
{
	KeyDown( &in_attack );
	gHUD.m_Spectator.HandleButtonsDown( IN_ATTACK );
}

void IN_AttackUp(void)
{
	KeyUp( &in_attack );
	in_cancel = 0;

	// Attack2up only for onos so it can end +attack onos charges.  Attack2up for all causes blink and leap to cancel if you release attack while blinking or leaping.
	if (gHUD.GetCurrentWeaponID() == AVH_ABILITY_CHARGE)
	{
		IN_Attack2Up();
	}
}

void IN_AttackDownForced(void)
{
	KeyDownForced( &in_attack );
}

void IN_AttackUpForced(void)
{
	KeyUpForced( &in_attack );
}

void IN_AttackHandlerDown(void)
{
	if (gHUD.GetCurrentWeaponID() == AVH_WEAPON_PISTOL && cl_pistoltrigger && cl_pistoltrigger->value)
	{
		IN_AttackDown();
		IN_AttackUp();
		pistolHandlerDown = true;
	}
	else
	{
		IN_AttackDown();
	}
}

void IN_AttackHandlerUp(void)
{
	// Pistol binary trigger. Check if attack was down, otherwise the engine will fire the pistol when closing the console or tabbing in as it calls every -command.
	if (gHUD.GetCurrentWeaponID() == AVH_WEAPON_PISTOL && cl_pistoltrigger && cl_pistoltrigger->value && pistolHandlerDown)
	{
		IN_AttackDown();
		IN_AttackUp();
		pistolHandlerDown = false;
	}
	else
	{
		IN_AttackUp();
	}
}

// Special handling
void IN_Cancel(void)
{
	in_cancel = 1;
}

bool CheckInAttack(void)
{
	return (in_attack.state & 1 || in_attack2.state & 1);
}

bool CheckInAttack2(void)
{
	return (in_attack2.state & 1);
}

void IN_Impulse (void)
{
	//char msg[1024];
	//sprintf(msg, "in_impulse=%s\n", gEngfuncs.Cmd_Argv(1));
	//CenterPrint(msg);

	in_impulse = atoi( gEngfuncs.Cmd_Argv(1) );
}

void IN_ScoreDown(void)
{
	KeyDown(&in_score);
	if ( gViewPort )
	{
		gViewPort->ShowScoreBoard();
	}
}

void IN_ScoreUp(void)
{
	KeyUp(&in_score);
	if ( gViewPort )
	{
		gViewPort->HideScoreBoard();
	}
}

void IN_MLookUp (void)
{
	KeyUp( &in_mlook );
	if ( !( in_mlook.state & 1 ) && lookspring->value )
	{
		V_StartPitchDrift();
	}
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val = 0.0;
	int			impulsedown, impulseup, down;
	
	impulsedown = key->state & 2;
	impulseup	= key->state & 4;
	down		= key->state & 1;
	
	if ( impulsedown && !impulseup )
	{
		// pressed and held this frame?
		val = down ? 0.5 : 0.0;
	}

	if ( impulseup && !impulsedown )
	{
		// released this frame?
		val = down ? 0.0 : 0.0;
	}

	if ( !impulsedown && !impulseup )
	{
		// held the entire frame?
		val = down ? 1.0 : 0.0;
	}

	if ( impulsedown && impulseup )
	{
		if ( down )
		{
			// released and re-pressed this frame
			val = 0.75;	
		}
		else
		{
			// pressed and released this frame
			val = 0.25;	
		}
	}

	// clear impulses
	key->state &= 1;		
	return val;
}

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles ( float frametime, float *viewangles )
{
	float	speed;
	float	up, down;
	
	if (in_speed.state & 1)
	{
		speed = frametime * cl_anglespeedkey->value;
	}
	else
	{
		speed = frametime;
	}

	if (!(in_strafe.state & 1))
	{
		viewangles[YAW] -= speed*cl_yawspeed->value*CL_KeyState (&in_right);
		viewangles[YAW] += speed*cl_yawspeed->value*CL_KeyState (&in_left);
		viewangles[YAW] = anglemod(viewangles[YAW]);
	}
	if (in_klook.state & 1)
	{
		V_StopPitchDrift ();
		viewangles[PITCH] -= speed*cl_pitchspeed->value * CL_KeyState (&in_forward);
		viewangles[PITCH] += speed*cl_pitchspeed->value * CL_KeyState (&in_back);
	}
	
	up = CL_KeyState (&in_lookup);
	down = CL_KeyState(&in_lookdown);
	
	viewangles[PITCH] -= speed*cl_pitchspeed->value * up;
	viewangles[PITCH] += speed*cl_pitchspeed->value * down;

	if (up || down)
		V_StopPitchDrift ();
		
	if (viewangles[PITCH] > cl_pitchdown->value)
		viewangles[PITCH] = cl_pitchdown->value;
	if (viewangles[PITCH] < -cl_pitchup->value)
		viewangles[PITCH] = -cl_pitchup->value;

	if (viewangles[ROLL] > 50)
		viewangles[ROLL] = 50;
	if (viewangles[ROLL] < -50)
		viewangles[ROLL] = -50;
}

/*
================
CL_CreateMove

Send the intended movement message to the server
if active == 1 then we are 1) not playing back demos ( where our commands are ignored ) and
2 ) we have finished signing on to server
================
*/
void CL_DLLEXPORT CL_CreateMove ( float frametime, struct usercmd_s *cmd, int active )
{	
	//RecClCL_CreateMove(frametime, cmd, active);
	//@linux Commander overview needs to be fixed!
	float spd;
	vec3_t viewangles;
	static vec3_t oldangles;

	if ( active && (!gViewPort || !gViewPort->IsOptionsMenuVisible()) /*&& !gHUD.GetShowingMap()*/)
	{
		int theButtonState = CL_ButtonBits(1);
		//memset( viewangles, 0, sizeof( vec3_t ) );
		//viewangles[ 0 ] = viewangles[ 1 ] = viewangles[ 2 ] = 0.0;
		//gEngfuncs.GetViewAngles( (float *)viewangles );

		//CL_AdjustAngles ( frametime, viewangles );

		memset (cmd, 0, sizeof(*cmd));
		//@ linux void IN_Move ( float frametime, usercmd_t *cmd)
		float theRotationDeltas[3] = {0,0,0}; //{90,0,90} --> top down --> 	viewangles[YAW] 
		float theTranslationDeltas[3] = {0,0,0}; //--> cmd->sidemove

		IN_Move ( frametime, cmd );

		if(gResetViewAngles)
		{
			VectorCopy(gViewAngles,viewangles);
			gResetViewAngles = false;
		}
		else
		{
			gEngfuncs.GetViewAngles( (float *)viewangles );
		}
		VectorAdd(viewangles,theRotationDeltas,viewangles);
		CL_AdjustAngles ( frametime, viewangles );

		gEngfuncs.SetViewAngles( (float *)viewangles );
		VectorCopy (viewangles,gWorldViewAngles);

		// If we're in topdown mode
		bool theProcessedMove = false;
		bool theIsSendingSpecialEvent = false;
		bool theOverrideImpulse = false;
		float theWorldX, theWorldY;
		int theScriptImpulse = 0;

		AvHMessageID theAlienAbility = MESSAGE_NULL;
		AvHMessageID theGroupMessage = MESSAGE_NULL;

		//int theUpgradeVar = gEngfuncs.GetLocalPlayer()->curstate.iuser4;
		//bool theIsParalyzed = GetHasUpgrade(theUpgradeVar, PLAYER_PARALYZED);
		if(AvHScriptManager::Instance()->GetClientMove(theButtonState, theScriptImpulse))
		{
			if(theScriptImpulse)
			{
				theOverrideImpulse = true;
			}
			//theProcessedMove = true;
			
			// TODO: Pass theButtonState to override all CL_KeyState() calls
		}
		else if(gHUD.GetInTopDownMode())
		{
			cmd->upmove = cmd->sidemove = cmd->forwardmove = 0;

			// If a button was JUST pressed or released
			vec3_t theMouseNormPos;
			AvHMessageID theTechEvent = MESSAGE_NULL;
			if(gCommanderHandler.GetMoveToWorldPosition(theWorldX, theWorldY))
			{
				// Commander wants to scroll to an area of the mini-map
				cmd->impulse = COMMANDER_MOVETO;
				cmd->upmove = theWorldX/kWorldPosNetworkConstant;
				cmd->sidemove = theWorldY/kWorldPosNetworkConstant;
				gCommanderHandler.ClearMoveToPosition();
				gHUD.ClearTrackingEntity();
				
				theIsSendingSpecialEvent = true;
				theProcessedMove = true;
			}
			else if(gCommanderHandler.GetDefaultOrderPosition(theWorldX, theWorldY))
			{
				// Commander wants to scroll to an area of the mini-map
				cmd->impulse = COMMANDER_DEFAULTORDER;
				cmd->upmove = theWorldX/kWorldPosNetworkConstant;
				cmd->sidemove = theWorldY/kWorldPosNetworkConstant;
				gCommanderHandler.ClearDefaultOrderPosition();
				gHUD.ClearTrackingEntity();
				
				theIsSendingSpecialEvent = true;
				theProcessedMove = true;
			}
			else if(gHUD.GetAndClearTechEvent(theTechEvent))
			{
				cmd->impulse = theTechEvent;
				theProcessedMove = true;
				theIsSendingSpecialEvent = true;
				gHUD.ClearTrackingEntity();

			}
			// else scroll
			else
			{
				// Scroll the view if the HUD tells us to, otherwise use normal key presses
				int theScrollX = 0, theScrollY = 0, theScrollZ = 0;
				gHUD.GetAndClearTopDownScrollAmount(theScrollX, theScrollY, theScrollZ);
				//Arrow key scrolling.
				theScrollY += CL_KeyState(&in_scrollup);
				theScrollY -= CL_KeyState(&in_scrolldown);
				theScrollX -= CL_KeyState(&in_scrollleft);
				theScrollX += CL_KeyState(&in_scrollright);
				
				if(theScrollX || theScrollY || theScrollZ)
				{
					// Commander move speed
					float kCommanderMoveSpeed = 1000;
					cmd->upmove += kCommanderMoveSpeed * theScrollY;
					cmd->sidemove += kCommanderMoveSpeed * theScrollX;
					cmd->forwardmove += kCommanderMoveSpeed * theScrollZ;

					cmd->impulse = COMMANDER_SCROLL;
					theOverrideImpulse = true;

					gHUD.ClearTrackingEntity();

					//theIsSendingSpecialEvent = true;
					theProcessedMove = true;
				}
				else if(gHUD.GetAndClearGroupEvent(theGroupMessage))
				{
					cmd->impulse = theGroupMessage;
					theIsSendingSpecialEvent = true;
					theProcessedMove = true;
					
					gHUD.SetLastHotkeySelectionEvent(theGroupMessage);
				}
//				else if(gHUD.GetTrackingEntity() > 0)
//				{
//					int theTrackingEntity = gHUD.GetTrackingEntity();
//					cmd->upmove = theTrackingEntity*kHotKeyNetworkFactor;
//					cmd->impulse = COMMANDER_TRACKENTITY;
//					
//					theIsSendingSpecialEvent = true;
//					theProcessedMove = true;
//				}
                else if(in_impulse != 0)
                {
                    bool theProcessImpulse = false;
                    switch(in_impulse)
                    {
                    case COMMANDER_SELECTALL:
                    case COMMANDER_NEXTIDLE:
                    case COMMANDER_NEXTAMMO:
                    case COMMANDER_NEXTHEALTH:
                        theProcessImpulse = true;
                        break;
                    }

                    if(theProcessImpulse)
                    {
                        cmd->impulse = in_impulse;
                        in_impulse = 0;

                        theProcessedMove = true;
                        theIsSendingSpecialEvent = true;
                    }
                }

				if(!theProcessedMove && gHUD.GetAndClearSelectionEvent(theMouseNormPos, theTechEvent))
				{
					// Store world position x,y in upmove,sidemove 
					cmd->upmove = theMouseNormPos.x*kSelectionNetworkConstant;
					cmd->sidemove = theMouseNormPos.y*kSelectionNetworkConstant;
					cmd->forwardmove = theMouseNormPos.z*kSelectionNetworkConstant;
					
					// Set impulse indicating this
					//cmd->impulse = COMMANDER_MOUSECOORD;
					
					// This could be COMMANDER_MOUSECOORD or BUILD_TURRET or one of the other BUILD_ events
					// They are all sent the same way
					cmd->impulse = theTechEvent;
					
					// Order mode isn't currently used but may be in the future
					//cmd->weaponselect = gHUD.GetOrderMode();
					
					// Set buttons.  Attack gets turned off when we're in mouse mode (apparently)
					// so we need to set the buttons manually
					cmd->buttons = theButtonState;
					if(gHUD.GetMouseOneDown())
					{
						cmd->buttons |= IN_ATTACK;
					}
					if(gHUD.GetMouseTwoDown())
					{
						cmd->buttons |= IN_ATTACK2;
					}
					
					gHUD.ClearTrackingEntity();

					theIsSendingSpecialEvent = true;
					theProcessedMove = true;
				}
			}
		}
		else if(gHUD.GetAndClearAlienAbility(theAlienAbility))
		{
			cmd->impulse = theAlienAbility;
			
			// Added by mmcguire.
			// 255 signifies that the impulse came from us and not from the console.
			cmd->weaponselect = 255; 
			
			theProcessedMove = true;
			theIsSendingSpecialEvent = true;

		}

		// else process move normally
		if(!theProcessedMove)
		{
		if ( in_strafe.state & 1 )
			{
				//cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_right);
				//cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_left);
				cmd->sidemove += kSideSpeed * CL_KeyState (&in_right);
				cmd->sidemove -= kSideSpeed * CL_KeyState (&in_left);
			}

			//cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_moveright);
			//cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_moveleft);
			cmd->sidemove += kSideSpeed * CL_KeyState (&in_moveright);
			cmd->sidemove -= kSideSpeed * CL_KeyState (&in_moveleft);

			cmd->upmove += cl_upspeed->value * CL_KeyState (&in_up);
			cmd->upmove -= cl_upspeed->value * CL_KeyState (&in_down);

			if ( !(in_klook.state & 1 ) )
			{	
				//cmd->forwardmove += cl_forwardspeed->value * CL_KeyState (&in_forward);
				cmd->forwardmove += kForwardSpeed * CL_KeyState(&in_forward);
				//cmd->forwardmove -= cl_backspeed->value * CL_KeyState (&in_back);
				cmd->forwardmove -= kBackSpeed * CL_KeyState(&in_back);
			}		
		}
		
		if(!theIsSendingSpecialEvent)
		{
			// adjust for speed key
			if ( in_speed.state & 1 )
			{
				cmd->forwardmove *= cl_movespeedkey->value;
				cmd->sidemove *= cl_movespeedkey->value;
				cmd->upmove *= cl_movespeedkey->value;
			}
			
			// clip to maxspeed
			spd = gEngfuncs.GetClientMaxspeed();

			if ( spd != 0.0 )
			{
				// scale the 3 speeds so that the total velocity is not > cl.maxspeed
				float fmov = sqrt( (cmd->forwardmove*cmd->forwardmove) + (cmd->sidemove*cmd->sidemove) + (cmd->upmove*cmd->upmove) );
				
				if ( fmov > spd )
				{
					float fratio = spd / fmov;
					cmd->forwardmove *= fratio;
					cmd->sidemove *= fratio;
					cmd->upmove *= fratio;
				}
			}

			// Allow mice and other controllers to add their inputs
			IN_Move ( frametime, cmd );
			
			if(!theOverrideImpulse)
			{
				if (in_impulse == ORDER_ACK){
					gHUD.SetDrawOrderOverlay(false);
				}

				cmd->impulse = in_impulse;
				in_impulse = 0;
			}
			
			cmd->weaponselect = g_weaponselect;
			g_weaponselect = 0;
			
			//
			// set button and flag bits
			//
			cmd->buttons = theButtonState;

			// If they're in a modal dialog, or we're stunned, ignore the attack button.
			int theLocalUpgrades = gHUD.GetLocalUpgrades();
			if( GetClientVoiceMgr()->IsInSquelchMode() )
			{
				cmd->buttons &= ~IN_ATTACK;
			}
			
			// Using joystick?
			if ( in_joystick->value )
			{
				if ( cmd->forwardmove > 0 )
				{
					cmd->buttons |= IN_FORWARD;
				}
				else if ( cmd->forwardmove < 0 )
				{
					cmd->buttons |= IN_BACK;
				}
			}
		}
	}
	
	gEngfuncs.GetViewAngles( (float *)viewangles );
	// Set current view angles.

	// Set current view angles but not if frozen (this still allows you to rotate in first-person, but player model won't change)
	int theUser4 = gHUD.GetLocalUpgrades();
	bool theIsFrozen = GetHasUpgrade(theUser4, MASK_PLAYER_STUNNED) || GetHasUpgrade(theUser4, MASK_ALIEN_EMBRYO);

	if ( g_iAlive && !theIsFrozen)
	{
		VectorCopy( viewangles, cmd->viewangles );
		VectorCopy( viewangles, oldangles );
	}
	else
	{
		VectorCopy( oldangles, cmd->viewangles );
	}

	//Bench_SetViewAngles( 1, (float *)&cmd->viewangles, frametime, cmd );
}

/*
============
CL_IsDead

Returns 1 if health is <= 0
============
*/
int	CL_IsDead( void )
{
	return ( gHUD.m_Health.m_iHealth <= 0 ) ? 1 : 0;
}

/*
============
CL_ButtonBits

Returns appropriate button info for keyboard and mouse state
Set bResetState to 1 to clear old state info
============
*/
int CL_ButtonBits( int bResetState )
{
	int bits = 0;

	if ( in_attack.state & 3 )
	{
		bits |= IN_ATTACK;
	}

	if ( in_speed.state & 3 )
	{
		bits |= IN_WALK;
	}

	// : duck toggle
	if ( g_bDuckToggled )
	{
		if (!(in_duck.state & 3))
		{
			bits |= IN_DUCK;
		}
	} 
	else if (in_duck.state & 3)
	{
		bits |= IN_DUCK;
	}
	// :

	if (in_jump.state & 3)
	{
		bits |= IN_JUMP;
	}

	if ( in_forward.state & 3 )
	{
		bits |= IN_FORWARD;
	}
	
	if (in_back.state & 3)
	{
		bits |= IN_BACK;
	}

	if (in_use.state & 3)
	{
		bits |= IN_USE;
	}

	if (in_cancel)
	{
		bits |= IN_CANCEL;
	}

	if ( in_left.state & 3 )
	{
		bits |= IN_LEFT;
	}
	
	if (in_right.state & 3)
	{
		bits |= IN_RIGHT;
	}
	
	if ( in_moveleft.state & 3 )
	{
		bits |= IN_MOVELEFT;
	}
	
	if (in_moveright.state & 3)
	{
		bits |= IN_MOVERIGHT;
	}

	if (in_attack2.state & 3)
	{
		bits |= IN_ATTACK2;
	}

	if (in_reload.state & 3)
	{
		bits |= IN_RELOAD;
	}

	if (in_alt1.state & 3)
	{
		bits |= IN_ALT1;
	}

	if ( in_score.state & 3 )
	{
		bits |= IN_SCORE;
	}

	// Dead or in intermission? Shore scoreboard, too
	if ( CL_IsDead() || gHUD.m_iIntermission )
	{
		bits |= IN_SCORE;
	}

	if ( bResetState )
	{
		in_attack.state &= ~2;
		in_speed.state &= ~2;
		in_duck.state &= ~2;
		in_jump.state &= ~2;
		in_forward.state &= ~2;
		in_back.state &= ~2;
		in_use.state &= ~2;
		in_left.state &= ~2;
		in_right.state &= ~2;
		in_moveleft.state &= ~2;
		in_moveright.state &= ~2;
		in_attack2.state &= ~2;
		in_reload.state &= ~2;
		in_alt1.state &= ~2;
		in_score.state &= ~2;
	}

	return bits;
}

/*
============
CL_ResetButtonBits

============
*/
void CL_ResetButtonBits( int bits )
{
	int bitsNew = CL_ButtonBits( 0 ) ^ bits;

	// Has the attack button been changed
	if ( bitsNew & IN_ATTACK )
	{
		// Was it pressed? or let go?
		if ( bits & IN_ATTACK )
		{
			KeyDown( &in_attack );
		}
		else
		{
			// totally clear state
			in_attack.state &= ~7;
			in_attack2.state &= ~7;
		}
	}
}

void NsVersion(void)
{
	static char theGameVersion[1024];

	string theGameVersionString;

	theGameVersionString = "v" + MakeStringFromInt(BALANCE_VAR(kGameVersionMajor)) + "." + MakeStringFromInt(BALANCE_VAR(kGameVersionMinor)) + "b" +
		MakeStringFromInt(BALANCE_VAR(kGameVersionRevision)) + "  " + __DATE__ + " " + __TIME__;

	//memset(theGameVersion, 0, 1024);
	strcpy(theGameVersion, theGameVersionString.c_str());

	gEngfuncs.Con_Printf("%s %s\n", kAvHGameAcronymn, theGameVersion);
}

void EchoDev(void)
{
	////This method needs spaces added back in if using multiple args.
	////Printing each arg followed by new line won't show in developer 1 text corner since new line gets executed seprarately last.
	//char strbuff[8191];
	//strbuff[0] = 0;
	//
	//int	i;
	//for (i = 1; i < gEngfuncs.Cmd_Argc(); i++) {
	//	strcat(strbuff, gEngfuncs.Cmd_Argv(i));
	//}
	//gEngfuncs.Con_Printf("%s\n", strbuff);

	//One arg. Use quotes.
	gEngfuncs.Con_Printf("%s\n", gEngfuncs.Cmd_Argv(1));
}

void NsPreset(void)
{
	int presetChoice = atoi(gEngfuncs.Cmd_Argv(1));
	bool printToChat = gViewPort;
	char execText[1024];
	//char localizedText[1024];

	switch (presetChoice)
	{
	case 1:
		ClientCmd("exec presetcfgs/AV/ns32.cfg");

		if (printToChat)
		{
			// Localize later.
			//sprintf(localizedText, CHudTextMessage::BufferedLocaliseTextString("#Preset1"));
			snprintf(execText, 1024, "%c** %s\n", HUD_PRINTTALK, "Classic NS audio/visual presets applied. Left click once to fix bug. See console for details.");
			gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, (int)strlen(execText) + 1, execText);
			ClientCmd("echo \"NOTE: You must left click once after applying these settings, or your next left click won't register. It's an HL engine bug.\""); // Use echo or it's out of order.
		}
		else
		{
			ClientCmd("toggleconsole");
		}
		break;
	case 2:
		ClientCmd("exec presetcfgs/AV/newdefault.cfg");

		if (printToChat)
		{
			// Localize later.
			//sprintf(localizedText, CHudTextMessage::BufferedLocaliseTextString("#Preset2"));
			snprintf(execText, 1024, "%c** %s\n", HUD_PRINTTALK, "NS 3.3 audio/visual presets applied. Left click once to fix bug. See console for details. ");
			gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, (int)strlen(execText) + 1, execText);
			ClientCmd("echo \"NOTE: You must left click once after applying these settings, or your next left click won't register. It's an HL engine bug.\""); // Use echo or it's out of order.
		}
		else
		{
			ClientCmd("toggleconsole");
		}
		break;
	case 3:
		ClientCmd("exec presetcfgs/AV/competitive.cfg");

		if (printToChat)
		{
			// Localize later.
			//sprintf(localizedText, CHudTextMessage::BufferedLocaliseTextString("#Preset3"));
			snprintf(execText, 1024, "%c** %s\n", HUD_PRINTTALK, "Competitive audio/visual presets applied. Left click once to fix bug. See console for details.");
			gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, (int)strlen(execText) + 1, execText);
			ClientCmd("echo \"NOTE: You must left click once after applying these settings, or your next left click won't register. It's an HL engine bug.\""); // Use echo or it's out of order.
		}
		else
		{
			ClientCmd("toggleconsole");
		}
		break;
	default:
		gEngfuncs.Con_Printf("NS configuration preset selector. Usage:\n1: apply NS 3.2 settings\n2: apply NS 3.3 settings\n3: apply competitive settings\n");
	}
}

void NsRates(void)
{
	int ratesChoice = atoi(gEngfuncs.Cmd_Argv(1));
	char execText[30];

	if (ratesChoice >= 1 && ratesChoice <= 20)
	{
		snprintf(execText, 30, "exec presetcfgs/rates/%d.cfg", ratesChoice);
		ClientCmd(execText);
	}
	// Don't show this if 0 is entered as an arg, as it's the normal behavior for the blank default setting in options.
	else if (gEngfuncs.Cmd_Argc() <= 1 || ratesChoice != 0)
	{
		gEngfuncs.Con_Printf("nsrates selects from preset network rate commands. Start from \"nsrates 1\" and increase the number until your connection feels stable.\n");
	}
}


/*
============
InitInput
============
*/
void InitInput (void)
{
	gEngfuncs.pfnAddCommand ("+moveup",IN_UpDown);
	gEngfuncs.pfnAddCommand ("-moveup",IN_UpUp);
	gEngfuncs.pfnAddCommand ("+movedown",IN_DownDown);
	gEngfuncs.pfnAddCommand ("-movedown",IN_DownUp);
	gEngfuncs.pfnAddCommand ("+left",IN_LeftDown);
	gEngfuncs.pfnAddCommand ("-left",IN_LeftUp);
	gEngfuncs.pfnAddCommand ("+right",IN_RightDown);
	gEngfuncs.pfnAddCommand ("-right",IN_RightUp);
	gEngfuncs.pfnAddCommand ("+forward",IN_ForwardDown);
	gEngfuncs.pfnAddCommand ("-forward",IN_ForwardUp);
	gEngfuncs.pfnAddCommand ("+back",IN_BackDown);
	gEngfuncs.pfnAddCommand ("-back",IN_BackUp);
	gEngfuncs.pfnAddCommand ("+lookup", IN_LookupDown);
	gEngfuncs.pfnAddCommand ("-lookup", IN_LookupUp);
	gEngfuncs.pfnAddCommand ("+lookdown", IN_LookdownDown);
	gEngfuncs.pfnAddCommand ("-lookdown", IN_LookdownUp);
	gEngfuncs.pfnAddCommand ("+strafe", IN_StrafeDown);
	gEngfuncs.pfnAddCommand ("-strafe", IN_StrafeUp);
	gEngfuncs.pfnAddCommand ("+moveleft", IN_MoveleftDown);
	gEngfuncs.pfnAddCommand ("-moveleft", IN_MoveleftUp);
	gEngfuncs.pfnAddCommand ("+moveright", IN_MoverightDown);
	gEngfuncs.pfnAddCommand ("-moveright", IN_MoverightUp);
	gEngfuncs.pfnAddCommand ("+speed", IN_SpeedDown);
	gEngfuncs.pfnAddCommand ("-speed", IN_SpeedUp);
	gEngfuncs.pfnAddCommand ("+attack", IN_AttackHandlerDown);
	gEngfuncs.pfnAddCommand ("-attack", IN_AttackHandlerUp);
	//gEngfuncs.pfnAddCommand ("+movement", IN_Attack2Down);
	//gEngfuncs.pfnAddCommand ("-movement", IN_Attack2Up);
	gEngfuncs.pfnAddCommand ("+use", IN_UseDown);
	gEngfuncs.pfnAddCommand ("-use", IN_UseUp);
	gEngfuncs.pfnAddCommand ("+jump", IN_JumpDown);
	gEngfuncs.pfnAddCommand ("-jump", IN_JumpUp);
	gEngfuncs.pfnAddCommand ("impulse", IN_Impulse);
	gEngfuncs.pfnAddCommand ("+klook", IN_KLookDown);
	gEngfuncs.pfnAddCommand ("-klook", IN_KLookUp);
	gEngfuncs.pfnAddCommand ("+mlook", IN_MLookDown);
	gEngfuncs.pfnAddCommand ("-mlook", IN_MLookUp);
	gEngfuncs.pfnAddCommand ("+jlook", IN_JLookDown);
	gEngfuncs.pfnAddCommand ("-jlook", IN_JLookUp);
	gEngfuncs.pfnAddCommand ("+duck", IN_DuckDown);
	gEngfuncs.pfnAddCommand ("-duck", IN_DuckUp);
	// : duck toggle
	gEngfuncs.pfnAddCommand ("toggleduck", IN_DuckToggle);
	// :
	gEngfuncs.pfnAddCommand ("+reload", IN_ReloadDown);
	gEngfuncs.pfnAddCommand ("-reload", IN_ReloadUp);
	gEngfuncs.pfnAddCommand ("+alt1", IN_Alt1Down);
	gEngfuncs.pfnAddCommand ("-alt1", IN_Alt1Up);
	gEngfuncs.pfnAddCommand ("+score", IN_ScoreDown);
	gEngfuncs.pfnAddCommand ("-score", IN_ScoreUp);
	gEngfuncs.pfnAddCommand ("+showscores", IN_ScoreDown);
	gEngfuncs.pfnAddCommand ("-showscores", IN_ScoreUp);
	gEngfuncs.pfnAddCommand ("+graph", IN_GraphDown);
	gEngfuncs.pfnAddCommand ("-graph", IN_GraphUp);
	gEngfuncs.pfnAddCommand ("+break",IN_BreakDown);
	gEngfuncs.pfnAddCommand ("-break",IN_BreakUp);
	gEngfuncs.pfnAddCommand("+scrollup", IN_ScrollUpDown);
	gEngfuncs.pfnAddCommand("-scrollup", IN_ScrollUpUp);
	gEngfuncs.pfnAddCommand("+scrolldown", IN_ScrollDownDown);
	gEngfuncs.pfnAddCommand("-scrolldown", IN_ScrollDownUp);
	gEngfuncs.pfnAddCommand("+scrollleft", IN_ScrollLeftDown);
	gEngfuncs.pfnAddCommand("-scrollleft", IN_ScrollLeftUp);
	gEngfuncs.pfnAddCommand("+scrollright", IN_ScrollRightDown);
	gEngfuncs.pfnAddCommand("-scrollright", IN_ScrollRightUp);

	gEngfuncs.pfnAddCommand("nsversion", NsVersion);
	gEngfuncs.pfnAddCommand("echodev", EchoDev);
	gEngfuncs.pfnAddCommand("nspreset", NsPreset);
	gEngfuncs.pfnAddCommand("nsrates", NsRates);

	lookstrafe			= gEngfuncs.pfnRegisterVariable ( "lookstrafe", "0", FCVAR_ARCHIVE );
	lookspring			= gEngfuncs.pfnRegisterVariable ( "lookspring", "0", FCVAR_ARCHIVE );
	cl_anglespeedkey	= gEngfuncs.pfnRegisterVariable ( "cl_anglespeedkey", "0.67", 0 );
	cl_yawspeed			= gEngfuncs.pfnRegisterVariable ( "cl_yawspeed", "210", 0 );
	cl_pitchspeed		= gEngfuncs.pfnRegisterVariable ( "cl_pitchspeed", "225", 0 );
	cl_upspeed			= gEngfuncs.pfnRegisterVariable ( "cl_upspeed", "320", 0 );
	//cl_forwardspeed		= gEngfuncs.pfnRegisterVariable ( "cl_forwardspeed", "400", FCVAR_ARCHIVE );
	//cl_backspeed		= gEngfuncs.pfnRegisterVariable ( "cl_backspeed", "400", FCVAR_ARCHIVE );
	//cl_sidespeed		= gEngfuncs.pfnRegisterVariable ( "cl_sidespeed", "400", 0 );
	cl_movespeedkey		= gEngfuncs.pfnRegisterVariable ( "cl_movespeedkey", "0.3", 0 );
	cl_pitchup			= gEngfuncs.pfnRegisterVariable ( "cl_pitchup", "89", 0 );
	cl_pitchdown		= gEngfuncs.pfnRegisterVariable ( "cl_pitchdown", "89", 0 );

	cl_vsmoothing		= gEngfuncs.pfnRegisterVariable ( "cl_vsmoothing", "0.05", FCVAR_ARCHIVE );

	m_pitch			    = gEngfuncs.pfnRegisterVariable ( "m_pitch","0.022", FCVAR_ARCHIVE );
	m_yaw				= gEngfuncs.pfnRegisterVariable ( "m_yaw","0.022", FCVAR_ARCHIVE );
	m_forward			= gEngfuncs.pfnRegisterVariable ( "m_forward","1", FCVAR_ARCHIVE );
	m_side				= gEngfuncs.pfnRegisterVariable ( "m_side","0.8", FCVAR_ARCHIVE );

	cl_autohelp			= gEngfuncs.pfnRegisterVariable ( kvAutoHelp, "1.0", FCVAR_ARCHIVE );
	cl_centerentityid	= gEngfuncs.pfnRegisterVariable ( kvCenterEntityID, "0.0", FCVAR_ARCHIVE );
	cl_musicenabled		= gEngfuncs.pfnRegisterVariable ( kvMusicEnabled, "1", FCVAR_ARCHIVE );
	cl_musicvolume		= gEngfuncs.pfnRegisterVariable ( kvMusicVolume, "0.6", FCVAR_ARCHIVE );
	cl_musicdir			= gEngfuncs.pfnRegisterVariable ( kvMusicDirectory, "", FCVAR_ARCHIVE);
	cl_musicdelay		= gEngfuncs.pfnRegisterVariable ( kvMusicDelay, "90", FCVAR_ARCHIVE);
	cl_dynamiclights	= gEngfuncs.pfnRegisterVariable ( kvDynamicLights, "1", FCVAR_ARCHIVE );
	cl_buildmessages	= gEngfuncs.pfnRegisterVariable ( kvBuildMessages, "1", FCVAR_ARCHIVE);
	cl_quickselecttime	= gEngfuncs.pfnRegisterVariable ( kvQuickSelectTime, ".15", FCVAR_ARCHIVE );
	cl_highdetail		= gEngfuncs.pfnRegisterVariable ( kvHighDetail, "1", FCVAR_ARCHIVE );
	cl_cmhotkeys		= gEngfuncs.pfnRegisterVariable ( kvCMHotkeys, "qwerasdfzxcv", FCVAR_ARCHIVE );
	//cl_forcedefaultfov	= gEngfuncs.pfnRegisterVariable ( kvForceDefaultFOV, "0", FCVAR_ARCHIVE );
	cl_particleinfo		= gEngfuncs.pfnRegisterVariable ( kvParticleInfo, "0", FCVAR_ARCHIVE );
	//cl_widescreen		= gEngfuncs.pfnRegisterVariable	( kvWidescreen, "1", FCVAR_ARCHIVE );
	cl_ambientsound		= gEngfuncs.pfnRegisterVariable	( kvAmbientSound, "0.6", FCVAR_ARCHIVE);
	senslock			= gEngfuncs.pfnRegisterVariable	("senslock", "0", FCVAR_ARCHIVE);
	cl_chatbeep			= gEngfuncs.pfnRegisterVariable	("cl_chatbeep", "1", FCVAR_ARCHIVE);
	cl_mutemenu			= gEngfuncs.pfnRegisterVariable ("cl_mutemenu", "3", FCVAR_ARCHIVE);
	cl_weaponcfgs		= gEngfuncs.pfnRegisterVariable ("cl_weaponcfgs", "1", FCVAR_ARCHIVE);
	cl_pistoltrigger	= gEngfuncs.pfnRegisterVariable ("cl_pistoltrigger", "1", FCVAR_ARCHIVE);
	cl_cmcancellast		= gEngfuncs.pfnRegisterVariable("cl_cmcancellast", "0", FCVAR_ARCHIVE);
	cl_teamcfgs			= gEngfuncs.pfnRegisterVariable("cl_teamcfgs", "1", FCVAR_ARCHIVE);

	gEngfuncs.pfnRegisterVariable("cl_placebo", "0", 0); // Junk command for options menu dividers.

	// Initialize third person camera controls.
	CAM_Init();
	// Initialize inputs
	IN_Init();
	// Initialize keyboard
	KB_Init();
	// Initialize view system
	V_Init();
}

/*
============
ShutdownInput
============
*/
void ShutdownInput (void)
{
	IN_Shutdown();
	KB_Shutdown();
}
#include "interface.h"
void CL_DLLEXPORT HUD_Shutdown( void )
{
//	RecClShutdown();

	ShutdownInput();

	gHUD.Shutdown();
}
