
#include "AvHAICommander.h"
#include "AvHAITactical.h"
#include "AvHAIMath.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIWeaponHelper.h"
#include "AvHAINavigation.h"
#include "AvHAITask.h"
#include "AvHAIHelper.h"

#include "../AvHSharedUtil.h"
#include "../AvHServerUtil.h"

bool AICOMM_DeployStructure(AvHAIPlayer* pBot, const AvHAIDeployableStructureType StructureToDeploy, const Vector Location, StructurePurpose Purpose)
{
	if (vIsZero(Location)) { return false; }

	AvHMessageID StructureID = UTIL_StructureTypeToImpulseCommand(StructureToDeploy);

	Vector BuildLocation = Location;
	BuildLocation.z += 4.0f;

	if (!AvHSHUGetIsSiteValidForBuild(StructureID, &BuildLocation)) { return false; }

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

	bool theSuccess = (AvHSUBuildTechForPlayer(StructureID, BuildLocation, pBot->Player) != NULL);

	if (!theSuccess) { return false; }

	pBot->Player->PayPurchaseCost(theCost);

	pBot->next_commander_action_time = gpGlobals->time + 0.2f;

	return true;
}

bool AICOMM_ResearchTech(AvHAIPlayer* pBot, AvHAIBuildableStructure* StructureToResearch, AvHMessageID Research)
{
	if (FNullEnt(StructureToResearch->edict)) { return false; }

	// Don't do anything if the structure is being recycled, or we DON'T want to recycle but the structure is already busy
	if (StructureToResearch->EntityRef->GetIsRecycling() || (Research != BUILD_RECYCLE && StructureToResearch->EntityRef->GetIsResearching())) { return false; }

	int StructureIndex = ENTINDEX(StructureToResearch->edict);

	if (StructureIndex < 0) { return false; }

	if (!StructureToResearch->EntityRef->GetIsTechnologyAvailable(Research)) { return false; }

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

	AvHAIBuildableStructure* TF = AITAC_FindClosestDeployableToLocation(HiveToSecure->FloorLocation, &TFFilter);

	if (!TF)
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

	int NumTurrets = AITAC_GetNumDeployablesNearLocation(TF->Location, &TurretFilter);

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

	switch (Request->RequestType)
	{
		case COMMANDER_NEXTHEALTH:
			return Request->Requestor->v.health < Request->Requestor->v.max_health;
		default:
			return true;
	}

	return true;
}

