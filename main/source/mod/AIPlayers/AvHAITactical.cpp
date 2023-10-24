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

#include "../AvHGamerules.h"
#include "../AvHServerUtil.h"

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
unsigned int StructureRefreshFrame = 0;
// Increments by 1 every time the item list is refreshed. Used to detect if items have been removed from play and no longer show up
unsigned int ItemRefreshFrame = 0;

Vector TeamAStartingLocation = ZERO_VECTOR;
Vector TeamBStartingLocation = ZERO_VECTOR;

extern nav_mesh NavMeshes[MAX_NAV_MESHES]; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)
extern nav_profile BaseNavProfiles[MAX_NAV_PROFILES]; // Array of nav profiles

bool bNavMeshModified = false;

std::vector<AvHAIBuildableStructure*> AITAC_FindAllDeployables(const Vector& Location, const DeployableSearchFilter* Filter)
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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

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

AvHAIBuildableStructure* AITAC_FindClosestDeployableToLocation(const Vector& Location, const DeployableSearchFilter* Filter)
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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

			unsigned int StructureReachabilityFlags = (it.second.TeamAReachabilityFlags | it.second.TeamBReachabilityFlags);

			if (Filter->ReachabilityTeam != TEAM_IND)
			{
				StructureReachabilityFlags = (Filter->ReachabilityTeam == TeamA) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;
			}

			if (Filter->ReachabilityFlags != AI_REACHABILITY_NONE && !(StructureReachabilityFlags & Filter->ReachabilityFlags)) { continue; }

			if (it.second.StructureType & Filter->DeployableTypes)
			{
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

AvHAIBuildableStructure* AITAC_GetDeployableRefFromEdict(const edict_t* Structure)
{
	if (FNullEnt(Structure)) { return nullptr; }

	int EntIndex = ENTINDEX(Structure);

	if (EntIndex < 0) { return nullptr; }

	AvHTeamNumber TeamA = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamB = GetGameRules()->GetTeamBNumber();

	return (Structure->v.team == TeamA) ? &TeamAStructureMap[EntIndex] : &TeamBStructureMap[EntIndex];

}

AvHAIBuildableStructure* AITAC_GetNearestDeployableDirectlyReachable(AvHAIPlayer* pBot, const Vector Location, const DeployableSearchFilter* Filter)
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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

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
			if (it.second.StructureStatusFlags & Filter->ExcludeStatusFlags) { continue; }
			if ((it.second.StructureStatusFlags & Filter->IncludeStatusFlags) != Filter->IncludeStatusFlags) { continue; }

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

	Vector HiveFloorLoc = UTIL_GetFloorUnderEntity(Hive->HiveEntity->edict());

	Vector NearestNavigableLoc = ZERO_VECTOR;

	FOR_ALL_ENTITIES(kesTeamStart, AvHTeamStartEntity*)
		if (NearestNavigableLoc == ZERO_VECTOR)
		{
			NearestNavigableLoc = FindClosestNavigablePointToDestination(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], theEntity->pev->origin, HiveFloorLoc, UTIL_MetresToGoldSrcUnits(10.0f));
		}
	END_FOR_ALL_ENTITIES(kesTeamStart);

	if (NearestNavigableLoc != ZERO_VECTOR)
	{
		return NearestNavigableLoc;
	}
	else
	{
		return HiveFloorLoc;
	}
}

