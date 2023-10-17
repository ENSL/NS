
#include "AvHAITask.h"
#include "AvHAINavigation.h"
#include "AvHAITactical.h"
#include "AvHAIMath.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIWeaponHelper.h"
#include "AvHAIHelper.h"
#include "AvHAIPlayerManager.h"
#include "AvHAIConfig.h"

#include "../AvHSharedUtil.h"
#include "../AvHAlienWeaponConstants.h"
#include "../AvHGamerules.h"
#include "../AvHWeldable.h"

extern nav_mesh NavMeshes[MAX_NAV_MESHES]; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)
extern nav_profile BaseNavProfiles[MAX_NAV_PROFILES]; // Array of nav profiles

void AITASK_ClearAllBotTasks(AvHAIPlayer* pBot)
{
	AITASK_ClearBotTask(pBot, &pBot->PrimaryBotTask);
	AITASK_ClearBotTask(pBot, &pBot->SecondaryBotTask);
	AITASK_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);
	AITASK_ClearBotTask(pBot, &pBot->CommanderTask);
}

void AITASK_BotUpdateAndClearTasks(AvHAIPlayer* pBot)
{
	if (pBot->CommanderTask.TaskType != TASK_NONE)
	{
		if (!AITASK_IsTaskStillValid(pBot, &pBot->CommanderTask))
		{
			if (AITASK_IsTaskCompleted(pBot, &pBot->CommanderTask))
			{
				AITASK_OnCompleteCommanderTask(pBot, &pBot->CommanderTask);
			}
			else
			{
				AITASK_ClearBotTask(pBot, &pBot->CommanderTask);
			}
		}
	}

	if (pBot->PrimaryBotTask.TaskType != TASK_NONE)
	{
		if (!AITASK_IsTaskStillValid(pBot, &pBot->PrimaryBotTask))
		{
			AITASK_ClearBotTask(pBot, &pBot->PrimaryBotTask);
		}
	}

	if (pBot->SecondaryBotTask.TaskType != TASK_NONE)
	{
		if (!AITASK_IsTaskStillValid(pBot, &pBot->SecondaryBotTask))
		{
			AITASK_ClearBotTask(pBot, &pBot->SecondaryBotTask);
		}
	}

	if (pBot->WantsAndNeedsTask.TaskType != TASK_NONE)
	{
		if (!AITASK_IsTaskStillValid(pBot, &pBot->WantsAndNeedsTask))
		{
			AITASK_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);
		}
	}

}

void AITASK_OnCompleteCommanderTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || !IsPlayerMarine(pBot->Edict)) { return; }

	BotTaskType OldTaskType = Task->TaskType;
	AITASK_ClearBotTask(pBot, Task);

	if (OldTaskType == TASK_GUARD)
	{
		UTIL_ClearGuardInfo(pBot);
	}

	if (OldTaskType == TASK_MOVE)
	{
		DeployableSearchFilter EnemyResTowerFilter;
		EnemyResTowerFilter.DeployableTypes = SEARCH_ANY_RES_TOWER;
		EnemyResTowerFilter.Team = (AIMGR_GetEnemyTeam(pBot->Player->GetTeam()));
		EnemyResTowerFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
		EnemyResTowerFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

		AvHAIBuildableStructure* NearbyAlienTower = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &EnemyResTowerFilter);

		if (NearbyAlienTower)
		{
			const AvHAIResourceNode* NodeRef = AITAC_GetNearestResourceNodeToLocation(NearbyAlienTower->Location);
			if (NodeRef)
			{
				AITASK_SetCapResNodeTask(pBot, Task, NodeRef, false);
				Task->bIssuedByCommander = true;
				return;
			}
		}
	}

	// After completing a move or build task, wait a bit in case the commander wants to do something else
	if (OldTaskType == TASK_MOVE || OldTaskType == TASK_BUILD)
	{
		Task->TaskType = TASK_GUARD;
		Task->TaskLocation = pBot->Edict->v.origin;
		Task->TaskLength = (OldTaskType == TASK_MOVE) ? 30.0f : 20.0f;
		Task->TaskStartedTime = gpGlobals->time;
		Task->bIssuedByCommander = true;
	}

}

void AITASK_ClearBotTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return; }

	if (Task->TaskType == TASK_GUARD)
	{
		UTIL_ClearGuardInfo(pBot);
	}

	Task->TaskType = TASK_NONE;
	Task->TaskLocation = g_vecZero;
	Task->TaskTarget = nullptr;
	Task->TaskSecondaryTarget = nullptr;
	Task->TaskStartedTime = 0.0f;
	Task->TaskLength = 0.0f;
	Task->bIssuedByCommander = false;
	Task->bTargetIsPlayer = false;
	Task->bTaskIsUrgent = false;
	Task->bIsWaitingForBuildLink = false;
	Task->LastBuildAttemptTime = 0.0f;
	Task->BuildAttempts = 0;
	Task->StructureType = STRUCTURE_NONE;
}

bool AITASK_IsTaskUrgent(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (Task->TaskType == TASK_NONE) { return false; }

	if (Task->bTaskIsUrgent) { return true; }

	switch (Task->TaskType)
	{
	case TASK_GET_AMMO:
		return (BotGetPrimaryWeaponAmmoReserve(pBot) == 0);
	case TASK_GET_HEALTH:
		return (IsPlayerMarine(pBot->Edict)) ? (pBot->Edict->v.health < 50.0f) : (GetPlayerOverallHealthPercent(pBot->Edict) < 50.0f);
	case TASK_ATTACK:
	case TASK_GET_WEAPON:
	case TASK_GET_EQUIPMENT:
	case TASK_WELD:
		return false;
	case TASK_RESUPPLY:
		return (pBot->Edict->v.health < 50.0f) || (BotGetPrimaryWeaponAmmoReserve(pBot) == 0);
	case TASK_MOVE:
		return AITASK_IsMoveTaskUrgent(pBot, Task);
	case TASK_BUILD:
		return AITASK_IsBuildTaskUrgent(pBot, Task);
	case TASK_GUARD:
		return AITASK_IsGuardTaskUrgent(pBot, Task);
	default:
		return false;
	}

	return false;
}

bool AITASK_IsGuardTaskUrgent(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (Task->TaskTarget)
	{
		AvHAIDeployableStructureType StructType = GetStructureTypeFromEdict(Task->TaskTarget);

		if (StructType == STRUCTURE_MARINE_PHASEGATE || StructType == STRUCTURE_MARINE_TURRETFACTORY)
		{
			return true;
		}
	}

	return false;
}

bool AITASK_IsBuildTaskUrgent(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task->TaskTarget) { return false; }

	AvHAIDeployableStructureType StructType = GetStructureTypeFromEdict(Task->TaskTarget);

	if (StructType == STRUCTURE_MARINE_PHASEGATE || StructType == STRUCTURE_MARINE_TURRETFACTORY) { return true; }

	return false;
}

bool AITASK_IsMoveTaskUrgent(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	return false; //UTIL_IsNearActiveHive(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(30.0f)) || UTIL_IsAlienPlayerInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(20.0f));
}

AvHAIPlayerTask* BotGetNextTask(AvHAIPlayer* pBot)
{

	// Any orders issued by the commander take priority over everything else
	if (pBot->CommanderTask.TaskType != TASK_NONE)
	{
		if (pBot->SecondaryBotTask.bTaskIsUrgent)
		{
			return &pBot->SecondaryBotTask;
		}
		else
		{
			return &pBot->CommanderTask;
		}
	}

	// Prioritise healing our friends (heal tasks are only valid if the target is close by anyway)
	if (pBot->SecondaryBotTask.TaskType == TASK_HEAL)
	{
		return &pBot->SecondaryBotTask;
	}

	if (AITASK_IsTaskUrgent(pBot, &pBot->WantsAndNeedsTask))
	{
		return &pBot->WantsAndNeedsTask;
	}

	if (AITASK_IsTaskUrgent(pBot, &pBot->PrimaryBotTask))
	{
		return &pBot->PrimaryBotTask;
	}

	if (AITASK_IsTaskUrgent(pBot, &pBot->SecondaryBotTask))
	{
		return &pBot->SecondaryBotTask;
	}

	if (pBot->WantsAndNeedsTask.TaskType != TASK_NONE)
	{
		return &pBot->WantsAndNeedsTask;
	}

	if (pBot->SecondaryBotTask.TaskType != TASK_NONE)
	{
		return &pBot->SecondaryBotTask;
	}

	return &pBot->PrimaryBotTask;
}

bool AITASK_IsTaskCompleted(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return false; }

	switch (Task->TaskType)
	{
	case TASK_MOVE:
		return vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) <= sqrf(max_player_use_reach);
	case TASK_BUILD:
		return !FNullEnt(Task->TaskTarget) && UTIL_StructureIsFullyBuilt(Task->TaskTarget);
	case TASK_ATTACK:
		return FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO);
	case TASK_GUARD:
		return (gpGlobals->time - Task->TaskStartedTime) > Task->TaskLength;
	default:
		return !AITASK_IsTaskStillValid(pBot, Task);
	}

	return false;
}

bool AITASK_IsTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || FNullEnt(pBot->Edict)) { return false; }

	if ((Task->TaskStartedTime > 0.0f && Task->TaskLength > 0.0f) && (gpGlobals->time - Task->TaskStartedTime >= Task->TaskLength)) { return false; }

	switch (Task->TaskType)
	{
	case TASK_NONE:
		return false;
	case TASK_MOVE:
		return AITASK_IsMoveTaskStillValid(pBot, Task);
	case TASK_GET_AMMO:
		return AITASK_IsAmmoPickupTaskStillValid(pBot, Task);
	case TASK_GET_HEALTH:
	{
		if (IsPlayerMarine(pBot->Edict))
		{
			return AITASK_IsHealthPickupTaskStillValid(pBot, Task);
		}
		else
		{
			return AITASK_IsAlienGetHealthTaskStillValid(pBot, Task);
		}
	}
	case TASK_GET_EQUIPMENT:
		return AITASK_IsEquipmentPickupTaskStillValid(pBot, Task);
	case TASK_GET_WEAPON:
		return AITASK_IsWeaponPickupTaskStillValid(pBot, Task);
	case TASK_RESUPPLY:
		return AITASK_IsResupplyTaskStillValid(pBot, Task);
	case TASK_ATTACK:
		return AITASK_IsAttackTaskStillValid(pBot, Task);
	case TASK_GUARD:
		return AITASK_IsGuardTaskStillValid(pBot, Task);
	case TASK_BUILD:
	{
		if (IsPlayerMarine(pBot->Edict))
		{
			return AITASK_IsMarineBuildTaskStillValid(pBot, Task);
		}
		else
		{
			return AITASK_IsAlienBuildTaskStillValid(pBot, Task);
		}
	}
	case TASK_CAP_RESNODE:
	{
		if (IsPlayerMarine(pBot->Edict))
		{
			return AITASK_IsMarineCapResNodeTaskStillValid(pBot, Task);
		}
		else
		{
			return AITASK_IsAlienCapResNodeTaskStillValid(pBot, Task);
		}
	}
	case TASK_REINFORCE_STRUCTURE:
		return AITASK_IsReinforceStructureTaskStillValid(pBot, Task);
	case TASK_DEFEND:
		return AITASK_IsDefendTaskStillValid(pBot, Task);
	case TASK_WELD:
		return AITASK_IsWeldTaskStillValid(pBot, Task);
	case TASK_EVOLVE:
		return AITASK_IsEvolveTaskStillValid(pBot, Task);
	case TASK_HEAL:
		return AITASK_IsAlienHealTaskStillValid(pBot, Task);
	case TASK_USE:
		return AITASK_IsUseTaskStillValid(pBot, Task);
	case TASK_PLACE_MINE:
		return AITASK_IsMineStructureTaskStillValid(pBot, Task);
	case TASK_TOUCH:
		return AITASK_IsTouchTaskStillValid(pBot, Task);
	default:
		return false;
	}

	return false;
}

