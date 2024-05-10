//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_gorge.cpp
// 
// Contains gorge-related functions. Needs refactoring into helper function file
//




#include "AvHAITactical.h"
#include "AvHAINavigation.h"
#include "AvHAITask.h"
#include "AvHAIMath.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIHelper.h"
#include "AvHAIConstants.h"
#include "AvHAIPlayerManager.h"
#include "AvHAIConfig.h"

#include "AvHGamerules.h"
#include "AvHServerUtil.h"
#include "AvHSharedUtil.h"
#include "AvHMarineEquipment.h"
#include "AvHTurret.h"

#include <float.h>

#include "DetourTileCacheBuilder.h"

#include <unordered_map>


vector<AvHAIResourceNode> ResourceNodes;
vector<AvHAIHiveDefinition> Hives;

float CommanderViewZHeight;

std::unordered_map<int, AvHAIBuildableStructure> TeamAStructureMap;

std::unordered_map<int, AvHAIBuildableStructure> TeamBStructureMap;

std::unordered_map<int, AvHAIDroppedItem> MarineDroppedItemMap;

float last_structure_refresh_time = 0.0f;
float last_item_refresh_time = 0.0f;

// Increments by 1 every time the structure list is refreshed. Used to detect if structures have been destroyed and no longer show up
unsigned int StructureRefreshFrame = 1;
// Increments by 1 every time the item list is refreshed. Used to detect if items have been removed from play and no longer show up
unsigned int ItemRefreshFrame = 0;

Vector TeamAStartingLocation = ZERO_VECTOR;
Vector TeamBStartingLocation = ZERO_VECTOR;

extern nav_mesh NavMeshes[MAX_NAV_MESHES]; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)
extern nav_profile BaseNavProfiles[MAX_NAV_PROFILES]; // Array of nav profiles

bool bNavMeshModified = false;
extern bool bTileCacheUpToDate;

edict_t* LastSeenLerkTeamA = nullptr; // Track who went lerk on team A last time. This ensures we don't get endless cycles of lerks
edict_t* LastSeenLerkTeamB = nullptr; // Track who went lerk on team B last time. This ensures we don't get endless cycles of lerks

float LastSeenLerkTeamATime = 0.0f;
float LastSeenLerkTeamBTime = 0.0f;

vector<AvHAISquad> ActiveSquads;

std::vector<AvHAIBuildableStructure> AITAC_FindAllDeployables(const Vector& Location, const DeployableSearchFilter* Filter)
{
	std::vector<AvHAIBuildableStructure> Result;

	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();
	
	float CurrMinDist = 0.0f;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE)
			{
				unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

				if (Filter->ReachabilityTeam != TEAM_IND)
				{
					StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
				}

				if (!(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }
			}

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
				{
					Result.push_back(it.second);
				}
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE)
			{
				unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

				if (Filter->ReachabilityTeam != TEAM_IND)
				{
					StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
				}

				if (!(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }
			}

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
				{
					Result.push_back(it.second);
				}
			}
		}
	}

	return Result;
}

std::vector<AvHAIBuildableStructure*> AITAC_FindAllDeployablesByRef(const Vector& Location, const DeployableSearchFilter* Filter)
{
	std::vector<AvHAIBuildableStructure*> Result;

	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	float CurrMinDist = 0.0f;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE)
			{
				unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

				if (Filter->ReachabilityTeam != TEAM_IND)
				{
					StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
				}

				if (!(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }
			}

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
				{
					Result.push_back(&it.second);
				}
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE)
			{
				unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

				if (Filter->ReachabilityTeam != TEAM_IND)
				{
					StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
				}

				if (!(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }
			}

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
				{
					Result.push_back(&it.second);
				}
			}
		}
	}

	return Result;
}

bool AITAC_DeployableExistsAtLocation(const Vector& Location, const DeployableSearchFilter* Filter)
{
	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
				{
					return true;
				}
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
				{
					return true;
				}
			}
		}
	}

	return false;
}

AvHAIBuildableStructure	AITAC_FindClosestDeployableToLocation(const Vector& Location, const DeployableSearchFilter* Filter)
{
	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	AvHAIBuildableStructure Result;
	float CurrMinDist = 0.0f;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (!(it.second.StructureType & Filter->DeployableTypes)) { continue; }
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

			if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (FNullEnt(Result.edict) || DistSq < CurrMinDist))
			{
				Result = it.second;
				CurrMinDist = DistSq;
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (!(it.second.StructureType & Filter->DeployableTypes)) { continue; }
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

			if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (FNullEnt(Result.edict) || DistSq < CurrMinDist))
			{
				Result = it.second;
				CurrMinDist = DistSq;
			}
		}
	}

	return Result;
}

AvHAIBuildableStructure* AITAC_FindClosestDeployableToLocationByRef(const Vector& Location, const DeployableSearchFilter* Filter)
{
	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	AvHAIBuildableStructure* Result = NULL;
	float CurrMinDist = 0.0f;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (!(it.second.StructureType & Filter->DeployableTypes)) { continue; }
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }	
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

			if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (!Result || DistSq < CurrMinDist))
			{
				Result = &it.second;
				CurrMinDist = DistSq;
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (!(it.second.StructureType & Filter->DeployableTypes)) { continue; }
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }
			
			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

			if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (!Result || DistSq < CurrMinDist))
			{
				Result = &it.second;
				CurrMinDist = DistSq;
			}
		}
	}

	return Result;
}

AvHAIBuildableStructure AITAC_FindFurthestDeployableFromLocation(const Vector& Location, const DeployableSearchFilter* Filter)
{
	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	AvHAIBuildableStructure Result;
	float CurrMinDist = 0.0f;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (!(it.second.StructureType & Filter->DeployableTypes)) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

			if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && DistSq > CurrMinDist)
			{
				Result = it.second;
				CurrMinDist = DistSq;
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (!(it.second.StructureType & Filter->DeployableTypes)) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

			if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && DistSq > CurrMinDist)
			{
				Result = it.second;
				CurrMinDist = DistSq;
			}
		}
	}

	return Result;
}

AvHAIBuildableStructure* AITAC_FindFurthestDeployableFromLocationByRef(const Vector& Location, const DeployableSearchFilter* Filter)
{
	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	AvHAIBuildableStructure* Result = nullptr;
	float CurrMinDist = 0.0f;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (!(it.second.StructureType & Filter->DeployableTypes)) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

			if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && DistSq > CurrMinDist)
			{
				Result = &it.second;
				CurrMinDist = DistSq;
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (!(it.second.StructureType & Filter->DeployableTypes)) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

			if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && DistSq > CurrMinDist)
			{
				Result = &it.second;
				CurrMinDist = DistSq;
			}
		}
	}

	return Result;
}

AvHAIDroppedItem* AITAC_GetDroppedItemRefFromEdict(edict_t* ItemEdict)
{
	if (FNullEnt(ItemEdict)) { return nullptr; }

	int EntIndex = ENTINDEX(ItemEdict);

	if (EntIndex < 0) { return nullptr; }

	return &MarineDroppedItemMap[EntIndex];
}

AvHAIDroppedItem* AITAC_FindClosestItemToLocation(const Vector& Location, const AvHAIDeployableItemType ItemType, AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags, float MinRadius, float MaxRadius, bool bConsiderPhaseDistance)
{
	AvHAIDroppedItem* Result = NULL;
	float CurrMinDist = 0.0f;

	float MinDistSq = sqrf(MinRadius);
	float MaxDistSq = sqrf(MaxRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	for (auto& it : MarineDroppedItemMap)
	{
		if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

		unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

		if (SearchingTeam != TEAM_IND)
		{
			StructureReachabilityFlags = (SearchingTeam == GetGameRules()->GetTeamANumber()) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
		}

		if (ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & ReachabilityFlags)) { continue; }

		if (it.second.ItemType != ItemType) { continue; }

		float DistSq = (bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

		if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (!Result || DistSq < CurrMinDist))
		{
			Result = &it.second;
			CurrMinDist = DistSq;
		}

	}

	return Result;
}

int	AITAC_GetNumItemsInLocation(const Vector& Location, const AvHAIDeployableItemType ItemType, AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags, float MinRadius, float MaxRadius, bool bConsiderPhaseDistance)
{
	int Result = 0;

	float MinDistSq = sqrf(MinRadius);
	float MaxDistSq = sqrf(MaxRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	for (auto& it : MarineDroppedItemMap)
	{
		if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

		if (it.second.ItemType != ItemType) { continue; }

		unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

		if (SearchingTeam != TEAM_IND)
		{
			StructureReachabilityFlags = (SearchingTeam == GetGameRules()->GetTeamANumber()) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
		}

		if (ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & ReachabilityFlags)) { continue; }

		float DistSq = (bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

		if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
		{
			Result++;
		}

	}

	return Result;
}

bool AITAC_ItemExistsInLocation(const Vector& Location, const AvHAIDeployableItemType ItemType, AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags, float MinRadius, float MaxRadius, bool bConsiderPhaseDistance)
{

	float MinDistSq = sqrf(MinRadius);
	float MaxDistSq = sqrf(MaxRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	for (auto& it : MarineDroppedItemMap)
	{
		if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

		unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

		if (SearchingTeam != TEAM_IND)
		{
			StructureReachabilityFlags = (SearchingTeam == GetGameRules()->GetTeamANumber()) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
		}

		if (ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & ReachabilityFlags)) { continue; }

		if (it.second.ItemType != ItemType) { continue; }

		float DistSq = (bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

		if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
		{
			return true;
		}

	}

	return false;
}

AvHAIBuildableStructure AITAC_GetDeployableFromEdict(const edict_t* Structure)
{
	AvHAIBuildableStructure Result;

	if (FNullEnt(Structure)) { return Result; }

	int EntIndex = ENTINDEX(Structure);

	if (EntIndex < 0) { return Result; }

	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	Result = (Structure->v.team == TeamA) ? TeamAStructureMap[EntIndex] : TeamBStructureMap[EntIndex];

	return Result;

}

AvHAIBuildableStructure* AITAC_GetDeployableRefFromEdict(const edict_t* Structure)
{
	if (FNullEnt(Structure)) { return nullptr; }

	int EntIndex = ENTINDEX(Structure);

	if (EntIndex < 0) { return nullptr; }

	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	return (Structure->v.team == TeamA) ? &TeamAStructureMap[EntIndex] : &TeamBStructureMap[EntIndex];

}

AvHAIBuildableStructure AITAC_GetNearestDeployableDirectlyReachable(AvHAIPlayer* pBot, const Vector Location, const DeployableSearchFilter* Filter)
{
	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	AvHAIBuildableStructure Result;
	float CurrMinDist = 0.0f;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				if (!UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, it.second.Location)) { continue; }

				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (!Result.IsValid() || DistSq < CurrMinDist))
				{
					Result = it.second;
					CurrMinDist = DistSq;
				}
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				if (!UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, it.second.Location)) { continue; }

				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (!Result.IsValid() || DistSq < CurrMinDist))
				{
					Result = it.second;
					CurrMinDist = DistSq;
				}
			}
		}
	}

	return Result;
}

AvHAIBuildableStructure* AITAC_GetNearestDeployableDirectlyReachableByRef(AvHAIPlayer* pBot, const Vector Location, const DeployableSearchFilter* Filter)
{
	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	AvHAIBuildableStructure* Result = nullptr;
	float CurrMinDist = 0.0f;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				if (!UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, it.second.Location)) { continue; }

				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (!Result || DistSq < CurrMinDist))
				{
					Result = &it.second;
					CurrMinDist = DistSq;
				}
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				if (!UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, it.second.Location)) { continue; }

				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (!Result || DistSq < CurrMinDist))
				{
					Result = &it.second;
					CurrMinDist = DistSq;
				}
			}
		}
	}

	return Result;
}

int AITAC_GetNumDeployablesNearLocation(const Vector& Location, const DeployableSearchFilter* Filter)
{
	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	int Result = 0;

	if (Filter->DeployableTeam == TeamA || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamAStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
				{
					Result++;
				}
			}
		}
	}

	if (Filter->DeployableTeam == TeamB || Filter->DeployableTeam == TEAM_IND)
	{
		for (auto& it : TeamBStructureMap)
		{
			if (FNullEnt(it.second.edict) || it.second.edict->v.deadflag != DEAD_NO || (it.second.edict->v.effects & EF_NODRAW)) { continue; }

			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }
			if (it.second.Purpose != Filter->PurposeFlags && (it.second.Purpose & Filter->PurposeFlags) != it.second.Purpose) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
				float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it.second.Location, Location)) : vDist2DSq(it.second.Location, Location);

				if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
				{
					Result++;
				}
			}
		}
	}

	return Result;
}

Vector AITAC_GetFloorLocationForHive(const AvHAIHiveDefinition* Hive)
{
	if (!Hive) { return ZERO_VECTOR; }

	Vector HiveFloorLoc = UTIL_GetFloorUnderEntity(Hive->HiveEdict);

	Vector NearestNavigableLoc = ZERO_VECTOR;

	nav_profile TestNavProfile = GetBaseNavProfile(MARINE_BASE_NAV_PROFILE);
	TestNavProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);
	TestNavProfile.ReachabilityFlag = AI_REACHABILITY_WELDER;

	FOR_ALL_ENTITIES(kwsTeamCommand, AvHCommandStation*)
		if (vIsZero(NearestNavigableLoc))
		{
			NearestNavigableLoc = FindClosestNavigablePointToDestination(TestNavProfile, theEntity->pev->origin, HiveFloorLoc, UTIL_MetresToGoldSrcUnits(10.0f));
		}
	END_FOR_ALL_ENTITIES(kwsTeamCommand);

	if (!vIsZero(NearestNavigableLoc))
	{
		Vector ProjectedPoint = UTIL_ProjectPointToNavmesh(NearestNavigableLoc, Vector(500.0f, 500.0f, 500.0f), GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

		if (!vIsZero(ProjectedPoint)) { return ProjectedPoint; }

		return NearestNavigableLoc;
	}
	else
	{
		Vector ProjectedPoint = UTIL_ProjectPointToNavmesh(HiveFloorLoc, Vector(500.0f, 500.0f, 500.0f), GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

		if (!vIsZero(ProjectedPoint)) { return ProjectedPoint; }

		return HiveFloorLoc;
	}

}

void AITAC_PopulateHiveData()
{
	Hives.clear();

	const AvHBaseInfoLocationListType& theInfoLocations = GetGameRules()->GetInfoLocations();

	FOR_ALL_ENTITIES(kesTeamHive, AvHHive*)

		AvHAIHiveDefinition NewHive;
		NewHive.HiveEntity = theEntity;
		NewHive.HiveEdict = theEntity->edict();
		NewHive.Location = theEntity->pev->origin;
		memset(&NewHive.ObstacleRefs, 0, sizeof(NewHive.ObstacleRefs));

		AvHAIResourceNode* NearestNode = AITAC_GetNearestResourceNodeToLocation(theEntity->pev->origin);

		if (NearestNode)
		{
			NewHive.HiveResNodeRef = NearestNode;
			NearestNode->ParentHive = NewHive.HiveEdict;
		}

		NewHive.FloorLocation = UTIL_GetFloorUnderEntity(NewHive.HiveEdict); // Some hives are suspended in the air, this is the floor location directly beneath it

		string HiveName;

		string theLocationName;
		if (AvHSHUGetNameOfLocation(GetGameRules()->GetInfoLocations(), NewHive.Location, theLocationName))
		{
			UTIL_LocalizeText(theLocationName.c_str(), theLocationName);
			HiveName = theLocationName;
		}

		sprintf(NewHive.HiveName, HiveName.c_str(), "%s");


		Hives.push_back(NewHive);

	END_FOR_ALL_ENTITIES(kesTeamHive)
}

void AITAC_RefreshHiveData()
{
	if (Hives.size() == 0)
	{
		AITAC_PopulateHiveData();
	}

	int NextRefresh = 0;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		AvHHive* theEntity = it->HiveEntity;

		it->TechStatus = theEntity->GetTechnology();
		it->bIsUnderAttack = GetGameRules()->GetIsEntityUnderAttack(theEntity->entindex());

		AvHTeamNumber CurrentOwningTeam = theEntity->GetTeamNumber();
		HiveStatusType CurrentStatus = (theEntity->GetIsActive() ? HIVE_STATUS_BUILT : (theEntity->GetIsSpawning() ? HIVE_STATUS_BUILDING : HIVE_STATUS_UNBUILT));

		if (CurrentStatus == HIVE_STATUS_BUILT)
		{
			it->HealthPercent = (it->HiveEdict->v.health / it->HiveEdict->v.max_health);
		}
		else
		{
			it->HealthPercent = 1.0f;
		}

		bool bHiveDestroyed = (CurrentOwningTeam != it->OwningTeam) || (it->Status == HIVE_STATUS_BUILT && CurrentStatus != it->Status);

		if (bHiveDestroyed)
		{
			if (it->OwningTeam == GetGameRules()->GetTeamANumber())
			{
				TeamAStartingLocation = ZERO_VECTOR;
			}
			else
			{
				TeamBStartingLocation = ZERO_VECTOR;
			}

			AITAC_GetTeamStartingLocation(it->OwningTeam); // Force refresh
		}

		it->OwningTeam = CurrentOwningTeam;
		it->Status = CurrentStatus;

		if (it->HiveResNodeRef)
		{
			it->HiveResNodeRef->bIsBaseNode = (it->Status != HIVE_STATUS_UNBUILT);
		}

		if (it->Status != HIVE_STATUS_UNBUILT && it->ObstacleRefs[REGULAR_NAV_MESH] == 0)
		{
			UTIL_AddTemporaryObstacles(UTIL_GetCentreOfEntity(it->HiveEdict) - Vector(0.0f, 0.0f, 25.0f), 125.0f, 300.0f, DT_AREA_NULL, it->ObstacleRefs);
			it->NextFloorLocationCheck = gpGlobals->time + 1.0f;
		}
		else if (it->Status == HIVE_STATUS_UNBUILT && it->ObstacleRefs[REGULAR_NAV_MESH] != 0)
		{
			UTIL_RemoveTemporaryObstacles(it->ObstacleRefs);
			it->NextFloorLocationCheck = gpGlobals->time + 1.0f;
		}

		if (gpGlobals->time >= it->NextFloorLocationCheck)
		{
			it->FloorLocation = AITAC_GetFloorLocationForHive(&(*it));

			it->NextFloorLocationCheck = gpGlobals->time + (5.0f + (0.1f * NextRefresh));

			AITAC_RefreshReachabilityForHive(&(*it));
		}

		NextRefresh++;
	}
}

Vector AITAC_GetTeamStartingLocation(AvHTeamNumber Team)
{
	if (vIsZero(TeamAStartingLocation) || vIsZero(TeamBStartingLocation))
	{
		AvHTeamNumber TeamANum = GetGameRules()->GetTeamANumber();
		AvHTeamNumber TeamBNum = GetGameRules()->GetTeamBNumber();

		AvHTeam* AvHTeamARef = GetGameRules()->GetTeamA();
		AvHTeam* AvHTeamBRef = GetGameRules()->GetTeamB();

		if (AvHTeamARef)
		{
			Vector TeamStartLocation = AvHTeamARef->GetStartingLocation();

			if (AvHTeamARef->GetTeamType() == AVH_CLASS_TYPE_MARINE)
			{
				DeployableSearchFilter IFFilter;
				IFFilter.DeployableTeam = TeamANum;
				IFFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
				IFFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
				IFFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
				
				AvHAIBuildableStructure InfantryPortal = AITAC_FindClosestDeployableToLocation(ZERO_VECTOR, &IFFilter);

				if (InfantryPortal.IsValid())
				{
					TeamAStartingLocation = InfantryPortal.Location;
				}
				else
				{
					Vector CommChairLocation = AITAC_GetCommChairLocation(TeamANum);
					TeamAStartingLocation = (!vIsZero(CommChairLocation)) ? CommChairLocation : TeamStartLocation;
					TeamAStartingLocation = UTIL_ProjectPointToNavmesh(TeamAStartingLocation, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));
				}
			}
			else
			{
				TeamAStartingLocation = TeamStartLocation;

				const AvHAIHiveDefinition* Hive = AITAC_GetActiveHiveNearestLocation(TeamANum, TeamStartLocation);

				if (Hive)
				{
					TeamAStartingLocation = AITAC_GetFloorLocationForHive(Hive);
				}
				else
				{
					TeamBStartingLocation = ZERO_VECTOR;
				}
			}
		}

		if (AvHTeamBRef)
		{
			Vector TeamStartLocation = AvHTeamBRef->GetStartingLocation();

			if (AvHTeamBRef->GetTeamType() == AVH_CLASS_TYPE_MARINE)
			{
				DeployableSearchFilter IFFilter;
				IFFilter.DeployableTeam = TeamBNum;
				IFFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
				IFFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
				IFFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

				AvHAIBuildableStructure InfantryPortal = AITAC_FindClosestDeployableToLocation(ZERO_VECTOR, &IFFilter);

				if (InfantryPortal.IsValid())
				{
					TeamBStartingLocation = InfantryPortal.Location;
				}
				else
				{
					Vector CommChairLocation = AITAC_GetCommChairLocation(TeamBNum);
					TeamBStartingLocation = (!vIsZero(CommChairLocation)) ? CommChairLocation : TeamStartLocation;
					TeamBStartingLocation = UTIL_ProjectPointToNavmesh(TeamBStartingLocation, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));
				}
			}
			else
			{
				TeamBStartingLocation = TeamStartLocation;

				const AvHAIHiveDefinition* Hive = AITAC_GetActiveHiveNearestLocation(TeamBNum, TeamStartLocation);

				if (Hive)
				{
					TeamBStartingLocation = AITAC_GetFloorLocationForHive(Hive);
				}
				else
				{
					TeamBStartingLocation = ZERO_VECTOR;
				}
			}
		}

		// Update reachabilities since team starting points have been modified
		bNavMeshModified = true;

		AITAC_OnTeamStartsModified();
	}

	return (Team == GetGameRules()->GetTeamANumber()) ? TeamAStartingLocation : TeamBStartingLocation;
}

