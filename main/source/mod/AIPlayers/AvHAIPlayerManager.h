#ifndef AVH_AI_PLAYER_MANAGER_H
#define AVH_AI_PLAYER_MANAGER_H

#include "../AvHConstants.h"

// Max rate bot can run its logic, default is 1/60th second. WARNING: Increasing the rate past 100hz causes bots to move and turn slowly due to GoldSrc limits!
static const double BOT_MIN_FRAME_TIME = (1.0 / 60.0);

void	AIMGR_AddAIPlayerToTeam(int Team);
void	AIMGR_RemoveAIPlayerFromTeam(int Team);
void	AIMGR_UpdateAIPlayers();
void	AIMGR_RemoveBotsInReadyRoom();
float	AIMGR_GetBotDeltaTime();

void	AIMGR_UpdateAIPlayerCounts();
void	AIMGR_UpdateTeamBalance();
void	AIMGR_UpdateFillTeams();

int		AIMGR_GetNumAIPlayers();
int		AIMGR_AIPlayerExistsOnTeam(AvHTeamNumber Team);

#endif