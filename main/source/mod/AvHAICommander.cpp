
#include "AvHAICommander.h"
#include "AvHAITactical.h"
#include "AvHAIMath.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIWeaponHelper.h"
#include "AvHAINavigation.h"
#include "AvHAITask.h"
#include "AvHAIHelper.h"
#include "AvHAIPlayerManager.h"

#include "AvHSharedUtil.h"
#include "AvHServerUtil.h"

bool AICOMM_DeployStructure(AvHAIPlayer* pBot, const AvHAIDeployableStructureType StructureToDeploy, const Vector Location, StructurePurpose Purpose)
{
	if (vIsZero(Location)) { return false; }

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
		return false; 
	}

	string theErrorMessage;
	int theCost = 0;
	bool thePurchaseAllowed = pBot->Player->GetPurchaseAllowed(StructureID, theCost, &theErrorMessage);

	if (!thePurchaseAllowed) { return false; }

	CBaseEntity* NewStructureEntity = AvHSUBuildTechForPlayer(StructureID, BuildLocation, pBot->Player);

	if (!NewStructureEntity) { return false; }

	AvHAIBuildableStructure* NewStructure = AITAC_UpdateBuildableStructure(NewStructureEntity);

	if (NewStructure)
	{
		NewStructure->Purpose = Purpose;
	}

	pBot->Player->PayPurchaseCost(theCost);

	pBot->next_commander_action_time = gpGlobals->time + 1.0f;

	return true;
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

bool AICOMM_IsOrderStillValid(AvHAIPlayer* pBot, ai_commander_order* Order)
{
	if (FNullEnt(Order->Assignee) || FNullEnt(Order->OrderTarget) || !IsPlayerActiveInGame(Order->Assignee) || Order->OrderPurpose == ORDERPURPOSE_NONE) { return false; }

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
		default:
			return false;
	}

	return false;
}

bool AICOMM_DoesPlayerOrderNeedReminder(AvHAIPlayer* pBot, ai_commander_order* Order)
{
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
	
	int NumPlayersOnTeam = AITAC_GetNumActivePlayersOnTeam(pBot->Player->GetTeam());
	int DesiredPlayers = imini(2, (int)ceilf((float)NumPlayersOnTeam *0.5f));

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	Vector TeamStartingLocation = AITAC_GetTeamStartingLocation(BotTeam);

	const AvHAIHiveDefinition* SiegedHive = AITAC_GetNearestHiveUnderActiveSiege(pBot->Player->GetTeam(), AITAC_GetCommChairLocation(pBot->Player->GetTeam()));

	if (SiegedHive)
	{
		int NumAssignedPlayers = AICOMM_GetNumPlayersAssignedToOrder(pBot, SiegedHive->HiveEdict, ORDERPURPOSE_SIEGE_HIVE);

		if (NumAssignedPlayers < DesiredPlayers)
		{
			for (int i = 0; i < (DesiredPlayers - NumAssignedPlayers); i++)
			{
				edict_t* NewAssignee = AICOMM_GetPlayerWithNoOrderNearestLocation(pBot, SiegedHive->FloorLocation);

				if (!FNullEnt(NewAssignee))
				{
					AICOMM_AssignNewPlayerOrder(pBot, NewAssignee, SiegedHive->HiveEdict, ORDERPURPOSE_SIEGE_HIVE);
				}
			}
		}
	}

	if (AICOMM_ShouldCommanderPrioritiseNodes(pBot))
	{
		

		DeployableSearchFilter ResNodeFilter;
		ResNodeFilter.ReachabilityTeam = pBot->Player->GetTeam();
		ResNodeFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;

		vector<AvHAIResourceNode*> EligibleResNodes = AITAC_GetAllMatchingResourceNodes(ZERO_VECTOR, &ResNodeFilter);

		AvHAIResourceNode* NearestNode = nullptr;
		float MinDist = 0.0f;

		for (auto it = EligibleResNodes.begin(); it != EligibleResNodes.end(); it++)
		{
			AvHAIResourceNode* ThisNode = (*it);

			if (!ThisNode || ThisNode->OwningTeam == BotTeam) { continue; }

			int NumDesiredPlayers = (ThisNode->OwningTeam == EnemyTeam) ? 2 : 1;
			int NumAssignedPlayers = AICOMM_GetNumPlayersAssignedToOrder(pBot, ThisNode->ResourceEntity->edict(), ORDERPURPOSE_SECURE_RESNODE);

			if (NumAssignedPlayers >= NumDesiredPlayers) { continue; }

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
			}
		}

		return;
	}

	vector<AvHAIHiveDefinition*> Hives = AITAC_GetAllHives();

	AvHAIHiveDefinition* EmptyHive = nullptr;
	float MinDist = 0.0f;
	int MinNumAssignedPlayers = 0;

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		AvHAIHiveDefinition* ThisHive = (*it);
		if (ThisHive->Status != HIVE_STATUS_UNBUILT) { continue; }
		if (AICOMM_IsHiveFullySecured(pBot, ThisHive, false)) { continue; }

		int NumAssignedPlayers = AICOMM_GetNumPlayersAssignedToOrder(pBot, ThisHive->HiveEdict, ORDERPURPOSE_SECURE_HIVE);

		if (NumAssignedPlayers < DesiredPlayers)
		{
			float ThisDist = vDist2DSq(TeamStartingLocation, ThisHive->Location);

			if (!EmptyHive || ThisDist < MinDist)
			{
				MinNumAssignedPlayers = NumAssignedPlayers;
				EmptyHive = ThisHive;
				MinDist = ThisDist;
			}
		}
	}

	if (EmptyHive)
	{
		for (int i = 0; i < (DesiredPlayers - MinNumAssignedPlayers); i++)
		{
			edict_t* NewAssignee = AICOMM_GetPlayerWithNoOrderNearestLocation(pBot, EmptyHive->FloorLocation);

			if (!FNullEnt(NewAssignee))
			{
				AICOMM_AssignNewPlayerOrder(pBot, NewAssignee, EmptyHive->HiveEdict, ORDERPURPOSE_SECURE_HIVE);
			}
		}
	}

	if (!AITAC_ResearchIsComplete(BotTeam, TECH_RESEARCH_PHASETECH)) { return; }

	if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
	{
		AvHAIHiveDefinition* HiveSiegeOpportunity = nullptr;

		float MinDist = 0.0f;

		for (auto it = Hives.begin(); it != Hives.end(); it++)
		{
			AvHAIHiveDefinition* ThisHive = (*it);
			if (ThisHive->Status == HIVE_STATUS_UNBUILT) { continue; }

			DeployableSearchFilter ExistingSiegeFilter;
			ExistingSiegeFilter.DeployableTeam = BotTeam;
			ExistingSiegeFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			ExistingSiegeFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
			ExistingSiegeFilter.ReachabilityTeam = BotTeam;
			ExistingSiegeFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
			ExistingSiegeFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(25.0f);

			AvHAIBuildableStructure SiegeStructure = AITAC_FindClosestDeployableToLocation(ThisHive->Location, &ExistingSiegeFilter);

			if (SiegeStructure.IsValid())
			{
				HiveSiegeOpportunity = ThisHive;
				break;
			}
			else
			{
				float ThisDist = vDist2DSq(ThisHive->FloorLocation, TeamStartingLocation);

				if (!HiveSiegeOpportunity || ThisDist < MinDist)
				{
					HiveSiegeOpportunity = ThisHive;
					MinDist = ThisDist;
				}
			}
		}

		if (HiveSiegeOpportunity)
		{
			for(int i = 0; i < (DesiredPlayers - MinNumAssignedPlayers); i++)
			{
				edict_t* NewAssignee = AICOMM_GetPlayerWithNoOrderNearestLocation(pBot, HiveSiegeOpportunity->FloorLocation);

				if (!FNullEnt(NewAssignee))
				{
					AICOMM_AssignNewPlayerOrder(pBot, NewAssignee, HiveSiegeOpportunity->HiveEdict, ORDERPURPOSE_SIEGE_HIVE);
				}
			}
		}

	}

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
		default:
			return true;
	}

	return true;
}