void AITAC_OnTeamStartsModified()
{
	AvHTeamNumber TeamANum = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamBNum = GetGameRules()->GetTeamBNumber();

	bool bTeamAIsMarine = (AIMGR_GetTeamType(TeamANum) == AVH_CLASS_TYPE_MARINE);
	bool bTeamBIsMarine = (AIMGR_GetTeamType(TeamBNum) == AVH_CLASS_TYPE_MARINE);

	if (!bTeamAIsMarine && !bTeamBIsMarine) { return; }
		
	AvHAIResourceNode* TeamAMarineNode = nullptr;
	AvHAIResourceNode* TeamBMarineNode = nullptr;

	if (bTeamAIsMarine)
	{
		TeamAMarineNode = AITAC_GetNearestResourceNodeToLocation(TeamAStartingLocation);
	}

	if (bTeamBIsMarine)
	{
		TeamBMarineNode = AITAC_GetNearestResourceNodeToLocation(TeamBStartingLocation);
	}

	vector<AvHAIResourceNode*> AllNodes = AITAC_GetAllResourceNodes();

	for (auto it = AllNodes.begin(); it != AllNodes.end(); it++)
	{
		AvHAIResourceNode* ThisNode = (*it);

		if (!ThisNode) { continue; }

		ThisNode->bIsBaseNode = (!ThisNode->ParentHive) && (ThisNode == TeamAMarineNode || ThisNode == TeamBMarineNode);
	}
}

Vector AITAC_GetCommChairLocation(AvHTeamNumber Team)
{
	if (Team != TEAM_IND)
	{
		AvHTeam* TeamRef = GetGameRules()->GetTeam(Team);

		if (TeamRef->GetTeamType() != AVH_CLASS_TYPE_MARINE)
		{
			return ZERO_VECTOR;
		}
	}

	edict_t* Chair = AITAC_GetCommChair(Team);

	if (!FNullEnt(Chair))
	{
		return Chair->v.origin;
	}

	return ZERO_VECTOR;
}

void AITAC_RefreshReachabilityForItem(AvHAIDroppedItem* Item)
{
	if (FNullEnt(Item->edict) || Item->edict->v.deadflag != DEAD_NO || (Item->edict->v.effects & EF_NODRAW)) { return; }

	if (Item->ItemType == DEPLOYABLE_ITEM_SCAN)
	{
		Item->TeamAReachabilityFlags = AI_REACHABILITY_ALL;
		Item->TeamBReachabilityFlags = AI_REACHABILITY_ALL;
		return;
	}

	Item->TeamAReachabilityFlags = AI_REACHABILITY_NONE;
	Item->TeamBReachabilityFlags = AI_REACHABILITY_NONE;

	bool bOnNavMesh = UTIL_PointIsOnNavmesh(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], Item->edict->v.origin, Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));

	if (!bOnNavMesh)
	{
		Item->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		Item->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		return;
	}

	if (GetGameRules()->GetTeamA()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), Item->edict->v.origin, max_player_use_reach);

		if (bIsReachableMarine)
		{
			Item->TeamAReachabilityFlags |= AI_REACHABILITY_MARINE;
			Item->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), Item->edict->v.origin, max_player_use_reach);

			if (bIsReachableWelder)
			{
				Item->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
			else
			{
				Item->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
			}
		}
	}

	if (GetGameRules()->GetTeamB()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), Item->edict->v.origin, max_player_use_reach);

		if (bIsReachableMarine)
		{
			Item->TeamBReachabilityFlags |= AI_REACHABILITY_MARINE;
			Item->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), Item->edict->v.origin, max_player_use_reach);

			if (bIsReachableWelder)
			{
				Item->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
			else
			{
				Item->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
			}
		}
	}
}

void AITAC_RefreshAllResNodeReachability()
{
	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		AITAC_RefreshReachabilityForResNode(&(*it));
	}
}

void AITAC_RefreshReachabilityForHive(AvHAIHiveDefinition* Hive)
{

	if (!bTileCacheUpToDate) { return; }

	Hive->TeamAReachabilityFlags = AI_REACHABILITY_NONE;
	Hive->TeamBReachabilityFlags = AI_REACHABILITY_NONE;

	Vector HiveLocation = Hive->FloorLocation;

	bool bOnNavMesh = UTIL_PointIsOnNavmesh(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), HiveLocation, Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));

	if (!bOnNavMesh)
	{
		Hive->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		Hive->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		return;
	}

	Vector TeamAStart = AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber());
	Vector TeamBStart = AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber());

	if (GetGameRules()->GetTeamA()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), TeamAStart, HiveLocation, max_player_use_reach);

		if (bIsReachableMarine)
		{
			Hive->TeamAReachabilityFlags |= AI_REACHABILITY_MARINE;
			Hive->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, TeamAStart, HiveLocation, max_player_use_reach);

			if (bIsReachableWelder)
			{
				Hive->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
			else
			{
				Hive->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
			}
		}
	}
	else
	{
		bool bIsReachableSkulk = UTIL_PointIsReachable(GetBaseNavProfile(SKULK_BASE_NAV_PROFILE), TeamAStart, HiveLocation, max_player_use_reach);
		bool bIsReachableGorge = UTIL_PointIsReachable(GetBaseNavProfile(GORGE_BASE_NAV_PROFILE), TeamAStart, HiveLocation, max_player_use_reach);
		bool bIsReachableOnos = UTIL_PointIsReachable(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), TeamAStart, HiveLocation, max_player_use_reach);

		if (bIsReachableSkulk)
		{
			Hive->TeamAReachabilityFlags |= AI_REACHABILITY_SKULK;
		}

		if (bIsReachableGorge)
		{
			Hive->TeamAReachabilityFlags |= AI_REACHABILITY_GORGE;
		}

		if (bIsReachableOnos)
		{
			Hive->TeamAReachabilityFlags |= AI_REACHABILITY_ONOS;
		}

		if (Hive->TeamAReachabilityFlags == AI_REACHABILITY_NONE)
		{
			Hive->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		}
	}

	if (GetGameRules()->GetTeamB()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), TeamBStart, HiveLocation, max_player_use_reach);

		if (bIsReachableMarine)
		{
			Hive->TeamBReachabilityFlags |= AI_REACHABILITY_MARINE;
			Hive->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, TeamBStart, HiveLocation, max_player_use_reach);

			if (bIsReachableWelder)
			{
				Hive->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
			else
			{
				Hive->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
			}
		}
	}
	else
	{
		bool bIsReachableSkulk = UTIL_PointIsReachable(GetBaseNavProfile(SKULK_BASE_NAV_PROFILE), TeamBStart, HiveLocation, max_player_use_reach);
		bool bIsReachableGorge = UTIL_PointIsReachable(GetBaseNavProfile(GORGE_BASE_NAV_PROFILE), TeamBStart, HiveLocation, max_player_use_reach);
		bool bIsReachableOnos = UTIL_PointIsReachable(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), TeamBStart, HiveLocation, max_player_use_reach);

		if (bIsReachableSkulk)
		{
			Hive->TeamBReachabilityFlags |= AI_REACHABILITY_SKULK;
		}

		if (bIsReachableGorge)
		{
			Hive->TeamBReachabilityFlags |= AI_REACHABILITY_GORGE;
		}

		if (bIsReachableOnos)
		{
			Hive->TeamBReachabilityFlags |= AI_REACHABILITY_ONOS;
		}

		if (Hive->TeamBReachabilityFlags == AI_REACHABILITY_NONE)
		{
			Hive->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		}
	}
}

void AITAC_RefreshReachabilityForResNode(AvHAIResourceNode* ResNode)
{
	if (Hives.size() == 0)
	{
		AITAC_RefreshHiveData();
	}

	if (!bTileCacheUpToDate) { return; }

	ResNode->bReachabilityMarkedDirty = false;
	ResNode->NextReachabilityRefreshTime = 0.0f;

	ResNode->TeamAReachabilityFlags = AI_REACHABILITY_NONE;
	ResNode->TeamBReachabilityFlags = AI_REACHABILITY_NONE;

	Vector ResNodeLocation = ResNode->Location;

	bool bOnNavMesh = UTIL_PointIsOnNavmesh(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), ResNodeLocation, Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));

	if (!bOnNavMesh)
	{
		ResNode->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		ResNode->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		return;
	}

	Vector TeamAStart = AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber());
	Vector TeamBStart = AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber());

	if (GetGameRules()->GetTeamA()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), TeamAStart, ResNodeLocation, 4.0f);

		if (bIsReachableMarine)
		{
			ResNode->TeamAReachabilityFlags |= AI_REACHABILITY_MARINE;
			ResNode->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, TeamAStart, ResNodeLocation, 4.0f);

			if (bIsReachableWelder)
			{
				ResNode->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
			else
			{
				ResNode->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
			}
		}
	}
	else
	{
		bool bIsReachableSkulk = UTIL_PointIsReachable(GetBaseNavProfile(SKULK_BASE_NAV_PROFILE), TeamAStart, ResNodeLocation, 4.0f);
		bool bIsReachableGorge = UTIL_PointIsReachable(GetBaseNavProfile(GORGE_BASE_NAV_PROFILE), TeamAStart, ResNodeLocation, 4.0f);
		bool bIsReachableOnos = UTIL_PointIsReachable(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), TeamAStart, ResNodeLocation, 4.0f);

		if (bIsReachableSkulk)
		{
			ResNode->TeamAReachabilityFlags |= AI_REACHABILITY_SKULK;
		}

		if (bIsReachableGorge)
		{
			ResNode->TeamAReachabilityFlags |= AI_REACHABILITY_GORGE;
		}

		if (bIsReachableOnos)
		{
			ResNode->TeamAReachabilityFlags |= AI_REACHABILITY_ONOS;
		}

		if (ResNode->TeamAReachabilityFlags == AI_REACHABILITY_NONE)
		{
			ResNode->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		}
	}

	if (GetGameRules()->GetTeamB()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), TeamBStart, ResNodeLocation, 4.0f);

		if (bIsReachableMarine)
		{
			ResNode->TeamBReachabilityFlags |= AI_REACHABILITY_MARINE;
			ResNode->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, TeamBStart, ResNodeLocation, 4.0f);

			if (bIsReachableWelder)
			{
				ResNode->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
			else
			{
				ResNode->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
			}
		}
	}
	else
	{
		bool bIsReachableSkulk = UTIL_PointIsReachable(GetBaseNavProfile(SKULK_BASE_NAV_PROFILE), TeamBStart, ResNodeLocation, 4.0f);
		bool bIsReachableGorge = UTIL_PointIsReachable(GetBaseNavProfile(GORGE_BASE_NAV_PROFILE), TeamBStart, ResNodeLocation, 4.0f);
		bool bIsReachableOnos = UTIL_PointIsReachable(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), TeamBStart, ResNodeLocation, 4.0f);

		if (bIsReachableSkulk)
		{
			ResNode->TeamBReachabilityFlags |= AI_REACHABILITY_SKULK;
		}

		if (bIsReachableGorge)
		{
			ResNode->TeamBReachabilityFlags |= AI_REACHABILITY_GORGE;
		}

		if (bIsReachableOnos)
		{
			ResNode->TeamBReachabilityFlags |= AI_REACHABILITY_ONOS;
		}

		if (ResNode->TeamBReachabilityFlags == AI_REACHABILITY_NONE)
		{
			ResNode->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		}
	}
}

void AITAC_PopulateResourceNodes()
{
	ResourceNodes.clear();

	FOR_ALL_ENTITIES(kesFuncResource, AvHFuncResource*)

		AvHAIResourceNode NewResNode;
		NewResNode.ResourceEntity = theEntity;
		NewResNode.ResourceEdict = theEntity->edict();
		NewResNode.Location = theEntity->pev->origin;
		NewResNode.TeamAReachabilityFlags = AI_REACHABILITY_NONE;
		NewResNode.TeamBReachabilityFlags = AI_REACHABILITY_NONE;
		NewResNode.bReachabilityMarkedDirty = true;
		NewResNode.NextReachabilityRefreshTime = 0.0f;

		ResourceNodes.push_back(NewResNode);

	END_FOR_ALL_ENTITIES(kesFuncResource)
}

void AITAC_RefreshResourceNodes()
{
	if (ResourceNodes.size() == 0)
	{
		AITAC_PopulateResourceNodes();
	}

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		AvHFuncResource* ResourceEntity = it->ResourceEntity;

		it->bIsOccupied = ResourceEntity->GetIsOccupied();
		
		if (it->bIsOccupied)
		{
			DeployableSearchFilter TowerFilter;
			TowerFilter.DeployableTypes = (STRUCTURE_MARINE_RESTOWER | STRUCTURE_ALIEN_RESTOWER);

			AvHAIBuildableStructure OccupyingTower = AITAC_FindClosestDeployableToLocation(it->Location, &TowerFilter);

			if (OccupyingTower.IsValid())
			{
				it->ActiveTowerEntity = OccupyingTower.edict;
				it->OwningTeam = OccupyingTower.EntityRef->GetTeamNumber();
			}
		}
		else
		{
			it->ActiveTowerEntity = nullptr;
			it->OwningTeam = TEAM_IND;
		}

		if (it->bReachabilityMarkedDirty)
		{

			if (it->NextReachabilityRefreshTime == 0.0f)
			{
				it->NextReachabilityRefreshTime = gpGlobals->time + frandrange(0.5f, 1.5f);
			}
			else
			{
				if (gpGlobals->time > it->NextReachabilityRefreshTime)
				{
					AITAC_RefreshReachabilityForResNode(&(*it));
				}
			}			
		}
	}
}

AvHAIResourceNode* AITAC_GetRandomResourceNode(AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags)
{
	AvHAIResourceNode* Result = nullptr;
	float MaxScore = 0.0f;

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		if (ReachabilityFlags != AI_REACHABILITY_NONE)
		{
			unsigned int StructureReachabilityFlags = (it->TeamAReachabilityFlags | it->TeamBReachabilityFlags);

			if (SearchingTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (SearchingTeam == GetGameRules()->GetTeamANumber()) ? it->TeamAReachabilityFlags : it->TeamBReachabilityFlags;
			}

			if (!(StructureReachabilityFlags & ReachabilityFlags)) { continue; }
		} 

		float ThisScore = frandrange(0.0f, 1.0f);

		if (!Result || ThisScore > MaxScore)
		{
			Result = &(*it);
			MaxScore = ThisScore;
		}
	}

	return Result;
}

void AITAC_UpdateMapAIData()
{

	AITAC_RefreshHiveData();

	UTIL_UpdateDoors(false);
	UTIL_UpdateWeldableObstacles();
	

	if (gpGlobals->time - last_structure_refresh_time >= structure_inventory_refresh_rate)
	{
		AITAC_RefreshBuildableStructures();
		AITAC_RefreshResourceNodes();
		last_structure_refresh_time = gpGlobals->time;
	}

	if (gpGlobals->time - last_item_refresh_time >= item_inventory_refresh_rate)
	{
		AITAC_RefreshMarineItems();
		last_item_refresh_time = gpGlobals->time;
	}

	vector<AvHPlayer*> AllTeamAPlayers = AITAC_GetAllPlayersOnTeamOfClass(GetGameRules()->GetTeamANumber(), AVH_USER3_ALIEN_PLAYER3, nullptr);
	edict_t* LastTeamALerk = LastSeenLerkTeamA;

	if (!FNullEnt(LastTeamALerk) && IsPlayerLerk(LastTeamALerk))
	{
		LastSeenLerkTeamATime = gpGlobals->time;
	}
	else
	{
		for (auto it = AllTeamAPlayers.begin(); it != AllTeamAPlayers.end(); it++)
		{
			edict_t* PlayerEdict = (*it)->edict();

			if (FNullEnt(LastTeamALerk) || IsPlayerHuman(PlayerEdict))
			{
				LastTeamALerk = PlayerEdict;
				LastSeenLerkTeamATime = gpGlobals->time;
			}
		}

		LastSeenLerkTeamA = LastTeamALerk;
	}

	

	vector<AvHPlayer*> AllTeamBPlayers = AITAC_GetAllPlayersOnTeamOfClass(GetGameRules()->GetTeamBNumber(), AVH_USER3_ALIEN_PLAYER3, nullptr);
	edict_t* LastTeamBLerk = LastSeenLerkTeamB;

	if (!FNullEnt(LastTeamBLerk) && IsPlayerLerk(LastTeamBLerk))
	{
		LastSeenLerkTeamBTime = gpGlobals->time;
	}
	else
	{
		for (auto it = AllTeamBPlayers.begin(); it != AllTeamBPlayers.end(); it++)
		{
			edict_t* PlayerEdict = (*it)->edict();

			if (FNullEnt(LastTeamBLerk) || IsPlayerHuman(PlayerEdict))
			{
				LastTeamBLerk = PlayerEdict;
				LastSeenLerkTeamBTime = gpGlobals->time;
			}
		}

		LastSeenLerkTeamB = LastTeamBLerk;
	}



	

}

void AITAC_CheckNavMeshModified()
{
	if (bNavMeshModified)
	{
		AITAC_OnNavMeshModified();
	}
}

void AITAC_OnNavMeshModified()
{
	if (!NavmeshLoaded()) { return; }

	for (auto it = TeamAStructureMap.begin(); it != TeamAStructureMap.end(); it++)
	{
		it->second.bReachabilityMarkedDirty = true;
	}

	for (auto it = TeamBStructureMap.begin(); it != TeamBStructureMap.end(); it++)
	{
		it->second.bReachabilityMarkedDirty = true;
	}

	for (auto it = MarineDroppedItemMap.begin(); it != MarineDroppedItemMap.end(); it++)
	{
		it->second.bReachabilityMarkedDirty = true;
	}

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		it->bReachabilityMarkedDirty = true;
	}

	vector<AvHAIPlayer*> AllAIPlayers = AIMGR_GetAllAIPlayers();

	for (auto it = AllAIPlayers.begin(); it != AllAIPlayers.end(); it++)
	{
		AvHAIPlayer* ThisPlayer = (*it);

		if (IsPlayerActiveInGame(ThisPlayer->Edict) && ThisPlayer->BotNavInfo.CurrentPath.size() > 0)
		{
			ThisPlayer->BotNavInfo.NextForceRecalc = gpGlobals->time + frandrange(0.0f, 1.0f);
		}
	}

	bNavMeshModified = false;
}