bool AICOMM_CheckForNextBuildAction(AvHAIPlayer* pBot, commander_action* Action)
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
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

	int NumInfantryPortals = AITAC_GetNumDeployablesNearLocation(CommChair->v.origin, &StructureFilter);

	if (NumInfantryPortals < 2)
	{
		if (AICOMM_BuildInfantryPortal(pBot, CommChair, Action))
		{
			return true;
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY;

	AvHAIBuildableStructure* BaseArmoury = AITAC_FindClosestDeployableToLocation(CommChair->v.origin, &StructureFilter);

	if (!BaseArmoury)
	{

		StructureFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

		AvHAIBuildableStructure* NearestInfantryPortal = AITAC_FindClosestDeployableToLocation(CommChair->v.origin, &StructureFilter);

		Vector BuildLocation = ZERO_VECTOR;

		if (NearestInfantryPortal)
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestInfantryPortal->Location, UTIL_MetresToGoldSrcUnits(5.0f));
		}

		if (vIsZero(BuildLocation))
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));
		}

		if (!vIsZero(BuildLocation))
		{
			if (AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_ARMOURY, BuildLocation))
			{
				return true;
			}
		}
	}

	const AvHAIHiveDefinition* HiveUnderSiege = AITAC_GetNearestHiveUnderActiveSiege(pBot->Player->GetTeam(), AITAC_GetCommChairLocation(pBot->Player->GetTeam()));

	if (HiveUnderSiege)
	{
		bool bAlreadyScanning = AITAC_ItemExistsInLocation(HiveUnderSiege->Location, DEPLOYABLE_ITEM_SCAN, pBot->Player->GetTeam(), AI_REACHABILITY_NONE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);

		if (!bAlreadyScanning)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), HiveUnderSiege->FloorLocation, UTIL_MetresToGoldSrcUnits(3.0f));

			if (AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_SCAN, BuildLocation))
			{
				return true;
			}
		}
	}

	if (AITAC_PhaseGatesAvailable(TeamNumber))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		bool bPhaseNearBase = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);

		if (!bPhaseNearBase)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

			if (AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PHASEGATE, BuildLocation))
			{
				return true;
			}
		}
	}

	const AvHAIResourceNode* CappableNode = AICOMM_GetNearestResourceNodeCapOpportunity(TeamNumber, CommChair->v.origin);

	if (CappableNode)
	{
		if (AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_RESTOWER, CappableNode->Location))
		{
			return true;
		}
	}

	const AvHAIHiveDefinition* HiveToSecure = AICOMM_GetEmptyHiveOpportunityNearestLocation(pBot, AITAC_GetCommChairLocation(TeamNumber));

	if (HiveToSecure)
	{
		if (AICOMM_PerformNextSecureHiveAction(pBot, HiveToSecure, Action))
		{
			return true;
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
	StructureFilter.MaxSearchRadius = 0.0f;

	bool bHasArmsLab = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);

	if (!bHasArmsLab)
	{
		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (!vIsZero(BuildLocation))
		{
			if (AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_ARMSLAB, BuildLocation))
			{
				return true;
			}
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_OBSERVATORY;

	bool bHasObservatory = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);

	if (!bHasObservatory)
	{
		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (!vIsZero(BuildLocation))
		{
			if (AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_OBSERVATORY, BuildLocation))
			{
				return true;
			}
		}
	}

	if (!AITAC_ResearchIsComplete(TeamNumber, TECH_RESEARCH_PHASETECH))
	{
		return false;
	}

	const AvHAIHiveDefinition* HiveToSiege = AICOMM_GetHiveSiegeOpportunityNearestLocation(pBot, AITAC_GetCommChairLocation(TeamNumber));

	if (HiveToSiege)
	{
		if (AICOMM_PerformNextSiegeHiveAction(pBot, HiveToSiege, Action))
		{
			return true;
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_ADVARMOURY;

	bool bHasAdvArmoury = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);
	bool bIsResearchingArmoury = false;

	if (!bHasAdvArmoury)
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMOURY;
		StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
		StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* NearestArmoury = AITAC_FindClosestDeployableToLocation(CommChair->v.origin, &StructureFilter);

		if (NearestArmoury)
		{
			bIsResearchingArmoury = UTIL_StructureIsUpgrading(NearestArmoury->edict);

			if (!bIsResearchingArmoury)
			{
				if (AICOMM_UpgradeStructure(pBot, NearestArmoury))
				{
					return true;
				}
			}
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_PROTOTYPELAB;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
	StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_NONE;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	bool bHasPrototypeLab = AITAC_DeployableExistsAtLocation(CommChair->v.origin, &StructureFilter);

	if (!bHasPrototypeLab && bHasAdvArmoury)
	{
		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInDonutIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), BaseArmoury->Location, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(BuildLocation))
		{
			if (AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PROTOTYPELAB, BuildLocation))
			{
				return true;
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

		AvHAIBuildableStructure* ResTower = AITAC_FindFurthestDeployableFromLocation(CommChair->v.origin, &StructureFilter);

		if (ResTower && AITAC_ElectricalResearchIsAvailable(ResTower->edict))
		{
			if (AICOMM_ResearchTech(pBot, ResTower, RESEARCH_ELECTRICAL))
			{
				return true;
			}
		}
	}

	return false;
}

