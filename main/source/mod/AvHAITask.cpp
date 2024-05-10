
#include "AvHAITask.h"
#include "AvHAINavigation.h"
#include "AvHAITactical.h"
#include "AvHAIMath.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIWeaponHelper.h"
#include "AvHAIHelper.h"
#include "AvHAIPlayerManager.h"
#include "AvHAIConfig.h"

#include "AvHSharedUtil.h"
#include "AvHAlienWeaponConstants.h"
#include "AvHMarineEquipmentConstants.h"
#include "AvHGamerules.h"
#include "AvHWeldable.h"
#include "AvHTurret.h"

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
		AvHTeamNumber BotTeam = pBot->Player->GetTeam();
		AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

		if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
		{
			const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(pBot->Edict->v.origin);

			if (NearestHive && vDist2DSq(NearestHive->FloorLocation, pBot->Edict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
			{
				if (NearestHive->Status == HIVE_STATUS_UNBUILT)
				{
					AITASK_SetSecureHiveTask(pBot, Task, NearestHive->HiveEdict, NearestHive->FloorLocation, false);
					Task->bIssuedByCommander = true;
					return;
				}
				else
				{
					if (UTIL_PlayerHasLOSToEntity(pBot->Edict, NearestHive->HiveEdict, UTIL_MetresToGoldSrcUnits(15.0f), false))
					{
						AITASK_SetAttackTask(pBot, Task, NearestHive->HiveEdict, false);
						Task->bIssuedByCommander = true;
						return;
					}
				}
			}
		}
		else
		{
			if (vDist2DSq(pBot->Edict->v.origin, AITAC_GetTeamStartingLocation(EnemyTeam)) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				DeployableSearchFilter EnemyStuffFilter;
				EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
				EnemyStuffFilter.DeployableTeam = EnemyTeam;
				EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
				EnemyStuffFilter.ReachabilityTeam = (AvHTeamNumber)pBot->Edict->v.team;
				EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

				AvHAIBuildableStructure NearbyEnemyStructure = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(EnemyTeam), &EnemyStuffFilter);

				if (NearbyEnemyStructure.IsValid())
				{
					AITASK_SetAttackTask(pBot, Task, NearbyEnemyStructure.edict, false);
					Task->bIssuedByCommander = true;
					return;
				}

			}
		}

		DeployableSearchFilter EnemyResTowerFilter;
		EnemyResTowerFilter.DeployableTypes = SEARCH_ANY_RES_TOWER;
		EnemyResTowerFilter.DeployableTeam = (AIMGR_GetEnemyTeam(pBot->Player->GetTeam()));
		EnemyResTowerFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		EnemyResTowerFilter.ReachabilityTeam = (AvHTeamNumber)pBot->Edict->v.team;
		EnemyResTowerFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

		AvHAIBuildableStructure NearbyAlienTower = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &EnemyResTowerFilter);

		if (NearbyAlienTower.IsValid())
		{
			const AvHAIResourceNode* NodeRef = AITAC_GetNearestResourceNodeToLocation(NearbyAlienTower.Location);
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
	Task->LastBuildAttemptTime = 0.0f;
	Task->BuildAttempts = 0;
	Task->StructureType = STRUCTURE_NONE;

	memset(&Task->ActiveBuildInfo, 0, sizeof(AvHAIBuildAttempt));
}

bool AITASK_IsTaskUrgent(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (Task->TaskType == TASK_NONE) { return false; }

	if (Task->bTaskIsUrgent) { return true; }

	switch (Task->TaskType)
	{
	case TASK_GET_AMMO:
		return (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) == 0);
	case TASK_GET_HEALTH:
		return Task->bTaskIsUrgent || ((IsPlayerMarine(pBot->Edict)) ? (pBot->Edict->v.health < 50.0f) : (GetPlayerOverallHealthPercent(pBot->Edict) < 0.5f));
	case TASK_ATTACK:
	case TASK_GET_WEAPON:
	case TASK_GET_EQUIPMENT:
	case TASK_WELD:
		return false;
	case TASK_RESUPPLY:
		return (pBot->Edict->v.health < 50.0f) || (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) == 0);
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
	case TASK_SECURE_HIVE:
	{
		if (IsPlayerMarine(pBot->Edict))
		{
			return AITASK_IsMarineSecureHiveTaskStillValid(pBot, Task);
		}
		else
		{
			return AITASK_IsAlienSecureHiveTaskStillValid(pBot, Task);
		}
	}
	case TASK_ATTACK_BASE:
		return true;
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
	if (vIsZero(Task->TaskLocation)) { return false; }

	if (pBot->BotNavInfo.NavProfile.bFlyingProfile && vEquals(pBot->Edict->v.origin, Task->TaskLocation, 50.0f)) { return false; }

	return (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(GetPlayerRadius(pBot->Player)) || fabsf(pBot->Edict->v.origin.z - Task->TaskLocation.z) > 50.0f);
}

bool AITASK_IsWeldTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return false; }
	if (!Task->TaskTarget || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }
	if (Task->TaskTarget == pBot->Edict) { return false; }
	if (!PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
	{
		if (FNullEnt(Task->TaskSecondaryTarget))
		{
			AvHAIDroppedItem* NearestWelder = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_WELDER, pBot->Player->GetTeam(), pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, 0.0f, true);

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

	return (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f))) && (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player));
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
	if (!Task->TaskTarget || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if ((vIsZero(Task->TaskTarget->v.origin) && vIsZero(Task->TaskLocation))) { return false; }

	if ((Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (IsEdictStructure(Task->TaskTarget) && !UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (IsPlayerSkulk(pBot->Edict))
	{
		if (UTIL_IsStructureElectrified(Task->TaskTarget)) { return false; }
	}

	if (IsPlayerGorge(pBot->Edict) && (Task->TaskTarget->v.health > 100 && !PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB))) { return false; }

	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(Task->TaskTarget);

	if (IsPlayerMarine(pBot->Edict))
	{
		if (StructureType == STRUCTURE_ALIEN_HIVE || StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
		{
			if (UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) <= 0 && UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) <= 0)
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

	if (IsAreaAffectedBySpores(Task->TaskTarget->v.origin) && !PlayerHasHeavyArmour(pBot->Edict)) { return false; }

	return (
			(pBot->Edict->v.health < pBot->Edict->v.max_health)
		||	(UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player))
		||	(UTIL_GetPlayerSecondaryAmmoReserve(pBot->Player) < UTIL_GetPlayerSecondaryMaxAmmoReserve(pBot->Player))
		);
}

bool AITASK_IsGuardTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return false; }

	if (vIsZero(Task->TaskLocation))
	{
		return false;
	}

	if (Task->TaskLength > 0.0f && Task->TaskStartedTime > 0.0f)
	{
		return (gpGlobals->time - Task->TaskStartedTime < Task->TaskLength);
	}

	return true;
}

bool AITASK_IsMineStructureTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task->TaskTarget || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (UTIL_StructureIsRecycling(Task->TaskTarget)) { return false; }

	if (!PlayerHasWeapon(pBot->Player, WEAPON_MARINE_MINES)) { return false; }

	DeployableSearchFilter MineFilter;
	MineFilter.DeployableTypes = STRUCTURE_MARINE_DEPLOYEDMINE;
	MineFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(2.0f);
	MineFilter.bConsiderPhaseDistance = false;
	MineFilter.DeployableTeam = pBot->Player->GetTeam();

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

	// Always go build if commanded to, regardless of how many are already working on it
	if (!Task->bIssuedByCommander)
	{
		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea((AvHTeamNumber)pBot->Edict->v.team, Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), false, pBot->Edict, AVH_USER3_NONE);
		
		// Only one marine should build stuff if it's near the marine base. If not, then two for safety
		int NumDesiredBuilders = (vDist2DSq(Task->TaskTarget->v.origin, AITAC_GetCommChairLocation(pBot->Player->GetTeam())) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f))) ? 1 : 2;

		if (NumBuilders >= NumDesiredBuilders)
		{
			return false;
		}
	}

	return !UTIL_StructureIsFullyBuilt(Task->TaskTarget);
}

bool AITASK_IsAlienBuildTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return false; }

	if (vIsZero(Task->TaskLocation)) { return false; }

	if (Task->StructureType == STRUCTURE_NONE) { return false; }

	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING) { return true; }

	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_FAILED && Task->ActiveBuildInfo.NumAttempts >= 3) { return false; }

	if (Task->ActiveBuildInfo.LinkedStructure && (UTIL_StructureIsFullyBuilt(Task->ActiveBuildInfo.LinkedStructure->edict) || !UTIL_IsBuildableStructureStillReachable(pBot, Task->ActiveBuildInfo.LinkedStructure->edict))) { return false; }

	if (Task->StructureType == STRUCTURE_ALIEN_HIVE)
	{
		const AvHAIHiveDefinition* HiveIndex = AITAC_GetHiveNearestLocation(Task->TaskLocation);

		if (!HiveIndex) { return false; }

		if (HiveIndex->Status != HIVE_STATUS_UNBUILT) { return false; }
	}

	if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER)
	{
		const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

		if (!ResNodeIndex) { return false; }

		if (ResNodeIndex->bIsOccupied)
		{
			if (ResNodeIndex->OwningTeam != pBot->Player->GetTeam()) { return false; } // Node has been capped by the enemy, no longer relevant

			if (UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity)) { return false; } // Don't bother if it's already completed
		}

		return true;
	}

	return UTIL_GetNavAreaAtLocation(Task->TaskLocation) == SAMPLE_POLYAREA_GROUND;
}