bool AITASK_IsTouchTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	return (!FNullEnt(Task->TaskTarget) && !IsPlayerTouchingEntity(pBot->Edict, Task->TaskTarget));
}

bool AITASK_IsMoveTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task->TaskLocation) { return false; }

	return (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(max_player_use_reach) || !UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, Task->TaskLocation));
}

bool AITASK_IsWeldTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return false; }
	if (FNullEnt(Task->TaskTarget)) { return false; }
	if (Task->TaskTarget == pBot->Edict) { return false; }
	if (!PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
	{
		if (FNullEnt(Task->TaskSecondaryTarget))
		{
			AvHAIDroppedItem* NearestWelder = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_WELDER, 0.0f, 0.0f, true);

			if (NearestWelder)
			{
				Task->TaskSecondaryTarget = NearestWelder->edict;
			}
			else
			{
				return false;
			}
		}
	}

	if (IsEdictPlayer(Task->TaskTarget))
	{
		if (Task->TaskTarget->v.team != pBot->Edict->v.team || !IsPlayerMarine(Task->TaskTarget) || !IsPlayerActiveInGame(Task->TaskTarget)) { return false; }
		return (Task->TaskTarget->v.armorvalue < GetPlayerMaxArmour(Task->TaskTarget));
	}
	else
	{
		if (IsEdictStructure(Task->TaskTarget))
		{
			if (Task->TaskTarget->v.team != pBot->Edict->v.team || !UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

			return (Task->TaskTarget->v.health < Task->TaskTarget->v.max_health);
		}

		AvHWeldable* WeldableRef = dynamic_cast<AvHWeldable*>(CBaseEntity::Instance(Task->TaskTarget));

		if (WeldableRef)
		{
			return !WeldableRef->GetIsWelded();
		}		
	}

	return false;
}

bool AITASK_IsAmmoPickupTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->Edict) || !IsPlayerActiveInGame(pBot->Edict) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_IsDroppedItemStillReachable(pBot, Task->TaskTarget)) { return false; }

	return (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f))) && (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxAmmoReserve(pBot));
}

bool AITASK_IsHealthPickupTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->Edict) || !IsPlayerActiveInGame(pBot->Edict) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_IsDroppedItemStillReachable(pBot, Task->TaskTarget)) { return false; }

	return ((vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f))) && (pBot->Edict->v.health < pBot->Edict->v.max_health));
}

bool AITASK_IsEquipmentPickupTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->Edict) || !IsPlayerActiveInGame(pBot->Edict) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_IsDroppedItemStillReachable(pBot, Task->TaskTarget)) { return false; }

	return !PlayerHasEquipment(pBot->Edict);
}

bool AITASK_IsWeaponPickupTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->Edict) || !IsPlayerActiveInGame(pBot->Edict) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_IsDroppedItemStillReachable(pBot, Task->TaskTarget)) { return false; }

	AvHAIWeapon WeaponType = UTIL_GetWeaponTypeFromEdict(Task->TaskTarget);

	if (WeaponType == WEAPON_INVALID) { return false; }

	return !PlayerHasWeapon(pBot->Player, WeaponType);
}

bool AITASK_IsAttackTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || vIsZero(Task->TaskTarget->v.origin)) { return false; }

	if ((Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (!UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (IsPlayerSkulk(pBot->Edict))
	{
		if (UTIL_IsStructureElectrified(Task->TaskTarget)) { return false; }
	}

	if (IsPlayerGorge(pBot->Edict) && !PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB)) { return false; }

	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(Task->TaskTarget);

	if (IsPlayerMarine(pBot->Edict))
	{
		if (StructureType == STRUCTURE_ALIEN_HIVE || StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
		{
			if (BotGetPrimaryWeaponClipAmmo(pBot) <= 0 && BotGetPrimaryWeaponAmmoReserve(pBot) <= 0)
			{
				return false;
			}
		}
	}

	return Task->TaskTarget->v.team != pBot->Edict->v.team;

}

bool AITASK_IsResupplyTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->Edict) || !IsPlayerActiveInGame(pBot->Edict) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (!UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(Task->TaskTarget);

	if (StructureType != STRUCTURE_MARINE_ARMOURY && StructureType != STRUCTURE_MARINE_ADVARMOURY) { return false; }

	if (!UTIL_StructureIsFullyBuilt(Task->TaskTarget) || UTIL_StructureIsRecycling(Task->TaskTarget)) { return false; }

	return ((pBot->Edict->v.health < pBot->Edict->v.max_health)
		|| (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxAmmoReserve(pBot))
		|| (BotGetSecondaryWeaponAmmoReserve(pBot) < BotGetSecondaryWeaponMaxAmmoReserve(pBot))
		);
}

bool AITASK_IsGuardTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return false; }

	if (vIsZero(Task->TaskLocation))
	{
		return false;
	}

	return true;
}

bool AITASK_IsMineStructureTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || UTIL_StructureIsRecycling(Task->TaskTarget)) { return false; }

	if (!PlayerHasWeapon(pBot->Player, WEAPON_MARINE_MINES)) { return false; }

	DeployableSearchFilter MineFilter;
	MineFilter.DeployableTypes = STRUCTURE_MARINE_DEPLOYEDMINE;
	MineFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(2.0f);
	MineFilter.bConsiderPhaseDistance = false;
	MineFilter.Team = pBot->Player->GetTeam();

	if (AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &MineFilter) >= 4) { return false; }

	return true;
}

bool AITASK_IsMarineBuildTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(Task->TaskTarget);

	if (StructureType == STRUCTURE_NONE) { return false; }

	if (!UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (UTIL_StructureIsRecycling(Task->TaskTarget))
	{
		return false;
	}

	if (!Task->bIssuedByCommander)
	{
		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea((AvHTeamNumber)pBot->Edict->v.team, Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), false, pBot->Edict, AVH_USER3_NONE);

		if (NumBuilders >= 2)
		{
			return false;
		}
	}

	return !UTIL_StructureIsFullyBuilt(Task->TaskTarget);
}

bool AITASK_IsAlienBuildTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return false; }

	if (!Task->TaskLocation) { return false; }

	if (Task->StructureType == STRUCTURE_NONE) { return false; }

	if (Task->BuildAttempts >= 3) { return false; }

	if (Task->bIsWaitingForBuildLink) { return true; }

	if (!FNullEnt(Task->TaskTarget) && !UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (Task->StructureType == STRUCTURE_ALIEN_HIVE)
	{
		if (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f) { return true; }

		const AvHAIHiveDefinition* HiveIndex = AITAC_GetHiveNearestLocation(Task->TaskLocation);

		if (!HiveIndex) { return false; }

		if (HiveIndex->Status != HIVE_STATUS_UNBUILT) { return false; }

		edict_t* OtherHiveBuilder = nullptr;
		AvHAIPlayer* OtherHiveBuilderBot = GetFirstBotWithBuildTask(pBot->Player->GetTeam(), STRUCTURE_ALIEN_HIVE, pBot->Edict);

		if (OtherHiveBuilderBot)
		{
			OtherHiveBuilder = OtherHiveBuilderBot->Edict;
		}

		if (!FNullEnt(OtherHiveBuilder) && GetPlayerResources(OtherHiveBuilder) > GetPlayerResources(pBot->Edict)) { return false; }

		edict_t* OtherGorge = AITAC_GetNearestPlayerOfClassInArea(pBot->Player->GetTeam(), HiveIndex->Location, UTIL_MetresToGoldSrcUnits(10.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

		if (!FNullEnt(OtherGorge) && GetPlayerResources(OtherGorge) > pBot->Player->GetResources())
		{
			char buf[512];
			sprintf(buf, "I won't drop hive, %s can do it", STRING(OtherGorge->v.netname));
			BotSay(pBot, true, 1.0f, buf);
			return false;
		}

		DeployableSearchFilter EnemyFilter;
		EnemyFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
		EnemyFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		EnemyFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		// Marines have built a phase gate and/or turret factory in the hive
		if (AITAC_DeployableExistsAtLocation(HiveIndex->Location, &EnemyFilter))
		{
			string LocationName;
			char buf[512];

			if (GetNearestMapLocationAtPoint(HiveIndex->Location, LocationName))
			{
				sprintf(buf, "We need to clear %s before I can build the hive", LocationName.c_str());
			}
			else
			{
				sprintf(buf, "We need to clear the hive before I can build it");
			}			
			
			BotSay(pBot, true, 1.0f, buf);
			return false;
		}	

		return true;
	}

	if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER)
	{
		const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

		if (!ResNodeIndex) { return false; }

		if (ResNodeIndex->bIsOccupied)
		{
			if (ResNodeIndex->OwningTeam != pBot->Player->GetTeam()) { return false; } // Node has been capped by the enemy, no longer relevant

			if (!IsPlayerGorge(pBot->Edict)) { return false; } // Don't evolve into a gorge just to finish building an already-placed structure

			if (UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity)) { return false; } // Don't bother if it's already completed

			if (vDist2DSq(pBot->Edict->v.origin, ResNodeIndex->Location) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { return false; } // Only bother finishing the build if you're close enough

			if (FNullEnt(Task->TaskTarget))
			{
				Task->TaskTarget = ResNodeIndex->ActiveTowerEntity;
			}
		}

		edict_t* OtherGorge = AITAC_GetNearestPlayerOfClassInArea(pBot->Player->GetTeam(), Task->TaskLocation, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

		// Check if another player is planning to cap the res node. If they are closer, and either the tower is already placed or they have enough res to place the tower, then move on and do something else
		if (!FNullEnt(OtherGorge))
		{
			if (vDist2DSq(OtherGorge->v.origin, Task->TaskLocation) < vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) && (!FNullEnt(Task->TaskTarget) || (GetPlayerResources(OtherGorge) >= BALANCE_VAR(kResourceTowerCost))))
			{
				return false;
			}
		}

		return true;
	}

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = Task->StructureType;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);
	StructureFilter.Team = pBot->Player->GetTeam();

	// Don't build more if we've already got quite a few in the immediate vicinity. Helps prevent structure spam
	if (AITAC_GetNumDeployablesNearLocation(Task->TaskLocation, &StructureFilter) >= 3)
	{
		return false;
	}

	if (!FNullEnt(Task->TaskTarget))
	{
		if ((Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag == DEAD_DEAD)) { return false; }
		return !UTIL_StructureIsFullyBuilt(Task->TaskTarget);
	}
	else
	{
		if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER) { return true; }

		return UTIL_GetNavAreaAtLocation(Task->TaskLocation) == SAMPLE_POLYAREA_GROUND;
	}
}

bool AITASK_IsAlienCapResNodeTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (vIsZero(Task->TaskLocation))
	{
		return false;
	}

	if (!IsPlayerSkulk(pBot->Edict) && !IsPlayerGorge(pBot->Edict)) { return false; }

	const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex)
	{
		return false;
	}

	// If another gorge is claiming this spot, then move on
	if (!IsPlayerGorge(pBot->Edict) && !ResNodeIndex->bIsOccupied)
	{
		edict_t* OtherBuilder = AITAC_GetNearestPlayerOfClassInArea(pBot->Player->GetTeam(), ResNodeIndex->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

		if (!FNullEnt(OtherBuilder))
		{
			if (GetPlayerResources(OtherBuilder) >= (int)(kResourceTowerCost * 0.7f))
			{
				return false;
			}
		}
	}


	if (ResNodeIndex->bIsOccupied)
	{
		if (IsPlayerGorge(pBot->Edict))
		{
			if (ResNodeIndex->OwningTeam == pBot->Player->GetTeam())
			{
				return !UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity);
			}
			else
			{
				return PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB);
			}

		}
		else
		{
			return (ResNodeIndex->OwningTeam != pBot->Player->GetTeam());
		}
	}
	else
	{
		if (Task->BuildAttempts > 3)
		{
			return false;
		}
	}

	return true;
}

