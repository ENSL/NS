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
static const float item_inventory_refresh_rate = 0.2f;

bool						AITAC_DeployableExistsAtLocation(const Vector& Location, const DeployableSearchFilter* Filter);
std::vector<AvHAIBuildableStructure> AITAC_FindAllDeployables(const Vector& Location, const DeployableSearchFilter* Filter);
std::vector<AvHAIBuildableStructure*> AITAC_FindAllDeployablesByRef(const Vector& Location, const DeployableSearchFilter* Filter);
AvHAIBuildableStructure		AITAC_FindClosestDeployableToLocation(const Vector& Location, const DeployableSearchFilter* Filter);
AvHAIBuildableStructure*	AITAC_FindClosestDeployableToLocationByRef(const Vector& Location, const DeployableSearchFilter* Filter);
AvHAIBuildableStructure		AITAC_FindFurthestDeployableFromLocation(const Vector& Location, const DeployableSearchFilter* Filter);
AvHAIBuildableStructure*	AITAC_FindFurthestDeployableFromLocationByRef(const Vector& Location, const DeployableSearchFilter* Filter);
AvHAIBuildableStructure		AITAC_GetDeployableFromEdict(const edict_t* Structure);
AvHAIBuildableStructure*	AITAC_GetDeployableRefFromEdict(const edict_t* Structure);
AvHAIBuildableStructure		AITAC_GetNearestDeployableDirectlyReachable(AvHAIPlayer* pBot, const Vector Location, const DeployableSearchFilter* Filter);
AvHAIBuildableStructure*	AITAC_GetNearestDeployableDirectlyReachableByRef(AvHAIPlayer* pBot, const Vector Location, const DeployableSearchFilter* Filter);
int							AITAC_GetNumDeployablesNearLocation(const Vector& Location, const DeployableSearchFilter* Filter);
void						AITAC_PopulateHiveData();
void						AITAC_RefreshHiveData();
void						AITAC_PopulateResourceNodes();
void						AITAC_RefreshResourceNodes();
void						AITAC_UpdateMapAIData();
void						AITAC_CheckNavMeshModified();
void						AITAC_RefreshBuildableStructures();
AvHAIBuildableStructure*	AITAC_UpdateBuildableStructure(CBaseEntity* Structure);
void						AITAC_RefreshReachabilityForStructure(AvHAIBuildableStructure* Structure);
void						AITAC_RefreshReachabilityForResNode(AvHAIResourceNode* ResNode);
void						AITAC_RefreshReachabilityForHive(AvHAIHiveDefinition* Hive);
void						AITAC_RefreshAllResNodeReachability();
void						AITAC_RefreshReachabilityForItem(AvHAIDroppedItem* Item);
void						AITAC_OnStructureCreated(AvHAIBuildableStructure* NewStructure);
void						AITAC_OnStructureCompleted(AvHAIBuildableStructure* NewStructure);
void						AITAC_OnStructureBeginRecycling(AvHAIBuildableStructure* RecyclingStructure);
void						AITAC_OnStructureDestroyed(AvHAIBuildableStructure* DestroyedStructure);
void						AITAC_LinkDeployedItemToAction(AvHAIPlayer* CommanderBot, const AvHAIDroppedItem* NewItem);
void						AITAC_LinkStructureToPlayer(AvHAIBuildableStructure* NewStructure);

float						AITAC_GetPhaseDistanceBetweenPoints(const Vector StartPoint, const Vector EndPoint);

const AvHAIHiveDefinition*	AITAC_GetHiveAtIndex(int Index);
const AvHAIHiveDefinition*	AITAC_GetHiveNearestLocation(const Vector SearchLocation);
const AvHAIHiveDefinition*	AITAC_GetActiveHiveNearestLocation(AvHTeamNumber Team, const Vector SearchLocation);
const AvHAIHiveDefinition*	AITAC_GetNonEmptyHiveNearestLocation(const Vector SearchLocation);

Vector						AITAC_GetCommChairLocation(AvHTeamNumber Team);
edict_t*					AITAC_GetCommChair(AvHTeamNumber Team);

Vector						AITAC_GetTeamStartingLocation(AvHTeamNumber Team);

AvHAIResourceNode*			AITAC_GetRandomResourceNode(AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags);