bool AITASK_IsAlienCapResNodeTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (vIsZero(Task->TaskLocation))
	{
		return false;
	}

	const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex)
	{
		return false;
	}

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	if (ResNodeIndex->bIsBaseNode)
	{
		if (FNullEnt(ResNodeIndex->ParentHive)) { return false; } // This is the marine base res node, leave it alone

		AvHAIHiveDefinition* ParentHive = AITAC_GetHiveFromEdict(ResNodeIndex->ParentHive);

		// An enemy is now building the hive that this res node belongs to, abort
		if (ParentHive && ParentHive->OwningTeam == EnemyTeam) { return false; }
	}
	
	// Don't waste resources switching down to gorge if we're a lerk, fade or onos
	// but we can still clear the area of enemy structures
	if (!IsPlayerSkulk(pBot->Edict) && !IsPlayerGorge(pBot->Edict))
	{
		DeployableSearchFilter EnemyStructuresFilter;
		EnemyStructuresFilter.DeployableTeam = EnemyTeam;
		EnemyStructuresFilter.ReachabilityTeam = BotTeam;
		EnemyStructuresFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		EnemyStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

		return AITAC_DeployableExistsAtLocation(ResNodeIndex->Location, &EnemyStructuresFilter); 
	}

	if (!FNullEnt(ResNodeIndex->ParentHive))
	{
		DeployableSearchFilter EnemyStructuresFilter;
		EnemyStructuresFilter.DeployableTeam = EnemyTeam;
		EnemyStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		EnemyStructuresFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		EnemyStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE)
		{
			EnemyStructuresFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
		}
		else
		{
			EnemyStructuresFilter.DeployableTypes = (STRUCTURE_ALIEN_OFFENCECHAMBER);
		}
		
		// Enemy has started fortifying the hive we want to build a RT in, abort
		if (AITAC_DeployableExistsAtLocation(ResNodeIndex->Location, &EnemyStructuresFilter)) { return false; }
	}

	// We can attack structures basically if we aren't stuck with Gorge's spit attack
	bool bCanAttackStructures = (!IsPlayerGorge(pBot->Edict) || PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB));

	if (ResNodeIndex->bIsOccupied)
	{
		// If we have a tower on the node, we can still help build it if it's not finished yet,
		// or we can clear the area if we're able to
		if (ResNodeIndex->OwningTeam == BotTeam)
		{
			if (IsPlayerGorge(pBot->Edict) && !UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity)) { return true; }

			// If the tower is fully built and we don't have bile bomb then our work here is done
			if (!bCanAttackStructures) { return false; }
			
			// If we can clear the area of enemy junk then do so, otherwise we're finished
			DeployableSearchFilter EnemyStructuresFilter;
			EnemyStructuresFilter.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
			EnemyStructuresFilter.ReachabilityTeam = BotTeam;
			EnemyStructuresFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
			EnemyStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

			return AITAC_DeployableExistsAtLocation(ResNodeIndex->Location, &EnemyStructuresFilter);
		}
		else
		{
			// Enemy owns the res node, but we can cap it if we're able to attack structures, or there's a friend in the area who can.
			return (bCanAttackStructures || AITAC_GetNumPlayersOfTeamInArea(BotTeam, ResNodeIndex->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2) > 0);
		}
	}

	// If another gorge is claiming this spot, then move on
	if (!IsPlayerGorge(pBot->Edict))
	{
		edict_t* OtherBuilder = AITAC_GetNearestPlayerOfClassInArea(pBot->Player->GetTeam(), ResNodeIndex->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

		if (!FNullEnt(OtherBuilder))
		{
			if (GetPlayerResources(OtherBuilder) >= (int)(kResourceTowerCost * 0.7f))
			{
				// If we can clear the area of enemy junk then do so, otherwise we will let the other guy place the tower
				DeployableSearchFilter EnemyStructuresFilter;
				EnemyStructuresFilter.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
				EnemyStructuresFilter.ReachabilityTeam = BotTeam;
				EnemyStructuresFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
				EnemyStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

				return AITAC_DeployableExistsAtLocation(ResNodeIndex->Location, &EnemyStructuresFilter);
			}
		}
	}

	return true;
}

bool AITASK_IsMarineCapResNodeTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || vIsZero(Task->TaskLocation)) { return false; }

	const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return false; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	// Always obey commander orders even if there's a bunch of other marines already there, otherwise don't bother if someone else is securing it
	if (!Task->bIssuedByCommander)
	{
		int DesiredNumCappers = (ResNodeIndex->OwningTeam == AIMGR_GetEnemyTeam(pBot->Player->GetTeam())) ? 2 : 1;
		int NumMarinesNearby = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), Task->TaskLocation, UTIL_MetresToGoldSrcUnits(4.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		if (NumMarinesNearby >= DesiredNumCappers && vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(4.0f))) { return false; }
	}

	// Obviously still valid task if the node is empty
	if (!ResNodeIndex->bIsOccupied) { return true; }

	// Also obviously still valid if it's owned by the enemy
	if (ResNodeIndex->OwningTeam != BotTeam) { return true; }

	// Likewise, still valid if there isn't a tower or it's not fully built
	if (FNullEnt(ResNodeIndex->ActiveTowerEntity) || !UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity)) { return true; }

	// At this point, the node is capped fully. However, don't consider a res node secured if the enemy still has their junk lying around. Clear it all out.
	DeployableSearchFilter EnemyStructures;
	EnemyStructures.DeployableTypes = SEARCH_ALL_STRUCTURES;
	EnemyStructures.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
	EnemyStructures.ReachabilityTeam = BotTeam;
	EnemyStructures.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

	return AITAC_DeployableExistsAtLocation(ResNodeIndex->Location, &EnemyStructures);
}

bool AITASK_IsDefendTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task->TaskTarget || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

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
	if (!Task->TaskTarget || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (!FNullEnt(Task->TaskSecondaryTarget) && !UTIL_StructureIsFullyBuilt(Task->TaskSecondaryTarget)) { return true; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	if (Task->TaskTarget->v.team == EnemyTeam) { return false; }

	// The reinforce structure task is true if we have an undecided hive available that we could build a new chamber with
	bool bActiveHiveWithoutTechExists = AITAC_TeamHiveWithTechExists(pBot->Player->GetTeam(), MESSAGE_NULL);

	if (bActiveHiveWithoutTechExists) { return true; }

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER | ALIEN_BUILD_DEFENSE_CHAMBER | ALIEN_BUILD_MOVEMENT_CHAMBER | ALIEN_BUILD_SENSORY_CHAMBER;
	StructureFilter.MaxSearchRadius = (IsEdictHive(Task->TaskTarget)) ? UTIL_MetresToGoldSrcUnits(10.0f) : UTIL_MetresToGoldSrcUnits(5.0f);
	StructureFilter.DeployableTeam = pBot->Player->GetTeam();

	vector<AvHAIBuildableStructure> AllNearbyStructures = AITAC_FindAllDeployables(Task->TaskTarget->v.origin, &StructureFilter);

	bool bUnfinishedStructureExists = false;
	int NumOffenceChambers = 0;
	int NumDefenceChambers = 0;
	int NumMovementChambers = 0;
	int NumSensoryChambers = 0;

	for (auto it = AllNearbyStructures.begin(); it != AllNearbyStructures.end(); it++)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		if (!(ThisStructure.StructureStatusFlags & STRUCTURE_STATUS_COMPLETED)) { bUnfinishedStructureExists = true; }

		switch (ThisStructure.StructureType)
		{
			case STRUCTURE_ALIEN_OFFENCECHAMBER:
				NumOffenceChambers++;
				break;
			case STRUCTURE_ALIEN_DEFENCECHAMBER:
				NumDefenceChambers++;
				break;
			case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
				NumMovementChambers++;
				break;
			case STRUCTURE_ALIEN_SENSORYCHAMBER:
				NumSensoryChambers++;
				break;
			default:
				break;

		}
	}

	// Task is still valid if we have any missing structures, or we're a gorge at the target site and there is an incomplete structure that we can finish off

	if (NumOffenceChambers < 2
		|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_DEFENSE_CHAMBER) && NumDefenceChambers < 2)
		|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_MOVEMENT_CHAMBER) && NumMovementChambers < 1)
		|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_SENSORY_CHAMBER) && NumSensoryChambers < 1)
		|| (IsPlayerGorge(pBot->Edict) && bUnfinishedStructureExists && vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		) {	return true; }

	// Otherwise, are there any enemy structures lying around we could clear out?

	bool bCanAttackStructures = (!IsPlayerGorge(pBot->Edict) || PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB));

	if (!bCanAttackStructures) { return false; }

	DeployableSearchFilter EnemyStuff;
	EnemyStuff.DeployableTypes = SEARCH_ALL_STRUCTURES;
	EnemyStuff.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
	EnemyStuff.ReachabilityTeam = BotTeam;
	EnemyStuff.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyStuff.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

	return AITAC_DeployableExistsAtLocation(Task->TaskTarget->v.origin, &EnemyStuff);
}

bool AITASK_IsAlienSecureHiveTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || FNullEnt(Task->TaskTarget) || !IsPlayerAlien(pBot->Edict)) { return false; }

	AvHAIHiveDefinition* HiveToSecure = AITAC_GetHiveFromEdict(Task->TaskTarget);

	if (!HiveToSecure) { return false; }

	if (HiveToSecure->Status != HIVE_STATUS_UNBUILT && HiveToSecure->OwningTeam != pBot->Player->GetTeam()) { return true; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	DeployableSearchFilter EnemyStuff;
	EnemyStuff.DeployableTypes = SEARCH_ALL_STRUCTURES;
	EnemyStuff.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
	EnemyStuff.ReachabilityTeam = BotTeam;
	EnemyStuff.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyStuff.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	return AITAC_DeployableExistsAtLocation(HiveToSecure->FloorLocation, &EnemyStuff);
}

bool AITASK_IsMarineSecureHiveTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || FNullEnt(Task->TaskTarget) || IsPlayerAlien(pBot->Edict)) { return false; }

	AvHAIHiveDefinition* HiveToSecure = AITAC_GetHiveFromEdict(Task->TaskTarget);

	if (!HiveToSecure || HiveToSecure->Status != HIVE_STATUS_UNBUILT) { return false; }

	// A marine bot will consider their "secure hive" task completed if the following structures have been fully built:
	// Phase gate (only if tech available)
	// Turret factory (regular or advanced)
	// 5 turrets
	// Resource node has been capped by the bot's team

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	bool bPhaseGatesAvailable = AITAC_PhaseGatesAvailable(BotTeam);

	bool bHasPhaseGate = false;
	bool bHasTurretFactory = false;
	bool bTurretFactoryElectrified = false;
	int NumTurrets = 0;

	DeployableSearchFilter SearchFilter;
	SearchFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
	SearchFilter.DeployableTeam = BotTeam;
	SearchFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	SearchFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	SearchFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	vector<AvHAIBuildableStructure> HiveStructures = AITAC_FindAllDeployables(HiveToSecure->FloorLocation, &SearchFilter);

	for (auto it = HiveStructures.begin(); it != HiveStructures.end(); it++)
	{
		AvHAIBuildableStructure Structure = (*it);

		if (Structure.StructureType == STRUCTURE_MARINE_PHASEGATE)
		{
			bHasPhaseGate = true;
		}

		if (Structure.StructureType == STRUCTURE_MARINE_TURRETFACTORY || Structure.StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
		{
			bHasTurretFactory = true;

			SearchFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;
			SearchFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(8.0f);

			NumTurrets = AITAC_GetNumDeployablesNearLocation(Structure.Location, &SearchFilter);

		}

	}

	const AvHAIResourceNode* ResNode = HiveToSecure->HiveResNodeRef;

	bool bSecuredResNode = (!ResNode || (ResNode->OwningTeam == BotTeam && !FNullEnt(ResNode->ActiveTowerEntity) && UTIL_StructureIsFullyBuilt(ResNode->ActiveTowerEntity)));

	if ((bPhaseGatesAvailable && !bHasPhaseGate) || !bHasTurretFactory || NumTurrets < 5) { return true; }

	// Don't consider a hive secured if the enemy still has their junk in there. Clear it all out.

	DeployableSearchFilter EnemyStructures;
	EnemyStructures.DeployableTypes = SEARCH_ALL_STRUCTURES;
	EnemyStructures.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
	EnemyStructures.ReachabilityTeam = BotTeam;
	EnemyStructures.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	return AITAC_DeployableExistsAtLocation(HiveToSecure->FloorLocation, &EnemyStructures);
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
	case ALIEN_EVOLUTION_ONE:
	case ALIEN_EVOLUTION_TWO:
	case ALIEN_EVOLUTION_THREE:
		return (!PlayerHasAlienUpgradeOfType(pBot->Edict, HIVE_TECH_DEFENCE) && AITAC_IsAlienUpgradeAvailableForTeam(pBot->Player->GetTeam(), HIVE_TECH_DEFENCE));
	case ALIEN_EVOLUTION_SEVEN:
	case ALIEN_EVOLUTION_EIGHT:
	case ALIEN_EVOLUTION_NINE:
		return (!PlayerHasAlienUpgradeOfType(pBot->Edict, HIVE_TECH_MOVEMENT) && AITAC_IsAlienUpgradeAvailableForTeam(pBot->Player->GetTeam(), HIVE_TECH_MOVEMENT));
	case ALIEN_EVOLUTION_TEN:
	case ALIEN_EVOLUTION_ELEVEN:
	case ALIEN_EVOLUTION_TWELVE:
		return (!PlayerHasAlienUpgradeOfType(pBot->Edict, HIVE_TECH_SENSORY) && AITAC_IsAlienUpgradeAvailableForTeam(pBot->Player->GetTeam(), HIVE_TECH_SENSORY));
	default:
		return false;
	}

	return false;
}