void AITAC_RefreshBuildableStructures()
{
	if (!NavmeshLoaded()) { return; }

	CBaseEntity* currStructure = NULL;

	// Marine Structures
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_command")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "resourcetower")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_infportal")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_armory")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_turretfactory")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_advturretfactory")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "siegeturret")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "turret")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_advarmory")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_armslab")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_prototypelab")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_observatory")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "phasegate")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "item_mine")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}


	// Alien Structures
	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "alienresourcetower")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "defensechamber")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "offensechamber")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "movementchamber")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "sensorychamber")) != NULL) && currStructure)
	{
		AITAC_UpdateBuildableStructure(currStructure);
	}

	int NumReachabilitiesCalculated = 0;

	for (auto it = TeamAStructureMap.begin(); it != TeamAStructureMap.end();)
	{
		if (it->second.LastSeen < StructureRefreshFrame || FNullEnt(it->second.edict) || it->second.edict->v.deadflag != DEAD_NO || (it->second.edict->v.effects & EF_NODRAW))
		{
			AITAC_OnStructureDestroyed(&it->second);
			it = TeamAStructureMap.erase(it);
		}
		else
		{
			if (NumReachabilitiesCalculated < 3 && it->second.bReachabilityMarkedDirty)
			{
				AITAC_RefreshReachabilityForStructure(&it->second);
				NumReachabilitiesCalculated++;
			}
			it++;
		}
	}

	for (auto it = TeamBStructureMap.begin(); it != TeamBStructureMap.end();)
	{
		if (it->second.LastSeen < StructureRefreshFrame || FNullEnt(it->second.edict) || it->second.edict->v.deadflag != DEAD_NO || (it->second.edict->v.effects & EF_NODRAW))
		{
			AITAC_OnStructureDestroyed(&it->second);
			it = TeamBStructureMap.erase(it);
		}
		else
		{
			if (NumReachabilitiesCalculated < 3 && it->second.bReachabilityMarkedDirty)
			{
				AITAC_RefreshReachabilityForStructure(&it->second);
				NumReachabilitiesCalculated++;
			}
			it++;
		}
	}

	StructureRefreshFrame++;

}

void AITAC_RefreshMarineItems()
{
	if (!NavmeshLoaded()) { return; }

	CBaseEntity* currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "item_health")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_HEALTHPACK);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "item_genericammo")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_AMMO);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "item_heavyarmor")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_HEAVYARMOUR);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "item_jetpack")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_JETPACK);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "item_catalyst")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_CATALYSTS);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "weapon_mine")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_MINES);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "weapon_shotgun")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_SHOTGUN);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "weapon_heavymachinegun")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_HMG);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "weapon_grenadegun")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_GRENADELAUNCHER);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "weapon_welder")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_WELDER);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "weapon_mine")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_MINES);
	}

	currItem = NULL;
	while ((currItem = UTIL_FindEntityByClassname(currItem, "scan")) != NULL)
	{
		AITAC_UpdateMarineItem(currItem, DEPLOYABLE_ITEM_SCAN);
	}

	int NumReachabilitiesCalculated = 0;

	for (auto it = MarineDroppedItemMap.begin(); it != MarineDroppedItemMap.end();)
	{
		if (it->second.LastSeen < ItemRefreshFrame)
		{
			it = MarineDroppedItemMap.erase(it);
		}
		else
		{
			if (NumReachabilitiesCalculated < 3 && it->second.bReachabilityMarkedDirty)
			{
				AITAC_RefreshReachabilityForItem(&it->second);
				NumReachabilitiesCalculated++;
			}
			it++;
		}
	}

	ItemRefreshFrame++;

}

void AITAC_UpdateMarineItem(CBaseEntity* Item, AvHAIDeployableItemType ItemType)
{
	if (!Item) { return; }

	edict_t* ItemEdict = Item->edict();

	if (FNullEnt(ItemEdict) || ItemEdict->v.deadflag != DEAD_NO || (ItemEdict->v.effects & EF_NODRAW)) { return; }

	// All items except scans are of interest only if they're collectable. Without this check, marines will attempt to grab weapons from other players.
	if (ItemType != DEPLOYABLE_ITEM_SCAN)
	{
		if (ItemEdict->v.solid != SOLID_TRIGGER) { return; }

		if (ItemEdict->v.effects & EF_NODRAW) { return; }
	}
	
	int EntIndex = ENTINDEX(ItemEdict);
	if (EntIndex < 0) { return; }

	MarineDroppedItemMap[EntIndex].edict = ItemEdict;
	MarineDroppedItemMap[EntIndex].ItemType = ItemType;

	if (MarineDroppedItemMap[EntIndex].LastSeen == 0 || !vEquals(ItemEdict->v.origin, MarineDroppedItemMap[EntIndex].Location, 5.0f))
	{
		AITAC_RefreshReachabilityForItem(&MarineDroppedItemMap[EntIndex]);
	}

	MarineDroppedItemMap[EntIndex].Location = ItemEdict->v.origin;

	if (MarineDroppedItemMap[EntIndex].LastSeen == 0)
	{
		AITAC_OnItemDropped(&MarineDroppedItemMap[EntIndex]);
	}

	MarineDroppedItemMap[EntIndex].LastSeen = ItemRefreshFrame;
}

void AITAC_OnItemDropped(const AvHAIDroppedItem* NewItem)
{
	AvHTeamNumber TeamANumber = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamBNumber = GetGameRules()->GetTeamBNumber();

	
	AvHAIPlayer* TeamACommander = AIMGR_GetAICommander(TeamANumber);
	AvHAIPlayer* TeamBCommander = AIMGR_GetAICommander(TeamBNumber);

	if (TeamACommander)
	{
		AITAC_LinkDeployedItemToAction(TeamACommander, NewItem);
	}

	if (TeamBCommander)
	{
		AITAC_LinkDeployedItemToAction(TeamBCommander, NewItem);
	}
}

void AITAC_RefreshReachabilityForStructure(AvHAIBuildableStructure* Structure)
{
	if (Hives.size() == 0)
	{
		AITAC_RefreshHiveData();
	}

	if (!Structure || FNullEnt(Structure->edict) || Structure->edict->v.deadflag != DEAD_NO || (Structure->edict->v.effects & EF_NODRAW)) { return; }

	if (Structure->StructureType == STRUCTURE_MARINE_DEPLOYEDMINE)
	{
		Structure->TeamAReachabilityFlags = AI_REACHABILITY_ALL;
		Structure->TeamBReachabilityFlags = AI_REACHABILITY_ALL;
		return;
	}

	Structure->bReachabilityMarkedDirty = false;

	Structure->TeamAReachabilityFlags = AI_REACHABILITY_NONE;
	Structure->TeamBReachabilityFlags = AI_REACHABILITY_NONE;

	bool bIsOnNavMesh = UTIL_PointIsOnNavmesh(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], UTIL_GetEntityGroundLocation(Structure->edict), Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));

	if (!bIsOnNavMesh)
	{
		Structure->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		Structure->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		return;
	}

	if (GetGameRules()->GetTeamA()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);

		// Check if basic marines can reach. If they can then no need to separately check welder marines as they automatically can. If not, separately check for welders.
		if (bIsReachableMarine)
		{
			Structure->TeamAReachabilityFlags |= AI_REACHABILITY_MARINE;
			Structure->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);

			if (bIsReachableWelder)
			{
				Structure->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
			else
			{
				Structure->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
			}
		}
	}
	else
	{
		bool bIsReachableSkulk = UTIL_PointIsReachable(BaseNavProfiles[SKULK_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);
		bool bIsReachableGorge = UTIL_PointIsReachable(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);
		bool bIsReachableOnos = UTIL_PointIsReachable(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);

		if (bIsReachableSkulk)
		{
			Structure->TeamAReachabilityFlags |= AI_REACHABILITY_SKULK;
		}

		if (bIsReachableGorge)
		{
			Structure->TeamAReachabilityFlags |= AI_REACHABILITY_GORGE;
		}

		if (bIsReachableOnos)
		{
			Structure->TeamAReachabilityFlags |= AI_REACHABILITY_ONOS;
		}

		if (Structure->TeamAReachabilityFlags == AI_REACHABILITY_NONE)
		{
			Structure->TeamAReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		}
	}

	if (GetGameRules()->GetTeamB()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);

		// Check if basic marines can reach. If they can then no need to separately check welder marines as they automatically can. If not, separately check for welders.
		if (bIsReachableMarine)
		{
			Structure->TeamBReachabilityFlags |= AI_REACHABILITY_MARINE;
			Structure->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);

			if (bIsReachableWelder)
			{
				Structure->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
			else
			{
				Structure->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
			}
		}
	}
	else
	{
		bool bIsReachableSkulk = UTIL_PointIsReachable(BaseNavProfiles[SKULK_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);
		bool bIsReachableGorge = UTIL_PointIsReachable(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);
		bool bIsReachableOnos = UTIL_PointIsReachable(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);

		if (bIsReachableSkulk)
		{
			Structure->TeamBReachabilityFlags |= AI_REACHABILITY_SKULK;
		}

		if (bIsReachableGorge)
		{
			Structure->TeamBReachabilityFlags |= AI_REACHABILITY_GORGE;
		}

		if (bIsReachableOnos)
		{
			Structure->TeamBReachabilityFlags |= AI_REACHABILITY_ONOS;
		}

		if (Structure->TeamBReachabilityFlags == AI_REACHABILITY_NONE)
		{
			Structure->TeamBReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
		}
	}
}