bool AICOMM_CheckForNextBuildAction(AvHAIPlayer* pBot)
{

	AvHTeamNumber TeamNumber = pBot->Player->GetTeam();

	edict_t* CommChair = AITAC_GetCommChair(TeamNumber);

	if (FNullEnt(CommChair)) { return false; }

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	StructureFilter.DeployableTeam = TeamNumber;
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	StructureFilter.ReachabilityTeam = TeamNumber;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
	StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_BASE;

	edict_t* BaseBuilder = AITAC_GetNearestHiddenPlayerInLocation(TeamNumber, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

	int NumInfantryPortals = AITAC_GetNumDeployablesNearLocation(CommChair->v.origin, &StructureFilter);

	if (NumInfantryPortals < 2 && !FNullEnt(BaseBuilder))
	{
		bool bEnemyInBase = NumInfantryPortals > 1 && AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		bool bSuccess = !bEnemyInBase && AICOMM_BuildInfantryPortal(pBot, CommChair);
		return (bSuccess || pBot->Player->GetResources() <= BALANCE_VAR(kInfantryPortalCost) + 10);
	}

	StructureFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
	StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_BASE;

	AvHAIBuildableStructure BaseArmoury = AITAC_FindClosestDeployableToLocation(CommChair->v.origin, &StructureFilter);

	if (!BaseArmoury.IsValid() && !FNullEnt(BaseBuilder))
	{
		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(STRUCTURE_MARINE_ARMOURY, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			if (!bEnemyInBase)
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_ARMOURY, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}
		}

		StructureFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;

		AvHAIBuildableStructure NearestInfantryPortal = AITAC_FindClosestDeployableToLocation(CommChair->v.origin, &StructureFilter);

		if (NearestInfantryPortal.IsValid())
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestInfantryPortal.Location, UTIL_MetresToGoldSrcUnits(5.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

				if (!bEnemyInBase)
				{
					bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_ARMOURY, BuildLocation, STRUCTURE_PURPOSE_BASE);

					if (bSuccess) { return true; }
				}
			}
		}

		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			if (!bEnemyInBase)
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_ARMOURY, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}
		}

		if (pBot->Player->GetResources() <= BALANCE_VAR(kArmoryCost) + 5) { return true; }
	}

	const AvHAIHiveDefinition* HiveUnderSiege = AITAC_GetNearestHiveUnderActiveSiege(pBot->Player->GetTeam(), AITAC_GetCommChairLocation(pBot->Player->GetTeam()));

	if (HiveUnderSiege)
	{
		bool bAlreadyScanning = AITAC_ItemExistsInLocation(HiveUnderSiege->Location, DEPLOYABLE_ITEM_SCAN, TEAM_IND, AI_REACHABILITY_NONE, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), false);

		if (!bAlreadyScanning)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), HiveUnderSiege->FloorLocation, UTIL_MetresToGoldSrcUnits(3.0f));

			if (AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_SCAN, BuildLocation))
			{
				return true;
			}
		}
	}

	if (AITAC_PhaseGatesAvailable(TeamNumber))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
		StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_BASE;

		bool bPhaseNearBase = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);

		if (!bPhaseNearBase && !FNullEnt(BaseBuilder))
		{

			Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(STRUCTURE_MARINE_PHASEGATE, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

				if (!bEnemyInBase)
				{
					bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PHASEGATE, BuildLocation, STRUCTURE_PURPOSE_BASE);

					if (bSuccess) { return true; }
				}
			}

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

				bool bSuccess = !bEnemyInBase && AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PHASEGATE, BuildLocation, STRUCTURE_PURPOSE_BASE);
				if (bSuccess || pBot->Player->GetResources() <= BALANCE_VAR(kPhaseGateCost) + 10) { return true; }
			}
		}
	}

	const AvHAIResourceNode* CappableNode = AICOMM_GetNearestResourceNodeCapOpportunity(TeamNumber, CommChair->v.origin);

	if (CappableNode)
	{
		bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_RESTOWER, CappableNode->Location);

		if (bSuccess || pBot->Player->GetResources() <= BALANCE_VAR(kResourceTowerCost) + 10) { return true; }

	}

	const AvHAIHiveDefinition* HiveToSecure = AICOMM_GetEmptyHiveOpportunityNearestLocation(pBot, AITAC_GetCommChairLocation(TeamNumber));

	if (HiveToSecure)
	{
		bool bSuccess = AICOMM_PerformNextSecureHiveAction(pBot, HiveToSecure);

		if (bSuccess || pBot->Player->GetResources() <= BALANCE_VAR(kTurretFactoryCost) + 5) { return true; }
	}

	if (AICOMM_ShouldCommanderPrioritiseNodes(pBot) && pBot->Player->GetResources() < 30) { return false; }

	StructureFilter.DeployableTypes = (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
	StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_BASE;

	AvHAIBuildableStructure TF = AITAC_FindClosestDeployableToLocation(CommChair->v.origin, &StructureFilter);

	if (!TF.IsValid())
	{
		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(STRUCTURE_MARINE_TURRETFACTORY, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			if (!bEnemyInBase)
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRETFACTORY, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}
		}

		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			bool bSuccess = !bEnemyInBase && AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRETFACTORY, BuildLocation, STRUCTURE_PURPOSE_BASE);

			if (bSuccess || pBot->Player->GetResources() <= BALANCE_VAR(kTurretFactoryCost) + 5) { return true; }

		}
	}

	if (TF.IsValid() && (TF.StructureStatusFlags & STRUCTURE_STATUS_COMPLETED))
	{
		StructureFilter.DeployableTypes = (STRUCTURE_MARINE_TURRET);
		StructureFilter.MaxSearchRadius = BALANCE_VAR(kTurretFactoryBuildDistance);
		StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_BASE;

		int NumTurrets = AITAC_GetNumDeployablesNearLocation(TF.Location, &StructureFilter);

		if (NumTurrets < 5)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), TF.Location, (BALANCE_VAR(kTurretFactoryBuildDistance) * 0.8f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRET, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), TF.Location, (BALANCE_VAR(kTurretFactoryBuildDistance) * 0.8f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRET, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess || pBot->Player->GetResources() <= BALANCE_VAR(kSentryCost) + 5) { return true; }
			}

		}
		else
		{
			if (!(TF.StructureStatusFlags & STRUCTURE_STATUS_ELECTRIFIED) && pBot->Player->GetResources() > 50)
			{
				bool bSuccess = AICOMM_ResearchTech(pBot, &TF, RESEARCH_ELECTRICAL);

				if (bSuccess) { return true; }
			}
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
	StructureFilter.MaxSearchRadius = 0.0f;
	StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_BASE;

	bool bHasArmsLab = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);

	if (!bHasArmsLab && !FNullEnt(BaseBuilder))
	{
		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(STRUCTURE_MARINE_ARMSLAB, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			if (!bEnemyInBase)
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_ARMSLAB, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}
		}

		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));	

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			bool bSuccess = !bEnemyInBase && AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_ARMSLAB, BuildLocation, STRUCTURE_PURPOSE_BASE);

			if (bSuccess || pBot->Player->GetResources() <= BALANCE_VAR(kArmsLabCost) + 10) { return true; }
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_OBSERVATORY;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
	StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_BASE;

	bool bHasObservatory = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);

	if (!bHasObservatory && !FNullEnt(BaseBuilder))
	{
		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(STRUCTURE_MARINE_OBSERVATORY, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			if (!bEnemyInBase)
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_OBSERVATORY, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}
		}
		
		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			bool bSuccess = !bEnemyInBase && AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_OBSERVATORY, BuildLocation, STRUCTURE_PURPOSE_BASE);

			if (bSuccess || pBot->Player->GetResources() <= BALANCE_VAR(kObservatoryCost) + 10) { return true; }

		}
	}

	if (!AITAC_ResearchIsComplete(TeamNumber, TECH_RESEARCH_PHASETECH))
	{
		return false;
	}

	const AvHAIHiveDefinition* HiveToSiege = AICOMM_GetHiveSiegeOpportunityNearestLocation(pBot, AITAC_GetCommChairLocation(TeamNumber));

	if (HiveToSiege)
	{
		bool bSuccess = AICOMM_PerformNextSiegeHiveAction(pBot, HiveToSiege);
		if (bSuccess || pBot->Player->GetResources() <= BALANCE_VAR(kTurretFactoryCost) + 5) { return true; }
	}

	StructureFilter.MaxSearchRadius = 0.0f;
	StructureFilter.DeployableTypes = STRUCTURE_MARINE_ADVARMOURY;
	StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_BASE;

	bool bHasAdvArmoury = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);
	bool bIsResearchingArmoury = false;

	if (!bHasAdvArmoury)
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMOURY;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
		StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_RESEARCHING;
		
		AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(CommChair->v.origin, &StructureFilter);

		if (NearestArmoury.IsValid())
		{
			bIsResearchingArmoury = UTIL_StructureIsUpgrading(NearestArmoury.edict);

			if (!bIsResearchingArmoury)
			{
				if (AICOMM_UpgradeStructure(pBot, &NearestArmoury))
				{
					return true;
				}
			}
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_PROTOTYPELAB;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
	StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_NONE;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_BASE;

	bool bHasPrototypeLab = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);

	if (!bHasPrototypeLab && bHasAdvArmoury && !FNullEnt(BaseBuilder))
	{
		Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(STRUCTURE_MARINE_PROTOTYPELAB, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));
		
		if(!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			if (!bEnemyInBase)
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PROTOTYPELAB, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}
		}

		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			if (!bEnemyInBase)
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PROTOTYPELAB, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}
		}

		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(20.0f));

		if (!vIsZero(BuildLocation))
		{
			bool bEnemyInBase = AITAC_AnyPlayerOnTeamWithLOS(AIMGR_GetEnemyTeam(TeamNumber), BuildLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_MetresToGoldSrcUnits(10.0f));

			if (!bEnemyInBase)
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PROTOTYPELAB, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess || pBot->Player->GetResources() < BALANCE_VAR(kPrototypeLabCost) + 20) { return true; }
			}
		}
	}

	int Resources = pBot->Player->GetResources();

	if (Resources > 100)
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_RESTOWER;
		StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_ELECTRIFIED;
		StructureFilter.MaxSearchRadius = 0.0f;
		StructureFilter.PurposeFlags = STRUCTURE_PURPOSE_ANY;

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
		AvHAIDroppedItem* NearestWelder = AITAC_FindClosestItemToLocation(AITAC_GetCommChairLocation(CommanderTeam), DEPLOYABLE_ITEM_WELDER, CommanderTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(15.0f), false);
		bool bPlayerHasWelder = false;

		if (!NearestWelder)
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

		if (!NearestWelder && !bPlayerHasWelder)
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

	// Now work out how many welders we want on the team generally
	
	int NumDesiredWelders = 1;

	if (!AICOMM_ShouldCommanderPrioritiseNodes(pBot) && pBot->Player->GetResources() >= 20)
	{
		NumDesiredWelders = (int)ceilf((float)AIMGR_GetNumPlayersOnTeam(CommanderTeam) * 0.3f);
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

	NumDesiredWelders = imini(NumDesiredWelders, (int)(ceilf((float)AIMGR_GetNumPlayersOnTeam(CommanderTeam) * 0.5f)));

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
	if (AICOMM_ShouldCommanderPrioritiseNodes(pBot) && pBot->Player->GetResources() < 20) { return false; }


	int NumDesiredShotguns = (int)ceilf(AIMGR_GetNumPlayersOnTeam(CommanderTeam) * 0.33f);
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

	AvHAIDroppedItem* ExistingHA = AITAC_FindClosestItemToLocation(NearestPrototypeLab.Location, DEPLOYABLE_ITEM_HEAVYARMOUR, CommanderTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
	AvHAIDroppedItem* ExistingHMG = AITAC_FindClosestItemToLocation(NearestAdvArmoury.Location, DEPLOYABLE_ITEM_HMG, CommanderTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
	AvHAIDroppedItem* ExistingWelder = AITAC_FindClosestItemToLocation(NearestAdvArmoury.Location, DEPLOYABLE_ITEM_WELDER, CommanderTeam, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);

	if (ExistingHA && ExistingHMG && ExistingWelder) { return false; }

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

	if (!ExistingHA && !bDropWelder && !bDropWeapon)
	{
		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestPrototypeLab.Location, UTIL_MetresToGoldSrcUnits(3.0f));

		if (vIsZero(DeployLocation))
		{
			DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestPrototypeLab.Location, UTIL_MetresToGoldSrcUnits(3.0f));
		}

		bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_HEAVYARMOUR, DeployLocation);

		return bSuccess;
	}

	if (bDropWeapon && !ExistingHMG)
	{
		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestAdvArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));

		if (vIsZero(DeployLocation))
		{
			DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), NearestAdvArmoury.Location, UTIL_MetresToGoldSrcUnits(3.0f));
		}

		bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_HMG, DeployLocation);

		return bSuccess;
	}

	if (bDropWelder && !ExistingWelder)
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


	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTeam = CommanderTeam;
	StructureFilter.ReachabilityTeam = CommanderTeam;
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_GRENADES))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure Armoury = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (Armoury.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &Armoury, RESEARCH_GRENADES);
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
	StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

	AvHAIBuildableStructure ArmsLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_ARMOR_ONE))
	{
		if (ArmsLab.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &ArmsLab, RESEARCH_ARMOR_ONE);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_WEAPONS_ONE))
	{
		if (ArmsLab.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &ArmsLab, RESEARCH_WEAPONS_ONE);
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_OBSERVATORY;
	StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

	AvHAIBuildableStructure Observatory = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_PHASETECH))
	{
		if (Observatory.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &Observatory, RESEARCH_PHASETECH);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_MOTIONTRACK))
	{
		if (Observatory.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &Observatory, RESEARCH_MOTIONTRACK);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_ARMOR_TWO))
	{
		if (ArmsLab.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &ArmsLab, RESEARCH_ARMOR_TWO);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_WEAPONS_TWO))
	{
		if (ArmsLab.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &ArmsLab, RESEARCH_WEAPONS_TWO);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_CATALYSTS))
	{
		if (ArmsLab.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &ArmsLab, RESEARCH_CATALYSTS);
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_PROTOTYPELAB;
	StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

	AvHAIBuildableStructure ProtoLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_HEAVYARMOR))
	{
		if (ProtoLab.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &ProtoLab, RESEARCH_HEAVYARMOR);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_JETPACKS))
	{
		if (ProtoLab.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &ProtoLab, RESEARCH_JETPACKS);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_ARMOR_THREE))
	{
		if (ArmsLab.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &ArmsLab, RESEARCH_ARMOR_THREE);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_WEAPONS_THREE))
	{
		if (ArmsLab.IsValid())
		{
			return AICOMM_ResearchTech(pBot, &ArmsLab, RESEARCH_WEAPONS_THREE);
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

bool AICOMM_PerformNextSiegeHiveAction(AvHAIPlayer* pBot, const AvHAIHiveDefinition* HiveToSiege)
{
	AvHTeamNumber CommanderTeam = pBot->Player->GetTeam();

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTeam = CommanderTeam;
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	StructureFilter.ReachabilityTeam = CommanderTeam;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(25.0f);

	Vector SiegeLocation = ZERO_VECTOR;

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
	AvHAIBuildableStructure ExistingPG = AITAC_FindClosestDeployableToLocation(HiveToSiege->Location, &StructureFilter);

	StructureFilter.DeployableTypes = (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
	AvHAIBuildableStructure ExistingTF = AITAC_FindClosestDeployableToLocation(HiveToSiege->Location, &StructureFilter);

	StructureFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
	AvHAIBuildableStructure ExistingArmoury = AITAC_FindClosestDeployableToLocation(HiveToSiege->Location, &StructureFilter);

	edict_t* NearestBuilder = nullptr;

	// We only build one of these at a time, so we don't drop a bunch of structures and then our intrepid sieger gets killed and the aliens nom them all
	if (ExistingPG.IsValid())
	{
		if (ExistingPG.IsCompleted())
		{
			SiegeLocation = ExistingPG.Location;
			NearestBuilder = AITAC_GetClosestPlayerOnTeamWithLOS(CommanderTeam, ExistingPG.Location, UTIL_MetresToGoldSrcUnits(5.0f), pBot->Edict);
		}
		else
		{
			// Don't do anything else until we've finished building the phase gate
			return false;
		}
	}

	if (ExistingTF.IsValid())
	{
		if (ExistingTF.IsCompleted())
		{
			if (vIsZero(SiegeLocation))
			{
				SiegeLocation = ExistingTF.Location;
			}

			if (FNullEnt(NearestBuilder))
			{
				NearestBuilder = AITAC_GetClosestPlayerOnTeamWithLOS(CommanderTeam, ExistingTF.Location, UTIL_MetresToGoldSrcUnits(5.0f), pBot->Edict);
			}
		}
		else
		{
			// Don't do anything else until we've finished building the turret factory
			return false;
		}
	}
	else
	{
		if (FNullEnt(NearestBuilder))
		{
			NearestBuilder = AITAC_GetNearestHiddenPlayerInLocation(CommanderTeam, HiveToSiege->Location, UTIL_MetresToGoldSrcUnits(20.0f));
		}
	}

	if (FNullEnt(NearestBuilder)) { return false; }

	bool bPhaseGatesAvailable = AITAC_PhaseGatesAvailable(CommanderTeam);

	if (vIsZero(SiegeLocation))
	{
		SiegeLocation = NearestBuilder->v.origin;
	}

	AvHAIDeployableStructureType NextStructure = STRUCTURE_NONE;

	if (!ExistingPG.IsValid() && bPhaseGatesAvailable)
	{
		NextStructure = STRUCTURE_MARINE_PHASEGATE;
	}
	else if (!ExistingTF.IsValid())
	{
		NextStructure = STRUCTURE_MARINE_TURRETFACTORY;
	}
	else if (!ExistingArmoury.IsValid())
	{
		NextStructure = STRUCTURE_MARINE_ARMOURY;
	}

	if (NextStructure != STRUCTURE_NONE)
	{
		Vector NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), SiegeLocation, UTIL_MetresToGoldSrcUnits(3.0f));

		if (!vIsZero(NextBuildPosition) && vDist2DSq(NextBuildPosition, HiveToSiege->Location) < sqrf(UTIL_MetresToGoldSrcUnits(22.0f)))
		{
			bool bSuccess = AICOMM_DeployStructure(pBot, NextStructure, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);

			if (bSuccess) { return true; }
		}

		NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), SiegeLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(NextBuildPosition) && vDist2DSq(NextBuildPosition, HiveToSiege->Location) < sqrf(UTIL_MetresToGoldSrcUnits(22.0f)))
		{
			bool bSuccess = AICOMM_DeployStructure(pBot, NextStructure, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);

			if (bSuccess) { return true; }
		}

		NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), SiegeLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(NextBuildPosition) && vDist2DSq(NextBuildPosition, HiveToSiege->Location) < sqrf(UTIL_MetresToGoldSrcUnits(22.0f)))
		{
			bool bSuccess = AICOMM_DeployStructure(pBot, NextStructure, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);

			if (bSuccess) { return true; }
		}

		return false;
	}

	if (!ExistingTF.IsValid()) { return false; }

	if ((ExistingTF.StructureStatusFlags & STRUCTURE_STATUS_RESEARCHING)) { return false; }

	if (ExistingTF.StructureType != STRUCTURE_MARINE_ADVTURRETFACTORY)
	{
		return AICOMM_UpgradeStructure(pBot, &ExistingTF);
	}
	
	StructureFilter.DeployableTypes = STRUCTURE_MARINE_SIEGETURRET;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

	int NumSiegeTurrets = AITAC_GetNumDeployablesNearLocation(ExistingTF.Location, &StructureFilter);

	if (NumSiegeTurrets == 0 || (NumSiegeTurrets < 3 && UTIL_IsStructureElectrified(ExistingTF.edict)))
	{
		Vector NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), ExistingTF.Location, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(NextBuildPosition) && vDist2DSq(NextBuildPosition, HiveToSiege->Location) <= sqrf(BALANCE_VAR(kSiegeTurretRange)))
		{
			bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_SIEGETURRET, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);

			if (bSuccess) { return true; }
		}

		NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), ExistingTF.Location, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(NextBuildPosition) && vDist2DSq(NextBuildPosition, HiveToSiege->Location) <= sqrf(BALANCE_VAR(kSiegeTurretRange)))
		{
			bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_SIEGETURRET, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);

			if (bSuccess) { return true; }
		}

		NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), ExistingTF.Location, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(NextBuildPosition) && vDist2DSq(NextBuildPosition, HiveToSiege->Location) <= sqrf(BALANCE_VAR(kSiegeTurretRange)))
		{
			bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_SIEGETURRET, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);

			if (bSuccess) { return true; }
		}

	}

	if (!UTIL_IsStructureElectrified(ExistingTF.edict))
	{
		return AICOMM_ResearchTech(pBot, &ExistingTF, RESEARCH_ELECTRICAL);
	}

	return false;

}

