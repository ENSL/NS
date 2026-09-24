
#include "AvHAICommander.h"
#include "AvHAITactical.h"
#include "AvHAIMath.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIWeaponHelper.h"
#include "AvHAINavigation.h"
#include "AvHAITask.h"
#include "AvHAIHelper.h"
#include "AvHAIPlayerManager.h"
#include "AvHAIConfig.h"
#include "AvHAIMarineBuildOrderManager.h"

#include "AvHSharedUtil.h"
#include "AvHServerUtil.h"

AvHAIBuildableStructure* AICOMM_DeployStructure(AvHAIPlayer* pBot, const AvHAIDeployableStructureType StructureToDeploy, const Vector Location, StructurePurpose Purpose, bool bPlacedByHuman)
{
	if (vIsZero(Location)) { return nullptr; }

	nav_profile WelderProfile = GetBaseNavProfile(MARINE_BASE_NAV_PROFILE);
	WelderProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);

	// Don't allow the commander to place a structure somewhere unreachable to marines
	if (!UTIL_PointIsReachable(WelderProfile, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam()), Location, max_player_use_reach)) 
	{
		return false;
	}

	AvHMessageID StructureID = UTIL_StructureTypeToImpulseCommand(StructureToDeploy);

	Vector BuildLocation = Location;
	BuildLocation.z += 4.0f;

	// This would be rejected if a human was trying to build here, so don't let the bot do it
	if (!AvHSHUGetIsSiteValidForBuild(StructureID, &BuildLocation))
	{
		return nullptr; 
	}

	string theErrorMessage;
	int theCost = 0;
	bool thePurchaseAllowed = pBot->Player->GetPurchaseAllowed(StructureID, theCost, &theErrorMessage);

	if (!thePurchaseAllowed) { return nullptr; }

	CBaseEntity* NewStructureEntity = AvHSUBuildTechForPlayer(StructureID, BuildLocation, pBot->Player);

	if (!NewStructureEntity) { return nullptr; }

	AvHAIBuildableStructure* NewStructure = AITAC_UpdateBuildableStructure(NewStructureEntity);

	if (NewStructure)
	{
		NewStructure->Purpose = Purpose;
		NewStructure->bPlacedByHuman = bPlacedByHuman;
	}

	pBot->Player->PayPurchaseCost(theCost);

	pBot->next_commander_action_time = gpGlobals->time + 1.0f;

	return NewStructure;
}

bool AICOMM_DeployItem(AvHAIPlayer* pBot, const AvHAIDeployableItemType ItemToDeploy, const Vector Location)
{
	AvHMessageID StructureID =  UTIL_ItemTypeToImpulseCommand(ItemToDeploy);

	Vector BuildLocation = Location;

	string theErrorMessage;
	int theCost = 0;
	bool thePurchaseAllowed = pBot->Player->GetPurchaseAllowed(StructureID, theCost, &theErrorMessage);

	if (!thePurchaseAllowed) { return false; }

	if (!AvHSHUGetIsSiteValidForBuild(StructureID, &BuildLocation)) { return false; }

	CBaseEntity* NewItem = AvHSUBuildTechForPlayer(StructureID, BuildLocation, pBot->Player);

	if (!NewItem) { return false; }

	AITAC_UpdateMarineItem(NewItem, ItemToDeploy);

	pBot->Player->PayPurchaseCost(theCost);

	pBot->next_commander_action_time = gpGlobals->time + 0.2f;

	return true;
}

bool AICOMM_ResearchTech(AvHAIPlayer* pBot, AvHAIBuildableStructure* StructureToResearch, AvHMessageID Research)
{
	if (!StructureToResearch || FNullEnt(StructureToResearch->edict) || !StructureToResearch->EntityRef) { return false; }

	// Don't do anything if the structure is being recycled, or we DON'T want to recycle but the structure is already busy
	if (StructureToResearch->EntityRef->GetIsRecycling() || (Research != BUILD_RECYCLE && StructureToResearch->EntityRef->GetIsResearching())) { return false; }

	int StructureIndex = ENTINDEX(StructureToResearch->edict);

	if (StructureIndex < 0) { return false; }

	if (!StructureToResearch->EntityRef->GetIsTechnologyAvailable(Research)) { return false; }

	AvHTeam* CommanderTeamRef = AIMGR_GetTeamRef(pBot->Player->GetTeam());

	if (!CommanderTeamRef) { return false; }

	AvHResearchManager& theResearchManager = CommanderTeamRef->GetResearchManager();

	bool theIsResearchable = false;
	int theResearchCost = 0.0f;
	float theResearchTime = 0.0f;

	theResearchManager.GetResearchInfo(Research, theIsResearchable, theResearchCost, theResearchTime);

	if (pBot->Player->GetResources() < theResearchCost) { return false; }

	pBot->Player->SetSelection(StructureIndex, true);

	pBot->Button |= IN_ATTACK2;
	pBot->Impulse = Research;

	pBot->next_commander_action_time = gpGlobals->time + 1.0f;

	return true;
}

bool AICOMM_UpgradeStructure(AvHAIPlayer* pBot, AvHAIBuildableStructure* StructureToUpgrade)
{
	AvHMessageID UpgradeImpulse = MESSAGE_NULL;

	switch (StructureToUpgrade->StructureType)
	{
	case STRUCTURE_MARINE_ARMOURY:
		UpgradeImpulse = ARMORY_UPGRADE;
		break;
	case STRUCTURE_MARINE_TURRETFACTORY:
		UpgradeImpulse = TURRET_FACTORY_UPGRADE;
		break;
	default:
		return false;
	}

	return AICOMM_ResearchTech(pBot, StructureToUpgrade, UpgradeImpulse);
}

bool AICOMM_RecycleStructure(AvHAIPlayer* pBot, AvHAIBuildableStructure* StructureToRecycle)
{
	if (!StructureToRecycle || StructureToRecycle->StructureType == STRUCTURE_MARINE_DEPLOYEDMINE || UTIL_StructureIsRecycling(StructureToRecycle->edict)) { return false; }

	return AICOMM_ResearchTech(pBot, StructureToRecycle, BUILD_RECYCLE);
}

bool AICOMM_IssueMovementOrder(AvHAIPlayer* pBot, edict_t* Recipient, const Vector MoveLocation)
{
	if (FNullEnt(Recipient) || !IsPlayerActiveInGame(Recipient) || vIsZero(MoveLocation)) { return false; }

	int ReceiverIndex = ENTINDEX(Recipient);

	if (ReceiverIndex <= 0) { return false; }

	AvHOrder NewOrder;

	NewOrder.SetOrderType(ORDERTYPEL_MOVE);
	NewOrder.SetReceiver(ReceiverIndex);
	NewOrder.SetLocation(MoveLocation);
	NewOrder.SetOrderID();

	pBot->Player->SetSelection(ReceiverIndex, true);

	pBot->Player->GiveOrderToSelection(NewOrder);

	return true;
}

bool AICOMM_IssueBuildOrder(AvHAIPlayer* pBot, edict_t* Recipient, edict_t* TargetStructure)
{
	if (FNullEnt(Recipient) || !IsPlayerActiveInGame(Recipient) || FNullEnt(TargetStructure) || UTIL_StructureIsFullyBuilt(TargetStructure)) { return false; }

	int ReceiverIndex = ENTINDEX(Recipient);
	int TargetIndex = ENTINDEX(TargetStructure);

	if (ReceiverIndex <= 0 || TargetIndex <= 0) { return false; }

	AvHBaseBuildable* BuildingRef = dynamic_cast<AvHBaseBuildable*>(CBaseEntity::Instance(TargetStructure));

	if (!BuildingRef) { return false; }

	AvHOrder NewOrder;

	NewOrder.SetOrderType(ORDERTYPET_BUILD);
	NewOrder.SetReceiver(ReceiverIndex);
	NewOrder.SetTargetIndex(TargetIndex);
	NewOrder.SetUser3TargetType((AvHUser3)TargetStructure->v.iuser3);
	NewOrder.SetOrderTargetType(ORDERTARGETTYPE_LOCATION);
	NewOrder.SetLocation(TargetStructure->v.origin);
	NewOrder.SetOrderID();

	pBot->Player->SetSelection(ReceiverIndex, true);

	pBot->Player->GiveOrderToSelection(NewOrder);

	return true;
}

void AICOMM_AssignNewPlayerOrder(AvHAIPlayer* pBot, edict_t* Assignee, edict_t* TargetEntity, AvHAIOrderPurpose OrderPurpose)
{
	if (FNullEnt(Assignee) || FNullEnt(TargetEntity) || OrderPurpose == ORDERPURPOSE_NONE) { return; }

	// Clear any existing order we have for this player
	for (auto it = pBot->ActiveOrders.begin(); it != pBot->ActiveOrders.end();)
	{
		if (it->Assignee == Assignee)
		{
			it = pBot->ActiveOrders.erase(it);
		}
		else
		{
			it++;
		}
	}

	ai_commander_order NewOrder;
	NewOrder.Assignee = Assignee;
	NewOrder.OrderTarget = TargetEntity;
	NewOrder.OrderLocation = TargetEntity->v.origin;
	NewOrder.OrderPurpose = OrderPurpose;
	NewOrder.LastReminderTime = 0.0f;
	NewOrder.LastPlayerDistance = 0.0f;

	pBot->ActiveOrders.push_back(NewOrder);

	if (AICOMM_DoesPlayerOrderNeedReminder(pBot, &NewOrder))
	{
		AICOMM_IssueOrderForAssignedJob(pBot, &NewOrder);
	}
}

void AICOMM_AssignNewPlayerOrder(AvHAIPlayer* pBot, edict_t* Assignee, Vector OrderLocation, AvHAIOrderPurpose OrderPurpose)
{
	if (FNullEnt(Assignee) || vIsZero(OrderLocation) || OrderPurpose == ORDERPURPOSE_NONE) { return; }

	// Clear any existing order we have for this player
	for (auto it = pBot->ActiveOrders.begin(); it != pBot->ActiveOrders.end();)
	{
		if (it->Assignee == Assignee)
		{
			it = pBot->ActiveOrders.erase(it);
		}
		else
		{
			it++;
		}
	}

	ai_commander_order NewOrder;
	NewOrder.Assignee = Assignee;
	NewOrder.OrderLocation = OrderLocation;
	NewOrder.OrderPurpose = OrderPurpose;
	NewOrder.LastReminderTime = 0.0f;
	NewOrder.LastPlayerDistance = 0.0f;

	pBot->ActiveOrders.push_back(NewOrder);

	if (AICOMM_DoesPlayerOrderNeedReminder(pBot, &NewOrder))
	{
		AICOMM_IssueOrderForAssignedJob(pBot, &NewOrder);
	}
}

void AICOMM_IssueOrderForAssignedJob(AvHAIPlayer* pBot, ai_commander_order* Order)
{
	if (Order->OrderPurpose == ORDERPURPOSE_BUILD_GUARDPOST || Order->OrderPurpose == ORDERPURPOSE_BUILD_SIEGE || Order->OrderPurpose == ORDERPURPOSE_BUILD_OUTPOST || Order->OrderPurpose == ORDERPURPOSE_BUILD_MAINBASE)
	{
		Vector MoveLoc = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), Order->OrderLocation, UTIL_MetresToGoldSrcUnits(1.0f));

		AICOMM_IssueMovementOrder(pBot, Order->Assignee, MoveLoc);
		Order->LastReminderTime = gpGlobals->time;
		Order->LastPlayerDistance = vDist2DSq(Order->Assignee->v.origin, MoveLoc);
		Order->OrderLocation = MoveLoc;
		return;
	}

	if (Order->OrderPurpose == ORDERPURPOSE_SIEGE_HIVE || Order->OrderPurpose == ORDERPURPOSE_SECURE_HIVE)
	{
		bool bIsSiegeHiveOrder = (Order->OrderPurpose == ORDERPURPOSE_SIEGE_HIVE);
		const AvHAIHiveDefinition* Hive = AITAC_GetHiveFromEdict(Order->OrderTarget);

		if (Hive)
		{
			Vector OrderLocation = Hive->FloorLocation;

			DeployableSearchFilter StructureFilter;
			StructureFilter.DeployableTeam = pBot->Player->GetTeam();
			StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY;
			StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			StructureFilter.MaxSearchRadius = (bIsSiegeHiveOrder) ? UTIL_MetresToGoldSrcUnits(25.0f) : UTIL_MetresToGoldSrcUnits(10.0f);

			AvHAIBuildableStructure NearestToHive = AITAC_FindClosestDeployableToLocation(Hive->Location, &StructureFilter);

			if (NearestToHive.IsValid())
			{
				if (!(NearestToHive.StructureStatusFlags & STRUCTURE_STATUS_COMPLETED))
				{
					AICOMM_IssueBuildOrder(pBot, Order->Assignee, NearestToHive.edict);
					Order->LastReminderTime = gpGlobals->time;
					Order->LastPlayerDistance = vDist2DSq(Order->Assignee->v.origin, NearestToHive.Location);
					Order->OrderLocation = NearestToHive.Location;
					return;
				}
				else
				{
					Vector MoveLoc = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestToHive.Location, UTIL_MetresToGoldSrcUnits(3.0f));

					AICOMM_IssueMovementOrder(pBot, Order->Assignee, MoveLoc);
					Order->LastReminderTime = gpGlobals->time;
					Order->LastPlayerDistance = vDist2DSq(Order->Assignee->v.origin, MoveLoc);
					Order->OrderLocation = MoveLoc;
					return;
				}
			}
			else
			{
				Vector MoveLoc = (bIsSiegeHiveOrder) ? UTIL_GetRandomPointOnNavmeshInDonutIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f), UTIL_MetresToGoldSrcUnits(20.0f)) : Hive->FloorLocation;

				AICOMM_IssueMovementOrder(pBot, Order->Assignee, MoveLoc);
				Order->LastReminderTime = gpGlobals->time;
				Order->LastPlayerDistance = vDist2DSq(Order->Assignee->v.origin, MoveLoc);
				Order->OrderLocation = MoveLoc;
				return;
			}
		}

		Order->LastReminderTime = gpGlobals->time;
		return;
	}

	if (Order->OrderPurpose == ORDERPURPOSE_SECURE_RESNODE)
	{
		const AvHAIResourceNode* ResNode = AITAC_GetResourceNodeFromEdict(Order->OrderTarget);

		if (ResNode)
		{
			if (ResNode->OwningTeam == pBot->Player->GetTeam() && ResNode->ActiveTowerEntity && !UTIL_StructureIsFullyBuilt(ResNode->ActiveTowerEntity))
			{
				AICOMM_IssueBuildOrder(pBot, Order->Assignee, ResNode->ActiveTowerEntity);
				Order->LastReminderTime = gpGlobals->time;
				Order->LastPlayerDistance = vDist2DSq(Order->Assignee->v.origin, ResNode->Location);
				Order->OrderLocation = ResNode->Location;
				return;
			}

			Vector MoveLoc = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), ResNode->Location, UTIL_MetresToGoldSrcUnits(3.0f));
			
			AICOMM_IssueMovementOrder(pBot, Order->Assignee, MoveLoc);
			Order->LastReminderTime = gpGlobals->time;
			Order->LastPlayerDistance = vDist2DSq(Order->Assignee->v.origin, MoveLoc);
			Order->OrderLocation = MoveLoc;
			return;

		}

		return;
	}
}

int AICOMM_GetNumPlayersAssignedToOrderLocation(AvHAIPlayer* pBot, Vector OrderLocation, AvHAIOrderPurpose OrderPurpose)
{
	int Result = 0;

	for (auto it = pBot->ActiveOrders.begin(); it != pBot->ActiveOrders.end(); it++)
	{
		if ((OrderPurpose == ORDERPURPOSE_NONE || it->OrderPurpose == OrderPurpose) && vDist2DSq(it->OrderLocation, OrderLocation) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			Result++;
		}
	}

	return Result;
}

int AICOMM_GetNumPlayersAssignedToOrderType(AvHAIPlayer* pBot, AvHAIOrderPurpose OrderPurpose)
{
	int Result = 0;

	for (auto it = pBot->ActiveOrders.begin(); it != pBot->ActiveOrders.end(); it++)
	{
		if (it->OrderPurpose == OrderPurpose)
		{
			Result++;
		}
	}

	return Result;
}

int AICOMM_GetNumPlayersAssignedToOrder(AvHAIPlayer* pBot, edict_t* TargetEntity, AvHAIOrderPurpose OrderPurpose)
{
	int Result = 0;

	for (auto it = pBot->ActiveOrders.begin(); it != pBot->ActiveOrders.end(); it++)
	{
		if (it->OrderTarget == TargetEntity && it->OrderPurpose == OrderPurpose)
		{
			Result++;
		}
	}

	return Result;
}

AvHAIMarineBase* AICOMM_GetNearestBaseToLocation(AvHAIPlayer* pBot, Vector SearchLocation)
{
	AvHAIMarineBase* Result = nullptr;

	float MinDist = 0.0f;

	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
	{
		AvHAIMarineBase* ThisBase = &(*it);

		float ThisDist = vDist2DSq(ThisBase->BaseLocation, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = ThisBase;
			MinDist = ThisDist;
		}

	}

	return Result;
}

bool AICOMM_IsOrderStillValid(AvHAIPlayer* pBot, ai_commander_order* Order)
{
	if (FNullEnt(Order->Assignee) || (FNullEnt(Order->OrderTarget) && vIsZero(Order->OrderLocation)) || !IsPlayerActiveInGame(Order->Assignee) || Order->OrderPurpose == ORDERPURPOSE_NONE) { return false; }

	switch (Order->OrderPurpose)
	{
		case ORDERPURPOSE_SECURE_HIVE:
		{
			const AvHAIHiveDefinition* Hive = AITAC_GetHiveFromEdict(Order->OrderTarget);

			if (!Hive || Hive->Status != HIVE_STATUS_UNBUILT) { return false; }

			return !AICOMM_IsHiveFullySecured(pBot, Hive, false);
		}
		break;
		case ORDERPURPOSE_SIEGE_HIVE:
		{
			const AvHAIHiveDefinition* Hive = AITAC_GetHiveFromEdict(Order->OrderTarget);

			// Hive has been destroyed, no longer needs sieging
			return (Hive && Hive->Status != HIVE_STATUS_UNBUILT);

		}
		break;
		case ORDERPURPOSE_SECURE_RESNODE:
		{
			if (!AICOMM_ShouldCommanderPrioritiseNodes(pBot)) { return false; }

			const AvHAIResourceNode* ResNode = AITAC_GetResourceNodeFromEdict(Order->OrderTarget);

			if (!ResNode) { return false; }

			return (ResNode->OwningTeam != pBot->Player->GetTeam() || !ResNode->ActiveTowerEntity || !UTIL_StructureIsFullyBuilt(ResNode->ActiveTowerEntity));

		}
		break;
		case ORDERPURPOSE_BUILD_SIEGE:
		case ORDERPURPOSE_BUILD_OUTPOST:
		case ORDERPURPOSE_BUILD_GUARDPOST:
		case ORDERPURPOSE_BUILD_MAINBASE:
		{
			AvHAIMarineBase* NearestBase = AICOMM_GetNearestBaseToLocation(pBot, Order->OrderLocation);

			if (!NearestBase || NearestBase->bRecycleBase || !NearestBase->bIsActive || !NearestBase->bCanBeBuiltOut) { return false; }

			// If this is a main base, it's established (has 2 or more infantry portals) and our active comm chair is based there, then we don't need anyone ordered to this location any more
			if (NearestBase->BaseType == MARINE_BASE_MAINBASE && NearestBase->bBaseInitialised && vDist2DSq(NearestBase->BaseLocation, AITAC_GetCommChairLocation(pBot->Player->GetTeam())) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { return false; }

			if (vDist2DSq(NearestBase->BaseLocation, Order->OrderLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f))) { return false; }

			return true;
		}
		break;
		default:
			return false;
	}

	return false;
}

bool AICOMM_DoesPlayerOrderNeedReminder(AvHAIPlayer* pBot, ai_commander_order* Order)
{
	// For now, disable reminders as it is annoying for humans and pointless for bots who should obey it always anyway
	return (Order->LastReminderTime < 0.1f);

	float NewDist = vDist2DSq(Order->Assignee->v.origin, Order->OrderLocation);
	float OldDist = Order->LastPlayerDistance;
	Order->LastPlayerDistance = NewDist;

	if (gpGlobals->time - Order->LastReminderTime < MIN_COMMANDER_REMIND_TIME) { return false; }

	if (Order->OrderPurpose == ORDERPURPOSE_SECURE_RESNODE)
	{
		if (vDist2DSq(Order->Assignee->v.origin, Order->OrderTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f))) { return false; }
	}

	if (Order->OrderPurpose == ORDERPURPOSE_SIEGE_HIVE)
	{
		if (vDist2DSq(Order->Assignee->v.origin, Order->OrderTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(25.0f))) { return false; }
	}

	if (Order->OrderPurpose == ORDERPURPOSE_SECURE_HIVE)
	{
		if (vDist2DSq(Order->Assignee->v.origin, Order->OrderTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f))) { return false; }
	}	

	return NewDist >= OldDist;
}

bool AICOMM_ShouldCommanderPrioritiseNodes(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	int NumOwnedNodes = 0;
	int NumEligibleNodes = 0;
	int NumFreeNodes = 0;

	

	// First get ours and the enemy's ownership of all eligible nodes (we can reach them, and they're in the enemy base)
	vector<AvHAIResourceNode*> AllNodes = AITAC_GetAllReachableResourceNodes(BotTeam);

	for (auto it = AllNodes.begin(); it != AllNodes.end(); it++)
	{
		AvHAIResourceNode* ThisNode = (*it);

		// We don't care about the node at marine spawn or enemy hives, ignore then in our calculations
		if (ThisNode->OwningTeam == EnemyTeam && ThisNode->bIsBaseNode) { continue; }

		if (ThisNode->OwningTeam == TEAM_IND)
		{
			NumFreeNodes++;
		}

		NumEligibleNodes++;

		if (ThisNode->OwningTeam == BotTeam) { NumOwnedNodes++; }
	}

	int NumDesiredNodes = imini(4, (int)ceilf((float)NumEligibleNodes * 0.5f));

	int NumNodesLeft = NumEligibleNodes - NumOwnedNodes;

	if (NumNodesLeft == 0) { return false; }

	return NumOwnedNodes < NumDesiredNodes || NumFreeNodes > 1;

}