AvHAIBuildableStructure* AITAC_UpdateBuildableStructure(CBaseEntity* Structure)
{
	if (!Structure || (Structure->pev->effects & EF_NODRAW) || (Structure->pev->deadflag != DEAD_NO)) { return nullptr; }

	edict_t* BuildingEdict = Structure->edict();

	AvHAIDeployableStructureType StructureType = UTIL_IUSER3ToStructureType(BuildingEdict->v.iuser3);

	if (StructureType == STRUCTURE_NONE) { return nullptr; }

	int EntIndex = ENTINDEX(BuildingEdict);

	if (EntIndex < 0) { return nullptr; }

	AvHTeamNumber TeamANumber = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamBNumber = GetGameRules()->GetTeamBNumber();

	std::unordered_map<int, AvHAIBuildableStructure>& BuildingMap = ((AvHTeamNumber)BuildingEdict->v.team == TeamANumber) ? TeamAStructureMap : TeamBStructureMap;

	AvHAIBuildableStructure* StructureRef = &BuildingMap[EntIndex];

	if (StructureType == STRUCTURE_MARINE_DEPLOYEDMINE)
	{
		StructureRef->StructureType = StructureType;
		if (StructureRef->LastSeen == 0)
		{
			StructureRef->Location = BuildingEdict->v.origin;
			StructureRef->edict = BuildingEdict;
			StructureRef->healthPercent = 1.0f;
			StructureRef->EntityRef = nullptr;
			StructureRef->StructureStatusFlags = STRUCTURE_STATUS_COMPLETED;
			StructureRef->TeamAReachabilityFlags = (AI_REACHABILITY_ALL & ~(AI_REACHABILITY_UNREACHABLE));
			StructureRef->TeamBReachabilityFlags = (AI_REACHABILITY_ALL & ~(AI_REACHABILITY_UNREACHABLE));
			AITAC_OnStructureCreated(&BuildingMap[EntIndex]);
		}

		StructureRef->LastSeen = StructureRefreshFrame;

		return StructureRef;
	}

	AvHBaseBuildable* BaseBuildable = dynamic_cast<AvHBaseBuildable*>(Structure);

	if (!BaseBuildable) 
	{
		return nullptr; 
	}

	StructureRef->StructureType = StructureType;

	// This is the first time we've seen this structure, so it must be new
	if (StructureRef->LastSeen == 0)
	{
		StructureRef->EntityRef = BaseBuildable;
		StructureRef->edict = BuildingEdict;

		StructureRef->OffMeshConnections.clear();
		StructureRef->Obstacles.clear();

		StructureRef->Location = g_vecZero; // We set this just below after calculating reachability

		AITAC_OnStructureCreated(&BuildingMap[EntIndex]);
	}

	if (vIsZero(StructureRef->Location) || !vEquals(BaseBuildable->pev->origin, StructureRef->Location, 5.0f))
	{
		AITAC_RefreshReachabilityForStructure(&BuildingMap[EntIndex]);

		StructureRef->Location = BaseBuildable->pev->origin;
	}

	unsigned int NewFlags = STRUCTURE_STATUS_NONE;

	bool bJustCompleted = false;
	bool bJustRecycled = false;
	bool bJustDestroyed = false;

	if (BaseBuildable->GetIsBuilt())
	{
		if (!(StructureRef->StructureStatusFlags & STRUCTURE_STATUS_COMPLETED))
		{
			bJustCompleted = true;
		}
		NewFlags |= STRUCTURE_STATUS_COMPLETED;
	}

	if (UTIL_IsStructureElectrified(BuildingEdict))
	{
		NewFlags |= STRUCTURE_STATUS_ELECTRIFIED;
	}

	if (BuildingEdict->v.iuser4 & MASK_PARASITED)
	{
		NewFlags |= STRUCTURE_STATUS_PARASITED;
	}

	if (BaseBuildable->GetIsRecycling())
	{
		if (!(StructureRef->StructureStatusFlags & STRUCTURE_STATUS_RECYCLING))
		{
			bJustRecycled = true;
		}
		NewFlags |= STRUCTURE_STATUS_RECYCLING;
	}

	if (BaseBuildable->GetIsResearching())
	{
		NewFlags |= STRUCTURE_STATUS_RESEARCHING;
	}

	if (StructureType == STRUCTURE_MARINE_TURRET)
	{
		AvHTurret* TurretRef = dynamic_cast<AvHTurret*>(BaseBuildable);

		if (TurretRef && !TurretRef->GetEnabledState())
		{
			NewFlags |= STRUCTURE_STATUS_DISABLED;
		}
	}

	float NewHealthPercent = (BuildingEdict->v.health / BuildingEdict->v.max_health);

	if (NewHealthPercent < StructureRef->healthPercent)
	{
		StructureRef->lastDamagedTime = gpGlobals->time;
	}

	StructureRef->healthPercent = NewHealthPercent;

	if (StructureRef->healthPercent < 0.99f && BaseBuildable->GetIsBuilt())
	{
		NewFlags |= STRUCTURE_STATUS_DAMAGED;
	}

	if (gpGlobals->time - StructureRef->lastDamagedTime < 10.0f)
	{
		NewFlags |= STRUCTURE_STATUS_UNDERATTACK;
	}

	StructureRef->StructureStatusFlags = NewFlags;
	StructureRef->LastSeen = StructureRefreshFrame;

	if (bJustCompleted)
	{
		AITAC_OnStructureCompleted(&BuildingMap[EntIndex]);
	}

	if (bJustRecycled)
	{
		AITAC_OnStructureBeginRecycling(&BuildingMap[EntIndex]);
	}

	if (StructureRef->Purpose == STRUCTURE_PURPOSE_NONE)
	{
		AvHTeamNumber StructureTeam = (AvHTeamNumber)StructureRef->edict->v.team;

		if (AIMGR_GetTeamType(StructureTeam) == AVH_CLASS_TYPE_MARINE)
		{
			switch (StructureRef->StructureType)
			{
				case STRUCTURE_MARINE_COMMCHAIR:
				case STRUCTURE_MARINE_INFANTRYPORTAL:
				case STRUCTURE_MARINE_ARMSLAB:
				case STRUCTURE_MARINE_PROTOTYPELAB:
					StructureRef->Purpose = STRUCTURE_PURPOSE_BASE;
					break;
				case STRUCTURE_MARINE_RESTOWER:
					StructureRef->Purpose = STRUCTURE_PURPOSE_GENERAL;
					break;
				case STRUCTURE_MARINE_TURRET:
					StructureRef->Purpose = STRUCTURE_PURPOSE_FORTIFY;
					break;
				case STRUCTURE_MARINE_SIEGETURRET:
					StructureRef->Purpose = STRUCTURE_PURPOSE_SIEGE;
					break;
				default:
				{
					Vector TeamStart = AITAC_GetTeamStartingLocation(StructureTeam);

					if (vDist2DSq(StructureRef->Location, TeamStart) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
					{
						StructureRef->Purpose = STRUCTURE_PURPOSE_BASE;
					}
					else
					{
						AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(StructureTeam);

						const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(StructureRef->Location);

						if (NearestHive)
						{
							if (NearestHive->Status == HIVE_STATUS_UNBUILT && vDist2DSq(NearestHive->FloorLocation, StructureRef->Location) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
							{
								StructureRef->Purpose = STRUCTURE_PURPOSE_FORTIFY;
							}
							else if (NearestHive->Status != HIVE_STATUS_UNBUILT && vDist2DSq(NearestHive->FloorLocation, StructureRef->Location) < sqrf(UTIL_MetresToGoldSrcUnits(25.0f)))
							{
								StructureRef->Purpose = STRUCTURE_PURPOSE_SIEGE;
							}
							else
							{
								StructureRef->Purpose = STRUCTURE_PURPOSE_GENERAL;
							}
						}
						else
						{
							StructureRef->Purpose = STRUCTURE_PURPOSE_GENERAL;
						}
					}

				}
				break;

			}
		}
		else
		{
			StructureRef->Purpose = STRUCTURE_PURPOSE_GENERAL;
		}
	}

	return StructureRef;

}

void AITAC_OnStructureCreated(AvHAIBuildableStructure* NewStructure)
{
	if (!NewStructure || FNullEnt(NewStructure->edict) || NewStructure->edict->v.deadflag != DEAD_NO || (NewStructure->edict->v.effects & EF_NODRAW)) { return; }

	UTIL_AddStructureTemporaryObstacles(NewStructure);

	AvHTeamNumber StructureTeam = (AvHTeamNumber)NewStructure->edict->v.team;

	AITAC_RefreshReachabilityForStructure(NewStructure);

	vector<NavHint*> NavHints = NAV_GetHintsOfType(STRUCTURE_NONE);

	for (auto it = NavHints.begin(); it != NavHints.end(); it++)
	{
		NavHint* ThisHint = (*it);

		if (vDist2DSq(NewStructure->edict->v.origin, ThisHint->Position) < sqrf(64.0f) && fabsf(NewStructure->Location.z - ThisHint->Position.z) < 50.0f)
		{
			ThisHint->OccupyingBuilding = NewStructure->edict;
		}
	}

	if (StructureTeam == TEAM_IND) { return; }
	
	AvHTeam* Team = GetGameRules()->GetTeam(StructureTeam);

	if (!Team) { return; }

	if (Team->GetTeamType() == AVH_CLASS_TYPE_ALIEN || NewStructure->StructureType == STRUCTURE_MARINE_DEPLOYEDMINE)
	{
		AITAC_LinkStructureToPlayer(NewStructure);
	}

}

void AITAC_OnStructureCompleted(AvHAIBuildableStructure* NewStructure)
{
	if (!NewStructure || FNullEnt(NewStructure->edict) || NewStructure->edict->v.deadflag != DEAD_NO || (NewStructure->edict->v.effects & EF_NODRAW)) { return; }

	if (NewStructure->StructureType == STRUCTURE_MARINE_PHASEGATE)
	{
		DeployableSearchFilter Filter;
		Filter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
		Filter.DeployableTeam = (AvHTeamNumber)NewStructure->edict->v.team;
		Filter.ReachabilityFlags = AI_REACHABILITY_NONE;
		Filter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;

		// Get all other completed phase gates for this team and add bidirectional connections to them
		std::vector<AvHAIBuildableStructure> OtherPhaseGates = AITAC_FindAllDeployables(NewStructure->Location, &Filter);

		SamplePolyFlags NewFlag = ((AvHTeamNumber)NewStructure->edict->v.team == GetGameRules()->GetTeamANumber()) ? SAMPLE_POLYFLAGS_TEAM1PHASEGATE : SAMPLE_POLYFLAGS_TEAM2PHASEGATE;

		for (auto pg = OtherPhaseGates.begin(); pg != OtherPhaseGates.end(); pg++)
		{
			// Don't add off-mesh connections to ourselves!
			if (pg->edict == NewStructure->edict) { continue; }

			AvHAIBuildableStructure OtherPhaseGate = (*pg);

			AvHAIOffMeshConnection NewConnection;
			NewConnection.FromLocation = NewStructure->Location;
			NewConnection.ToLocation = OtherPhaseGate.Location;
			NewConnection.ConnectionFlags = NewFlag;
			NewConnection.TargetObject = OtherPhaseGate.edict;
			memset(&NewConnection.ConnectionRefs[0], 0, sizeof(NewConnection.ConnectionRefs));

			UTIL_AddOffMeshConnection(NewStructure->Location, OtherPhaseGate.Location, SAMPLE_POLYAREA_PHASEGATE, NewFlag, true, &NewConnection);

			NewStructure->OffMeshConnections.push_back(NewConnection);

		}
	}

	if (NewStructure->StructureType == STRUCTURE_MARINE_INFANTRYPORTAL)
	{
		AvHTeamNumber Team = (AvHTeamNumber)NewStructure->edict->v.team;

		if (Team == GetGameRules()->GetTeamANumber())
		{
			TeamAStartingLocation = ZERO_VECTOR;
		}
		else
		{
			TeamBStartingLocation = ZERO_VECTOR;
		}

		AITAC_GetTeamStartingLocation(Team); // Force refresh of reachabilities and team starting locations
	}
}

void AITAC_RemovePhaseGateConnections(AvHAIBuildableStructure* SourceGate, AvHAIBuildableStructure* TargetGate)
{
	if (!SourceGate || !TargetGate) { return; }

	for (auto it = SourceGate->OffMeshConnections.begin(); it != SourceGate->OffMeshConnections.end();) 
	{
		if (it->TargetObject == TargetGate->edict)
		{
			UTIL_RemoveOffMeshConnections(&(*it));
			it = SourceGate->OffMeshConnections.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void AITAC_OnStructureBeginRecycling(AvHAIBuildableStructure* RecyclingStructure)
{
	if (!RecyclingStructure || FNullEnt(RecyclingStructure->edict) || RecyclingStructure->edict->v.deadflag != DEAD_NO || (RecyclingStructure->edict->v.effects & EF_NODRAW)) { return; }

	// For phase gates, treat them like they've been destroyed
	if (RecyclingStructure->StructureType == STRUCTURE_MARINE_PHASEGATE)
	{
		AITAC_OnStructureDestroyed(RecyclingStructure);
	}
}

void AITAC_OnStructureDestroyed(AvHAIBuildableStructure* DestroyedStructure)
{
	if (!DestroyedStructure || FNullEnt(DestroyedStructure->edict)) { return; }

	UTIL_RemoveStructureTemporaryObstacles(DestroyedStructure);

	if (DestroyedStructure->StructureType == STRUCTURE_MARINE_PHASEGATE)
	{
		// Eliminate all connections from this phase gate
		for (auto it = DestroyedStructure->OffMeshConnections.begin(); it != DestroyedStructure->OffMeshConnections.end(); it++)
		{
			UTIL_RemoveOffMeshConnections(&(*it));
		}

		DestroyedStructure->OffMeshConnections.clear();

		DeployableSearchFilter Filter;
		Filter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
		Filter.DeployableTeam = (AvHTeamNumber)DestroyedStructure->edict->v.team;
		Filter.ReachabilityFlags = AI_REACHABILITY_NONE;
		Filter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;

		// Get all other completed phase gates for this team and remove any connections going to this structure
		std::vector<AvHAIBuildableStructure*> OtherPhaseGates = AITAC_FindAllDeployablesByRef(DestroyedStructure->Location, &Filter);
		
		for (auto it = OtherPhaseGates.begin(); it != OtherPhaseGates.end(); it++)
		{
			// Don't check for off-mesh connections from ourselves!
			if ((*it)->edict == DestroyedStructure->edict) { continue; }

			AvHAIBuildableStructure* OtherPhaseGate = (*it);

			AITAC_RemovePhaseGateConnections(OtherPhaseGate, DestroyedStructure);
		}
	}

	if (DestroyedStructure->StructureType == STRUCTURE_MARINE_INFANTRYPORTAL)
	{
		AvHTeamNumber Team = (AvHTeamNumber)DestroyedStructure->edict->v.team;

		if (Team == GetGameRules()->GetTeamANumber())
		{
			TeamAStartingLocation = ZERO_VECTOR;
		}
		else
		{
			TeamBStartingLocation = ZERO_VECTOR;
		}

		AITAC_GetTeamStartingLocation(Team); // Force refresh of reachabilities and team starting locations
	}

	vector<NavHint*> NavHints = NAV_GetHintsOfType(DestroyedStructure->StructureType);

	for (auto it = NavHints.begin(); it != NavHints.end(); it++)
	{
		NavHint* ThisHint = (*it);

		if (ThisHint->OccupyingBuilding == DestroyedStructure->edict)
		{
			ThisHint->OccupyingBuilding = nullptr;
		}
	}
}

void AITAC_LinkStructureToPlayer(AvHAIBuildableStructure* NewStructure)
{
	vector<AvHAIPlayer*> AllTeamPlayers = AIMGR_GetAIPlayersOnTeam((AvHTeamNumber)NewStructure->edict->v.team);

	for (auto it = AllTeamPlayers.begin(); it != AllTeamPlayers.end(); it++)
	{
		AvHAIPlayer* Player = (*it);

		if (Player->PrimaryBotTask.ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING && Player->PrimaryBotTask.ActiveBuildInfo.AttemptedStructureType == NewStructure->StructureType)
		{
			if (vDist2DSq(NewStructure->Location, Player->PrimaryBotTask.ActiveBuildInfo.AttemptedLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
			{
				Player->PrimaryBotTask.ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_SUCCESS;
				Player->PrimaryBotTask.ActiveBuildInfo.LinkedStructure = NewStructure;
			}
			
		}

		if (Player->SecondaryBotTask.ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING && Player->SecondaryBotTask.ActiveBuildInfo.AttemptedStructureType == NewStructure->StructureType)
		{
			if (vDist2DSq(NewStructure->Location, Player->SecondaryBotTask.ActiveBuildInfo.AttemptedLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
			{
				Player->SecondaryBotTask.ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_SUCCESS;
				Player->SecondaryBotTask.ActiveBuildInfo.LinkedStructure = NewStructure;
			}

		}
	}
}

void AITAC_LinkDeployedItemToAction(AvHAIPlayer* CommanderBot, const AvHAIDroppedItem* NewItem)
{

}

void AITAC_ClearStructureNavData()
{
	for (auto& it : TeamAStructureMap)
	{
		AITAC_OnStructureDestroyed(&it.second);
	}

	for (auto& it : TeamBStructureMap)
	{
		AITAC_OnStructureDestroyed(&it.second);
	}
}

void AITAC_ClearMapAIData(bool bInitialMapLoad)
{
	UTIL_ClearLocalizations();

	ResourceNodes.clear();

	// If we're clearing AI data due to a map load, then we just clear the hive data immediately since we've reloaded the nav mesh
	// If we're clearing AI data due to a round restart, then ensure we properly clear all temp obstacles and connections since we're not reloading the mesh
	if (!bInitialMapLoad)
	{
		AITAC_ClearHiveInfo();

		AITAC_ClearStructureNavData();

		while (!bTileCacheUpToDate)
		{
			UTIL_UpdateTileCache();
		}
	}
	else
	{
		Hives.clear();
	}

	AITAC_ClearSquads();

	MarineDroppedItemMap.clear();
	TeamAStructureMap.clear();
	TeamBStructureMap.clear();

	StructureRefreshFrame = 1;
	ItemRefreshFrame = 1;

	last_structure_refresh_time = 0.0f;
	last_item_refresh_time = 0.0f;

	TeamAStartingLocation = ZERO_VECTOR;
	TeamBStartingLocation = ZERO_VECTOR;
}

void AITAC_RefreshTeamStartingLocations()
{
	TeamAStartingLocation = ZERO_VECTOR;
	TeamBStartingLocation = ZERO_VECTOR;

	AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber());
}

void AITAC_ClearHiveInfo()
{
	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		if (it->ObstacleRefs[REGULAR_NAV_MESH] != 0)
		{
			UTIL_RemoveTemporaryObstacles(it->ObstacleRefs);
		}
	}
	
	Hives.clear();
}

bool AITAC_AlienHiveNeedsReinforcing(const AvHAIHiveDefinition* Hive)
{
	if (!Hive) { return false; }

	DeployableSearchFilter SearchFilter;
	SearchFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;
	SearchFilter.IncludeStatusFlags = 0;
	SearchFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
	SearchFilter.DeployableTeam = Hive->OwningTeam;
	SearchFilter.ReachabilityTeam = TEAM_IND;

	int NumOffenceChambers = AITAC_GetNumDeployablesNearLocation(Hive->FloorLocation, &SearchFilter);

	if (NumOffenceChambers < 2) { return true; }

	if (AITAC_TeamHiveWithTechExists(Hive->OwningTeam, ALIEN_BUILD_DEFENSE_CHAMBER))
	{
		SearchFilter.DeployableTypes = STRUCTURE_ALIEN_DEFENCECHAMBER;
		int NumDefenceChambers = AITAC_GetNumDeployablesNearLocation(Hive->FloorLocation, &SearchFilter);

		if (NumDefenceChambers < 2) { return true; }
	}

	if (AITAC_TeamHiveWithTechExists(Hive->OwningTeam, ALIEN_BUILD_MOVEMENT_CHAMBER))
	{
		SearchFilter.DeployableTypes = STRUCTURE_ALIEN_MOVEMENTCHAMBER;
		bool bHasMoveChamber = AITAC_DeployableExistsAtLocation(Hive->FloorLocation, &SearchFilter);

		if (!bHasMoveChamber) { return true; }
	}

	if (AITAC_TeamHiveWithTechExists(Hive->OwningTeam, ALIEN_BUILD_SENSORY_CHAMBER))
	{
		SearchFilter.DeployableTypes = STRUCTURE_ALIEN_SENSORYCHAMBER;
		bool bHasSensoryChamber = AITAC_DeployableExistsAtLocation(Hive->FloorLocation, &SearchFilter);

		if (!bHasSensoryChamber) { return true; }
	}

	return false;
}

const AvHAIHiveDefinition* AITAC_GetHiveAtIndex(int Index)
{
	if (Index > -1 && Index < Hives.size())
	{
		return &Hives[Index];
	}

	return nullptr;
}

float AITAC_GetPhaseDistanceBetweenPoints(const Vector StartPoint, const Vector EndPoint)
{
	DeployableSearchFilter PGFilter;
	PGFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
	PGFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	PGFilter.bConsiderPhaseDistance = false;

	int NumPhaseGates = AITAC_GetNumDeployablesNearLocation(ZERO_VECTOR, &PGFilter);

	float DirectDist = vDist2D(StartPoint, EndPoint);

	if (NumPhaseGates < 2)
	{
		return DirectDist;
	}

	PGFilter.MaxSearchRadius = DirectDist;

	AvHAIBuildableStructure StartPhase = AITAC_FindClosestDeployableToLocation(StartPoint, &PGFilter);

	if (!StartPhase.IsValid())
	{
		return DirectDist;
	}

	AvHAIBuildableStructure EndPhase = AITAC_FindClosestDeployableToLocation(EndPoint, &PGFilter);

	if (FNullEnt(EndPhase.edict) || EndPhase.edict == StartPhase.edict)
	{
		return DirectDist;
	}

	float PhaseDist = vDist2DSq(StartPoint, StartPhase.edict->v.origin) + vDist2DSq(EndPoint, EndPhase.edict->v.origin);
	PhaseDist = sqrtf(PhaseDist);


	return fminf(DirectDist, PhaseDist);
}

AvHAIDeployableStructureType UTIL_IUSER3ToStructureType(const int inIUSER3)
{
	if (inIUSER3 == AVH_USER3_COMMANDER_STATION) { return STRUCTURE_MARINE_COMMCHAIR; }
	if (inIUSER3 == AVH_USER3_RESTOWER) { return STRUCTURE_MARINE_RESTOWER; }
	if (inIUSER3 == AVH_USER3_INFANTRYPORTAL) { return STRUCTURE_MARINE_INFANTRYPORTAL; }
	if (inIUSER3 == AVH_USER3_ARMORY) { return STRUCTURE_MARINE_ARMOURY; }
	if (inIUSER3 == AVH_USER3_ADVANCED_ARMORY) { return STRUCTURE_MARINE_ADVARMOURY; }
	if (inIUSER3 == AVH_USER3_TURRET_FACTORY) { return STRUCTURE_MARINE_TURRETFACTORY; }
	if (inIUSER3 == AVH_USER3_ADVANCED_TURRET_FACTORY) { return STRUCTURE_MARINE_ADVTURRETFACTORY; }
	if (inIUSER3 == AVH_USER3_TURRET) { return STRUCTURE_MARINE_TURRET; }
	if (inIUSER3 == AVH_USER3_SIEGETURRET) { return STRUCTURE_MARINE_SIEGETURRET; }
	if (inIUSER3 == AVH_USER3_ARMSLAB) { return STRUCTURE_MARINE_ARMSLAB; }
	if (inIUSER3 == AVH_USER3_PROTOTYPE_LAB) { return STRUCTURE_MARINE_PROTOTYPELAB; }
	if (inIUSER3 == AVH_USER3_OBSERVATORY) { return STRUCTURE_MARINE_OBSERVATORY; }
	if (inIUSER3 == AVH_USER3_PHASEGATE) { return STRUCTURE_MARINE_PHASEGATE; }
	if (inIUSER3 == AVH_USER3_MINE) { return STRUCTURE_MARINE_DEPLOYEDMINE; }

	if (inIUSER3 == AVH_USER3_HIVE) { return STRUCTURE_ALIEN_HIVE; }
	if (inIUSER3 == AVH_USER3_ALIENRESTOWER) { return STRUCTURE_ALIEN_RESTOWER; }
	if (inIUSER3 == AVH_USER3_DEFENSE_CHAMBER) { return STRUCTURE_ALIEN_DEFENCECHAMBER; }
	if (inIUSER3 == AVH_USER3_SENSORY_CHAMBER) { return STRUCTURE_ALIEN_SENSORYCHAMBER; }
	if (inIUSER3 == AVH_USER3_MOVEMENT_CHAMBER) { return STRUCTURE_ALIEN_MOVEMENTCHAMBER; }
	if (inIUSER3 == AVH_USER3_OFFENSE_CHAMBER) { return STRUCTURE_ALIEN_OFFENCECHAMBER; }

	return STRUCTURE_NONE;

}

unsigned char UTIL_GetAreaForObstruction(AvHAIDeployableStructureType StructureType, const edict_t* BuildingEdict)
{
	if (StructureType == STRUCTURE_NONE) { return DT_TILECACHE_NULL_AREA; }

	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	unsigned char TeamStructureArea = (BuildingEdict->v.team == TeamA) ? DT_TILECACHE_TEAM1STRUCTURE_AREA : DT_TILECACHE_TEAM2STRUCTURE_AREA;

	switch (StructureType)
	{
	case STRUCTURE_MARINE_COMMCHAIR:
	case STRUCTURE_MARINE_ARMOURY:
	case STRUCTURE_MARINE_ADVARMOURY:
	case STRUCTURE_MARINE_OBSERVATORY:
	case STRUCTURE_ALIEN_RESTOWER:
	case STRUCTURE_MARINE_RESTOWER:
	case STRUCTURE_ALIEN_HIVE:
		return TeamStructureArea;
	default:
		return DT_TILECACHE_BLOCKED_AREA;
	}

	return DT_TILECACHE_BLOCKED_AREA;
}

float UTIL_GetStructureRadiusForObstruction(AvHAIDeployableStructureType StructureType)
{
	if (StructureType == STRUCTURE_NONE) { return 0.0f; }

	switch (StructureType)
	{
	case STRUCTURE_MARINE_TURRETFACTORY:
	case STRUCTURE_MARINE_COMMCHAIR:
		return 60.0f;
	case STRUCTURE_MARINE_TURRET:
		return 30.0f;
	case STRUCTURE_MARINE_DEPLOYEDMINE:
		return 12.0f;
	default:
		return 40.0f;

	}

	return 40.0f;
}

bool UTIL_ShouldStructureCollide(AvHAIDeployableStructureType StructureType)
{
	if (StructureType == STRUCTURE_NONE) { return false; }

	switch (StructureType)
	{
	case STRUCTURE_MARINE_INFANTRYPORTAL:
	case STRUCTURE_MARINE_PHASEGATE:
	case STRUCTURE_MARINE_TURRET:
	case STRUCTURE_MARINE_DEPLOYEDMINE:
		return false;
	default:
		return true;

	}

	return true;
}

bool UTIL_IsStructureElectrified(edict_t* Structure)
{
	if (FNullEnt(Structure)) { return false; }

	return (!UTIL_StructureIsRecycling(Structure) && (Structure->v.iuser4 & MASK_UPGRADE_11));


}

bool UTIL_StructureIsFullyBuilt(edict_t* Structure)
{
	if (FNullEnt(Structure)) { return false; }

	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(Structure);

	if (StructureType == STRUCTURE_ALIEN_HIVE)
	{
		const AvHAIHiveDefinition* Hive = AITAC_GetHiveFromEdict(Structure);

		return (Hive && Hive->Status != HIVE_STATUS_UNBUILT);
	}
	else
	{
		if (!Structure) { return false; }

		AvHBaseBuildable* StructureRef = dynamic_cast<AvHBaseBuildable*>(CBaseEntity::Instance(Structure));

		return (StructureRef && StructureRef->GetIsBuilt());
	}

}

bool UTIL_IsBuildableStructureStillReachable(AvHAIPlayer* pBot, const edict_t* Structure)
{
	int Index = ENTINDEX(Structure);

	if (Index < 0) { return false; }

	// Hives have static positions so should always be considered reachable.
	// Resource towers technically do too, but there could be some built by humans which the bots can't get to
	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(Structure);
	if (StructureType == STRUCTURE_ALIEN_HIVE || StructureType == STRUCTURE_NONE) { return true; }

	AvHAIBuildableStructure StructureRef = AITAC_GetDeployableFromEdict(Structure);

	if (!StructureRef.IsValid()) { return false; }

	unsigned int TeamReachability = (pBot->Edict->v.team == GetGameRules()->GetTeamANumber()) ? StructureRef.TeamAReachabilityFlags : StructureRef.TeamBReachabilityFlags;

	return (TeamReachability & pBot->BotNavInfo.NavProfile.ReachabilityFlag);
}

bool UTIL_IsDroppedItemStillReachable(AvHAIPlayer* pBot, const edict_t* Item)
{
	int Index = ENTINDEX(Item);

	if (Index < 0) { return false; }

	unsigned int TeamReachability = (pBot->Edict->v.team == GetGameRules()->GetTeamANumber()) ? MarineDroppedItemMap[Index].TeamAReachabilityFlags : MarineDroppedItemMap[Index].TeamBReachabilityFlags;

	return (TeamReachability & pBot->BotNavInfo.NavProfile.ReachabilityFlag);
}

AvHAIWeapon UTIL_GetWeaponTypeFromEdict(const edict_t* ItemEdict)
{
	int Index = ENTINDEX(ItemEdict);

	if (Index < 0) { return WEAPON_INVALID; }

	AvHAIDeployableItemType ItemType = MarineDroppedItemMap[Index].ItemType;
		
	switch (ItemType)
	{
	case DEPLOYABLE_ITEM_WELDER:
		return WEAPON_MARINE_WELDER;
	case DEPLOYABLE_ITEM_HMG:
		return WEAPON_MARINE_HMG;
	case DEPLOYABLE_ITEM_GRENADELAUNCHER:
		return WEAPON_MARINE_GL;
	case DEPLOYABLE_ITEM_SHOTGUN:
		return WEAPON_MARINE_SHOTGUN;
	case DEPLOYABLE_ITEM_MINES:
		return WEAPON_MARINE_MINES;
	default:
		return WEAPON_INVALID;
	}

	return WEAPON_INVALID;
}

bool UTIL_StructureIsRecycling(edict_t* Structure)
{
	if (!Structure) { return false; }

	AvHBaseBuildable* StructureRef = dynamic_cast<AvHBaseBuildable*>(CBaseEntity::Instance(Structure));

	return (StructureRef && StructureRef->GetIsRecycling());
}

bool UTIL_StructureIsUpgrading(edict_t* Structure)
{
	if (!Structure) { return false; }

	AvHBaseBuildable* StructureRef = dynamic_cast<AvHBaseBuildable*>(CBaseEntity::Instance(Structure));

	return (StructureRef && StructureRef->GetIsResearching() && (Structure->v.iuser2 == ARMORY_UPGRADE || Structure->v.iuser2 == TURRET_FACTORY_UPGRADE));
}

bool UTIL_StructureIsResearching(edict_t* Structure)
{
	if (!Structure) { return false; }

	AvHBaseBuildable* StructureRef = dynamic_cast<AvHBaseBuildable*>(CBaseEntity::Instance(Structure));
	
	return (StructureRef && StructureRef->GetIsResearching());
}

bool UTIL_StructureIsResearching(edict_t* Structure, const AvHMessageID Research)
{
	if (!Structure) { return false; }

	AvHBaseBuildable* StructureRef = dynamic_cast<AvHBaseBuildable*>(CBaseEntity::Instance(Structure));

	return (StructureRef && StructureRef->GetIsResearching() && Structure->v.iuser2 == (int)Research);
}

bool AITAC_MarineResearchIsAvailable(const AvHTeamNumber Team, const AvHMessageID Research)
{
	AvHTeam* PlayerTeam = GetGameRules()->GetTeam(Team);

	if (!PlayerTeam) { return false; }

	AvHMessageID Message = Research;

	return PlayerTeam->GetResearchManager().GetIsMessageAvailable(Message);

}

bool AITAC_ElectricalResearchIsAvailable(edict_t* Structure)
{
	if (FNullEnt(Structure)) { return false; }

	if (UTIL_IsStructureElectrified(Structure)) { return false; }

	AvHAIDeployableStructureType StructureTypeToElectrify = GetStructureTypeFromEdict(Structure);
	AvHAIDeployableStructureType ElectrifyableStructureTypes = (AvHAIDeployableStructureType)(STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY | STRUCTURE_MARINE_RESTOWER);

	if (!(ElectrifyableStructureTypes & StructureTypeToElectrify)) { return false; }

	DeployableSearchFilter TFFilter;
	TFFilter.DeployableTypes = STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY;
	TFFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	TFFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	TFFilter.DeployableTeam = (AvHTeamNumber)Structure->v.team;

	if (!AITAC_DeployableExistsAtLocation(AITAC_GetCommChairLocation((AvHTeamNumber)Structure->v.team), &TFFilter)) { return false; }

	return (Structure->v.iuser4 & MASK_UPGRADE_1);
}

AvHAIHiveDefinition* AITAC_GetHiveFromEdict(const edict_t* Edict)
{
	if (Edict->v.iuser3 != AVH_USER3_HIVE) { return nullptr; }

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		if (it->HiveEdict == Edict)
		{
			return &(*it);
		}
	}

	return nullptr;
}

AvHAIResourceNode* AITAC_GetResourceNodeFromEdict(const edict_t* Edict)
{
	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		if (it->ResourceEdict == Edict)
		{
			return &(*it);
		}
	}

	return nullptr;
}

const AvHAIHiveDefinition* AITAC_GetHiveNearestLocation(const Vector SearchLocation)
{
	AvHAIHiveDefinition* Result = nullptr;
	float MinDist = 0.0f;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		float ThisDist = vDist3DSq(SearchLocation, it->Location);

		if (!Result || ThisDist < MinDist)
		{
			Result = &(*it);
			MinDist = ThisDist;
		}
	}

	return Result;
}

const AvHAIHiveDefinition* AITAC_GetActiveHiveNearestLocation(AvHTeamNumber Team, const Vector SearchLocation)
{
	AvHAIHiveDefinition* Result = nullptr;
	float MinDist = 0.0f;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		if (it->Status != HIVE_STATUS_BUILT || it->OwningTeam != Team) { continue; }

		float ThisDist = vDist3DSq(SearchLocation, it->Location);

		if (!Result || ThisDist < MinDist)
		{
			Result = &(*it);
			MinDist = ThisDist;
		}
	}

	return Result;
}

const AvHAIHiveDefinition* AITAC_GetNonEmptyHiveNearestLocation(const Vector SearchLocation)
{
	AvHAIHiveDefinition* Result = nullptr;
	float MinDist = 0.0f;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		if (it->Status == HIVE_STATUS_UNBUILT) { continue; }

		float ThisDist = vDist3DSq(SearchLocation, it->Location);

		if (!Result || ThisDist < MinDist)
		{
			Result = &(*it);
			MinDist = ThisDist;
		}
	}

	return Result;
}

AvHAIResourceNode* AITAC_GetNearestResourceNodeToLocation(const Vector Location)
{
	AvHAIResourceNode* Result = nullptr;
	float CurrMinDist = 0;

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		float DistSq = vDist2DSq(it->Location, Location);

		if (!Result || DistSq < CurrMinDist)
		{
			Result = &(*it);
			CurrMinDist = DistSq;
		}
	}

	return Result;
}

float AITAC_GetTeamResNodeOwnership(const AvHTeamNumber Team, bool bIncludeBaseNodes)
{
	int NumViableResNodes = 0;
	int NumOwnedResNodes = 0;

	AvHAIResourceNode* MarineBaseNode = nullptr;
	bool bIsMarineType = AIMGR_GetTeamType(Team) == AVH_CLASS_TYPE_MARINE;

	if (bIsMarineType)
	{
		MarineBaseNode = AITAC_GetNearestResourceNodeToLocation(AITAC_GetCommChairLocation(Team));
	}

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		unsigned int CheckReachabilityFlags = (it->TeamAReachabilityFlags | it->TeamBReachabilityFlags);

		if (Team != TEAM_IND)
		{
			CheckReachabilityFlags = (Team == GetGameRules()->GetTeamANumber()) ? it->TeamAReachabilityFlags : it->TeamBReachabilityFlags;
		}

		if (CheckReachabilityFlags == AI_REACHABILITY_UNREACHABLE) { continue; }

		if (!bIncludeBaseNodes)
		{
			if (it->bIsBaseNode)
			{
				if (bIsMarineType)
				{
					if (it->ResourceEntity == MarineBaseNode->ResourceEntity) { continue; }
				}
				else
				{
					const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(it->Location);

					if (NearestHive->Status != HIVE_STATUS_UNBUILT) { continue; }
				}
			}
		}

		NumViableResNodes++;

		if (it->OwningTeam == Team)
		{
			NumOwnedResNodes++;
		}
	}

	// If there are no viable resource nodes, then report we own them all to avoid divide by zero
	if (NumViableResNodes == 0) { return 1.0f; }

	return (float)NumOwnedResNodes / (float)NumViableResNodes;
}