bool AICOMM_PerformNextSecureHiveAction(AvHAIPlayer* pBot, const AvHAIHiveDefinition* HiveToSecure)
{
	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_PHASEGATE;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
	StructureFilter.DeployableTeam = pBot->Player->GetTeam();
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	StructureFilter.ReachabilityTeam = pBot->Player->GetTeam();
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	AvHAIBuildableStructure ExistingStructure = AITAC_FindClosestDeployableToLocation(HiveToSecure->FloorLocation, &StructureFilter);
	AvHAIBuildableStructure ExistingPG;
	AvHAIBuildableStructure ExistingTF;

	Vector OutpostLocation = (ExistingStructure.IsValid()) ? ExistingStructure.Location : HiveToSecure->FloorLocation;

	if (HiveToSecure->HiveResNodeRef && HiveToSecure->HiveResNodeRef->OwningTeam == TEAM_IND)
	{
		AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_RESTOWER, HiveToSecure->HiveResNodeRef->Location);
		return true;
	}

	if (ExistingStructure.IsValid())
	{
		if (ExistingStructure.StructureType == STRUCTURE_MARINE_PHASEGATE)
		{
			ExistingPG = ExistingStructure;
		}
		else
		{
			ExistingTF = ExistingStructure;
		}
	}

	if (AITAC_PhaseGatesAvailable(pBot->Player->GetTeam()))
	{
		if (!ExistingPG.IsValid())
		{
			StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;

			ExistingPG = AITAC_FindClosestDeployableToLocation(OutpostLocation, &StructureFilter);

			if (!ExistingPG.IsValid())
			{
				Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), OutpostLocation, UTIL_MetresToGoldSrcUnits(5.0f));

				if (!vIsZero(BuildLocation))
				{
					bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PHASEGATE, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);

					if (bSuccess) { return true; }
				}

				BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), OutpostLocation, UTIL_MetresToGoldSrcUnits(5.0f));

				if (!vIsZero(BuildLocation))
				{
					bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PHASEGATE, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);

					if (bSuccess) { return true; }
				}

				BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), HiveToSecure->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

				if (!vIsZero(BuildLocation))
				{
					bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PHASEGATE, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);

					if (bSuccess) { return true; }
				}

				return false;
			}			
		}
	}

	if (!ExistingTF.IsValid())
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRETFACTORY;

		ExistingTF = AITAC_FindClosestDeployableToLocation(OutpostLocation, &StructureFilter);

		if (!ExistingTF.IsValid())
		{
			// First, try and put the TF near any existing phasegate (if it exists)
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), OutpostLocation, UTIL_MetresToGoldSrcUnits(3.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRETFACTORY, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);

				if (bSuccess) { return true; }
			}

			// That failed, now try expanding the radius a bit and ignoring reachability
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), OutpostLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRETFACTORY, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);

				if (bSuccess) { return true; }
			}

			// That failed too, try putting it anywhere near the hive location
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), HiveToSecure->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRETFACTORY, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);

				if (bSuccess) { return true; }
			}

			return false;
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

	int NumTurrets = AITAC_GetNumDeployablesNearLocation(ExistingTF.Location, &StructureFilter);

	if (NumTurrets < 5)
	{
		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), ExistingTF.Location, (BALANCE_VAR(kCommandStationBuildDistance) * 0.8f));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRET, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);

			if (bSuccess) { return true; }
		}

		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), ExistingTF.Location, (BALANCE_VAR(kCommandStationBuildDistance) * 0.8f));

		if (!vIsZero(BuildLocation))
		{
			bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRET, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);

			if (bSuccess) { return true; }
		}

		return false;
	}

	return false;
}