void AICOMM_UpdatePlayerOrders(AvHAIPlayer* pBot)
{
	// Clear out any orders which aren't relevant any more
	for (auto it = pBot->ActiveOrders.begin(); it != pBot->ActiveOrders.end();)
	{
		if (!AICOMM_IsOrderStillValid(pBot, &(*it)))
		{
			it = pBot->ActiveOrders.erase(it);
		}
		else
		{
			// If the person we're ordering around isn't doing as they're told, then issue them a reminder
			if (AICOMM_DoesPlayerOrderNeedReminder(pBot, &(*it)))
			{
				AICOMM_IssueOrderForAssignedJob(pBot, &(*it));
			}

			it++;
		}
	}

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);
	
	int NumPlayersOnTeam = AITAC_GetNumActivePlayersOnTeam(pBot->Player->GetTeam());

	// Do the Shotgun rush here
	if (AIBO_BuildOrderIsShotgunRush() && AIMGR_GetMatchLength() < 180)
	{
		DeployableSearchFilter ArmouryFilter;
		ArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
		ArmouryFilter.DeployableTeam = BotTeam;
		ArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(BotTeam), &ArmouryFilter);
		Vector CommChairLocation = AITAC_GetCommChairLocation(BotTeam);	
		const AvHAIHiveDefinition* HiveToAttack = AITAC_GetActiveHiveNearestLocation(EnemyTeam, CommChairLocation);

		// Order all players to stay in base until 80% of them have a shotgun or some time has passed.
		int NumPlayers = AIMGR_GetNumPlayersOnTeam(BotTeam);
		int NumShotguns = AITAC_GetNumWeaponsInPlay(BotTeam, WEAPON_MARINE_SHOTGUN);

		if (NumShotguns < 0.8 * NumPlayers && AIMGR_GetMatchLength() < 30)
		{
			Vector MoveLocation = (NearestArmoury.IsValid()) ? NearestArmoury.Location : CommChairLocation;
			edict_t* GuyToBuildBase = AICOMM_GetPlayerWithoutSpecificOrderNearestLocation(pBot, HiveToAttack->Location, ORDERPURPOSE_BUILD_MAINBASE);
			AICOMM_AssignNewPlayerOrder(pBot, GuyToBuildBase, MoveLocation, ORDERPURPOSE_BUILD_MAINBASE);
			return;
		}
		// Send all players to attack the hive
		if (HiveToAttack)
		{
			edict_t* HiveAttacker = AICOMM_GetPlayerWithoutSpecificOrderNearestLocation(pBot, CommChairLocation, ORDERPURPOSE_SIEGE_HIVE);
			if (HiveAttacker && !FNullEnt(HiveAttacker))
			{
				// AICOMM_AssignNewPlayerOrder(pBot, HiveAttacker, HiveToAttack->Location, ORDERPURPOSE_SIEGE_HIVE); <~~ why does this crash?
				AICOMM_IssueSecureHiveOrder(pBot, HiveAttacker, HiveToAttack);
			}
		}
		return;
	}

	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
	{
		AvHAIMarineBase* ThisBase = &(*it);

		if (ThisBase->BaseType == MARINE_BASE_MAINBASE && !ThisBase->bIsBaseEstablished)
		{
			bool bCommChairInBase = vDist2DSq(ThisBase->BaseLocation, AITAC_GetCommChairLocation(BotTeam)) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f));

			if (bCommChairInBase && ThisBase->bIsBaseEstablished) { continue; }

			// Either the base isn't established (has comm chair and infantry portals), or the active comm chair isn't in the base (we're relocating)
			int NumDesiredBuilders = ThisBase->bIsBaseEstablished ? 1 : 2;

			NumDesiredBuilders = imini(NumDesiredBuilders, NumPlayersOnTeam);

			int NumBuilders = ThisBase->NumBuilders + AICOMM_GetNumPlayersAssignedToOrderLocation(pBot, ThisBase->BaseLocation, ORDERPURPOSE_BUILD_MAINBASE);

			if (NumBuilders < NumDesiredBuilders)
			{
				edict_t* NewBuilder = AICOMM_GetPlayerWithoutSpecificOrderNearestLocation(pBot, ThisBase->BaseLocation, ORDERPURPOSE_BUILD_MAINBASE);

				if (!FNullEnt(NewBuilder))
				{
					AICOMM_AssignNewPlayerOrder(pBot, NewBuilder, ThisBase->BaseLocation, ORDERPURPOSE_BUILD_MAINBASE);
					return;
				}
			}
			else
			{
				continue;
			}
		}
	}

	int MinResGatherers = imini(3, (int)ceilf(NumPlayersOnTeam * 0.35f));

	if (AICOMM_ShouldCommanderPrioritiseNodes(pBot) && AICOMM_GetNumPlayersAssignedToOrderType(pBot, ORDERPURPOSE_SECURE_RESNODE) < MinResGatherers)
	{
		DeployableSearchFilter ResNodeFilter;
		ResNodeFilter.ReachabilityTeam = pBot->Player->GetTeam();
		ResNodeFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;

		vector<AvHAIResourceNode*> EligibleResNodes = AITAC_GetAllMatchingResourceNodes(ZERO_VECTOR, &ResNodeFilter);

		AvHAIResourceNode* NearestNode = nullptr;
		float MinDist = 0.0f;

		Vector TeamStartingLocation = AITAC_GetCommChairLocation(BotTeam);

		int NumResNodesOrdered = 0;

		for (auto it = EligibleResNodes.begin(); it != EligibleResNodes.end(); it++)
		{
			AvHAIResourceNode* ThisNode = (*it);

			if (!ThisNode || ThisNode->OwningTeam == BotTeam) { continue; }

			int NumAssignedPlayers = AICOMM_GetNumPlayersAssignedToOrder(pBot, ThisNode->ResourceEntity->edict(), ORDERPURPOSE_SECURE_RESNODE);

			if (NumAssignedPlayers > 0) { continue; }

			float ThisDist = vDist2DSq(TeamStartingLocation, ThisNode->Location);

			if (ThisNode->OwningTeam == EnemyTeam)
			{
				ThisDist *= 2.0f;
			}

			if (!NearestNode || ThisDist < MinDist)
			{
				NearestNode = ThisNode;
				MinDist = ThisDist;
			}
		}

		if (NearestNode)
		{
			edict_t* NewAssignee = AICOMM_GetPlayerWithNoOrderNearestLocation(pBot, NearestNode->Location);

			if (!FNullEnt(NewAssignee))
			{
				AICOMM_AssignNewPlayerOrder(pBot, NewAssignee, NearestNode->ResourceEntity->edict(), ORDERPURPOSE_SECURE_RESNODE);
				return;
			}
		}
	}

	AvHAIMarineBase* SiegeBase = nullptr;
	AvHAIMarineBase* Outpost = nullptr;
	AvHAIMarineBase* Guardpost = nullptr;

	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
	{
		AvHAIMarineBase* ThisBase = &(*it);

		if (ThisBase->bRecycleBase || !ThisBase->bIsActive || !ThisBase->bCanBeBuiltOut) { continue; }

		if (ThisBase->BaseType == MARINE_BASE_MAINBASE) { continue; }

		int NumDesiredBuilders = 0;

		AvHAIOrderPurpose NewOrderPurpose = ORDERPURPOSE_NONE;
		
		switch (ThisBase->BaseType)
		{
			case MARINE_BASE_SIEGE:
				NumDesiredBuilders = imini(3, (int)ceilf((float)NumPlayersOnTeam * 0.5f));
				NewOrderPurpose = ORDERPURPOSE_BUILD_SIEGE;
				break;
			case MARINE_BASE_OUTPOST:
				NumDesiredBuilders = imini(2, (int)ceilf((float)NumPlayersOnTeam * 0.25f));
				NewOrderPurpose = ORDERPURPOSE_BUILD_OUTPOST;
				break;
			case MARINE_BASE_GUARDPOST:
				NumDesiredBuilders = 1;
				NewOrderPurpose = ORDERPURPOSE_BUILD_GUARDPOST;
				break;
			default:
				continue;
		}
		
		int NumPlayersThere = ThisBase->NumBuilders;

		int Deficit = NumDesiredBuilders - NumPlayersThere;

		if (Deficit > 0)
		{
			int NumOrders = AICOMM_GetNumPlayersAssignedToOrderLocation(pBot, ThisBase->BaseLocation, NewOrderPurpose);

			Deficit -= NumOrders;

			if (Deficit > 0)
			{
				if (ThisBase->BaseType == MARINE_BASE_SIEGE)
				{
					SiegeBase = ThisBase;
				}
				else if (ThisBase->BaseType == MARINE_BASE_OUTPOST)
				{
					Outpost = ThisBase;
				}
				else if (ThisBase->BaseType == MARINE_BASE_GUARDPOST)
				{
					Guardpost = ThisBase;
				}
			}

		}		
	}

	if (SiegeBase)
	{
		edict_t* NearestEligiblePlayer = AICOMM_GetPlayerWithNoOrderNearestLocation(pBot, SiegeBase->BaseLocation);

		if (!FNullEnt(NearestEligiblePlayer))
		{
			AICOMM_AssignNewPlayerOrder(pBot, NearestEligiblePlayer, SiegeBase->BaseLocation, ORDERPURPOSE_BUILD_SIEGE);
			return;
		}
	}

	if (Outpost)
	{
		edict_t* NearestEligiblePlayer = AICOMM_GetPlayerWithNoOrderNearestLocation(pBot, Outpost->BaseLocation);

		if (!FNullEnt(NearestEligiblePlayer))
		{
			AICOMM_AssignNewPlayerOrder(pBot, NearestEligiblePlayer, Outpost->BaseLocation, ORDERPURPOSE_BUILD_OUTPOST);
			return;
		}
	}

	if (Guardpost)
	{
		edict_t* NearestEligiblePlayer = AICOMM_GetPlayerWithNoOrderNearestLocation(pBot, Guardpost->BaseLocation);

		if (!FNullEnt(NearestEligiblePlayer))
		{
			AICOMM_AssignNewPlayerOrder(pBot, NearestEligiblePlayer, Guardpost->BaseLocation, ORDERPURPOSE_BUILD_GUARDPOST);
			return;
		}
	}

}

edict_t* AICOMM_GetPlayerWithoutSpecificOrderNearestLocation(AvHAIPlayer* pBot, Vector SearchLocation, AvHAIOrderPurpose OrderPurpose)
{
	edict_t* Result = nullptr;
	float MinDist = 0.0f;

	vector<AvHPlayer*> PlayerList = AIMGR_GetAllPlayersOnTeam(pBot->Player->GetTeam());

	// First, remove all players who are dead or otherwise not active with boots on the ground (e.g. commander, or being digested)
	for (auto it = PlayerList.begin(); it != PlayerList.end();)
	{
		AvHPlayer* PlayerRef = (*it);
		AvHAIPlayer* AIPlayerRef = AIMGR_GetBotRefFromPlayer(PlayerRef);

		// Don't give orders to incapacitated players, or if the bot is currently playing a defensive role. Stops the commander sending everyone out
		// and leaving nobody at base
		if (!IsPlayerActiveInGame(PlayerRef->edict()) || (AIPlayerRef && AIPlayerRef->BotRole == BOT_ROLE_SWEEPER))
		{
			it = PlayerList.erase(it);
		}
		else
		{
			it++;
		}
	}

	// Next, erase all players with orders so we only have a list of players without orders assigned to them
	for (auto it = pBot->ActiveOrders.begin(); it != pBot->ActiveOrders.end(); it++)
	{
		if (it->OrderPurpose != OrderPurpose) { continue; }

		AvHPlayer* ThisPlayer = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(it->Assignee));

		if (!ThisPlayer) { continue; }

		std::vector<AvHPlayer*>::iterator FoundPlayer = std::find(PlayerList.begin(), PlayerList.end(), ThisPlayer);

		if (FoundPlayer != PlayerList.end())
		{
			PlayerList.erase(FoundPlayer);
		}
	}

	// Now rank them by distance and return the result
	for (auto it = PlayerList.begin(); it != PlayerList.end(); it++)
	{
		edict_t* PlayerEdict = (*it)->edict();

		float ThisDist = vDist2DSq(PlayerEdict->v.origin, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = PlayerEdict;
			MinDist = ThisDist;
		}
	}

	return Result;
}

edict_t* AICOMM_GetPlayerWithNoOrderNearestLocation(AvHAIPlayer* pBot, Vector SearchLocation)
{
	edict_t* Result = nullptr;
	float MinDist = 0.0f;

	vector<AvHPlayer*> PlayerList = AIMGR_GetAllPlayersOnTeam(pBot->Player->GetTeam());

	// First, remove all players who are dead or otherwise not active with boots on the ground (e.g. commander, or being digested)
	for (auto it = PlayerList.begin(); it != PlayerList.end();)
	{
		AvHPlayer* PlayerRef = (*it);
		AvHAIPlayer* AIPlayerRef = AIMGR_GetBotRefFromPlayer(PlayerRef);

		// Don't give orders to incapacitated players, or if the bot is currently playing a defensive role. Stops the commander sending everyone out
		// and leaving nobody at base
		if (!IsPlayerActiveInGame(PlayerRef->edict()) || (AIPlayerRef && AIPlayerRef->BotRole == BOT_ROLE_SWEEPER))
		{
			it = PlayerList.erase(it);
		}
		else
		{
			it++;
		}
	}

	// Next, erase all players with orders so we only have a list of players without orders assigned to them
	for (auto it = pBot->ActiveOrders.begin(); it != pBot->ActiveOrders.end(); it++)
	{
		AvHPlayer* ThisPlayer = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(it->Assignee));

		if (!ThisPlayer) { continue; }

		std::vector<AvHPlayer*>::iterator FoundPlayer = std::find(PlayerList.begin(), PlayerList.end(), ThisPlayer);

		if (FoundPlayer != PlayerList.end())
		{
			PlayerList.erase(FoundPlayer);
		}
	}

	// Now rank them by distance and return the result
	for (auto it = PlayerList.begin(); it != PlayerList.end(); it++)
	{
		edict_t* PlayerEdict = (*it)->edict();

		float ThisDist = vDist2DSq(PlayerEdict->v.origin, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = PlayerEdict;
			MinDist = ThisDist;
		}
	}

	return Result;

}

bool AICOMM_IssueSecureHiveOrder(AvHAIPlayer* pBot, edict_t* Recipient, const AvHAIHiveDefinition* HiveToSecure)
{
	if (!HiveToSecure || FNullEnt(Recipient) || !IsPlayerActiveInGame(Recipient)) { return false; }

	int ReceiverIndex = ENTINDEX(Recipient);

	if (ReceiverIndex <= 0) { return false; }

	AvHTeamNumber CommanderTeam = pBot->Player->GetTeam();

	bool bPhaseGatesAvailable = AITAC_PhaseGatesAvailable(CommanderTeam);

	if (bPhaseGatesAvailable)
	{
		DeployableSearchFilter PGFilter;
		PGFilter.DeployableTeam = CommanderTeam;
		PGFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
		PGFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		PGFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		PGFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		bool bPhaseExists = AITAC_DeployableExistsAtLocation(HiveToSecure->FloorLocation, &PGFilter);

		if (!bPhaseExists)
		{
			Vector OrderLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), HiveToSecure->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			return AICOMM_IssueMovementOrder(pBot, Recipient, OrderLocation);
		}
	}

	DeployableSearchFilter TFFilter;
	TFFilter.DeployableTeam = CommanderTeam;
	TFFilter.DeployableTypes = STRUCTURE_MARINE_TURRETFACTORY;
	TFFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	TFFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	TFFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	AvHAIBuildableStructure TF = AITAC_FindClosestDeployableToLocation(HiveToSecure->FloorLocation, &TFFilter);

	if (!TF.IsValid())
	{
		Vector OrderLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), HiveToSecure->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		return AICOMM_IssueMovementOrder(pBot, Recipient, OrderLocation);
	}

	DeployableSearchFilter TurretFilter;
	TurretFilter.DeployableTeam = CommanderTeam;
	TurretFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;
	TurretFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	TurretFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	TurretFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

	int NumTurrets = AITAC_GetNumDeployablesNearLocation(TF.Location, &TurretFilter);

	if (NumTurrets < 5)
	{
		Vector OrderLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), HiveToSecure->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		return AICOMM_IssueMovementOrder(pBot, Recipient, OrderLocation);
	}

	AvHAIResourceNode* HiveNode = HiveToSecure->HiveResNodeRef;

	if (HiveNode && (FNullEnt(HiveNode->ActiveTowerEntity) || HiveNode->ActiveTowerEntity->v.team != CommanderTeam || !UTIL_StructureIsFullyBuilt(HiveNode->ActiveTowerEntity)))
	{
		Vector OrderLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), HiveNode->Location, UTIL_MetresToGoldSrcUnits(2.0f));

		return AICOMM_IssueMovementOrder(pBot, Recipient, OrderLocation);
	}

	Vector OrderLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), HiveToSecure->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

	return AICOMM_IssueMovementOrder(pBot, Recipient, OrderLocation);

}

bool AICOMM_IssueSiegeHiveOrder(AvHAIPlayer* pBot, edict_t* Recipient, const AvHAIHiveDefinition* HiveToSiege, const Vector SiegePosition)
{
	if (!HiveToSiege || FNullEnt(Recipient) || !IsPlayerActiveInGame(Recipient)) { return false; }

	return AICOMM_IssueMovementOrder(pBot, Recipient, SiegePosition);
}

bool AICOMM_IssueSecureResNodeOrder(AvHAIPlayer* pBot, edict_t* Recipient, const AvHAIResourceNode* ResNode)
{
	if (!ResNode || FNullEnt(Recipient) || !IsPlayerActiveInGame(Recipient)) { return false; }

	Vector MoveLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), ResNode->Location, UTIL_MetresToGoldSrcUnits(3.0f));

	if (MoveLocation == ZERO_VECTOR)
	{
		MoveLocation = ResNode->Location;
	}

	return AICOMM_IssueMovementOrder(pBot, Recipient, MoveLocation);
}

ai_commander_request* AICOMM_GetExistingRequestForPlayer(AvHAIPlayer* pBot, edict_t* Requestor)
{
	for (auto it = pBot->ActiveRequests.begin(); it != pBot->ActiveRequests.end(); it++)
	{
		if (it->Requestor == Requestor)
		{
			return &(*it);
		}
	}

	return nullptr;
}

void AICOMM_CheckNewRequests(AvHAIPlayer* pBot)
{
	// Clear all expired requests
	for (auto it = pBot->ActiveRequests.begin(); it != pBot->ActiveRequests.end();)
	{
		if (gpGlobals->time - it->RequestTime > BALANCE_VAR(kAlertExpireTime))
		{
			it = pBot->ActiveRequests.erase(it);
		}
		else
		{
			it++;
		}
	}

	AvHTeam* TeamRef = pBot->Player->GetTeamPointer();

	AlertListType HealthRequests = TeamRef->GetAlerts(COMMANDER_NEXTHEALTH);
	AlertListType AmmoRequests = TeamRef->GetAlerts(COMMANDER_NEXTAMMO);
	AlertListType OrderRequests = TeamRef->GetAlerts(COMMANDER_NEXTIDLE);

	// Cycle through all active health requests and see if any are new or overriding existing ones
	for (auto it = HealthRequests.begin(); it != HealthRequests.end(); it++)
	{
		edict_t* Requestor = INDEXENT(it->GetEntityIndex());

		ai_commander_request* ExistingRequest = AICOMM_GetExistingRequestForPlayer(pBot, Requestor);

		// This is a new request from the player, update our list accordingly
		if (!ExistingRequest || ExistingRequest->RequestTime < it->GetTime())
		{
			ai_commander_request NewRequest;
			ai_commander_request* RequestRef = (ExistingRequest) ? ExistingRequest : &NewRequest;
						
			RequestRef->bNewRequest = true;
			RequestRef->bResponded = false;
			RequestRef->RequestTime = it->GetTime();
			RequestRef->RequestType = COMMANDER_NEXTHEALTH;
			RequestRef->Requestor = Requestor;

			if (!ExistingRequest)
			{
				pBot->ActiveRequests.push_back(NewRequest);
			}
		}
	}

	// Do same for ammo requests
	for (auto it = AmmoRequests.begin(); it != AmmoRequests.end(); it++)
	{
		edict_t* Requestor = INDEXENT(it->GetEntityIndex());

		ai_commander_request* ExistingRequest = AICOMM_GetExistingRequestForPlayer(pBot, Requestor);

		if (!ExistingRequest || ExistingRequest->RequestTime < it->GetTime())
		{
			ai_commander_request NewRequest;
			ai_commander_request* RequestRef = (ExistingRequest) ? ExistingRequest : &NewRequest;

			RequestRef->bNewRequest = true;
			RequestRef->bResponded = false;
			RequestRef->RequestTime = it->GetTime();
			RequestRef->RequestType = COMMANDER_NEXTAMMO;
			RequestRef->Requestor = Requestor;

			if (!ExistingRequest)
			{
				pBot->ActiveRequests.push_back(NewRequest);
			}
		}
	}

	// And for order requests
	for (auto it = OrderRequests.begin(); it != OrderRequests.end(); it++)
	{
		edict_t* Requestor = INDEXENT(it->GetEntityIndex());

		ai_commander_request* ExistingRequest = AICOMM_GetExistingRequestForPlayer(pBot, Requestor);

		if (!ExistingRequest || ExistingRequest->RequestTime < it->GetTime())
		{
			ai_commander_request NewRequest;
			ai_commander_request* RequestRef = (ExistingRequest) ? ExistingRequest : &NewRequest;

			RequestRef->bNewRequest = true;
			RequestRef->bResponded = false;
			RequestRef->RequestTime = it->GetTime();
			RequestRef->RequestType = COMMANDER_NEXTIDLE;
			RequestRef->Requestor = Requestor;

			if (!ExistingRequest)
			{
				pBot->ActiveRequests.push_back(NewRequest);
			}
		}
	}
}

bool AICOMM_IsRequestValid(ai_commander_request* Request)
{
	if (Request->bResponded) { return false; }

	// We tried and failed to respond 5 times, give up so we don't block the queue of people needing help
	if (Request->ResponseAttempts > 5) { return false; }

	edict_t* Requestor = Request->Requestor;

	if (FNullEnt(Requestor) || !IsPlayerActiveInGame(Requestor)) { return false; }

	AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(Requestor));

	if (!PlayerRef) { return false; }

	AvHTeamNumber RequestorTeam = PlayerRef->GetTeam();

	switch (Request->RequestType)
	{
		case COMMANDER_NEXTHEALTH:
			return Requestor->v.health < Requestor->v.max_health;
		case BUILD_SHOTGUN:
			return !PlayerHasWeapon(PlayerRef, WEAPON_MARINE_SHOTGUN)
				&& !AITAC_ItemExistsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_SHOTGUN, RequestorTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false) ;
		case BUILD_WELDER:
			return !PlayerHasWeapon(PlayerRef, WEAPON_MARINE_WELDER)
				&& !AITAC_ItemExistsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_WELDER, RequestorTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		case BUILD_HMG:
			return !PlayerHasWeapon(PlayerRef, WEAPON_MARINE_HMG)
				&& !AITAC_ItemExistsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_HMG, RequestorTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		case BUILD_HEAVY:
			return !PlayerHasEquipment(Requestor)
				&& !AITAC_ItemExistsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_HEAVYARMOUR, RequestorTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		case BUILD_JETPACK:
			return !PlayerHasEquipment(Requestor)
				&& !AITAC_ItemExistsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_JETPACK, RequestorTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		case BUILD_GRENADE_GUN:
			return !PlayerHasWeapon(PlayerRef, WEAPON_MARINE_GL)
				&& !AITAC_ItemExistsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_GRENADELAUNCHER, RequestorTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		case BUILD_MINES:
			return !PlayerHasWeapon(PlayerRef, WEAPON_MARINE_MINES)
				&& !AITAC_ItemExistsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_MINES, RequestorTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		case BUILD_CAT:
			return !IsPlayerBuffed(Requestor)
				&& !AITAC_ItemExistsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_CATALYSTS, RequestorTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		case BUILD_PHASEGATE:
			return !AITAC_IsStructureOfTypeNearLocation(RequestorTeam, STRUCTURE_MARINE_PHASEGATE, Requestor->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));
		case BUILD_TURRET_FACTORY:
			return !AITAC_IsStructureOfTypeNearLocation(RequestorTeam, (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));
		case BUILD_ARMORY:
			return !AITAC_IsStructureOfTypeNearLocation(RequestorTeam, (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));	
		case BUILD_COMMANDSTATION:
			return !AITAC_IsStructureOfTypeNearLocation(RequestorTeam, STRUCTURE_MARINE_COMMCHAIR, Requestor->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));
		case BUILD_SCAN:
			return !AITAC_ItemExistsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_SCAN, RequestorTeam, AI_REACHABILITY_NONE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		case BUILD_ARMSLAB:
			return !AITAC_IsStructureOfTypeNearLocation(RequestorTeam, STRUCTURE_MARINE_ARMSLAB, Requestor->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));
		case BUILD_PROTOTYPE_LAB:
			return !AITAC_IsStructureOfTypeNearLocation(RequestorTeam, STRUCTURE_MARINE_PROTOTYPELAB, Requestor->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));
		default:
			return true;
	}

	return true;
}

