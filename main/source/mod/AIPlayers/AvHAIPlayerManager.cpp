#include "AvHAIPlayerManager.h"
#include "AvHAIPlayer.h"
#include "../AvHGamerules.h"
#include "../dlls/client.h"
#include <time.h>

float last_think_time = 0.0f;
float BotDeltaTime = 0.01666667f;

AvHAIPlayer ActiveAIPlayers[MAX_PLAYERS];

extern cvar_t avh_botautomode;
extern cvar_t avh_botsenabled;
extern cvar_t avh_botminplayers;
extern cvar_t avh_botusemapdefaults;

float LastAIPlayerCountUpdate = 0.0f;

int BotNameIndex = 0;

void AIMGR_UpdateAIPlayerCounts()
{
	// Don't add or remove bots too quickly, otherwise it can cause lag or even overflows
	if (gpGlobals->time - LastAIPlayerCountUpdate < 0.2f) { return; }

	// If game has ended, kick bots that have dropped back to the ready room
	if (GetGameRules()->GetVictoryTeam() != TEAM_IND)
	{
		AIMGR_RemoveBotsInReadyRoom();
		return;
	}

	LastAIPlayerCountUpdate = gpGlobals->time;

	if (avh_botsenabled.value == 0)
	{
		if (AIMGR_GetNumAIPlayers() > 0)
		{
			AIMGR_RemoveAIPlayerFromTeam(0);
			
		}
		return;
	}

	if (avh_botautomode.value == 0) // Manual mode: do nothing, server can manually add/remove as they want
	{
		return;
	}

	if (avh_botautomode.value == 1) // Balance only: bots will only be added and removed to ensure teams remain balanced
	{
		AIMGR_UpdateTeamBalance();
		return;
	}

	if (avh_botautomode.value == 2) // Fill teams: bots will be added and removed to maintain a minimum player count
	{
		AIMGR_UpdateFillTeams();
		return;
	}
}

void AIMGR_UpdateTeamBalance()
{
	AvHTeamNumber teamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber teamB = GetGameRules()->GetTeamBNumber();

	// If team A has more players, then either remove a bot from team A, or add one to B to bring them in line
	if (GetGameRules()->GetTeamAPlayerCount() > GetGameRules()->GetTeamBPlayerCount())
	{
		// Favour removing bots to balance teams over adding more
		if (AIMGR_AIPlayerExistsOnTeam(teamA))
		{
			AIMGR_RemoveAIPlayerFromTeam(1);
			return;
		}
		else
		{
			AIMGR_AddAIPlayerToTeam(2);
			return;
		}
	}

	// Do the same if team B outmatches team A
	if (GetGameRules()->GetTeamBPlayerCount() > GetGameRules()->GetTeamAPlayerCount())
	{
		// Again, favour removing bots over adding more
		if (AIMGR_AIPlayerExistsOnTeam(teamB))
		{
			AIMGR_RemoveAIPlayerFromTeam(2);
			return;
		}
		else
		{
			AIMGR_AddAIPlayerToTeam(1);
			return;
		}
	}

	// If both teams are evenly matched, check to ensure we don't have bots on both sides. The purpose of balance mode
	// is to maintain the minimum bots required to keep teams even, so get rid of extras if needed
	if (AIMGR_AIPlayerExistsOnTeam(teamA) && AIMGR_AIPlayerExistsOnTeam(teamB))
	{
		AIMGR_RemoveAIPlayerFromTeam(teamB);
		return;
	}

}

void AIMGR_UpdateFillTeams()
{
	AvHTeamNumber teamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber teamB = GetGameRules()->GetTeamBNumber();

	int TeamSizeA = GetGameRules()->GetTeamAPlayerCount();
	int TeamSizeB = GetGameRules()->GetTeamBPlayerCount();
	int TotalPlayers = TeamSizeA + TeamSizeB;
	int NumDesiredPlayers = min(gpGlobals->maxClients, (int)floorf(avh_botminplayers.value));

	// We have exceeded our minimum desired player count, start removing bots to make more room for humans
	if (TotalPlayers > NumDesiredPlayers)
	{
		AIMGR_RemoveAIPlayerFromTeam(0);
		return;
	}

	// Add bots to ensure we reach the number of desired players
	if (TotalPlayers < NumDesiredPlayers)
	{
		AIMGR_AddAIPlayerToTeam(0);
		return;
	}
	
	if (TeamSizeA > (TeamSizeB + 1))
	{
		if (AIMGR_AIPlayerExistsOnTeam(teamA))
		{
			AIMGR_RemoveAIPlayerFromTeam(1);
		
		}

		return;
	}

	if (TeamSizeB > (TeamSizeA + 1))
	{
		if (AIMGR_AIPlayerExistsOnTeam(teamB))
		{
			AIMGR_RemoveAIPlayerFromTeam(2);

		}

		return;
	}

}