bool AITASK_IsMarineCapResNodeTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || vIsZero(Task->TaskLocation)) { return false; }

	const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return false; }

	// Always obey commander orders even if there's a bunch of other marines already there
	if (!Task->bIssuedByCommander)
	{
		int NumMarinesNearby = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), Task->TaskLocation, UTIL_MetresToGoldSrcUnits(4.0f), false, pBot->Edict, AVH_USER3_NONE);

		if (NumMarinesNearby >= 2 && vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(4.0f))) { return false; }
	}

	if (ResNodeIndex->bIsOccupied)
	{
		if (ResNodeIndex->OwningTeam == pBot->Player->GetTeam() && !FNullEnt(ResNodeIndex->ActiveTowerEntity))
		{
			return !UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity);
		}
		else
		{
			return true;
		}
	}

	return true;
}

bool AITASK_IsDefendTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (GetStructureTypeFromEdict(Task->TaskTarget) == STRUCTURE_NONE) { return false; }

	if (!UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (Task->TaskTarget->v.team != pBot->Edict->v.team) { return false; }

	int NumExistingDefenders = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

	if (NumExistingDefenders >= 2) { return false; }

	if (gpGlobals->time - pBot->LastCombatTime < 5.0f) { return true; }

	if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && gpGlobals->time - pBot->LastCombatTime > 10.0f) { return false; }

	return true;
}

bool AITASK_IsReinforceStructureTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (!FNullEnt(Task->TaskSecondaryTarget) && !UTIL_StructureIsFullyBuilt(Task->TaskSecondaryTarget)) { return true; }

	if (Task->TaskTarget->v.team != pBot->Player->GetTeam()) { return false; }

	bool bActiveHiveWithoutTechExists = AITAC_TeamHiveWithTechExists(pBot->Player->GetTeam(), MESSAGE_NULL);

	if (bActiveHiveWithoutTechExists) { return true; }

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);
	StructureFilter.Team = pBot->Player->GetTeam();

	// At least 2 offence chambers
	int NumOffenceChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);

	if (NumOffenceChambers < 2) { return true; }

	// At least 2 defence chambers, if the hive exists for it
	if (AITAC_TeamHiveWithTechExists(pBot->Player->GetTeam(), ALIEN_BUILD_DEFENSE_CHAMBER))
	{
		StructureFilter.DeployableTypes = STRUCTURE_ALIEN_DEFENCECHAMBER;
		int NumDefenceChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);

		if (NumDefenceChambers < 2) { return true; }

		StructureFilter.MaxSearchRadius = 0.0f;
		int NumTotalDefenceChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);

		if (NumTotalDefenceChambers < 3) { return true; }
	}

	// At least 1 movement and sensory chamber, if the hive exists for them
	if (AITAC_TeamHiveWithTechExists(pBot->Player->GetTeam(), ALIEN_BUILD_MOVEMENT_CHAMBER))
	{
		StructureFilter.DeployableTypes = STRUCTURE_ALIEN_MOVEMENTCHAMBER;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

		bool bHasMoveChamber = AITAC_DeployableExistsAtLocation(Task->TaskTarget->v.origin, &StructureFilter);

		if (!bHasMoveChamber) { return true; }

		StructureFilter.MaxSearchRadius = 0.0f;
		int NumTotalMoveChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);

		if (NumTotalMoveChambers < 3) { return true; }
	}

	if (AITAC_TeamHiveWithTechExists(pBot->Player->GetTeam(), ALIEN_BUILD_SENSORY_CHAMBER))
	{
		StructureFilter.DeployableTypes = STRUCTURE_ALIEN_SENSORYCHAMBER;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

		bool bHasSensoryChamber = AITAC_DeployableExistsAtLocation(Task->TaskTarget->v.origin, &StructureFilter);

		if (!bHasSensoryChamber) { return true; }

		StructureFilter.MaxSearchRadius = 0.0f;
		int NumTotalSensoryChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);

		if (NumTotalSensoryChambers < 3) { return true; }
	}

	// We have all available chambers set up
	return false;
}

bool AITASK_IsEvolveTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || Task->Evolution == MESSAGE_NULL || !IsPlayerAlien(pBot->Edict)) { return false; }

	switch (Task->Evolution)
	{
	case ALIEN_LIFEFORM_ONE:
		return !IsPlayerSkulk(pBot->Edict);
	case ALIEN_LIFEFORM_TWO:
		return !IsPlayerGorge(pBot->Edict) && pBot->Player->GetResources() >= BALANCE_VAR(kGorgeCost);
	case ALIEN_LIFEFORM_THREE:
		return !IsPlayerLerk(pBot->Edict) && pBot->Player->GetResources() >= BALANCE_VAR(kLerkCost);
	case ALIEN_LIFEFORM_FOUR:
		return !IsPlayerFade(pBot->Edict) && pBot->Player->GetResources() >= BALANCE_VAR(kFadeCost);
	case ALIEN_LIFEFORM_FIVE:
		return !IsPlayerOnos(pBot->Edict) && pBot->Player->GetResources() >= BALANCE_VAR(kOnosCost) && (AITAC_GetNumPlayersOnTeamOfClass(pBot->Player->GetTeam(), AVH_USER3_ALIEN_PLAYER5, nullptr) < 2);
	default:
		return false;
	}

	return false;
}

bool AITASK_IsAlienGetHealthTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (IsEdictStructure(Task->TaskTarget) && !UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (IsEdictPlayer(Task->TaskTarget))
	{
		if (!IsPlayerGorge(Task->TaskTarget)) { return false; }
	}
	return (pBot->Edict->v.health < pBot->Edict->v.max_health) || (!IsPlayerSkulk(pBot->Edict) && pBot->Edict->v.armorvalue < (GetPlayerMaxArmour(pBot->Edict) * 0.7f));
}

bool AITASK_IsAlienHealTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (!IsPlayerGorge(pBot->Edict)) { return false; }

	if (GetPlayerOverallHealthPercent(Task->TaskTarget) >= 0.99f) { return false; }

	if (IsEdictStructure(Task->TaskTarget)) { return true; }

	// If our target is a player, give up if they are too far away. I'm not going to waste time chasing you around the map!
	float MaxHealRelevant = sqrf(UTIL_MetresToGoldSrcUnits(5.0f));

	return (vDist2DSq(pBot->CurrentFloorPosition, Task->TaskTarget->v.origin) <= MaxHealRelevant);
}

bool AITASK_IsUseTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	CBaseToggle* ToggleRef = dynamic_cast<CBaseToggle*>(CBaseEntity::Instance(Task->TaskTarget));

	if (!ToggleRef) { return false; }

	return ToggleRef->GetToggleState() == TS_AT_BOTTOM;
}


void BotProgressMoveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	Task->TaskStartedTime = gpGlobals->time;

	if (IsPlayerLerk(pBot->Edict))
	{
		if (AITAC_ShouldBotBeCautious(pBot))
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_HIDE, 100.0f);
		}
		else
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL, 100.0f);
		}

		return;
	}

	MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);

	if (IsPlayerMarine(pBot->Edict))
	{
		if (gpGlobals->time - pBot->LastCombatTime > 5.0f)
		{
			BotReloadWeapons(pBot);
		}
	}
}

void BotProgressTouchTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
}

void BotProgressUseTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (IsPlayerInUseRange(pBot->Edict, Task->TaskTarget))
	{
		if (pBot->Edict->v.oldbuttons & IN_DUCK)
		{
			pBot->Button |= IN_DUCK;
		}

		BotUseObject(pBot, Task->TaskTarget, false);
		return;
	}
	else
	{
		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) < sqrf(18.0f))
		{
			if (pBot->Edict->v.origin.z < UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, Task->TaskTarget).z)
			{
				BotJump(pBot);
			}
			else
			{
				pBot->Button |= IN_DUCK;
			}
		}

		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
	}
}

void BotProgressPickupTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (Task->TaskType == TASK_GET_AMMO)
	{
		pBot->DesiredCombatWeapon = UTIL_GetBotPrimaryWeapon(pBot);
	}

	MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

	Task->TaskStartedTime = gpGlobals->time;

	float DistFromItem = vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin);

	if (DistFromItem < sqrf(UTIL_MetresToGoldSrcUnits(1.0f)))
	{
		BotLookAt(pBot, Task->TaskTarget);

		if (Task->TaskType == TASK_GET_WEAPON)
		{
			AvHAIDeployableItemType ItemType = UTIL_GetItemTypeFromEdict(Task->TaskTarget);

			// Allows bots to drop their current primary weapon to pick up a new weapon
			if (UTIL_DroppedItemIsPrimaryWeapon(ItemType))
			{
				AvHAIWeapon CurrentPrimaryWeapon = UTIL_GetBotPrimaryWeapon(pBot);

				if (CurrentPrimaryWeapon != WEAPON_NONE && CurrentPrimaryWeapon != UTIL_GetWeaponTypeFromEdict(Task->TaskTarget))
				{
					if (GetBotCurrentWeapon(pBot) != CurrentPrimaryWeapon)
					{
						pBot->DesiredCombatWeapon = CurrentPrimaryWeapon;
					}
					else
					{
						BotDropWeapon(pBot);
					}
				}
			}
		}
	}
}

void BotProgressMineStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	float DistToPlaceLocation = vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation);

	if (DistToPlaceLocation < sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
	{
		pBot->DesiredCombatWeapon = WEAPON_MARINE_MINES;
	}

	if (!FNullEnt(Task->TaskSecondaryTarget))
	{
		Task->TaskLocation = g_vecZero;
		Task->TaskSecondaryTarget = nullptr;
		Task->BuildAttempts = 0;
		return;
	}

	if (Task->bIsWaitingForBuildLink)
	{
		
		if (gpGlobals->time - Task->LastBuildAttemptTime > 1.0f)
		{
			Task->bIsWaitingForBuildLink = false;
			if (Task->BuildAttempts > 3)
			{
				float Size = fmaxf(Task->TaskTarget->v.size.x, Task->TaskTarget->v.size.y);
				Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE], Task->TaskTarget->v.origin, Size + 8.0f);
			}
			else
			{
				Vector Dir = UTIL_GetVectorNormal2D(Task->TaskLocation - Task->TaskTarget->v.origin);
				Task->TaskLocation = Task->TaskLocation + (Dir * 8.0f);
			}
		}
		return;
	}

	if (vIsZero(Task->TaskLocation))
	{
		Task->TaskLocation = UTIL_GetNextMinePosition(Task->TaskTarget);

		if (vIsZero(Task->TaskLocation))
		{
			AITASK_ClearBotTask(pBot, Task);
			return;
		}
	}	

	if (DistToPlaceLocation < sqrf(16.0f))
	{
		Vector MoveDir = UTIL_GetVectorNormal2D(Task->TaskLocation - pBot->Edict->v.origin);
		MoveDirectlyTo(pBot, Task->TaskLocation - (MoveDir * 28.0f));
		return;
	}

	if (DistToPlaceLocation > sqrf(32.0f))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	BotLookAt(pBot, Task->TaskLocation);

	if (GetBotCurrentWeapon(pBot) == WEAPON_MARINE_MINES)
	{
		float LookDot = UTIL_GetDotProduct(UTIL_GetForwardVector(pBot->Edict->v.v_angle), UTIL_GetVectorNormal(Task->TaskLocation - pBot->CurrentEyePosition));

		if (LookDot > 0.95f)
		{
			pBot->Button |= IN_ATTACK;
			Task->LastBuildAttemptTime = gpGlobals->time;
			Task->BuildAttempts++;
			Task->bIsWaitingForBuildLink = true;
		}
	}
}

void BotProgressReinforceStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget)) { return; }

	if (!FNullEnt(Task->TaskSecondaryTarget))
	{

		if (UTIL_StructureIsFullyBuilt(Task->TaskSecondaryTarget))
		{
			Task->TaskSecondaryTarget = nullptr;
			Task->BuildAttempts = 0;
			Task->bIsWaitingForBuildLink = false;
			Task->TaskLocation = g_vecZero;
		}
		else
		{
			if (IsPlayerInUseRange(pBot->Edict, Task->TaskSecondaryTarget))
			{
				BotUseObject(pBot, Task->TaskSecondaryTarget, true);
				if (vDist2DSq(pBot->Edict->v.origin, Task->TaskSecondaryTarget->v.origin) > sqrf(60.0f))
				{
					MoveDirectlyTo(pBot, Task->TaskSecondaryTarget->v.origin);
				}
				return;
			}

			MoveTo(pBot, Task->TaskSecondaryTarget->v.origin, MOVESTYLE_NORMAL);

			return;
		}		
	}

	if (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f)
	{
		return;
	}

	if (Task->bIsWaitingForBuildLink)
	{
		Task->TaskLocation = g_vecZero;
		Task->bIsWaitingForBuildLink = false;
	}

	AvHMessageID HiveTechOne = CONFIG_GetHiveTechAtIndex(0);
	AvHMessageID HiveTechTwo = CONFIG_GetHiveTechAtIndex(1);
	AvHMessageID HiveTechThree = CONFIG_GetHiveTechAtIndex(2);

	AvHAIDeployableStructureType ChamberTypeOne = UTIL_GetChamberTypeForHiveTech(HiveTechOne);
	AvHAIDeployableStructureType ChamberTypeTwo = UTIL_GetChamberTypeForHiveTech(HiveTechTwo);
	AvHAIDeployableStructureType ChamberTypeThree = UTIL_GetChamberTypeForHiveTech(HiveTechThree);

	bool bActiveHiveWithoutTechExists = AITAC_TeamHiveWithTechExists(pBot->Player->GetTeam(), MESSAGE_NULL);
	bool bActiveHiveWithTechOneExists = AITAC_TeamHiveWithTechExists(pBot->Player->GetTeam(), HiveTechOne);
	bool bActiveHiveWithTechTwoExists = AITAC_TeamHiveWithTechExists(pBot->Player->GetTeam(), HiveTechTwo);
	bool bActiveHiveWithTechThreeExists = AITAC_TeamHiveWithTechExists(pBot->Player->GetTeam(), HiveTechThree);

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = ChamberTypeOne;
	StructureFilter.Team = pBot->Player->GetTeam();

	int NumHiveTechOne = AITAC_GetNumDeployablesNearLocation(ZERO_VECTOR, &StructureFilter);
	StructureFilter.DeployableTypes = ChamberTypeTwo;
	int NumHiveTechTwo = AITAC_GetNumDeployablesNearLocation(ZERO_VECTOR, &StructureFilter);
	StructureFilter.DeployableTypes = ChamberTypeThree;
	int NumHiveTechThree = AITAC_GetNumDeployablesNearLocation(ZERO_VECTOR, &StructureFilter);

	if (Task->StructureType == STRUCTURE_NONE)
	{
		if (bActiveHiveWithoutTechExists)
		{
			if (!bActiveHiveWithTechOneExists)
			{
				Task->StructureType = ChamberTypeOne;
			}
			else if (!bActiveHiveWithTechTwoExists)
			{
				Task->StructureType = ChamberTypeTwo;
			}
			else if (!bActiveHiveWithTechThreeExists)
			{
				Task->StructureType = ChamberTypeThree;
			}
		}
		else
		{
			if (bActiveHiveWithTechOneExists && NumHiveTechOne < 3)
			{
				Task->StructureType = ChamberTypeOne;
			}
			else if (bActiveHiveWithTechTwoExists && NumHiveTechTwo < 3)
			{
				Task->StructureType = ChamberTypeTwo;
			}
			else if (bActiveHiveWithTechThreeExists && NumHiveTechThree < 3)
			{
				Task->StructureType = ChamberTypeThree;
			}
			else
			{
				Task->StructureType = STRUCTURE_ALIEN_OFFENCECHAMBER;
			}			
		}

	}

	bool bCanBuildChamberTypeOne = bActiveHiveWithoutTechExists || bActiveHiveWithTechOneExists;
	bool bCanBuildChamberTypeTwo = bActiveHiveWithoutTechExists || bActiveHiveWithTechTwoExists;
	bool bCanBuildChamberTypeThree = bActiveHiveWithoutTechExists || bActiveHiveWithTechThreeExists;

	StructureFilter.DeployableTypes = Task->StructureType;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

	int NumChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);
	int NumDesiredChambers = (Task->StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER || Task->StructureType == STRUCTURE_ALIEN_DEFENCECHAMBER) ? 2 : 1;

	if (Task->StructureType != STRUCTURE_ALIEN_OFFENCECHAMBER)
	{
		// The idea here is the gorge builds however many chambers at this hive/RT are needed to meet the minimum
		// reinforce requirements, or ensure 3 of each upgrade chamber are built, whichever is more.
		// e.g. normally they only build 1 MC/SC per hive/RT, but will build 3 if needed to ensure a min 3 of each chamber

		int Deficit = 0;

		if (Task->StructureType == ChamberTypeOne)
		{
			Deficit = clampi((3 - NumHiveTechOne), 0, 3);
		}

		if (Task->StructureType == ChamberTypeTwo)
		{
			Deficit = clampi((3 - NumHiveTechTwo), 0, 3);
		}

		if (Task->StructureType == ChamberTypeThree)
		{
			Deficit = clampi((3 - NumHiveTechThree), 0, 3);
		}

		Deficit += NumChambers;

		NumDesiredChambers = imaxi(Deficit, NumDesiredChambers);
	}

	if (NumChambers >= NumDesiredChambers)
	{
		bool bChosenNextChamber = false;

		if (!bActiveHiveWithoutTechExists)
		{
			StructureFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;

			int NumOffenceChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);

			if (NumOffenceChambers < 2)
			{
				Task->StructureType = STRUCTURE_ALIEN_OFFENCECHAMBER;
				bChosenNextChamber = true;
			}
		}

		if (!bChosenNextChamber && bCanBuildChamberTypeOne)
		{
			StructureFilter.DeployableTypes = ChamberTypeOne;

			int NumDefenceChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);

			if (NumDefenceChambers < 2)
			{
				Task->StructureType = ChamberTypeOne;
				bChosenNextChamber = true;
			}
		}

		if (!bChosenNextChamber && bCanBuildChamberTypeTwo)
		{
			StructureFilter.DeployableTypes = ChamberTypeTwo;

			int NumMoveChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);

			if (NumMoveChambers < 1)
			{
				Task->StructureType = ChamberTypeTwo;
				bChosenNextChamber = true;
			}
		}

		if (!bChosenNextChamber && bCanBuildChamberTypeThree)
		{
			StructureFilter.DeployableTypes = ChamberTypeThree;

			int NumSensoryChambers = AITAC_GetNumDeployablesNearLocation(Task->TaskTarget->v.origin, &StructureFilter);

			if (NumSensoryChambers < 1)
			{
				Task->StructureType = ChamberTypeThree;
				bChosenNextChamber = true;
			}
		}
		if (!bChosenNextChamber) { return; }
	}

	if (Task->TaskLocation != g_vecZero)
	{
		dtPolyRef Poly = UTIL_GetNavAreaAtLocation(BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE], Task->TaskLocation);

		if (Poly != SAMPLE_POLYAREA_GROUND)
		{
			Task->TaskLocation = g_vecZero;
		}
	}

	if (vIsZero(Task->TaskLocation))
	{
		AvHAIDeployableStructureType ReinforcedStructure = GetStructureTypeFromEdict(Task->TaskTarget);

		Vector TargetLocation = (ReinforcedStructure == STRUCTURE_ALIEN_HIVE) ? UTIL_GetFloorUnderEntity(Task->TaskTarget) : Task->TaskTarget->v.origin;

		Vector BuildLocation = FindClosestNavigablePointToDestination(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(GetGameRules()->GetTeamANumber()), TargetLocation, UTIL_MetresToGoldSrcUnits(50.0f));

		if (BuildLocation != g_vecZero)
		{
			float currDist = vDist2D(BuildLocation, TargetLocation);

			float MaxDist = fmaxf(UTIL_MetresToGoldSrcUnits(1.0f), (UTIL_MetresToGoldSrcUnits(5.0f) - currDist));

			Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], BuildLocation, MaxDist);
		}

		if (vIsZero(Task->TaskLocation)) { return; }
	}

	int ResRequired = UTIL_GetCostOfStructureType(Task->StructureType);

	if (!IsPlayerGorge(pBot->Edict))
	{
		ResRequired += BALANCE_VAR(kGorgeCost);
	}

	if (pBot->Player->GetResources() < ResRequired)
	{
		BotGuardLocation(pBot, Task->TaskLocation);
		return;
	}

	if (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(max_player_use_reach))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	if (!IsPlayerGorge(pBot->Edict))
	{
		AITASK_SetEvolveTask(pBot, &pBot->WantsAndNeedsTask, pBot->Edict->v.origin, ALIEN_LIFEFORM_TWO, true);
		return;
	}

	Vector LookLocation = Task->TaskLocation;
	LookLocation.z += 10.0f;

	BotLookAt(pBot, LookLocation);

	float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->Edict->v.v_angle), UTIL_GetVectorNormal2D(LookLocation - pBot->Edict->v.origin));

	if (LookDot > 0.9f)
	{
		pBot->Impulse = UTIL_StructureTypeToImpulseCommand(Task->StructureType);
		Task->LastBuildAttemptTime = gpGlobals->time;
		Task->BuildAttempts++;
		Task->bIsWaitingForBuildLink = true;
	}

}

void BotProgressResupplyTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxAmmoReserve(pBot))
	{
		pBot->DesiredCombatWeapon = UTIL_GetBotPrimaryWeapon(pBot);
	}
	else
	{
		pBot->DesiredCombatWeapon = GetBotMarineSecondaryWeapon(pBot);
	}

	if (UTIL_PlayerHasLOSToEntity(pBot->Edict, Task->TaskTarget, max_player_use_reach, false))
	{
		BotUseObject(pBot, Task->TaskTarget, true);
		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) > sqrf(50.0f))
		{
			MoveDirectlyTo(pBot, Task->TaskTarget->v.origin);
		}
		return;
	}

	MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

	if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		BotLookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
	}

}

void MarineProgressBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	edict_t* pEdict = pBot->Edict;

	if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
	{
		pBot->DesiredCombatWeapon = UTIL_GetBotPrimaryWeapon(pBot);
	}
	else
	{
		if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			pBot->DesiredCombatWeapon = GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			pBot->DesiredCombatWeapon = UTIL_GetBotPrimaryWeapon(pBot);
		}
	}

	// If we're not already building
	if (pBot->Edict->v.viewmodel != 0)
	{
		// If someone else is building, then we will guard
		edict_t* OtherBuilder = AITAC_GetClosestPlayerOnTeamWithLOS(pBot->Player->GetTeam(), Task->TaskLocation, UTIL_MetresToGoldSrcUnits(2.0f), pBot->Edict);

		if (!FNullEnt(OtherBuilder) && OtherBuilder->v.weaponmodel == 0)
		{
			BotGuardLocation(pBot, Task->TaskLocation);
			return;
		}
	}

	if (IsPlayerInUseRange(pBot->Edict, Task->TaskTarget))
	{
		// If we were ducking before then keep ducking
		if (pBot->Edict->v.oldbuttons & IN_DUCK)
		{
			pBot->Button |= IN_DUCK;
		}

		BotUseObject(pBot, Task->TaskTarget, true);

		// Haven't started building, maybe not quite looking at the right angle
		if (pBot->Edict->v.weaponmodel != 0)
		{
			if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) > sqrf(60.0f))
			{
				MoveDirectlyTo(pBot, Task->TaskTarget->v.origin);
			}
			else
			{
				Vector NewViewPoint = UTIL_GetRandomPointInBoundingBox(Task->TaskTarget->v.absmin, Task->TaskTarget->v.absmax);

				BotLookAt(pBot, NewViewPoint);
			}
		}

		return;
	}
	else
	{
		// Might need to duck if it's an infantry portal
		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(max_player_use_reach))
		{
			if (Task->TaskTarget->v.origin > pEdict->v.origin)
			{
				BotJump(pBot);
			}
			else
			{
				pBot->Button |= IN_DUCK;
			}
			
		}
	}

	MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

	if (IsPlayerMarine(pBot->Edict))
	{
		if (gpGlobals->time - pBot->LastCombatTime > 5.0f)
		{
			BotReloadWeapons(pBot);
		}
	}

	if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		BotLookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
	}

}

void BotProgressGuardTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (IsPlayerMarine(pBot->Edict))
	{
		if (gpGlobals->time - pBot->LastCombatTime > 5.0f)
		{
			BotReloadWeapons(pBot);
		}
	}

	if (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		if (IsPlayerLerk(pBot->Edict))
		{
			if (AITAC_ShouldBotBeCautious(pBot))
			{
				MoveTo(pBot, Task->TaskLocation, MOVESTYLE_HIDE, 100.0f);
			}
			else
			{
				MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL, 100.0f);
			}

			return;
		}


		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}
	else
	{
		if (Task->TaskStartedTime == 0.0f)
		{
			Task->TaskStartedTime = gpGlobals->time;
		}
		BotGuardLocation(pBot, Task->TaskLocation);
	}
}

void BotProgressAttackTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || FNullEnt(Task->TaskTarget)) { return; }

	if (Task->bTargetIsPlayer)
	{
		// For now just move to the target, the combat code will take over once the enemy is sighted
		MoveTo(pBot, UTIL_GetEntityGroundLocation(Task->TaskTarget), MOVESTYLE_AMBUSH);
		return;
	}


	BotAttackTarget(pBot, Task->TaskTarget);
	return;
}

void BotProgressDefendTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	BotProgressGuardTask(pBot, Task);

	if (!FNullEnt(Task->TaskTarget))
	{
		AvHAIBuildableStructure* StructureRef = AITAC_GetDeployableRefFromEdict(Task->TaskTarget);

		if (!StructureRef) { return; }

		// If the structure we're defending was damaged just now, look at it so we can see who is attacking
		if (gpGlobals->time - StructureRef->lastDamagedTime < 5.0f)
		{
			if (UTIL_QuickTrace(pBot->Edict, pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(Task->TaskTarget)))
			{
				BotLookAt(pBot, Task->TaskTarget);
			}
		}
	}
}

void BotProgressTakeCommandTask(AvHAIPlayer* pBot)
{
	edict_t* CommChair = AITAC_GetCommChair(pBot->Player->GetTeam());

	if (!CommChair) { return; }

	float DistFromChair = vDist2DSq(pBot->Edict->v.origin, CommChair->v.origin);

	if (!IsPlayerInUseRange(pBot->Edict, CommChair))
	{
		MoveTo(pBot, CommChair->v.origin, MOVESTYLE_NORMAL);

		if (DistFromChair < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			BotLookAt(pBot, CommChair);
		}
	}
	else
	{
		float CommanderWaitTime = CONFIG_GetCommanderWaitTime();

		if ((gpGlobals->time - GetGameRules()->GetTimeGameStarted()) > CommanderWaitTime)
		{
			BotUseObject(pBot, CommChair, false);
		}
		else
		{
			edict_t* NearestHuman = AITAC_GetNearestHumanAtLocation(pBot->Player->GetTeam(), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

			if (FNullEnt(NearestHuman))
			{
				BotUseObject(pBot, CommChair, false);
			}
			else
			{
				BotLookAt(pBot, NearestHuman);
			}
		}

	}
}

void BotProgressEvolveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || !Task->Evolution) { return; }

	// We tried evolving a second ago and nothing happened. Must be in a bad spot
	if (Task->TaskStartedTime > 0.0f)
	{
		if ((gpGlobals->time - Task->TaskStartedTime) > 1.0f)
		{
			Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE], pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (vIsZero(Task->TaskLocation))
			{
				Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
			}

			Task->TaskStartedTime = 0.0f;
		}
		return;
	}

	if (Task->TaskLocation != g_vecZero)
	{

		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(32.0f))
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
			return;
		}
		else
		{
			pBot->Impulse = Task->Evolution;
			Task->TaskStartedTime = gpGlobals->time;
		}
	}
	else
	{
		if (FNullEnt(Task->TaskTarget))
		{
			Task->TaskLocation = pBot->Edict->v.origin;
			return;
		}
		else
		{
			if (vDist2DSq(pBot->Edict->v.origin, UTIL_GetEntityGroundLocation(Task->TaskTarget)) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)) || UTIL_GetNavAreaAtLocation(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], pBot->Edict->v.origin) != SAMPLE_POLYAREA_GROUND)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(Task->TaskTarget), MOVESTYLE_NORMAL);
				return;
			}
			else
			{
				Task->TaskLocation = FindClosestNavigablePointToDestination(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], pBot->Edict->v.origin, UTIL_GetEntityGroundLocation(Task->TaskTarget), UTIL_MetresToGoldSrcUnits(10.0f));

				if (vIsZero(Task->TaskLocation))
				{
					Task->TaskLocation = pBot->Edict->v.origin;
				}

				if (Task->TaskLocation != g_vecZero)
				{
					Vector FinalEvolveLoc = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], Task->TaskLocation, UTIL_MetresToGoldSrcUnits(5.0f));

					if (FinalEvolveLoc != g_vecZero)
					{
						Task->TaskLocation = FinalEvolveLoc;
						return;
					}
				}
			}
		}
	}
}

void AlienProgressGetHealthTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{

	if (PlayerHasWeapon(pBot->Player, WEAPON_FADE_METABOLIZE))
	{
		pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;
		if (GetBotCurrentWeapon(pBot) == WEAPON_FADE_METABOLIZE)
		{
			pBot->Button |= IN_ATTACK;
		}
	}

	if (!FNullEnt(Task->TaskTarget))
	{
		Vector MoveLocation = (IsPlayerGorge(Task->TaskTarget)) ? Task->TaskTarget->v.origin : Task->TaskLocation;

		BotGuardLocation(pBot, MoveLocation);

		if (IsPlayerGorge(Task->TaskTarget) && vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
			BotLookAt(pBot, Task->TaskTarget);

			if (gpGlobals->time - pBot->LastRequestTime > min_request_spam_time)
			{
				pBot->Impulse = SAYING_4;
				pBot->LastRequestTime = gpGlobals->time;
			}
		}
	}
}

void AlienProgressHealTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!IsPlayerGorge(pBot->Edict) || FNullEnt(Task->TaskTarget) || IsPlayerDead(Task->TaskTarget)) { return; }

	float DesiredDist = (IsEdictStructure(Task->TaskTarget)) ? kHealingSprayRange : (kHealingSprayRange * 0.5f);

	BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, WEAPON_GORGE_HEALINGSPRAY, Task->TaskTarget);

	if (LOSCheck == ATTACK_SUCCESS)
	{
		pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;
		BotLookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
		if (GetBotCurrentWeapon(pBot) == WEAPON_GORGE_HEALINGSPRAY)
		{
			pBot->Button |= IN_ATTACK;
		}

		return;
	}
	else
	{
		MoveTo(pBot, UTIL_GetEntityGroundLocation(Task->TaskTarget), MOVESTYLE_NORMAL, kHealingSprayRange);
	}
}

void AlienProgressBuildHiveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f)
	{
		return;
	}

	const AvHAIHiveDefinition* Hive = AITAC_GetHiveNearestLocation(Task->TaskLocation);

	if (!Hive)
	{
		AITASK_ClearBotTask(pBot, Task);
		return;
	}

	if (Task->bIsWaitingForBuildLink)
	{
		Task->TaskLocation = FindClosestNavigablePointToDestination(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], pBot->Edict->v.origin, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(7.5f));

		if (vIsZero(Task->TaskLocation))
		{
			AITASK_ClearBotTask(pBot, Task);
			return;
		}

		float Dist = vDist2D(Task->TaskLocation, Hive->Location);

		float MaxDist = (UTIL_MetresToGoldSrcUnits(7.5f) - Dist);

		Vector AltLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], Task->TaskLocation, MaxDist);

		if (AltLocation != g_vecZero)
		{
			Task->TaskLocation = AltLocation;
		}

		Task->bIsWaitingForBuildLink = false;
	}

	int ResRequired = UTIL_GetCostOfStructureType(Task->StructureType);

	if (!IsPlayerGorge(pBot->Edict))
	{
		ResRequired += BALANCE_VAR(kGorgeCost);
	}

	if (pBot->Player->GetResources() < ResRequired)
	{
		BotGuardLocation(pBot, Task->TaskLocation);
		return;
	}

	if (vDist2DSq(pBot->Edict->v.origin, Hive->Location) > sqrf(UTIL_MetresToGoldSrcUnits(7.5f)))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	if (!IsPlayerGorge(pBot->Edict))
	{
		if (pBot->WantsAndNeedsTask.TaskType != TASK_EVOLVE || pBot->WantsAndNeedsTask.Evolution != ALIEN_LIFEFORM_TWO)
		{
			AITASK_SetEvolveTask(pBot, &pBot->WantsAndNeedsTask, pBot->Edict->v.origin, ALIEN_LIFEFORM_TWO, true);
		}

		return;
	}

	BotMoveLookAt(pBot, Hive->Location);

	Vector TraceStart = GetPlayerEyePosition(pBot->Edict);
	Vector TraceEnd = TraceStart + (UTIL_GetForwardVector(pBot->Edict->v.v_angle) * 60.0f);

	if (UTIL_QuickTrace(pBot->Edict, TraceStart, TraceEnd))
	{
		pBot->Impulse = UTIL_StructureTypeToImpulseCommand(Task->StructureType);

		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(max_player_use_reach))
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
			return;
		}
		else
		{
			Task->LastBuildAttemptTime = gpGlobals->time;
			Task->BuildAttempts++;
			Task->bIsWaitingForBuildLink = true;
			Task->bTaskIsUrgent = true;
		}
	}
}

void AlienProgressBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (Task->StructureType == STRUCTURE_ALIEN_HIVE)
	{
		AlienProgressBuildHiveTask(pBot, Task);
		return;
	}

	if (!FNullEnt(Task->TaskTarget))
	{

		if (IsPlayerInUseRange(pBot->Edict, Task->TaskTarget))
		{
			BotUseObject(pBot, Task->TaskTarget, true);
			if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) > sqrf(60.0f))
			{
				MoveDirectlyTo(pBot, Task->TaskTarget->v.origin);
			}
			return;
		}

		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

		return;
	}

	if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER)
	{
		const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

		if (ResNodeIndex)
		{
			if (ResNodeIndex->bIsOccupied && !ResNodeIndex->OwningTeam == pBot->Player->GetTeam())
			{
				Task->TaskTarget = ResNodeIndex->ActiveTowerEntity;
				return;
			}
		}
	}

	if (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f)
	{
		return;
	}

	if (Task->bIsWaitingForBuildLink)
	{
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], Task->TaskLocation, UTIL_MetresToGoldSrcUnits(2.0f));
		Task->bIsWaitingForBuildLink = false;
	}

	// If we are building a chamber
	if (Task->StructureType != STRUCTURE_ALIEN_RESTOWER)
	{
		dtPolyRef Poly = UTIL_GetNavAreaAtLocation(BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE], Task->TaskLocation);

		if (Poly != SAMPLE_POLYAREA_GROUND)
		{
			Vector NewLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE], Task->TaskLocation, UTIL_MetresToGoldSrcUnits(2.0f));

			if (NewLocation != g_vecZero)
			{
				Task->TaskLocation = NewLocation;
				return;
			}
		}
	}

	int ResRequired = UTIL_GetCostOfStructureType(Task->StructureType);

	if (!IsPlayerGorge(pBot->Edict))
	{
		ResRequired += BALANCE_VAR(kGorgeCost);
	}

	if (pBot->Player->GetResources() < ResRequired)
	{
		BotGuardLocation(pBot, Task->TaskLocation);
		return;
	}

	if (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(max_player_use_reach))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	if (!IsPlayerGorge(pBot->Edict))
	{
		AITASK_SetEvolveTask(pBot, &pBot->WantsAndNeedsTask, pBot->Edict->v.origin, ALIEN_LIFEFORM_TWO, true);
		
		return;
	}

	Vector LookLocation = Task->TaskLocation;
	LookLocation.z += 10.0f;

	if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER)
	{
		const AvHAIResourceNode* ResNode = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

		if (ResNode)
		{
			LookLocation = ResNode->Location;
		}
	}

	BotLookAt(pBot, LookLocation);

	float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->Edict->v.v_angle), UTIL_GetVectorNormal2D(LookLocation - pBot->Edict->v.origin));

	if (LookDot > 0.9f)
	{
		pBot->Impulse = UTIL_StructureTypeToImpulseCommand(Task->StructureType);
		Task->LastBuildAttemptTime = gpGlobals->time;
		Task->BuildAttempts++;
		Task->bIsWaitingForBuildLink = true;
		Task->bTaskIsUrgent = true;
	}

	return;
}

void AlienProgressCapResNodeTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return; }

	int NumResourcesRequired = (IsPlayerGorge(pBot->Edict) ? BALANCE_VAR(kResourceTowerCost) : (BALANCE_VAR(kResourceTowerCost) + BALANCE_VAR(kGorgeCost)));

	float DistFromNode = vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation);

	if (DistFromNode > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)) || !UTIL_QuickTrace(pBot->Edict, pBot->CurrentEyePosition, (Task->TaskLocation + Vector(0.0f, 0.0f, 50.0f))))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	if (ResNodeIndex->bIsOccupied)
	{
		Task->TaskTarget = ResNodeIndex->ActiveTowerEntity;

		if (ResNodeIndex->OwningTeam != pBot->Player->GetTeam())
		{

			if (IsPlayerGorge(pBot->Edict) && !PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB))
			{
				int NumAllies = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), Task->TaskLocation, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);
				if (NumAllies > 0)
				{
					BotGuardLocation(pBot, Task->TaskLocation);
				}
				else
				{
					BotEvolveLifeform(pBot, ALIEN_LIFEFORM_ONE);
				}
			}
			else
			{
				AvHAIWeapon AttackWeapon = BotAlienChooseBestWeaponForStructure(pBot, Task->TaskTarget);

				float MaxRange = GetMaxIdealWeaponRange(AttackWeapon);
				bool bHullSweep = IsMeleeWeapon(AttackWeapon);

				if (UTIL_PlayerHasLOSToEntity(pBot->Edict, Task->TaskTarget, MaxRange, bHullSweep))
				{
					pBot->DesiredCombatWeapon = AttackWeapon;

					if (GetBotCurrentWeapon(pBot) == AttackWeapon)
					{
						BotShootTarget(pBot, pBot->DesiredCombatWeapon, Task->TaskTarget);
						return;
					}
				}
				else
				{
					MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
				}
			}
			return;
		}
		else
		{
			if (!UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity))
			{
				if (UTIL_PlayerHasLOSToEntity(pBot->Edict, ResNodeIndex->ActiveTowerEntity, max_player_use_reach, true))
				{
					BotUseObject(pBot, ResNodeIndex->ActiveTowerEntity, true);
					if (vDist2DSq(pBot->Edict->v.origin, ResNodeIndex->ActiveTowerEntity->v.origin) > sqrf(60.0f))
					{
						MoveDirectlyTo(pBot, ResNodeIndex->ActiveTowerEntity->v.origin);
					}
					return;
				}

				MoveTo(pBot, ResNodeIndex->ActiveTowerEntity->v.origin, MOVESTYLE_NORMAL);

				if (vDist2DSq(pBot->Edict->v.origin, ResNodeIndex->ActiveTowerEntity->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					BotLookAt(pBot, UTIL_GetCentreOfEntity(ResNodeIndex->ActiveTowerEntity));
				}

				return;
			}
		}

		return;
	}

	if (!IsPlayerGorge(pBot->Edict))
	{
		BotEvolveLifeform(pBot, ALIEN_LIFEFORM_TWO);
		return;
	}

	BotLookAt(pBot, Task->TaskLocation);

	if (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f) { return; }

	float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->Edict->v.v_angle), UTIL_GetVectorNormal2D(Task->TaskLocation - pBot->Edict->v.origin));

	if (LookDot > 0.9f)
	{

		pBot->Impulse = ALIEN_BUILD_RESOURCES;
		Task->LastBuildAttemptTime = gpGlobals->time + 1.0f;
		Task->bIsWaitingForBuildLink = true;
		Task->BuildAttempts++;
	}
}

void BotProgressTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || Task->TaskType == TASK_NONE) { return; }

	switch (Task->TaskType)
	{
	case TASK_MOVE:
		BotProgressMoveTask(pBot, Task);
		break;
	case TASK_USE:
		BotProgressUseTask(pBot, Task);
		break;
	case TASK_TOUCH:
		BotProgressTouchTask(pBot, Task);
		break;
	case TASK_GET_AMMO:
	case TASK_GET_EQUIPMENT:
	case TASK_GET_WEAPON:
		BotProgressPickupTask(pBot, Task);
		break;
	case TASK_GET_HEALTH:
	{
		if (IsPlayerMarine(pBot->Edict))
		{
			BotProgressPickupTask(pBot, Task);
		}
		else
		{
			AlienProgressGetHealthTask(pBot, Task);
		}
	}	
	break;
	case TASK_RESUPPLY:
		BotProgressResupplyTask(pBot, Task);
		break;
	case TASK_BUILD:
	{
		if (IsPlayerMarine(pBot->Edict))
		{
			MarineProgressBuildTask(pBot, Task);
		}
		else
		{
			AlienProgressBuildTask(pBot, Task);
		}
	}
	break;
	case TASK_REINFORCE_STRUCTURE:
		BotProgressReinforceStructureTask(pBot, Task);
		break;
	case TASK_GUARD:
		BotProgressGuardTask(pBot, Task);
		break;
	case TASK_ATTACK:
		BotProgressAttackTask(pBot, Task);
		break;
	case TASK_CAP_RESNODE:
	{
		if (IsPlayerMarine(pBot->Edict))
		{
			MarineProgressCapResNodeTask(pBot, Task);
		}
		else
		{
			AlienProgressCapResNodeTask(pBot, Task);
		}
	}
	break;
	case TASK_WELD:
		BotProgressWeldTask(pBot, Task);
		break;
	case TASK_DEFEND:
		BotProgressDefendTask(pBot, Task);
		break;
	case TASK_COMMAND:
		BotProgressTakeCommandTask(pBot);
		break;
	case TASK_EVOLVE:
		BotProgressEvolveTask(pBot, Task);
		break;
	case TASK_PLACE_MINE:
		BotProgressMineStructureTask(pBot, Task);
		break;
	case TASK_HEAL:
		AlienProgressHealTask(pBot, Task);
		break;
	case TASK_SECURE_HIVE:
	{
		if (IsPlayerMarine(pBot->Edict))
		{
			MarineProgressSecureHiveTask(pBot, Task);
		}
	}
	break;
	default:
		break;

	}
}

void BotProgressWeldTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{

	if (!PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
	{
		if (!FNullEnt(Task->TaskSecondaryTarget))
		{
			MoveTo(pBot, Task->TaskSecondaryTarget->v.origin, MOVESTYLE_NORMAL);
		}
		else
		{
			AvHAIDroppedItem* Welder = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_WELDER, 0.0f, 0.0f, true);

			if (Welder)
			{
				Task->TaskSecondaryTarget = Welder->edict;
			}
		}

		return;
	}

	if (IsPlayerInUseRange(pBot->Edict, Task->TaskTarget))
	{
		BotLookAt(pBot, UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, Task->TaskTarget));
		pBot->DesiredCombatWeapon = WEAPON_MARINE_WELDER;

		if (GetBotCurrentWeapon(pBot) != WEAPON_MARINE_WELDER)
		{
			return;
		}

		pBot->Button |= IN_ATTACK;

		return;
	}
	else
	{
		if (IsEdictPlayer(Task->TaskTarget) || IsEdictStructure(Task->TaskTarget))
		{
			MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
		}
		else
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		}
	}

	return;
}

void MarineProgressSecureHiveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	const AvHAIHiveDefinition* Hive = AITAC_GetHiveFromEdict(Task->TaskTarget);

	if (!Hive) { return; }

	bool bWaitForBuildingPlacement = false;

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;

	AvHAIBuildableStructure* TF = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &StructureFilter);

	if (!TF || !(TF->StructureStatusFlags & STRUCTURE_STATUS_COMPLETED)) { bWaitForBuildingPlacement = true; }

	bool bPhaseGatesAvailable = UTIL_ResearchIsComplete(pBot->Player->GetTeam(), TECH_PHASE_GATE);

	if (bPhaseGatesAvailable && !bWaitForBuildingPlacement)
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;

		AvHAIBuildableStructure* PhaseGate = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &StructureFilter);

		if (!PhaseGate || !(TF->StructureStatusFlags & STRUCTURE_STATUS_COMPLETED)) { bWaitForBuildingPlacement = true; }
	}

	if (!bWaitForBuildingPlacement)
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;

		int NumTurrets = AITAC_GetNumDeployablesNearLocation(TF->Location, &StructureFilter);

		if (NumTurrets < 5) { bWaitForBuildingPlacement = true; }
	}

	if (bWaitForBuildingPlacement)
	{
		if (TF)
		{
			BotGuardLocation(pBot, TF->Location);
		}
		else
		{
			BotGuardLocation(pBot, Task->TaskLocation);
		}
		
		return;
	}

	const AvHAIResourceNode* ResNode = Hive->HiveResNodeRef;

	if (ResNode && ResNode->OwningTeam != pBot->Player->GetTeam())
	{
		if (ResNode->bIsOccupied)
		{
			BotAttackTarget(pBot, ResNode->ActiveTowerEntity);
		}
		else
		{
			BotGuardLocation(pBot, ResNode->Location);
		}

		return;
	}

	BotGuardLocation(pBot, Task->TaskLocation);


	
}

void MarineProgressCapResNodeTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return; }

	float DistFromNode = vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation);

	if (DistFromNode > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) || !UTIL_QuickTrace(pBot->Edict, pBot->CurrentEyePosition, (Task->TaskLocation + Vector(0.0f, 0.0f, 50.0f))))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);

		if (gpGlobals->time - pBot->LastCombatTime > 5.0f)
		{
			BotReloadWeapons(pBot);
		}

		return;
	}

	const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return; }

	if (ResNodeIndex->bIsOccupied)
	{
		Task->TaskTarget = ResNodeIndex->ActiveTowerEntity;
		// Cancel the waiting timeout since a tower has been placed for us
		Task->TaskLength = 0.0f;

		if (ResNodeIndex->OwningTeam == pBot->Player->GetTeam())
		{
			if (!UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity))
			{
				// Now we're committed, don't get distracted
				Task->bTaskIsUrgent = true;
				if (UTIL_PlayerHasLOSToEntity(pBot->Edict, ResNodeIndex->ActiveTowerEntity, max_player_use_reach, true))
				{
					BotUseObject(pBot, ResNodeIndex->ActiveTowerEntity, true);
					if (vDist2DSq(pBot->Edict->v.origin, ResNodeIndex->ActiveTowerEntity->v.origin) > sqrf(50.0f))
					{
						MoveDirectlyTo(pBot, ResNodeIndex->ActiveTowerEntity->v.origin);
					}
					return;
				}

				MoveTo(pBot, ResNodeIndex->ActiveTowerEntity->v.origin, MOVESTYLE_NORMAL);

				if (vDist2DSq(pBot->Edict->v.origin, ResNodeIndex->ActiveTowerEntity->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					BotLookAt(pBot, UTIL_GetCentreOfEntity(ResNodeIndex->ActiveTowerEntity));
				}

				return;
			}
		}
		else
		{
			AvHAIWeapon AttackWeapon = BotMarineChooseBestWeaponForStructure(pBot, Task->TaskTarget);

			float MaxRange = GetMaxIdealWeaponRange(AttackWeapon);
			bool bHullSweep = IsMeleeWeapon(AttackWeapon);

			if (UTIL_PlayerHasLOSToEntity(pBot->Edict, Task->TaskTarget, MaxRange, bHullSweep))
			{
				pBot->DesiredCombatWeapon = AttackWeapon;

				if (GetBotCurrentWeapon(pBot) == AttackWeapon)
				{
					//BotShootTarget(pBot, pBot->DesiredCombatWeapon, Task->TaskTarget);
				}
			}
			else
			{
				MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
			}
		}
	}
	else
	{
		// Give the commander 30 seconds to drop a tower for us, or give up and move on
		if (Task->TaskLength == 0.0f)
		{
			Task->TaskStartedTime = gpGlobals->time;
			Task->TaskLength = 30.0f;
		}
		BotGuardLocation(pBot, Task->TaskLocation);
	}


}