Vector AICOMM_GetNextScanLocation(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	AvHClassType EnemyType = AIMGR_GetTeamType(EnemyTeam);

	DeployableSearchFilter ObsFilter;
	ObsFilter.DeployableTeam = BotTeam;
	ObsFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	ObsFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	if (!AITAC_DeployableExistsAtLocation(ZERO_VECTOR, &ObsFilter)) { return ZERO_VECTOR; }

	AvHAIBuildableStructure SiegeTurret;

	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
	{
		AvHAIMarineBase* ThisBase = &(*it);

		if (ThisBase->BaseType != MARINE_BASE_SIEGE || ThisBase->bRecycleBase) { continue; }

		bool bHasAdvTF = false;
		bool bHasST = false;

		for (auto structIt = ThisBase->PlacedStructures.begin(); structIt != ThisBase->PlacedStructures.end(); structIt++)
		{
			AvHAIBuildableStructure StructureRef = AITAC_GetDeployableStructureByEntIndex(BotTeam, *structIt);

			if (StructureRef.IsValid() && StructureRef.IsCompleted())
			{
				if (StructureRef.StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
				{
					bHasAdvTF = true;
				}
				else if (StructureRef.StructureType == STRUCTURE_MARINE_SIEGETURRET)
				{
					SiegeTurret = StructureRef;
					bHasST = true;
				}
			}
		}

		if (bHasAdvTF && bHasST)
		{
			if (EnemyType == AVH_CLASS_TYPE_ALIEN)
			{
				const AvHAIHiveDefinition* SiegedHive = AITAC_GetActiveHiveNearestLocation(EnemyTeam, ThisBase->BaseLocation);

				if (SiegedHive && vDist2DSq(SiegedHive->Location, ThisBase->BaseLocation) < sqrf(BALANCE_VAR(kSiegeTurretRange)))
				{
					bool bAlreadyScanning = AITAC_ItemExistsInLocation(SiegedHive->Location, DEPLOYABLE_ITEM_SCAN, TEAM_IND, AI_REACHABILITY_NONE, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), false);

					if (!bAlreadyScanning)
					{
						Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), SiegedHive->FloorLocation, UTIL_MetresToGoldSrcUnits(3.0f));

						return (!vIsZero(BuildLocation)) ? BuildLocation : SiegedHive->FloorLocation;
					}
				}
			}

			DeployableSearchFilter EnemyStuffFilter;
			EnemyStuffFilter.DeployableTeam = EnemyTeam;
			EnemyStuffFilter.MaxSearchRadius = BALANCE_VAR(kSiegeTurretRange);

			AvHAIBuildableStructure NearestEnemyThing = AITAC_FindClosestDeployableToLocation(SiegeTurret.Location, &EnemyStuffFilter);

			if (NearestEnemyThing.IsValid())
			{
				bool bAlreadyScanning = AITAC_ItemExistsInLocation(NearestEnemyThing.Location, DEPLOYABLE_ITEM_SCAN, TEAM_IND, AI_REACHABILITY_NONE, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), false);

				if (!bAlreadyScanning)
				{
					Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestEnemyThing.Location, UTIL_MetresToGoldSrcUnits(3.0f));

					return (!vIsZero(BuildLocation)) ? BuildLocation : NearestEnemyThing.Location;
				}
			}
		}

	}

	return ZERO_VECTOR;
}

bool AICOMM_CheckForNextBuildAction(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	Vector NextScanLocation = AICOMM_GetNextScanLocation(pBot);

	if (!vIsZero(NextScanLocation))
	{
		bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_SCAN, NextScanLocation);

		if (bSuccess) { return true; }
	}

	edict_t* CommChair = AITAC_GetCommChair(BotTeam);

	if (FNullEnt(CommChair)) { return false; }	

	AvHAIMarineBase* MainBase = nullptr;
	AvHAIMarineBase* SiegeBase = nullptr;
	AvHAIMarineBase* OutpostBase = nullptr;	
	AvHAIMarineBase* GuardpostBase = nullptr;

	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
	{
		AvHAIMarineBase* ThisBase = &(*it);

		if (!ThisBase->bCanBeBuiltOut || ThisBase->bRecycleBase || !ThisBase->bIsActive || ThisBase->NumBuilders == 0) { continue; }

		if (ThisBase->BaseType == MARINE_BASE_MAINBASE)
		{
			MainBase = ThisBase;
			continue;
		}
				
		if (ThisBase->BaseType == MARINE_BASE_SIEGE)
		{
			if (ThisBase->NumEnemies > ThisBase->NumBuilders) { continue; }

			if (!ThisBase->bBaseInitialised)
			{
				if (!AITAC_GetNearestHiddenPlayerInLocation(BotTeam, ThisBase->BaseLocation, UTIL_MetresToGoldSrcUnits(5.0f))) { continue; }
			}

			// Always favour bases which are started but not established yet
			if (!SiegeBase || (SiegeBase->bIsBaseEstablished && !ThisBase->bIsBaseEstablished))
			{
				SiegeBase = ThisBase;
			}
		}
		else if (ThisBase->BaseType == MARINE_BASE_OUTPOST)
		{
			// Don't build this base out yet if there are enemies in it: helps avoid wasting resources
			if (ThisBase->NumEnemies > 0) { continue; }

			// Always favour bases which are started but not established yet
			if (!OutpostBase || (OutpostBase->bIsBaseEstablished && !ThisBase->bIsBaseEstablished))
			{
				OutpostBase = ThisBase;
			}
		}
		else if (ThisBase->BaseType == MARINE_BASE_GUARDPOST)
		{
			// Don't build this base out yet if there are enemies in it: helps avoid wasting resources
			if (ThisBase->NumEnemies > 0) { continue; }

			// Always favour bases which are started but not established yet
			if (!GuardpostBase || (GuardpostBase->bIsBaseEstablished && !ThisBase->bIsBaseEstablished))
			{
				GuardpostBase = ThisBase;
			}
		}
	}

	// This is important, make sure we always prioritise critical base infrastructure over everything else
	// If we don't have enough resources to drop the critical infrastructure then block the whole commander thought process
	// This ensures we aren't placing sentries in Eclipse Command when we don't even have an infantry portal at base
	if (MainBase)
	{
		bool bMustPrioritise = !MainBase->bIsBaseEstablished;

		if (!bMustPrioritise)
		{
			vector<AvHAIBuildableStructure> BaseStructures = AICOMM_GetBaseStructures(MainBase);

			bool bHasArmoury = false;
			bool bHasTF = false;
			bool bHasPG = false;
			int NumSentries = 0;

			int DesiredInfPortals = (int)ceilf((float)AIMGR_GetNumPlayersOnTeam(BotTeam) / 4.0f);
			int NumInfantryPortals = 0;

			for (auto it = BaseStructures.begin(); it != BaseStructures.end(); it++)
			{
				if (it->StructureType == STRUCTURE_MARINE_INFANTRYPORTAL) { NumInfantryPortals++; }
				else if (it->StructureType == STRUCTURE_MARINE_PHASEGATE) { bHasPG = true; }
				else if (it->StructureType & (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY)) { bHasArmoury = true; }
				else if (it->StructureType & (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY)) { bHasTF = true; }
				else if (it->StructureType == STRUCTURE_MARINE_TURRET) { NumSentries++; }				
			}

			bMustPrioritise = !bHasArmoury || !bHasTF || NumInfantryPortals < DesiredInfPortals || NumSentries < 3 || (!bHasPG && AITAC_ResearchIsComplete(BotTeam, TECH_RESEARCH_PHASETECH));
		}

		if (bMustPrioritise)
		{
			bool bSuccess = AICOMM_BuildOutBase(pBot, MainBase);

			if (bSuccess) { return true; }

			// We can't build anything in our base right now, but don't build anything elsewhere unless we have at least 30 resources so we can put down infrastructure when needed
			if (pBot->Player->GetResources() < BALANCE_VAR(kInfantryPortalCost) * 1.5f) { return true; }
		}
	}

	const AvHAIResourceNode* CappableNode = AICOMM_GetNearestResourceNodeCapOpportunity(BotTeam, CommChair->v.origin);

	if (CappableNode)
	{
		AvHAIBuildableStructure* DeployedStructure = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_RESTOWER, CappableNode->Location);

		if (DeployedStructure || pBot->Player->GetResources() <= BALANCE_VAR(kResourceTowerCost) + 10) { return true; }
	}

	if (SiegeBase)
	{
		bool bSuccess = AICOMM_BuildOutBase(pBot, SiegeBase);

		if (bSuccess) { return true; }
	}

	if (OutpostBase)
	{
		bool bSuccess = AICOMM_BuildOutBase(pBot, OutpostBase);

		if (bSuccess) { return true; }
	}

	if (GuardpostBase)
	{
		bool bSuccess = AICOMM_BuildOutBase(pBot, GuardpostBase);

		if (bSuccess) { return true; }
	}

	if (MainBase)
	{
		bool bSuccess = AICOMM_BuildOutBase(pBot, MainBase);

		if (bSuccess) { return true; }
	}

	int Resources = pBot->Player->GetResources();

	if (Resources > 100)
	{
		DeployableSearchFilter StructureFilter;

		StructureFilter.DeployableTypes = STRUCTURE_MARINE_RESTOWER;
		StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		StructureFilter.ExcludeStatusFlags = (STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_ELECTRIFIED);

		AvHAIBuildableStructure ResTower = AITAC_FindFurthestDeployableFromLocation(CommChair->v.origin, &StructureFilter);

		if (ResTower.IsValid() && AITAC_ElectricalResearchIsAvailable(ResTower.edict))
		{
			if (AICOMM_ResearchTech(pBot, &ResTower, RESEARCH_ELECTRICAL))
			{
				return true;
			}
		}
	}

	return false;

}

bool AICOMM_CheckForNextSupplyAction(AvHAIPlayer* pBot)
{
	AvHTeamNumber CommanderTeam = pBot->Player->GetTeam();

	// Even morer firsterer: Are we doing a shotgun rush? If so, drop a shotgun for everyone who doesn't have one yet
	if (AIBO_BuildOrderIsShotgunRush() && AIMGR_GetMatchLength() < 120)
	{
		// As long as we have enough resources, drop a shotgun for everyone who doesn't have one yet
		if (pBot->Player->GetResources() >= BALANCE_VAR(kShotgunCost))
		{
			int NumPlayers = AIMGR_GetNumPlayersOnTeam(CommanderTeam);
			int NumShotguns = AITAC_GetNumWeaponsInPlay(CommanderTeam, WEAPON_MARINE_SHOTGUN);
			if (NumShotguns < NumPlayers)
			{
				DeployableSearchFilter ArmouryFilter;
				ArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
				ArmouryFilter.DeployableTeam = CommanderTeam;
				ArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
				ArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

				AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &ArmouryFilter);

				if (NearestArmoury.IsValid())
				{
					Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));
					bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_SHOTGUN, DeployLocation);

					return bSuccess;
				}
			}
		}
	}

	// First thing: if our base is damaged and there's nobody able to weld, drop a welder so we don't let the base die
	bool bBaseIsDamaged = false;

	DeployableSearchFilter DamagedBaseStructures;
	DamagedBaseStructures.DeployableTypes = (STRUCTURE_MARINE_COMMCHAIR | STRUCTURE_MARINE_INFANTRYPORTAL);
	DamagedBaseStructures.DeployableTeam = CommanderTeam;
	DamagedBaseStructures.ReachabilityTeam = CommanderTeam;
	DamagedBaseStructures.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	DamagedBaseStructures.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	DamagedBaseStructures.IncludeStatusFlags = STRUCTURE_STATUS_DAMAGED;
	DamagedBaseStructures.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	DamagedBaseStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	bBaseIsDamaged = AITAC_DeployableExistsAtLocation(AITAC_GetCommChairLocation(CommanderTeam), &DamagedBaseStructures);

	if (bBaseIsDamaged)
	{
		AvHAIDroppedItem NearestWelder = AITAC_FindClosestItemToLocation(AITAC_GetCommChairLocation(CommanderTeam), DEPLOYABLE_ITEM_WELDER, CommanderTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(15.0f), false);
		bool bPlayerHasWelder = false;

		if (!NearestWelder.IsValid())
		{
			vector<AvHPlayer*> PlayersAtBase = AITAC_GetAllPlayersOfTeamInArea(CommanderTeam, AITAC_GetCommChairLocation(CommanderTeam), UTIL_MetresToGoldSrcUnits(15.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

			for (auto it = PlayersAtBase.begin(); it != PlayersAtBase.end(); it++)
			{
				AvHPlayer* ThisPlayer = (*it);

				if (PlayerHasWeapon(ThisPlayer, WEAPON_MARINE_WELDER))
				{
					bPlayerHasWelder = true;
				}
			}
		}

		if (!NearestWelder.IsValid() && !bPlayerHasWelder)
		{
			DeployableSearchFilter ArmouryFilter;
			ArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
			ArmouryFilter.DeployableTeam = CommanderTeam;
			ArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			ArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

			AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &ArmouryFilter);

			if (NearestArmoury.IsValid())
			{
				Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));
				bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_WELDER, DeployLocation);

				return bSuccess;
			}
		}

	}

	bool bShouldPrioritiseNodes = AICOMM_ShouldCommanderPrioritiseNodes(pBot);

	if (bShouldPrioritiseNodes && pBot->Player->GetResources() < 25) { return false; }

	// Now work out how many welders we want on the team generally
	
	int NumDesiredWelders = 1;

	if (!bShouldPrioritiseNodes && pBot->Player->GetResources() >= 20)
	{
		NumDesiredWelders = (int)ceilf((float)(AIMGR_GetNumPlayersOnTeam(CommanderTeam) - 1) * 0.3f);
	}

	int NumTeamWelders = AITAC_GetNumWeaponsInPlay(CommanderTeam, WEAPON_MARINE_WELDER);

	// Add additional welders to the team if we have hives or resource nodes which can only be reached with a welder

	vector<AvHAIResourceNode*> AllNodes = AITAC_GetAllResourceNodes();

	for (auto it = AllNodes.begin(); it != AllNodes.end(); it++)
	{
		AvHAIResourceNode* ThisNode = (*it);

		unsigned int TeamReachabilityFlags = (CommanderTeam == AIMGR_GetTeamANumber()) ? ThisNode->TeamAReachabilityFlags : ThisNode->TeamBReachabilityFlags;

		if ((TeamReachabilityFlags & AI_REACHABILITY_WELDER) && !(TeamReachabilityFlags & AI_REACHABILITY_MARINE))
		{
			NumDesiredWelders++;
			break;
		}

	}

	vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllHives();

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		AvHAIHiveDefinition* ThisHive = (*it);

		unsigned int TeamReachabilityFlags = (CommanderTeam == AIMGR_GetTeamANumber()) ? ThisHive->TeamAReachabilityFlags : ThisHive->TeamBReachabilityFlags;

		if ((TeamReachabilityFlags & AI_REACHABILITY_WELDER) && !(TeamReachabilityFlags & AI_REACHABILITY_MARINE))
		{
			NumDesiredWelders++;
			break;
		}
	}

	NumDesiredWelders = imini(NumDesiredWelders, (int)(ceilf((float)(AIMGR_GetNumPlayersOnTeam(CommanderTeam) - 1) * 0.5f)));

	if (NumTeamWelders < NumDesiredWelders)
	{
		DeployableSearchFilter ArmouryFilter;
		ArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
		ArmouryFilter.DeployableTeam = CommanderTeam;
		ArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		ArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &ArmouryFilter);

		if (NearestArmoury.IsValid())
		{
			Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));
			bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_WELDER, DeployLocation);

			return bSuccess;
		}
	}

	// Don't drop stuff if we badly need resource nodes
	if (AICOMM_ShouldCommanderPrioritiseNodes(pBot) && pBot->Player->GetResources() < 30) { return false; }

	// Get basic research first
	if (!AITAC_ResearchIsComplete(CommanderTeam, TECH_RESEARCH_ARMOR_ONE) || !AITAC_ResearchIsComplete(CommanderTeam, TECH_RESEARCH_WEAPONS_ONE)) { return false; }

	// Likewise, don't spend res if we can get phase tech going!
	if (!AITAC_ResearchIsComplete(CommanderTeam, TECH_RESEARCH_PHASETECH) && AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_PHASETECH)) { return false; }

	int NumDesiredShotguns = (int)ceilf((AIMGR_GetNumPlayersOnTeam(CommanderTeam) - 1) * 0.33f);
	int NumShottysInPlay = AITAC_GetNumWeaponsInPlay(CommanderTeam, WEAPON_MARINE_SHOTGUN);

	if (NumShottysInPlay < NumDesiredShotguns)
	{
		DeployableSearchFilter ArmouryFilter;
		ArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
		ArmouryFilter.DeployableTeam = CommanderTeam;
		ArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		ArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &ArmouryFilter);

		if (NearestArmoury.IsValid())
		{
			Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));
			bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_SHOTGUN, DeployLocation);

			return bSuccess;
		}
	}
	
	int NumMinesInPlay = AITAC_GetNumWeaponsInPlay(CommanderTeam, WEAPON_MARINE_MINES);
	bool bNeedsMines = false;
	int DesiredMines = 0;

	DeployableSearchFilter MineFilter;
	MineFilter.DeployableTypes = STRUCTURE_MARINE_DEPLOYEDMINE;

	int NumDeployedMines = AITAC_GetNumDeployablesNearLocation(ZERO_VECTOR, &MineFilter);

	if (NumMinesInPlay < 2 && NumDeployedMines < 32)
	{
		int UnminedStructures = 0;

		DeployableSearchFilter MineStructures;
		MineStructures.DeployableTeam = CommanderTeam;
		MineStructures.DeployableTypes = (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_INFANTRYPORTAL);
		MineStructures.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		MineStructures.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		MineStructures.ReachabilityTeam = CommanderTeam;
		MineStructures.ReachabilityFlags = AI_REACHABILITY_MARINE;

		vector <AvHAIBuildableStructure> MineableStructures = AITAC_FindAllDeployables(ZERO_VECTOR, &MineStructures);

		MineStructures.DeployableTypes = STRUCTURE_MARINE_DEPLOYEDMINE;
		MineStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(3.0f);

		for (auto it = MineableStructures.begin(); it != MineableStructures.end(); it++)
		{
			AvHAIBuildableStructure ThisStructure = (*it);

			int NumMines = AITAC_GetNumDeployablesNearLocation(ThisStructure.Location, &MineStructures);

			if (NumMines < 2)
			{
				UnminedStructures++;
			}
		}

		DesiredMines = (int)ceilf((float)UnminedStructures / 2.0f);
		DesiredMines = clampi(DesiredMines, 0, 2);
	}

	if (NumMinesInPlay < DesiredMines)
	{
		DeployableSearchFilter ArmouryFilter;
		ArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
		ArmouryFilter.DeployableTeam = CommanderTeam;
		ArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		ArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &ArmouryFilter);

		if (NearestArmoury.IsValid())
		{
			Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));
			bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_MINES, DeployLocation);

			return bSuccess;
		}
	}

	if (!AITAC_ResearchIsComplete(CommanderTeam, TECH_RESEARCH_HEAVYARMOR)) { return false; }
	
	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = STRUCTURE_MARINE_ADVARMOURY;
	StructureFilter.DeployableTeam = CommanderTeam;
	StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	AvHAIBuildableStructure NearestAdvArmoury = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_PROTOTYPELAB;
	AvHAIBuildableStructure NearestPrototypeLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

	if (!NearestAdvArmoury.IsValid() || !NearestPrototypeLab.IsValid()) { return false; }

	AvHAIDroppedItem ExistingHA = AITAC_FindClosestItemToLocation(NearestPrototypeLab.Location, DEPLOYABLE_ITEM_HEAVYARMOUR, CommanderTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
	AvHAIDroppedItem ExistingHMG = AITAC_FindClosestItemToLocation(NearestAdvArmoury.Location, DEPLOYABLE_ITEM_HMG, CommanderTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
	AvHAIDroppedItem ExistingWelder = AITAC_FindClosestItemToLocation(NearestAdvArmoury.Location, DEPLOYABLE_ITEM_WELDER, CommanderTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);

	if (ExistingHA.IsValid() && ExistingHMG.IsValid() && ExistingWelder.IsValid()) { return false; }

	vector<edict_t*> NearbyPlayers = AITAC_GetAllPlayersOfClassInArea(CommanderTeam, NearestAdvArmoury.Location, UTIL_MetresToGoldSrcUnits(10.0f), false, pBot->Edict, AVH_USER3_MARINE_PLAYER);

	bool bDropWeapon = false;
	bool bDropWelder = false;

	for (auto it = NearbyPlayers.begin(); it != NearbyPlayers.end(); it++)
	{
		edict_t* PlayerEdict = (*it);
		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));
		if (!PlayerEdict) { continue; }

		if (PlayerHasHeavyArmour(PlayerEdict) || PlayerHasJetpack(PlayerEdict))
		{
			if (PlayerHasWeapon(PlayerRef, WEAPON_MARINE_MG) || UTIL_GetPlayerPrimaryWeapon(PlayerRef) == WEAPON_INVALID)
			{
				bDropWeapon = true;
			}
			else
			{
				if (!PlayerHasWeapon(PlayerRef, WEAPON_MARINE_WELDER))
				{
					bDropWelder = true;
				}
			}
		}
	}

	if (!ExistingHA.IsValid() && !bDropWelder && !bDropWeapon)
	{
		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestPrototypeLab.Location, UTIL_MetresToGoldSrcUnits(3.0f));

		if (vIsZero(DeployLocation))
		{
			DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestPrototypeLab.Location, UTIL_MetresToGoldSrcUnits(3.0f));
		}

		bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_HEAVYARMOUR, DeployLocation);

		return bSuccess;
	}

	if (bDropWeapon && !ExistingHMG.IsValid())
	{
		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestAdvArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));

		if (vIsZero(DeployLocation))
		{
			DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestAdvArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));
		}

		bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_HMG, DeployLocation);

		return bSuccess;
	}

	if (bDropWelder && !ExistingWelder.IsValid())
	{
		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestAdvArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));

		if (vIsZero(DeployLocation))
		{
			DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestAdvArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));
		}

		bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_WELDER, DeployLocation);

		return bSuccess;
	}

	return false;
}