bool AICOMM_BuildInfantryPortal(AvHAIPlayer* pBot, edict_t* CommChair)
{
	if (FNullEnt(CommChair) || !UTIL_StructureIsFullyBuilt(CommChair)) { return false; }

	Vector BuildLocation = AITAC_GetRandomBuildHintInLocation(STRUCTURE_MARINE_INFANTRYPORTAL, CommChair->v.origin, BALANCE_VAR(kCommandStationBuildDistance));

	if (vIsZero(BuildLocation))
	{
		
		DeployableSearchFilter ExistingPortalFilter;
		ExistingPortalFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
		ExistingPortalFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
		ExistingPortalFilter.DeployableTeam = pBot->Player->GetTeam();
		ExistingPortalFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
		ExistingPortalFilter.ReachabilityTeam = pBot->Player->GetTeam();

		AvHAIBuildableStructure ExistingInfantryPortal = AITAC_FindClosestDeployableToLocation(CommChair->v.origin, &ExistingPortalFilter);

		// First see if we can place the next infantry portal next to the first one
		if (ExistingInfantryPortal.IsValid())
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInDonutIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), ExistingInfantryPortal.edict->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), UTIL_MetresToGoldSrcUnits(3.0f));

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_INFANTRYPORTAL, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}
		}

		Vector SearchPoint = ZERO_VECTOR;

		DeployableSearchFilter ResNodeFilter;
		ResNodeFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
		ResNodeFilter.ReachabilityTeam = pBot->Player->GetTeam();

		const AvHAIResourceNode* ResNode = AITAC_FindNearestResourceNodeToLocation(CommChair->v.origin, &ResNodeFilter);

		if (ResNode)
		{
			SearchPoint = ResNode->Location;
		}
		else
		{
			return false;
		}

		Vector NearestPointToChair = FindClosestNavigablePointToDestination(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), SearchPoint, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(NearestPointToChair))
		{
			float Distance = vDist2D(NearestPointToChair, CommChair->v.origin);
			float RandomDist = UTIL_MetresToGoldSrcUnits(5.0f) - Distance;

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestPointToChair, RandomDist);

			if (!vIsZero(BuildLocation))
			{
				bool bSuccess = AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_INFANTRYPORTAL, BuildLocation, STRUCTURE_PURPOSE_BASE);

				if (bSuccess) { return true; }
			}

		}

		BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
	}

	if (vIsZero(BuildLocation)) { return false; }

	return AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_INFANTRYPORTAL, BuildLocation, STRUCTURE_PURPOSE_BASE);
}