void BotGuardLocation(AvHAIPlayer* pBot, const Vector GuardLocation)
{
	float DistFromGuardLocation = vDist2DSq(pBot->Edict->v.origin, GuardLocation);

	if (DistFromGuardLocation > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		pBot->GuardInfo.GuardLocation = g_vecZero;
		MoveTo(pBot, GuardLocation, MOVESTYLE_NORMAL);
		return;
	}

	if (!pBot->GuardInfo.GuardLocation)
	{
		AITASK_GenerateGuardWatchPoints(pBot, GuardLocation);
		pBot->GuardInfo.GuardLocation = GuardLocation;
	}

	if (gpGlobals->time - pBot->GuardInfo.GuardStartLookTime > pBot->GuardInfo.ThisGuardLookTime)
	{
		if (pBot->GuardInfo.NumGuardPoints > 0)
		{
			int NewGuardLookIndex = irandrange(0, (pBot->GuardInfo.NumGuardPoints - 1));

			pBot->GuardInfo.GuardLookLocation = pBot->GuardInfo.GuardPoints[NewGuardLookIndex];
		}
		else
		{
			pBot->GuardInfo.GuardLookLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[SKULK_BASE_NAV_PROFILE], pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

			pBot->GuardInfo.GuardLookLocation.z = pBot->CurrentEyePosition.z;
		}

		Vector LookDir = UTIL_GetVectorNormal2D(pBot->GuardInfo.GuardLookLocation - GuardLocation);

		Vector NewMoveCentre = GuardLocation - (LookDir * UTIL_MetresToGoldSrcUnits(2.0f));

		Vector NewMoveLoc = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], NewMoveCentre, UTIL_MetresToGoldSrcUnits(2.0f));

		if (NewMoveLoc != g_vecZero)
		{
			pBot->GuardInfo.GuardStandPosition = NewMoveLoc;
		}
		else
		{
			pBot->GuardInfo.GuardStandPosition = GuardLocation;
		}

		pBot->GuardInfo.ThisGuardLookTime = frandrange(2.0f, 5.0f);
		pBot->GuardInfo.GuardStartLookTime = gpGlobals->time;
	}

	if (IsPlayerLerk(pBot->Edict))
	{
		MoveTo(pBot, pBot->GuardInfo.GuardStandPosition, MOVESTYLE_HIDE);
	}
	else
	{
		MoveTo(pBot, pBot->GuardInfo.GuardStandPosition, MOVESTYLE_NORMAL);
	}

	

	BotLookAt(pBot, pBot->GuardInfo.GuardLookLocation);


}

void UTIL_ClearGuardInfo(AvHAIPlayer* pBot)
{
	memset(&pBot->GuardInfo, 0, sizeof(AvHAIGuardInfo));
}

void AITASK_GenerateGuardWatchPoints(AvHAIPlayer* pBot, const Vector& GuardLocation)
{
	const edict_t* pEdict = pBot->Edict;

	UTIL_ClearGuardInfo(pBot);

	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());

	bool bEnemyIsAlien = GetGameRules()->GetTeam(EnemyTeam)->GetTeamType() == AVH_CLASS_TYPE_ALIEN;

	const nav_profile NavProfile = (bEnemyIsAlien) ? BaseNavProfiles[SKULK_BASE_NAV_PROFILE] : BaseNavProfiles[MARINE_BASE_NAV_PROFILE];

	bot_path_node path[MAX_AI_PATH_SIZE];
	int pathSize = 0;

	
	for (int i = 0; i < AITAC_GetNumHives(); i++)
	{
		const AvHAIHiveDefinition* Hive = AITAC_GetHiveAtIndex(i);

		if (!Hive) { continue; }

		if (UTIL_QuickTrace(pEdict, GuardLocation + Vector(0.0f, 0.0f, 10.0f), Hive->Location) || vDist2DSq(GuardLocation, Hive->Location) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { continue; }

		dtStatus SearchResult = FindPathClosestToPoint(NavProfile, Hive->FloorLocation, GuardLocation, path, &pathSize, 500.0f);

		if (dtStatusSucceed(SearchResult))
		{
			Vector FinalApproachDir = UTIL_GetVectorNormal2D(path[pathSize - 1].Location - path[pathSize - 2].Location);
			Vector ProspectiveNewGuardLoc = GuardLocation - (FinalApproachDir * 300.0f);

			ProspectiveNewGuardLoc.z = path[pathSize - 2].Location.z;

			pBot->GuardInfo.GuardPoints[pBot->GuardInfo.NumGuardPoints++] = ProspectiveNewGuardLoc;
		}
	}
	
	dtStatus SearchResult = FindPathClosestToPoint(NavProfile, AITAC_GetTeamStartingLocation(EnemyTeam), GuardLocation, path, &pathSize, 500.0f);

	if (dtStatusSucceed(SearchResult))
	{
		Vector FinalApproachDir = UTIL_GetVectorNormal2D(path[pathSize - 1].Location - path[pathSize - 2].Location);
		Vector ProspectiveNewGuardLoc = GuardLocation - (FinalApproachDir * 300.0f);

		ProspectiveNewGuardLoc.z = path[pathSize - 2].Location.z;

		pBot->GuardInfo.GuardPoints[pBot->GuardInfo.NumGuardPoints++] = ProspectiveNewGuardLoc;
	}

	if (vDist2DSq(GuardLocation, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam())) > sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
	{
		dtStatus SearchResult = FindPathClosestToPoint(NavProfile, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam()), GuardLocation, path, &pathSize, 500.0f);

		if (dtStatusSucceed(SearchResult))
		{
			Vector FinalApproachDir = UTIL_GetVectorNormal2D(path[pathSize - 1].Location - path[pathSize - 2].Location);
			Vector ProspectiveNewGuardLoc = GuardLocation - (FinalApproachDir * 300.0f);

			ProspectiveNewGuardLoc.z = path[pathSize - 2].Location.z;

			pBot->GuardInfo.GuardPoints[pBot->GuardInfo.NumGuardPoints++] = ProspectiveNewGuardLoc;
		}
	}

}

bool BotWithBuildTaskExists(AvHTeamNumber Team, AvHAIDeployableStructureType StructureType)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		AvHAIPlayer* Bot = AIMGR_GetAIPlayerAtIndex(i);

		if (!Bot || FNullEnt(Bot->Edict) || Bot->Player->GetTeam() != Team || !IsPlayerActiveInGame(Bot->Edict)) { continue; }

		if ((Bot->PrimaryBotTask.TaskType == TASK_BUILD && Bot->PrimaryBotTask.StructureType == StructureType) || (Bot->SecondaryBotTask.TaskType == TASK_BUILD && Bot->SecondaryBotTask.StructureType == StructureType))
		{
			return true;
		}
	}

	return false;
}

AvHAIPlayer* GetFirstBotWithBuildTask(AvHTeamNumber Team, AvHAIDeployableStructureType StructureType, edict_t* IgnorePlayer)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		AvHAIPlayer* Bot = AIMGR_GetAIPlayerAtIndex(i);

		if (!Bot || FNullEnt(Bot->Edict) || Bot->Player->GetTeam() != Team || !IsPlayerActiveInGame(Bot->Edict)) { continue; }

		bool bPrimaryIsBuildTask = (Bot->PrimaryBotTask.TaskType == TASK_BUILD || Bot->PrimaryBotTask.TaskType == TASK_REINFORCE_STRUCTURE);
		bool bSecondaryIsBuildTask = (Bot->SecondaryBotTask.TaskType == TASK_BUILD || Bot->SecondaryBotTask.TaskType == TASK_REINFORCE_STRUCTURE);

		if ((bPrimaryIsBuildTask && Bot->PrimaryBotTask.StructureType == StructureType) || (bSecondaryIsBuildTask && Bot->SecondaryBotTask.StructureType == StructureType))
		{
			return Bot;
		}
	}

	return nullptr;
}

AvHAIPlayer* GetFirstBotWithReinforceTask(AvHTeamNumber Team, edict_t* ReinforceStructure, edict_t* IgnorePlayer)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		AvHAIPlayer* Bot = AIMGR_GetAIPlayerAtIndex(i);

		if (!Bot || FNullEnt(Bot->Edict) || Bot->Player->GetTeam() != Team || !IsPlayerActiveInGame(Bot->Edict)) { continue; }

		if ((Bot->PrimaryBotTask.TaskType == TASK_REINFORCE_STRUCTURE && Bot->PrimaryBotTask.TaskTarget == ReinforceStructure) || (Bot->SecondaryBotTask.TaskType == TASK_REINFORCE_STRUCTURE && Bot->SecondaryBotTask.TaskTarget == ReinforceStructure))
		{
			return Bot;
		}
	}

	return nullptr;
}

char* AITASK_TaskTypeToChar(const BotTaskType TaskType)
{
	switch (TaskType)
	{
	case TASK_NONE:
		return "None";
	case TASK_BUILD:
		return "Build";
	case TASK_GET_AMMO:
		return "Get Ammo";
	case TASK_ATTACK:
		return "Attack";
	case TASK_GET_EQUIPMENT:
		return "Get Equipment";
	case TASK_GET_HEALTH:
		return "Get Health";
	case TASK_GET_WEAPON:
		return "Get Weapon";
	case TASK_GUARD:
		return "Guard";
	case TASK_HEAL:
		return "Heal";
	case TASK_MOVE:
		return "Move";
	case TASK_RESUPPLY:
		return "Resupply";
	case TASK_CAP_RESNODE:
		return "Cap Resource Node";
	case TASK_WELD:
		return "Weld";
	case TASK_DEFEND:
		return "Defend";
	case TASK_EVOLVE:
		return "Evolve";
	case TASK_REINFORCE_STRUCTURE:
		return "Reinforce Structure";
	case TASK_SECURE_HIVE:
		return "Secure Hive";
	case TASK_PLACE_MINE:
		return "Place Mine";
	default:
		return "INVALID";
	}
}

void AITASK_SetPickupTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent)
{
	if (FNullEnt(Target) || (Target->v.effects & EF_NODRAW))
	{
		AITASK_ClearBotTask(pBot, Task);
		return;
	}

	if (Task->TaskTarget == Target)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AvHAIDroppedItem* ItemToPickup = AITAC_GetDroppedItemRefFromEdict(Target);

	if (!ItemToPickup || FNullEnt(ItemToPickup->edict) || !ItemToPickup->bIsReachableMarine)
	{
		AITASK_ClearBotTask(pBot, Task);
		return;
	}

	Vector PickupLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, Target->v.origin, max_player_use_reach);

	if (vIsZero(PickupLocation))
	{
		AITASK_ClearBotTask(pBot, Task);
		return;
	}

	switch (ItemToPickup->ItemType)
	{
		case DEPLOYABLE_ITEM_AMMO:
			Task->TaskType = TASK_GET_AMMO;
			break;
		case DEPLOYABLE_ITEM_HEALTHPACK:
			Task->TaskType = TASK_GET_HEALTH;
			break;
		case DEPLOYABLE_ITEM_JETPACK:
		case DEPLOYABLE_ITEM_HEAVYARMOUR:
			Task->TaskType = TASK_GET_EQUIPMENT;
			break;
		default:
			Task->TaskType = TASK_GET_WEAPON;
			break;
	}

	Task->TaskTarget = Target;
	Task->TaskLocation = PickupLocation;
	Task->bTaskIsUrgent = bIsUrgent;

}