bool AITASK_IsAlienGetHealthTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (IsEdictHive(Task->TaskTarget))
	{
		AvHAIHiveDefinition* HiveRef = AITAC_GetHiveFromEdict(Task->TaskTarget);

		if (!HiveRef || HiveRef->Status != HIVE_STATUS_BUILT) { return false; }
	}

	if (IsEdictStructure(Task->TaskTarget) && !UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (IsEdictPlayer(Task->TaskTarget))
	{
		if (!IsPlayerGorge(Task->TaskTarget)) { return false; }
	}
	return (pBot->Edict->v.health < pBot->Edict->v.max_health) || (!IsPlayerSkulk(pBot->Edict) && pBot->Edict->v.armorvalue < (GetPlayerMaxArmour(pBot->Edict) * 0.8f));
}

bool AITASK_IsAlienHealTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task->TaskTarget || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2) { return false; }

	if (GetPlayerOverallHealthPercent(Task->TaskTarget) >= 0.99f) { return false; }

	if (IsEdictStructure(Task->TaskTarget)) { return true; }

	// If our target is a player, give up if they are too far away. I'm not going to waste time chasing you around the map!
	float MaxHealRelevant = sqrf(UTIL_MetresToGoldSrcUnits(5.0f));

	return (IsPlayerActiveInGame(Task->TaskTarget) && vDist2DSq(pBot->CurrentFloorPosition, Task->TaskTarget->v.origin) <= MaxHealRelevant);
}

bool AITASK_IsUseTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	CBaseToggle* ToggleRef = dynamic_cast<CBaseToggle*>(CBaseEntity::Instance(Task->TaskTarget));

	if (!ToggleRef) { return false; }

	DoorTrigger* TriggerRef = UTIL_GetDoorTriggerByEntity(Task->TaskTarget);

	if (TriggerRef && gpGlobals->time < TriggerRef->NextActivationTime) { return false; }

	return ToggleRef->GetToggleState() == TS_AT_BOTTOM || (ToggleRef->GetToggleState() == TS_AT_TOP && (ToggleRef->pev->spawnflags & 32));
}


void BotProgressMoveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	Task->TaskStartedTime = gpGlobals->time;

	bool bPlayerCloaked = pBot->Player->GetOpacity() < 0.5f && !GetHasUpgrade(pBot->Edict->v.iuser4, MASK_SENSORY_NEARBY);

	if (IsPlayerLerk(pBot->Edict) || bPlayerCloaked)
	{
		if (AITAC_ShouldBotBeCautious(pBot))
		{
			if (bPlayerCloaked)
			{
				pBot->BotNavInfo.bShouldWalk = true;
			}

			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_AMBUSH, 100.0f);
		}
		else
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL, 100.0f);
		}

		return;
	}

	bool bMoveSuccess = MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);

	if (!bMoveSuccess)
	{
		MoveDirectlyTo(pBot, Task->TaskLocation);
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

		bool bMoveSuccess = MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);

		if (!bMoveSuccess)
		{
			MoveDirectlyTo(pBot, Task->TaskLocation);
		}
	}
}

void BotProgressPickupTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (Task->TaskType == TASK_GET_AMMO)
	{
		pBot->DesiredCombatWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);
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
				AvHAIWeapon CurrentPrimaryWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);

				if (CurrentPrimaryWeapon != WEAPON_INVALID && CurrentPrimaryWeapon != UTIL_GetWeaponTypeFromEdict(Task->TaskTarget))
				{
					if (GetPlayerCurrentWeapon(pBot->Player) != CurrentPrimaryWeapon)
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
	if (vIsZero(Task->TaskLocation))
	{
		Task->TaskLocation = UTIL_GetNextMinePosition2(Task->TaskTarget);

		if (vIsZero(Task->TaskLocation))
		{
			AITASK_ClearBotTask(pBot, Task);
			return;
		}
	}

	float DistToPlaceLocation = vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation);

	if (DistToPlaceLocation < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		pBot->DesiredCombatWeapon = WEAPON_MARINE_MINES;
	}
	else
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING)
	{
		return;
	}

	if (Task->ActiveBuildInfo.BuildStatus != BUILD_ATTEMPT_NONE)
	{
		if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_FAILED)
		{
			float Size = fmaxf(Task->TaskTarget->v.size.x, Task->TaskTarget->v.size.y);
			Size += 8.0f;
			Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInDonut(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), Task->TaskTarget->v.origin, Size, Size + 8.0f);
		}
		else
		{
			Task->TaskLocation = ZERO_VECTOR;
		}

		Task->ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_NONE;
	}

	BotLookAt(pBot, Task->TaskLocation);

	if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_MARINE_MINES)
	{
		Vector TraceStart = pBot->Player->GetGunPosition();
		Vector TraceDir = UTIL_GetForwardVector(pBot->Edict->v.v_angle);

		TraceResult tr;
		UTIL_TraceLine(TraceStart, TraceStart + (TraceDir * kMineRange), dont_ignore_monsters, pBot->Edict->v.pContainingEntity, &tr);

		bool bTraceSuccessful = false;

		if (tr.flFraction < 1.0f)
		{
			if (tr.pHit != Task->TaskTarget)
			{
				if (vDist2DSq(tr.vecEndPos, Task->TaskLocation) < sqrf(8.0f))
				{
					bTraceSuccessful = true;
				}
			}
		}

		if (bTraceSuccessful)
		{
			pBot->Button |= IN_ATTACK;
			Task->ActiveBuildInfo.AttemptedLocation = Task->TaskLocation;
			Task->ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_PENDING;
			Task->ActiveBuildInfo.BuildAttemptTime = gpGlobals->time;
			Task->ActiveBuildInfo.AttemptedStructureType = STRUCTURE_MARINE_DEPLOYEDMINE;
			Task->ActiveBuildInfo.NumAttempts++;
		}
		else
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
			return;
		}
	}
}



void BotProgressReinforceStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget)) { return; }

	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING) { return; }

	// We had a go, whether it succeeded or not we should try a new location
	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_FAILED || Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_SUCCESS)
	{
		Task->TaskLocation = ZERO_VECTOR;
		Task->ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_NONE;
	}

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	Vector ReinforceLocation = UTIL_ProjectPointToNavmesh(UTIL_GetEntityGroundLocation(Task->TaskTarget), pBot->BotNavInfo.NavProfile);
	float SearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

	if (IsEdictHive(Task->TaskTarget))
	{
		AvHAIHiveDefinition* HiveToReinforce = AITAC_GetHiveFromEdict(Task->TaskTarget);

		if (HiveToReinforce)
		{
			ReinforceLocation = HiveToReinforce->FloorLocation;
		}

		SearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
	}

	AvHAIDeployableStructureType NextStructure = STRUCTURE_NONE;

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTeam = BotTeam;
	StructureFilter.MaxSearchRadius = SearchRadius;
	StructureFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;

	int NumOCs = AITAC_GetNumDeployablesNearLocation(ReinforceLocation, &StructureFilter);

	if (NumOCs < 3)
	{
		NextStructure = STRUCTURE_ALIEN_OFFENCECHAMBER;
	}

	if (NextStructure == STRUCTURE_NONE)
	{
		if (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_DEFENSE_CHAMBER))
		{
			StructureFilter.DeployableTypes = STRUCTURE_ALIEN_DEFENCECHAMBER;

			int NumDCs = AITAC_GetNumDeployablesNearLocation(ReinforceLocation, &StructureFilter);

			if (NumDCs < 2)
			{
				NextStructure = STRUCTURE_ALIEN_DEFENCECHAMBER;
			}
		}
	}

	if (NextStructure == STRUCTURE_NONE)
	{
		if (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_MOVEMENT_CHAMBER))
		{
			StructureFilter.DeployableTypes = STRUCTURE_ALIEN_MOVEMENTCHAMBER;

			int NumMCs = AITAC_GetNumDeployablesNearLocation(ReinforceLocation, &StructureFilter);

			if (NumMCs < 1)
			{
				NextStructure = STRUCTURE_ALIEN_MOVEMENTCHAMBER;
			}
		}
	}

	if (NextStructure == STRUCTURE_NONE)
	{
		if (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_SENSORY_CHAMBER))
		{
			StructureFilter.DeployableTypes = STRUCTURE_ALIEN_SENSORYCHAMBER;

			int NumSCs = AITAC_GetNumDeployablesNearLocation(ReinforceLocation, &StructureFilter);

			if (NumSCs < 1)
			{
				NextStructure = STRUCTURE_ALIEN_SENSORYCHAMBER;
			}
		}
	}

	if (NextStructure != STRUCTURE_NONE)
	{
		if (vIsZero(Task->TaskLocation))
		{
			Vector NewLoc = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), ReinforceLocation, SearchRadius);

			if (vIsZero(NewLoc))
			{
				NewLoc = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(GORGE_BASE_NAV_PROFILE), ReinforceLocation, SearchRadius);
			}

			Task->TaskLocation = NewLoc;
		}

		if (!vIsZero(Task->TaskLocation))
		{
			float ResourceCost = UTIL_GetCostOfStructureType(NextStructure);
			if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
			{
				ResourceCost += BALANCE_VAR(kGorgeCost);
			}

			if (pBot->Player->GetResources() >= ResourceCost)
			{
				BotAlienPlaceChamber(pBot, Task, NextStructure);
				return;
			}
		}
	}

	// We have nothing to build, or we don't have enough resources yet, see if there's any unfinished structures we can finish off
	if (GetPlayerActiveClass(pBot->Player) == AVH_USER3_ALIEN_PLAYER2)
	{
		DeployableSearchFilter UnfinishedFilter;
		UnfinishedFilter.DeployableTeam = BotTeam;
		UnfinishedFilter.ExcludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		UnfinishedFilter.ReachabilityTeam = BotTeam;
		UnfinishedFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		UnfinishedFilter.DeployableTypes = SEARCH_ALL_ALIEN_STRUCTURES;
		UnfinishedFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

		AvHAIBuildableStructure UnfinishedStructure = AITAC_FindClosestDeployableToLocation(ReinforceLocation, &UnfinishedFilter);

		if (UnfinishedStructure.IsValid())
		{
			AIPlayerBuildStructure(pBot, UnfinishedStructure.edict);
			return;
		}
	}

	// We can't build anything, see if there's anything to attack nearby
	if (!IsPlayerGorge(pBot->Edict) || PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB))
	{
		DeployableSearchFilter EnemyStructureFilter;
		EnemyStructureFilter.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
		EnemyStructureFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
		EnemyStructureFilter.ReachabilityTeam = BotTeam;
		EnemyStructureFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		EnemyStructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

		AvHAIBuildableStructure EnemyStructure = AITAC_FindClosestDeployableToLocation(ReinforceLocation, &EnemyStructureFilter);

		if (EnemyStructure.IsValid())
		{
			BotAttackNonPlayerTarget(pBot, EnemyStructure.edict);
			return;
		}

	}

	BotGuardLocation(pBot, (!vIsZero(Task->TaskLocation)) ? Task->TaskLocation : ReinforceLocation);
}

void BotProgressResupplyTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player))
	{
		pBot->DesiredCombatWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);
	}
	else
	{
		pBot->DesiredCombatWeapon = GetBotMarineSecondaryWeapon(pBot);
	}

	bool bHasLOS = UTIL_PlayerHasLOSToEntity(pBot->Edict, Task->TaskTarget, UTIL_MetresToGoldSrcUnits(5.0f), false);
	float DistToArmoury = vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin);

	if (bHasLOS && DistToArmoury <= sqrf(max_player_use_reach))
	{
		BotUseObject(pBot, Task->TaskTarget, true);
		if (DistToArmoury > sqrf(50.0f))
		{
			MoveDirectlyTo(pBot, Task->TaskTarget->v.origin);
		}
		return;
	}

	MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

	if (bHasLOS)
	{
		BotLookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
	}

}

void AIPlayerBuildStructure(AvHAIPlayer* pBot, edict_t* BuildTarget)
{
	if (!pBot || !IsPlayerActiveInGame(pBot->Edict) || FNullEnt(BuildTarget) || UTIL_StructureIsFullyBuilt(BuildTarget)) { return; }

	if (IsPlayerAlien(pBot->Edict) && !IsPlayerGorge(pBot->Edict)) { return; }

	if (IsPlayerMarine(pBot->Edict))
	{
		// If we're not already building
		if (pBot->Edict->v.viewmodel != 0)
		{
			// If someone else is building, then we will guard
			edict_t* OtherBuilder = AITAC_GetClosestPlayerOnTeamWithLOS(pBot->Player->GetTeam(), BuildTarget->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), pBot->Edict);

			if (!FNullEnt(OtherBuilder) && OtherBuilder->v.weaponmodel == 0)
			{
				BotGuardLocation(pBot, BuildTarget->v.origin);
				return;
			}
		}
	}

	if (IsPlayerInUseRange(pBot->Edict, BuildTarget))
	{
		BotUseObject(pBot, BuildTarget, true);

		// Haven't started building, maybe not quite looking at the right angle
		if (IsPlayerMarine(pBot->Edict))
		{
			// If we were ducking before then keep ducking
			if (pBot->Edict->v.oldbuttons & IN_DUCK)
			{
				pBot->Button |= IN_DUCK;
			}

			if (pBot->Edict->v.weaponmodel != 0)
			{

				if (vDist2DSq(pBot->Edict->v.origin, BuildTarget->v.origin) > sqrf(60.0f))
				{
					MoveDirectlyTo(pBot, BuildTarget->v.origin);
				}
				else
				{
					Vector NewViewPoint = UTIL_GetRandomPointInBoundingBox(BuildTarget->v.absmin, BuildTarget->v.absmax);

					BotLookAt(pBot, NewViewPoint);
				}
			}
		}

		return;
	}
	
	// Might need to duck if it's an infantry portal
	if (vDist2DSq(pBot->Edict->v.origin, BuildTarget->v.origin) < sqrf(max_player_use_reach))
	{
		if (BuildTarget->v.origin > pBot->Edict->v.origin)
		{
			BotJump(pBot);
		}
		else
		{
			pBot->Button |= IN_DUCK;
		}

	}

	MoveTo(pBot, BuildTarget->v.origin, MOVESTYLE_NORMAL);

	if (vDist2DSq(pBot->Edict->v.origin, BuildTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		BotLookAt(pBot, UTIL_GetCentreOfEntity(BuildTarget));
	}
}

void MarineProgressBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	edict_t* pEdict = pBot->Edict;

	if (UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0 || UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) > 0)
	{
		pBot->DesiredCombatWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);
	}
	else
	{
		if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			pBot->DesiredCombatWeapon = GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			pBot->DesiredCombatWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);
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

	if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		BotLookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
	}

}

void BotProgressGuardTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{

	if (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		Task->TaskStartedTime = 0.0f;

		bool bPlayerCloaked = pBot->Player->GetOpacity() < 0.5f && !GetHasUpgrade(pBot->Edict->v.iuser4, MASK_SENSORY_NEARBY);

		if (IsPlayerLerk(pBot->Edict) || bPlayerCloaked)
		{
			if (AITAC_ShouldBotBeCautious(pBot))
			{
				if (bPlayerCloaked)
				{
					pBot->BotNavInfo.bShouldWalk = true;
				}

				MoveTo(pBot, Task->TaskLocation, MOVESTYLE_AMBUSH, 100.0f);
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

	if (IsPlayerGorge(pBot->Edict) && !PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB))
	{
		BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_ONE);
		return;
	}

	AvHAIWeapon Weapon = WEAPON_INVALID;

	if (IsPlayerMarine(pBot->Edict))
	{
		Weapon = BotMarineChooseBestWeaponForStructure(pBot, Task->TaskTarget);
	}
	else
	{
		Weapon = BotAlienChooseBestWeaponForStructure(pBot, Task->TaskTarget);
	}

	if (IsPlayerMarine(pBot->Player) && IsDamagingStructure(Task->TaskTarget) && !IsMeleeWeapon(Weapon))
	{
		if (GetPlayerCurrentWeaponClipAmmo(pBot->Player) == 0 || IsPlayerReloading(pBot->Player))
		{
			BotReloadWeapons(pBot);

			AvHTurret* TurretRef = dynamic_cast<AvHTurret*>(CBaseEntity::Instance(Task->TaskTarget));

			if (TurretRef && TurretRef->GetIsValidTarget(pBot->Player))
			{
				if (vIsZero(pBot->LastSafeLocation))
				{
					pBot->LastSafeLocation = AITAC_GetTeamStartingLocation(pBot->Player->GetTeam());
				}

				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
				return;
			}
		}
	}

	BotAttackResult AttackResult = PerformAttackLOSCheck(pBot, Weapon, Task->TaskTarget);

	if (AttackResult == ATTACK_SUCCESS)
	{
		// If we were ducking before then keep ducking
		if (pBot->Edict->v.oldbuttons & IN_DUCK)
		{
			pBot->Button |= IN_DUCK;
		}

		if (PlayerHasWeapon(pBot->Player, WEAPON_LERK_PRIMALSCREAM) && !pBot->Player->GetIsScreaming())
		{
			int NumBuffTargets = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), pBot->Edict->v.origin, BALANCE_VAR(kPrimalScreamRange), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);
			
			if (NumBuffTargets > 0)
			{
				pBot->DesiredCombatWeapon = WEAPON_LERK_PRIMALSCREAM;

				if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_LERK_PRIMALSCREAM)
				{
					pBot->Button |= IN_ATTACK;
				}

				return;
			}

		}

		BotShootTarget(pBot, Weapon, Task->TaskTarget);

		if (IsDamagingStructure(Task->TaskTarget) && !IsMeleeWeapon(Weapon))
		{
			Vector EnemyOrientation = UTIL_GetVectorNormal2D(Task->TaskTarget->v.origin - pBot->Edict->v.origin);

			Vector RightDir = UTIL_GetCrossProduct(EnemyOrientation, UP_VECTOR);

			pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(RightDir) : UTIL_GetVectorNormal2D(-RightDir);

			// Let's get ziggy with it
			if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
			{
				pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
				pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
			}

			BotMovementInputs(pBot);
		}

		return;
	}

	if (!vIsZero(Task->TaskLocation))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL, max_player_use_reach);
		return;
	}

	Vector AttackLocation = (IsEdictPlayer(Task->TaskTarget) || IsEdictStructure(Task->TaskTarget)) ? Task->TaskTarget->v.origin : UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, Task->TaskTarget);

	if (vIsZero(AttackLocation))
	{
		AttackLocation = Task->TaskTarget->v.origin;
	}

	float WeaponRange = GetMaxIdealWeaponRange(Weapon);

	Vector NewTaskLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, AttackLocation, WeaponRange);

	Task->TaskLocation = (!vIsZero(NewTaskLocation)) ? NewTaskLocation : AttackLocation;

	MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL, max_player_use_reach);
	
	return;
}

void BotProgressDefendTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{

	if (!FNullEnt(Task->TaskTarget))
	{
		vector<AvHPlayer*> Attackers = AITAC_GetAllPlayersOnTeamWithLOS(AIMGR_GetEnemyTeam(pBot->Player->GetTeam()), Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(15.0f), nullptr);
		edict_t* NearestAttacker = nullptr;
		float MinDist = 0.0f;		

		for (auto it = Attackers.begin(); it != Attackers.end(); it++)
		{
			AvHPlayer* ThisPlayer = (*it);
			edict_t* PlayerEdict = ThisPlayer->edict();

			if (FNullEnt(PlayerEdict)) { continue; }

			float ThisDist = vDist2DSq(pBot->Edict->v.origin, PlayerEdict->v.origin);

			if (FNullEnt(NearestAttacker) || ThisDist < MinDist)
			{
				NearestAttacker = PlayerEdict;
				MinDist = ThisDist;
			}
		}

		if (!FNullEnt(NearestAttacker))
		{
			MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestAttacker), MOVESTYLE_NORMAL);
			return;
		}

		AvHAIBuildableStructure StructureRef = AITAC_GetDeployableFromEdict(Task->TaskTarget);

		if (StructureRef.IsValid())
		{
			// If the structure we're defending was damaged just now, look at it so we can see who is attacking
			if (gpGlobals->time - StructureRef.lastDamagedTime < 5.0f)
			{
				if (UTIL_QuickTrace(pBot->Edict, pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(Task->TaskTarget)))
				{
					BotLookAt(pBot, Task->TaskTarget);
				}
			}
		}
		else
		{
			AvHAIHiveDefinition* Hive = AITAC_GetHiveFromEdict(Task->TaskTarget);

			if (Hive && Hive->bIsUnderAttack)
			{
				if (UTIL_QuickTrace(pBot->Edict, pBot->CurrentEyePosition, Hive->Location))
				{
					BotLookAt(pBot, Task->TaskTarget);
				}
			}
		}		
	}

	BotProgressGuardTask(pBot, Task);
}