AvHAIDroppedItem*			AITAC_FindClosestItemToLocation(const Vector& Location, const AvHAIDeployableItemType ItemType, AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags, float MinRadius, float MaxRadius, bool bConsiderPhaseDistance);
bool						AITAC_ItemExistsInLocation(const Vector& Location, const AvHAIDeployableItemType ItemType, AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags, float MinRadius, float MaxRadius, bool bConsiderPhaseDistance);
int							AITAC_GetNumItemsInLocation(const Vector& Location, const AvHAIDeployableItemType ItemType, AvHTeamNumber SearchingTeam, const unsigned int ReachabilityFlags, float MinRadius, float MaxRadius, bool bConsiderPhaseDistance);

AvHAIDroppedItem*			AITAC_GetDroppedItemRefFromEdict(edict_t* ItemEdict);

Vector						AITAC_GetRandomBuildHintInLocation(const unsigned int StructureType, const Vector SearchLocation, const float SearchRadius);

Vector AITAC_GetFloorLocationForHive(const AvHAIHiveDefinition* Hive);

int AITAC_GetNumHives();
int AITAC_GetNumTeamHives(AvHTeamNumber Team, bool bFullyCompletedOnly);

void AITAC_OnNavMeshModified();

AvHMessageID UTIL_StructureTypeToImpulseCommand(const AvHAIDeployableStructureType StructureType);
AvHMessageID UTIL_ItemTypeToImpulseCommand(const AvHAIDeployableItemType ItemType);

edict_t* AITAC_GetClosestPlayerOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
bool AITAC_AnyPlayerOnTeamHasLOSToLocation(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
int AITAC_GetNumPlayersOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
vector<AvHPlayer*> AITAC_GetAllPlayersOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
bool AITAC_ShouldBotBeCautious(AvHAIPlayer* pBot);

// Clears out the marine and alien buildable structure maps, resource node and hive lists, and the marine item list
void AITAC_ClearMapAIData(bool bInitialMapLoad = false);
// Clear out all the hive information
void AITAC_ClearHiveInfo();

void AITAC_RefreshTeamStartingLocations();

void AITAC_ClearStructureNavData();

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
bool AITAC_StructureCanBeUpgraded(edict_t* Structure);

AvHAIHiveDefinition* AITAC_GetHiveFromEdict(const edict_t* Edict);
AvHAIResourceNode* AITAC_GetResourceNodeFromEdict(const edict_t* Edict);

// What percentage of all viable (can be reached by the requested team) resource nodes does the team currently own? Expressed as 0.0 - 1.0
float AITAC_GetTeamResNodeOwnership(const AvHTeamNumber Team, bool bIncludeBaseNodes);
int	AITAC_GetNumResourceNodesNearLocation(const Vector Location, const DeployableSearchFilter* Filter);
AvHAIResourceNode* AITAC_FindNearestResourceNodeToLocation(const Vector Location, const DeployableSearchFilter* Filter);
AvHAIResourceNode* AITAC_GetNearestResourceNodeToLocation(const Vector Location);
vector<AvHAIResourceNode*> AITAC_GetAllMatchingResourceNodes(const Vector Location, const DeployableSearchFilter* Filter);

bool UTIL_IsBuildableStructureStillReachable(AvHAIPlayer* pBot, const edict_t* Structure);
bool UTIL_IsDroppedItemStillReachable(AvHAIPlayer* pBot, const edict_t* Item);
AvHAIWeapon UTIL_GetWeaponTypeFromEdict(const edict_t* ItemEdict);

int AITAC_GetNumActivePlayersOnTeam(const AvHTeamNumber Team);
int AITAC_GetNumPlayersOfTeamInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 IgnoreClass);
bool AITAC_AnyPlayersOfTeamInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 IgnoreClass);
vector<AvHPlayer*> AITAC_GetAllPlayersOfTeamInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 IgnoreClass);
int AITAC_GetNumPlayersOfTeamAndClassInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 SearchClass);
int AITAC_GetNumPlayersOnTeamOfClass(const AvHTeamNumber Team, const AvHUser3 SearchClass, const edict_t* IgnorePlayer);
vector<AvHPlayer*> AITAC_GetAllPlayersOnTeamOfClass(const AvHTeamNumber Team, const AvHUser3 SearchClass, const edict_t* IgnorePlayer);
edict_t* AITAC_GetNearestPlayerOfClassInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 SearchClass);
vector<edict_t*> AITAC_GetAllPlayersOfClassInArea(const AvHTeamNumber Team, const Vector SearchLocation, const float SearchRadius, const bool bConsiderPhaseDist, const edict_t* IgnorePlayer, const AvHUser3 SearchClass);

AvHAIHiveDefinition* AITAC_GetTeamHiveWithTech(const AvHTeamNumber Team, const AvHMessageID Tech);
bool AITAC_TeamHiveWithTechExists(const AvHTeamNumber Team, const AvHMessageID Tech);

