//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_navigation.h
// 
// Handles all bot path finding and movement
//

#pragma once
#ifndef AVH_AI_COMMANDER_H
#define AVH_AI_COMMANDER_H

#include "AvHAIConstants.h"

static const float MIN_COMMANDER_REMIND_TIME = 20.0f; // How frequently the commander can nag a player to do something, if they don't think they're doing it

bool AICOMM_DeployStructure(AvHAIPlayer* pBot, const AvHAIDeployableStructureType StructureToDeploy, const Vector Location, StructurePurpose Purpose = STRUCTURE_PURPOSE_GENERAL);
bool AICOMM_DeployItem(AvHAIPlayer* pBot, const AvHAIDeployableItemType ItemToDeploy, const Vector Location);
bool AICOMM_UpgradeStructure(AvHAIPlayer* pBot, AvHAIBuildableStructure* StructureToUpgrade);
bool AICOMM_ResearchTech(AvHAIPlayer* pBot, AvHAIBuildableStructure* StructureToResearch, AvHMessageID Research);
bool AICOMM_RecycleStructure(AvHAIPlayer* pBot, AvHAIBuildableStructure* StructureToRecycle);

bool AICOMM_IssueMovementOrder(AvHAIPlayer* pBot, edict_t* Recipient, const Vector MoveLocation);
bool AICOMM_IssueBuildOrder(AvHAIPlayer* pBot, edict_t* Recipient, edict_t* TargetStructuree);
bool AICOMM_IssueSecureHiveOrder(AvHAIPlayer* pBot, edict_t* Recipient, const AvHAIHiveDefinition* HiveToSecure);
bool AICOMM_IssueSiegeHiveOrder(AvHAIPlayer* pBot, edict_t* Recipient, const AvHAIHiveDefinition* HiveToSiege, const Vector SiegePosition);
bool AICOMM_IssueSecureResNodeOrder(AvHAIPlayer* pBot, edict_t* Recipient, const AvHAIResourceNode* ResNode);

void AICOMM_AssignNewPlayerOrder(AvHAIPlayer* pBot, edict_t* Assignee, edict_t* TargetEntity, AvHAIOrderPurpose OrderPurpose);
int AICOMM_GetNumPlayersAssignedToOrder(AvHAIPlayer* pBot, edict_t* TargetEntity, AvHAIOrderPurpose OrderPurpose);
bool AICOMM_IsOrderStillValid(AvHAIPlayer* pBot, ai_commander_order* Order);
void AICOMM_UpdatePlayerOrders(AvHAIPlayer* pBot);
edict_t* AICOMM_GetPlayerWithNoOrderNearestLocation(AvHAIPlayer* pBot, Vector SearchLocation);
bool AICOMM_DoesPlayerOrderNeedReminder(AvHAIPlayer* pBot, ai_commander_order* Order);
void AICOMM_IssueOrderForAssignedJob(AvHAIPlayer* pBot, ai_commander_order* Order);

void AICOMM_ClearAction(commander_action* Action);

bool AICOMM_CheckForNextBuildAction(AvHAIPlayer* pBot);
bool AICOMM_CheckForNextSupportAction(AvHAIPlayer* pBot);
bool AICOMM_CheckForNextRecycleAction(AvHAIPlayer* pBot);
bool AICOMM_CheckForNextResearchAction(AvHAIPlayer* pBot);
bool AICOMM_CheckForNextSupplyAction(AvHAIPlayer* pBot);

void AICOMM_SetDropHealthAction(AvHAIPlayer* pBot, commander_action* Action, edict_t* Recipient);
void AICOMM_SetDropAmmoAction(AvHAIPlayer* pBot, commander_action* Action, edict_t* Recipient);
void AICOMM_SetDeployStructureAction(AvHAIPlayer* pBot, commander_action* Action, AvHAIDeployableStructureType StructureToBuild, const Vector Location, bool bIsUrgent);
void AICOMM_SetDeployItemAction(AvHAIPlayer* pBot, commander_action* Action, AvHAIDeployableItemType ItemToBuild, const Vector Location, bool bIsUrgent);

void AICOMM_CommanderThink(AvHAIPlayer* pBot);

const AvHAIHiveDefinition* AICOMM_GetEmptyHiveOpportunityNearestLocation(AvHAIPlayer* CommanderBot, const Vector SearchLocation);

bool AICOMM_BuildInfantryPortal(AvHAIPlayer* pBot, edict_t* CommChair);
bool AICOMM_PerformNextSiegeHiveAction(AvHAIPlayer* pBot, const AvHAIHiveDefinition* HiveToSiege);
bool AICOMM_PerformNextSecureHiveAction(AvHAIPlayer* pBot, const AvHAIHiveDefinition* HiveToSecure);

ai_commander_request* AICOMM_GetExistingRequestForPlayer(AvHAIPlayer* pBot, edict_t* Requestor);
void AICOMM_CheckNewRequests(AvHAIPlayer* pBot);
bool AICOMM_IsRequestValid(ai_commander_request* Request);

bool AICOMM_IsHiveFullySecured(AvHAIPlayer* CommanderBot, const AvHAIHiveDefinition* Hive, bool bIncludeElectrical);

bool AICOMM_ShouldCommanderLeaveChair(AvHAIPlayer* pBot);

const AvHAIResourceNode* AICOMM_GetNearestResourceNodeCapOpportunity(const AvHTeamNumber Team, const Vector SearchLocation);
const AvHAIHiveDefinition* AICOMM_GetHiveSiegeOpportunityNearestLocation(AvHAIPlayer* CommanderBot, const Vector SearchLocation);

bool AICOMM_ShouldCommanderPrioritiseNodes(AvHAIPlayer* pBot);
bool AICOMM_ShouldBeacon(AvHAIPlayer* pBot);

void AICOMM_ReceiveChatRequest(AvHAIPlayer* Commander, edict_t* Requestor, const char* Request);

#endif // AVH_AI_COMMANDER_H