void AIMGR_RemoveAIPlayerFromTeam(int Team)
{
	if (AIMGR_GetNumAIPlayers() == 0) { return; }

	AvHTeamNumber DesiredTeam = TEAM_IND;

	AvHTeamNumber teamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber teamB = GetGameRules()->GetTeamBNumber();

	if (Team > 0)
	{
		DesiredTeam = (Team == 1) ? teamA : teamB;
	}
	else
	{
		if (GetGameRules()->GetTeamAPlayerCount() > GetGameRules()->GetTeamBPlayerCount())
		{
			DesiredTeam = teamA;
		}
		else
		{
			DesiredTeam = teamB;
		}
	}

	// We will go through the potential bots we could kick. We want to avoid kicking bots which have a lot of
	// resources tied up in them or are commanding, which could cause big disruption to the team they're leaving

	int MinValue = 0; // Track the least valuable bot on the desired team.
	int IndexToKick = -1; // Current bot to be kicked

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		// Don't kick if the slot is empty, or the bot in that slot isn't on the right team
		if (!ActiveAIPlayers[i].Player || ActiveAIPlayers[i].Player->GetTeam() != DesiredTeam) { continue; }

		AvHPlayer* theAIPlayer = ActiveAIPlayers[i].Player;

		float BotValue = theAIPlayer->GetResources();

		AvHPlayerClass theAIPlayerClass = (AvHPlayerClass)theAIPlayer->GetEffectivePlayerClass();
		
		switch (theAIPlayerClass)
		{
			case PLAYERCLASS_COMMANDER:
				BotValue += 1000.0f; // Ensure this guy isn't kicked unless he's the only bot on the team!
				break;
			case PLAYERCLASS_ALIVE_HEAVY_MARINE:
				BotValue += kHeavyArmorCost;
				break;
			case PLAYERCLASS_ALIVE_JETPACK_MARINE:
				BotValue += kJetpackCost;
				break;
			case PLAYERCLASS_ALIVE_LEVEL2:
				BotValue += kGorgeCost;
				break;
			case PLAYERCLASS_ALIVE_LEVEL3:
				BotValue += kLerkCost;
				break;
			case PLAYERCLASS_ALIVE_LEVEL4:
				BotValue += kFadeCost;
				break;
			case PLAYERCLASS_ALIVE_LEVEL5:
				BotValue += kOnosCost;
				break;
			case PLAYERCLASS_ALIVE_GESTATING:
				BotValue += 10.0f;
				break;
			case PLAYERCLASS_DEAD_ALIEN:
			case PLAYERCLASS_DEAD_MARINE:
				BotValue -= 10.0f; // Favour kicking bots who are dead rather than alive
				break;
			case PLAYERCLASS_REINFORCING:
				BotValue -= 5.0f;
				break;
			default:
				break;
		}

		if (IndexToKick < 0 || BotValue < MinValue)
		{
			IndexToKick = i;
			MinValue = BotValue;
		}
	}

	if (IndexToKick > -1)
	{
		ActiveAIPlayers[IndexToKick].Player->Kick();

		memset(&ActiveAIPlayers[IndexToKick], 0, sizeof(AvHAIPlayer));
	}

}