void AITASK_SetWeldTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO))
	{
		AITASK_ClearBotTask(pBot, Task);
		return;
	}

	if (Task->TaskType == TASK_WELD && Task->TaskTarget == Target)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	if (!PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
	{
		AvHAIDroppedItem* NearestWelder = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_WELDER, 0.0f, 0.0f, true);

		if (!NearestWelder)
		{
			AITASK_ClearBotTask(pBot, Task);
			return;
		}
		else
		{
			Task->TaskSecondaryTarget = NearestWelder->edict;
		}
	}

	Task->TaskTarget = Target;
	Task->TaskType = TASK_WELD;
	Task->bTaskIsUrgent = bIsUrgent;
	Task->TaskLocation = ZERO_VECTOR;
	Task->TaskLength = 0.0f;
	
	if (IsEdictPlayer(Target) || IsEdictStructure(Target)) { return; }

	Vector TargetLocation = UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, Task->TaskTarget);

	if (vIsZero(TargetLocation))
	{
		TargetLocation = Task->TaskTarget->v.origin;
	}

	Vector TaskLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, TargetLocation, UTIL_MetresToGoldSrcUnits(5.0f));

	if (vIsZero(TaskLocation))
	{
		AITASK_ClearBotTask(pBot, Task);
		return;
	}

	Task->TaskLocation = TaskLocation;
}

void AITASK_SetAttackTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent)
{
	// Don't set the task if the target is invalid, dead or on the same team as the bot (can't picture a situation where you want them to teamkill...)
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO) || Target->v.team == pBot->Edict->v.team)
	{
		AITASK_ClearBotTask(pBot, Task);
		return;
	}

	if (Task->TaskType == TASK_ATTACK && Task->TaskTarget == Target) 
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	Task->TaskType = TASK_ATTACK;
	Task->TaskTarget = Target;
	Task->bTaskIsUrgent = bIsUrgent;

	// We don't need an attack location for players since this will be moving around anyway
	if (IsEdictPlayer(Target))
	{
		Task->bTargetIsPlayer = true;
		return;
	}

	// Get as close as possible to the target
	Vector AttackLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_GetEntityGroundLocation(Target), UTIL_MetresToGoldSrcUnits(20.0f));

	if (AttackLocation != g_vecZero)
	{
		Task->TaskLocation = AttackLocation;
	}
	else
	{
		AttackLocation = Target->v.origin;
	}
}

void AITASK_SetMoveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const Vector Location, bool bIsUrgent)
{
	if (Task->TaskType == TASK_MOVE && vDist2DSq(Task->TaskLocation, Location) < sqrf(16.0f))
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	if (vIsZero(Location)) { return; }

	// Get as close as possible to desired location
	Vector MoveLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, Location, UTIL_MetresToGoldSrcUnits(20.0f));

	if (!vIsZero(MoveLocation))
	{
		Task->TaskType = TASK_MOVE;
		Task->TaskLocation = MoveLocation;
		Task->bTaskIsUrgent = bIsUrgent;
		Task->TaskLength = 60.0f; // Set a maximum time to reach destination. Helps avoid bots getting permanently stuck
	}
}

void AITASK_SetBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const AvHAIDeployableStructureType StructureType, const Vector Location, const bool bIsUrgent)
{
	if (Task->TaskType == TASK_BUILD && Task->StructureType == StructureType && vDist2DSq(Task->TaskLocation, Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f))) { return; }
	
	AITASK_ClearBotTask(pBot, Task);


	if (vIsZero(Location)) { return; }

	// Get as close as possible to desired location
	Vector BuildLocation = FindClosestNavigablePointToDestination(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(pBot->Player->GetTeam()), Location, UTIL_MetresToGoldSrcUnits(10.0f));
	BuildLocation = UTIL_ProjectPointToNavmesh(BuildLocation, BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE]);

	if (vIsZero(BuildLocation))
	{
		BuildLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, Location, UTIL_MetresToGoldSrcUnits(10.0f));
	}

	if (BuildLocation != g_vecZero)
	{

		Task->TaskType = TASK_BUILD;
		Task->TaskLocation = BuildLocation;
		Task->StructureType = StructureType;
		Task->bTaskIsUrgent = bIsUrgent;

		if (StructureType == STRUCTURE_ALIEN_HIVE)
		{
			char buf[512];

			string MapLocationName;

			if (GetNearestMapLocationAtPoint(Task->TaskLocation, MapLocationName))
			{
				sprintf(buf, "I'll drop hive at %s", MapLocationName.c_str());
			}
			else
			{
				sprintf(buf, "I'll drop the hive");
			}			

			BotSay(pBot, true, 1.0f, buf);
		}
	}
}

void AITASK_SetBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* StructureToBuild, const bool bIsUrgent)
{
	AITASK_ClearBotTask(pBot, Task);

	if (FNullEnt(StructureToBuild) || UTIL_StructureIsFullyBuilt(StructureToBuild)) { return; }

	if (Task->TaskType == TASK_BUILD && Task->TaskTarget == StructureToBuild) { return; }

	// Get as close as possible to desired location
	Vector BuildLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_GetEntityGroundLocation(StructureToBuild), 80.0f);

	if (BuildLocation != g_vecZero)
	{
		Task->TaskType = TASK_BUILD;
		Task->TaskTarget = StructureToBuild;
		Task->TaskLocation = BuildLocation;
		Task->bTaskIsUrgent = bIsUrgent;
		Task->StructureType = GetStructureTypeFromEdict(StructureToBuild);
	}
}

void AITASK_SetCapResNodeTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const AvHAIResourceNode* NodeRef, const bool bIsUrgent)
{
	if (!NodeRef) { return; }

	AvHAIDeployableStructureType NodeStructureType = (IsPlayerMarine(pBot->Edict)) ? STRUCTURE_MARINE_RESTOWER : STRUCTURE_ALIEN_RESTOWER;

	if (Task->TaskType == TASK_CAP_RESNODE && Task->TaskLocation == NodeRef->Location)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		Task->TaskTarget = NodeRef->ActiveTowerEntity;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	Task->TaskType = TASK_CAP_RESNODE;
	Task->StructureType = NodeStructureType;
	Task->TaskLocation = NodeRef->Location;

	if (!FNullEnt(NodeRef->ActiveTowerEntity))
	{
		Task->TaskTarget = NodeRef->ActiveTowerEntity;
	}

	Task->bTaskIsUrgent = bIsUrgent;
}

void AITASK_SetDefendTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent)
{
	if (Task->TaskType == TASK_DEFEND && Task->TaskTarget == Target)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	// Can't defend an invalid or dead target
	if (FNullEnt(Target) || Target->v.deadflag != DEAD_NO) { return; }

	Task->TaskType = TASK_DEFEND;
	Task->TaskTarget = Target;
	Task->bTaskIsUrgent = bIsUrgent;

	Vector DefendPoint = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_GetEntityGroundLocation(Target), UTIL_MetresToGoldSrcUnits(10.0f));

	if (DefendPoint != g_vecZero)
	{
		Task->TaskLocation = DefendPoint;
	}
	else
	{
		Task->TaskLocation = UTIL_GetEntityGroundLocation(Target);
	}

	
}

void AITASK_SetEvolveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* EvolveHive, const AvHMessageID EvolveImpulse, const bool bIsUrgent)
{

	if (EvolveImpulse <= 0) 
	{
		AITASK_ClearBotTask(pBot, Task);
		return; 
	}

	if (Task->TaskType == TASK_EVOLVE && Task->TaskTarget == EvolveHive)
	{
		Task->Evolution = EvolveImpulse;
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	Task->TaskType = TASK_EVOLVE;
	Task->TaskTarget = EvolveHive;
	Task->TaskLocation = g_vecZero;
	Task->Evolution = EvolveImpulse;
	Task->bTaskIsUrgent = bIsUrgent;
}

void AITASK_SetEvolveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const Vector EvolveLocation, const AvHMessageID EvolveImpulse, const bool bIsUrgent)
{
	if (Task->TaskType == TASK_EVOLVE && Task->Evolution == EvolveImpulse)
	{
		Task->TaskLocation = EvolveLocation;
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	if (EvolveImpulse <= 0) { return; }

	Task->TaskType = TASK_EVOLVE;
	Task->TaskLocation = EvolveLocation;
	Task->Evolution = EvolveImpulse;
	Task->bTaskIsUrgent = bIsUrgent;
}

void AITASK_SetUseTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent)
{
	if (Task->TaskType == TASK_USE && Task->TaskTarget == Target)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	Task->TaskType = TASK_USE;
	Task->TaskTarget = Target;
	Task->TaskLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_ProjectPointToNavmesh(UTIL_GetCentreOfEntity(Target)), UTIL_MetresToGoldSrcUnits(10.0f));
	Task->bTaskIsUrgent = bIsUrgent;
	Task->TaskLength = 10.0f;
	Task->TaskStartedTime = gpGlobals->time;
}

void AITASK_SetUseTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const Vector UseLocation, const bool bIsUrgent)
{
	if (Task->TaskType == TASK_USE && Task->TaskTarget == Target)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	Task->TaskType = TASK_USE;
	Task->TaskTarget = Target;
	Task->TaskLocation = UseLocation;
	Task->bTaskIsUrgent = bIsUrgent;
	Task->TaskLength = 10.0f;
	Task->TaskStartedTime = gpGlobals->time;
}

void AITASK_SetTouchTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, bool bIsUrgent)
{
	if (Task->TaskType == TASK_TOUCH && Task->TaskTarget == Target)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	Task->TaskType = TASK_TOUCH;
	Task->TaskTarget = Target;
	Task->TaskLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_ProjectPointToNavmesh(UTIL_GetCentreOfEntity(Target)), UTIL_MetresToGoldSrcUnits(10.0f));
	Task->bTaskIsUrgent = bIsUrgent;
}

void AITASK_SetReinforceStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, bool bIsUrgent)
{
	if (Task->TaskType == TASK_REINFORCE_STRUCTURE && Target == Task->TaskTarget)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	Task->TaskType = TASK_REINFORCE_STRUCTURE;
	Task->TaskTarget = Target;
	Task->bTaskIsUrgent = bIsUrgent;
}

void AITASK_SetReinforceStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const AvHAIDeployableStructureType FirstStructureType, bool bIsUrgent)
{
	if (Task->TaskType == TASK_REINFORCE_STRUCTURE && Target == Task->TaskTarget)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		Task->StructureType = FirstStructureType;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	Task->TaskType = TASK_REINFORCE_STRUCTURE;
	Task->TaskTarget = Target;
	Task->bTaskIsUrgent = bIsUrgent;
	Task->StructureType = FirstStructureType;
}

void AITASK_SetSecureHiveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const Vector WaitLocation, bool bIsUrgent)
{
	if (Task->TaskType == TASK_SECURE_HIVE && Target == Task->TaskTarget)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		Task->TaskLocation = WaitLocation;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	Task->TaskType = TASK_SECURE_HIVE;
	Task->TaskTarget = Target;
	Task->bTaskIsUrgent = bIsUrgent;
	Task->TaskLocation = WaitLocation;
}

void AITASK_SetMineStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, bool bIsUrgent)
{
	if (Task->TaskType == TASK_PLACE_MINE && Target == Task->TaskTarget)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	Task->TaskType = TASK_PLACE_MINE;
	Task->TaskTarget = Target;
	Task->bTaskIsUrgent = bIsUrgent;
	Task->TaskLocation = UTIL_GetNextMinePosition(Target);
	Task->StructureType = STRUCTURE_MARINE_DEPLOYEDMINE;


}