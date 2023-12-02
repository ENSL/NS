//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_tactical.h
// 
// Contains all helper functions for making tactical decisions
//

#pragma once

#ifndef AVH_AI_TACTICAL_H
#define AVH_AI_TACTICAL_H

#include "AvHAIPlayer.h"
#include "AvHAIConstants.h"

// How frequently to update the global list of built structures (in seconds). 0 = every frame
static const float structure_inventory_refresh_rate = 0.2f;

// How frequently to update the global list of dropped marine items (in seconds). 0 = every frame
static const float item_inventory_refresh_rate = 0.1f;

bool						AITAC_DeployableExistsAtLocation(const Vector& Location, const DeployableSearchFilter* Filter);
std::vector<AvHAIBuildableStructure*> AITAC_FindAllDeployables(const Vector& Location, const DeployableSearchFilter* Filter);
AvHAIBuildableStructure*	AITAC_FindClosestDeployableToLocation(const Vector& Location, const DeployableSearchFilter* Filter);
AvHAIBuildableStructure*	AITAC_GetDeployableRefFromEdict(const edict_t* Structure);
AvHAIBuildableStructure*	AITAC_GetNearestDeployableDirectlyReachable(AvHAIPlayer* pBot, const Vector Location, const DeployableSearchFilter* Filter);
int							AITAC_GetNumDeployablesNearLocation(const Vector& Location, const DeployableSearchFilter* Filter);
void						AITAC_RefreshHiveData();
void						AITAC_RefreshResourceNodes();
void						AITAC_UpdateMapAIData();
void						AITAC_CheckNavMeshModified();
void						AITAC_RefreshBuildableStructures();
void						AITAC_UpdateBuildableStructure(CBaseEntity* Structure);
void						AITAC_RefreshReachabilityForStructure(AvHAIBuildableStructure* Structure);
void						AITAC_RefreshReachabilityForResNode(AvHAIResourceNode* ResNode);
void						AITAC_RefreshAllResNodeReachability();
void						AITAC_RefreshReachabilityForItem(AvHAIDroppedItem* Item);
void						AITAC_OnStructureCreated(AvHAIBuildableStructure* NewStructure);
void						AITAC_OnStructureCompleted(AvHAIBuildableStructure* NewStructure);
void						AITAC_OnStructureBeginRecycling(AvHAIBuildableStructure* RecyclingStructure);
void						AITAC_OnStructureDestroyed(AvHAIBuildableStructure* DestroyedStructure);
void						AITAC_LinkDeployedItemToAction(AvHAIPlayer* CommanderBot, const AvHAIDroppedItem* NewItem);
void						AITAC_LinkAlienStructureToTask(AvHAIPlayer* pBot, AvHAIBuildableStructure* NewStructure);

float						AITAC_GetPhaseDistanceBetweenPoints(const Vector StartPoint, const Vector EndPoint);

const AvHAIHiveDefinition*	AITAC_GetHiveAtIndex(int Index);
const AvHAIHiveDefinition*	AITAC_GetHiveNearestLocation(const Vector SearchLocation);
const AvHAIHiveDefinition*	AITAC_GetActiveHiveNearestLocation(const Vector SearchLocation);

Vector						AITAC_GetCommChairLocation(AvHTeamNumber Team);
edict_t*					AITAC_GetCommChair(AvHTeamNumber Team);

Vector						AITAC_GetTeamStartingLocation(AvHTeamNumber Team);

AvHAIResourceNode*			AITAC_GetRandomResourceNode(AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags);

AvHAIDroppedItem*			AITAC_FindClosestItemToLocation(const Vector& Location, const AvHAIDeployableItemType ItemType, AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags, float MinRadius, float MaxRadius, bool bConsiderPhaseDistance);

AvHAIDroppedItem*			AITAC_GetDroppedItemRefFromEdict(edict_t* ItemEdict);

Vector AITAC_GetFloorLocationForHive(const AvHAIHiveDefinition* Hive);

