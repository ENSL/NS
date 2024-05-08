#ifndef AVH_AI_PLAYER_MANAGER_H
#define AVH_AI_PLAYER_MANAGER_H

#include "AvHConstants.h"
#include "AvHAIPlayer.h"

// The rate at which the bot will call RunPlayerMove in, default is 100hz. WARNING: Increasing the rate past 100hz causes bots to move and turn slowly due to GoldSrc limits!
static const double BOT_SERVER_UPDATE_RATE = (1.0 / 100.0);
// The rate in hz (times per second) at which the bot will call AIPlayerThink, default is 10 times per second.
static const int BOT_THINK_RATE_HZ = 10;
// Once the first human player has joined the game, how long to wait before adding bots
static const float AI_GRACE_PERIOD = 5.0f;
// Max time to wait before spawning players if none connect (e.g. empty dedicated server)
static const float AI_MAX_START_TIMEOUT = 20.0f;

void AIMGR_BotPrecache();

// Called when the round restarts. Clears all tactical information but keeps navigation data.
void	AIMGR_ResetRound();
// Called when a new map is loaded. Clears all tactical information AND loads new navmesh.
void	AIMGR_NewMap();
// Called when the match begins (countdown finished). Populates initial tactical information.
void AIMGR_RoundStarted();

// Adds a new AI player to a team (0 = Auto-assign, 1 = Team A, 2 = Team B)
void	AIMGR_AddAIPlayerToTeam(int Team);
// Removed an AI player from the team (0 = Auto-select team, 1 = Team A, 2 = Team B)
void	AIMGR_RemoveAIPlayerFromTeam(int Team);
// Run AI player logic
void	AIMGR_UpdateAIPlayers();
// Kicks all bots in the ready room (used at round end when everyone is booted back to the ready room)
void	AIMGR_RemoveBotsInReadyRoom();


// Called every 0.2s to determine if bots need to be added/removed. Calls UpdateTeamBalance or UpdateFillTeams depending on auto-mode
void	AIMGR_UpdateAIPlayerCounts();
// Called by UpdateAIPlayerCounts. If auto-mode is for balance only, will add/remove bots needed to keep teams even
void	AIMGR_UpdateTeamBalance();
// Called by UpdateAIPlayerCounts. If auto-mode is fill teams, will add/remove bots needed to maintain minimum player counts and balance
void	AIMGR_UpdateFillTeams();

vector<AvHPlayer*> AIMGR_GetAllPlayersOnTeam(AvHTeamNumber Team);

// Convenient helper function to get total number of players (human and AI) on a team
int AIMGR_GetNumPlayersOnTeam(AvHTeamNumber Team);
// How many AI players are in the game (does NOT include third-party bots like RCBot/Whichbot)
int		AIMGR_GetNumAIPlayers();
// How many bot commanders we have (across both teams)
int AIMGR_GetNumAICommanders();
// Returns true if an AI player is on the requested team (does NOT include third-party bots like RCBot/Whichbot)
int		AIMGR_AIPlayerExistsOnTeam(AvHTeamNumber Team);

void AIMGR_RegenBotIni();

void	AIMGR_UpdateAIMapData();
bool AIMGR_ShouldStartPlayerBalancing();

AvHAICommanderMode AIMGR_GetCommanderMode();

void AIMGR_SetCommanderAllowedTime(AvHTeamNumber Team, float NewValue);
float AIMGR_GetCommanderAllowedTime(AvHTeamNumber Team);

#ifdef BOTDEBUG
Vector AIDEBUG_GetDebugVector1();
Vector AIDEBUG_GetDebugVector2();
void AIDEBUG_SetDebugVector1(const Vector NewVector);
void AIDEBUG_SetDebugVector2(const Vector NewVector);
void AIDEBUG_TestPathFind();
void AIDEBUG_TestFlightPathFind(Vector FromLoc, Vector ToLoc);
#endif

int AIMGR_GetNumAIPlayersOnTeam(AvHTeamNumber Team);
int AIMGR_GetNumHumanPlayersOnTeam(AvHTeamNumber Team);
int AIMGR_GetNumHumanPlayersOnServer();
int AIMGR_GetNumActiveHumanPlayers();

int AIMGR_GetNumAIPlayersWithRoleOnTeam(AvHTeamNumber Team, AvHAIBotRole Role, AvHAIPlayer* IgnoreAIPlayer);

int AIMGR_GetNumHumansOfClassOnTeam(AvHTeamNumber Team, AvHUser3 PlayerType);

bool AIMGR_IsNavmeshLoaded();
AvHAINavMeshStatus AIMGR_GetNavMeshStatus();

bool AIMGR_IsBotEnabled();

void AIMGR_LoadNavigationData();
void AIMGR_ReloadNavigationData();

AvHAIPlayer* AIMGR_GetAICommander(AvHTeamNumber Team);

AvHAIPlayer* AIMGR_GetBotRefFromPlayer(AvHPlayer* PlayerRef);
AvHAIPlayer* AIMGR_GetBotRefFromEdict(edict_t* PlayerEdict);

AvHTeamNumber AIMGR_GetEnemyTeam(const AvHTeamNumber FriendlyTeam);
AvHClassType AIMGR_GetEnemyTeamType(const AvHTeamNumber FriendlyTeam);
AvHClassType AIMGR_GetTeamType(const AvHTeamNumber Team);
AvHTeamNumber AIMGR_GetTeamANumber();
AvHTeamNumber AIMGR_GetTeamBNumber();

AvHTeam* AIMGR_GetTeamRef(const AvHTeamNumber Team);

// Returns all NS AI players. Does not include third-party bots
vector<AvHAIPlayer*> AIMGR_GetAllAIPlayers();
// Returns all NS AI players on the requested team. Does not include third-party bots
vector<AvHAIPlayer*> AIMGR_GetAIPlayersOnTeam(AvHTeamNumber Team);
// Returns all active players (i.e. not dead, commanding, spectating or in the ready room)
vector<AvHPlayer*> AIMGR_GetAllActivePlayers();

// Returns all players on a team which are not an internal NS bot. Will still include third party bots such as Whichbot and RCBot
vector<AvHPlayer*> AIMGR_GetNonAIPlayersOnTeam(AvHTeamNumber Team);

void AIMGR_ClearBotData();

AvHAIPlayer* AIMGR_GetDebugAIPlayer();
void AIMGR_SetDebugAIPlayer(edict_t* SpectatingPlayer, edict_t* AIPlayer);

void AIMGR_ReceiveCommanderRequest(AvHTeamNumber Team, edict_t* Requestor, const char* Request);

void AIMGR_ClientConnected(edict_t* NewClient);
void AIMGR_PlayerSpawned();

// Called when mp_botsenabled changes from 0 to 1
void AIMGR_OnBotEnabled();
// Called when mp_botsenabled changes from 1 to 0
void AIMGR_OnBotDisabled();

void AIMGR_UpdateAISystem();

bool AIMGR_HasMatchEnded();

bool AIMGR_IsMatchPracticallyOver();

void AIMGR_ProcessPendingSounds();

void AIMGR_SetFrameDelta(float NewValue);
float AIMGR_GetFrameDelta();

#endif