int	AITAC_GetNumResourceNodesNearLocation(const Vector Location, const DeployableSearchFilter* Filter)
{
	int Result = 0;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	float CurrMinDist = 0;

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE)
		{
			unsigned int CheckReachabilityFlags = (it->TeamAReachabilityFlags | it->TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				CheckReachabilityFlags = (Filter->ReachabilityTeam == GetGameRules()->GetTeamANumber()) ? it->TeamAReachabilityFlags : it->TeamBReachabilityFlags;
			}

			if (!(CheckReachabilityFlags & Filter->ReachabilityFlags)) { continue; }
		}


		if (it->OwningTeam != Filter->DeployableTeam) { continue; }

		float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it->Location, Location)) : vDist2DSq(it->Location, Location);

		if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (!Result || DistSq < CurrMinDist))
		{
			Result++;
		}
	}

	return Result;
}

vector<AvHAIResourceNode*> AITAC_GetAllMatchingResourceNodes(const Vector Location, const DeployableSearchFilter* Filter)
{
	vector<AvHAIResourceNode*> Results;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	float CurrMinDist = 0;

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE)
		{
			unsigned int CheckReachabilityFlags = (it->TeamAReachabilityFlags | it->TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				CheckReachabilityFlags = (Filter->ReachabilityTeam == GetGameRules()->GetTeamANumber()) ? it->TeamAReachabilityFlags : it->TeamBReachabilityFlags;
			}

			if (!(CheckReachabilityFlags & Filter->ReachabilityFlags)) { continue; }
		}


		if (it->OwningTeam != Filter->DeployableTeam) { continue; }

		float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it->Location, Location)) : vDist2DSq(it->Location, Location);

		if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq))
		{
			Results.push_back(&(*it));
		}
	}

	return Results;
}

AvHAIResourceNode* AITAC_FindNearestResourceNodeToLocation(const Vector Location, const DeployableSearchFilter* Filter)
{
	AvHAIResourceNode* Result = nullptr;

	float MinDistSq = sqrf(Filter->MinSearchRadius);
	float MaxDistSq = sqrf(Filter->MaxSearchRadius);

	bool bUseMinDist = MinDistSq > 0.1f;
	bool bUseMaxDist = MaxDistSq > 0.1f;

	float CurrMinDist = 0;

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE)
		{
			unsigned int CheckReachabilityFlags = (it->TeamAReachabilityFlags | it->TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				CheckReachabilityFlags = (Filter->ReachabilityTeam == GetGameRules()->GetTeamANumber()) ? it->TeamAReachabilityFlags : it->TeamBReachabilityFlags;
			}

			if (!(CheckReachabilityFlags & Filter->ReachabilityFlags)) { continue; }
		}

		
		if (it->OwningTeam != Filter->DeployableTeam) { continue; }
		
		float DistSq = (Filter->bConsiderPhaseDistance) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(it->Location, Location)) : vDist2DSq(it->Location, Location);

		if ((!bUseMinDist || DistSq >= MinDistSq) && (!bUseMaxDist || DistSq <= MaxDistSq) && (!Result || DistSq < CurrMinDist))
		{
			Result = &(*it);
		}
	}

	return Result;

}

int AITAC_GetNumActivePlayersOnTeam(const AvHTeamNumber Team)
{
	int Result = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && !PlayerEdict->free && IsPlayerActiveInGame(PlayerEdict)) { Result++; }

		
	}

	return Result;
}

int AITAC_GetNumPlayersOfTeamAndClassInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 SearchClass)
{
	int Result = 0;
	float MaxRadiusSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (PlayerRef != nullptr && GetPlayerActiveClass(PlayerRef) == SearchClass && (Team == TEAM_IND || PlayerRef->GetTeam() == Team) && IsPlayerActiveInGame(PlayerEdict))
		{
			float Dist = (bConsiderPhaseDist) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(PlayerEdict->v.origin, SearchLocation)) : vDist2DSq(PlayerEdict->v.origin, SearchLocation);

			if (Dist <= MaxRadiusSq)
			{
				Result++;
			}
		}
	}

	return Result;
}

vector<AvHPlayer*> AITAC_GetAllPlayersOfTeamInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 IgnoreClass)
{
	vector<AvHPlayer*> Result;

	float MaxRadiusSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (PlayerRef != nullptr && GetPlayerActiveClass(PlayerRef) != IgnoreClass && (Team == TEAM_IND || PlayerRef->GetTeam() == Team) && IsPlayerActiveInGame(PlayerEdict))
		{
			float Dist = (bConsiderPhaseDist) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(PlayerEdict->v.origin, SearchLocation)) : vDist2DSq(PlayerEdict->v.origin, SearchLocation);

			if (Dist <= MaxRadiusSq)
			{
				Result.push_back(PlayerRef);
			}
		}
	}

	return Result;
}

int AITAC_GetNumPlayersOfTeamInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 IgnoreClass)
{
	int Result = 0;
	float MaxRadiusSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (PlayerRef != nullptr && GetPlayerActiveClass(PlayerRef) != IgnoreClass && (Team == TEAM_IND || PlayerRef->GetTeam() == Team) && IsPlayerActiveInGame(PlayerEdict))
		{
			float Dist = (bConsiderPhaseDist) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(PlayerEdict->v.origin, SearchLocation)) : vDist2DSq(PlayerEdict->v.origin, SearchLocation);

			if (Dist <= MaxRadiusSq)
			{
				Result++;
			}
		}
	}

	return Result;

}

bool AITAC_AnyPlayersOfTeamInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 IgnoreClass)
{
	float MaxRadiusSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (PlayerRef != nullptr && GetPlayerActiveClass(PlayerRef) != IgnoreClass && (Team == TEAM_IND || PlayerRef->GetTeam() == Team) && IsPlayerActiveInGame(PlayerEdict))
		{
			float Dist = (bConsiderPhaseDist) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(PlayerEdict->v.origin, SearchLocation)) : vDist2DSq(PlayerEdict->v.origin, SearchLocation);

			if (Dist <= MaxRadiusSq)
			{
				return true;
			}
		}
	}

	return false;
}

vector<AvHPlayer*> AITAC_GetAllPlayersOnTeamOfClass(const AvHTeamNumber Team, const AvHUser3 SearchClass, const edict_t* IgnorePlayer)
{
	vector<AvHPlayer*> Result;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (PlayerRef != nullptr && (SearchClass == AVH_USER3_NONE || GetPlayerActiveClass(PlayerRef) == SearchClass) && (Team == TEAM_IND || PlayerRef->GetTeam() == Team) && IsPlayerActiveInGame(PlayerEdict))
		{
			Result.push_back(PlayerRef);
		}

	}

	return Result;
}

int AITAC_GetNumPlayersOnTeamOfClass(const AvHTeamNumber Team, const AvHUser3 SearchClass, const edict_t* IgnorePlayer)
{
	int Result = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (PlayerRef != nullptr && (SearchClass == AVH_USER3_NONE || GetPlayerActiveClass(PlayerRef) == SearchClass) && (Team == TEAM_IND || PlayerRef->GetTeam() == Team) && IsPlayerActiveInGame(PlayerEdict))
		{
			Result++;
		}

	}

	return Result;
}

edict_t* AITAC_GetNearestPlayerOfClassInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 SearchClass)
{
	edict_t* Result = nullptr;
	float MaxRadiusSq = sqrf(SearchRadius);
	float MinDistSq = 0.0f;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (PlayerRef != nullptr && (SearchClass == AVH_USER3_NONE || GetPlayerActiveClass(PlayerRef) == SearchClass) && (Team == TEAM_IND || PlayerRef->GetTeam() == Team) && IsPlayerActiveInGame(PlayerEdict))
		{
			float Dist = (bConsiderPhaseDist) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(PlayerEdict->v.origin, SearchLocation)) : vDist2DSq(PlayerEdict->v.origin, SearchLocation);

			if (Dist <= MaxRadiusSq && (FNullEnt(Result) || Dist < MinDistSq))
			{
				Result = PlayerEdict;
				MinDistSq = Dist;
			}
		}
	}

	return Result;

}

vector<edict_t*> AITAC_GetAllPlayersOfClassInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 SearchClass)
{
	vector<edict_t*> Result;
	float MaxRadiusSq = sqrf(SearchRadius);
	float MinDistSq = 0.0f;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (PlayerRef != nullptr && (SearchClass == AVH_USER3_NONE || GetPlayerActiveClass(PlayerRef) == SearchClass) && (Team == TEAM_IND || PlayerRef->GetTeam() == Team) && IsPlayerActiveInGame(PlayerEdict))
		{
			float Dist = (bConsiderPhaseDist) ? sqrf(AITAC_GetPhaseDistanceBetweenPoints(PlayerEdict->v.origin, SearchLocation)) : vDist2DSq(PlayerEdict->v.origin, SearchLocation);

			if (Dist <= MaxRadiusSq)
			{
				Result.push_back(PlayerEdict);
				MinDistSq = Dist;
			}
		}
	}

	return Result;
}

AvHAIHiveDefinition* AITAC_GetTeamHiveWithTech(const AvHTeamNumber Team, const AvHMessageID Tech)
{
	AvHTeam* TeamRef = GetGameRules()->GetTeam(Team);

	// If the team is invalid or marine team, return nothing since marines can't own hives.
	if (!TeamRef || TeamRef->GetTeamType() != AVH_CLASS_TYPE_ALIEN) { return nullptr; }


	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		// Only return active hives with the tech
		if (it->OwningTeam == Team && it->Status == HIVE_STATUS_BUILT && it->TechStatus == Tech)
		{
			return &(*it);
		}
	}

	return nullptr;
}

bool AITAC_TeamHiveWithTechExists(const AvHTeamNumber Team, const AvHMessageID Tech)
{
	AvHTeam* TeamRef = GetGameRules()->GetTeam(Team);

	// If the team is invalid or marine team, return nothing since marines can't own hives.
	if (!TeamRef || TeamRef->GetTeamType() != AVH_CLASS_TYPE_ALIEN) { return false; }


	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		// Only return active hives with the tech
		if (it->OwningTeam == Team && it->Status == HIVE_STATUS_BUILT && it->TechStatus == Tech)
		{
			return true;
		}
	}

	return false;
}

AvHAIDeployableItemType UTIL_GetItemTypeFromEdict(const edict_t* ItemEdict)
{
	if (FNullEnt(ItemEdict)) { return DEPLOYABLE_ITEM_NONE; }

	int ItemIndex = ENTINDEX(ItemEdict);

	if (ItemIndex < 0) { return DEPLOYABLE_ITEM_NONE; }

	return MarineDroppedItemMap[ItemIndex].ItemType;
}

bool UTIL_DroppedItemIsPrimaryWeapon(const AvHAIDeployableItemType ItemType)
{
	switch (ItemType)
	{
		case DEPLOYABLE_ITEM_GRENADELAUNCHER:
		case DEPLOYABLE_ITEM_HMG:
		case DEPLOYABLE_ITEM_SHOTGUN:
			return true;
		default:
			return false;
	}

	return false;
}

AvHAIWeapon UTIL_GetWeaponTypeFromDroppedItem(const AvHAIDeployableItemType ItemType)
{
	switch (ItemType)
	{
	case DEPLOYABLE_ITEM_GRENADELAUNCHER:
		return WEAPON_MARINE_GL;
	case DEPLOYABLE_ITEM_HMG:
		return WEAPON_MARINE_HMG;
	case DEPLOYABLE_ITEM_SHOTGUN:
		return WEAPON_MARINE_SHOTGUN;
	case DEPLOYABLE_ITEM_WELDER:
		return WEAPON_MARINE_WELDER;
	case DEPLOYABLE_ITEM_MINES:
		return WEAPON_MARINE_MINES;
	default:
		return WEAPON_INVALID;
	}

	return WEAPON_INVALID;
}