void BotProgressTakeCommandTask(AvHAIPlayer* pBot)
{
	// Don't take command if we already have a commander
	if (pBot->Player->GetCommander()) { return; }

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
		if (gpGlobals->time > AIMGR_GetCommanderAllowedTime(pBot->Player->GetTeam()))
		{
			BotUseObject(pBot, CommChair, false);
		}
		else
		{
			edict_t* NearestHuman = AITAC_GetNearestHumanAtLocation(pBot->Player->GetTeam(), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

			if (FNullEnt(NearestHuman))
			{
				BotLookAt(pBot, CommChair);
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
			Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (vIsZero(Task->TaskLocation))
			{
				Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));
			}

			Vector FinalEvolveLocation = FindClosestNavigablePointToDestination(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], pBot->CurrentFloorPosition, Task->TaskLocation, 60.0f);

			if (!vIsZero(FinalEvolveLocation))
			{
				Task->TaskLocation = FinalEvolveLocation;
			}

			Task->TaskStartedTime = 0.0f;
		}
		return;
	}

	if (!vIsZero(Task->TaskLocation))
	{
		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			if (!UTIL_PointIsReachable(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], pBot->CurrentFloorPosition, Task->TaskLocation, 60.0f))
			{
				Vector FinalEvolveLocation = FindClosestNavigablePointToDestination(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], pBot->CurrentFloorPosition, Task->TaskLocation, 60.0f);

				if (!vIsZero(FinalEvolveLocation))
				{
					Task->TaskLocation = FinalEvolveLocation;
				}
			}
		}

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
			Task->TaskLocation = FindClosestNavigablePointToDestination(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], AITAC_GetTeamStartingLocation(pBot->Player->GetTeam()), UTIL_GetEntityGroundLocation(Task->TaskTarget), UTIL_MetresToGoldSrcUnits(10.0f));

			if (vIsZero(Task->TaskLocation))
			{
				Task->TaskLocation = pBot->CurrentFloorPosition;
			}

			return;
		}
		else
		{
			if (vDist2DSq(pBot->Edict->v.origin, UTIL_GetEntityGroundLocation(Task->TaskTarget)) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)) || UTIL_GetNavAreaAtLocation(BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE], pBot->Edict->v.origin) != SAMPLE_POLYAREA_GROUND)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(Task->TaskTarget), MOVESTYLE_NORMAL, UTIL_MetresToGoldSrcUnits(10.0f));
				return;
			}
			else
			{
				Task->TaskLocation = FindClosestNavigablePointToDestination(BaseNavProfiles[ONOS_BASE_NAV_PROFILE], pBot->CurrentFloorPosition, UTIL_GetEntityGroundLocation(Task->TaskTarget), UTIL_MetresToGoldSrcUnits(10.0f));

				if (vIsZero(Task->TaskLocation))
				{
					Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE], Task->TaskLocation, UTIL_MetresToGoldSrcUnits(5.0f));
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
		if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_FADE_METABOLIZE)
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

			AIPlayerRequestHealth(pBot);
		}
	}
}

void AlienProgressHealTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (FNullEnt(Task->TaskTarget) || IsPlayerDead(Task->TaskTarget)) { return; }

	BotAlienHealTarget(pBot, Task->TaskTarget);
}

void AlienProgressBuildHiveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING)
	{
		return;
	}

	const AvHAIHiveDefinition* Hive = AITAC_GetHiveNearestLocation(Task->TaskLocation);

	if (!Hive)
	{
		AITASK_ClearBotTask(pBot, Task);
		return;
	}

	
	int ResRequired = UTIL_GetCostOfStructureType(Task->StructureType);

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		ResRequired += BALANCE_VAR(kGorgeCost);
	}

	if (pBot->Player->GetResources() < ResRequired)
	{
		BotGuardLocation(pBot, Task->TaskLocation);
		return;
	}

	BotAlienBuildHive(pBot, Task, Hive);
}

void AlienProgressBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	// We tried and failed to place the structure
	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING)
	{
		return;
	}

	if (Task->StructureType == STRUCTURE_ALIEN_HIVE)
	{
		AlienProgressBuildHiveTask(pBot, Task);
		return;
	}

	if (Task->ActiveBuildInfo.LinkedStructure)
	{
		edict_t* LinkedEdict = Task->ActiveBuildInfo.LinkedStructure->edict;

		if (UTIL_StructureIsFullyBuilt(LinkedEdict)) { return; }

		if (IsPlayerInUseRange(pBot->Edict, LinkedEdict))
		{
			BotUseObject(pBot, LinkedEdict, true);
			if (vDist2DSq(pBot->Edict->v.origin, LinkedEdict->v.origin) > sqrf(60.0f))
			{
				MoveDirectlyTo(pBot, LinkedEdict->v.origin);
			}
			return;
		}

		MoveTo(pBot, LinkedEdict->v.origin, MOVESTYLE_NORMAL);

		return;
	}

	// We tried and failed to place the structure
	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_FAILED)
	{
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[GORGE_BASE_NAV_PROFILE], Task->TaskLocation, UTIL_MetresToGoldSrcUnits(2.0f));
		Task->ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_NONE;
	}

	int ResRequired = UTIL_GetCostOfStructureType(Task->StructureType);

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		ResRequired += BALANCE_VAR(kGorgeCost);
	}

	if (pBot->Player->GetResources() < ResRequired)
	{

		if (IsPlayerGorge(pBot->Edict))
		{
			AvHTeamNumber BotTeam = pBot->Player->GetTeam();

			DeployableSearchFilter UnfinishedFilter;
			UnfinishedFilter.DeployableTeam = BotTeam;
			UnfinishedFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
			UnfinishedFilter.ReachabilityTeam = BotTeam;
			UnfinishedFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
			UnfinishedFilter.ExcludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			UnfinishedFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

			AvHAIBuildableStructure UnfinishedStructure = AITAC_FindClosestDeployableToLocation(Task->TaskLocation, &UnfinishedFilter);

			if (UnfinishedStructure.IsValid())
			{
				AIPlayerBuildStructure(pBot, UnfinishedStructure.edict);
				return;
			}
		}


		BotGuardLocation(pBot, Task->TaskLocation);
		return;
	}

	BotAlienPlaceChamber(pBot, Task, Task->StructureType);
}

void BotAlienPlaceChamber(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, AvHAIDeployableStructureType DesiredStructure)
{
	if (vIsZero(Task->TaskLocation) || DesiredStructure == STRUCTURE_NONE) { return; }

	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING) { return; }

	float DistFromBuildLocation = vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation);

	if (DistFromBuildLocation > sqrf(UTIL_MetresToGoldSrcUnits(1.0f)))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	if (DistFromBuildLocation < sqrf(UTIL_MetresToGoldSrcUnits(0.5f)))
	{
		BotLookAt(pBot, Task->TaskLocation);
		Vector Orientation = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - Task->TaskLocation);
		Vector NewMoveLoc = Task->TaskLocation + (Orientation * UTIL_MetresToGoldSrcUnits(2.0f));
		MoveToWithoutNav(pBot, NewMoveLoc);

		return;
	}

	Vector LookLocation = Task->TaskLocation;
	LookLocation.z += 10.0f;

	BotLookAt(pBot, LookLocation);

	int ResRequired = UTIL_GetCostOfStructureType(DesiredStructure);

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		ResRequired += BALANCE_VAR(kGorgeCost);
	}

	if (pBot->Player->GetResources() < ResRequired)
	{
		return;
	}

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_TWO);
		return;
	}
	
	float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->Edict->v.v_angle), UTIL_GetVectorNormal2D(LookLocation - pBot->Edict->v.origin));

	if (LookDot > 0.9f)
	{
		pBot->Impulse = UTIL_StructureTypeToImpulseCommand(DesiredStructure);
		RegisterBotAlienBuildAttempt(pBot, Task, Task->TaskLocation, DesiredStructure);
	}
}

void BotAlienBuildResTower(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const AvHAIResourceNode* NodeToCap)
{
	if (NodeToCap->bIsOccupied) { return; }

	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING) { return; }

	float CurrDist = vDist2DSq(pBot->CurrentFloorPosition, NodeToCap->Location);

	// Get close enough to place the tower if we aren't
	if (CurrDist > sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
	{
		MoveTo(pBot, NodeToCap->Location, MOVESTYLE_NORMAL);
		return;
	}

	// Back up a bit if we're too close
	if (CurrDist < sqrf(UTIL_MetresToGoldSrcUnits(1.0f)))
	{
		BotLookAt(pBot, NodeToCap->Location);
		Vector Orientation = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - NodeToCap->Location);
		Vector NewMoveLoc = NodeToCap->Location + (Orientation * UTIL_MetresToGoldSrcUnits(2.0f));
		MoveToWithoutNav(pBot, NewMoveLoc);

		return;
	}

	int ResRequired = UTIL_GetCostOfStructureType(STRUCTURE_ALIEN_RESTOWER);

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		ResRequired += BALANCE_VAR(kGorgeCost);
	}

	BotLookAt(pBot, NodeToCap->Location);

	if (pBot->Player->GetResources() < ResRequired)
	{
		return;
	}

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_TWO);
		return;
	}

	float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->Edict->v.v_angle), UTIL_GetVectorNormal2D(NodeToCap->Location - pBot->Edict->v.origin));

	if (LookDot > 0.9f)
	{
		pBot->Impulse = UTIL_StructureTypeToImpulseCommand(STRUCTURE_ALIEN_RESTOWER);
		RegisterBotAlienBuildAttempt(pBot, Task, NodeToCap->Location, STRUCTURE_ALIEN_RESTOWER);
	}

}

void BotAlienBuildHive(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const AvHAIHiveDefinition* HiveToBuild)
{
	// Do nothing if the hive is already built / under construction
	if (HiveToBuild->Status != HIVE_STATUS_UNBUILT) { return; }

	if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING) { return; }

	if (vDist2DSq(pBot->Edict->v.origin, HiveToBuild->Location) > sqrf(UTIL_MetresToGoldSrcUnits(7.5f)))
	{
		MoveTo(pBot, HiveToBuild->FloorLocation, MOVESTYLE_NORMAL);
		return;
	}

	BotLookAt(pBot, HiveToBuild->Location);

	Vector TraceStart = GetPlayerEyePosition(pBot->Edict);
	Vector TraceEnd = TraceStart + (UTIL_GetForwardVector(pBot->Edict->v.v_angle) * 60.0f);

	if (!UTIL_QuickTrace(pBot->Edict, TraceStart, TraceEnd))
	{
		MoveTo(pBot, HiveToBuild->FloorLocation, MOVESTYLE_NORMAL);
		return;
	}
	else
	{
		int ResRequired = UTIL_GetCostOfStructureType(STRUCTURE_ALIEN_HIVE);

		if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
		{
			ResRequired += BALANCE_VAR(kGorgeCost);
		}

		if (pBot->Player->GetResources() < ResRequired)
		{
			return;
		}

		if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
		{
			BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_TWO);
			return;
		}

		pBot->Impulse = UTIL_StructureTypeToImpulseCommand(STRUCTURE_ALIEN_HIVE);
		RegisterBotAlienBuildAttempt(pBot, Task, HiveToBuild->Location, STRUCTURE_ALIEN_HIVE);
	}
}

