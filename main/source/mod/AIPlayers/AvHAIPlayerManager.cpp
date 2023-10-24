#include "AvHAIPlayerManager.h"
#include "AvHAIPlayer.h"
#include "AvHAIMath.h"
#include "AvHAITactical.h"
#include "AvHAINavigation.h"
#include "AvHAIConfig.h"
#include "AvHAIWeaponHelper.h"
#include "AvHAIHelper.h"
#include "../AvHGamerules.h"
#include "../dlls/client.h"
#include <time.h>

double last_think_time = 0.0;
float BotDeltaTime = 0.01666667f;

AvHAIPlayer ActiveAIPlayers[MAX_PLAYERS];

extern cvar_t avh_botautomode;
extern cvar_t avh_botsenabled;
extern cvar_t avh_botminplayers;
extern cvar_t avh_botusemapdefaults;
extern cvar_t avh_botcommandermode;

float LastAIPlayerCountUpdate = 0.0f;

int BotNameIndex = 0;

float AIStartedTime = 0.0f; // Used to give 5-second grace period before adding bots

extern int m_spriteTexture;

Vector DebugVector1 = ZERO_VECTOR;
Vector DebugVector2 = ZERO_VECTOR;

string BotNames[MAX_PLAYERS] = { "MrRobot",
									"Wall-E",
									"BeepBoop",
									"Robotnik",
									"JonnyAutomaton",
									"Burninator",
									"SteelDeath",
									"Meatbag",
									"Undertaker",
									"Botini",
									"Robottle",
									"Rusty",
									"HeavyMetal",
									"Combot",
									"BagelLover",
									"Screwdriver",
									"LoveBug",
									"iSmash",
									"Chippy",
									"Baymax",
									"BoomerBot",
									"Jarvis",
									"Marvin",
									"Data",
									"Scrappy",
									"Mortis",
									"TerrorHertz",
									"Omicron",
									"Herbie",
									"Robogeddon",
									"Velociripper",
									"TerminalFerocity"
};


