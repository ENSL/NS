#pragma once

#ifndef AVH_AI_TASK_H
#define AVH_AI_TASK_H

#include "AvHAIPlayer.h"


void AITASK_ClearAllBotTasks(AvHAIPlayer* pBot);
void AITASK_ClearBotTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);


bool AITASK_IsTaskCompleted(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

bool AITASK_IsTaskUrgent(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsMoveTaskUrgent(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsBuildTaskUrgent(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsGuardTaskUrgent(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void AITASK_OnCompleteCommanderTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AITASK_BotUpdateAndClearTasks(AvHAIPlayer* pBot);

bool AITASK_IsMoveTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsTouchTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsAmmoPickupTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsHealthPickupTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsEquipmentPickupTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsWeaponPickupTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsWeldTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsAttackTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsResupplyTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsGuardTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsMineStructureTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsUseTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

bool AITASK_IsAlienBuildTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsMarineBuildTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

bool AITASK_IsAlienCapResNodeTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsMarineCapResNodeTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

bool AITASK_IsDefendTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsEvolveTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

bool AITASK_IsReinforceStructureTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsMarineSecureHiveTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsAlienSecureHiveTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

bool AITASK_IsAlienGetHealthTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
bool AITASK_IsAlienHealTaskStillValid(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

char* AITASK_TaskTypeToChar(const BotTaskType TaskType);

void AITASK_SetAttackTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent);
void AITASK_SetMoveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const Vector Location, const bool bIsUrgent);
void AITASK_SetBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const AvHAIDeployableStructureType StructureType, const Vector Location, const bool bIsUrgent);
void AITASK_SetBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* StructureToBuild, const bool bIsUrgent);
void AITASK_SetCapResNodeTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const AvHAIResourceNode* NodeRef, const bool bIsUrgent);
void AITASK_SetDefendTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent);
void AITASK_SetEvolveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const Vector EvolveLocation, const AvHMessageID EvolveImpulse, const bool bIsUrgent);
void AITASK_SetEvolveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* EvolveHive, const AvHMessageID EvolveImpulse, const bool bIsUrgent);
void AITASK_SetUseTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent);
void AITASK_SetUseTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const Vector UseLocation, const bool bIsUrgent);
void AITASK_SetTouchTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, bool bIsUrgent);
void AITASK_SetReinforceStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, bool bIsUrgent);
void AITASK_SetReinforceStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const AvHAIDeployableStructureType FirstStructureType, bool bIsUrgent);
void AITASK_SetSecureHiveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const Vector WaitLocation, bool bIsUrgent);
void AITASK_SetMineStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, bool bIsUrgent);
void AITASK_SetWeldTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent);
void AITASK_SetPickupTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* Target, const bool bIsUrgent);
void AITASK_SetGetHealthTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, edict_t* HealingSource, const bool bIsUrgent);

void BotProgressTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void BotProgressMoveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void BotProgressUseTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void BotProgressTouchTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void BotProgressPickupTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void BotProgressMineStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void BotProgressGuardTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void BotProgressResupplyTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void BotProgressAttackTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void BotProgressDefendTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void BotProgressTakeCommandTask(AvHAIPlayer* pBot);
void BotProgressEvolveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void MarineProgressBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void MarineProgressCapResNodeTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void BotProgressWeldTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void AIPlayerBuildStructure(AvHAIPlayer* pBot, edict_t* BuildTarget);

void MarineProgressSecureHiveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

// Clear all enemy structures
void AlienProgressSecureHiveTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void AlienProgressGetHealthTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AlienProgressHealTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AlienProgressBuildTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AlienProgressCapResNodeTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void BotProgressReinforceStructureTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void BotGuardLocation(AvHAIPlayer* pBot, const Vector GuardLocation);

void AITASK_GenerateGuardWatchPoints(AvHAIPlayer* pBot, const Vector& GuardLocation);

bool BotWithBuildTaskExists(AvHTeamNumber Team, AvHAIDeployableStructureType StructureType);
int AITASK_GetNumBotsWithBuildTask(AvHTeamNumber Team, AvHAIDeployableStructureType StructureType, edict_t* IgnorePlayer);
AvHAIPlayer* GetFirstBotWithBuildTask(AvHTeamNumber Team, AvHAIDeployableStructureType StructureType, edict_t* IgnorePlayer);
AvHAIPlayer* GetFirstBotWithReinforceTask(AvHTeamNumber Team, edict_t* ReinforceStructure, edict_t* IgnorePlayer);

void UTIL_ClearGuardInfo(AvHAIPlayer* pBot);

void BotAlienPlaceChamber(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, AvHAIDeployableStructureType DesiredStructure);
void BotAlienBuildHive(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const AvHAIHiveDefinition* HiveToBuild);
void BotAlienBuildResTower(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, const AvHAIResourceNode* NodeToCap);
void BotAlienHealTarget(AvHAIPlayer* pBot, edict_t* HealTarget);

void RegisterBotAlienBuildAttempt(AvHAIPlayer* pBot, AvHAIPlayerTask* Task, Vector PlacementLocation, AvHAIDeployableStructureType DesiredStructure);


#endif