bool AICOMM_CheckForNextResearchAction(AvHAIPlayer* pBot, commander_action* Action)
{
	AvHTeamNumber CommanderTeam = pBot->Player->GetTeam();

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTeam = CommanderTeam;
	StructureFilter.ReachabilityTeam = CommanderTeam;
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_GRENADES))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* Armoury = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (Armoury)
		{
			return AICOMM_ResearchTech(pBot, Armoury, RESEARCH_GRENADES);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_ARMOR_ONE))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* ArmsLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (ArmsLab)
		{
			return AICOMM_ResearchTech(pBot, ArmsLab, RESEARCH_ARMOR_ONE);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_WEAPONS_ONE))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* ArmsLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (ArmsLab)
		{
			return AICOMM_ResearchTech(pBot, ArmsLab, RESEARCH_WEAPONS_ONE);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_PHASETECH))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_OBSERVATORY;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* Observatory = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (Observatory)
		{
			return AICOMM_ResearchTech(pBot, Observatory, RESEARCH_PHASETECH);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_MOTIONTRACK))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_OBSERVATORY;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* Observatory = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (Observatory)
		{
			return AICOMM_ResearchTech(pBot, Observatory, RESEARCH_MOTIONTRACK);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_ARMOR_TWO))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* ArmsLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (ArmsLab)
		{
			return AICOMM_ResearchTech(pBot, ArmsLab, RESEARCH_ARMOR_TWO);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_WEAPONS_TWO))
	{

		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* ArmsLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (ArmsLab)
		{
			return AICOMM_ResearchTech(pBot, ArmsLab, RESEARCH_WEAPONS_TWO);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_CATALYSTS))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* ArmsLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (ArmsLab)
		{
			return AICOMM_ResearchTech(pBot, ArmsLab, RESEARCH_CATALYSTS);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_HEAVYARMOR))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_PROTOTYPELAB;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* ProtoLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (ProtoLab)
		{
			return AICOMM_ResearchTech(pBot, ProtoLab, RESEARCH_HEAVYARMOR);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_JETPACKS))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_PROTOTYPELAB;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* ProtoLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (ProtoLab)
		{
			return AICOMM_ResearchTech(pBot, ProtoLab, RESEARCH_JETPACKS);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_ARMOR_THREE))
	{

		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* ArmsLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (ArmsLab)
		{
			return AICOMM_ResearchTech(pBot, ArmsLab, RESEARCH_ARMOR_THREE);
		}
	}

	if (AITAC_MarineResearchIsAvailable(CommanderTeam, RESEARCH_WEAPONS_THREE))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMSLAB;
		StructureFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_RESEARCHING;

		AvHAIBuildableStructure* ArmsLab = AITAC_FindClosestDeployableToLocation(AITAC_GetTeamStartingLocation(CommanderTeam), &StructureFilter);

		if (ArmsLab)
		{
			return AICOMM_ResearchTech(pBot, ArmsLab, RESEARCH_WEAPONS_THREE);
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

		AvHAIBuildableStructure* BuiltPhaseGate = AITAC_FindClosestDeployableToLocation(Hive->Location, &StructureFilter);

		// If we have a phase gate already in place, then keep building as long as someone is there. If we don't have a phase gate, only build if there is a marine who isn't sighted by the enemy (to allow element of surprise)
		if (BuiltPhaseGate)
		{
			int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(CommanderTeam, BuiltPhaseGate->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, CommanderBot->Edict, AVH_USER3_COMMANDER_PLAYER);

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

		float ThisDist = vDist2DSq(ResNode->Location, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = ResNode;
			MinDist = ThisDist;
		}
	}

	return Result;
}