void BotAlienHealTarget(AvHAIPlayer* pBot, edict_t* HealTarget)
{
	float MaxHealRange = GetMaxIdealWeaponRange(WEAPON_GORGE_HEALINGSPRAY);
	float TargetHealRange = MaxHealRange * 0.5f;
	
	BotAttackResult HitCheck = PerformAttackLOSCheck(pBot, WEAPON_GORGE_HEALINGSPRAY, HealTarget);

	if (HitCheck == ATTACK_SUCCESS)
	{
		if (IsPlayerGorge(pBot->Edict))
		{
			BotShootTarget(pBot, WEAPON_GORGE_HEALINGSPRAY, HealTarget);
			return;
		}
		else
		{
			BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_TWO);
			return;
		}
	}
	else
	{
		MoveTo(pBot, UTIL_GetEntityGroundLocation(HealTarget), MOVESTYLE_NORMAL, MaxHealRange);
	}

}

void RegisterBotAlienBuildAttempt(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, Vector PlacementLocation, AvHAIDeployableStructureType DesiredStructure)
{
	Task->ActiveBuildInfo.AttemptedLocation = PlacementLocation;
	Task->ActiveBuildInfo.BuildAttemptTime = gpGlobals->time;
	Task->ActiveBuildInfo.LinkedStructure = nullptr;
	Task->ActiveBuildInfo.NumAttempts++;
	Task->ActiveBuildInfo.AttemptedStructureType = DesiredStructure;
	Task->ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_PENDING;
}

void AlienProgressCapResNodeTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	// We can attack structures if we're not a gorge stuck with spit as our only offensive weapon
	bool bBotCanAttackStructures = !IsPlayerGorge(pBot->Edict) || PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB);

	// First, clear out any marine phase gates nearby so they can't send backup, if we can actually do meaningful damage to them
	if (bBotCanAttackStructures)
	{
		AvHClassType EnemyClassType = AIMGR_GetEnemyTeamType(BotTeam);

		if (EnemyClassType == AVH_CLASS_TYPE_MARINE)
		{
			DeployableSearchFilter PGFilter;
			PGFilter.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
			PGFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
			PGFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);
			PGFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

			AvHAIBuildableStructure PG = AITAC_FindClosestDeployableToLocation(ResNodeIndex->Location, &PGFilter);

			if (PG.IsValid())
			{
				BotAttackNonPlayerTarget(pBot, PG.edict);
				return;
			}
		}
	}

	if (ResNodeIndex->bIsOccupied)
	{
		// If we have a tower on there already then help build it if we're gorge
		if (ResNodeIndex->OwningTeam == BotTeam)
		{
			if (!UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity))
			{
				if (IsPlayerGorge(pBot->Edict))
				{
					AIPlayerBuildStructure(pBot, ResNodeIndex->ActiveTowerEntity);
					return;
				}				
			}
		}
		else
		{
			// If the enemy owns it then destroy it if we can, or go Skulk to do so.
			if (bBotCanAttackStructures)
			{
				BotAttackNonPlayerTarget(pBot, ResNodeIndex->ActiveTowerEntity);
				return;
			}
			else
			{
				BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_ONE);
				return;
			}
		}
	}
	else
	{
		// Node is empty and not capped by either side

		int NumResourcesRequired = (GetPlayerActiveClass(pBot->Player) == AVH_USER3_ALIEN_PLAYER2) ? BALANCE_VAR(kResourceTowerCost) : (BALANCE_VAR(kResourceTowerCost) + BALANCE_VAR(kGorgeCost));

		// We have enough resources to place the tower (includes cost of evolving to gorge if necessary)
		if (pBot->Player->GetResources() >= NumResourcesRequired)
		{
			BotAlienBuildResTower(pBot, Task, ResNodeIndex);
			return;
		}
	}

	// We don't have enough resources to cap the node yet, so take out any enemy structures in the area while we wait if we can
	if (bBotCanAttackStructures)
	{
		DeployableSearchFilter EnemyStructureFilter;
		EnemyStructureFilter.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
		EnemyStructureFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
		EnemyStructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

		AvHAIBuildableStructure AttackTarget = AITAC_FindClosestDeployableToLocation(ResNodeIndex->Location, &EnemyStructureFilter);

		if (AttackTarget.IsValid())
		{
			BotAttackNonPlayerTarget(pBot, AttackTarget.edict);
			return;
		}
	}

	// No structures to take out, just wait for resources
	BotGuardLocation(pBot, ResNodeIndex->Location);


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
		else
		{
			AlienProgressSecureHiveTask(pBot, Task);
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
			AvHAIDroppedItem* Welder = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_WELDER, pBot->Player->GetTeam(), pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, 0.0f, true);

			if (Welder)
			{
				Task->TaskSecondaryTarget = Welder->edict;
			}
		}

		return;
	}

	if (IsPlayerInUseRange(pBot->Edict, Task->TaskTarget))
	{
		Vector EntityCentre = UTIL_GetCentreOfEntity(Task->TaskTarget);
		Vector AimLocation = EntityCentre;

		// If we're targeting a func_weldable, then the centre of the entity might be in a wall or out of reach
		// so instead aim at the closest point on the func_weldable to us.
		if (!IsEdictPlayer(Task->TaskTarget) && !IsEdictStructure(Task->TaskTarget))
		{
			if (Task->TaskTarget->v.size.Length2D() < 100.0f)
			{
				AimLocation = UTIL_GetCentreOfEntity(Task->TaskTarget);
			}
			else
			{
				Vector BBMin = Task->TaskTarget->v.absmin + Vector(5.0f, 5.0f, 5.0f);
				Vector BBMax = Task->TaskTarget->v.absmax - Vector(5.0f, 5.0f, 5.0f);

				vScaleBB(BBMin, BBMax, 0.75f);

				AimLocation = vClosestPointOnBB(pBot->CurrentEyePosition, BBMin, BBMax);

				if (Task->TaskTarget->v.absmax.z - Task->TaskTarget->v.absmin.z < 100.0f)
				{
					AimLocation.z = EntityCentre.z;
				}
			}

		}

		BotLookAt(pBot, AimLocation);
		pBot->DesiredCombatWeapon = WEAPON_MARINE_WELDER;

		if (GetPlayerCurrentWeapon(pBot->Player) != WEAPON_MARINE_WELDER)
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

void AlienProgressSecureHiveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	const AvHAIHiveDefinition* Hive = AITAC_GetHiveFromEdict(Task->TaskTarget);

	if (!Hive) { return; }

	AvHAISquad* ActiveSquad = AITAC_GetSquadForObjective(pBot, Task->TaskTarget, Task->TaskType);

	if (ActiveSquad && !ActiveSquad->bExecuteObjective && !vIsZero(ActiveSquad->SquadGatherLocation))
	{
		BotGuardLocation(pBot, ActiveSquad->SquadGatherLocation);
		return;
	}

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	bool bEnemyIsMarines = AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE;
	// Don't prioritise electrified structures if we're skulk or lerk
	bool bAvoidElectrified = (pBot->Player->GetUser3() == AVH_USER3_ALIEN_PLAYER1 || pBot->Player->GetUser3() == AVH_USER3_ALIEN_PLAYER3);

	DeployableSearchFilter EnemyStuffFilter;
	EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
	EnemyStuffFilter.DeployableTeam = EnemyTeam;
	EnemyStuffFilter.ReachabilityTeam = BotTeam;
	EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	if (bEnemyIsMarines)
	{
		EnemyStuffFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;

		AvHAIBuildableStructure PhaseGate = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &EnemyStuffFilter);

		if (PhaseGate.IsValid())
		{
			if (bAvoidElectrified)
			{
				EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
				EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_ELECTRIFIED;
				EnemyStuffFilter.MaxSearchRadius = BALANCE_VAR(kElectricalRange);

				AvHAIBuildableStructure ElectrifiedStructure = AITAC_FindClosestDeployableToLocation(PhaseGate.Location, &EnemyStuffFilter);

				if (ElectrifiedStructure.IsValid())
				{
					BotAlienAttackNonPlayerTarget(pBot, ElectrifiedStructure.edict);
					return;
				}
			}

			BotAlienAttackNonPlayerTarget(pBot, PhaseGate.edict);
			return;
		}

		EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
		EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_NONE;
		EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		AvHAIBuildableStructure TF = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &EnemyStuffFilter);

		if (TF.IsValid())
		{
			BotAlienAttackNonPlayerTarget(pBot, TF.edict);
			return;
		}

		EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;

		AvHAIBuildableStructure EnemyStructure = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &EnemyStuffFilter);

		if (EnemyStructure.IsValid())
		{
			BotAlienAttackNonPlayerTarget(pBot, EnemyStructure.edict);
			return;
		}
	}
	else
	{
		EnemyStuffFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;

		AvHAIBuildableStructure EnemyOC = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &EnemyStuffFilter);

		if (EnemyOC.IsValid())
		{
			BotAlienAttackNonPlayerTarget(pBot, EnemyOC.edict);
			return;
		}

		if (Hive->Status == HIVE_STATUS_BUILT && Hive->OwningTeam != BotTeam)
		{
			BotAlienAttackNonPlayerTarget(pBot, Hive->HiveEdict);
			return;
		}

		EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;

		AvHAIBuildableStructure AnyEnemyStructure = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &EnemyStuffFilter);

		if (AnyEnemyStructure.IsValid())
		{
			BotAlienAttackNonPlayerTarget(pBot, AnyEnemyStructure.edict);
			return;
		}

		if (Hive->Status != HIVE_STATUS_UNBUILT && Hive->OwningTeam != BotTeam)
		{
			BotAlienAttackNonPlayerTarget(pBot, Hive->HiveEdict);
			return;
		}
	}

	BotGuardLocation(pBot, Hive->FloorLocation);
}

void MarineProgressSecureHiveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	const AvHAIHiveDefinition* Hive = AITAC_GetHiveFromEdict(Task->TaskTarget);

	if (!Hive) { return; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = SEARCH_ALL_MARINE_STRUCTURES;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
	StructureFilter.DeployableTeam = BotTeam;
	StructureFilter.ReachabilityTeam = BotTeam;
	StructureFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	vector<AvHAIBuildableStructure> BuildableStructures = AITAC_FindAllDeployables(Hive->FloorLocation, &StructureFilter);

	bool bKeyStructureBuilt = false;

	AvHAIBuildableStructure StructureToBuild;
	float MinDist = 0.0f;

	for (auto it = BuildableStructures.begin(); it != BuildableStructures.end(); it++)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		if ((ThisStructure.StructureStatusFlags & STRUCTURE_STATUS_COMPLETED) && (ThisStructure.StructureType & (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_PHASEGATE)))
		{
			bKeyStructureBuilt = true;
		}

		if (ThisStructure.StructureStatusFlags & STRUCTURE_STATUS_COMPLETED) { continue; }
		
		// Phase gates always take priority, so just go and build it if there is one
		if (ThisStructure.StructureType == STRUCTURE_MARINE_PHASEGATE)
		{
			AIPlayerBuildStructure(pBot, ThisStructure.edict);
			return;
		}

		float ThisDist = vDist2DSq(pBot->Edict->v.origin, ThisStructure.Location);

		if (FNullEnt(StructureToBuild.edict) || ThisDist < MinDist)
		{
			StructureToBuild = ThisStructure;
			MinDist = ThisDist;
		}
	}

	if (StructureToBuild.IsValid())
	{
		AIPlayerBuildStructure(pBot, StructureToBuild.edict);
		return;
	}

	if (Hive->Status != HIVE_STATUS_UNBUILT && Hive->OwningTeam != BotTeam)
	{
		BotAttackNonPlayerTarget(pBot, Hive->HiveEdict);
		return;
	}

	const AvHAIResourceNode* ResNode = Hive->HiveResNodeRef;

	if (ResNode && ResNode->bIsOccupied)
	{
		if (ResNode->OwningTeam != BotTeam)
		{
			// Don't attack the RT until we have build a TF or PG. Avoids giving the game away too quickly
			if (bKeyStructureBuilt)
			{
				BotAttackNonPlayerTarget(pBot, ResNode->ActiveTowerEntity);
				return;
			}
			
		}
		else
		{
			if (!UTIL_StructureIsFullyBuilt(ResNode->ActiveTowerEntity))
			{
				AIPlayerBuildStructure(pBot, ResNode->ActiveTowerEntity);
				return;
			}
		}
	}

	// We won't start attacking enemy structures until we have built a turret factory or phase gate so we don't reveal our evil plans until we're ready
	if (bKeyStructureBuilt)
	{
		DeployableSearchFilter EnemyStructures;
		EnemyStructures.DeployableTypes = SEARCH_ALL_STRUCTURES;
		EnemyStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
		EnemyStructures.DeployableTeam = EnemyTeam;
		EnemyStructures.ReachabilityTeam = BotTeam;
		EnemyStructures.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

		AvHAIBuildableStructure EnemyStructure = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &EnemyStructures);

		if (EnemyStructure.IsValid())
		{
			BotAttackNonPlayerTarget(pBot, EnemyStructure.edict);
			return;
		}
	}


	BotGuardLocation(pBot, Task->TaskLocation);
	
}

void MarineProgressCapResNodeTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task) { return; }

	const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

	// This shouldn't happen, but if somehow it does then at least do SOMETHING
	if (!ResNodeIndex)
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	// There is a res tower, ours or the enemies
	if (ResNodeIndex->bIsOccupied)
	{
		Task->TaskTarget = ResNodeIndex->ActiveTowerEntity;
		
		if (ResNodeIndex->OwningTeam == pBot->Player->GetTeam())
		{
			if (!UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity))
			{
				// Cancel the waiting timeout since there is something for us to do
				Task->TaskLength = 0.0f;
				AIPlayerBuildStructure(pBot, ResNodeIndex->ActiveTowerEntity);

				return;
			}
		}
		else
		{
			// Cancel the waiting timeout since there is something for us to do
			Task->TaskLength = 0.0f;

			// If we're playing MvM, then check the enemy hasn't got a phase gate nearby which could bring in defenders. If so, take that out first.
			AvHClassType EnemyType = AIMGR_GetEnemyTeamType(pBot->Player->GetTeam());

			if (EnemyType == AVH_CLASS_TYPE_MARINE)
			{
				DeployableSearchFilter EnemyStructureFilter;
				EnemyStructureFilter.DeployableTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());
				EnemyStructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
				EnemyStructureFilter.ReachabilityTeam = pBot->Player->GetTeam();
				EnemyStructureFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
				EnemyStructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

				AvHAIBuildableStructure EnemyStructure = AITAC_FindClosestDeployableToLocation(Task->TaskLocation, &EnemyStructureFilter);

				if (EnemyStructure.IsValid())
				{
					BotAttackNonPlayerTarget(pBot, EnemyStructure.edict);
					return;
				}
			}

			BotAttackNonPlayerTarget(pBot, ResNodeIndex->ActiveTowerEntity);
			return;
		}
	}

	// Clear out any enemy structures around the node
	DeployableSearchFilter EnemyStructureFilter;

	EnemyStructureFilter.DeployableTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());
	EnemyStructureFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
	EnemyStructureFilter.ReachabilityTeam = pBot->Player->GetTeam();
	EnemyStructureFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyStructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

	AvHAIBuildableStructure EnemyStructure = AITAC_FindClosestDeployableToLocation(Task->TaskLocation, &EnemyStructureFilter);

	if (EnemyStructure.IsValid())
	{
		// Cancel the waiting timeout since we have something useful to do
		Task->TaskLength = 0.0f;
		BotAttackNonPlayerTarget(pBot, EnemyStructure.edict);
		return;
	}
	else
	{
		// If we're not at our destination yet, go there
		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
			return;
		}

		// Empty res node with nothing to do but wait, stick around for 30 seconds and then move on if the commander doesn't drop an RT to build
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
		UTIL_ClearGuardInfo(pBot);
		MoveTo(pBot, GuardLocation, MOVESTYLE_NORMAL, UTIL_MetresToGoldSrcUnits(10.0f));
		return;
	}

	if (vIsZero(pBot->GuardInfo.GuardLocation))
	{
		AITASK_GenerateGuardWatchPoints(pBot, GuardLocation);
		pBot->GuardInfo.GuardLocation = GuardLocation;
	}

	if (gpGlobals->time > pBot->GuardInfo.ThisGuardLookTime)
	{
		if (pBot->GuardInfo.GuardPoints.size() > 0)
		{
			if (pBot->GuardInfo.GuardPoints.size() == 1)
			{
				pBot->GuardInfo.GuardLookLocation = (*pBot->GuardInfo.GuardPoints.begin());
			}
			else
			{
				Vector NewLookPoint = pBot->GuardInfo.GuardLookLocation;

				int HighestScore = 0.0f;

				for (auto it = pBot->GuardInfo.GuardPoints.begin(); it != pBot->GuardInfo.GuardPoints.end(); it++)
				{
					if (vEquals((*it), pBot->GuardInfo.GuardLookLocation)) { continue; }

					float thisScore = frandrange(0.01f, 1.0f);

					if (thisScore > HighestScore)
					{
						NewLookPoint = (*it);
						HighestScore = thisScore;
					}
				}

				pBot->GuardInfo.GuardLookLocation = NewLookPoint;
			}

		}
		else
		{
			pBot->GuardInfo.GuardLookLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[SKULK_BASE_NAV_PROFILE], pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(3.0f));

			pBot->GuardInfo.GuardLookLocation.z = pBot->CurrentEyePosition.z;
		}

		pBot->GuardInfo.ThisGuardLookTime = gpGlobals->time + frandrange(2.0f, 5.0f);
	}

	if (gpGlobals->time > pBot->GuardInfo.ThisGuardStandTime)
	{
		pBot->GuardInfo.GuardStandPosition = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, GuardLocation, UTIL_MetresToGoldSrcUnits(3.0f));

		pBot->GuardInfo.ThisGuardStandTime = gpGlobals->time + frandrange(5.0f, 10.0f);
	}

	if (vDist2DSq(pBot->Edict->v.origin, pBot->GuardInfo.GuardStandPosition) > sqrf(32.0f))
	{
		if (IsPlayerLerk(pBot->Edict))
		{
			MoveTo(pBot, pBot->GuardInfo.GuardStandPosition, MOVESTYLE_AMBUSH);
		}
		else
		{
			MoveTo(pBot, pBot->GuardInfo.GuardStandPosition, MOVESTYLE_NORMAL);
		}
	}

	BotLookAt(pBot, pBot->GuardInfo.GuardLookLocation);


}

void UTIL_ClearGuardInfo(AvHAIPlayer* pBot)
{
	pBot->GuardInfo.GuardLocation = ZERO_VECTOR;
	pBot->GuardInfo.GuardLookLocation = ZERO_VECTOR;
	pBot->GuardInfo.GuardPoints.clear();
	pBot->GuardInfo.GuardStandPosition = ZERO_VECTOR;
	pBot->GuardInfo.GuardStartLookTime = 0.0f;
	pBot->GuardInfo.GuardStartStandTime = 0.0f;
	pBot->GuardInfo.ThisGuardLookTime = 0.0f;
	pBot->GuardInfo.ThisGuardStandTime = 0.0f;
}