bool AICOMM_CheckForNextResearchAction(AvHAIPlayer* pBot)
{
	AvHTeamNumber CommanderTeam = pBot->Player->GetTeam();

	vector<AvHAIHiveDefinition*> Hives = AITAC_GetAllHives();

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		AvHAIHiveDefinition* Hive = (*it);

		if (Hive->Status != HIVE_STATUS_UNBUILT) { continue; }

		DeployableSearchFilter TFFilter;
		TFFilter.DeployableTeam = pBot->Player->GetTeam();
		TFFilter.DeployableTypes = STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY;
		TFFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		TFFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_ELECTRIFIED;
		TFFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		AvHAIBuildableStructure NearestTF = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &TFFilter);

		if (NearestTF.IsValid())
		{
			TFFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;
			TFFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

			int NumTurrets = AITAC_GetNumDeployablesNearLocation(NearestTF.Location, &TFFilter);

			if (NumTurrets > 0)
			{
				if (AICOMM_ResearchTech(pBot, &NearestTF, RESEARCH_ELECTRICAL))
				{
					return true;
				}
			}
		}

		if (pBot->Player->GetResources() > 60)
		{
			if (Hive->HiveResNodeRef && Hive->HiveResNodeRef->OwningTeam == pBot->Player->GetTeam())
			{
				edict_t* Tower = Hive->HiveResNodeRef->ActiveTowerEntity;
				if (!FNullEnt(Tower) && UTIL_StructureIsFullyBuilt(Tower) && !UTIL_IsStructureElectrified(Tower))
				{
					AvHAIBuildableStructure ResTower = AITAC_GetDeployableFromEdict(Tower);

					if (ResTower.IsValid() && AICOMM_ResearchTech(pBot, &ResTower, RESEARCH_ELECTRICAL))
					{
						return true;
					}
				}
			}
		}

	}

	// We will look at the structures we can research from first, then the ones we can build


	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTeam = CommanderTeam;
	StructureFilter.ReachabilityTeam = CommanderTeam;
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	// Let's put the new logic here. We iterate over the build order just like when building the main base.
	if (!AIBO_CurrentBuildOrder)
	{
		g_engfuncs.pfnServerPrint("Trying to research tech but no current build order set.\n");
		return false;
	}

	// This exact check exists twice in this file.
	// Perhaps we should move it to a function in AvHAIMarineBuildOrderManager?
	// Note to future me: Do that! With love your past self.
	for (auto& NextBuildOrderEntry : AIBO_CurrentBuildOrder->BuildOrder)
	{
		if (NextBuildOrderEntry.BuildOrderType != BUILD_ORDER_UPGRADE) { continue; }
		if (NextBuildOrderEntry.UpgradeToResearch == TECH_NULL) { continue; }

		// Check the conditions defined in the build order
		bool conditionOne = NextBuildOrderEntry.BuildConditionOne == CONDITION_NONE ? true : false;
		bool conditionTwo = NextBuildOrderEntry.BuildConditionTwo == CONDITION_NONE ? true : false;;
		bool connective = false;

		if (NextBuildOrderEntry.BuildConditionOne == STRUCTURE_EXISTS)
		{
			DeployableSearchFilter RequiredStructuresFilter;
			RequiredStructuresFilter.DeployableTeam = CommanderTeam;
			RequiredStructuresFilter.DeployableTypes = NextBuildOrderEntry.StructureRequiredOne;
			RequiredStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			RequiredStructuresFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

			vector <AvHAIBuildableStructure> FoundStructures = AITAC_FindAllDeployables(ZERO_VECTOR, &RequiredStructuresFilter);
			conditionOne = FoundStructures.size() == 0 ? false : true;
		}
		if (NextBuildOrderEntry.BuildConditionOne == UPGRADE_EXISTS)
		{
			conditionOne = (NextBuildOrderEntry.UpgradeRequiredOne == TECH_NULL || !AITAC_ResearchIsComplete(CommanderTeam, NextBuildOrderEntry.UpgradeRequiredOne)) ? false : true;
		}
		if (NextBuildOrderEntry.BuildConditionOne == TIME_ELAPSED)
		{
			conditionOne = AIMGR_GetMatchLength() < NextBuildOrderEntry.TimeLimitInSecondsOne ? false : true;
		}

		if (NextBuildOrderEntry.BuildConditionTwo == STRUCTURE_EXISTS)
		{
			DeployableSearchFilter RequiredStructuresFilter;
			RequiredStructuresFilter.DeployableTeam = CommanderTeam;
			RequiredStructuresFilter.DeployableTypes = NextBuildOrderEntry.StructureRequiredTwo;
			RequiredStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			RequiredStructuresFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

			vector <AvHAIBuildableStructure> FoundStructures = AITAC_FindAllDeployables(ZERO_VECTOR, &RequiredStructuresFilter);
			conditionTwo = FoundStructures.size() == 0 ? false : true;
		}
		if (NextBuildOrderEntry.BuildConditionTwo == UPGRADE_EXISTS)
		{
			conditionTwo = (NextBuildOrderEntry.UpgradeRequiredTwo == TECH_NULL || !AITAC_ResearchIsComplete(CommanderTeam, NextBuildOrderEntry.UpgradeRequiredTwo)) ? false : true;
		}
		if (NextBuildOrderEntry.BuildConditionTwo == TIME_ELAPSED)
		{

			conditionTwo = AIMGR_GetMatchLength() < NextBuildOrderEntry.TimeLimitInSecondsTwo ? false : true;
		}

		if (NextBuildOrderEntry.Connective == AND)
		{
			connective = conditionOne && conditionTwo;
		}
		if (NextBuildOrderEntry.Connective == OR)
		{
			connective = conditionOne || conditionTwo;
		}

		if (!connective) { continue; }
		// Done checking the conditions

		if (AITAC_MarineResearchIsAvailable(CommanderTeam,AIBO_MapTechIDToMessageID(NextBuildOrderEntry.UpgradeToResearch)))
		{
			StructureFilter.DeployableTypes = AIBO_MapTechToRequiredStructure(NextBuildOrderEntry.UpgradeToResearch);
			StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;
			AvHAIBuildableStructure NearestStructure = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);
			if (NearestStructure.IsValid())
			{
				bool bSuccess = AICOMM_ResearchTech(pBot, &NearestStructure, AIBO_MapTechIDToMessageID(NextBuildOrderEntry.UpgradeToResearch));
				if (bSuccess) { return true; }
				return pBot->Player->GetResources() < (AIBO_GetResearchCost(NextBuildOrderEntry.UpgradeToResearch) * 1.5f);
			}
		}
	}
	return false;
}

const AvHAIHiveDefinition* AICOMM_GetHiveSiegeOpportunityNearestLocation(AvHAIPlayer* CommanderBot, const Vector SearchLocation)
{
	AvHTeamNumber CommanderTeam = CommanderBot->Player->GetTeam();

	bool bPhaseGatesAvailable = AITAC_PhaseGatesAvailable(CommanderTeam);

	// Only siege if we have phase gates available
	if (!bPhaseGatesAvailable) { return nullptr; }

	const AvHAIHiveDefinition* Result = nullptr;
	float MinDist = 0.0f;

	const vector<AvHAIHiveDefinition*> Hives = AITAC_GetAllHives();

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		const AvHAIHiveDefinition* Hive = (*it);

		if (Hive->Status == HIVE_STATUS_UNBUILT) { continue; }

		DeployableSearchFilter StructureFilter;
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
		StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure BuiltPhaseGate = AITAC_FindClosestDeployableToLocation(Hive->Location, &StructureFilter);

		// If we have a phase gate already in place, then keep building as long as someone is there. If we don't have a phase gate, only build if there is a marine who isn't sighted by the enemy (to allow element of surprise)
		if (BuiltPhaseGate.IsValid())
		{
			int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(CommanderTeam, BuiltPhaseGate.Location, UTIL_MetresToGoldSrcUnits(5.0f), false, CommanderBot->Edict, AVH_USER3_COMMANDER_PLAYER);

			if (NumBuilders == 0) { continue; }
		}
		else
		{
			if (AITAC_GetMarineEligibleToBuildSiege(CommanderTeam, Hive) == nullptr) { continue; }
		}

		float ThisDist = vDist2DSq(Hive->FloorLocation, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = Hive;
			MinDist = ThisDist;
		}

	}

	return Result;
}

const AvHAIResourceNode* AICOMM_GetNearestResourceNodeCapOpportunity(const AvHTeamNumber Team, const Vector SearchLocation)
{
	vector<AvHAIResourceNode*> AllNodes = AITAC_GetAllResourceNodes();

	AvHAIResourceNode* Result = nullptr;
	float MinDist = 0.0f;

	for (auto it = AllNodes.begin(); it != AllNodes.end(); it++)
	{
		AvHAIResourceNode* ResNode = (*it);

		if (ResNode->bIsOccupied) { continue; }

		if (!AITAC_AnyPlayerOnTeamWithLOS(Team, (ResNode->Location + Vector(0.0f, 0.0f, 32.0f)), UTIL_MetresToGoldSrcUnits(5.0f))) { continue; }

		if (AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(Team), (ResNode->Location + Vector(0.0f, 0.0f, 32.0f)), UTIL_MetresToGoldSrcUnits(10.0f))) { continue; }

		float ThisDist = vDist2DSq(ResNode->Location, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = ResNode;
			MinDist = ThisDist;
		}
	}

	return Result;
}

bool AICOMM_CheckForNextRecycleAction(AvHAIPlayer* pBot)
{
	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
	{
		AvHAIMarineBase* ThisBase = &(*it);

		if (ThisBase->bRecycleBase)
		{
			for (auto structIt = ThisBase->PlacedStructures.begin(); structIt != ThisBase->PlacedStructures.end(); structIt++)
			{
				AvHAIBuildableStructure ThisStructure = AITAC_GetDeployableStructureByEntIndex(pBot->Player->GetTeam(), (*structIt));

				if (ThisStructure.IsValid() && !(ThisStructure.StructureStatusFlags & STRUCTURE_STATUS_RECYCLING))
				{
					bool bSuccess = AICOMM_RecycleStructure(pBot, &ThisStructure);

					if (bSuccess) { return true; }
				}
			}
		}
		else if (ThisBase->BaseType != MARINE_BASE_MAINBASE)
		{
			for (auto structIt = ThisBase->PlacedStructures.begin(); structIt != ThisBase->PlacedStructures.end(); structIt++)
			{
				AvHAIBuildableStructure ThisStructure = AITAC_GetDeployableStructureByEntIndex(pBot->Player->GetTeam(), (*structIt));

				if (ThisStructure.IsValid() && !(ThisStructure.StructureStatusFlags & STRUCTURE_STATUS_RECYCLING))
				{
					// Don't allow any infantry portals to be scattered elsewhere outside the base
					if (ThisStructure.StructureType == STRUCTURE_MARINE_INFANTRYPORTAL)
					{
						bool bSuccess = AICOMM_RecycleStructure(pBot, &ThisStructure);

						if (bSuccess) { return true; }
					}
				}
			}
		}
	}

	return false;
}