void AIMGR_UpdateAIPlayerCounts()
{
	// Don't add or remove bots too quickly, otherwise it can cause lag or even overflows
	if (gpGlobals->time - LastAIPlayerCountUpdate < 0.2f) { return; }

	if (gpGlobals->time - AIStartedTime < AI_GRACE_PERIOD) { return; }

	// If game has ended, kick bots that have dropped back to the ready room
	if (GetGameRules()->GetVictoryTeam() != TEAM_IND)
	{
		AIMGR_RemoveBotsInReadyRoom();
		return;
	}

	LastAIPlayerCountUpdate = gpGlobals->time;

	// If bots are disabled, ensure we've removed all bots from the game
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
	const char* MapName = STRING(gpGlobals->mapname);

	AvHTeamNumber teamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber teamB = GetGameRules()->GetTeamBNumber();

	int TeamSizeA = GetGameRules()->GetTeamAPlayerCount();
	int TeamSizeB = GetGameRules()->GetTeamBPlayerCount();

	int NumDesiredTeamA = (avh_botusemapdefaults.value > 0) ? CONFIG_GetTeamASizeForMap(MapName) : (int)ceilf(avh_botminplayers.value * 0.5f);
	int NumDesiredTeamB = (avh_botusemapdefaults.value > 0) ? CONFIG_GetTeamBSizeForMap(MapName) : (int)floorf(avh_botminplayers.value * 0.5f);

	if ((NumDesiredTeamA + NumDesiredTeamB) > gpGlobals->maxClients)
	{
		int Delta = (NumDesiredTeamA + NumDesiredTeamB) - gpGlobals->maxClients;

		bool bRemoveB = true;
		int BotsRemoved = 0;

		while (BotsRemoved < Delta)
		{
			if (bRemoveB)
			{
				NumDesiredTeamB--;
			}
			else
			{
				NumDesiredTeamA--;
			}
			BotsRemoved++;
			bRemoveB = !bRemoveB;
		}
	}
	
	if (TeamSizeA < NumDesiredTeamA)
	{
		AIMGR_AddAIPlayerToTeam(1);
		return;
	}

	if (TeamSizeA > NumDesiredTeamA)
	{
		if (AIMGR_GetNumAIPlayersOnTeam(teamA) > 0)
		{
			AIMGR_RemoveAIPlayerFromTeam(1);
			return;
		}
	}

	if (TeamSizeB < NumDesiredTeamB)
	{
		AIMGR_AddAIPlayerToTeam(2);
		return;
	}

	if (TeamSizeB > NumDesiredTeamB)
	{
		if (AIMGR_GetNumAIPlayersOnTeam(teamB) > 0)
		{
			AIMGR_RemoveAIPlayerFromTeam(2);
			return;
		}
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

	// If game has ended, don't allow new bots to be added
	if (GetGameRules()->GetVictoryTeam() != TEAM_IND)
	{
		return;
	}


	for (int i = 0; i < gpGlobals->maxClients; i++)
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

	if (!NavmeshLoaded())
	{
		CONFIG_ParseConfigFile();

		const char* theCStrLevelName = STRING(gpGlobals->mapname);

		if (!loadNavigationData(theCStrLevelName))
		{
			return;
		}
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
	// Slap a [BOT] tag too so players know they're not human
	string NewName = CONFIG_GetBotPrefix() + BotNames[BotNameIndex];

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

		AvHAIPlayer* NewBotRef = &ActiveAIPlayers[NewBotIndex];

		const bot_skill BotSkillSettings = CONFIG_GetGlobalBotSkillLevel();

		memcpy(&NewBotRef->BotSkillSettings, &BotSkillSettings, sizeof(bot_skill));
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

	static float PrevTime = 0.0f;
	static float CurrTime = 0.0f;

	static float LastThinkTime = 0.0f;

	CurrTime = gpGlobals->time;

	if (CurrTime < PrevTime)
	{
		PrevTime = 0.0f;
	}

	if (CurrTime < LastThinkTime)
	{
		LastThinkTime = 0.0f;
	}

	float FrameDelta = CurrTime - PrevTime;
	float ThinkDelta = CurrTime - LastThinkTime;
		
	for (int bot_index = 0; bot_index < gpGlobals->maxClients; bot_index++)
	{
		if (!ActiveAIPlayers[bot_index].Player) { continue; } // Slot isn't filled

		AvHAIPlayer* bot = &ActiveAIPlayers[bot_index];

		BotUpdateViewRotation(bot, FrameDelta);

		if (IS_DEDICATED_SERVER() || ThinkDelta >= BOT_MIN_FRAME_TIME)
		{
			BotDeltaTime = ThinkDelta;

			if (ShouldBotThink(bot))
			{
				if (bot->bIsInactive)
				{
					BotResumePlay(bot);
				}

				StartNewBotFrame(bot);

				UpdateBotChat(bot);

				DroneThink(bot);

				AvHAIWeapon DesiredWeapon = (bot->DesiredMoveWeapon != WEAPON_NONE) ? bot->DesiredMoveWeapon : bot->DesiredCombatWeapon;

				if (DesiredWeapon != WEAPON_NONE && GetBotCurrentWeapon(bot) != DesiredWeapon)
				{
					BotSwitchToWeapon(bot, DesiredWeapon);
				}

				BotUpdateDesiredViewRotation(bot);
			}
			else
			{
				ClearBotInputs(bot);
				bot->bIsInactive = true;
			}

			// Needed to correctly handle client prediction and physics calculations
			byte adjustedmsec = BotThrottledMsec(bot);

			// save the command time
			bot->f_previous_command_time = gpGlobals->time;

			// Simulate PM_PlayerMove so client prediction and stuff can be executed correctly.
			RUN_AI_MOVE(bot->Edict, bot->Edict->v.v_angle, bot->ForwardMove,
				bot->SideMove, bot->UpMove, bot->Button, bot->Impulse, adjustedmsec);

			LastThinkTime = gpGlobals->time;
		}		
	}

	PrevTime = CurrTime;

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

void AIMGR_ResetRound()
{
	if (avh_botsenabled.value == 0) { return; } // Do nothing if we're not using bots

	// AI Players would be 0 if the round is being reset because a new game is starting. If the round is reset
	// from a console command, or tournament mode readying up etc, then bot logic is unaffected
	if (AIMGR_GetNumAIPlayers() == 0)
	{
		// This is used to track the 5-second "grace period" before adding bots to the game if fill teams is enabled
		AIStartedTime = gpGlobals->time;
	}

	UTIL_PopulateDoors();
	UTIL_PopulateWeldableObstacles();

	AITAC_ClearMapAIData();

	ALERT(at_console, "AI Manager Reset Round\n");
}

void AIMGR_ClearBotData()
{
	memset(&ActiveAIPlayers, 0, sizeof(ActiveAIPlayers));
}

void AIMGR_NewMap()
{
	if (avh_botsenabled.value == 0) { return; } // Do nothing if we're not using bots

	AIStartedTime = gpGlobals->time;
	ALERT(at_console, "AI Manager New Map\n");

	if (NavmeshLoaded())
	{
		UnloadNavigationData();
	}

	CONFIG_ParseConfigFile();

	const char* theCStrLevelName = STRING(gpGlobals->mapname);

	if (!loadNavigationData(theCStrLevelName))
	{
		return;
	}

	AIMGR_BotPrecache();
}

AvHAIPlayer* AIMGR_GetAICommander(AvHTeamNumber Team)
{
	AvHPlayer* ActiveCommander = GetGameRules()->GetTeam(Team)->GetCommanderPlayer();

	if (!ActiveCommander) { return nullptr; }

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (ActiveAIPlayers[i].Player == ActiveCommander)
		{
			return &ActiveAIPlayers[i];
		}
	}

	return nullptr;
}

AvHAIPlayer* AIMGR_FindPlayerOnTeamWaitingBuildLink(const AvHTeamNumber Team, const AvHAIDeployableStructureType NewStructure, const Vector BuildLocation)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (ActiveAIPlayers[i].Player != nullptr && ActiveAIPlayers[i].Player->GetTeam() == Team)
		{
			if (ActiveAIPlayers[i].PrimaryBotTask.bIsWaitingForBuildLink && ActiveAIPlayers[i].PrimaryBotTask.StructureType == NewStructure)
			{
				if (vDist2DSq(BuildLocation, ActiveAIPlayers[i].PrimaryBotTask.TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
				{
					return &ActiveAIPlayers[i];
				}
				
			}

			if (ActiveAIPlayers[i].SecondaryBotTask.bIsWaitingForBuildLink && ActiveAIPlayers[i].SecondaryBotTask.StructureType == NewStructure)
			{
				if (vDist2DSq(BuildLocation, ActiveAIPlayers[i].SecondaryBotTask.TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
				{
					return &ActiveAIPlayers[i];
				}
			}

			if (ActiveAIPlayers[i].WantsAndNeedsTask.bIsWaitingForBuildLink && ActiveAIPlayers[i].WantsAndNeedsTask.StructureType == NewStructure)
			{
				if (vDist2DSq(BuildLocation, ActiveAIPlayers[i].WantsAndNeedsTask.TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
				{
					return &ActiveAIPlayers[i];
				}
			}
		}
	}

	return nullptr;
}

AvHTeamNumber AIMGR_GetEnemyTeam(const AvHTeamNumber FriendlyTeam)
{
	AvHTeamNumber TeamANumber = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamBNumber = GetGameRules()->GetTeamBNumber();

	return (FriendlyTeam == TeamANumber) ? TeamBNumber : TeamANumber;
}

AvHAIPlayer* AIMGR_GetAIPlayerAtIndex(const int Index)
{
	if (Index < 0 || Index >= MAX_PLAYERS) { return nullptr; }

	return &ActiveAIPlayers[Index];
}

void AIMGR_UpdateAIMapData()
{
	UTIL_UpdateTileCache();
	AITAC_UpdateMapAIData();
}

void AIMGR_BotPrecache()
{
	m_spriteTexture = PRECACHE_MODEL("sprites/zbeam6.spr");
}