bool AICOMM_PerformNextSiegeHiveAction(AvHAIPlayer* pBot, const AvHAIHiveDefinition* HiveToSiege, commander_action* Action)
{
	AvHTeamNumber CommanderTeam = pBot->Player->GetTeam();

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTeam = CommanderTeam;
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	StructureFilter.ReachabilityTeam = CommanderTeam;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(25.0f);

	Vector SiegeLocation = ZERO_VECTOR;
	AvHAIBuildableStructure* ExistingPG = nullptr;

	edict_t* NearestBuilder = nullptr;

	if (AITAC_PhaseGatesAvailable(CommanderTeam))
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;

		ExistingPG = AITAC_FindClosestDeployableToLocation(HiveToSiege->Location, &StructureFilter);

		if (ExistingPG)
		{
			SiegeLocation = ExistingPG->Location;
		}

	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY;

	AvHAIBuildableStructure* ExistingTF = AITAC_FindClosestDeployableToLocation(HiveToSiege->Location, &StructureFilter);

	if (vIsZero(SiegeLocation))
	{
		if (ExistingTF)
		{
			SiegeLocation = ExistingTF->Location;
		}
		else
		{
			NearestBuilder = AITAC_GetNearestHiddenPlayerInLocation(CommanderTeam, HiveToSiege->Location, UTIL_MetresToGoldSrcUnits(20.0f));

			if (FNullEnt(NearestBuilder)) { return false; }

			SiegeLocation = NearestBuilder->v.origin;
		}
	}

	if (FNullEnt(NearestBuilder))
	{
		NearestBuilder = AITAC_GetNearestHiddenPlayerInLocation(CommanderTeam, SiegeLocation, UTIL_MetresToGoldSrcUnits(20.0f));
	}

	if (FNullEnt(NearestBuilder)) { return false; }

	Vector NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), SiegeLocation, UTIL_MetresToGoldSrcUnits(3.0f));

	if (vIsZero(NextBuildPosition))
	{
		NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), SiegeLocation, UTIL_MetresToGoldSrcUnits(3.0f));
	}

	if (!ExistingPG)
	{
		return AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PHASEGATE, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);
	}

	if (!ExistingTF)
	{
		return AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRETFACTORY, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

	AvHAIBuildableStructure* ExistingArmoury = AITAC_FindClosestDeployableToLocation(SiegeLocation, &StructureFilter);

	if (!ExistingArmoury)
	{
		return AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_ARMOURY, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);
	}

	if (ExistingTF->StructureType != STRUCTURE_MARINE_ADVTURRETFACTORY)
	{
		return AICOMM_UpgradeStructure(pBot, ExistingTF);
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_SIEGETURRET;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

	int NumSiegeTurrets = AITAC_GetNumDeployablesNearLocation(ExistingTF->Location, &StructureFilter);

	if (NumSiegeTurrets == 0 || (NumSiegeTurrets < 5 && UTIL_IsStructureElectrified(ExistingTF->edict)))
	{
		SiegeLocation = ExistingTF->Location;

		NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), SiegeLocation, UTIL_MetresToGoldSrcUnits(3.0f));

		if (vIsZero(NextBuildPosition))
		{
			NextBuildPosition = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), SiegeLocation, UTIL_MetresToGoldSrcUnits(3.0f));
		}

		return AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_SIEGETURRET, NextBuildPosition, STRUCTURE_PURPOSE_SIEGE);
	}

	if (!UTIL_IsStructureElectrified(ExistingTF->edict))
	{
		return AICOMM_ResearchTech(pBot, ExistingTF, RESEARCH_ELECTRICAL);
	}

	return false;

}

bool AICOMM_PerformNextSecureHiveAction(AvHAIPlayer* pBot, const AvHAIHiveDefinition* HiveToSecure, commander_action* Action)
{
	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_PHASEGATE;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
	StructureFilter.DeployableTeam = pBot->Player->GetTeam();
	StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	StructureFilter.ReachabilityTeam = pBot->Player->GetTeam();
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	AvHAIBuildableStructure* ExistingStructure = AITAC_FindClosestDeployableToLocation(HiveToSecure->FloorLocation, &StructureFilter);
	AvHAIBuildableStructure* ExistingPG = nullptr;
	AvHAIBuildableStructure* ExistingTF = nullptr;

	Vector OutpostLocation = (ExistingStructure) ? ExistingStructure->Location : HiveToSecure->FloorLocation;

	if (ExistingStructure)
	{
		if (ExistingStructure->StructureType == STRUCTURE_MARINE_PHASEGATE)
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
		if (!ExistingPG)
		{
			StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;

			ExistingPG = AITAC_FindClosestDeployableToLocation(OutpostLocation, &StructureFilter);

			if (!ExistingPG)
			{
				Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), OutpostLocation, UTIL_MetresToGoldSrcUnits(5.0f));

				if (!vIsZero(BuildLocation))
				{
					return AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_PHASEGATE, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);
				}

				return false;
			}			
		}
	}

	if (!ExistingTF)
	{
		StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRETFACTORY;

		ExistingTF = AITAC_FindClosestDeployableToLocation(OutpostLocation, &StructureFilter);

		if (!ExistingTF)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), OutpostLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			if (!vIsZero(BuildLocation))
			{
				return AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRETFACTORY, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);
			}

			return false;
		}
	}

	StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

	int NumTurrets = AITAC_GetNumDeployablesNearLocation(ExistingTF->Location, &StructureFilter);

	if (NumTurrets < 5)
	{
		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), ExistingTF->Location, UTIL_MetresToGoldSrcUnits(3.0f));

		if (!vIsZero(BuildLocation))
		{
			return AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_TURRET, BuildLocation, STRUCTURE_PURPOSE_FORTIFY);
		}

		return false;
	}

	if (!UTIL_IsStructureElectrified(ExistingTF->edict))
	{
		return AICOMM_ResearchTech(pBot, ExistingTF, RESEARCH_ELECTRICAL);
	}


	return false;
}