bool AICOMM_CheckForNextSupportAction(AvHAIPlayer* pBot)
{
	AvHTeamNumber CommanderTeam = pBot->Player->GetTeam();

	AICOMM_CheckNewRequests(pBot);

	ai_commander_request* NextRequest = nullptr;

	float OldestTime = 0.0f;

	// Find the oldest request we haven't fulfilled yet
	for (auto it = pBot->ActiveRequests.begin(); it != pBot->ActiveRequests.end(); it++)
	{
		// Ignore if we've already responded, or the request is too new (leave a nice 1 second response time to requests)
		if (it->bResponded || (gpGlobals->time - it->RequestTime) < 1.0f) { continue; }

		// Ignore the request if it's not valid (e.g. request for health while not injured)
		if (!AICOMM_IsRequestValid(&(*it)))
		{
			it->bResponded = true;
			continue;
		}

		float ThisTime = gpGlobals->time - it->RequestTime;

		if (ThisTime > OldestTime)
		{
			NextRequest = &(*it);
			OldestTime = ThisTime;
		}

	}

	// We didn't find any unresponded requests outstanding
	if (!NextRequest) {	return false; }
	
	edict_t* Requestor = NextRequest->Requestor;

	if (FNullEnt(Requestor) || !IsEdictPlayer(Requestor) || !IsPlayerActiveInGame(Requestor))
	{
		NextRequest->bResponded = true;
		return false;
	}

	int NumDesiredHealthPacks = 0;
	int NumDesiredAmmoPacks = 0;

	float RequestorHealthDeficit = Requestor->v.max_health - Requestor->v.health;

	float HealthPerPack = BALANCE_VAR(kPointsPerHealth);

	if (RequestorHealthDeficit > 10.0f)
	{
		NumDesiredHealthPacks = (int)ceilf(RequestorHealthDeficit / HealthPerPack);

		int NumHealthPacksPresent = AITAC_GetNumItemsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_HEALTHPACK, (AvHTeamNumber)Requestor->v.team, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		NumDesiredHealthPacks -= NumHealthPacksPresent;
	}

	if (NumDesiredHealthPacks > 0)
	{
		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(2.0f));
		bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_HEALTHPACK, DeployLocation);

		if (bSuccess)
		{
			if (NextRequest->RequestType == COMMANDER_NEXTHEALTH && NumDesiredHealthPacks <= 1)
			{
				NextRequest->bResponded = true;
			}
		}
		else
		{
			NextRequest->ResponseAttempts++;
		}

		return true;
	}
	else
	{
		if (NextRequest->RequestType == COMMANDER_NEXTHEALTH)
		{
			NextRequest->bResponded = true;
			return false;
		}
	}

	if (NextRequest->RequestType == COMMANDER_NEXTAMMO)
	{
		AvHPlayer* thePlayer = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(Requestor));

		bool bFillPrimaryWeapon = true;

		AvHAIWeapon WeaponType = UTIL_GetPlayerPrimaryWeapon(thePlayer);

		// Requesting player doesn't have a primary weapon, check if they have a pistol
		if (WeaponType == WEAPON_INVALID)
		{
			bFillPrimaryWeapon = false;
			WeaponType = UTIL_GetPlayerSecondaryWeapon(thePlayer);
		}

		// They've only got a knife, don't bother dropping ammo
		if (WeaponType == WEAPON_INVALID)
		{
			NextRequest->bResponded = true;
			return false;
		}

		int AmmoDeficit = (bFillPrimaryWeapon) ? (UTIL_GetPlayerPrimaryMaxAmmoReserve(thePlayer) - UTIL_GetPlayerPrimaryAmmoReserve(thePlayer)) : (UTIL_GetPlayerSecondaryMaxAmmoReserve(thePlayer) - UTIL_GetPlayerSecondaryAmmoReserve(thePlayer));
		int WeaponClipSize = (bFillPrimaryWeapon) ? UTIL_GetPlayerPrimaryWeaponMaxClipSize(thePlayer) : UTIL_GetPlayerSecondaryWeaponMaxClipSize(thePlayer);

		// Player already has full ammo, they're yanking our chain
		if (AmmoDeficit == 0)
		{
			NextRequest->bResponded = true;
			return false;
		}

		int DesiredNumAmmoPacks = (int)(ceilf((float)AmmoDeficit / (float)WeaponClipSize));
		// Don't drop more than 5 at any one time
		DesiredNumAmmoPacks = clampi(DesiredNumAmmoPacks, 0, 5);

		int NumAmmoPacksPresent = AITAC_GetNumItemsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_AMMO, (AvHTeamNumber)Requestor->v.team, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		DesiredNumAmmoPacks -= NumAmmoPacksPresent;

		// Do we need to drop any ammo, or has the player got enough surrounding them already?
		if (DesiredNumAmmoPacks > 0)
		{
			Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(2.0f));
			bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_AMMO, DeployLocation);

			if (bSuccess)
			{
				// We've dropped enough that the player has enough to fill their boots. Mission accomplished
				if (DesiredNumAmmoPacks <= 1)
				{
					NextRequest->bResponded = true;
				}
			}
			else
			{
				NextRequest->ResponseAttempts++;
			}

			return true;
		}
		else
		{
			// Player already has enough ammo packs deployed by them to satisfy. Don't drop any more
			NextRequest->bResponded = true;
		}

		return false;
	}

	if (NextRequest->RequestType == COMMANDER_NEXTIDLE)
	{
		// TODO: Have the commander prioritise this player when looking for people to give orders to
		NextRequest->bResponded = true;
	}

	if (NextRequest->RequestType == BUILD_CAT)
	{
		if (!AITAC_ResearchIsComplete(CommanderTeam, TECH_RESEARCH_CATALYSTS))
		{
			char msg[128];
			sprintf(msg, "We haven't researched catalysts yet, %s. Ask again later.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}

		if (!AITAC_IsCompletedStructureOfTypeNearLocation(CommanderTeam, (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY), ZERO_VECTOR, 0.0f))
		{
			char msg[128];
			sprintf(msg, "Don't have an armory anymore, %s. We need to build one.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}


		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(2.0f));
		bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_CATALYSTS, DeployLocation);

		if (bSuccess)
		{
			NextRequest->bResponded = true;
		}
		else
		{
			NextRequest->ResponseAttempts++;
		}

		return true;
	}

	if (NextRequest->RequestType == BUILD_WELDER || NextRequest->RequestType == BUILD_SHOTGUN || NextRequest->RequestType == BUILD_MINES)
	{
		AvHAIDeployableItemType ItemToDrop = DEPLOYABLE_ITEM_NONE;
		float Cost = 0.0f;

		switch (NextRequest->RequestType)
		{
			case BUILD_WELDER:
				ItemToDrop = DEPLOYABLE_ITEM_WELDER;
				Cost = BALANCE_VAR(kWelderCost);
				break;
			case BUILD_SHOTGUN:
				ItemToDrop = DEPLOYABLE_ITEM_SHOTGUN;
				Cost = BALANCE_VAR(kShotgunCost);
				break;
			case BUILD_MINES:
				ItemToDrop = DEPLOYABLE_ITEM_MINES;
				Cost = BALANCE_VAR(kMineCost);
				break;
			default:
				ItemToDrop = DEPLOYABLE_ITEM_WELDER;
				Cost = BALANCE_VAR(kWelderCost);
				break;
		}

		DeployableSearchFilter ArmouryFilter;
		ArmouryFilter.DeployableTeam = CommanderTeam;
		ArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
		ArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		ArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		ArmouryFilter.MaxSearchRadius = BALANCE_VAR(kArmoryBuildDistance);

		AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(Requestor->v.origin, &ArmouryFilter);

		if (!NearestArmoury.IsValid())
		{
			if (!NextRequest->bAcknowledged)
			{
				char msg[128];
				sprintf(msg, "Get to an working armory %s, and ask again.", STRING(Requestor->v.netname));
				BotSay(pBot, true, 0.5f, msg);
				NextRequest->bAcknowledged = true;
			}

			return false;
		}

		if (pBot->Player->GetResources() < Cost)
		{
			if (!NextRequest->bAcknowledged)
			{
				char msg[128];
				sprintf(msg, "Wait for resources %s, I'll drop it soon.", STRING(Requestor->v.netname));
				BotSay(pBot, true, 0.5f, msg);
				NextRequest->bAcknowledged = true;
			}
			return false; 
		}

		Vector IdealDeployLocation = Requestor->v.origin + (UTIL_GetForwardVector2D(Requestor->v.angles) * 75.0f);
		Vector ProjectedDeployLocation = AdjustPointForPathfinding(IdealDeployLocation);

		if (vDist2DSq(ProjectedDeployLocation, NearestArmoury.Location) < BALANCE_VAR(kArmoryBuildDistance))
		{
			bool bSuccess = AICOMM_DeployItem(pBot, ItemToDrop, ProjectedDeployLocation);

			if (bSuccess)
			{
				NextRequest->bResponded = bSuccess;
				return true;
			}
		}

		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestArmoury.Location, UTIL_MetresToGoldSrcUnits(4.0f));

		if (vIsZero(DeployLocation))
		{
			DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestArmoury.Location, UTIL_MetresToGoldSrcUnits(4.0f));
		}

		if (vIsZero(DeployLocation))
		{
			char msg[128];
			sprintf(msg, "I can't find a drop location, %s. Try asking again elsewhere.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}

		bool bSuccess = AICOMM_DeployItem(pBot, ItemToDrop, DeployLocation);

		NextRequest->ResponseAttempts++;

		NextRequest->bResponded = bSuccess;
		return true;

	}

	if (NextRequest->RequestType == BUILD_HEAVY || NextRequest->RequestType == BUILD_JETPACK)
	{
		AvHAIDeployableItemType ItemToDrop = (NextRequest->RequestType == BUILD_HEAVY) ? DEPLOYABLE_ITEM_HEAVYARMOUR : DEPLOYABLE_ITEM_JETPACK;
		float Cost = (NextRequest->RequestType == BUILD_HEAVY) ? BALANCE_VAR(kHeavyArmorCost) : BALANCE_VAR(kJetpackCost);
		AvHTechID TechNeeded = (NextRequest->RequestType == BUILD_HEAVY) ? TECH_RESEARCH_HEAVYARMOR : TECH_RESEARCH_JETPACKS;

		if (!AITAC_ResearchIsComplete(CommanderTeam, TechNeeded))
		{
			char msg[128];
			sprintf(msg, "We haven't researched it yet, %s. Ask again later.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}

		DeployableSearchFilter PrototypeLabFilter;
		PrototypeLabFilter.DeployableTeam = CommanderTeam;
		PrototypeLabFilter.DeployableTypes = STRUCTURE_MARINE_PROTOTYPELAB;
		PrototypeLabFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		PrototypeLabFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure NearestPL = AITAC_FindClosestDeployableToLocation(Requestor->v.origin, &PrototypeLabFilter);

		if (!NearestPL.IsValid())
		{
			char msg[128];
			sprintf(msg, "We don't have a prototype lab %s, ask again later.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;

			return false;
		}

		if (vDist2DSq(Requestor->v.origin, NearestPL.Location) > sqrf(BALANCE_VAR(kArmoryBuildDistance)))
		{
			if (!NextRequest->bAcknowledged)
			{
				char msg[128];
				sprintf(msg, "Get near the prototype lab %s, and I will drop it for you.", STRING(Requestor->v.netname));
				BotSay(pBot, true, 0.5f, msg);
				NextRequest->bAcknowledged = true;
			}
			return false;
		}

		Vector IdealDeployLocation = Requestor->v.origin + (UTIL_GetForwardVector2D(Requestor->v.angles) * 75.0f);
		Vector ProjectedDeployLocation = AdjustPointForPathfinding(IdealDeployLocation);

		if (vDist2DSq(ProjectedDeployLocation, NearestPL.Location) < BALANCE_VAR(kArmoryBuildDistance))
		{
			bool bSuccess = AICOMM_DeployItem(pBot, ItemToDrop, ProjectedDeployLocation);

			if (bSuccess)
			{
				NextRequest->bResponded = bSuccess;
				return true;
			}
		}

		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestPL.Location, UTIL_MetresToGoldSrcUnits(4.0f));

		if (vIsZero(DeployLocation))
		{
			DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestPL.Location, UTIL_MetresToGoldSrcUnits(4.0f));
		}

		if (vIsZero(DeployLocation))
		{
			char msg[128];
			sprintf(msg, "I can't find a drop location, %s. Try asking again elsewhere.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}

		bool bSuccess = AICOMM_DeployItem(pBot, ItemToDrop, DeployLocation);

		NextRequest->ResponseAttempts++;

		NextRequest->bResponded = bSuccess;
		return true;
	}

	if (NextRequest->RequestType == BUILD_HMG || NextRequest->RequestType == BUILD_GRENADE_GUN)
	{
		AvHAIDeployableItemType ItemToDrop = (NextRequest->RequestType == BUILD_HMG) ? DEPLOYABLE_ITEM_HMG : DEPLOYABLE_ITEM_GRENADELAUNCHER;
		float Cost = (NextRequest->RequestType == BUILD_HMG) ? BALANCE_VAR(kHMGCost) : BALANCE_VAR(kGrenadeLauncherCost);

		DeployableSearchFilter ArmouryFilter;
		ArmouryFilter.DeployableTeam = CommanderTeam;
		ArmouryFilter.DeployableTypes = STRUCTURE_MARINE_ADVARMOURY;
		ArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		ArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(Requestor->v.origin, &ArmouryFilter);

		if (!NearestArmoury.IsValid())
		{
			char msg[128];
			sprintf(msg, "We don't have an adv armory yet %s, ask again later.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;

			return false;
		}

		if (vDist2DSq(Requestor->v.origin, NearestArmoury.Location) > sqrf(BALANCE_VAR(kArmoryBuildDistance)))
		{
			if (!NextRequest->bAcknowledged)
			{
				char msg[128];
				sprintf(msg, "Get near the adv armory %s, and I will drop it for you.", STRING(Requestor->v.netname));
				BotSay(pBot, true, 0.5f, msg);
				NextRequest->bAcknowledged = true;
			}
			return false;
		}

		if (pBot->Player->GetResources() < Cost)
		{
			if (!NextRequest->bAcknowledged)
			{
				char msg[128];
				sprintf(msg, "Wait for resources %s, will drop asap.", STRING(Requestor->v.netname));
				BotSay(pBot, true, 0.5f, msg);
				NextRequest->bAcknowledged = true;
			}
			return false;
		}

		Vector IdealDeployLocation = Requestor->v.origin + (UTIL_GetForwardVector2D(Requestor->v.angles) * 75.0f);
		Vector ProjectedDeployLocation = AdjustPointForPathfinding(IdealDeployLocation);

		if (vDist2DSq(ProjectedDeployLocation, NearestArmoury.Location) < BALANCE_VAR(kArmoryBuildDistance))
		{
			bool bSuccess = AICOMM_DeployItem(pBot, ItemToDrop, ProjectedDeployLocation);

			if (bSuccess)
			{
				NextRequest->bResponded = bSuccess;
				return true;
			}
		}

		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestArmoury.Location, UTIL_MetresToGoldSrcUnits(4.0f));

		if (vIsZero(DeployLocation))
		{
			DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestArmoury.Location, UTIL_MetresToGoldSrcUnits(4.0f));
		}

		if (vIsZero(DeployLocation))
		{
			char msg[128];
			sprintf(msg, "I can't find a drop location, %s. Try asking again elsewhere.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}

		bool bSuccess = AICOMM_DeployItem(pBot, ItemToDrop, DeployLocation);

		NextRequest->ResponseAttempts++;

		NextRequest->bResponded = bSuccess;
		return true;
	}

	if (NextRequest->RequestType == BUILD_SCAN)
	{
		DeployableSearchFilter ObsFilter;
		ObsFilter.DeployableTeam = CommanderTeam;
		ObsFilter.DeployableTypes = STRUCTURE_MARINE_OBSERVATORY;
		ObsFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		ObsFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure NearestObservatory = AITAC_FindClosestDeployableToLocation(Requestor->v.origin, &ObsFilter);

		if (!NearestObservatory.IsValid())
		{
			char msg[128];
			sprintf(msg, "We don't have an observatory yet %s, ask again later.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;

			return false;
		}

		Vector IdealDeployLocation = Requestor->v.origin + (UTIL_GetForwardVector2D(Requestor->v.angles) * 75.0f);
		Vector ProjectedDeployLocation = AdjustPointForPathfinding(IdealDeployLocation, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

		if (!vIsZero(ProjectedDeployLocation))
		{
			bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_SCAN, ProjectedDeployLocation);

			if (bSuccess)
			{
				NextRequest->bResponded = true;
				return true;
			}
		}

		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(1.0f));

		if (!vIsZero(DeployLocation))
		{
			bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_SCAN, DeployLocation);

			if (bSuccess)
			{
				NextRequest->bResponded = true;
				return true;
			}
		}

		DeployLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(DeployLocation))
		{
			bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_SCAN, DeployLocation);

			if (bSuccess)
			{
				NextRequest->bResponded = true;
				return true;
			}
			else
			{
				char msg[128];
				sprintf(msg, "I can't find a good scan spot, %s. Try again elsewhere.", STRING(Requestor->v.netname));
				BotSay(pBot, true, 0.5f, msg);
				NextRequest->bResponded = true;
				return false;
			}
		}
		else
		{
			char msg[128];
			sprintf(msg, "I can't find a good scan spot, %s. Try again elsewhere.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}

		return false;

	}

	if (NextRequest->RequestType == BUILD_PHASEGATE && !AITAC_ResearchIsComplete(CommanderTeam, TECH_RESEARCH_PHASETECH))
	{
		char msg[128];
		sprintf(msg, "We haven't got phase tech yet, %s. Ask again later.", STRING(Requestor->v.netname));
		BotSay(pBot, true, 0.5f, msg);
		NextRequest->bResponded = true;
		return false;
	}

	if (NextRequest->RequestType == BUILD_OBSERVATORY || NextRequest->RequestType == BUILD_ARMSLAB)
	{
		DeployableSearchFilter ArmouryFilter;
		ArmouryFilter.DeployableTeam = CommanderTeam;
		ArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
		ArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		ArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		bool bHasArmoury = AITAC_DeployableExistsAtLocation(ZERO_VECTOR, &ArmouryFilter);

		if (!bHasArmoury)
		{
			char msg[128];
			sprintf(msg, "We haven't got an armory yet, %s. Ask again later.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}
	}

	if (NextRequest->RequestType == BUILD_PROTOTYPE_LAB)
	{
		DeployableSearchFilter RequiredFilter;
		RequiredFilter.DeployableTeam = CommanderTeam;
		RequiredFilter.DeployableTypes = STRUCTURE_MARINE_ADVARMOURY;
		RequiredFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		RequiredFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		bool bHasArmoury = AITAC_DeployableExistsAtLocation(ZERO_VECTOR, &RequiredFilter);

		if (!bHasArmoury)
		{
			char msg[128];
			sprintf(msg, "We haven't got an advanced armory yet, %s. Ask again later.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}

		RequiredFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;

		bool bHasArmsLab = AITAC_DeployableExistsAtLocation(ZERO_VECTOR, &RequiredFilter);

		if (!bHasArmsLab)
		{
			char msg[128];
			sprintf(msg, "We haven't got an arms lab yet, %s. Ask again later.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}
	}


	float RequiredRes = 0.0f;
	AvHAIDeployableStructureType StructureToDeploy = STRUCTURE_NONE;

	switch (NextRequest->RequestType)
	{
	case BUILD_ARMORY:
		RequiredRes = BALANCE_VAR(kArmoryCost);
		StructureToDeploy = STRUCTURE_MARINE_ARMOURY;
		break;
	case BUILD_COMMANDSTATION:
		RequiredRes = BALANCE_VAR(kCommandStationCost);
		StructureToDeploy = STRUCTURE_MARINE_COMMCHAIR;
		break;
	case BUILD_OBSERVATORY:
		RequiredRes = BALANCE_VAR(kObservatoryCost);
		StructureToDeploy = STRUCTURE_MARINE_OBSERVATORY;
		break;
	case BUILD_TURRET_FACTORY:
		RequiredRes = BALANCE_VAR(kTurretFactoryCost);
		StructureToDeploy = STRUCTURE_MARINE_TURRETFACTORY;
		break;
	case BUILD_TURRET:
		RequiredRes = BALANCE_VAR(kSentryCost);
		StructureToDeploy = STRUCTURE_MARINE_TURRET;
		break;
	case BUILD_PHASEGATE:
		RequiredRes = BALANCE_VAR(kPhaseGateCost);
		StructureToDeploy = STRUCTURE_MARINE_PHASEGATE;
		break;
	case BUILD_INFANTRYPORTAL:
		RequiredRes = BALANCE_VAR(kInfantryPortalCost);
		StructureToDeploy = STRUCTURE_MARINE_INFANTRYPORTAL;
		break;
	case BUILD_PROTOTYPE_LAB:
		RequiredRes = BALANCE_VAR(kPrototypeLabCost);
		StructureToDeploy = STRUCTURE_MARINE_PROTOTYPELAB;
		break;
	case BUILD_ARMSLAB:
		RequiredRes = BALANCE_VAR(kArmsLabCost);
		StructureToDeploy = STRUCTURE_MARINE_ARMSLAB;
		break;
	default:
		break;
	}

	// Invalid commander request
	if (StructureToDeploy == STRUCTURE_NONE)
	{
		NextRequest->bResponded = true;
		return false;
	}

	if (pBot->Player->GetResources() < RequiredRes)
	{
		if (!NextRequest->bAcknowledged)
		{
			char msg[128];
			sprintf(msg, "Just waiting on resources, %s. Will drop asap.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bAcknowledged = true;
			return false;
		}
		return false;
	}

	float ProjectDistance = (NextRequest->RequestType == BUILD_INFANTRYPORTAL || NextRequest->RequestType == BUILD_SIEGE) ? 32.0f : 75.0f;

	Vector IdealDeployLocation = Requestor->v.origin + (UTIL_GetForwardVector2D(Requestor->v.angles) * ProjectDistance);
	Vector ProjectedDeployLocation = AdjustPointForPathfinding(IdealDeployLocation, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

	AvHAIMarineBase* BaseToDeployIn = nullptr;
	float MinDist = 0.0f;

	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
	{
		float DistFromBase = vDist2DSq(Requestor->v.origin, it->BaseLocation);

		if (it->BaseType == MARINE_BASE_MAINBASE && DistFromBase < sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			BaseToDeployIn = &(*it);
			break;
		}

		float DesiredDist = (it->BaseType == MARINE_BASE_GUARDPOST) ? BALANCE_VAR(kTurretFactoryBuildDistance) : UTIL_MetresToGoldSrcUnits(15.0f);

		if (DistFromBase < sqrf(DesiredDist))
		{
			if (!BaseToDeployIn || DistFromBase < MinDist)
			{
				BaseToDeployIn = &(*it);
				MinDist = DistFromBase;
			}
		}
	}

	// We've requested a building away from any existing bases the commander is building
	if (!BaseToDeployIn && (NextRequest->RequestType == BUILD_PHASEGATE || NextRequest->RequestType == BUILD_TURRET_FACTORY))
	{
		
		const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(Requestor->v.origin);
		float DesiredDistance = (NearestHive->Status != HIVE_STATUS_UNBUILT) ? sqrf(BALANCE_VAR(kSiegeTurretRange)) : sqrf(UTIL_MetresToGoldSrcUnits(10.0f));
		float DistFromHive = vDist2DSq(NearestHive->Location, Requestor->v.origin);

		if (NearestHive && DistFromHive < DesiredDistance)
		{
			AvHAIMarineBase* ExistingBase = nullptr;
				
			for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
			{
				if (!it->bIsActive || !it->bRecycleBase) { continue; }

				if (NearestHive->Status != HIVE_STATUS_UNBUILT)
				{
					if (it->BaseType == MARINE_BASE_SIEGE && vEquals2D(it->SiegeTarget, NearestHive->Location))
					{
						if (!it->bBaseInitialised)
						{
							BaseToDeployIn = &(*it);
							BaseToDeployIn->BaseLocation = Requestor->v.origin;
							break;
						}
					}
				}
				else
				{
					if (it->BaseType == MARINE_BASE_OUTPOST && vDist2DSq(it->BaseLocation, NearestHive->FloorLocation) < UTIL_MetresToGoldSrcUnits(15.0f))
					{
						if (!it->bBaseInitialised)
						{
							BaseToDeployIn = &(*it);
							break;
						}
					}
				}

			}

			if (!BaseToDeployIn && NearestHive)
			{
				MarineBaseType NewBaseType = (NearestHive->Status != HIVE_STATUS_UNBUILT) ? MARINE_BASE_SIEGE : MARINE_BASE_OUTPOST;

				BaseToDeployIn = AICOMM_AddNewBase(pBot, Requestor->v.origin, NewBaseType);
			}
		}
		else
		{
			BaseToDeployIn = AICOMM_AddNewBase(pBot, Requestor->v.origin, MARINE_BASE_GUARDPOST);
		}


	}

	if (!vIsZero(ProjectedDeployLocation))
	{
		if (NextRequest->RequestType == BUILD_INFANTRYPORTAL)
		{
			DeployableSearchFilter CCFilter;
			CCFilter.DeployableTeam = CommanderTeam;
			CCFilter.DeployableTypes = STRUCTURE_MARINE_COMMCHAIR;
			CCFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			CCFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			CCFilter.MaxSearchRadius = BALANCE_VAR(kCommandStationBuildDistance);

			bool bHasChair = AITAC_DeployableExistsAtLocation(ProjectedDeployLocation, &CCFilter);

			if (!bHasChair)
			{
				char msg[128];
				sprintf(msg, "There isn't a comm chair there, %s. Ask again next to the CC", STRING(Requestor->v.netname));
				BotSay(pBot, true, 0.5f, msg);
				NextRequest->bResponded = true;
				return false;
			}
		}

		if (NextRequest->RequestType == BUILD_SIEGE || NextRequest->RequestType == BUILD_TURRET)
		{
			DeployableSearchFilter TFFilter;
			TFFilter.DeployableTeam = CommanderTeam;
			TFFilter.DeployableTypes = (NextRequest->RequestType == BUILD_SIEGE) ? STRUCTURE_MARINE_ADVTURRETFACTORY : STRUCTURE_MARINE_TURRETFACTORY;
			TFFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			TFFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			TFFilter.MaxSearchRadius = BALANCE_VAR(kTurretFactoryBuildDistance);

			bool bHasTF = AITAC_DeployableExistsAtLocation(ProjectedDeployLocation, &TFFilter);

			if (!bHasTF)
			{
				char msg[128];

				if (NextRequest->RequestType == BUILD_SIEGE)
				{
					sprintf(msg, "There isn't an advanced turret factory in range, %s. Ask again near one", STRING(Requestor->v.netname));
				}
				else
				{
					sprintf(msg, "There isn't a completed turret factory in range, %s. Ask again near one", STRING(Requestor->v.netname));
				}
				BotSay(pBot, true, 0.5f, msg);
				NextRequest->bResponded = true;
				return false;
			}
		}

		bool bSuccess = false;

		if (BaseToDeployIn)
		{
			bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, ProjectedDeployLocation, BaseToDeployIn);
		}
		else
		{
			AvHAIBuildableStructure* DeployedStructure = AICOMM_DeployStructure(pBot, StructureToDeploy, ProjectedDeployLocation, STRUCTURE_PURPOSE_GENERAL, true);

			bSuccess = DeployedStructure != nullptr;
		}
		
		if (bSuccess)
		{
			NextRequest->bResponded = true;
			return true;
		}
	}

	Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

	if (!vIsZero(DeployLocation))
	{
		bool bSuccess = false;

		if (BaseToDeployIn)
		{
			bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, DeployLocation, BaseToDeployIn);
		}
		else
		{
			AvHAIBuildableStructure* DeployedStructure = AICOMM_DeployStructure(pBot, StructureToDeploy, DeployLocation, STRUCTURE_PURPOSE_GENERAL, true);

			bSuccess = DeployedStructure != nullptr;
		}


		if (bSuccess)
		{
			NextRequest->bResponded = true;
			return true;
		}
	}

	DeployLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

	if (!vIsZero(DeployLocation))
	{
		bool bSuccess = false;

		if (BaseToDeployIn)
		{
			bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, DeployLocation, BaseToDeployIn);
		}
		else
		{
			AvHAIBuildableStructure* DeployedStructure = AICOMM_DeployStructure(pBot, StructureToDeploy, DeployLocation, STRUCTURE_PURPOSE_GENERAL, true);

			bSuccess = DeployedStructure != nullptr;
		}


		if (bSuccess)
		{
			NextRequest->bResponded = true;
			return true;
		}
		else
		{
			char msg[128];
			sprintf(msg, "I can't find a good deploy spot, %s. Try again elsewhere.", STRING(Requestor->v.netname));
			BotSay(pBot, true, 0.5f, msg);
			NextRequest->bResponded = true;
			return false;
		}
	}
	else
	{
		char msg[128];
		sprintf(msg, "I can't find a good deploy spot, %s. Try again elsewhere.", STRING(Requestor->v.netname));
		BotSay(pBot, true, 0.5f, msg);
		NextRequest->bResponded = true;
		return false;
	}

	return false;
}

void AICOMM_PopulateBaseList(AvHAIPlayer* pBot)
{
	pBot->Bases.clear();

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	// First, figure out where our main base is. Might not be the chair we're in!

	edict_t* ActiveCommChair = AITAC_GetCommChair(BotTeam);

	// Shouldn't happen, but might as well double-check
	if (FNullEnt(ActiveCommChair)) { return; }

	edict_t* WinningCommChair = nullptr;

	Vector BaseLocation = ActiveCommChair->v.origin;

	DeployableSearchFilter CommChairsFilter;
	CommChairsFilter.DeployableTeam = BotTeam;
	CommChairsFilter.DeployableTypes = STRUCTURE_MARINE_COMMCHAIR;
	CommChairsFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	vector<AvHAIBuildableStructure> AllCommChairs = AITAC_FindAllDeployables(ZERO_VECTOR, &CommChairsFilter);

	int MaxInfPortals = 0;

	// Basically, find all comm chairs in the map, and whichever one has the most infantry portals around it, that's the base
	for (auto it = AllCommChairs.begin(); it != AllCommChairs.end(); it++)
	{
		DeployableSearchFilter InfPortalFilter;
		InfPortalFilter.DeployableTeam = BotTeam;
		InfPortalFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
		InfPortalFilter.MaxSearchRadius = BALANCE_VAR(kCommandStationBuildDistance);

		int ThisNumInfPortals = AITAC_GetNumDeployablesNearLocation(it->Location, &InfPortalFilter);

		if (ThisNumInfPortals > MaxInfPortals)
		{
			WinningCommChair = it->edict;
			MaxInfPortals = ThisNumInfPortals;
		}
	}

	// None of the comm chairs have any infantry portals, therefore the chair we're in is (for now) where our base will be
	if (FNullEnt(WinningCommChair))
	{
		WinningCommChair = ActiveCommChair;
	}

	// Add an entry for the base now
	AvHAIMarineBase* MainBase = AICOMM_AddNewBase(pBot, WinningCommChair->v.origin, MARINE_BASE_MAINBASE);

	if (!MainBase) { return; }

	// Get all structures and start adding them to our base

	DeployableSearchFilter AllStructuresFilter;
	AllStructuresFilter.DeployableTeam = BotTeam;
	AllStructuresFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	AllStructuresFilter.DeployableTypes = (SEARCH_ALL_STRUCTURES & ~(STRUCTURE_MARINE_RESTOWER)); // Don't include resource towers in this list

	vector<AvHAIBuildableStructure> AllTeamStructures = AITAC_FindAllDeployables(ZERO_VECTOR, &AllStructuresFilter);

	for (auto it = AllTeamStructures.begin(); it != AllTeamStructures.end();)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		// Obviously make sure the comm chair is part of the base
		if (ThisStructure.edict == WinningCommChair)
		{
			MainBase->PlacedStructures.push_back(ThisStructure.EntIndex);
			it = AllTeamStructures.erase(it);
			continue;
		}

		// All structures within a 15m radius of the comm chair can be considered part of the main base
		if (vDist2DSq(ThisStructure.Location, MainBase->BaseLocation) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
		{
			MainBase->PlacedStructures.push_back(ThisStructure.EntIndex);
			it = AllTeamStructures.erase(it);
			continue;
		}

		it++;
	}

	// Get all remaining phase gates not in the main base
	vector<AvHAIBuildableStructure> AllPhaseGates;

	for (auto it = AllTeamStructures.begin(); it != AllTeamStructures.end();)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		if (ThisStructure.StructureType == STRUCTURE_MARINE_PHASEGATE)
		{
			AllPhaseGates.push_back(ThisStructure);
			it = AllTeamStructures.erase(it);
		}
		else
		{
			it++;
		}
	}

	for (auto it = AllPhaseGates.begin(); it != AllPhaseGates.end(); it++)
	{
		AvHAIBuildableStructure ThisPG = (*it);

		AvHAIMarineBase* NewOutpost = AICOMM_AddNewBase(pBot, ThisPG.Location, MARINE_BASE_OUTPOST);

		if (!NewOutpost) { continue; }

		NewOutpost->PlacedStructures.push_back(ThisPG.EntIndex);

		bool bHasSiegeTurrets = false;

		for (auto allStructIt = AllTeamStructures.begin(); allStructIt != AllTeamStructures.end();)
		{
			AvHAIBuildableStructure ThisStructure = (*allStructIt);

			// All structures within a 10m radius of the phase gate can be considered part of the outpost or siege base
			if (vDist2DSq(ThisStructure.Location, NewOutpost->BaseLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				NewOutpost->PlacedStructures.push_back(ThisStructure.EntIndex);

				if (ThisStructure.StructureType == STRUCTURE_MARINE_SIEGETURRET)
				{
					bHasSiegeTurrets = true;
				}

				// Remove these structures from the list
				allStructIt = AllTeamStructures.erase(allStructIt);

				continue;
			}

			allStructIt++;
		}

		// Determine if the new outpost should be a siege base or not. Bot will assume it's a siege base if:
		// It has siege turrets (obviously)
		// It is in siege range of a hive which is either owned by an enemy alien team, or has been colonised by an enemy marine team

		if (bHasSiegeTurrets)
		{
			NewOutpost->BaseType = MARINE_BASE_SIEGE;
		}
		else
		{
			const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(ThisPG.Location);

			if (NearestHive && vDist2DSq(NearestHive->Location, ThisPG.Location) <= sqrf(UTIL_MetresToGoldSrcUnits(25.0f)))
			{
				if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
				{
					if (NearestHive->Status != HIVE_STATUS_UNBUILT)
					{
						NewOutpost->BaseType = MARINE_BASE_SIEGE;
					}
				}
				else
				{
					DeployableSearchFilter EnemyStuffFilter;
					EnemyStuffFilter.DeployableTeam = EnemyTeam;
					EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
					EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
					EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

					if (AITAC_GetNumDeployablesNearLocation(NearestHive->FloorLocation, &EnemyStuffFilter) > 0)
					{
						NewOutpost->BaseType = MARINE_BASE_SIEGE;
					}
				}
			}
		}
	}

	// All that is left now in AllTeamStructures are any structures not yet considered part of the main base, an outpost or siege base

	// Get all turret factories left
	vector<AvHAIBuildableStructure> AllRemainingTFs;

	for (auto it = AllTeamStructures.begin(); it != AllTeamStructures.end();)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		if (ThisStructure.StructureType & (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY))
		{
			AllRemainingTFs.push_back(ThisStructure);
			it = AllTeamStructures.erase(it);
		}
		else
		{
			it++;
		}
	}

	//Loop through all the turret factories. These will either be guard posts, or siege bases without a phase gate

	for (auto it = AllRemainingTFs.begin(); it != AllRemainingTFs.end(); it++)
	{
		AvHAIBuildableStructure ThisTF = (*it);

		MarineBaseType NewBaseType = MARINE_BASE_GUARDPOST;

		if (vDist2DSq(ThisTF.Location, AITAC_GetTeamOriginalStartLocation(BotTeam)) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			NewBaseType = MARINE_BASE_OUTPOST;
		}

		AvHAIMarineBase* NewOutpost = AICOMM_AddNewBase(pBot, ThisTF.Location, NewBaseType);

		if (!NewOutpost) { continue; }

		NewOutpost->PlacedStructures.push_back(ThisTF.EntIndex);

		bool bHasSiegeTurrets = false;

		for (auto allStructIt = AllTeamStructures.begin(); allStructIt != AllTeamStructures.end();)
		{
			AvHAIBuildableStructure ThisStructure = (*allStructIt);

			// All structures within a 10m radius of the phase gate can be considered part of the outpost or siege base
			if (vDist2DSq(ThisStructure.Location, NewOutpost->BaseLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				NewOutpost->PlacedStructures.push_back(ThisStructure.EntIndex);

				if (ThisStructure.StructureType == STRUCTURE_MARINE_SIEGETURRET)
				{
					bHasSiegeTurrets = true;
				}

				// Remove these structures from the list
				allStructIt = AllTeamStructures.erase(allStructIt);

				continue;
			}

			allStructIt++;
		}

		if (ThisTF.StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
		{
			if (bHasSiegeTurrets)
			{
				NewOutpost->BaseType = MARINE_BASE_SIEGE;
			}
			else
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(ThisTF.Location);

				float DistToHive = vDist2DSq(NearestHive->Location, ThisTF.Location);

				if (NearestHive && DistToHive <= sqrf(UTIL_MetresToGoldSrcUnits(25.0f)))
				{
					if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
					{
						// Player was sieging a hive
						if (NearestHive->Status != HIVE_STATUS_UNBUILT)
						{
							NewOutpost->BaseType = MARINE_BASE_SIEGE;
						}
						else
						{
							// This is an outpost in an empty hive
							if (DistToHive <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
							{
								NewOutpost->BaseType = MARINE_BASE_OUTPOST;
							}
						}
					}
					else
					{
						DeployableSearchFilter EnemyStuffFilter;
						EnemyStuffFilter.DeployableTeam = EnemyTeam;
						EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
						EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
						EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

						// Player was sieging a bunch of stuff
						if (AITAC_GetNumDeployablesNearLocation(NearestHive->FloorLocation, &EnemyStuffFilter) > 0)
						{
							NewOutpost->BaseType = MARINE_BASE_SIEGE;
						}
					}
				}
			}
		}
	}


}

void AICOMM_CommanderThink(AvHAIPlayer* pBot)
{
	if (IsPlayerCommander(pBot->Edict))
	{
		if (AICOMM_ShouldBeacon(pBot))
		{
			return;
		}
	}

	if (!BuildMessageAnnounced)
	{
		BuildMessageAnnounced = true;
		if (AIBO_CurrentBuildOrder)
		{
			std::string BuildOrderMessage = GetBuildOrderMessage();
			char BuildOrderMessageBuffer[256];
			strncpy(BuildOrderMessageBuffer, BuildOrderMessage.c_str(), sizeof(BuildOrderMessageBuffer) - 1);
			BuildOrderMessageBuffer[sizeof(BuildOrderMessageBuffer) - 1] = '\0';
			BotSay(pBot, false, 0.5f, BuildOrderMessageBuffer);
		}
	}

	// Thanks to EterniumDev (Alien) for the suggestion to have the commander jump out and build if nobody is around to help
	if (AICOMM_ShouldCommanderLeaveChair(pBot))
	{
		if (IsPlayerCommander(pBot->Edict))
		{
			BotStopCommanderMode(pBot);
			return;
		}

		DeployableSearchFilter StructureFilter;
		StructureFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
		StructureFilter.DeployableTeam = pBot->Player->GetTeam();
		StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
		StructureFilter.ReachabilityTeam = pBot->Player->GetTeam();
		StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_COMPLETED | STRUCTURE_STATUS_RECYCLING;

		AvHAIBuildableStructure NearestUnbuiltStructure = AITAC_FindClosestDeployableToLocation(AITAC_GetCommChairLocation(pBot->Player->GetTeam()), &StructureFilter);

		if (NearestUnbuiltStructure.IsValid())
		{
			AITASK_SetBuildTask(pBot, &pBot->PrimaryBotTask, NearestUnbuiltStructure.edict, false);
		}

		BotProgressTask(pBot, &pBot->PrimaryBotTask);

		return;
	}

	if (!IsPlayerCommander(pBot->Edict))
	{
		BotProgressTakeCommandTask(pBot);
		return;
	}

	if (gpGlobals->time < pBot->next_commander_action_time) { return; }

	if (pBot->Bases.size() == 0)
	{
		AICOMM_PopulateBaseList(pBot);
	}

	AICOMM_ManageActiveBases(pBot);
	AICOMM_DeployBases(pBot);

	AICOMM_UpdatePlayerOrders(pBot);

	if (AICOMM_CheckForNextSupportAction(pBot)) { return; }
	if (AICOMM_CheckForNextRecycleAction(pBot)) { return; }	
	if (AICOMM_CheckForNextBuildAction(pBot)) { return; }
	if (AICOMM_CheckForNextResearchAction(pBot)) { return; }
	if (AICOMM_CheckForNextSupplyAction(pBot)) { return; }
}

bool AICOMM_IsMarineBaseValid(AvHAIMarineBase* Base)
{
	if (Base->PlacedStructures.size() > 0) { return true; }

	if (!Base->bBaseInitialised && !Base->bIsActive) { return false; }

	if ((Base->bRecycleBase || Base->bBaseInitialised) && Base->PlacedStructures.size() == 0) { return false; }

	return true;
}

void AICOMM_UpdateBaseStatus(AvHAIPlayer* pBot, AvHAIMarineBase* Base)
{
	if (!pBot || !Base) { return; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	// TODO: Add check if base is doomed

	float BuildRadius = UTIL_MetresToGoldSrcUnits(10.0f);
	float EnemyRadius = UTIL_MetresToGoldSrcUnits(10.0f);

	switch (Base->BaseType)
	{
		case MARINE_BASE_SIEGE:
			AICOMM_UpdateSiegeBaseStatus(pBot, Base);
			BuildRadius = UTIL_MetresToGoldSrcUnits(10.0f);
			EnemyRadius = UTIL_MetresToGoldSrcUnits(5.0f);
			break;
		case MARINE_BASE_GUARDPOST:
			AICOMM_UpdateGuardpostStatus(pBot, Base);
			BuildRadius = UTIL_MetresToGoldSrcUnits(5.0f);
			EnemyRadius = UTIL_MetresToGoldSrcUnits(5.0f);
			break;
		case MARINE_BASE_OUTPOST:
			AICOMM_UpdateOutpostStatus(pBot, Base);
			BuildRadius = UTIL_MetresToGoldSrcUnits(10.0f);
			EnemyRadius = UTIL_MetresToGoldSrcUnits(10.0f);
			break;
		case MARINE_BASE_MAINBASE:
			AICOMM_UpdateMainBaseStatus(pBot, Base);
			BuildRadius = UTIL_MetresToGoldSrcUnits(15.0f);
			break;
		default:
			break;
	}	

	if (!Base->bRecycleBase && Base->bIsActive)
	{
		Base->bCanBeBuiltOut = AITAC_CanBuildOutBase(Base);
		if (Base->bCanBeBuiltOut)
		{
			Base->NumBuilders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, Base->BaseLocation, BuildRadius, false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);
			Base->NumEnemies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, Base->BaseLocation + Vector(0.0f, 0.0f, 16.0f), EnemyRadius, nullptr);
		}
	}

}

vector<AvHAIBuildableStructure> AICOMM_GetBaseStructures(AvHAIMarineBase* Base)
{
	vector<AvHAIBuildableStructure> Result;

	for (auto it = Base->PlacedStructures.begin(); it != Base->PlacedStructures.end(); it++)
	{
		AvHAIBuildableStructure ThisStructure = AITAC_GetDeployableStructureByEntIndex(Base->BaseTeam, (*it));

		Result.push_back(ThisStructure);
	}

	return Result;
}

void AICOMM_UpdateMainBaseStatus(AvHAIPlayer* pBot, AvHAIMarineBase* Base)
{
	// If this main base location is far from the active comm chair we're in, then we must be relocating
	// In that case, if we've not yet established the base and the enemy have already started building fortifications there, then abandon the base.
	if (!Base->bIsBaseEstablished && vDist2DSq(Base->BaseLocation, AITAC_GetCommChairLocation(pBot->Player->GetTeam())) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		DeployableSearchFilter EnemyStuffFilter;
		EnemyStuffFilter.DeployableTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());
		EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_COMMCHAIR | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_PHASEGATE | STRUCTURE_ALIEN_OFFENCECHAMBER);
		EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

		if (AITAC_DeployableExistsAtLocation(Base->BaseLocation, &EnemyStuffFilter))
		{
			Base->bRecycleBase = true;
			Base->bIsActive = false;
			return;
		}
	}

	// Either the enemy haven't started fortifying this area, or this base is where our active comm chair is. Either way, we need to crack on
	Base->bRecycleBase = false;
	Base->bIsActive = true;

	// Check how far we are into building this outpost. If we have a TF and at least 3 sentries then we can consider it "established"
	// even if it isn't finished yet
	vector<AvHAIBuildableStructure> BaseStructures = AICOMM_GetBaseStructures(Base);

	bool bHasCommChair = true;
	int NumInfPortals = 0;

	for (auto structIt = BaseStructures.begin(); structIt != BaseStructures.end(); structIt++)
	{
		AvHAIBuildableStructure ThisStructure = (*structIt);

		if (ThisStructure.StructureType == STRUCTURE_MARINE_COMMCHAIR) { bHasCommChair = true; }
		if (ThisStructure.StructureType == STRUCTURE_MARINE_INFANTRYPORTAL && (ThisStructure.StructureStatusFlags & STRUCTURE_STATUS_COMPLETED)) { NumInfPortals++; }
	}

	Base->bIsBaseEstablished = bHasCommChair && NumInfPortals > 0;
}

void AICOMM_UpdateGuardpostStatus(AvHAIPlayer* pBot, AvHAIMarineBase* Base)
{
	// If we've not started building this base and the enemy are already securing this area then don't start building it out
	if (!Base->bBaseInitialised)
	{
		DeployableSearchFilter EnemyStuffFilter;
		EnemyStuffFilter.DeployableTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());
		EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_COMMCHAIR | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_PHASEGATE | STRUCTURE_ALIEN_OFFENCECHAMBER);
		EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

		if (AITAC_DeployableExistsAtLocation(Base->BaseLocation, &EnemyStuffFilter))
		{
			Base->bRecycleBase = false;
			Base->bIsActive = false;
			return;
		}
	}
	
	Base->bRecycleBase = false;
	Base->bIsActive = true;
	
	// Check how far we are into building this outpost. If we have a TF and at least 3 sentries then we can consider it "established"
	// even if it isn't finished yet
	vector<AvHAIBuildableStructure> BaseStructures = AICOMM_GetBaseStructures(Base);

	bool bHasTF = true;
	int NumSentries = 0;

	for (auto structIt = BaseStructures.begin(); structIt != BaseStructures.end(); structIt++)
	{
		AvHAIBuildableStructure ThisStructure = (*structIt);

		if (ThisStructure.StructureType & (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY)) { bHasTF = true; }
		if (ThisStructure.StructureType == STRUCTURE_MARINE_TURRET) { NumSentries++; }
	}

	Base->bIsBaseEstablished = bHasTF && NumSentries > 2;
}

void AICOMM_UpdateOutpostStatus(AvHAIPlayer* pBot, AvHAIMarineBase* Base)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(Base->BaseLocation);

	// Recycle this base if it's meant to be securing a hive and that hive is not inactive any more
	// We will replace with a siege base outside the hive
	if (NearestHive && vDist2DSq(NearestHive->FloorLocation, Base->BaseLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		if (NearestHive->Status != HIVE_STATUS_UNBUILT)
		{
			Base->bIsActive = false;
			Base->bRecycleBase = true;
			return;
		}
	}

	// If we've not started building this base and the enemy are already securing this area then don't start building it out
	if (!Base->bBaseInitialised)
	{
		DeployableSearchFilter EnemyStuffFilter;
		EnemyStuffFilter.DeployableTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());
		EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_COMMCHAIR | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_PHASEGATE | STRUCTURE_ALIEN_OFFENCECHAMBER);
		EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

		if (AITAC_DeployableExistsAtLocation(Base->BaseLocation, &EnemyStuffFilter))
		{
			Base->bRecycleBase = false;
			Base->bIsActive = false;
			return;
		}
	}

	// Check how far we are into building this outpost. If we have a PG (assuming phase tech researched), a TF and at least 3 sentries then we can consider it "established"
	// even if it isn't finished yet
	vector<AvHAIBuildableStructure> BaseStructures = AICOMM_GetBaseStructures(Base);

	bool bHasPG = true;
	bool bHasTF = true;
	int NumSentries = 0;

	for (auto structIt = BaseStructures.begin(); structIt != BaseStructures.end(); structIt++)
	{
		AvHAIBuildableStructure ThisStructure = (*structIt);

		if (ThisStructure.StructureType == STRUCTURE_MARINE_PHASEGATE) { bHasPG = true; }
		if (ThisStructure.StructureType & (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY)) { bHasTF = true; }
		if (ThisStructure.StructureType == STRUCTURE_MARINE_TURRET) { NumSentries++; }
	}

	Base->bIsBaseEstablished = (bHasPG || !AITAC_ResearchIsComplete(BotTeam, TECH_RESEARCH_PHASETECH)) && bHasTF && NumSentries > 2;

	Base->bIsActive = true;
	Base->bRecycleBase = false;
}

void AICOMM_UpdateSiegeBaseStatus(AvHAIPlayer* pBot, AvHAIMarineBase* Base)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	DeployableSearchFilter EnemyStuffFilter;
	EnemyStuffFilter.DeployableTeam = EnemyTeam;
	EnemyStuffFilter.MaxSearchRadius = BALANCE_VAR(kSiegeTurretRange);

	bool bStillStuffToSiege = AITAC_DeployableExistsAtLocation(Base->BaseLocation, &EnemyStuffFilter);

	const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(Base->BaseLocation);
	const AvHAIHiveDefinition* SiegeHive = nullptr;

	if (vDist2DSq(Base->BaseLocation, NearestHive->Location) < sqrf(BALANCE_VAR(kSiegeTurretRange)))
	{
		SiegeHive = NearestHive;
	}

	if (bStillStuffToSiege || (SiegeHive && SiegeHive->Status != HIVE_STATUS_UNBUILT))
	{
		if (SiegeHive && SiegeHive->Status != HIVE_STATUS_UNBUILT)
		{
			Base->SiegeTarget = SiegeHive->Location;
		}
		else
		{
			// Basically, find the centroid (average location) of all siege structures, that will be our target
			vector<AvHAIBuildableStructure> NearbyEnemyStuff = AITAC_FindAllDeployables(Base->BaseLocation, &EnemyStuffFilter);

			Vector Centroid = ZERO_VECTOR;

			for (auto it = NearbyEnemyStuff.begin(); it != NearbyEnemyStuff.end(); it++)
			{
				Centroid = Centroid + it->Location;
			}

			Centroid = Centroid / NearbyEnemyStuff.size();

			Base->SiegeTarget = Centroid;
		}
		Base->bRecycleBase = false;
		Base->bIsActive = true;
		return;
	}
	else
	{
		if (!SiegeHive)
		{
			Base->bRecycleBase = false;
			Base->bIsActive = false;
			return;
		}
		else
		{
			for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
			{
				if (it->BaseType == MARINE_BASE_OUTPOST && vDist2DSq(SiegeHive->FloorLocation, it->BaseLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
				{
					if (it->bIsBaseEstablished)
					{
						Base->bRecycleBase = true;
						Base->bIsActive = false;
						return;
					}
				}
			}

			Base->bRecycleBase = false;
			Base->bIsActive = false;
			return;
		}
	}

	Base->SiegeTarget = ZERO_VECTOR;
	Base->bRecycleBase = true;
	Base->bIsActive = false;
}

void AICOMM_DeployBases(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	Vector CurrentChairLocation = AITAC_GetCommChairLocation(BotTeam);

	if (AICOMM_ShouldCommanderRelocate(pBot))
	{
		AvHAIMarineBase* CurrentMainBase = nullptr;

		for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
		{
			if (it->BaseType == MARINE_BASE_MAINBASE)
			{
				if (!CurrentMainBase || it->bIsBaseEstablished)
				{
					CurrentMainBase = &(*it);
				}
			}
		}

		if (!CurrentMainBase)
		{
			AICOMM_AddNewBase(pBot, CurrentChairLocation, MARINE_BASE_MAINBASE);
			return;
		}

		bool bMainBaseAtCommChair = vDist2DSq(CurrentMainBase->BaseLocation, CurrentChairLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f));

		if (!bMainBaseAtCommChair && !CurrentMainBase->bIsBaseEstablished && AIMGR_GetMatchLength() > 90.0f)
		{
			AvHAIMarineBase* ChairBase = AICOMM_GetNearestBaseToLocation(pBot, CurrentChairLocation);

			if (ChairBase != CurrentMainBase)
			{
				CurrentMainBase->BaseType = MARINE_BASE_OUTPOST;
				ChairBase->BaseType = MARINE_BASE_MAINBASE;

				char NewMsg[128];
				if (AICOMM_GetRelocationMessage(CurrentChairLocation, NewMsg))
				{
					BotSay(pBot, true, 1.0f, NewMsg);
				}

				return;

			}
			else
			{
				CurrentMainBase->BaseType = MARINE_BASE_OUTPOST;
				AICOMM_AddNewBase(pBot, CurrentChairLocation, MARINE_BASE_MAINBASE);				

				char NewMsg[128];
				if (AICOMM_GetRelocationMessage(CurrentChairLocation, NewMsg))
				{
					BotSay(pBot, true, 1.0f, NewMsg);
				}

				return;
			}
		}
		else
		{
			Vector NewBaseLocation = AITAC_FindNewTeamRelocationPoint(BotTeam);

			if (!vIsZero(NewBaseLocation) && vDist2DSq(NewBaseLocation, CurrentMainBase->BaseLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				// Turn our starting base into an outpost, and lay down a new main base elsewhere
				CurrentMainBase->BaseType = MARINE_BASE_OUTPOST;
				AICOMM_AddNewBase(pBot, NewBaseLocation, MARINE_BASE_MAINBASE);

				char NewMsg[128];
				if (AICOMM_GetRelocationMessage(NewBaseLocation, NewMsg))
				{
					BotSay(pBot, true, 1.0f, NewMsg);
				}
				return;
			}
			else
			{
				if (!bMainBaseAtCommChair)
				{
					AvHAIMarineBase* ChairBase = AICOMM_GetNearestBaseToLocation(pBot, CurrentChairLocation);

					if (ChairBase != CurrentMainBase)
					{
						CurrentMainBase->BaseType = MARINE_BASE_OUTPOST;
						ChairBase->BaseType = MARINE_BASE_MAINBASE;

						char NewMsg[128];
						if (AICOMM_GetRelocationMessage(CurrentChairLocation, NewMsg))
						{
							BotSay(pBot, true, 1.0f, NewMsg);
						}
						return;
					}
				}
			}
		}
	}

	vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllHives();

	bool bNeedsResources = AICOMM_ShouldCommanderPrioritiseNodes(pBot);

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		const AvHAIHiveDefinition* ThisHive = (*it);

		if (ThisHive->Status == HIVE_STATUS_UNBUILT)
		{
			bool bHasOutpost = false;

			for (auto baseIt = pBot->Bases.begin(); baseIt != pBot->Bases.end(); baseIt++)
			{
				AvHAIMarineBase* ThisBase = &(*baseIt);

				if ((ThisBase->BaseType == MARINE_BASE_OUTPOST || ThisBase->BaseType == MARINE_BASE_MAINBASE) && !ThisBase->bRecycleBase)
				{
					if (vDist2DSq(ThisHive->FloorLocation, ThisBase->BaseLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
					{
						bHasOutpost = true;
						break;
					}
				}
			}

			if (!bHasOutpost)
			{
				DeployableSearchFilter EnemyStuffFilter;
				EnemyStuffFilter.DeployableTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());
				EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
				EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
				EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_COMMCHAIR | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_PHASEGATE | STRUCTURE_ALIEN_OFFENCECHAMBER);
				EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

				if (!AITAC_DeployableExistsAtLocation(ThisHive->FloorLocation, &EnemyStuffFilter))
				{
					Vector ProjectedPoint = UTIL_ProjectPointToNavmesh(ThisHive->FloorLocation, Vector(500.0f, 500.0f, 500.0f), GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));
					if (!vIsZero(ProjectedPoint))
					{
						AICOMM_AddNewBase(pBot, ProjectedPoint, MARINE_BASE_OUTPOST);
					}
				}
			}
		}
		else
		{
			// Only siege if we can use phase gates
			// TODO: Define scenario where we can siege without using phase gates. Probably when we don't have any outposts or guardposts to build
			if (!AITAC_ResearchIsComplete(BotTeam, TECH_RESEARCH_PHASETECH)) { continue; }
			
			// Also don't try sieging if we don't have enough nodes
			if (bNeedsResources && pBot->Player->GetResources() < 50) { continue; }

			bool bHasSiege = false;

			for (auto baseIt = pBot->Bases.begin(); baseIt != pBot->Bases.end(); baseIt++)
			{
				AvHAIMarineBase* ThisBase = &(*baseIt);

				if (ThisBase->BaseType == MARINE_BASE_SIEGE)
				{
					if (vDist2DSq(ThisHive->Location, ThisBase->BaseLocation) < sqrf(BALANCE_VAR(kSiegeTurretRange)))
					{
						bHasSiege = true;
						break;
					}
				}
			}

			if (!bHasSiege)
			{
				vector<AvHPlayer*> PotentialBuilders = AITAC_GetAllPlayersOfTeamInArea(BotTeam, ThisHive->Location, BALANCE_VAR(kSiegeTurretRange), false, nullptr, AVH_USER3_COMMANDER_PLAYER);

				for (auto playerIt = PotentialBuilders.begin(); playerIt != PotentialBuilders.end(); playerIt++)
				{
					AvHPlayer* ThisPlayer = (*playerIt);
					if (vDist2DSq(ThisPlayer->pev->origin, ThisHive->Location) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
					{
						// If this player isn't right inside the hive and hasn't been spotted by an enemy, then they're eligible to start building a siege base
						if (!(ThisPlayer->pev->iuser4 & MASK_VIS_SIGHTED))
						{
							DeployableSearchFilter EnemyStuffFilter;
							EnemyStuffFilter.DeployableTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());
							EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
							EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
							EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_COMMCHAIR | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_PHASEGATE | STRUCTURE_ALIEN_OFFENCECHAMBER);
							EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

							if (!AITAC_DeployableExistsAtLocation(ThisPlayer->pev->origin, &EnemyStuffFilter))
							{
								// Make sure that our siege guy is somewhere we can build
								Vector ProjectedPoint = UTIL_ProjectPointToNavmesh(ThisPlayer->pev->origin, Vector(100.0f, 100.0f, 100.0f), GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));
								if (!vIsZero(ProjectedPoint) && vDist2DSq(ProjectedPoint, ThisHive->Location) < sqrf(BALANCE_VAR(kSiegeTurretRange)))
								{
									AICOMM_AddNewBase(pBot, ProjectedPoint, MARINE_BASE_SIEGE);
								}								
							}
						}
					}
				}
			}
		}
	}
}