AvHAIDeployableItemType UTIL_GetItemTypeFromEdict(const edict_t* ItemEdict);
bool UTIL_DroppedItemIsPrimaryWeapon(const AvHAIDeployableItemType ItemType);

AvHAIWeapon UTIL_GetWeaponTypeFromDroppedItem(const AvHAIDeployableItemType ItemType);

bool UTIL_StructureIsResearching(edict_t* Structure);
bool UTIL_StructureIsResearching(edict_t* Structure, const AvHMessageID Research);
bool UTIL_StructureIsUpgrading(edict_t* Structure);

bool AITAC_MarineResearchIsAvailable(const AvHTeamNumber Team, const AvHMessageID Research);
bool AITAC_ElectricalResearchIsAvailable(edict_t* Structure);

Vector UTIL_GetNextMinePosition(edict_t* StructureToMine);
Vector UTIL_GetNextMinePosition2(edict_t* StructureToMine);
int UTIL_GetCostOfStructureType(AvHAIDeployableStructureType StructureType);

edict_t* AITAC_GetNearestHumanAtLocation(const AvHTeamNumber Team, const Vector Location, const float MaxSearchRadius);

AvHAIDeployableStructureType UTIL_GetChamberTypeForHiveTech(AvHMessageID HiveTech);

bool AITAC_ResearchIsComplete(const AvHTeamNumber Team, const AvHTechID Research);

bool AITAC_PhaseGatesAvailable(const AvHTeamNumber Team);

int AITAC_GetNumDeadPlayersOnTeam(const AvHTeamNumber Team);

const AvHAIHiveDefinition* AITAC_GetNearestHiveUnderActiveSiege(AvHTeamNumber SiegingTeam, const Vector SearchLocation);
edict_t* AITAC_GetMarineEligibleToBuildSiege(AvHTeamNumber Team, const AvHAIHiveDefinition* Hive);

edict_t* AITAC_GetNearestHiddenPlayerInLocation(AvHTeamNumber Team, const Vector Location, const float MaxRadius);

const vector<AvHAIResourceNode*> AITAC_GetAllResourceNodes();
const vector<AvHAIResourceNode*> AITAC_GetAllReachableResourceNodes(AvHTeamNumber Team);
const vector<AvHAIHiveDefinition*> AITAC_GetAllHives();
const vector<AvHAIHiveDefinition*> AITAC_GetAllTeamHives(AvHTeamNumber Team, bool bFullyBuiltOnly);
const AvHAIHiveDefinition* AITAC_GetNearestTeamHive(AvHTeamNumber Team, const Vector SearchLocation, bool bFullyBuiltOnly);

bool AITAC_AnyPlayerOnTeamWithLOS(AvHTeamNumber Team, const Vector& Location, float SearchRadius);

bool AITAC_IsAlienBuilderNeeded(AvHAIPlayer* pBot);
bool AITAC_IsAlienCapperNeeded(AvHAIPlayer* pBot);
bool AITAC_IsAlienHarasserNeeded(AvHAIPlayer* pBot);

bool AITAC_ShouldBotBuildHive(AvHAIPlayer* pBot, AvHAIHiveDefinition** EligibleHive);

AvHAIDeployableStructureType AITAC_GetNextMissingUpgradeChamberForTeam(AvHTeamNumber Team, int& NumMissing);

void AITAC_OnTeamStartsModified();

edict_t* AITAC_AlienFindNearestHealingSource(AvHTeamNumber Team, Vector SearchLocation, edict_t* SearchingPlayer, bool bIncludeGorges);

bool AITAC_IsAlienUpgradeAvailableForTeam(AvHTeamNumber Team, HiveTechStatus DesiredTech);

int AITAC_GetNumWeaponsInPlay(AvHTeamNumber Team, AvHAIWeapon WeaponType);

edict_t* AITAC_GetLastSeenLerkForTeam(AvHTeamNumber Team, float& LastSeenTime);

bool AITAC_IsCompletedStructureOfTypeNearLocation(AvHTeamNumber Team, unsigned int StructureType, Vector SearchLocation, float SearchRadius);
bool AITAC_IsStructureOfTypeNearLocation(AvHTeamNumber Team, unsigned int StructureType, Vector SearchLocation, float SearchRadius);

void AITAC_UpdateSquads();
void AITAC_ManageSquads();
void AITAC_ClearSquads();
AvHAISquad* AITAC_GetSquadForObjective(AvHAIPlayer* pBot, edict_t* TaskTarget, BotTaskType ObjectiveType);
Vector AITAC_GetGatherLocationForSquad(AvHAISquad* Squad);

#endif