Vector UTIL_GetNextMinePosition2(edict_t* StructureToMine)
{
	if (FNullEnt(StructureToMine)) { return ZERO_VECTOR; }

	AvHTeamNumber StructureTeam = (AvHTeamNumber)StructureToMine->v.team;
	
	nav_profile MineCheckProfile = GetBaseNavProfile(MARINE_BASE_NAV_PROFILE);
	MineCheckProfile.Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_BLOCKED);
	MineCheckProfile.Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_TEAM1STRUCTURE);
	MineCheckProfile.Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_TEAM2STRUCTURE);
	MineCheckProfile.Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_WELD);
	MineCheckProfile.Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_DOOR);
	MineCheckProfile.Filters.addExcludeFlags(SAMPLE_POLYFLAGS_BLOCKED);
	MineCheckProfile.Filters.addExcludeFlags(SAMPLE_POLYFLAGS_TEAM1STRUCTURE);
	MineCheckProfile.Filters.addExcludeFlags(SAMPLE_POLYFLAGS_TEAM2STRUCTURE);
	MineCheckProfile.Filters.addExcludeFlags(SAMPLE_POLYFLAGS_WELD);
	MineCheckProfile.Filters.addExcludeFlags(SAMPLE_POLYFLAGS_DOOR);

	Vector FwdVector = UTIL_GetForwardVector2D(StructureToMine->v.angles);
	Vector RightVector = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(FwdVector, UP_VECTOR));

	bool bFwd = false;
	bool bRight = false;
	bool bBack = false;
	bool bLeft = false;

	DeployableSearchFilter MineFilter;
	MineFilter.DeployableTeam = StructureTeam;
	MineFilter.DeployableTypes = STRUCTURE_MARINE_DEPLOYEDMINE;
	MineFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(3.0f);

	vector<AvHAIBuildableStructure> SurroundingMines = AITAC_FindAllDeployables(StructureToMine->v.origin, &MineFilter);

	for (auto it = SurroundingMines.begin(); it != SurroundingMines.end(); it++)
	{
		AvHAIBuildableStructure ThisMine = (*it);

		Vector Dir = UTIL_GetVectorNormal2D(ThisMine.Location - StructureToMine->v.origin);

		if (UTIL_GetDotProduct2D(FwdVector, Dir) > 0.7f)
		{
			bFwd = true;
		}

		if (UTIL_GetDotProduct2D(FwdVector, Dir) < -0.7f)
		{
			bBack = true;
		}

		if (UTIL_GetDotProduct2D(RightVector, Dir) > 0.7f)
		{
			bRight = true;
		}

		if (UTIL_GetDotProduct2D(RightVector, Dir) < -0.7f)
		{
			bLeft = true;
		}
	}

	float Size = fmaxf(StructureToMine->v.size.x, StructureToMine->v.size.y);
	Size += 8.0f;

	if (!bFwd)
	{
		Vector SearchLocation = StructureToMine->v.origin + (FwdVector * Size);

		Vector BuildLocation = UTIL_ProjectPointToNavmesh(SearchLocation, MineCheckProfile);

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	if (!bBack)
	{
		Vector SearchLocation = StructureToMine->v.origin - (FwdVector * Size);

		Vector BuildLocation = UTIL_ProjectPointToNavmesh(SearchLocation, MineCheckProfile);

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	if (!bRight)
	{
		Vector SearchLocation = StructureToMine->v.origin + (RightVector * Size);

		Vector BuildLocation = UTIL_ProjectPointToNavmesh(SearchLocation, MineCheckProfile);

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	if (!bLeft)
	{
		Vector SearchLocation = StructureToMine->v.origin - (RightVector * Size);

		Vector BuildLocation = UTIL_ProjectPointToNavmesh(SearchLocation, MineCheckProfile);

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(MineCheckProfile, StructureToMine->v.origin, Size);

	return BuildLocation;
}

Vector UTIL_GetNextMinePosition(edict_t* StructureToMine)
{
	if (FNullEnt(StructureToMine)) { return ZERO_VECTOR; }

	AvHTeamNumber StructureTeam = (AvHTeamNumber)StructureToMine->v.team;

	Vector FwdVector = UTIL_GetForwardVector2D(StructureToMine->v.angles);
	Vector RightVector = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(FwdVector, UP_VECTOR));

	bool bFwd = false;
	bool bRight = false;
	bool bBack = false;
	bool bLeft = false;

	DeployableSearchFilter MineFilter;
	MineFilter.DeployableTeam = StructureTeam;
	MineFilter.DeployableTypes = STRUCTURE_MARINE_DEPLOYEDMINE;
	MineFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(3.0f);

	vector<AvHAIBuildableStructure> SurroundingMines = AITAC_FindAllDeployables(StructureToMine->v.origin, &MineFilter);

	for (auto it = SurroundingMines.begin(); it != SurroundingMines.end(); it++)
	{
		AvHAIBuildableStructure ThisMine = (*it);

		Vector Dir = UTIL_GetVectorNormal2D(ThisMine.Location - StructureToMine->v.origin);

		if (UTIL_GetDotProduct2D(FwdVector, Dir) > 0.7f)
		{
			bFwd = true;
		}

		if (UTIL_GetDotProduct2D(FwdVector, Dir) < -0.7f)
		{
			bBack = true;
		}

		if (UTIL_GetDotProduct2D(RightVector, Dir) > 0.7f)
		{
			bRight = true;
		}

		if (UTIL_GetDotProduct2D(RightVector, Dir) < -0.7f)
		{
			bLeft = true;
		}
	}

	float Size = fmaxf(StructureToMine->v.size.x, StructureToMine->v.size.y);
	Size += 8.0f;

	if (!bFwd)
	{
		Vector SearchLocation = StructureToMine->v.origin + (FwdVector * Size);

		Vector BuildLocation = UTIL_ProjectPointToNavmesh(SearchLocation);

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	if (!bBack)
	{
		Vector SearchLocation = StructureToMine->v.origin - (FwdVector * Size);

		Vector BuildLocation = UTIL_ProjectPointToNavmesh(SearchLocation);

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	if (!bRight)
	{
		Vector SearchLocation = StructureToMine->v.origin + (RightVector * Size);

		Vector BuildLocation = UTIL_ProjectPointToNavmesh(SearchLocation);

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	if (!bLeft)
	{
		Vector SearchLocation = StructureToMine->v.origin - (RightVector * Size);

		Vector BuildLocation = UTIL_ProjectPointToNavmesh(SearchLocation);

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), StructureToMine->v.origin, Size);

	return BuildLocation;
}

int UTIL_GetCostOfStructureType(AvHAIDeployableStructureType StructureType)
{
	switch (StructureType)
	{
	case STRUCTURE_MARINE_ARMOURY:
		return BALANCE_VAR(kArmoryCost);
	case STRUCTURE_MARINE_ARMSLAB:
		return BALANCE_VAR(kArmsLabCost);
	case STRUCTURE_MARINE_COMMCHAIR:
		return BALANCE_VAR(kCommandStationCost);
	case STRUCTURE_MARINE_INFANTRYPORTAL:
		return BALANCE_VAR(kInfantryPortalCost);
	case STRUCTURE_MARINE_OBSERVATORY:
		return BALANCE_VAR(kObservatoryCost);
	case STRUCTURE_MARINE_PHASEGATE:
		return BALANCE_VAR(kPhaseGateCost);
	case STRUCTURE_MARINE_PROTOTYPELAB:
		return BALANCE_VAR(kPrototypeLabCost);
	case STRUCTURE_MARINE_RESTOWER:
	case STRUCTURE_ALIEN_RESTOWER:
		return BALANCE_VAR(kResourceTowerCost);
	case STRUCTURE_MARINE_SIEGETURRET:
		return BALANCE_VAR(kSiegeCost);
	case STRUCTURE_MARINE_TURRET:
		return BALANCE_VAR(kSentryCost);
	case STRUCTURE_MARINE_TURRETFACTORY:
		return BALANCE_VAR(kTurretFactoryCost);
	case STRUCTURE_ALIEN_HIVE:
		return BALANCE_VAR(kHiveCost);
	case STRUCTURE_ALIEN_OFFENCECHAMBER:
		return BALANCE_VAR(kOffenseChamberCost);
	case STRUCTURE_ALIEN_DEFENCECHAMBER:
		return BALANCE_VAR(kDefenseChamberCost);
	case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
		return BALANCE_VAR(kMovementChamberCost);
	case STRUCTURE_ALIEN_SENSORYCHAMBER:
		return BALANCE_VAR(kSensoryChamberCost);
	default:
		return 0;

	}

	return 0;
}

int AITAC_GetNumHives()
{
	return Hives.size();
}

int AITAC_GetNumTeamHives(AvHTeamNumber Team, bool bFullyCompletedOnly)
{
	int Result = 0;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		if (it->OwningTeam == Team && (!bFullyCompletedOnly || it->Status == HIVE_STATUS_BUILT))
		{
			Result++;
		}
	}

	return Result;
}

AvHMessageID UTIL_StructureTypeToImpulseCommand(const AvHAIDeployableStructureType StructureType)
{
	switch (StructureType)
	{
	case STRUCTURE_MARINE_ARMOURY:
		return BUILD_ARMORY;
	case STRUCTURE_MARINE_ARMSLAB:
		return BUILD_ARMSLAB;
	case STRUCTURE_MARINE_COMMCHAIR:
		return BUILD_COMMANDSTATION;
	case STRUCTURE_MARINE_INFANTRYPORTAL:
		return BUILD_INFANTRYPORTAL;
	case STRUCTURE_MARINE_OBSERVATORY:
		return BUILD_OBSERVATORY;
	case STRUCTURE_MARINE_PHASEGATE:
		return BUILD_PHASEGATE;
	case STRUCTURE_MARINE_PROTOTYPELAB:
		return BUILD_PROTOTYPE_LAB;
	case STRUCTURE_MARINE_RESTOWER:
		return BUILD_RESOURCES;
	case STRUCTURE_MARINE_SIEGETURRET:
		return BUILD_SIEGE;
	case STRUCTURE_MARINE_TURRET:
		return BUILD_TURRET;
	case STRUCTURE_MARINE_TURRETFACTORY:
		return BUILD_TURRET_FACTORY;

	case STRUCTURE_ALIEN_DEFENCECHAMBER:
		return ALIEN_BUILD_DEFENSE_CHAMBER;
	case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
		return ALIEN_BUILD_MOVEMENT_CHAMBER;
	case STRUCTURE_ALIEN_SENSORYCHAMBER:
		return ALIEN_BUILD_SENSORY_CHAMBER;
	case STRUCTURE_ALIEN_OFFENCECHAMBER:
		return ALIEN_BUILD_OFFENSE_CHAMBER;
	case STRUCTURE_ALIEN_RESTOWER:
		return ALIEN_BUILD_RESOURCES;
	case STRUCTURE_ALIEN_HIVE:
		return ALIEN_BUILD_HIVE;
	default:
		return MESSAGE_NULL;

	}

	return MESSAGE_NULL;
}

AvHMessageID UTIL_ItemTypeToImpulseCommand(const AvHAIDeployableItemType ItemType)
{
	switch (ItemType)
	{
							
		case DEPLOYABLE_ITEM_HEAVYARMOUR:
			return BUILD_HEAVY;
		case DEPLOYABLE_ITEM_JETPACK:
			return BUILD_JETPACK;
		case DEPLOYABLE_ITEM_CATALYSTS:
			return BUILD_CAT;
		case DEPLOYABLE_ITEM_SCAN:
			return BUILD_SCAN;
		case DEPLOYABLE_ITEM_HEALTHPACK:
			return BUILD_HEALTH;
		case DEPLOYABLE_ITEM_AMMO:
			return BUILD_AMMO;
		case DEPLOYABLE_ITEM_MINES:
			return BUILD_MINES;
		case DEPLOYABLE_ITEM_WELDER:
			return BUILD_WELDER;
		case DEPLOYABLE_ITEM_SHOTGUN:
			return BUILD_SHOTGUN;
		case DEPLOYABLE_ITEM_HMG:
			return BUILD_HMG;
		case DEPLOYABLE_ITEM_GRENADELAUNCHER:
			return BUILD_GRENADE_GUN;
		default:
			return MESSAGE_NULL;

	}

	return MESSAGE_NULL;
}

edict_t* AITAC_GetClosestPlayerOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer)
{
	float distSq = sqrf(SearchRadius);
	float MinDist = 0.0f;
	edict_t* Result = nullptr;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict != IgnorePlayer && PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq && UTIL_QuickTrace(PlayerEdict, GetPlayerEyePosition(PlayerEdict), Location))
			{
				if (FNullEnt(Result) || ThisDist < MinDist)
				{
					Result = PlayerEdict;
					MinDist = ThisDist;
				}

			}
		}
	}

	return Result;
}

bool AITAC_AnyPlayerOnTeamHasLOSToLocation(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer)
{
	float distSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict != IgnorePlayer && PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq && UTIL_QuickTrace(PlayerEdict, GetPlayerEyePosition(PlayerEdict), Location))
			{
				return true;
			}
		}
	}

	return false;
}

vector<AvHPlayer*> AITAC_GetAllPlayersOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer)
{
	vector<AvHPlayer*> Results;

	float distSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict != IgnorePlayer && PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq && UTIL_QuickTrace(PlayerEdict, GetPlayerEyePosition(PlayerEdict), Location))
			{
				AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

				if (PlayerRef)
				{
					Results.push_back(PlayerRef);
				}
			}
		}
	}

	return Results;
}

int AITAC_GetNumPlayersOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer)
{
	int Result = 0;

	float distSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict != IgnorePlayer && PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq && UTIL_QuickTrace(PlayerEdict, GetPlayerEyePosition(PlayerEdict), Location))
			{
				Result++;
			}
		}
	}

	return Result;
}

bool AITAC_ShouldBotBeCautious(AvHAIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size()) { return false; }

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	if (CurrentPathNode.area != SAMPLE_POLYAREA_GROUND) { return false; }

	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());

	if (AITAC_AnyPlayerOnTeamHasLOSToLocation(EnemyTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(50.0f), nullptr)) { return false; }

	int NumEnemiesAtDestination = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, CurrentPathNode.Location, UTIL_MetresToGoldSrcUnits(50.0f), pBot->Edict);

	if (NumEnemiesAtDestination <= 1)
	{
		DeployableSearchFilter TurretFilter;
		TurretFilter.DeployableTeam = EnemyTeam;
		TurretFilter.DeployableTypes = (STRUCTURE_MARINE_TURRET | STRUCTURE_ALIEN_OFFENCECHAMBER);
		TurretFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		TurretFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		TurretFilter.MaxSearchRadius = BALANCE_VAR(kTurretRange);

		vector<AvHAIBuildableStructure> Turrets = AITAC_FindAllDeployables(CurrentPathNode.Location, &TurretFilter);

		for (auto it = Turrets.begin(); it != Turrets.end(); it++)
		{
			if (UTIL_QuickTrace(pBot->Edict, GetPlayerTopOfCollisionHull(it->edict), CurrentPathNode.Location))
			{
				NumEnemiesAtDestination++;
			}
		}

	}

	if (NumEnemiesAtDestination > 1)
	{
		return (vDist2DSq(pBot->Edict->v.origin, CurrentPathNode.Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));
	}

	return false;
}

edict_t* AITAC_GetCommChair(AvHTeamNumber Team)
{
	// Invalid team, or team is alien and don't have a comm chair
	if (!GetGameRules()->GetTeam(Team) || GetGameRules()->GetTeam(Team)->GetTeamType() != AVH_CLASS_TYPE_MARINE) { return nullptr; }

	DeployableSearchFilter ChairFilter;
	ChairFilter.DeployableTypes = STRUCTURE_MARINE_COMMCHAIR;
	ChairFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	ChairFilter.DeployableTeam = Team;
	ChairFilter.ReachabilityTeam = TEAM_IND;

	vector<AvHAIBuildableStructure> CommChairs = AITAC_FindAllDeployables(ZERO_VECTOR, &ChairFilter);

	edict_t* MainCommChair = nullptr;
	edict_t* BackupChair = nullptr;

	// If the team has more than one comm chair, pick the one in use
	for (auto it = CommChairs.begin(); it != CommChairs.end(); it++)
	{
		AvHCommandStation* ChairRef = dynamic_cast<AvHCommandStation*>((*it).EntityRef);

		// Idle animation will be 3 if the chair is in use (closed animation). See AvHCommandStation::GetIdleAnimation
		if (ChairRef && ChairRef->GetIdleAnimation() == 3)
		{
			MainCommChair = ChairRef->edict();
		}
		else
		{
			BackupChair = ChairRef->edict();
		}
	}

	if (!FNullEnt(MainCommChair)) { return MainCommChair;}

	return BackupChair;
}

edict_t* AITAC_GetNearestHumanAtLocation(const AvHTeamNumber Team, const Vector Location, const float MaxSearchRadius)
{
	edict_t* Result = nullptr;

	float distSq = sqrf(MaxSearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict->v.team == Team && !(PlayerEdict->v.flags & FL_FAKECLIENT) && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq)
			{
				Result = PlayerEdict;
			}
		}
	}

	return Result;
}

AvHAIDeployableStructureType UTIL_GetChamberTypeForHiveTech(AvHMessageID HiveTech)
{
	switch (HiveTech)
	{
		case ALIEN_BUILD_DEFENSE_CHAMBER:
			return STRUCTURE_ALIEN_DEFENCECHAMBER;
		case ALIEN_BUILD_MOVEMENT_CHAMBER:
			return STRUCTURE_ALIEN_MOVEMENTCHAMBER;
		case ALIEN_BUILD_SENSORY_CHAMBER:
			return STRUCTURE_ALIEN_SENSORYCHAMBER;
		default:
			return STRUCTURE_NONE;
	}

	return STRUCTURE_NONE;
}

bool AITAC_ResearchIsComplete(const AvHTeamNumber Team, const AvHTechID Research)
{
	AvHTeam* TeamRef = GetGameRules()->GetTeam(Team);

	if (!TeamRef) { return false; }

	AvHResearchManager ResearchManager = TeamRef->GetResearchManager();

	return ResearchManager.GetTechNodes().GetIsTechResearched(Research);
}

bool AITAC_PhaseGatesAvailable(const AvHTeamNumber Team)
{
	DeployableSearchFilter ObsFilter;
	ObsFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	ObsFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	ObsFilter.DeployableTypes = STRUCTURE_MARINE_OBSERVATORY;
	ObsFilter.DeployableTeam = Team;

	bool bObsExists = AITAC_DeployableExistsAtLocation(AITAC_GetTeamStartingLocation(Team), &ObsFilter);

	return bObsExists && AITAC_ResearchIsComplete(Team, TECH_RESEARCH_PHASETECH);
}

int AITAC_GetNumDeadPlayersOnTeam(const AvHTeamNumber Team)
{
	AvHTeam* TeamRef = GetGameRules()->GetTeam(Team);

	if (!TeamRef) { return 0; }

	return TeamRef->GetPlayerCount(true);
}

bool AITAC_StructureCanBeUpgraded(edict_t* Structure)
{
	// We can't upgrade a structure if it's not built, destroyed, or already doing something
	if (FNullEnt(Structure)
		|| Structure->v.deadflag != DEAD_NO
		|| !UTIL_StructureIsFullyBuilt(Structure)
		|| UTIL_StructureIsRecycling(Structure)
		|| UTIL_StructureIsResearching(Structure)
		|| UTIL_StructureIsUpgrading(Structure))
	{
		return false;
	}

	return (GetStructureTypeFromEdict(Structure) == STRUCTURE_MARINE_ARMOURY || GetStructureTypeFromEdict(Structure) == STRUCTURE_MARINE_TURRETFACTORY);
}

const AvHAIHiveDefinition* AITAC_GetNearestHiveUnderActiveSiege(AvHTeamNumber SiegingTeam, const Vector SearchLocation)
{
	AvHAIHiveDefinition* Result = nullptr;
	float MinDist = 0.0f;

	// Only marines can siege, so return nothing if the enemy are not marines
	if (AIMGR_GetTeamType(SiegingTeam) != AVH_CLASS_TYPE_MARINE) { return nullptr; }

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		if (it->Status == HIVE_STATUS_UNBUILT) { continue; }

		DeployableSearchFilter SiegeFilter;
		SiegeFilter.DeployableTypes = STRUCTURE_MARINE_ADVTURRETFACTORY;
		SiegeFilter.DeployableTeam = SiegingTeam;
		SiegeFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(25.0f);
		SiegeFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		SiegeFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure AdvTF = AITAC_FindClosestDeployableToLocation(it->Location, &SiegeFilter);

		if (!AdvTF.IsValid()) { continue; }

		SiegeFilter.DeployableTypes = STRUCTURE_MARINE_SIEGETURRET;
		SiegeFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

		bool bSTExists = AITAC_DeployableExistsAtLocation(AdvTF.Location, &SiegeFilter);

		if (bSTExists)
		{
			float ThisDist = vDist2DSq(SearchLocation, it->Location);

			if (!Result || ThisDist < MinDist)
			{
				Result = &(*it);
				MinDist = ThisDist;
			}
		}

	}

	return Result;
}

edict_t* AITAC_GetMarineEligibleToBuildSiege(AvHTeamNumber Team, const AvHAIHiveDefinition* Hive)
{
	if (!Hive) { return nullptr; }

	edict_t* Result = nullptr;

	vector<AvHPlayer*> TeamPlayers = AIMGR_GetAllPlayersOnTeam(Team);

	float MinDist = 0.0f;

	for (auto it = TeamPlayers.begin(); it != TeamPlayers.end(); it++)
	{
		edict_t* PlayerEdict = (*it)->edict();

		// Find a player who is alive and not currently sighted by the enemy
		if (!IsPlayerActiveInGame(PlayerEdict) || (PlayerEdict->v.iuser4 & MASK_VIS_SIGHTED)) { continue; }

		float ThisDist = vDist2DSq(PlayerEdict->v.origin, Hive->Location);

		if (ThisDist < sqrf(UTIL_MetresToGoldSrcUnits(25.0f)) && !UTIL_QuickTrace(PlayerEdict, GetPlayerEyePosition(PlayerEdict), Hive->Location))
		{
			if (FNullEnt(Result) || ThisDist < MinDist)
			{
				Result = PlayerEdict;
				MinDist = ThisDist;
			}
		}
	}

	return Result;
}