void AITAC_RefreshHiveData()
{
	if (Hives.size() == 0)
	{
		FOR_ALL_ENTITIES(kesTeamHive, AvHHive*)

			AvHAIHiveDefinition NewHive;
			NewHive.HiveEntity = theEntity;
			NewHive.Location = theEntity->pev->origin;
			NewHive.HiveResNodeRef = AITAC_GetNearestResourceNodeToLocation(theEntity->pev->origin);
			NewHive.FloorLocation = UTIL_GetFloorUnderEntity(theEntity->edict()); // Some hives are suspended in the air, this is the floor location directly beneath it

			Hives.push_back(NewHive);

		END_FOR_ALL_ENTITIES(kesTeamHive)

	}

	int NextRefresh = 0;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		AvHHive* theEntity = it->HiveEntity;

		it->TechStatus = theEntity->GetTechnology();
		it->bIsUnderAttack = GetGameRules()->GetIsEntityUnderAttack(theEntity->entindex());
		it->OwningTeam = theEntity->GetTeamNumber();
		it->Status = (theEntity->GetIsActive() ? HIVE_STATUS_BUILT : (theEntity->GetIsSpawning() ? HIVE_STATUS_BUILDING : HIVE_STATUS_UNBUILT));

		if (it->Status != HIVE_STATUS_UNBUILT && it->ObstacleRefs[REGULAR_NAV_MESH] == 0)
		{
			UTIL_AddTemporaryObstacles(UTIL_GetCentreOfEntity(it->HiveEntity->edict()) - Vector(0.0f, 0.0f, 25.0f), 125.0f, 300.0f, DT_AREA_NULL, it->ObstacleRefs);
			it->NextFloorLocationCheck = gpGlobals->time + 1.0f;
		}
		else if (it->Status == HIVE_STATUS_UNBUILT && it->ObstacleRefs[REGULAR_NAV_MESH] != 0)
		{
			UTIL_RemoveTemporaryObstacles(it->ObstacleRefs);
			it->NextFloorLocationCheck = gpGlobals->time + 1.0f;
		}

		if (it->NextFloorLocationCheck > 0.0f && gpGlobals->time >= it->NextFloorLocationCheck)
		{
			it->FloorLocation = AITAC_GetFloorLocationForHive(&(*it));

			it->NextFloorLocationCheck = gpGlobals->time + (5.0f + (0.1f * NextRefresh));
		}

		NextRefresh++;
	}

}

Vector AITAC_GetTeamStartingLocation(AvHTeamNumber Team)
{
	if (TeamAStartingLocation == ZERO_VECTOR || TeamBStartingLocation == ZERO_VECTOR)
	{
		AvHTeam* AvHTeamARef = GetGameRules()->GetTeamA();
		AvHTeam* AvHTeamBRef = GetGameRules()->GetTeamB();

		if (AvHTeamARef)
		{
			Vector TeamStartLocation = AvHTeamARef->GetStartingLocation();

			if (AvHTeamARef->GetTeamType() == AVH_CLASS_TYPE_MARINE)
			{
				TeamAStartingLocation = TeamStartLocation;
			}
			else
			{
				TeamAStartingLocation = TeamStartLocation;

				const AvHAIHiveDefinition* Hive = AITAC_GetHiveNearestLocation(TeamStartLocation);

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
				TeamBStartingLocation = AvHTeamBRef->GetStartingLocation();
			}
			else
			{
				TeamBStartingLocation = TeamStartLocation;

				const AvHAIHiveDefinition* Hive = AITAC_GetHiveNearestLocation(TeamStartLocation);

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
	}

	return (Team == GetGameRules()->GetTeamANumber()) ? TeamAStartingLocation : TeamBStartingLocation;
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

	DeployableSearchFilter ChairFilter;
	ChairFilter.DeployableTypes = STRUCTURE_MARINE_COMMCHAIR;
	ChairFilter.DeployableTeam = Team;
	ChairFilter.ReachabilityTeam = TEAM_IND;
	ChairFilter.bConsiderPhaseDistance = false;
	ChairFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;

	AvHAIBuildableStructure* ChairRef = AITAC_FindClosestDeployableToLocation(ZERO_VECTOR, &ChairFilter);

	if (ChairRef)
	{
		return ChairRef->Location;
	}

	return ZERO_VECTOR;
}

void AITAC_RefreshReachabilityForItem(AvHAIDroppedItem* Item)
{
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
		Item->TeamAReachabilityFlags = AI_REACHABILITY_NONE;
		Item->TeamBReachabilityFlags = AI_REACHABILITY_NONE;
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

			WelderProfile.Filters.removeExcludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), Item->edict->v.origin, max_player_use_reach);

			if (bIsReachableWelder)
			{
				Item->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
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

			WelderProfile.Filters.removeExcludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), Item->edict->v.origin, max_player_use_reach);

			if (bIsReachableWelder)
			{
				Item->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
		}
	}
}