void AICOMM_ManageActiveBases(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end();)
	{
		AvHAIMarineBase* ThisBase = &(*it);

		if (!ThisBase->bBaseInitialised)
		{
			ThisBase->bBaseInitialised = it->PlacedStructures.size() > 0;
		}

		for (auto structIt = it->PlacedStructures.begin(); structIt != it->PlacedStructures.end();)
		{
			AvHAIBuildableStructure StructureRef = AITAC_GetDeployableStructureByEntIndex(BotTeam, (*structIt));

			if (!StructureRef.IsValid() || (StructureRef.StructureStatusFlags & STRUCTURE_STATUS_RECYCLING))
			{
				structIt = it->PlacedStructures.erase(structIt);
			}
			else
			{
				structIt++;
			}
		}

		if (!AICOMM_IsMarineBaseValid(&(*it)))
		{
			it = pBot->Bases.erase(it);
		}
		else
		{
			AICOMM_UpdateBaseStatus(pBot, ThisBase);

			it++;
		}
	}

}

bool AICOMM_GetRelocationMessage(Vector RelocationPoint, char* MessageBuffer)
{
	const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(RelocationPoint);

	string LocationName;

	if (NearestHive && vDist2DSq(RelocationPoint, NearestHive->FloorLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		LocationName = string(NearestHive->HiveName);
	}
	else
	{
		LocationName = AITAC_GetLocationName(RelocationPoint);
	}

	if (LocationName.empty())
	{
		sprintf(MessageBuffer, "Get ready to relocate");
		return true;
	}

	int MsgIndex = irandrange(0, 2);

	switch (MsgIndex)
	{
		case 0:
			sprintf(MessageBuffer, "We're relocating to %s, lads", LocationName.c_str());
			return true;
		case 1:
			sprintf(MessageBuffer, "Relocate to %s, go go go", LocationName.c_str());
			return true;
		case 2:
			sprintf(MessageBuffer, "I'm relocating to %s", LocationName.c_str());
			return true;
		default:
			sprintf(MessageBuffer, "We're relocating, get ready");
			return true;
	}
	
	return false;
}

bool AICOMM_ShouldCommanderLeaveChair(AvHAIPlayer* pBot)
{
	if (pBot->BotRole != BOT_ROLE_COMMAND) { return true; }

	if (AIMGR_GetCommanderMode() == COMMANDERMODE_DISABLED) { return true; }

	if (AIMGR_GetCommanderMode() == COMMANDERMODE_IFNOHUMAN)
	{
		if (AIMGR_GetNumHumanPlayersOnTeam(pBot->Player->GetTeam()) > 0) { return true;}
	}

	if (IsPlayerCommander(pBot->Edict) && AIMGR_GetCommanderAllowedTime(pBot->Player->GetTeam()) > gpGlobals->time) { return true; }

	int NumAliveMarinesInBase = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), AITAC_GetCommChairLocation(pBot->Player->GetTeam()), UTIL_MetresToGoldSrcUnits(30.0f), true, pBot->Edict, AVH_USER3_NONE);

	if (NumAliveMarinesInBase > 0) { return false; }

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
	StructureFilter.DeployableTeam = pBot->Player->GetTeam();
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	StructureFilter.ReachabilityTeam = pBot->Player->GetTeam();
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_COMPLETED | STRUCTURE_STATUS_RECYCLING;

	int NumUnbuiltStructuresInBase = AITAC_GetNumDeployablesNearLocation(AITAC_GetCommChairLocation(pBot->Player->GetTeam()), &StructureFilter);

	if (NumUnbuiltStructuresInBase == 0) { return false; }

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;

	int NumInfantryPortals = AITAC_GetNumDeployablesNearLocation(AITAC_GetCommChairLocation(pBot->Player->GetTeam()), &StructureFilter);

	if (NumInfantryPortals == 0) { return true; }

	if (AITAC_GetNumDeadPlayersOnTeam(pBot->Player->GetTeam()) == 0) { return true; }

	return false;
}