edict_t* AITAC_GetNearestHiddenPlayerInLocation(AvHTeamNumber Team, const Vector Location, const float MaxRadius)
{
	edict_t* Result = nullptr;
	float MaxRadiusSq = sqrf(MaxRadius);

	vector<AvHPlayer*> TeamPlayers = AIMGR_GetAllPlayersOnTeam(Team);

	float MinDist = 0.0f;

	for (auto it = TeamPlayers.begin(); it != TeamPlayers.end(); it++)
	{
		edict_t* PlayerEdict = (*it)->edict();

		// Find a player who is alive and not currently sighted by the enemy
		if (!IsPlayerActiveInGame(PlayerEdict) || (PlayerEdict->v.iuser4 & MASK_VIS_SIGHTED)) { continue; }

		float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

		if (ThisDist < MaxRadiusSq)
		{
			if (FNullEnt(Result) || ThisDist < MinDist)
			{
				Result = PlayerEdict;
				MinDist = ThisDist;
			}
		}
	}

	return Result;
}

const vector<AvHAIResourceNode*> AITAC_GetAllReachableResourceNodes(AvHTeamNumber Team)
{
	vector<AvHAIResourceNode*> Results;

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{		
		unsigned int CheckReachabilityFlags = (Team == GetGameRules()->GetTeamANumber()) ? it->TeamAReachabilityFlags : it->TeamBReachabilityFlags;

		if (CheckReachabilityFlags != AI_REACHABILITY_UNREACHABLE && CheckReachabilityFlags != AI_REACHABILITY_NONE)
		{
			Results.push_back(&(*it));
		}			
	}

	return Results;
}

const vector<AvHAIResourceNode*> AITAC_GetAllResourceNodes()
{
	vector<AvHAIResourceNode*> Results;

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		Results.push_back(&(*it));
	}

	return Results;
}

const vector<AvHAIHiveDefinition*> AITAC_GetAllHives()
{
	vector<AvHAIHiveDefinition*> Results;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		Results.push_back(&(*it));
	}

	return Results;
}

const AvHAIHiveDefinition* AITAC_GetNearestTeamHive(AvHTeamNumber Team, const Vector SearchLocation, bool bFullyBuiltOnly)
{
	AvHAIHiveDefinition* Result = nullptr;
	float MinDist = 0.0f;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		if (it->OwningTeam == Team && (!bFullyBuiltOnly || it->Status == HIVE_STATUS_BUILT))
		{
			float ThisDist = vDist2DSq(it->FloorLocation, SearchLocation);

			if (!Result || ThisDist < MinDist)
			{
				Result = &(*it);
				MinDist = ThisDist;
			}
		}
	}

	return Result;
}

const vector<AvHAIHiveDefinition*> AITAC_GetAllTeamHives(AvHTeamNumber Team, bool bFullyBuiltOnly)
{
	vector<AvHAIHiveDefinition*> Results;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		if (it->OwningTeam == Team && (!bFullyBuiltOnly || it->Status == HIVE_STATUS_BUILT))
		{
			Results.push_back(&(*it));
		}
	}

	return Results;
}

bool AITAC_AnyPlayerOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius)
{
	float distSq = sqrf(SearchRadius);

	vector<AvHPlayer*> Players = AIMGR_GetAllPlayersOnTeam(Team);

	for (auto it = Players.begin(); it != Players.end(); it++)
	{
		edict_t* PlayerRef = (*it)->edict();

		if (!IsPlayerActiveInGame(PlayerRef)) { continue; }

		if (vDist2DSq(PlayerRef->v.origin, Location) <= distSq && UTIL_QuickTrace(PlayerRef, GetPlayerEyePosition(PlayerRef), Location))
		{
			return true;
		}
	}

	return false;
}

bool AITAC_IsAlienHarasserNeeded(AvHAIPlayer* pBot)
{
	if (IsPlayerLerk(pBot->Edict)) { return true; }

	if (!CONFIG_IsLerkAllowed()) { return false; }

	// Don't downgrade to lerk if already a fade or onos!
	if (IsPlayerFade(pBot->Edict) || IsPlayerOnos(pBot->Edict)) { return false; }

	if (pBot->Player->GetResources() < BALANCE_VAR(kLerkCost)) { return false; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	if (pBot->BotRole == BOT_ROLE_ASSAULT && pBot->Player->GetResources() > (BALANCE_VAR(kFadeCost) * 0.8f))
	{
		int NumFades = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER4, pBot->Edict);
		int NumOnos = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER5, pBot->Edict);

		int NumTeamHives = AITAC_GetNumTeamHives(BotTeam, false);

		if (NumFades == 0 || (NumOnos == 0 && NumTeamHives > 1)) { return false; }
	}	

	int NumTeamPlayers = AIMGR_GetNumPlayersOnTeam(BotTeam);
	int DesiredLerks = (int)ceilf((float)NumTeamPlayers * 0.1f);
	int NumLerks = imaxi(AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER3, nullptr), AIMGR_GetNumAIPlayersWithRoleOnTeam(BotTeam, BOT_ROLE_HARASS, pBot));

	if (NumLerks < DesiredLerks)
	{
		if (DesiredLerks > 1) { return true; }

		float LastSeenTime;
		edict_t* PreviousLerk = AITAC_GetLastSeenLerkForTeam(BotTeam, LastSeenTime);

		// We only go lerk if the last lerk we had in the match was either us, or we've not had another lerk in 60 seconds
		// It avoids aliens spending all their resources on evolving lerks if they keep dying
		// It also means that if a human was playing lerk and died, a bot doesn't immediately take over that role, and lets the human try again if they want


		return (FNullEnt(PreviousLerk) || (gpGlobals->time - LastSeenTime > CONFIG_GetLerkCooldown()) || PreviousLerk == pBot->Edict);
	}

	return false;
}

bool AITAC_ShouldBotBuildHive(AvHAIPlayer* pBot, AvHAIHiveDefinition** EligibleHive)
{
	*EligibleHive = nullptr;

	float HiveCost = BALANCE_VAR(kHiveCost);

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		HiveCost += BALANCE_VAR(kGorgeCost);
	}

	if (pBot->Player->GetResources() < HiveCost) { return false; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	// Prioritise getting at least one fade or Onos on the team before putting up a second hive, or we're likely to lose it pretty quickly
	int NumHeavyHitters = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER4, nullptr) + AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER5, nullptr);

	if (NumHeavyHitters == 0) { return false; }

	// If we're a higher lifeform, ensure we can't leave this to someone else before considering losing those resources
	// We will ignore humans and third party bots, because we don't know if they will drop the hive or not. Not everyone can be as team-spirited as us...
	if (!IsPlayerSkulk(pBot->Edict) && GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		vector<AvHAIPlayer*> OtherAITeamMates = AIMGR_GetAIPlayersOnTeam(BotTeam);

		for (auto it = OtherAITeamMates.begin(); it != OtherAITeamMates.end(); it++)
		{
			AvHAIPlayer* OtherBot = (*it);

			// If the other bot has enough resources to drop a hive, and they're a less expensive life form than us, let them do it.
			if (OtherBot->Player->GetResources() >= BALANCE_VAR(kHiveCost) * 0.8f && OtherBot->Player->GetUser3() < pBot->Player->GetUser3()) { return false; }
		}
	}

	AvHAIHiveDefinition* SuitableHive = nullptr;
	float MinDist = 0.0f;

	vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllHives();

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		AvHAIHiveDefinition* ThisHive = (*it);

		if (ThisHive->Status == HIVE_STATUS_BUILT) { continue; }

		if (ThisHive->Status == HIVE_STATUS_BUILDING)
		{
			// Aliens can only build one hive at a time, so if we have one already under construction then automatic no
			if (ThisHive->OwningTeam == BotTeam)
			{
				return false; 
			}
			else
			{
				// It's an enemy hive under construction, so we can't build this one but can keep searching
				continue;
			}
		}

		// Check to make sure someone else isn't planning to drop a hive, otherwise don't bother
		bool bHasOtherBuilder = false;
		vector<edict_t*> OtherBuilders = AITAC_GetAllPlayersOfClassInArea(BotTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

		for (auto BuildIt = OtherBuilders.begin(); BuildIt != OtherBuilders.end(); BuildIt++)
		{
			edict_t* OtherBuilder = (*BuildIt);

			if (vDist2DSq(OtherBuilder->v.origin, ThisHive->FloorLocation) && GetPlayerResources(OtherBuilder) >= BALANCE_VAR(kHiveCost) * 0.8f) { bHasOtherBuilder = true; }
		}

		if (bHasOtherBuilder) { return false; }

		// Enemy are in here right now, wait until they're cleared out
		if (AITAC_GetNumPlayersOfTeamInArea(EnemyTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_COMMANDER_PLAYER) > 2) { continue; }

		// Must be an empty hive
		DeployableSearchFilter EnemyFortificationsFilter;
		EnemyFortificationsFilter.DeployableTeam = EnemyTeam;		

		if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE)
		{
			EnemyFortificationsFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
			EnemyFortificationsFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_SIEGETURRET);
			EnemyFortificationsFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			EnemyFortificationsFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED; // This is important to prevent exploiting the AI. Those structures have to be built first!
		}
		else
		{
			EnemyFortificationsFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
			EnemyFortificationsFilter.DeployableTypes = (STRUCTURE_ALIEN_OFFENCECHAMBER);
		}

		// Enemy have built some stuff, wait until it's clear before building
		if (AITAC_DeployableExistsAtLocation(ThisHive->FloorLocation, &EnemyFortificationsFilter)) { continue; }


		// Should be clear to drop dat hive!

		float ThisDist = vDist2DSq(pBot->Edict->v.origin, ThisHive->FloorLocation);

		if (!SuitableHive || ThisDist < MinDist)
		{
			SuitableHive = ThisHive;
			MinDist = ThisDist;
		}

	}

	*EligibleHive = SuitableHive;

	return (SuitableHive != nullptr);
}

bool AITAC_IsAlienCapperNeeded(AvHAIPlayer* pBot)
{
	// Ok so this logic is fairly involved:
	// A node is eligible if it is reachable, and not in the enemy base (either marine spawn or a live enemy hive).
	// We set a max acceptable % of team allowed to be capping nodes at one time (default 50%).
	// The number of desired cappers is expressed as an inverse function of how many nodes we own, so the more nodes we own = the smaller % of team should be capping
	// Max desired cappers is also limited by how many available nodes there are so we can't end up with more cappers than available nodes

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	int NumOwnedNodes = 0;
	int NumEnemyNodes = 0;
	int NumEligibleNodes = 0;

	// First get ours and the enemy's ownership of all eligible nodes (we can reach them, and they're in the enemy base)
	vector<AvHAIResourceNode*> AllNodes = AITAC_GetAllReachableResourceNodes(BotTeam);

	for (auto it = AllNodes.begin(); it != AllNodes.end(); it++)
	{
		AvHAIResourceNode* ThisNode = (*it);

		// We don't care about the node at marine spawn or enemy hives, ignore then in our calculations
		if (ThisNode->OwningTeam == EnemyTeam && ThisNode->bIsBaseNode) { continue; }

		NumEligibleNodes++;

		if (ThisNode->OwningTeam == BotTeam) { NumOwnedNodes++; }
		if (ThisNode->OwningTeam == EnemyTeam) { NumEnemyNodes++; }
	}

	// If we are currently an assault bot and we are almost able to go fade, only sacrifice that if we are truly desperate and have fades already on the team
	if (pBot->BotRole == BOT_ROLE_ASSAULT)
	{
		if (pBot->Player->GetResources() > BALANCE_VAR(kFadeCost) * 0.8f)
		{
			if (NumOwnedNodes >= 3) { return false; }

			int NumFades = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER4, pBot->Edict);

			if (NumFades < 1) { return false; }
		}
	}

	int NumNodesLeft = NumEligibleNodes - NumOwnedNodes;

	if (NumNodesLeft == 0) { return false; }

	float OurNodeOwnership = (float)NumOwnedNodes / (float)NumEligibleNodes;
	float EnemyNodeOwnership = (float)NumEnemyNodes / (float)NumEligibleNodes;

	int NumTeamPlayers = AIMGR_GetNumPlayersOnTeam(BotTeam);
	float MaxCapperPercent = 0.5f;

	int NumCurrentCappers = AIMGR_GetNumAIPlayersWithRoleOnTeam(BotTeam, BOT_ROLE_FIND_RESOURCES, pBot);

	float DesiredCapperPercent = MaxCapperPercent * (1.0f - OurNodeOwnership);
	
	int DesiredCappers = imini(NumNodesLeft, (int)roundf(DesiredCapperPercent * (float)NumTeamPlayers));

	if (NumCurrentCappers >= DesiredCappers) { return false; }

	int CapperDeficit = DesiredCappers - NumCurrentCappers;

	// Ok, we have established that we need cappers, but let's see if WE should be capping

	float ResourcesNeeded = BALANCE_VAR(kResourceTowerCost);

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		ResourcesNeeded += BALANCE_VAR(kGorgeCost);
	}

	if (pBot->Player->GetResources() < ResourcesNeeded)
	{
		if (EnemyNodeOwnership < 0.35f) { return false; }
	}

	bool bIsHigherLifeform = (!IsPlayerSkulk(pBot->Edict) && GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2);
	bool bIsBuilder = (GetPlayerActiveClass(pBot->Player) == AVH_USER3_ALIEN_PLAYER2 && pBot->BotRole == BOT_ROLE_BUILDER);

	if (bIsHigherLifeform || bIsBuilder)
	{
		if (OurNodeOwnership > 0.35f) { return false; }

		int NumAlternativeCandidates = 0;

		vector<AvHPlayer*> CandidateTeamMates = AIMGR_GetNonAIPlayersOnTeam(BotTeam);
		vector<AvHAIPlayer*> AITeamMates = AIMGR_GetAIPlayersOnTeam(BotTeam);

		for (auto it = AITeamMates.begin(); it != AITeamMates.end(); it++)
		{
			AvHAIPlayer* ThisBot = (*it);

			if (ThisBot == pBot) { continue; }

			if (ThisBot->BotRole != BOT_ROLE_FIND_RESOURCES)
			{
				CandidateTeamMates.push_back(ThisBot->Player);
			}
		}

		for (auto it = CandidateTeamMates.begin(); it != CandidateTeamMates.end(); it++)
		{
			AvHPlayer* ThisPlayer = (*it);

			if (ThisPlayer == pBot->Player) { continue; }

			if (ThisPlayer->GetUser3() < pBot->Player->GetUser3() || (ThisPlayer->GetUser3() == pBot->Player->GetUser3() && ThisPlayer->GetResources() > pBot->Player->GetResources()))
			{
				NumAlternativeCandidates++;
			}

			if (NumAlternativeCandidates >= CapperDeficit) { return false; }

		}
	}

	// Nobody better to do the job
	return true;
}

bool AITAC_IsAlienBuilderNeeded(AvHAIPlayer* pBot)
{
	// The basic logic here is that we have a max number of builders based on team size.
	// We add one extra required builder for every empty hive that needs fortifications placed
	// One for any missing upgrade chambers
	// and one if there are any resource towers that need reinforcing which aren't part of a hive
	// This should mean that we could have up to 3 builders at the start if the team is big enough, then that should reduce down to just 1 eventually

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	float ResNodeOwnership = AITAC_GetTeamResNodeOwnership(BotTeam, true);

	// Don't lose all those resources if we're a higher lifeform!
	if (pBot->Player->GetUser3() > AVH_USER3_ALIEN_PLAYER2)
	{
		if (IsPlayerLerk(pBot->Edict)) { return false; }

		int NumFades = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER4, pBot->Edict);
		int NumOnos = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER5, pBot->Edict);

		// Don't downgrade to gorge if we're the only fade or onos on the team
		if ((IsPlayerFade(pBot->Edict) && NumFades == 0) || (IsPlayerOnos(pBot->Edict) && NumOnos == 0)) { return false; }

		// Only waste those resources if we have loads of res nodes and can easily replenish the lost resources
		if (pBot->Player->GetResources() < 75 || ResNodeOwnership < 0.6f)
		{
			return false;
		}
	}

	AvHMessageID HiveTechOne = CONFIG_GetHiveTechAtIndex(0);
	AvHMessageID HiveTechTwo = CONFIG_GetHiveTechAtIndex(1);
	AvHMessageID HiveTechThree = CONFIG_GetHiveTechAtIndex(2);

	AvHAIDeployableStructureType ChamberTypeOne = UTIL_GetChamberTypeForHiveTech(HiveTechOne);
	AvHAIDeployableStructureType ChamberTypeTwo = UTIL_GetChamberTypeForHiveTech(HiveTechTwo);
	AvHAIDeployableStructureType ChamberTypeThree = UTIL_GetChamberTypeForHiveTech(HiveTechThree);

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTeam = BotTeam;

	int NumTeamPlayers = AIMGR_GetNumPlayersOnTeam(BotTeam);
	int MaxBuilders = (int)ceilf((float)NumTeamPlayers * 0.3f); // Max 1/3 of team can be builder at any one time

	// If we're struggling for resources, then cut back on max number of builders to have more focused on capping nodes
	if (ResNodeOwnership < 0.25f)
	{
		MaxBuilders = (imaxi(1, MaxBuilders - 1));
	}

	// Don't build if we're a higher lifeform and there are others who could do the job instead
	if (!IsPlayerSkulk(pBot->Edict) && GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		int NumSkulks = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER1, nullptr);
		int NumGorges = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER2, nullptr);
		int NumDead = AITAC_GetNumDeadPlayersOnTeam(BotTeam);

		if ((NumSkulks + NumGorges + NumDead) > MaxBuilders) { return false; }
	}


	int NumCurrentBuilders = AIMGR_GetNumAIPlayersWithRoleOnTeam(BotTeam, BOT_ROLE_BUILDER, pBot);

	vector<AvHPlayer*> NonAIPlayers = AIMGR_GetNonAIPlayersOnTeam(BotTeam);

	for (auto it = NonAIPlayers.begin(); it != NonAIPlayers.end(); it++)
	{
		AvHPlayer* ThisPlayer = (*it);

		if (GetPlayerActiveClass(ThisPlayer) == AVH_USER3_ALIEN_PLAYER2)
		{
			NumCurrentBuilders++;
		}
	}

	// We already have too many gorges running around
	if (NumCurrentBuilders >= MaxBuilders) { return false; }

	int DesiredBuilders = 0;

	vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllHives();

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		const AvHAIHiveDefinition* ThisHive = (*it);

		// Can't build in an enemy hive (if playing AvA)
		if (ThisHive->OwningTeam == EnemyTeam) { continue; }

		if (ThisHive->OwningTeam == BotTeam)
		{
			if (ThisHive->Status == HIVE_STATUS_BUILT)
			{
				// Hive hasn't got a chamber assigned yet, we need at least one builder for that
				if (ThisHive->TechStatus == MESSAGE_NULL)
				{
					DesiredBuilders++;

					if (DesiredBuilders >= MaxBuilders)
					{
						return NumCurrentBuilders < DesiredBuilders;
					}
				}

				continue;
			}
		}
			
		bool bEnemyIsMarines = (AIMGR_GetEnemyTeamType(BotTeam) == AVH_CLASS_TYPE_MARINE);

		DeployableSearchFilter EnemyStructures;
		EnemyStructures.DeployableTeam = EnemyTeam;
		EnemyStructures.ReachabilityTeam = EnemyTeam;
		EnemyStructures.ReachabilityFlags = (bEnemyIsMarines) ? AI_REACHABILITY_MARINE : AI_REACHABILITY_SKULK;
		EnemyStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		if (bEnemyIsMarines)
		{
			EnemyStructures.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			EnemyStructures.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			EnemyStructures.DeployableTypes = STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY;
		}
		else
		{
			EnemyStructures.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;
		}

		// Enemy have a foothold here, don't get involved
		if (AITAC_GetNumDeployablesNearLocation(ThisHive->Location, &EnemyStructures) > 0)
		{
			continue;
		}

		// The enemy don't have a proper foothold yet

		DeployableSearchFilter ExistingReinforcementFilter;
		ExistingReinforcementFilter.DeployableTeam = BotTeam;
		ExistingReinforcementFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
		ExistingReinforcementFilter.DeployableTypes = SEARCH_ALL_ALIEN_STRUCTURES;

		vector<AvHAIBuildableStructure> AllReinforcingStructures = AITAC_FindAllDeployables(ThisHive->FloorLocation, &ExistingReinforcementFilter);
			
		int NumOCs = 0;
		int NumDCs = 0;
		int NumMCs = 0;
		int NumSCs = 0;

		for (auto it = AllReinforcingStructures.begin(); it != AllReinforcingStructures.end(); it++)
		{
			switch ((*it).StructureType)
			{
				case STRUCTURE_ALIEN_OFFENCECHAMBER:
					NumOCs++;
					break;
				case STRUCTURE_ALIEN_DEFENCECHAMBER:
					NumDCs++;
					break;
				case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
					NumMCs++;
					break;
				case STRUCTURE_ALIEN_SENSORYCHAMBER:
					NumSCs++;
					break;
				default:
					break;
			}
		}

		if (NumOCs < 3
			|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_DEFENSE_CHAMBER) && NumDCs < 2)
			|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_MOVEMENT_CHAMBER) && NumMCs < 1)
			|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_SENSORY_CHAMBER) && NumSCs < 1))
		{
			DesiredBuilders++;

			if (DesiredBuilders >= MaxBuilders)
			{
				return NumCurrentBuilders < DesiredBuilders;
			}
		}
		
	}

	// We have hives to fortify and upgrades to enable. Ignore resource nodes for now. We will get a bot assigned to those once we've done everything else
	if (DesiredBuilders > 0)
	{
		return NumCurrentBuilders < DesiredBuilders;
	}
	
	DeployableSearchFilter ResNodeFilter;
	ResNodeFilter.DeployableTeam = BotTeam;
	ResNodeFilter.ReachabilityTeam = BotTeam;
	ResNodeFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

	vector <AvHAIResourceNode*> OwnedNodes = AITAC_GetAllMatchingResourceNodes(pBot->Edict->v.origin, &ResNodeFilter);

	for (auto it = OwnedNodes.begin(); it != OwnedNodes.end(); it++)
	{
		AvHAIResourceNode* ThisNode = (*it);

		if (ThisNode->bIsBaseNode && !FNullEnt(ThisNode->ParentHive))
		{
			AvHAIHiveDefinition* HiveRef = AITAC_GetHiveFromEdict(ThisNode->ParentHive);
			if (HiveRef && HiveRef->Status != HIVE_STATUS_UNBUILT)
			{
				continue;
			}
		}

		DeployableSearchFilter ExistingReinforcementFilter;
		ExistingReinforcementFilter.DeployableTeam = BotTeam;
		ExistingReinforcementFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);
		ExistingReinforcementFilter.DeployableTypes = SEARCH_ALL_ALIEN_STRUCTURES;

		vector<AvHAIBuildableStructure> AllReinforcingStructures = AITAC_FindAllDeployables(ThisNode->Location, &ExistingReinforcementFilter);

		int NumOCs = 0;
		int NumDCs = 0;
		int NumMCs = 0;
		int NumSCs = 0;

		for (auto it = AllReinforcingStructures.begin(); it != AllReinforcingStructures.end(); it++)
		{
			switch ((*it).StructureType)
			{
			case STRUCTURE_ALIEN_OFFENCECHAMBER:
				NumOCs++;
				break;
			case STRUCTURE_ALIEN_DEFENCECHAMBER:
				NumDCs++;
				break;
			case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
				NumMCs++;
				break;
			case STRUCTURE_ALIEN_SENSORYCHAMBER:
				NumSCs++;
				break;
			default:
				break;
			}
		}

		if (NumOCs < 2
			|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_DEFENSE_CHAMBER) && NumDCs < 2)
			|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_MOVEMENT_CHAMBER) && NumDCs < 1)
			|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_SENSORY_CHAMBER) && NumDCs < 1))
		{
			DesiredBuilders++;

			if (DesiredBuilders >= MaxBuilders)
			{
				return NumCurrentBuilders < DesiredBuilders;
			}

			break;
		}

	}

	return NumCurrentBuilders < DesiredBuilders;

}