bool AICOMM_CheckForNextRecycleAction(AvHAIPlayer* pBot)
{
	DeployableSearchFilter UnreachableFilter;
	UnreachableFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
	UnreachableFilter.DeployableTeam = pBot->Player->GetTeam();
	UnreachableFilter.ReachabilityTeam = pBot->Player->GetTeam();
	UnreachableFilter.ReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
	UnreachableFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_RESEARCHING;
	
	AvHAIBuildableStructure UnreachableStructure = AITAC_FindClosestDeployableToLocation(AITAC_GetCommChairLocation(pBot->Player->GetTeam()), &UnreachableFilter);

	// Recycle any structures which are unreachable (e.g. sunk below the map)
	if (UnreachableStructure.IsValid())
	{
		return AICOMM_RecycleStructure(pBot, &UnreachableStructure);
	}

	vector<AvHAIHiveDefinition*> Hives = AITAC_GetAllHives();

	for (auto HiveIt = Hives.begin(); HiveIt != Hives.end(); HiveIt++)
	{
		AvHAIHiveDefinition* Hive = (*HiveIt);

		// If the hive is still active or growing, then clearly we should keep any siege bases
		if (Hive->Status != HIVE_STATUS_UNBUILT) { continue; }

		// If the hive is empty, but we've not secured it yet, then keep any siege bases nearby in case we need to re-siege later
		DeployableSearchFilter SecuringStructuresFilter;
		SecuringStructuresFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_TURRET);
		SecuringStructuresFilter.DeployableTeam = pBot->Player->GetTeam();
		SecuringStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		SecuringStructuresFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		SecuringStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		vector<AvHAIBuildableStructure> NearbySecuringStructures = AITAC_FindAllDeployables(Hive->Location, &SecuringStructuresFilter);

		bool bHiveHasPG = false;
		bool bHiveHasTF = false;
		bool bHiveHasTurret = false;

		for (auto SecureIt = NearbySecuringStructures.begin(); SecureIt != NearbySecuringStructures.end(); SecureIt++)
		{
			AvHAIBuildableStructure Structure = (*SecureIt);

			if (Structure.Purpose == STRUCTURE_PURPOSE_FORTIFY)
			{
				if (Structure.StructureType == STRUCTURE_MARINE_PHASEGATE)
				{
					bHiveHasPG = true;
				}

				if (Structure.StructureType == STRUCTURE_MARINE_TURRETFACTORY || Structure.StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
				{
					bHiveHasTF = true;
				}

				if (Structure.StructureType == STRUCTURE_MARINE_TURRET)
				{
					bHiveHasTurret = true;
				}
			}
		}

		bool bHiveIsSecureEnough = (bHiveHasPG && bHiveHasTF && bHiveHasTurret);

		if (!bHiveIsSecureEnough) { continue; }

		// Ok, hive is secured by us, now we can check if there are any siege objects to be got rid of
		DeployableSearchFilter RedundantFilter;
		RedundantFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
		RedundantFilter.DeployableTeam = pBot->Player->GetTeam();
		RedundantFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		RedundantFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(30.0f);

		vector<AvHAIBuildableStructure> NearbyStructures = AITAC_FindAllDeployables(Hive->Location, &RedundantFilter);

		for (auto StructIt = NearbyStructures.begin(); StructIt != NearbyStructures.end(); StructIt++)
		{
			AvHAIBuildableStructure Structure = (*StructIt);

			if (Structure.Purpose == STRUCTURE_PURPOSE_SIEGE)
			{
				// Check for the potential situation where we can siege more than one hive at a time
				const AvHAIHiveDefinition* NearestHive = AITAC_GetNonEmptyHiveNearestLocation(Structure.Location);

				if (!NearestHive || vDist2DSq(NearestHive->Location, Structure.Location) > sqrf(UTIL_MetresToGoldSrcUnits(25.0f)))
				{
					return AICOMM_RecycleStructure(pBot, &Structure);
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

	if (NextRequest->RequestType == BUILD_OBSERVATORY && !AITAC_ResearchIsComplete(CommanderTeam, TECH_RESEARCH_PHASETECH))
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

	Vector IdealDeployLocation = Requestor->v.origin + (UTIL_GetForwardVector2D(Requestor->v.angles) * 75.0f);
	Vector ProjectedDeployLocation = AdjustPointForPathfinding(IdealDeployLocation, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

	if (!vIsZero(ProjectedDeployLocation))
	{
		bool bSuccess = AICOMM_DeployStructure(pBot, StructureToDeploy, ProjectedDeployLocation, STRUCTURE_PURPOSE_NONE);

		if (bSuccess)
		{
			NextRequest->bResponded = true;
			return true;
		}
	}

	Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

	if (!vIsZero(DeployLocation))
	{
		bool bSuccess = AICOMM_DeployStructure(pBot, StructureToDeploy, DeployLocation, STRUCTURE_PURPOSE_NONE);

		if (bSuccess)
		{
			NextRequest->bResponded = true;
			return true;
		}
	}

	DeployLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

	if (!vIsZero(DeployLocation))
	{
		bool bSuccess = AICOMM_DeployStructure(pBot, StructureToDeploy, DeployLocation, STRUCTURE_PURPOSE_NONE);

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

void AICOMM_SetDropHealthAction(AvHAIPlayer* pBot, commander_action* Action, edict_t* Recipient)
{
	AICOMM_ClearAction(Action);

	Action->ActionType = ACTION_DEPLOY;
	Action->ActionTarget = Recipient;
	Action->ItemToPlace = DEPLOYABLE_ITEM_HEALTHPACK;
	Action->NumDesiredInstances = (Recipient->v.health < 50.0f) ? 2 : 1;
}

void AICOMM_SetDropAmmoAction(AvHAIPlayer* pBot, commander_action* Action, edict_t* Recipient)
{
	AICOMM_ClearAction(Action);

	Action->ActionType = ACTION_DEPLOY;
	Action->ActionTarget = Recipient;
	Action->ItemToPlace = DEPLOYABLE_ITEM_AMMO;
	Action->NumDesiredInstances = 4;
}

void AICOMM_SetDeployStructureAction(AvHAIPlayer* pBot, commander_action* Action, AvHAIDeployableStructureType StructureToBuild, const Vector Location, bool bIsUrgent)
{
	AICOMM_ClearAction(Action);

	Action->ActionType = ACTION_DEPLOY;
	Action->StructureToBuild = StructureToBuild;
	Action->BuildLocation = Location;
	Action->NumDesiredInstances = 1;
	Action->bIsActionUrgent = bIsUrgent;
}

void AICOMM_SetDeployItemAction(AvHAIPlayer* pBot, commander_action* Action, AvHAIDeployableItemType ItemToBuild, const Vector Location, bool bIsUrgent)
{
	AICOMM_ClearAction(Action);

	Action->ActionType = ACTION_DEPLOY;
	Action->ItemToPlace = ItemToBuild;
	Action->BuildLocation = Location;
	Action->NumDesiredInstances = 1;
	Action->bIsActionUrgent = bIsUrgent;
}

void AICOMM_ClearAction(commander_action* Action)
{
	memset(Action, 0, sizeof(commander_action));
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

	AICOMM_UpdatePlayerOrders(pBot);

	if (AICOMM_CheckForNextRecycleAction(pBot)) { return; }
	if (AICOMM_CheckForNextSupportAction(pBot)) { return; }
	if (AICOMM_CheckForNextBuildAction(pBot)) { return; }
	if (AICOMM_CheckForNextResearchAction(pBot)) { return; }
	if (AICOMM_CheckForNextSupplyAction(pBot)) { return; }
}

bool AICOMM_IsCommanderActionValid(AvHAIPlayer* pBot, commander_action* Action)
{
	if (Action->NumActionAttempts > 5) { return false; }

	if (Action->bIsAwaitingBuildLink) { return true; }

	switch (Action->ActionType)
	{
	case ACTION_RECYCLE:
		return !FNullEnt(Action->ActionTarget) && AvHSHUGetIsMarineStructure((AvHUser3)Action->ActionTarget->v.iuser3) && !UTIL_StructureIsRecycling(Action->ActionTarget);
	case ACTION_UPGRADE:
		return !FNullEnt(Action->ActionTarget) && AITAC_StructureCanBeUpgraded(Action->ActionTarget);
	case ACTION_DEPLOY:
		return (Action->NumInstances < Action->NumDesiredInstances);
	case ACTION_RESEARCH:
	{
		if (Action->ResearchId == RESEARCH_ELECTRICAL)
		{
			return AITAC_ElectricalResearchIsAvailable(Action->ActionTarget);
		}
		return (AITAC_MarineResearchIsAvailable(pBot->Player->GetTeam(), Action->ResearchId) && !FNullEnt(Action->ActionTarget));
	}
	case ACTION_GIVEORDER:
		return Action->AssignedPlayer > -1;
	default:
		return false;
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

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		const AvHAIHiveDefinition* Hive = (*it);

		if (Hive->Status != HIVE_STATUS_UNBUILT) { continue; }

		if (AICOMM_IsHiveFullySecured(CommanderBot, Hive, false)) { continue; }

		Vector SecureLocation = Hive->FloorLocation;

		DeployableSearchFilter StructureFilter;
		StructureFilter.DeployableTeam = CommanderTeam;
		StructureFilter.ReachabilityTeam = CommanderTeam;
		StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
		StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

		StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

		AvHAIBuildableStructure ExistingStructure = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &StructureFilter);

		if (ExistingStructure.IsValid() && UTIL_QuickTrace(nullptr, UTIL_GetCentreOfEntity(ExistingStructure.edict), Hive->Location))
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
			SearchFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(8.0f);

			NumTurrets = AITAC_GetNumDeployablesNearLocation(Structure.Location, &SearchFilter);

		}

	}

	const AvHAIResourceNode* ResNode = Hive->HiveResNodeRef;

	bool bSecuredResNode = (!ResNode || (ResNode->bIsOccupied && ResNode->OwningTeam == CommanderTeam && UTIL_StructureIsFullyBuilt(ResNode->ActiveTowerEntity)));

	return ((!bPhaseGatesAvailable || bHasPhaseGate) && bHasTurretFactory && (!bIncludeElectrical || bTurretFactoryElectrified) && NumTurrets >= 5 && bSecuredResNode);
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

	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	Vector BaseLocation = AITAC_GetTeamStartingLocation(BotTeam);

	DeployableSearchFilter BaseStructureFilter;
	BaseStructureFilter.DeployableTypes = (STRUCTURE_MARINE_INFANTRYPORTAL | STRUCTURE_MARINE_COMMCHAIR);
	BaseStructureFilter.DeployableTeam = BotTeam;
	BaseStructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	BaseStructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	vector<AvHAIBuildableStructure> BaseStructures = AITAC_FindAllDeployables(BaseLocation, &BaseStructureFilter);

	bool bHasInfantryPortals = false;
	bool bBaseUnderAttack = false;

	for (auto it = BaseStructures.begin(); it != BaseStructures.end(); it++)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		if (ThisStructure.StructureStatusFlags & STRUCTURE_STATUS_UNDERATTACK)
		{
			bBaseUnderAttack = true;
		}

		if (ThisStructure.StructureType == STRUCTURE_MARINE_INFANTRYPORTAL)
		{
			bHasInfantryPortals = true;
		}
	}

	if (!bBaseUnderAttack && bHasInfantryPortals) { return false; }

	int EnemyStrength = 0;
	int DefenderStrength = AITAC_GetNumPlayersOfTeamInArea(BotTeam, BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_COMMANDER_PLAYER);

	if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE)
	{
		EnemyStrength = AITAC_GetNumPlayersOfTeamInArea(EnemyTeam, BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_COMMANDER_PLAYER);
	}
	else
	{
		int NumSkulks = AITAC_GetNumPlayersOfTeamAndClassInArea(EnemyTeam, BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_ALIEN_PLAYER1);
		int NumFades = AITAC_GetNumPlayersOfTeamAndClassInArea(EnemyTeam, BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_ALIEN_PLAYER4);
		int NumOnos = AITAC_GetNumPlayersOfTeamAndClassInArea(EnemyTeam, BaseLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_ALIEN_PLAYER5);

		EnemyStrength = NumSkulks + (NumFades * 2) + (NumOnos * 2);
	}
	
	if (EnemyStrength >= 3 && EnemyStrength >= DefenderStrength * 3)
	{
		return AICOMM_ResearchTech(pBot, &Observatory, RESEARCH_DISTRESSBEACON);
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