const AvHAIHiveDefinition* AICOMM_GetEmptyHiveOpportunityNearestLocation(AvHAIPlayer* CommanderBot, const Vector SearchLocation)
{
	AvHTeamNumber CommanderTeam = CommanderBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(CommanderTeam);

	const AvHAIHiveDefinition* Result = nullptr;
	float MinDist = 0.0f;

	const vector<AvHAIHiveDefinition*> Hives = AITAC_GetAllHives();

	Vector TeamStartLocation = AITAC_GetTeamStartingLocation(CommanderTeam);
	Vector RelocationPoint = CommanderBot->RelocationSpot;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		const AvHAIHiveDefinition* Hive = (*it);

		if (Hive->Status != HIVE_STATUS_UNBUILT) { continue; }

		if (AICOMM_IsHiveFullySecured(CommanderBot, Hive, false)) { continue; }

		// If the hive is close enough that we could siege it from our base, then don't bother securing it
		if (vDist2DSq(TeamStartLocation, Hive->FloorLocation) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f))) { continue; }

		// If the hive is close enough that we could siege it from our base, then don't bother securing it
		if (!vIsZero(CommanderBot->RelocationSpot))
		{
			// Likewise, don't secure if we're planning to move there. We will build the base instead
			if (vDist2DSq(CommanderBot->RelocationSpot, Hive->FloorLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { continue; }
		}

		Vector SecureLocation = Hive->FloorLocation;

		DeployableSearchFilter StructureFilter;
		StructureFilter.DeployableTeam = CommanderTeam;
		StructureFilter.ReachabilityTeam = CommanderTeam;
		StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
		StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		AvHAIBuildableStructure ExistingStructure = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &StructureFilter);

		if (ExistingStructure.IsValid() && (ExistingStructure.Purpose == STRUCTURE_PURPOSE_FORTIFY || ExistingStructure.Purpose == STRUCTURE_PURPOSE_BASE))
		{
			SecureLocation = ExistingStructure.Location;
		}

		float MarineDist = (ExistingStructure.IsValid()) ? UTIL_MetresToGoldSrcUnits(5.0f) : UTIL_MetresToGoldSrcUnits(10.0f);

		if (AITAC_GetNearestHiddenPlayerInLocation(CommanderTeam, SecureLocation, MarineDist) == nullptr) { continue; }

		int NumEnemiesNearby = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, SecureLocation + Vector(0.0f, 0.0f, 10.0f), UTIL_MetresToGoldSrcUnits(15.0f), nullptr);

		if (NumEnemiesNearby > 0) { continue; }

		DeployableSearchFilter EnemyStuff;
		EnemyStuff.DeployableTeam = EnemyTeam;
		EnemyStuff.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		EnemyStuff.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		EnemyStuff.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

		if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE)
		{
			EnemyStuff.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
		}
		else
		{
			EnemyStuff.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;
		}

		if (AITAC_DeployableExistsAtLocation(SecureLocation, &EnemyStuff)) { continue; }

		float ThisDist = vDist2DSq(Hive->FloorLocation, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = Hive;
			MinDist = ThisDist;
		}
	}


	return Result;
}

bool AICOMM_IsHiveFullySecured(AvHAIPlayer* CommanderBot, const AvHAIHiveDefinition* Hive, bool bIncludeElectrical)
{
	AvHTeamNumber CommanderTeam = CommanderBot->Player->GetTeam();

	bool bPhaseGatesAvailable = AITAC_PhaseGatesAvailable(CommanderTeam);

	bool bHasPhaseGate = false;
	bool bHasTurretFactory = false;
	bool bTurretFactoryElectrified = false;
	int NumTurrets = 0;

	DeployableSearchFilter SearchFilter;
	SearchFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
	SearchFilter.DeployableTeam = CommanderTeam;
	SearchFilter.ReachabilityTeam = CommanderTeam;
	SearchFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	SearchFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	SearchFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	SearchFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	vector<AvHAIBuildableStructure> HiveStructures = AITAC_FindAllDeployables(Hive->FloorLocation, &SearchFilter);

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
			bTurretFactoryElectrified = (Structure.StructureStatusFlags & STRUCTURE_STATUS_ELECTRIFIED);

			SearchFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;
			SearchFilter.MaxSearchRadius = BALANCE_VAR(kTurretFactoryBuildDistance);

			NumTurrets = AITAC_GetNumDeployablesNearLocation(Structure.Location, &SearchFilter);

		}

	}

	const AvHAIResourceNode* ResNode = Hive->HiveResNodeRef;

	bool bSecuredResNode = (!ResNode || (ResNode->bIsOccupied && ResNode->OwningTeam == CommanderTeam && UTIL_StructureIsFullyBuilt(ResNode->ActiveTowerEntity)));

	return ((!bPhaseGatesAvailable || bHasPhaseGate) && bHasTurretFactory && (!bIncludeElectrical || bTurretFactoryElectrified) && NumTurrets >= 5 && bSecuredResNode);
}

bool AICOMM_IsMainBaseInTrouble(AvHAIPlayer* pBot, AvHAIMarineBase* Base)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	Vector BaseLocation = AITAC_GetTeamStartingLocation(BotTeam);

	vector<AvHAIBuildableStructure> BaseStructures = AICOMM_GetBaseStructures(Base);

	bool bHasInfantryPortals = false;
	bool bCriticalStructureUnderAttack = false;
	bool bAnyStructureUnderAttack = false;

	for (auto it = BaseStructures.begin(); it != BaseStructures.end(); it++)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		if (ThisStructure.StructureStatusFlags & STRUCTURE_STATUS_UNDERATTACK)
		{
			bAnyStructureUnderAttack = true;

			if (ThisStructure.StructureType == STRUCTURE_MARINE_COMMCHAIR || ThisStructure.StructureType == STRUCTURE_MARINE_INFANTRYPORTAL)
			{
				bCriticalStructureUnderAttack = true;
			}
		}
	}

	if (!bAnyStructureUnderAttack) { return false; }

	float EnemyStrength = 0.0f;
	float DefenderStrength = 0.0f;
	
	vector<AvHPlayer*> DefendingPlayers = AITAC_GetAllPlayersOfTeamInArea(BotTeam, Base->BaseLocation, UTIL_MetresToGoldSrcUnits(20.0f), true, nullptr, AVH_USER3_COMMANDER_PLAYER);

	for (auto it = DefendingPlayers.begin(); it != DefendingPlayers.end(); it++)
	{
		AvHPlayer* ThisPlayer = (*it);
		edict_t* PlayerEdict = ThisPlayer->edict();

		float ThisPlayerValue = 1.0f;

		if (PlayerHasHeavyArmour(PlayerEdict))
		{
			ThisPlayerValue += 0.5f;
		}

		if (PlayerHasSpecialWeapon(ThisPlayer))
		{
			ThisPlayerValue += 1.0f;
		}

		DefenderStrength += ThisPlayerValue;
	}
	

	if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE)
	{
		vector<AvHPlayer*> AttackingPlayers = AITAC_GetAllPlayersOfTeamInArea(EnemyTeam, Base->BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_COMMANDER_PLAYER);

		for (auto it = AttackingPlayers.begin(); it != AttackingPlayers.end(); it++)
		{
			AvHPlayer* ThisPlayer = (*it);
			edict_t* PlayerEdict = ThisPlayer->edict();

			float ThisPlayerValue = 1.0f;

			if (PlayerHasHeavyArmour(PlayerEdict))
			{
				ThisPlayerValue += 0.5f;
			}

			if (PlayerHasSpecialWeapon(ThisPlayer))
			{
				ThisPlayerValue += 1.0f;
			}

			DefenderStrength += ThisPlayerValue;
		}
	}
	else
	{
		int NumSkulks = AITAC_GetNumPlayersOfTeamAndClassInArea(EnemyTeam, BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_ALIEN_PLAYER1);
		int NumFades = AITAC_GetNumPlayersOfTeamAndClassInArea(EnemyTeam, BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_ALIEN_PLAYER4);
		int NumOnos = AITAC_GetNumPlayersOfTeamAndClassInArea(EnemyTeam, BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_ALIEN_PLAYER5);

		EnemyStrength = (NumSkulks * 0.8f) + (NumFades * 2) + (NumOnos * 2.5f);
	}

	if (EnemyStrength >= 3.0f && EnemyStrength >= DefenderStrength * 3.0f)
	{
		return true;
	}

	return false;
}

bool AICOMM_ShouldBeacon(AvHAIPlayer* pBot)
{
	if (pBot->Player->GetResources() < BALANCE_VAR(kDistressBeaconCost)) { return false; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	DeployableSearchFilter ObservatoryFilter;
	ObservatoryFilter.DeployableTypes = STRUCTURE_MARINE_OBSERVATORY;
	ObservatoryFilter.DeployableTeam = BotTeam;
	ObservatoryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	ObservatoryFilter.ExcludeStatusFlags = (STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_RESEARCHING);

	AvHAIBuildableStructure Observatory = AITAC_FindClosestDeployableToLocation(ZERO_VECTOR, &ObservatoryFilter);

	if (!Observatory.IsValid()) { return false; }

	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
	{
		if (it->BaseType == MARINE_BASE_MAINBASE)
		{
			return AICOMM_IsMainBaseInTrouble(pBot, &(*it));
		}
	}

	return false;

}

void AICOMM_ReceiveChatRequest(AvHAIPlayer* Commander, edict_t* Requestor, const char* Request)
{
	AvHMessageID NewRequestType = MESSAGE_NULL;

	if (!stricmp(Request, "shotgun") || !stricmp(Request, "sg") || !stricmp(Request, "shotty"))
	{
		NewRequestType = BUILD_SHOTGUN;
	}
	else if (!stricmp(Request, "welder"))
	{
		NewRequestType = BUILD_WELDER;
	}
	else if (!stricmp(Request, "HMG"))
	{
		NewRequestType = BUILD_HMG;
	}
	else if (!stricmp(Request, "gl"))
	{
		NewRequestType = BUILD_GRENADE_GUN;
	}
	else if (!stricmp(Request, "mines"))
	{
		NewRequestType = BUILD_MINES;
	}
	else if (!stricmp(Request, "ha") || !stricmp(Request, "heavy") || !stricmp(Request, "heavyarmor") || !stricmp(Request, "heavy armor"))
	{
		NewRequestType = BUILD_HEAVY;
	}
	else if (!stricmp(Request, "jp") || !stricmp(Request, "jetpack") || !stricmp(Request, "jet pack"))
	{
		NewRequestType = BUILD_JETPACK;
	}
	else if (!stricmp(Request, "cat") || !stricmp(Request, "cats") || !stricmp(Request, "catalysts"))
	{
		NewRequestType = BUILD_CAT;
	}
	else if (!stricmp(Request, "pg") || !stricmp(Request, "phase") || !stricmp(Request, "phasegate"))
	{
		NewRequestType = BUILD_PHASEGATE;
	}
	else if (!stricmp(Request, "TF") || !stricmp(Request, "turretfactory"))
	{
		NewRequestType = BUILD_TURRET_FACTORY;
	}
	else if (!stricmp(Request, "turret"))
	{
		NewRequestType = BUILD_TURRET;
	}
	else if (!stricmp(Request, "armory") || !stricmp(Request, "armoury"))
	{
		NewRequestType = BUILD_ARMORY;
	}
	else if (!stricmp(Request, "scan"))
	{
		NewRequestType = BUILD_SCAN;
	}
	else if (!stricmp(Request, "cc") || !stricmp(Request, "chair") || !stricmp(Request, "command chair"))
	{
		NewRequestType = BUILD_COMMANDSTATION;
	}
	else if (!stricmp(Request, "obs") || !stricmp(Request, "observatory"))
	{
		NewRequestType = BUILD_OBSERVATORY;
	}
	else if (!stricmp(Request, "pl") || !stricmp(Request, "protolab") || !stricmp(Request, "proto lab") || !stricmp(Request, "prototype lab"))
	{
		NewRequestType = BUILD_PROTOTYPE_LAB;
	}
	else if (!stricmp(Request, "ip") || !stricmp(Request, "portal") || !stricmp(Request, "inf portal") || !stricmp(Request, "infantry portal"))
	{
		NewRequestType = BUILD_INFANTRYPORTAL;
	}
	else if (!stricmp(Request, "al") || !stricmp(Request, "armslab") || !stricmp(Request, "arms lab"))
	{
		NewRequestType = BUILD_ARMSLAB;
	}
	else if (!stricmp(Request, "sc") || !stricmp(Request, "st") || !stricmp(Request, "siegeturret") || !stricmp(Request, "siege turret") || !stricmp(Request, "siegecannon") || !stricmp(Request, "siege cannon"))
	{
		NewRequestType = BUILD_SIEGE;
	}

	if (NewRequestType == MESSAGE_NULL) { return; }

	ai_commander_request* ExistingRequest = AICOMM_GetExistingRequestForPlayer(Commander, Requestor);

	ai_commander_request NewRequest;
	ai_commander_request* RequestRef = (ExistingRequest) ? ExistingRequest : &NewRequest;

	RequestRef->bNewRequest = true;
	RequestRef->bResponded = false;
	RequestRef->RequestTime = gpGlobals->time;
	RequestRef->RequestType = NewRequestType;
	RequestRef->Requestor = Requestor;
	RequestRef->RequestLocation = UTIL_GetFloorUnderEntity(Requestor) + (UTIL_GetForwardVector2D(Requestor->v.angles) * 75.0f);

	if (!ExistingRequest)
	{
		Commander->ActiveRequests.push_back(NewRequest);
	}
}

bool AICOMM_ShouldCommanderRelocate(AvHAIPlayer* pBot)
{
	if (!CONFIG_IsRelocationAllowed()) { return false; }
	if (!AIBO_BuildOrderIsShotgunRush()) { return false; }

	AvHTeamNumber Team = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(Team);

	edict_t* CurrentCommChair = AITAC_GetCommChair(Team);

	if (FNullEnt(CurrentCommChair)) { return false; }

	AvHAIMarineBase* CurrentMainBase = nullptr;

	for (auto it = pBot->Bases.begin(); it != pBot->Bases.end(); it++)
	{
		if (it->BaseType == MARINE_BASE_MAINBASE)
		{
			CurrentMainBase = &(*it);
			break;
		}
	}

	if (!CurrentMainBase) { return true; }

	if (!CurrentMainBase->bIsActive || CurrentMainBase->bRecycleBase) { return true; }

	// If our main base is within 10m of the chair we're sitting in, then we clearly haven't relocated yet
	bool bMainBaseIsAtChair = vDist2DSq(CurrentCommChair->v.origin, CurrentMainBase->BaseLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f));

	// We're not able to relocate after 90 seconds, best find somewhere else or we're in trouble
	if (AIMGR_GetMatchLength() > 90.0f && !CurrentMainBase->bIsBaseEstablished && !bMainBaseIsAtChair)
	{
		return (CurrentMainBase->NumBuilders == 0);
	}

	if (AITAC_IsRelocationAtStartEnabled() && AIMGR_GetMatchLength() < 90.0f)
	{
		// We've not tried relocating yet
		if (bMainBaseIsAtChair) { return true; }

		const AvHAIHiveDefinition* RelocationHive = AITAC_GetHiveNearestLocation(CurrentMainBase->BaseLocation);

		// The hive we were planning to relocate to has somehow started being built within 90 seconds of start. Not sure how, but we better not move there now
		if (RelocationHive && vDist2DSq(RelocationHive->FloorLocation, CurrentMainBase->BaseLocation) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
		{
			if (RelocationHive->Status != HIVE_STATUS_UNBUILT) { return true; }
		}

		// The enemy beat us to it and has started setting up shop in that location
		DeployableSearchFilter EnemyStuffFilter;
		EnemyStuffFilter.DeployableTeam = EnemyTeam;
		EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_COMMCHAIR | STRUCTURE_MARINE_INFANTRYPORTAL | STRUCTURE_MARINE_TURRETFACTORY);
		EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

		// Those marine bastards stole our spot!
		if (AITAC_DeployableExistsAtLocation(CurrentMainBase->BaseLocation, &EnemyStuffFilter)) { return true; }

		EnemyStuffFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;

		// Those alien bastards stole our spot!
		if (AITAC_GetNumDeployablesNearLocation(CurrentMainBase->BaseLocation, &EnemyStuffFilter) > 1) { return true; }
	}


	// If we have an established main base, then only relocate if it's fucked. This means:
	// Base is not at the original start, or we can't beacon to save the base
	// Critical structures are under attack (comm chair or infantry portals)
	// The number of enemies is overwhelming and we're doomed
	if (CurrentMainBase->bIsBaseEstablished)
	{
		if (CurrentMainBase->NumBuilders > 0) { return false; }

		DeployableSearchFilter ObsFilter;
		ObsFilter.DeployableTeam = Team;
		ObsFilter.DeployableTypes = STRUCTURE_MARINE_OBSERVATORY;
		ObsFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		ObsFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		bool bCanBeacon = AITAC_DeployableExistsAtLocation(ZERO_VECTOR, &ObsFilter);

		bool bCriticalStructureAttacked = false;
		bool bBaseIsAtStart = vDist2DSq(CurrentMainBase->BaseLocation, AITAC_GetTeamOriginalStartLocation(Team)) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f));

		vector<AvHAIBuildableStructure> BaseStructures = AICOMM_GetBaseStructures(CurrentMainBase);

		for (auto structIt = BaseStructures.begin(); structIt != BaseStructures.end(); structIt++)
		{
			AvHAIBuildableStructure* ThisStructure = &(*structIt);

			if ((ThisStructure->StructureStatusFlags & STRUCTURE_STATUS_RECYCLING) || !(ThisStructure->StructureStatusFlags & STRUCTURE_STATUS_COMPLETED)) { continue; }

			if (ThisStructure->StructureStatusFlags & STRUCTURE_STATUS_UNDERATTACK)
			{
				if (ThisStructure->StructureType == STRUCTURE_MARINE_COMMCHAIR || ThisStructure->StructureType == STRUCTURE_MARINE_INFANTRYPORTAL)
				{
					bCriticalStructureAttacked = true;
				}
			}
		}

		if (!bCriticalStructureAttacked) { return false; }

		if (bBaseIsAtStart && bCanBeacon) { return false; }

		return AICOMM_IsMainBaseInTrouble(pBot, CurrentMainBase);
	}

	return false;
}

bool AICOMM_BuildOutBase(AvHAIPlayer* pBot, AvHAIMarineBase* BaseToBuildOut)
{
	if (!pBot || !BaseToBuildOut) { return false; }

	switch (BaseToBuildOut->BaseType)
	{
		case MARINE_BASE_SIEGE:
			return AICOMM_BuildOutSiege(pBot, BaseToBuildOut);
		case MARINE_BASE_OUTPOST:
			return AICOMM_BuildOutOutpost(pBot, BaseToBuildOut);
		case MARINE_BASE_MAINBASE:
			return AICOMM_BuildOutMainBase(pBot, BaseToBuildOut);
		case MARINE_BASE_GUARDPOST:
			return AICOMM_BuildOutGuardPost(pBot, BaseToBuildOut);
		default:
			return false;
	}

	return false;
}