int AITAC_GetNumHives();

void AITAC_OnNavMeshModified();

AvHMessageID UTIL_StructureTypeToImpulseCommand(const AvHAIDeployableStructureType StructureType);

edict_t* AITAC_GetClosestPlayerOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
bool AITAC_AnyPlayerOnTeamHasLOSToLocation(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
bool AITAC_GetNumPlayersOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
bool AITAC_ShouldBotBeCautious(AvHAIPlayer* pBot);

// Clears out the marine and alien buildable structure maps, resource node and hive lists, and the marine item list
void AITAC_ClearMapAIData();
// Clear out all the hive information
void AITAC_ClearHiveInfo();

bool AITAC_AlienHiveNeedsReinforcing(const AvHAIHiveDefinition* Hive);

void AITAC_RefreshMarineItems();
void AITAC_UpdateMarineItem(CBaseEntity* Item, AvHAIDeployableItemType ItemType);

void AITAC_OnItemDropped(const AvHAIDroppedItem* NewItem);

AvHAIDeployableStructureType UTIL_IUSER3ToStructureType(const int inIUSER3);

bool UTIL_ShouldStructureCollide(AvHAIDeployableStructureType StructureType);
float UTIL_GetStructureRadiusForObstruction(AvHAIDeployableStructureType StructureType);
unsigned char UTIL_GetAreaForObstruction(AvHAIDeployableStructureType StructureType, const edict_t* BuildingEdict);

bool UTIL_IsStructureElectrified(edict_t* Structure);
bool UTIL_StructureIsFullyBuilt(edict_t* Structure);
bool UTIL_StructureIsRecycling(edict_t* Structure);

AvHAIHiveDefinition* AITAC_GetHiveFromEdict(const edict_t* Edict);

AvHAIResourceNode* AITAC_FindNearestResourceNodeToLocation(const Vector Location, const DeployableSearchFilter* Filter);
AvHAIResourceNode* AITAC_GetNearestResourceNodeToLocation(const Vector Location);

bool UTIL_IsBuildableStructureStillReachable(AvHAIPlayer* pBot, const edict_t* Structure);
bool UTIL_IsDroppedItemStillReachable(AvHAIPlayer* pBot, const edict_t* Item);
AvHAIWeapon UTIL_GetWeaponTypeFromEdict(const edict_t* ItemEdict);

int AITAC_GetNumPlayersOfTeamInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 IgnoreClass);
int AITAC_GetNumPlayersOnTeamOfClass(const AvHTeamNumber Team, const AvHUser3 SearchClass, const edict_t* IgnorePlayer);
edict_t* AITAC_GetNearestPlayerOfClassInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 SearchClass);

AvHAIHiveDefinition* AITAC_GetTeamHiveWithTech(const AvHTeamNumber Team, const AvHMessageID Tech);
bool AITAC_TeamHiveWithTechExists(const AvHTeamNumber Team, const AvHMessageID Tech);

AvHAIDeployableItemType UTIL_GetItemTypeFromEdict(const edict_t* ItemEdict);
bool UTIL_DroppedItemIsPrimaryWeapon(const AvHAIDeployableItemType ItemType);

bool UTIL_StructureIsResearching(edict_t* Structure);
bool UTIL_StructureIsResearching(edict_t* Structure, const AvHMessageID Research);
bool UTIL_StructureIsUpgrading(edict_t* Structure);

Vector UTIL_GetNextMinePosition(edict_t* StructureToMine);
int UTIL_GetCostOfStructureType(AvHAIDeployableStructureType StructureType);

edict_t* AITAC_GetNearestHumanAtLocation(const AvHTeamNumber Team, const Vector Location, const float MaxSearchRadius);

AvHAIDeployableStructureType UTIL_GetChamberTypeForHiveTech(AvHMessageID HiveTech);

bool UTIL_ResearchIsComplete(const AvHTeamNumber Team, const AvHTechID Research);

#endif