bool AICOMM_BuildInfantryPortal(AvHAIPlayer* pBot, edict_t* CommChair, commander_action* Action)
{
	if (FNullEnt(CommChair) || !UTIL_StructureIsFullyBuilt(CommChair)) { return false; }

	Vector BuildLocation = ZERO_VECTOR;

	DeployableSearchFilter ExistingPortalFilter;
	ExistingPortalFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
	ExistingPortalFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
	ExistingPortalFilter.DeployableTeam = pBot->Player->GetTeam();
	ExistingPortalFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
	ExistingPortalFilter.ReachabilityTeam = pBot->Player->GetTeam();

	AvHAIBuildableStructure* ExistingInfantryPortal = AITAC_FindClosestDeployableToLocation(CommChair->v.origin, &ExistingPortalFilter);

	// First see if we can place the next infantry portal next to the first one
	if (ExistingInfantryPortal)
	{
		BuildLocation = UTIL_GetRandomPointOnNavmeshInDonutIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), ExistingInfantryPortal->edict->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), UTIL_MetresToGoldSrcUnits(3.0f));
	}

	if (vIsZero(BuildLocation))
	{
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

		if (NearestPointToChair != ZERO_VECTOR)
		{
			float Distance = vDist2D(NearestPointToChair, CommChair->v.origin);
			float RandomDist = UTIL_MetresToGoldSrcUnits(5.0f) - Distance;

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), NearestPointToChair, RandomDist);

		}
		else
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), CommChair->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
		}
	}

	if (vIsZero(BuildLocation)) { return false; }

	return AICOMM_DeployStructure(pBot, STRUCTURE_MARINE_INFANTRYPORTAL, BuildLocation);
}

bool AICOMM_CheckForNextRecycleAction(AvHAIPlayer* pBot)
{
	DeployableSearchFilter UnreachableFilter;
	UnreachableFilter.DeployableTeam = pBot->Player->GetTeam();
	UnreachableFilter.ReachabilityTeam = pBot->Player->GetTeam();
	UnreachableFilter.ReachabilityFlags = AI_REACHABILITY_UNREACHABLE;
	UnreachableFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_RESEARCHING;
	
	AvHAIBuildableStructure* UnreachableStructure = AITAC_FindClosestDeployableToLocation(AITAC_GetCommChairLocation(pBot->Player->GetTeam()), &UnreachableFilter);

	// Recycle any structures which are unreachable (e.g. sunk below the map)
	if (UnreachableStructure)
	{
		return AICOMM_RecycleStructure(pBot, UnreachableStructure);
	}

	vector<AvHAIHiveDefinition*> Hives = AITAC_GetAllHives();

	for (auto HiveIt = Hives.begin(); HiveIt != Hives.end(); HiveIt++)
	{
		AvHAIHiveDefinition* Hive = (*HiveIt);

		if (Hive->Status != HIVE_STATUS_UNBUILT) { continue; }

		DeployableSearchFilter RedundantFilter;
		RedundantFilter.DeployableTeam = pBot->Player->GetTeam();
		RedundantFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		RedundantFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(25.0f);

		vector<AvHAIBuildableStructure*> NearbyStructures = AITAC_FindAllDeployables(Hive->Location, &RedundantFilter);

		for (auto StructIt = NearbyStructures.begin(); StructIt != NearbyStructures.end(); StructIt++)
		{
			AvHAIBuildableStructure* Structure = (*StructIt);

			if (Structure->Purpose == STRUCTURE_PURPOSE_SIEGE)
			{
				// Check for the potential situation where we can siege more than one hive at a time
				const AvHAIHiveDefinition* NearestHive = AITAC_GetNonEmptyHiveNearestLocation(Structure->Location);

				if (!NearestHive || vDist2DSq(NearestHive->Location, Structure->Location) > sqrf(UTIL_MetresToGoldSrcUnits(25.0f)))
				{
					return AICOMM_RecycleStructure(pBot, Structure);
				}
			}
		}
	}

	return false;
}