void AIMGR_AddAIPlayerToTeam(int Team)
{
	int NewBotIndex = -1;
	edict_t* BotEnt = nullptr;

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (!ActiveAIPlayers[i].Player)
		{
			NewBotIndex = i;
			break;
		}
	}

	if (NewBotIndex < 0)
	{
		ALERT(at_console, "Bot limit reached, cannot add more\n");
		return;
	}

	if (AIMGR_GetNumAIPlayers() == 0)
	{
		// Initialise the name index to a random number so we don't always get the same bot names
		BotNameIndex = RANDOM_LONG(0, 31);
	}

	AvHTeamNumber DesiredTeam = TEAM_IND;

	AvHTeamNumber teamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber teamB = GetGameRules()->GetTeamBNumber();

	if (Team > 0)
	{
		DesiredTeam = (Team == 1) ? teamA : teamB;
	}

	// Retrieve the current bot name and then cycle the index so the names are always unique
	string NewName = "[BOT]" + BotNames[BotNameIndex];

	BotEnt = (*g_engfuncs.pfnCreateFakeClient)(NewName.c_str());

	BotNameIndex++;

	if (BotNameIndex > 31)
	{
		BotNameIndex = 0;
	}

	if (!BotEnt)
	{
		ALERT(at_console, "Failed to create AI player: server is full\n");
		return;
	}

	char ptr[128];  // allocate space for message from ClientConnect
	int clientIndex;

	char* infobuffer = (*g_engfuncs.pfnGetInfoKeyBuffer)(BotEnt);
	clientIndex = ENTINDEX(BotEnt);

	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "model", "");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "rate", "3500.000000");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "cl_updaterate", "20");

	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "cl_lw", "0");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "cl_lc", "0");

	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "tracker", "0");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "cl_dlmax", "128");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "lefthand", "1");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "friends", "0");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "dm", "0");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "ah", "1");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "_vgui_menus", "0");

	ClientConnect(BotEnt, STRING(BotEnt->v.netname), "127.0.0.1", ptr);
	ClientPutInServer(BotEnt);

	BotEnt->v.flags |= FL_FAKECLIENT; // Shouldn't be needed but just to be sure

	BotEnt->v.idealpitch = BotEnt->v.v_angle.x;
	BotEnt->v.ideal_yaw = BotEnt->v.v_angle.y;

	BotEnt->v.pitch_speed = 270;  // slightly faster than HLDM of 225
	BotEnt->v.yaw_speed = 250; // slightly faster than HLDM of 210

	AvHPlayer* theNewAIPlayer = GetClassPtr((AvHPlayer*)&BotEnt->v);

	if (theNewAIPlayer)
	{
		if (DesiredTeam != TEAM_IND)
		{
			ALERT(at_console, "Adding AI Player to team: %d\n", (int)Team);
			GetGameRules()->AttemptToJoinTeam(theNewAIPlayer, DesiredTeam, false);
		}
		else
		{
			ALERT(at_console, "Auto-assigning AI Player to team\n");
			GetGameRules()->AutoAssignPlayer(theNewAIPlayer);
		}

		ActiveAIPlayers[NewBotIndex].Player = theNewAIPlayer;
		ActiveAIPlayers[NewBotIndex].Edict = BotEnt;
		ActiveAIPlayers[NewBotIndex].Team = theNewAIPlayer->GetTeam();
	}
	else
	{
		ALERT(at_console, "Failed to create AI player: invalid AvHPlayer instance\n");
	}

}

byte BotThrottledMsec(AvHAIPlayer* inAIPlayer)
{
	// Thanks to The Storm (ePODBot) for this one, finally fixed the bot running speed!
	int newmsec = (int)((gpGlobals->time - inAIPlayer->f_previous_command_time) * 1000);
	
	if (newmsec > 255)
	{
		newmsec = 255;
	}		

	return (byte)newmsec;
}

void AIMGR_UpdateAIPlayers()
{
	// If bots are not enabled then do nothing
	if (avh_botsenabled.value == 0) { return; }

	static clock_t prevtime = 0.0f;
	static clock_t currTime = 0.0f;

	currTime = clock();

	double timeSinceLastThink = (double)((currTime - last_think_time) / CLOCKS_PER_SEC);

	if (IS_DEDICATED_SERVER() || timeSinceLastThink >= BOT_MIN_FRAME_TIME)
	{
		BotDeltaTime = timeSinceLastThink;


		for (int bot_index = 0; bot_index < MAX_PLAYERS; bot_index++)
		{
			if (!ActiveAIPlayers[bot_index].Player) { continue; } // Slot isn't filled

			AvHAIPlayer* bot = &ActiveAIPlayers[bot_index];
			
			// Needed to correctly handle client prediction and physics calculations
			byte adjustedmsec = BotThrottledMsec(bot);

			// save the command time
			bot->f_previous_command_time = gpGlobals->time;

			// Simulate PM_PlayerMove so client prediction and stuff can be executed correctly.
			RUN_AI_MOVE(bot->Edict, bot->Edict->v.v_angle, bot->ForwardMove,
				bot->SideMove, bot->UpMove, bot->Button, bot->Impulse, adjustedmsec);
		}

	}

	prevtime = currTime;

}

float AIMGR_GetBotDeltaTime()
{
	return BotDeltaTime;
}

int AIMGR_GetNumAIPlayers()
{
	int Result = 0;

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (ActiveAIPlayers[i].Player != nullptr)
		{
			Result++;
		}
	}

	return Result;
}

int AIMGR_GetNumAIPlayersOnTeam(AvHTeamNumber Team)
{
	int Result = 0;

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (ActiveAIPlayers[i].Player != nullptr && ActiveAIPlayers[i].Team == Team)
		{
			Result++;
		}
	}

	return Result;
}

int AIMGR_AIPlayerExistsOnTeam(AvHTeamNumber Team)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (ActiveAIPlayers[i].Player != nullptr && ActiveAIPlayers[i].Team == Team)
		{
			return true;
		}
	}

	return false;
}

void AIMGR_RemoveBotsInReadyRoom()
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (ActiveAIPlayers[i].Player != nullptr && ActiveAIPlayers[i].Player->GetInReadyRoom())
		{
			ActiveAIPlayers[i].Player->Kick();

			memset(&ActiveAIPlayers[i], 0, sizeof(AvHAIPlayer));
		}
	}
}