void AITAC_RefreshReachabilityForResNode(AvHAIResourceNode* ResNode)
{
	ResNode->bReachabilityMarkedDirty = false;

	ResNode->TeamAReachabilityFlags = AI_REACHABILITY_NONE;
	ResNode->TeamBReachabilityFlags = AI_REACHABILITY_NONE;

	bool bOnNavMesh = UTIL_PointIsOnNavmesh(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], ResNode->Location, Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));

	if (!bOnNavMesh)
	{
		ResNode->TeamAReachabilityFlags = AI_REACHABILITY_NONE;
		ResNode->TeamBReachabilityFlags = AI_REACHABILITY_NONE;
		return;
	}

	if (GetGameRules()->GetTeamA()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), ResNode->Location, max_player_use_reach);

		if (bIsReachableMarine)
		{
			ResNode->TeamAReachabilityFlags |= AI_REACHABILITY_MARINE;
			ResNode->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.removeExcludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), ResNode->Location, max_player_use_reach);

			if (bIsReachableWelder)
			{
				ResNode->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
		}
	}
	else
	{
		bool bIsReachableSkulk = UTIL_PointIsReachable(BaseNavProfiles[SKULK_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), ResNode->Location, max_player_use_reach);
		bool bIsReachableGorge = UTIL_PointIsReachable(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), ResNode->Location, max_player_use_reach);
		bool bIsReachableOnos = UTIL_PointIsReachable(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), ResNode->Location, max_player_use_reach);

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
	}

	if (GetGameRules()->GetTeamB()->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		bool bIsReachableMarine = UTIL_PointIsReachable(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), ResNode->Location, max_player_use_reach);

		if (bIsReachableMarine)
		{
			ResNode->TeamBReachabilityFlags |= AI_REACHABILITY_MARINE;
			ResNode->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
		}
		else
		{
			nav_profile WelderProfile;
			memcpy(&WelderProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));

			WelderProfile.Filters.removeExcludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), ResNode->Location, max_player_use_reach);

			if (bIsReachableWelder)
			{
				ResNode->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
			}
		}
	}
	else
	{
		bool bIsReachableSkulk = UTIL_PointIsReachable(BaseNavProfiles[SKULK_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), ResNode->Location, max_player_use_reach);
		bool bIsReachableGorge = UTIL_PointIsReachable(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), ResNode->Location, max_player_use_reach);
		bool bIsReachableOnos = UTIL_PointIsReachable(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamBNumber()), ResNode->Location, max_player_use_reach);

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
	}

}