void AITASK_GenerateGuardWatchPoints(AvHAIPlayer* pBot, const Vector& GuardLocation)
{
	const edict_t* pEdict = pBot->Edict;

	UTIL_ClearGuardInfo(pBot);

	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());

	bool bEnemyIsAlien = GetGameRules()->GetTeam(EnemyTeam)->GetTeamType() == AVH_CLASS_TYPE_ALIEN;

	const nav_profile NavProfile = (bEnemyIsAlien) ? BaseNavProfiles[SKULK_BASE_NAV_PROFILE] : BaseNavProfiles[MARINE_BASE_NAV_PROFILE];

	vector<bot_path_node> path;
	path.clear();
	
	vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllHives();

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		const AvHAIHiveDefinition* ThisHive = (*it);

		if (UTIL_QuickTrace(pEdict, GuardLocation, ThisHive->Location)) { continue; }

		if (UTIL_QuickTrace(pEdict, GuardLocation + Vector(0.0f, 0.0f, 10.0f), ThisHive->Location) || vDist2DSq(GuardLocation, ThisHive->Location) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { continue; }

		dtStatus SearchResult = FindPathClosestToPoint(NavProfile, GuardLocation, ThisHive->FloorLocation, path, 500.0f);

		if (dtStatusSucceed(SearchResult) && path.size() > 0)
		{
			Vector FurthestPoint = UTIL_GetFurthestVisiblePointOnPath(GuardLocation + Vector(0.0f, 0.0f, 64.0f), path, true);
			FurthestPoint.z += 64.0f;

			Vector LookDir = UTIL_GetVectorNormal(FurthestPoint - GuardLocation);

			bool bShouldAdd = true;

			for (auto it = pBot->GuardInfo.GuardPoints.begin(); it != pBot->GuardInfo.GuardPoints.end(); it++)
			{
				Vector ThisLookDir = UTIL_GetVectorNormal((*it) - GuardLocation);

				if (UTIL_GetDotProduct(ThisLookDir, LookDir) > 0.8f)
				{
					bShouldAdd = false;
					break;
				}
			}

			if (bShouldAdd)
			{
				pBot->GuardInfo.GuardPoints.push_back(FurthestPoint);
			}
		}
	}
	
	if (AIMGR_GetEnemyTeamType(pBot->Player->GetTeam()) == AVH_CLASS_TYPE_MARINE)
	{
		if (!UTIL_QuickTrace(nullptr, GuardLocation, AITAC_GetTeamStartingLocation(EnemyTeam)))
		{
			dtStatus SearchResult = FindPathClosestToPoint(NavProfile, GuardLocation, AITAC_GetTeamStartingLocation(EnemyTeam), path, 500.0f);

			if (dtStatusSucceed(SearchResult) && path.size() > 0)
			{
				Vector FurthestPoint = UTIL_GetFurthestVisiblePointOnPath(GuardLocation + Vector(0.0f, 0.0f, 64.0f), path, true);
				FurthestPoint.z += 64.0f;

				Vector LookDir = UTIL_GetVectorNormal(FurthestPoint - GuardLocation);

				bool bShouldAdd = true;

				for (auto it = pBot->GuardInfo.GuardPoints.begin(); it != pBot->GuardInfo.GuardPoints.end(); it++)
				{
					Vector ThisLookDir = UTIL_GetVectorNormal((*it) - GuardLocation);

					if (UTIL_GetDotProduct(ThisLookDir, LookDir) > 0.8f)
					{
						bShouldAdd = false;
						break;
					}
				}

				if (bShouldAdd)
				{
					pBot->GuardInfo.GuardPoints.push_back(FurthestPoint);
				}
			}
		}
	}

	if (AIMGR_GetTeamType(pBot->Player->GetTeam()) == AVH_CLASS_TYPE_MARINE)
	{
		if (!UTIL_QuickTrace(pEdict, GuardLocation, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam())))
		{

			if (vDist2DSq(GuardLocation, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam())) > sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
			{
				dtStatus SearchResult = FindPathClosestToPoint(NavProfile, GuardLocation, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam()), path, 500.0f);

				if (dtStatusSucceed(SearchResult) && path.size() > 0)
				{
					Vector FurthestPoint = UTIL_GetFurthestVisiblePointOnPath(GuardLocation + Vector(0.0f, 0.0f, 64.0f), path, true);
					FurthestPoint.z += 64.0f;

					Vector LookDir = UTIL_GetVectorNormal(FurthestPoint - GuardLocation);

					bool bShouldAdd = true;

					for (auto it = pBot->GuardInfo.GuardPoints.begin(); it != pBot->GuardInfo.GuardPoints.end(); it++)
					{
						Vector ThisLookDir = UTIL_GetVectorNormal((*it) - GuardLocation);

						if (UTIL_GetDotProduct(ThisLookDir, LookDir) > 0.8f)
						{
							bShouldAdd = false;
							break;
						}
					}

					if (bShouldAdd)
					{
						pBot->GuardInfo.GuardPoints.push_back(FurthestPoint);
					}
				}
			}
		}
	}

}

bool BotWithBuildTaskExists(AvHTeamNumber Team, AvHAIDeployableStructureType StructureType)
{
	vector<AvHAIPlayer*> AIPlayers = AIMGR_GetAIPlayersOnTeam(Team);

	for (auto it = AIPlayers.begin(); it != AIPlayers.end(); it++)
	{
		AvHAIPlayer* Bot = (*it);

		if (!IsPlayerActiveInGame(Bot->Edict)) { continue; }

		if ((Bot->PrimaryBotTask.TaskType == TASK_BUILD && Bot->PrimaryBotTask.StructureType == StructureType) || (Bot->SecondaryBotTask.TaskType == TASK_BUILD && Bot->SecondaryBotTask.StructureType == StructureType))
		{
			return true;
		}
	}

	return false;
}

AvHAIPlayer* GetFirstBotWithBuildTask(AvHTeamNumber Team, AvHAIDeployableStructureType StructureType, edict_t* IgnorePlayer)
{
	vector<AvHAIPlayer*> AIPlayers = AIMGR_GetAIPlayersOnTeam(Team);

	for (auto it = AIPlayers.begin(); it != AIPlayers.end(); it++)
	{
		AvHAIPlayer* Bot = (*it);

		if (!IsPlayerActiveInGame(Bot->Edict) || Bot->Edict == IgnorePlayer) { continue; }

		bool bPrimaryIsBuildTask = (Bot->PrimaryBotTask.TaskType == TASK_BUILD || Bot->PrimaryBotTask.TaskType == TASK_REINFORCE_STRUCTURE);
		bool bSecondaryIsBuildTask = (Bot->SecondaryBotTask.TaskType == TASK_BUILD || Bot->SecondaryBotTask.TaskType == TASK_REINFORCE_STRUCTURE);

		if ((bPrimaryIsBuildTask && Bot->PrimaryBotTask.StructureType == StructureType) || (bSecondaryIsBuildTask && Bot->SecondaryBotTask.StructureType == StructureType))
		{
			return Bot;
		}

	}

	return nullptr;
}

int AITASK_GetNumBotsWithBuildTask(AvHTeamNumber Team, AvHAIDeployableStructureType StructureType, edict_t* IgnorePlayer)
{
	vector<AvHAIPlayer*> AIPlayers = AIMGR_GetAIPlayersOnTeam(Team);
	int Result = 0;

	for (auto it = AIPlayers.begin(); it != AIPlayers.end(); it++)
	{
		AvHAIPlayer* Bot = (*it);

		if (!IsPlayerActiveInGame(Bot->Edict) || Bot->Edict == IgnorePlayer) { continue; }

		bool bPrimaryIsBuildTask = (Bot->PrimaryBotTask.TaskType == TASK_BUILD || Bot->PrimaryBotTask.TaskType == TASK_REINFORCE_STRUCTURE);
		bool bSecondaryIsBuildTask = (Bot->SecondaryBotTask.TaskType == TASK_BUILD || Bot->SecondaryBotTask.TaskType == TASK_REINFORCE_STRUCTURE);

		if ((bPrimaryIsBuildTask && Bot->PrimaryBotTask.StructureType == StructureType) || (bSecondaryIsBuildTask && Bot->SecondaryBotTask.StructureType == StructureType))
		{
			Result++;
		}

	}

	return Result;
}

AvHAIPlayer* GetFirstBotWithReinforceTask(AvHTeamNumber Team, edict_t* ReinforceStructure, edict_t* IgnorePlayer)
{
	vector<AvHAIPlayer*> AIPlayers = AIMGR_GetAIPlayersOnTeam(Team);

	for (auto it = AIPlayers.begin(); it != AIPlayers.end(); it++)
	{
		AvHAIPlayer* Bot = (*it);

		if (!IsPlayerActiveInGame(Bot->Edict) || Bot->Edict == IgnorePlayer) { continue; }

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
	if (FNullEnt(Target) || (Target->v.effects & EF_NODRAW) || !UTIL_IsDroppedItemStillReachable(pBot, Target))
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

	if (!ItemToPickup)
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

void AITASK_SetGetHealthTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* HealingSource, const bool bIsUrgent)
{
	if (Task->TaskType == TASK_GET_HEALTH && Task->TaskTarget == HealingSource)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	if (FNullEnt(HealingSource)) { return; }

	Vector HealLocation = ZERO_VECTOR;
	AvHAIHiveDefinition* HiveRef = AITAC_GetHiveFromEdict(HealingSource);

	if (HiveRef)
	{
		HealLocation = HiveRef->FloorLocation;
	}
	else
	{
		if (IsEdictPlayer(HealingSource))
		{
			HealLocation = HealingSource->v.origin;
		}
		else
		{
			HealLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, HealingSource->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
		}
	}

	if (!vIsZero(HealLocation))
	{
		Task->TaskType = TASK_GET_HEALTH;
		Task->TaskTarget = HealingSource;
		Task->TaskLocation = HealLocation;
		Task->bTaskIsUrgent = bIsUrgent;
	}

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
		AvHAIDroppedItem* NearestWelder = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_WELDER, pBot->Player->GetTeam(), pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, 0.0f, true);

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
	if (!Target || FNullEnt(Target) || (Target->v.deadflag != DEAD_NO) || Target->v.team == pBot->Edict->v.team)
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

	UpdateBotMoveProfile(pBot, MOVESTYLE_NORMAL);

	Vector MoveStart = AdjustPointForPathfinding(pBot->CurrentFloorPosition);
	Vector MoveEnd = AdjustPointForPathfinding(Location);

	// Get as close as possible to desired location
	Vector MoveLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, MoveStart, MoveEnd, UTIL_MetresToGoldSrcUnits(1.0f));

	if (vIsZero(MoveLocation))
	{
		Vector ReverseMove = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, MoveEnd, MoveStart, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(ReverseMove))
		{
			MoveLocation = MoveEnd;
		}
	}

	if (!vIsZero(MoveLocation))
	{
		Task->TaskType = TASK_MOVE;
		Task->TaskLocation = MoveLocation;
		Task->bTaskIsUrgent = bIsUrgent;
		Task->TaskLength = 120.0f; // Set a maximum time to reach destination. Helps avoid bots getting permanently stuck
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

	if (Task->TaskType == TASK_BUILD && Task->TaskTarget == StructureToBuild) 
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return; 
	}

	// Get as close as possible to desired location
	Vector BuildLocation = UTIL_ProjectPointToNavmesh(StructureToBuild->v.origin);

	Task->TaskType = TASK_BUILD;
	Task->TaskTarget = StructureToBuild;
	Task->TaskLocation = (!vIsZero(BuildLocation)) ? BuildLocation : UTIL_GetFloorUnderEntity(StructureToBuild);
	Task->bTaskIsUrgent = bIsUrgent;
	Task->StructureType = GetStructureTypeFromEdict(StructureToBuild);
}

void AITASK_SetCapResNodeTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const AvHAIResourceNode* NodeRef, const bool bIsUrgent)
{
	if (!NodeRef) { return; }

	AvHAIDeployableStructureType NodeStructureType = (IsPlayerMarine(pBot->Edict)) ? STRUCTURE_MARINE_RESTOWER : STRUCTURE_ALIEN_RESTOWER;

	if (Task->TaskType == TASK_CAP_RESNODE && Task->TaskTarget == NodeRef->ResourceEdict)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

	Vector WaitLocation = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, NodeRef->Location, UTIL_MetresToGoldSrcUnits(1.0f));

	Task->TaskType = TASK_CAP_RESNODE;
	Task->StructureType = NodeStructureType;
	Task->TaskLocation = (!vIsZero(WaitLocation)) ? WaitLocation : NodeRef->Location;
	Task->TaskTarget = NodeRef->ResourceEdict;

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

	if (FNullEnt(Target) || Target->v.deadflag != DEAD_NO) { return; }

	Task->TaskType = TASK_REINFORCE_STRUCTURE;
	Task->TaskTarget = Target;
	Task->StructureType = GetStructureTypeFromEdict(Target);
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
	Task->TaskLocation = UTIL_GetNextMinePosition2(Target);
	Task->StructureType = STRUCTURE_MARINE_DEPLOYEDMINE;


}