bool AICOMM_BuildOutMainBase(AvHAIPlayer* pBot, AvHAIMarineBase* BaseToBuildOut)
{
	if (!BaseToBuildOut) { return false; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	AvHAIBuildableStructure CommChair;
	AvHAIBuildableStructure ArmsLab;
	AvHAIBuildableStructure ProtoLab;
	AvHAIBuildableStructure PhaseGate;
	AvHAIBuildableStructure Armoury;
	AvHAIBuildableStructure TurretFactory;
	AvHAIBuildableStructure Observatory;
	int NumInfPortals = 0;
	int NumTurrets = 0;
	int NumIncomplete = 0;

	vector<Vector> TurretLocations;

	int DesiredInfPortals = (int)ceilf((float)AIMGR_GetNumPlayersOnTeam(BotTeam) / 4.0f);

	for (auto it = BaseToBuildOut->PlacedStructures.begin(); it != BaseToBuildOut->PlacedStructures.end(); it++)
	{
		AvHAIBuildableStructure StructureRef = AITAC_GetDeployableStructureByEntIndex(BaseToBuildOut->BaseTeam, *it);

		if (!StructureRef.IsCompleted()) { NumIncomplete++; }

		switch (StructureRef.StructureType)
		{
		case STRUCTURE_MARINE_COMMCHAIR:
			CommChair = StructureRef;
			break;
		case STRUCTURE_MARINE_INFANTRYPORTAL:
			NumInfPortals++;
			break;
		case STRUCTURE_MARINE_ARMSLAB:
		{
			// We do this in case we have more than one arms lab. This ensures we always pick
			// the complete one (if it exists) and don't accidentally pick up an unfinished one
			if (!ArmsLab.IsValid() || !ArmsLab.IsCompleted())
			{
				ArmsLab = StructureRef;
			}
		}
		break;
		case STRUCTURE_MARINE_PROTOTYPELAB:
		{
			if (!ProtoLab.IsValid() || !ProtoLab.IsCompleted())
			{
				ProtoLab = StructureRef;
			}
		}
		break;
		case STRUCTURE_MARINE_PHASEGATE:
		{
			if (!PhaseGate.IsValid() || !PhaseGate.IsCompleted())
			{
				PhaseGate = StructureRef;
			}
		}
		break;
		case STRUCTURE_MARINE_TURRETFACTORY:
		{
			if (!TurretFactory.IsValid() || !TurretFactory.IsCompleted())
			{
				TurretFactory = StructureRef;
			}
		}
		break;
		case STRUCTURE_MARINE_ADVTURRETFACTORY:
			TurretFactory = StructureRef;
			break;
		case STRUCTURE_MARINE_ARMOURY:
		{
			if (!Armoury.IsValid() || !Armoury.IsCompleted())
			{
				Armoury = StructureRef;
			}
		}
		break;
		case STRUCTURE_MARINE_ADVARMOURY:
			Armoury = StructureRef;
			break;
		case STRUCTURE_MARINE_OBSERVATORY:
		{
			if (!Observatory.IsValid() || !Observatory.IsCompleted())
			{
				Observatory = StructureRef;
			}
		}
		break;
		case STRUCTURE_MARINE_TURRET:
			NumTurrets++;
			TurretLocations.push_back(StructureRef.Location);
			break;
		default:
			break;
		}
	}

	if (NumIncomplete > 0)
	{
		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_COMMANDER_PLAYER);

		// Don't spam too many structures
		if (NumIncomplete > NumBuilders) { return false; }
	}

	if (!CommChair.IsValid())
	{
		if (pBot->Player->GetResources() < BALANCE_VAR(kCommandStationCost)) { return true; }

		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(STRUCTURE_MARINE_COMMCHAIR, BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(20.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, STRUCTURE_MARINE_COMMCHAIR, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		int NumAttempts = 0;

		while (NumAttempts < 5)
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, STRUCTURE_MARINE_COMMCHAIR, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, STRUCTURE_MARINE_COMMCHAIR, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			NumAttempts++;
		}

		return pBot->Player->GetResources() <= (BALANCE_VAR(kCommandStationCost) * 1.5f);

	}

	if (!CommChair.IsCompleted()) { return false; }

	if (!AIBO_CurrentBuildOrder)
	{
		g_engfuncs.pfnServerPrint("Trying to build the main base but no current build order set.\n");
		return false;
	}

	// Build the initial infantry portals
	if (NumInfPortals < AIBO_CurrentBuildOrder->InitialInfantryPortalCount)
	{
		if (pBot->Player->GetResources() < BALANCE_VAR(kInfantryPortalCost)) { return true; }

		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(STRUCTURE_MARINE_INFANTRYPORTAL, CommChair.Location, BALANCE_VAR(kCommandStationBuildDistance));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, STRUCTURE_MARINE_INFANTRYPORTAL, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		int NumAttempts = 0;

		while (NumAttempts < 5)
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair.Location, BALANCE_VAR(kCommandStationBuildDistance) * 0.8f);

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, STRUCTURE_MARINE_INFANTRYPORTAL, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), CommChair.Location, BALANCE_VAR(kCommandStationBuildDistance) * 0.8f);

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, STRUCTURE_MARINE_INFANTRYPORTAL, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			NumAttempts++;
		}

		return pBot->Player->GetResources() <= (BALANCE_VAR(kInfantryPortalCost) * 1.5f);
	}

	for (auto& NextBuildOrderEntry : AIBO_CurrentBuildOrder->BuildOrder)
	{

		if (NextBuildOrderEntry.BuildOrderType != BUILD_ORDER_STRUCTURE) { continue; }
		if (NextBuildOrderEntry.StructureToBuild == STRUCTURE_NONE) { continue; }

		if (NextBuildOrderEntry.StructureToBuild == STRUCTURE_MARINE_RESTOWER)
		{
			if (AICOMM_ShouldCommanderPrioritiseNodes(pBot) && pBot->Player->GetResources() < 30) { return false; }
			continue;
		}

		AvHAIBuildableStructure Structure;
		int StructureCost = 0;

		switch (NextBuildOrderEntry.StructureToBuild)
		{
		case STRUCTURE_MARINE_COMMCHAIR:
			Structure = CommChair;
			StructureCost = BALANCE_VAR(kCommandStationCost);
			break;
		case STRUCTURE_MARINE_ARMSLAB:
			Structure = ArmsLab;
			StructureCost = BALANCE_VAR(kArmsLabCost);
			break;
		case STRUCTURE_MARINE_PROTOTYPELAB:
			Structure = ProtoLab;
			StructureCost = BALANCE_VAR(kPrototypeLabCost);
			break;
		case STRUCTURE_MARINE_PHASEGATE:
			Structure = PhaseGate;
			StructureCost = BALANCE_VAR(kPhaseGateCost);
			break;
		case STRUCTURE_MARINE_ARMOURY:
			Structure = Armoury;
			StructureCost = BALANCE_VAR(kArmoryCost);
			break;
		case STRUCTURE_MARINE_ADVARMOURY:
			Structure = Armoury;
			StructureCost = BALANCE_VAR(kArmoryUpgradeCost);	
			break;
		case STRUCTURE_MARINE_TURRETFACTORY:
			Structure = TurretFactory;
			StructureCost = BALANCE_VAR(kTurretFactoryCost);
			break;
		case STRUCTURE_MARINE_OBSERVATORY:
			Structure = Observatory;
			StructureCost = BALANCE_VAR(kObservatoryCost);
			break;
		case STRUCTURE_MARINE_INFANTRYPORTAL:
			if (NumInfPortals >= DesiredInfPortals) { continue; }
			StructureCost = BALANCE_VAR(kInfantryPortalCost);
			break;
		case STRUCTURE_MARINE_TURRET:
			if (NumTurrets >= 5) { continue; }
			StructureCost = BALANCE_VAR(kSentryCost);
			break;
		default:
			break;
		}

		if (NextBuildOrderEntry.StructureToBuild == STRUCTURE_MARINE_ADVARMOURY && Armoury.StructureType == STRUCTURE_MARINE_ADVARMOURY) { continue; }
		if (Structure.IsValid() && NextBuildOrderEntry.StructureToBuild != STRUCTURE_MARINE_ADVARMOURY) { continue; }
		if (NextBuildOrderEntry.StructureToBuild == STRUCTURE_MARINE_INFANTRYPORTAL && NumInfPortals >= DesiredInfPortals) { continue; }
		if (NextBuildOrderEntry.StructureToBuild == STRUCTURE_MARINE_TURRET && (!TurretFactory.IsValid() || !TurretFactory.IsCompleted()) ) { continue; }
		if (NextBuildOrderEntry.StructureToBuild == STRUCTURE_MARINE_TURRET && NumTurrets >= 5) { continue; }

		if (pBot->Player->GetResources() < StructureCost) { return true; }

		// Check all the conditions for this entry
		bool conditionOne = NextBuildOrderEntry.BuildConditionOne == CONDITION_NONE ? true : false;
		bool conditionTwo = NextBuildOrderEntry.BuildConditionTwo == CONDITION_NONE ? true : false;;
		bool connective = false;

		if (NextBuildOrderEntry.BuildConditionOne == STRUCTURE_EXISTS)
		{
			DeployableSearchFilter RequiredStructuresFilter;
			RequiredStructuresFilter.DeployableTeam = BotTeam;
			RequiredStructuresFilter.DeployableTypes = NextBuildOrderEntry.StructureRequiredOne;
			RequiredStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			RequiredStructuresFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

			vector <AvHAIBuildableStructure> FoundStructures = AITAC_FindAllDeployables(ZERO_VECTOR, &RequiredStructuresFilter);
			conditionOne = FoundStructures.size() == 0 ? false : true;
		}
		if (NextBuildOrderEntry.BuildConditionOne == UPGRADE_EXISTS)
		{
			conditionOne = (NextBuildOrderEntry.UpgradeRequiredOne == TECH_NULL || !AITAC_ResearchIsComplete(BotTeam, NextBuildOrderEntry.UpgradeRequiredOne)) ? false : true;
		}
		if (NextBuildOrderEntry.BuildConditionOne == TIME_ELAPSED)
		{
			conditionOne = AIMGR_GetMatchLength() < NextBuildOrderEntry.TimeLimitInSecondsOne ? false : true;
		}

		if (NextBuildOrderEntry.BuildConditionTwo == STRUCTURE_EXISTS)
		{
			DeployableSearchFilter RequiredStructuresFilter;
			RequiredStructuresFilter.DeployableTeam = BotTeam;
			RequiredStructuresFilter.DeployableTypes = NextBuildOrderEntry.StructureRequiredTwo;
			RequiredStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			RequiredStructuresFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

			vector <AvHAIBuildableStructure> FoundStructures = AITAC_FindAllDeployables(ZERO_VECTOR, &RequiredStructuresFilter);
			conditionTwo = FoundStructures.size() == 0 ? false : true;
		}
		if (NextBuildOrderEntry.BuildConditionTwo == UPGRADE_EXISTS)
		{
			conditionTwo = (NextBuildOrderEntry.UpgradeRequiredTwo == TECH_NULL || !AITAC_ResearchIsComplete(BotTeam, NextBuildOrderEntry.UpgradeRequiredTwo)) ? false : true;
		}
		if (NextBuildOrderEntry.BuildConditionTwo == TIME_ELAPSED)
		{

			conditionTwo = AIMGR_GetMatchLength() < NextBuildOrderEntry.TimeLimitInSecondsTwo ? false : true;
		}

		if (NextBuildOrderEntry.Connective == AND)
		{
			connective = conditionOne && conditionTwo;
		}
		if (NextBuildOrderEntry.Connective == OR)
		{
			connective = conditionOne || conditionTwo;
		}

		if (!connective) { continue; }
		// Done checking the conditions

		// Maybe upgrade the armoury to the advanced armoury
		if (NextBuildOrderEntry.StructureToBuild == STRUCTURE_MARINE_ADVARMOURY)
		{
			if (Armoury.StructureType == STRUCTURE_MARINE_ADVARMOURY) { return false;  }
			if (pBot->Player->GetResources() < BALANCE_VAR(kArmoryUpgradeCost)) { return true; }

			bool bSuccess = AICOMM_UpgradeStructure(pBot, &Armoury);

			if (bSuccess) { return true; }

			return pBot->Player->GetResources() <= (BALANCE_VAR(kArmoryUpgradeCost) * 1.5f);
		}

		if (!connective)
		{
			std::string debugMessage = "We are trying to build " + std::to_string(NextBuildOrderEntry.StructureToBuild) + " even though we shouldn't.";
			g_engfuncs.pfnServerPrint(debugMessage.c_str());
		}

		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(NextBuildOrderEntry.StructureToBuild, BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(20.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, NextBuildOrderEntry.StructureToBuild, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		int NumAttempts = 0;

		while (NumAttempts < 5)
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair.Location, UTIL_MetresToGoldSrcUnits(10.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, NextBuildOrderEntry.StructureToBuild, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), CommChair.Location, UTIL_MetresToGoldSrcUnits(10.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, NextBuildOrderEntry.StructureToBuild, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			NumAttempts++;
		}

		return pBot->Player->GetResources() <= (StructureCost * 1.5f);
	}
	return false;
}

bool AICOMM_BuildOutOutpost(AvHAIPlayer* pBot, AvHAIMarineBase* BaseToBuildOut)
{
	AvHAIDeployableStructureType StructureToDeploy = STRUCTURE_NONE;

	AvHAIBuildableStructure PhaseGate;
	AvHAIBuildableStructure Armoury;
	AvHAIBuildableStructure TurretFactory;
	AvHAIBuildableStructure Observatory;
	int NumTurrets = 0;
	int NumIncomplete = 0;

	vector<Vector> TurretLocations;

	for (auto it = BaseToBuildOut->PlacedStructures.begin(); it != BaseToBuildOut->PlacedStructures.end(); it++)
	{
		AvHAIBuildableStructure StructureRef = AITAC_GetDeployableStructureByEntIndex(BaseToBuildOut->BaseTeam, *it);

		if (!StructureRef.IsCompleted()) { NumIncomplete++; }

		switch (StructureRef.StructureType)
		{
			case STRUCTURE_MARINE_PHASEGATE:
				PhaseGate = StructureRef;
				break;
			case STRUCTURE_MARINE_TURRETFACTORY:
			case STRUCTURE_MARINE_ADVTURRETFACTORY:
				TurretFactory = StructureRef;
				break;
			case STRUCTURE_MARINE_ARMOURY:
			case STRUCTURE_MARINE_ADVARMOURY:
				Armoury = StructureRef;
				break;
			case STRUCTURE_MARINE_OBSERVATORY:
				Observatory = StructureRef;
				break;
			case STRUCTURE_MARINE_TURRET:
				NumTurrets++;
				TurretLocations.push_back(StructureRef.Location);
				break;
			default:
				break;
		}

	}

	if (NumIncomplete > 0)
	{
		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_COMMANDER_PLAYER);

		// Don't spam too many structures
		if (NumIncomplete >= NumBuilders) { return false; }
	}

	if (PhaseGate.IsValid() && !PhaseGate.IsCompleted()) { return false; }

	if (TurretFactory.IsValid() && !TurretFactory.IsCompleted()) { return false; }

	if (!PhaseGate.IsValid() && AITAC_ResearchIsComplete(pBot->Player->GetTeam(), TECH_RESEARCH_PHASETECH))
	{
		StructureToDeploy = STRUCTURE_MARINE_PHASEGATE;
	}
	else if (!TurretFactory.IsValid())
	{
		StructureToDeploy = STRUCTURE_MARINE_TURRETFACTORY;
	}
	else if (TurretFactory.IsCompleted() && NumTurrets < 5)
	{
		StructureToDeploy = STRUCTURE_MARINE_TURRET;
	}
	else if (!Armoury.IsValid())
	{
		StructureToDeploy = STRUCTURE_MARINE_ARMOURY;
	}
	else if (!Observatory.IsValid())
	{
		StructureToDeploy = STRUCTURE_MARINE_OBSERVATORY;
	}

	if (StructureToDeploy == STRUCTURE_NONE) { return false; }

	int ResourcesRequired = UTIL_GetCostOfStructureType(StructureToDeploy);

	if (pBot->Player->GetResources() < ResourcesRequired) { return true; }

	if (StructureToDeploy == STRUCTURE_MARINE_TURRET)
	{
		int NumAttempts = 0;

		while (NumAttempts < 5)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), TurretFactory.Location, (BALANCE_VAR(kTurretFactoryBuildDistance) * 0.8f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), TurretFactory.Location, (BALANCE_VAR(kTurretFactoryBuildDistance) * 0.8f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			NumAttempts++;
		}

		return pBot->Player->GetResources() <= 15;
	}

	Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(StructureToDeploy, BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(15.0f));

	if (!vIsZero(BuildLocation))
	{
		bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

		if (bSuccess) { return true; }
	}

	if (StructureToDeploy == STRUCTURE_MARINE_TURRETFACTORY)
	{
		if (NumTurrets > 0)
		{
			Vector Centroid = ZERO_VECTOR;

			for (auto tLocs = TurretLocations.begin(); tLocs != TurretLocations.end(); tLocs++)
			{
				Centroid = Centroid + *tLocs;
			}

			Centroid = Centroid / TurretLocations.size();

			if (!vIsZero(Centroid))
			{
				Vector CentroidBuildLocation = UTIL_ProjectPointToNavmesh(Centroid, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

				if (!vIsZero(CentroidBuildLocation))
				{
					bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, CentroidBuildLocation, BaseToBuildOut);

					if (bSuccess) { return true; }
				}
			}
		}
	}

	int NumAttempts = 0;

	while (NumAttempts < 5)
	{
		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(8.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(ONOS_BASE_NAV_PROFILE), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(8.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		NumAttempts++;
	}

	return pBot->Player->GetResources() <= 15;

}

bool AICOMM_BuildOutSiege(AvHAIPlayer* pBot, AvHAIMarineBase* BaseToBuildOut)
{
	AvHAIDeployableStructureType StructureToDeploy = STRUCTURE_NONE;

	AvHAIBuildableStructure PhaseGate;
	AvHAIBuildableStructure Armoury;
	AvHAIBuildableStructure TurretFactory;
	AvHAIBuildableStructure Observatory;
	int NumTurrets = 0;
	int NumIncomplete = 0;

	vector<Vector> TurretLocations;

	for (auto it = BaseToBuildOut->PlacedStructures.begin(); it != BaseToBuildOut->PlacedStructures.end(); it++)
	{
		AvHAIBuildableStructure StructureRef = AITAC_GetDeployableStructureByEntIndex(BaseToBuildOut->BaseTeam, *it);

		if (!StructureRef.IsCompleted())
		{
			NumIncomplete++;
		}

		switch (StructureRef.StructureType)
		{
		case STRUCTURE_MARINE_PHASEGATE:
			PhaseGate = StructureRef;
			break;
		case STRUCTURE_MARINE_TURRETFACTORY:
		case STRUCTURE_MARINE_ADVTURRETFACTORY:
			TurretFactory = StructureRef;
			break;
		case STRUCTURE_MARINE_ARMOURY:
		case STRUCTURE_MARINE_ADVARMOURY:
			Armoury = StructureRef;
			break;
		case STRUCTURE_MARINE_OBSERVATORY:
			Observatory = StructureRef;
			break;
		case STRUCTURE_MARINE_SIEGETURRET:
			NumTurrets++;
			TurretLocations.push_back(StructureRef.Location);
			break;
		default:
			break;
		}
	}

	if (NumIncomplete > 0)
	{
		// Don't spam too many structures
		if (NumIncomplete >= BaseToBuildOut->NumBuilders) { return false; }
	}

	// Don't place any more structures until we have the phase gate up
	if (PhaseGate.IsValid() && !PhaseGate.IsCompleted()) { return false; }

	if (!PhaseGate.IsValid() && AITAC_ResearchIsComplete(BaseToBuildOut->BaseTeam, TECH_RESEARCH_PHASETECH))
	{
		StructureToDeploy = STRUCTURE_MARINE_PHASEGATE;
	}
	else if (!TurretFactory.IsValid())
	{
		StructureToDeploy = STRUCTURE_MARINE_TURRETFACTORY;
	}
	else if (TurretFactory.IsCompleted() && TurretFactory.IsIdle() && TurretFactory.StructureType != STRUCTURE_MARINE_ADVTURRETFACTORY)
	{
		return AICOMM_UpgradeStructure(pBot, &TurretFactory);
	}
	else if (TurretFactory.StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY && NumTurrets < 3)
	{
		StructureToDeploy = STRUCTURE_MARINE_SIEGETURRET;
	}
	else if (!Armoury.IsValid())
	{
		StructureToDeploy = STRUCTURE_MARINE_ARMOURY;
	}
	else if (!Observatory.IsValid())
	{
		StructureToDeploy = STRUCTURE_MARINE_OBSERVATORY;
	}

	if (StructureToDeploy == STRUCTURE_NONE) { return false; }

	int ResourcesRequired = UTIL_GetCostOfStructureType(StructureToDeploy);

	if (pBot->Player->GetResources() < ResourcesRequired) { return true; }

	if (StructureToDeploy == STRUCTURE_MARINE_TURRETFACTORY)
	{
		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(StructureToDeploy, BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(BuildLocation) && (vIsZero(BaseToBuildOut->SiegeTarget) || vDist2DSq(BuildLocation, BaseToBuildOut->SiegeTarget) < sqrf(BALANCE_VAR(kSiegeTurretRange))))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		int NumAttempts = 0;

		if (NumTurrets > 0)
		{
			Vector Centroid = ZERO_VECTOR;

			for (auto tLocs = TurretLocations.begin(); tLocs != TurretLocations.end(); tLocs++)
			{
				Centroid = Centroid + *tLocs;
			}

			Centroid = Centroid / TurretLocations.size();

			if (!vIsZero(Centroid))
			{
				Vector BuildLocation = UTIL_ProjectPointToNavmesh(Centroid, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

				if (!vIsZero(BuildLocation))
				{
					bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

					if (bSuccess) { return true; }
				}
			}			
		}

		while (NumAttempts < 5)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(2.0f + NumAttempts));

			if (!vIsZero(BuildLocation) && (vIsZero(BaseToBuildOut->SiegeTarget) || vDist2DSq(BuildLocation, BaseToBuildOut->SiegeTarget) < sqrf(BALANCE_VAR(kSiegeTurretRange))))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(2.0f + NumAttempts));

			if (!vIsZero(BuildLocation) && (vIsZero(BaseToBuildOut->SiegeTarget) || vDist2DSq(BuildLocation, BaseToBuildOut->SiegeTarget) < sqrf(BALANCE_VAR(kSiegeTurretRange))))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			NumAttempts++;
		}

		return (pBot->Player->GetResources() <= BALANCE_VAR(kTurretFactoryCost) * 2);
	}

	if (StructureToDeploy == STRUCTURE_MARINE_SIEGETURRET)
	{
		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(StructureToDeploy, TurretFactory.Location, BALANCE_VAR(kTurretFactoryBuildDistance));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		int NumAttempts = 0;

		while (NumAttempts < 5)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), TurretFactory.Location, (BALANCE_VAR(kTurretFactoryBuildDistance) * 0.4f));

			if (!vIsZero(BuildLocation) && (vIsZero(BaseToBuildOut->SiegeTarget) || vDist2DSq(BuildLocation, BaseToBuildOut->SiegeTarget) < sqrf(BALANCE_VAR(kSiegeTurretRange))))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), TurretFactory.Location, (BALANCE_VAR(kTurretFactoryBuildDistance) * 0.6f));

			if (!vIsZero(BuildLocation) && (vIsZero(BaseToBuildOut->SiegeTarget) || vDist2DSq(BuildLocation, BaseToBuildOut->SiegeTarget) < sqrf(BALANCE_VAR(kSiegeTurretRange))))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }				
			}

			NumAttempts++;
		}

		return (pBot->Player->GetResources() <= BALANCE_VAR(kSiegeCost) + 5);
	}

	Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(StructureToDeploy, BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f));

	if (!vIsZero(BuildLocation))
	{
		bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

		if (bSuccess) { return true; }
	}

	int NumAttempts = 0;

	while (NumAttempts < 5)
	{
		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(3.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		NumAttempts++;
	}

	return (pBot->Player->GetResources() <= 15);

}

bool AICOMM_BuildOutGuardPost(AvHAIPlayer* pBot, AvHAIMarineBase* BaseToBuildOut)
{
	AvHAIBuildableStructure TurretFactory;
	AvHAIBuildableStructure Observatory;
	int NumTurrets = 0;
	int NumIncomplete = 0;

	vector<Vector> TurretLocations;

	for (auto it = BaseToBuildOut->PlacedStructures.begin(); it != BaseToBuildOut->PlacedStructures.end(); it++)
	{
		AvHAIBuildableStructure StructureRef = AITAC_GetDeployableStructureByEntIndex(BaseToBuildOut->BaseTeam, *it);

		if (!StructureRef.IsCompleted())
		{
			NumIncomplete++;
		}

		if (StructureRef.StructureType & (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY))
		{
			if (!TurretFactory.IsValid() || !TurretFactory.IsCompleted())
			{
				TurretFactory = StructureRef;
			}
		}
		else if (StructureRef.StructureType == STRUCTURE_MARINE_TURRET)
		{
			NumTurrets++;
			TurretLocations.push_back(StructureRef.Location);
		}
		else if (StructureRef.StructureType == STRUCTURE_MARINE_OBSERVATORY)
		{
			Observatory = StructureRef;
		}
	}

	if (NumIncomplete > 0)
	{
		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_COMMANDER_PLAYER);

		// Don't spam too many structures
		if (NumIncomplete >= NumBuilders) { return false; }
	}

	AvHAIDeployableStructureType StructureToDeploy = STRUCTURE_NONE;

	if (!TurretFactory.IsValid())
	{
		StructureToDeploy = STRUCTURE_MARINE_TURRETFACTORY;
	}
	else
	{
		if (!TurretFactory.IsCompleted()) { return false; }

		if (NumTurrets < 5)
		{
			StructureToDeploy = STRUCTURE_MARINE_TURRET;
		}
		else if (!Observatory.IsValid())
		{
			StructureToDeploy = STRUCTURE_MARINE_OBSERVATORY;
		}
	}

	if (StructureToDeploy == STRUCTURE_NONE) { return false; }

	int ResourcesRequired = UTIL_GetCostOfStructureType(StructureToDeploy);

	if (pBot->Player->GetResources() < ResourcesRequired) { return true; }

	if (StructureToDeploy == STRUCTURE_MARINE_TURRET)
	{
		int NumAttempts = 0;

		while (NumAttempts < 5)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), TurretFactory.Location, (BALANCE_VAR(kTurretFactoryBuildDistance) * 0.8f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), TurretFactory.Location, (BALANCE_VAR(kTurretFactoryBuildDistance) * 0.8f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

				if (bSuccess) { return true; }
			}

			NumAttempts++;
		}

		return pBot->Player->GetResources() <= 15;
	}

	Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(StructureToDeploy, BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(5.0f));

	if (!vIsZero(BuildLocation))
	{
		bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

		if (bSuccess) { return true; }
	}

	if (StructureToDeploy == STRUCTURE_MARINE_TURRETFACTORY)
	{
		if (NumTurrets > 0)
		{
			Vector Centroid = ZERO_VECTOR;

			for (auto tLocs = TurretLocations.begin(); tLocs != TurretLocations.end(); tLocs++)
			{
				Centroid = Centroid + *tLocs;
			}

			Centroid = Centroid / TurretLocations.size();

			if (!vIsZero(Centroid))
			{
				Vector CentroidBuildLocation = UTIL_ProjectPointToNavmesh(Centroid, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

				if (!vIsZero(CentroidBuildLocation))
				{
					bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, CentroidBuildLocation, BaseToBuildOut);

					if (bSuccess) { return true; }
				}
			}
		}
	}

	int NumAttempts = 0;

	while (NumAttempts < 5)
	{
		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), BaseToBuildOut->BaseLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_AddStructureToBase(pBot, StructureToDeploy, BuildLocation, BaseToBuildOut);

			if (bSuccess) { return true; }
		}

		NumAttempts++;
	}

	return pBot->Player->GetResources() <= 15;

}

AvHAIMarineBase* AICOMM_AddNewBase(AvHAIPlayer* pBot, Vector NewBaseLocation, MarineBaseType NewBaseType)
{
	AvHAIMarineBase NewBase;
	NewBase.BaseLocation = NewBaseLocation;
	NewBase.BaseType = NewBaseType;
	NewBase.BaseTeam = pBot->Player->GetTeam();

	pBot->Bases.push_back(NewBase);

	return &pBot->Bases.back();
}

bool AICOMM_AddStructureToBase(AvHAIPlayer* pBot, AvHAIDeployableStructureType StructureToDeploy, Vector BuildLocation, AvHAIMarineBase* BaseToAdd)
{
	if (!BaseToAdd || vIsZero(BuildLocation)) { return false; }

	AvHAIBuildableStructure* DeployedStructure = AICOMM_DeployStructure(pBot, StructureToDeploy, BuildLocation);

	if (DeployedStructure)
	{
		BaseToAdd->PlacedStructures.push_back(DeployedStructure->EntIndex);
		BaseToAdd->bBaseInitialised = true;
		if (BaseToAdd->BaseType == MARINE_BASE_MAINBASE)
		{
			DeployedStructure->Purpose = STRUCTURE_PURPOSE_BASE;
		}
		else if (BaseToAdd->BaseType == MARINE_BASE_SIEGE)
		{
			DeployedStructure->Purpose = STRUCTURE_PURPOSE_SIEGE;
		}
		else
		{
			DeployedStructure->Purpose = STRUCTURE_PURPOSE_FORTIFY;
		}
		return true;
	}

	return false;
}