bool AICOMM_CheckForNextSupportAction(AvHAIPlayer* pBot)
{
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

	// We didn't find any requests outstanding
	if (!NextRequest) { return false; }
	
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

		if (WeaponType == WEAPON_INVALID)
		{
			bFillPrimaryWeapon = false;
			WeaponType = UTIL_GetPlayerSecondaryWeapon(thePlayer);
		}

		if (WeaponType == WEAPON_INVALID)
		{
			NextRequest->bResponded = true;
			return false;
		}

		int AmmoDeficit = (bFillPrimaryWeapon) ? (UTIL_GetPlayerPrimaryMaxAmmoReserve(thePlayer) - UTIL_GetPlayerPrimaryAmmoReserve(thePlayer)) : (UTIL_GetPlayerSecondaryMaxAmmoReserve(thePlayer) - UTIL_GetPlayerSecondaryAmmoReserve(thePlayer));
		int WeaponClipSize = (bFillPrimaryWeapon) ? UTIL_GetPlayerPrimaryWeaponMaxClipSize(thePlayer) : UTIL_GetPlayerSecondaryWeaponMaxClipSize(thePlayer);

		if (AmmoDeficit == 0)
		{
			NextRequest->bResponded = true;
			return false;
		}

		int DesiredNumAmmoPacks = (int)(ceilf((float)AmmoDeficit / (float)WeaponClipSize));
		DesiredNumAmmoPacks = clampi(DesiredNumAmmoPacks, 0, 5);

		int NumAmmoPacksPresent = AITAC_GetNumItemsInLocation(Requestor->v.origin, DEPLOYABLE_ITEM_AMMO, (AvHTeamNumber)Requestor->v.team, AI_REACHABILITY_MARINE, 0.0f, UTIL_MetresToGoldSrcUnits(5.0f), false);
		DesiredNumAmmoPacks -= NumAmmoPacksPresent;

		if (DesiredNumAmmoPacks > 0)
		{
			Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), Requestor->v.origin, UTIL_MetresToGoldSrcUnits(2.0f));
			bool bSuccess = AICOMM_DeployItem(pBot, DEPLOYABLE_ITEM_AMMO, DeployLocation);

			if (bSuccess)
			{
				if (DesiredNumAmmoPacks <= 1)
				{
					NextRequest->bResponded = true;
				}
			}

			return true;
		}
		else
		{
			NextRequest->bResponded = true;
		}

		return false;
	}

	if (NextRequest->RequestType == COMMANDER_NEXTIDLE)
	{
		NextRequest->bResponded = true;
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

		AvHAIBuildableStructure* NearestUnbuiltStructure = AITAC_FindClosestDeployableToLocation(AITAC_GetCommChairLocation(pBot->Player->GetTeam()), &StructureFilter);

		if (NearestUnbuiltStructure)
		{
			AITASK_SetBuildTask(pBot, &pBot->PrimaryBotTask, NearestUnbuiltStructure->edict, false);
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

	if (AICOMM_CheckForNextRecycleAction(pBot)) { return; }
	if (AICOMM_CheckForNextSupportAction(pBot)) { return; }
	if (AICOMM_CheckForNextBuildAction(pBot, &pBot->BuildAction)) { return; }
	if (AICOMM_CheckForNextResearchAction(pBot, &pBot->ResearchAction)) { return; }
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

	const AvHAIHiveDefinition* Result = nullptr;
	float MinDist = 0.0f;

	const vector<AvHAIHiveDefinition*> Hives = AITAC_GetAllHives();

	for (auto it = Hives.begin(); it != Hives.end(); it++)
	{
		const AvHAIHiveDefinition* Hive = (*it);

		if (Hive->Status != HIVE_STATUS_UNBUILT) { continue; }

		if (AICOMM_IsHiveFullySecured(CommanderBot, Hive)) { continue; }

		if (AITAC_GetNearestHiddenPlayerInLocation(CommanderTeam, Hive->Location, UTIL_MetresToGoldSrcUnits(10.0f)) == nullptr) { continue; }

		if (AITAC_AnyPlayerOnTeamWithLOS(CommanderTeam, Hive->Location, UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			DeployableSearchFilter StructureFilter;
			StructureFilter.DeployableTeam = CommanderTeam;
			StructureFilter.ReachabilityTeam = CommanderTeam;
			StructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;
			StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	
			StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
			StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

			AvHAIBuildableStructure* PG = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &StructureFilter);

			bool bCanSeePG = (!PG || AITAC_AnyPlayerOnTeamWithLOS(CommanderTeam, UTIL_GetCentreOfEntity(PG->edict), UTIL_MetresToGoldSrcUnits(10.0f)));

			if (!bCanSeePG)
			{

				StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY;
				
				AvHAIBuildableStructure* TF = AITAC_FindClosestDeployableToLocation(Hive->FloorLocation, &StructureFilter);

				bool bNeedsElectrifying = false;

				if (TF)
				{
					StructureFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;

					bNeedsElectrifying = (UTIL_StructureIsFullyBuilt(TF->edict) && !UTIL_IsStructureElectrified(TF->edict) && AITAC_DeployableExistsAtLocation(TF->Location, &StructureFilter));
				}

				bool bCanSeeTF = (!TF || AITAC_AnyPlayerOnTeamWithLOS(CommanderTeam, UTIL_GetCentreOfEntity(TF->edict), UTIL_MetresToGoldSrcUnits(10.0f)));

				if (!bNeedsElectrifying && !bCanSeePG && !bCanSeeTF) { continue; }
			}

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

bool AICOMM_IsHiveFullySecured(AvHAIPlayer* CommanderBot, const AvHAIHiveDefinition* Hive)
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
	SearchFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	SearchFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	vector<AvHAIBuildableStructure*> HiveStructures = AITAC_FindAllDeployables(Hive->FloorLocation, &SearchFilter);

	for (auto it = HiveStructures.begin(); it != HiveStructures.end(); it++)
	{
		AvHAIBuildableStructure* Structure = (*it);

		if (Structure->StructureType == STRUCTURE_MARINE_TURRETFACTORY)
		{
			bHasPhaseGate = true;
		}

		if (Structure->StructureType == STRUCTURE_MARINE_TURRETFACTORY || Structure->StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
		{
			bHasTurretFactory = true;
			bTurretFactoryElectrified = (Structure->StructureStatusFlags & STRUCTURE_STATUS_ELECTRIFIED);

			SearchFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;
			SearchFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(8.0f);

			NumTurrets = AITAC_GetNumDeployablesNearLocation(Structure->Location, &SearchFilter);


		}

	}

	const AvHAIResourceNode* ResNode = Hive->HiveResNodeRef;

	bool bSecuredResNode = (!ResNode || (ResNode->bIsOccupied && ResNode->OwningTeam == CommanderTeam));

	bool bShouldElectrifyResNode = (ResNode && bSecuredResNode && CommanderBot->Player->GetResources() > 100 && AITAC_ElectricalResearchIsAvailable(ResNode->ActiveTowerEntity));

	return ((!bPhaseGatesAvailable || bHasPhaseGate) && bHasTurretFactory && bTurretFactoryElectrified && NumTurrets >= 5 && bSecuredResNode && !bShouldElectrifyResNode);
}