AvHAIDeployableStructureType AITAC_GetNextMissingUpgradeChamberForTeam(AvHTeamNumber Team, int& NumMissing)
{
	if (AIMGR_GetTeamType(Team) != AVH_CLASS_TYPE_ALIEN) { return STRUCTURE_NONE; }

	AvHMessageID HiveTechOne = CONFIG_GetHiveTechAtIndex(0);
	AvHMessageID HiveTechTwo = CONFIG_GetHiveTechAtIndex(1);
	AvHMessageID HiveTechThree = CONFIG_GetHiveTechAtIndex(2);

	AvHAIDeployableStructureType ChamberTypeOne = UTIL_GetChamberTypeForHiveTech(HiveTechOne);
	AvHAIDeployableStructureType ChamberTypeTwo = UTIL_GetChamberTypeForHiveTech(HiveTechTwo);
	AvHAIDeployableStructureType ChamberTypeThree = UTIL_GetChamberTypeForHiveTech(HiveTechThree);

	DeployableSearchFilter SearchFilter;
	SearchFilter.DeployableTeam = Team;

	bool bHasFreeHive = AITAC_TeamHiveWithTechExists(Team, MESSAGE_NULL);

	if (ChamberTypeOne != STRUCTURE_NONE && (bHasFreeHive || AITAC_TeamHiveWithTechExists(Team, HiveTechOne)))
	{
		SearchFilter.DeployableTypes = ChamberTypeOne;

		int NumChambers = AITAC_GetNumDeployablesNearLocation(AITAC_GetTeamStartingLocation(Team), &SearchFilter);

		if (NumChambers < 3)
		{
			NumMissing = 3 - NumChambers;
			return ChamberTypeOne;
		}
	}

	if (ChamberTypeTwo != STRUCTURE_NONE && (bHasFreeHive || AITAC_TeamHiveWithTechExists(Team, HiveTechTwo)))
	{
		SearchFilter.DeployableTypes = ChamberTypeTwo;

		int NumChambers = AITAC_GetNumDeployablesNearLocation(AITAC_GetTeamStartingLocation(Team), &SearchFilter);

		if (NumChambers < 3)
		{
			NumMissing = 3 - NumChambers;
			return ChamberTypeTwo;
		}
	}

	if (ChamberTypeThree != STRUCTURE_NONE && (bHasFreeHive || AITAC_TeamHiveWithTechExists(Team, HiveTechThree)))
	{
		SearchFilter.DeployableTypes = ChamberTypeThree;

		int NumChambers = AITAC_GetNumDeployablesNearLocation(AITAC_GetTeamStartingLocation(Team), &SearchFilter);

		if (NumChambers < 3)
		{
			NumMissing = 3 - NumChambers;
			return ChamberTypeThree;
		}
	}

	return STRUCTURE_NONE;
}

edict_t* AITAC_AlienFindNearestHealingSource(AvHTeamNumber Team, Vector SearchLocation, edict_t* SearchingPlayer, bool bIncludeGorges)
{
	edict_t* Result = nullptr;
	float MinDist = 0.0f;

	vector<AvHAIHiveDefinition*> AllTeamHives = AITAC_GetAllTeamHives(Team, true);

	for (auto it = AllTeamHives.begin(); it != AllTeamHives.end(); it++)
	{
		float ThisDist = vDist2DSq((*it)->Location, SearchLocation);
		// Factor healing radius into the distance checks, we don't have to be right at the hive to heal
		ThisDist -= BALANCE_VAR(kHiveHealRadius) * 0.75f;
		
		// We're already in healing distance of a hive, that's our healing source
		if (ThisDist <= 0.0f) { return (*it)->HiveEdict; }

		if (FNullEnt(Result) || ThisDist < MinDist)
		{
			Result = (*it)->HiveEdict;
			MinDist = ThisDist;
		}
	}

	DeployableSearchFilter DCFilter;
	DCFilter.DeployableTeam = Team;
	DCFilter.DeployableTypes = STRUCTURE_ALIEN_DEFENCECHAMBER;
	DCFilter.MaxSearchRadius = (!FNullEnt(Result)) ? MinDist : 0.0f; // We should always have a result, unless we have no hives left. That's our benchmark: only look for DCs closer than the hive

	vector<AvHAIBuildableStructure> AllDCs = AITAC_FindAllDeployables(SearchLocation, &DCFilter);

	for (auto it = AllDCs.begin(); it != AllDCs.end(); it++)
	{
		AvHAIBuildableStructure ThisDC = (*it);

		float ThisDist = vDist2DSq(ThisDC.Location, SearchLocation);
		// Factor healing radius into the distance checks, we don't have to be sat on top of the DC to heal
		ThisDist -= BALANCE_VAR(kHiveHealRadius) * 0.75f;

		// We're already in healing distance of a DC, that's our healing source
		if (ThisDist <= 0.0f) { return ThisDC.edict; }

		if (FNullEnt(Result) || ThisDist < MinDist)
		{
			Result = ThisDC.edict;
			MinDist = ThisDist;
		}
	}

	edict_t* FriendlyGorge = nullptr;

	if (bIncludeGorges)
	{
		float PlayerSearchDist = (!FNullEnt(Result)) ? MinDist : 0.0f; // As before, we only want players closer than our current "winner"
		FriendlyGorge = AITAC_GetNearestPlayerOfClassInArea(Team, SearchLocation, PlayerSearchDist, false, SearchingPlayer, AVH_USER3_ALIEN_PLAYER2);
	}

	return (!FNullEnt(FriendlyGorge) ? FriendlyGorge : Result);

}

bool AITAC_IsAlienUpgradeAvailableForTeam(AvHTeamNumber Team, HiveTechStatus DesiredTech)
{
	AvHAIDeployableStructureType SearchType;

	switch (DesiredTech)
	{
		case HIVE_TECH_DEFENCE:
			SearchType = STRUCTURE_ALIEN_DEFENCECHAMBER;
			break;
		case HIVE_TECH_MOVEMENT:
			SearchType = STRUCTURE_ALIEN_MOVEMENTCHAMBER;
			break;
		case HIVE_TECH_SENSORY:
			SearchType = STRUCTURE_ALIEN_SENSORYCHAMBER;
			break;
		default:
			return false;
	}

	DeployableSearchFilter ChamberFilter;
	ChamberFilter.DeployableTeam = Team;
	ChamberFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	ChamberFilter.DeployableTypes = SearchType;

	return (AITAC_DeployableExistsAtLocation(ZERO_VECTOR, &ChamberFilter));
}

int AITAC_GetNumWeaponsInPlay(AvHTeamNumber Team, AvHAIWeapon WeaponType)
{
	int Result = 0;

	vector<AvHPlayer*> PlayerList = AIMGR_GetAllPlayersOnTeam(Team);

	for (auto it = PlayerList.begin(); it != PlayerList.end(); it++)
	{
		AvHPlayer* PlayerRef = (*it);

		if (!PlayerRef) { continue; }

		edict_t* PlayerEdict = PlayerRef->edict();

		if (PlayerRef && !FNullEnt(PlayerEdict) && IsPlayerActiveInGame(PlayerEdict) && PlayerHasWeapon(PlayerRef, WeaponType))
		{
			Result++;
		}
	}


	for (auto it = MarineDroppedItemMap.begin(); it != MarineDroppedItemMap.end(); it++)
	{
		AvHAIWeapon ThisWeaponType = UTIL_GetWeaponTypeFromDroppedItem(it->second.ItemType);

		if (ThisWeaponType != WeaponType) { continue; }

		unsigned int ReachabilityFlags = (Team == TEAM_IND) ? (it->second.TeamAReachabilityFlags | it->second.TeamBReachabilityFlags) : ((Team == GetGameRules()->GetTeamANumber()) ? it->second.TeamAReachabilityFlags : it->second.TeamBReachabilityFlags);

		if (ReachabilityFlags != AI_REACHABILITY_UNREACHABLE)
		{
			DeployableSearchFilter ArmouryFilter;
			ArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
			ArmouryFilter.DeployableTeam = Team;
			ArmouryFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
			
			if (AITAC_DeployableExistsAtLocation(it->second.Location, &ArmouryFilter))
			{
				Result++;
			}
		}
	}

	return Result;
}

edict_t* AITAC_GetLastSeenLerkForTeam(AvHTeamNumber Team, float& LastSeenTime)
{
	if (Team == GetGameRules()->GetTeamANumber())
	{
		LastSeenTime = LastSeenLerkTeamATime;
		return LastSeenLerkTeamA;
	}
	else
	{
		LastSeenTime = LastSeenLerkTeamBTime;
		return LastSeenLerkTeamB;
	}
}

bool AITAC_IsCompletedStructureOfTypeNearLocation(AvHTeamNumber Team, unsigned int StructureType, Vector SearchLocation, float SearchRadius)
{
	DeployableSearchFilter SearchFilter;
	SearchFilter.DeployableTeam = Team;
	SearchFilter.DeployableTypes = StructureType;
	SearchFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	SearchFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	SearchFilter.MaxSearchRadius = SearchRadius;

	return AITAC_DeployableExistsAtLocation(SearchLocation, &SearchFilter);
}

bool AITAC_IsStructureOfTypeNearLocation(AvHTeamNumber Team, unsigned int StructureType, Vector SearchLocation, float SearchRadius)
{
	DeployableSearchFilter SearchFilter;
	SearchFilter.DeployableTeam = Team;
	SearchFilter.DeployableTypes = StructureType;
	SearchFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	SearchFilter.MaxSearchRadius = SearchRadius;

	return AITAC_DeployableExistsAtLocation(SearchLocation, &SearchFilter);
}

Vector AITAC_GetRandomBuildHintInLocation(const unsigned int StructureType, const Vector SearchLocation, const float SearchRadius)
{
	Vector Result = ZERO_VECTOR;

	vector<NavHint*> IPHints = NAV_GetHintsOfTypeInRadius(StructureType, SearchLocation, SearchRadius, true);
	int WinningRoll = 0;

	for (auto it = IPHints.begin(); it != IPHints.end(); it++)
	{
		NavHint* ThisHint = (*it);

		int ThisRoll = irandrange(0, 10);

		if (vIsZero(Result) || ThisRoll > WinningRoll)
		{
			Result = ThisHint->Position;
			WinningRoll = ThisRoll;
		}
	}

	return Result;
}

bool AITAC_IsBotPursuingSquadObjective(AvHAIPlayer* pBot, AvHAISquad* Squad)
{
	// Bot is dead, no longer playing, or otherwise incapacitated
	if (!IsPlayerActiveInGame(pBot->Edict) || pBot->Player->GetTeam() != Squad->SquadTeam) { return false; } 
	
	// Bot no longer has this squad's objective as its primary task
	if (pBot->PrimaryBotTask.TaskType != Squad->SquadObjective || pBot->PrimaryBotTask.TaskTarget != Squad->SquadTarget) { return false; }

	// Bot is focused on the job at hand
	if (!pBot->CurrentTask || pBot->CurrentTask == &pBot->PrimaryBotTask) { return true; }

	// Bot isn't currently pursuing squad objective, so check if it's doing something in the vicinity
	Vector TaskLocation = (!FNullEnt(pBot->CurrentTask->TaskTarget)) ? pBot->CurrentTask->TaskTarget->v.origin : pBot->CurrentTask->TaskLocation;

	return vDist2DSq(TaskLocation, Squad->SquadTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f));
}

void AITAC_ManageSquads()
{
	for (auto it = ActiveSquads.begin(); it != ActiveSquads.end();)
	{
		for (auto pIt = it->SquadMembers.begin(); pIt != it->SquadMembers.end();)
		{
			AvHAIPlayer* ThisPlayer = (*pIt);
			if (!AITAC_IsBotPursuingSquadObjective(ThisPlayer, &(*it)))
			{
				pIt = it->SquadMembers.erase(pIt);
			}
			else
			{
				pIt++;
			}
		}

		if (it->SquadMembers.size() == 0)
		{
			it = ActiveSquads.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void AITAC_UpdateSquads()
{
	AITAC_ManageSquads();

	for (auto it = ActiveSquads.begin(); it != ActiveSquads.end(); it++)
	{
		if (it->SquadMembers.size() > 1)
		{
			if (vIsZero(it->SquadGatherLocation))
			{
				vector<bot_path_node> TravelPath;

				dtStatus PathFindResult = FindPathClosestToPoint(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), AITAC_GetTeamStartingLocation(it->SquadTeam), UTIL_GetEntityGroundLocation(it->SquadTarget), TravelPath, UTIL_MetresToGoldSrcUnits(20.0f));

				if (dtStatusSucceed(PathFindResult))
				{
					for (auto pIt = TravelPath.rbegin(); pIt != TravelPath.rend(); pIt++)
					{
						if (pIt->area != SAMPLE_POLYAREA_GROUND || pIt->flag != SAMPLE_POLYFLAGS_WALK) { continue; }

						if (UTIL_QuickTrace(nullptr, pIt->Location, it->SquadTarget->v.origin)) { continue; }

						if (vDist2DSq(pIt->Location, it->SquadTarget->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
						{
							DeployableSearchFilter EnemyStuff;
							EnemyStuff.DeployableTeam = AIMGR_GetEnemyTeam(it->SquadTeam);
							EnemyStuff.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
							EnemyStuff.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

							if (AITAC_DeployableExistsAtLocation(pIt->Location, &EnemyStuff))
							{
								continue;
							}

							it->SquadGatherLocation = pIt->Location;
							break;
						}
					}
				}
			}
		}
		else
		{
			it->SquadGatherLocation = ZERO_VECTOR;
		}

		if (!it->bExecuteObjective && !vIsZero(it->SquadGatherLocation))
		{
			bool bAllHaveGathered = true;

			for (auto playerIt = it->SquadMembers.begin(); playerIt != it->SquadMembers.end(); playerIt++)
			{
				AvHAIPlayer* ThisBot = (*playerIt);

				if (vDist2DSq(ThisBot->Edict->v.origin, it->SquadGatherLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					bAllHaveGathered = false;
				}
			}

			if (bAllHaveGathered)
			{
				it->bExecuteObjective = true;
			}
		}
	}
}

AvHAISquad* AITAC_GetSquadForObjective(AvHAIPlayer* pBot, edict_t* TaskTarget, BotTaskType ObjectiveType)
{
	AvHAISquad* JoinSquad = nullptr;

	for (auto it = ActiveSquads.begin(); it != ActiveSquads.end(); it++)
	{
		if (it->SquadTeam == pBot->Player->GetTeam() && it->SquadTarget == TaskTarget && it->SquadObjective == ObjectiveType)
		{
			auto element = std::find(it->SquadMembers.begin(), it->SquadMembers.end(), pBot);

			if (element != it->SquadMembers.end())
			{
				return &(*it);
			}
			else
			{
				if (!JoinSquad && !it->bExecuteObjective)
				{
					JoinSquad = &(*it);
				}
			}
		}
	}

	if (JoinSquad)
	{
		JoinSquad->SquadMembers.push_back(pBot);
		return JoinSquad; 
	}

	AvHAISquad NewSquad;
	NewSquad.SquadTeam = pBot->Player->GetTeam();
	NewSquad.SquadTarget = TaskTarget;
	NewSquad.SquadObjective = ObjectiveType;
	NewSquad.bExecuteObjective = false;
	NewSquad.SquadGatherLocation = ZERO_VECTOR;

	ActiveSquads.push_back(NewSquad);

	return nullptr;
}

void AITAC_ClearSquads()
{
	ActiveSquads.clear();
}

Vector AITAC_GetGatherLocationForSquad(AvHAISquad* Squad)
{
	if (!Squad || FNullEnt(Squad->SquadTarget)) { return ZERO_VECTOR; }

	vector<bot_path_node> TravelPath;

	dtStatus PathFindResult = FindPathClosestToPoint(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), AITAC_GetTeamStartingLocation(Squad->SquadTeam), UTIL_GetEntityGroundLocation(Squad->SquadTarget), TravelPath, UTIL_MetresToGoldSrcUnits(20.0f));

	if (dtStatusSucceed(PathFindResult))
	{
		for (auto pIt = TravelPath.rend(); pIt != TravelPath.rbegin(); pIt++)
		{
			if (pIt->area != SAMPLE_POLYAREA_GROUND || pIt->flag != SAMPLE_POLYFLAGS_WALK) { continue; }

			if (UTIL_QuickTrace(nullptr, pIt->Location, Squad->SquadTarget->v.origin)) { continue; }

			if (vDist2DSq(pIt->Location, Squad->SquadTarget->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
			{
				DeployableSearchFilter EnemyStuff;
				EnemyStuff.DeployableTeam = AIMGR_GetEnemyTeam(Squad->SquadTeam);
				EnemyStuff.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
				EnemyStuff.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

				if (AITAC_DeployableExistsAtLocation(pIt->Location, &EnemyStuff))
				{
					continue;
				}

				return pIt->Location;
			}
		}
	}

	return ZERO_VECTOR;
}