void AITAC_RefreshResourceNodes()
{
	if (ResourceNodes.size() == 0)
	{
		FOR_ALL_ENTITIES(kesFuncResource, AvHFuncResource*)

			AvHAIResourceNode NewResNode;
			NewResNode.ResourceEntity = theEntity;
			NewResNode.Location = theEntity->pev->origin;
			NewResNode.TeamAReachabilityFlags = AI_REACHABILITY_NONE;
			NewResNode.TeamBReachabilityFlags = AI_REACHABILITY_NONE;
			NewResNode.bReachabilityMarkedDirty = true;	

			ResourceNodes.push_back(NewResNode);

		END_FOR_ALL_ENTITIES(kesFuncResource)
	}

	for (auto it = ResourceNodes.begin(); it != ResourceNodes.end(); it++)
	{
		AvHFuncResource* ResourceEntity = it->ResourceEntity;

		it->bIsOccupied = ResourceEntity->GetIsOccupied();
		
		if (it->bIsOccupied)
		{
			DeployableSearchFilter TowerFilter;
			TowerFilter.DeployableTypes = (STRUCTURE_MARINE_RESTOWER | STRUCTURE_ALIEN_RESTOWER);

			AvHAIBuildableStructure* OccupyingTower = AITAC_FindClosestDeployableToLocation(it->Location, &TowerFilter);

			if (OccupyingTower)
			{
				it->ActiveTowerEntity = OccupyingTower->edict;
				it->OwningTeam = OccupyingTower->EntityRef->GetTeamNumber();
			}
		}
		else
		{
			it->ActiveTowerEntity = nullptr;
			it->OwningTeam = TEAM_IND;
		}

		if (it->bReachabilityMarkedDirty)
		{
			AITAC_RefreshReachabilityForResNode(&(*it));
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

	UTIL_UpdateDoors(false);
	UTIL_UpdateWeldableObstacles();

	AITAC_RefreshHiveData();

	if (bNavMeshModified)
	{
		AITAC_OnNavMeshModified();
		bNavMeshModified = false;
	}
}

void AITAC_OnNavMeshModified()
{
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
		if (it->second.LastSeen < StructureRefreshFrame)
		{
			AITAC_OnStructureDestroyed(&it->second);
			UTIL_RemoveTemporaryObstacles(it->second.ObstacleRefs);
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
		if (it->second.LastSeen < StructureRefreshFrame)
		{
			AITAC_OnStructureDestroyed(&it->second);
			UTIL_RemoveTemporaryObstacles(it->second.ObstacleRefs);
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

	if (FNullEnt(ItemEdict)) { return; }

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
	Structure->bReachabilityMarkedDirty = false;

	bool bIsOnNavMesh = UTIL_PointIsOnNavmesh(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], UTIL_GetEntityGroundLocation(Structure->edict), Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));

	if (!bIsOnNavMesh)
	{
		Structure->TeamAReachabilityFlags = AI_REACHABILITY_NONE;
		Structure->TeamBReachabilityFlags = AI_REACHABILITY_NONE;
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

			WelderProfile.Filters.removeExcludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);

			if (bIsReachableWelder)
			{
				Structure->TeamAReachabilityFlags |= AI_REACHABILITY_WELDER;
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

			WelderProfile.Filters.removeExcludeFlags(SAMPLE_POLYFLAGS_WELD);

			bool bIsReachableWelder = UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), UTIL_GetEntityGroundLocation(Structure->edict), max_player_use_reach);

			if (bIsReachableWelder)
			{
				Structure->TeamBReachabilityFlags |= AI_REACHABILITY_WELDER;
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
	}
}

void AITAC_UpdateBuildableStructure(CBaseEntity* Structure)
{
	if (!Structure || (Structure->pev->effects & EF_NODRAW) || (Structure->pev->deadflag != DEAD_NO)) { return; }

	AvHBaseBuildable* BaseBuildable = dynamic_cast<AvHBaseBuildable*>(Structure);

	if (!BaseBuildable) { return; }

	edict_t* BuildingEdict = BaseBuildable->edict();

	AvHAIDeployableStructureType StructureType = UTIL_IUSER3ToStructureType(BaseBuildable->pev->iuser3);

	if (StructureType == STRUCTURE_NONE) { return; }

	int EntIndex = BaseBuildable->entindex();
	if (EntIndex < 0) { return; }

	AvHTeamNumber TeamANumber = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamBNumber = GetGameRules()->GetTeamBNumber();

	std::unordered_map<int, AvHAIBuildableStructure>& BuildingMap = (BaseBuildable->GetTeamNumber() == TeamANumber) ? TeamAStructureMap : TeamBStructureMap;

	if (BuildingMap[EntIndex].LastSeen == 0)
	{
		BuildingMap[EntIndex].EntityRef = BaseBuildable;
		BuildingMap[EntIndex].edict = BuildingEdict;
		BuildingMap[EntIndex].StructureType = StructureType;

		BuildingMap[EntIndex].OffMeshConnections.clear();

		memset(&BuildingMap[EntIndex].ObstacleRefs, 0, sizeof(BuildingMap[EntIndex].ObstacleRefs));

		bool bShouldCollide = UTIL_ShouldStructureCollide(StructureType);

		if (bShouldCollide)
		{
			unsigned int area = UTIL_GetAreaForObstruction(StructureType, BuildingEdict);
			float Radius = UTIL_GetStructureRadiusForObstruction(StructureType);
			UTIL_AddTemporaryObstacles(UTIL_GetCentreOfEntity(BuildingMap[EntIndex].edict), Radius, 100.0f, area, BuildingMap[EntIndex].ObstacleRefs);
		}
		else
		{
			memset(BuildingMap[EntIndex].ObstacleRefs, 0, sizeof(unsigned int) * MAX_NAV_MESHES);
		}

		BuildingMap[EntIndex].Location = g_vecZero;

		AITAC_OnStructureCreated(&BuildingMap[EntIndex]);
	}

	if (vIsZero(BuildingMap[EntIndex].Location) || !vEquals(BaseBuildable->pev->origin, BuildingMap[EntIndex].Location, 5.0f))
	{
		AITAC_RefreshReachabilityForStructure(&BuildingMap[EntIndex]);

		BuildingMap[EntIndex].Location = BaseBuildable->pev->origin;
	}

	unsigned int NewFlags = STRUCTURE_STATUS_NONE;

	if (BaseBuildable->GetIsBuilt())
	{
		if (!(BuildingMap[EntIndex].StructureStatusFlags & STRUCTURE_STATUS_COMPLETED)) {
			AITAC_OnStructureCompleted(&BuildingMap[EntIndex]);
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
		if (!(BuildingMap[EntIndex].StructureStatusFlags & STRUCTURE_STATUS_RECYCLING))
		{
			AITAC_OnStructureBeginRecycling(&BuildingMap[EntIndex]);
		}
		NewFlags |= STRUCTURE_STATUS_RECYCLING;
	}

	float NewHealthPercent = (BuildingEdict->v.health / BuildingEdict->v.max_health);

	if (NewHealthPercent < BuildingMap[EntIndex].healthPercent)
	{
		BuildingMap[EntIndex].lastDamagedTime = gpGlobals->time;
	}

	BuildingMap[EntIndex].healthPercent = NewHealthPercent;

	if (gpGlobals->time - BuildingMap[EntIndex].lastDamagedTime < 10.0f)
	{
		NewFlags |= STRUCTURE_STATUS_UNDERATTACK;
	}

	BuildingMap[EntIndex].StructureStatusFlags = NewFlags;
	BuildingMap[EntIndex].LastSeen = StructureRefreshFrame;

}

void AITAC_OnStructureCreated(AvHAIBuildableStructure* NewStructure)
{
	if (!GetGameRules()->GetGameStarted()) { return; }

	AvHTeamNumber StructureTeam = NewStructure->EntityRef->GetTeamNumber();

	if (StructureTeam == TEAM_IND) { return; }
	
	AvHTeam* Team = GetGameRules()->GetTeam(StructureTeam);

	if (!Team) { return; }

	if (Team->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		AvHAIPlayer* ActiveAICommander = AIMGR_GetAICommander(StructureTeam);

		if (ActiveAICommander)
		{
			LinkDeployedObjectToCommanderAction(ActiveAICommander, NewStructure);
		}
	}
	else
	{
		AvHAIPlayer* BuildingPlayer = AIMGR_FindPlayerOnTeamWaitingBuildLink(StructureTeam, NewStructure->StructureType, NewStructure->Location);

		if (BuildingPlayer)
		{
			AITAC_LinkAlienStructureToTask(BuildingPlayer, NewStructure);
		}

	}

}

void AITAC_OnStructureCompleted(AvHAIBuildableStructure* NewStructure)
{
	if (NewStructure->StructureType == STRUCTURE_MARINE_PHASEGATE)
	{
		DeployableSearchFilter Filter;
		Filter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
		Filter.DeployableTeam = (AvHTeamNumber)NewStructure->edict->v.team;
		Filter.ReachabilityFlags = AI_REACHABILITY_NONE;
		Filter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;

		// Get all other completed phase gates for this team and add bidirectional connections to them
		std::vector<AvHAIBuildableStructure*> OtherPhaseGates = AITAC_FindAllDeployables(NewStructure->Location, &Filter);

		SamplePolyFlags NewFlag = ((AvHTeamNumber)NewStructure->edict->v.team == GetGameRules()->GetTeamANumber()) ? SAMPLE_POLYFLAGS_TEAM1PHASEGATE : SAMPLE_POLYFLAGS_TEAM2PHASEGATE;

		for (auto pg = OtherPhaseGates.begin(); pg != OtherPhaseGates.end(); pg++)
		{
			// Don't add off-mesh connections to ourselves!
			if ((*pg) == NewStructure) { continue; }

			AvHAIBuildableStructure* OtherPhaseGate = (*pg);

			AvHAIOffMeshConnection NewConnection;
			NewConnection.FromLocation = NewStructure->Location;
			NewConnection.ToLocation = OtherPhaseGate->Location;
			NewConnection.ConnectionFlags = NewFlag;
			NewConnection.TargetObject = OtherPhaseGate->edict;
			NewConnection.MeshConnectionIndex = -1;

			UTIL_AddOffMeshConnection(NewStructure->Location, OtherPhaseGate->Location, SAMPLE_POLYAREA_GROUND, NewFlag, true, &NewConnection);

			NewStructure->OffMeshConnections.push_back(NewConnection);

		}
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
	// For phase gates, treat them like they've been destroyed
	if (RecyclingStructure->StructureType == STRUCTURE_MARINE_PHASEGATE)
	{
		AITAC_OnStructureDestroyed(RecyclingStructure);
	}
}

void AITAC_OnStructureDestroyed(AvHAIBuildableStructure* DestroyedStructure)
{
	if (DestroyedStructure->StructureType == STRUCTURE_MARINE_PHASEGATE)
	{
		// Eliminate all connections from this phase gate
		for (auto it = DestroyedStructure->OffMeshConnections.begin(); it != DestroyedStructure->OffMeshConnections.begin();)
		{
			UTIL_RemoveOffMeshConnections(&(*it));

			it = DestroyedStructure->OffMeshConnections.erase(it);
		}

		DestroyedStructure->OffMeshConnections.clear();

		DeployableSearchFilter Filter;
		Filter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
		Filter.DeployableTeam = (AvHTeamNumber)DestroyedStructure->edict->v.team;
		Filter.ReachabilityFlags = AI_REACHABILITY_NONE;
		Filter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;

		// Get all other completed phase gates for this team and remove any connections going to this structure
		std::vector<AvHAIBuildableStructure*> OtherPhaseGates = AITAC_FindAllDeployables(DestroyedStructure->Location, &Filter);
		
		for (auto it = OtherPhaseGates.begin(); it != OtherPhaseGates.end(); it++)
		{
			// Don't check for off-mesh connections from ourselves!
			if ((*it) == DestroyedStructure) { continue; }

			AvHAIBuildableStructure* OtherPhaseGate = (*it);

			AITAC_RemovePhaseGateConnections(OtherPhaseGate, DestroyedStructure);
		}
	}
}

void AITAC_LinkAlienStructureToTask(AvHAIPlayer* pBot, AvHAIBuildableStructure* NewStructure)
{

}

void AITAC_LinkDeployedItemToAction(AvHAIPlayer* CommanderBot, const AvHAIDroppedItem* NewItem)
{

}

void AITAC_ClearMapAIData()
{
	ResourceNodes.clear();

	AITAC_ClearHiveInfo();

	MarineDroppedItemMap.clear();
	TeamAStructureMap.clear();
	TeamBStructureMap.clear();

	StructureRefreshFrame = 0;
	ItemRefreshFrame = 0;

	last_structure_refresh_time = 0.0f;
	last_item_refresh_time = 0.0f;

	TeamAStartingLocation = ZERO_VECTOR;
	TeamBStartingLocation = ZERO_VECTOR;
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

	AvHAIBuildableStructure* StartPhase = AITAC_FindClosestDeployableToLocation(StartPoint, &PGFilter);

	if (!StartPhase)
	{
		return DirectDist;
	}

	AvHAIBuildableStructure* EndPhase = AITAC_FindClosestDeployableToLocation(EndPoint, &PGFilter);

	if (!EndPhase || EndPhase == StartPhase)
	{
		return DirectDist;
	}

	float PhaseDist = vDist2DSq(StartPoint, StartPhase->edict->v.origin) + vDist2DSq(EndPoint, EndPhase->edict->v.origin);
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

	unsigned char StructureArea = (BuildingEdict->v.team == TeamA) ? DT_TILECACHE_TEAM1STRUCTURE_AREA : DT_TILECACHE_TEAM2STRUCTURE_AREA;

	switch (StructureType)
	{
	case STRUCTURE_MARINE_RESTOWER:
	case STRUCTURE_MARINE_COMMCHAIR:
	case STRUCTURE_MARINE_ARMOURY:
	case STRUCTURE_MARINE_ADVARMOURY:
	case STRUCTURE_MARINE_OBSERVATORY:
	case STRUCTURE_ALIEN_RESTOWER:
	case STRUCTURE_ALIEN_HIVE:
		return StructureArea;
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

	AvHAIBuildableStructure* StructureRef = AITAC_GetDeployableRefFromEdict(Structure);

	if (!StructureRef) { return false; }

	unsigned int TeamReachability = (pBot->Edict->v.team == GetGameRules()->GetTeamANumber()) ? StructureRef->TeamAReachabilityFlags : StructureRef->TeamBReachabilityFlags;

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

AvHAIHiveDefinition* AITAC_GetHiveFromEdict(const edict_t* Edict)
{
	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		if (it->HiveEntity->edict() == Edict)
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

	int NumMines = 0;

	AvHTeamNumber TeamANumber = GetGameRules()->GetTeamANumber();
	AvHTeamNumber TeamBNumber = GetGameRules()->GetTeamBNumber();

	std::unordered_map<int, AvHAIBuildableStructure>& BuildingMap = (StructureTeam == TeamANumber) ? TeamAStructureMap : TeamBStructureMap;

	for (auto& it : BuildingMap)
	{
		unsigned int TeamReachabilityFlag = (StructureTeam == TeamANumber) ? it.second.TeamAReachabilityFlags : it.second.TeamBReachabilityFlags;

		if (it.second.StructureType != STRUCTURE_MARINE_DEPLOYEDMINE || !(TeamReachabilityFlag & AI_REACHABILITY_MARINE)) { continue; }

		if (vDist2DSq(StructureToMine->v.origin, it.second.Location) > sqrf(UTIL_MetresToGoldSrcUnits(2.0f))) { continue; }

		NumMines++;

		Vector Dir = UTIL_GetVectorNormal2D(it.second.Location - StructureToMine->v.origin);

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

	Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInDonut(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], StructureToMine->v.origin, Size, Size + 16.0f);

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

bool AITAC_GetNumPlayersOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer)
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
	if (pBot->BotNavInfo.PathSize == 0) { return false; }

	if (UTIL_GetBotCurrentPathArea(pBot) != SAMPLE_POLYAREA_GROUND) { return false; }

	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());

	if (AITAC_AnyPlayerOnTeamHasLOSToLocation(EnemyTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(50.0f), nullptr)) { return false; }

	int NumEnemiesAtDestination = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint].Location, UTIL_MetresToGoldSrcUnits(50.0f), pBot->Edict);

	if (NumEnemiesAtDestination > 1)
	{
		return (vDist2DSq(pBot->Edict->v.origin, pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint].Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));
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

	AvHAIBuildableStructure* ChairStructure = AITAC_FindClosestDeployableToLocation(ZERO_VECTOR, &ChairFilter);

	if (ChairStructure)
	{
		return ChairStructure->edict;
	}

	ChairFilter.DeployableTeam = TEAM_IND;

	ChairStructure = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(Team), &ChairFilter);

	if (ChairStructure)
	{
		return ChairStructure->edict;
	}

	return nullptr;
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
			return STRUCTURE_ALIEN_OFFENCECHAMBER;
		default:
			return STRUCTURE_NONE;
	}

	return STRUCTURE_NONE;
}

bool UTIL_ResearchIsComplete(const AvHTeamNumber Team, const AvHTechID Research)
{
	AvHTeam* TeamRef = GetGameRules()->GetTeam(Team);

	if (!TeamRef) { return false; }

	AvHResearchManager ResearchManager = TeamRef->GetResearchManager();

	return ResearchManager.GetTechNodes().GetIsTechResearched(Research);
}