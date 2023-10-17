#ifndef AVH_AI_PLAYER_H
#define AVH_AI_PLAYER_H

#include "../AvHPlayer.h"
#include "AvHAIConstants.h"




void BotJump(AvHAIPlayer* pBot);
void BotSuicide(AvHAIPlayer* pBot);
void BotLookAt(AvHAIPlayer* pBot, Vector NewLocation);
void BotLookAt(AvHAIPlayer* pBot, edict_t* target);
void BotMoveLookAt(AvHAIPlayer* pBot, const Vector target);
void BotDirectLookAt(AvHAIPlayer* pBot, Vector target);

bool BotUseObject(AvHAIPlayer* pBot, edict_t* Target, bool bContinuous);

bool CanBotLeap(AvHAIPlayer* pBot);
void BotLeap(AvHAIPlayer* pBot, const Vector TargetLocation);
float GetLeapCost(AvHAIPlayer* pBot);

void BotReloadWeapons(AvHAIPlayer* pBot);

void LinkDeployedObjectToCommanderAction(AvHAIPlayer* Commander, AvHAIBuildableStructure* NewStructure);

// Make the bot type something in either global or team chat
void BotSay(AvHAIPlayer* pBot, bool bTeamSay, float Delay, char* textToSay);
bot_msg* GetAvailableBotMsgSlot(AvHAIPlayer* pBot);

void BotDropWeapon(AvHAIPlayer* pBot);

void BotAttackTarget(AvHAIPlayer* pBot, edict_t* Target);

void BotShootTarget(AvHAIPlayer* pBot, AvHAIWeapon AttackWeapon, edict_t* Target);
void BotShootLocation(AvHAIPlayer* pBot, AvHAIWeapon AttackWeapon, const Vector TargetLocation);
void BombardierAttackTarget(AvHAIPlayer* pBot, edict_t* Target);

void BotEvolveLifeform(AvHAIPlayer* pBot, AvHMessageID Lifeform);

enemy_status* GetTrackedEnemyRefForTarget(AvHAIPlayer* pBot, edict_t* Target);

void BotUpdateDesiredViewRotation(AvHAIPlayer* pBot);
void BotUpdateViewRotation(AvHAIPlayer* pBot, float DeltaTime);
void BotUpdateView(AvHAIPlayer* pBot);
void BotClearEnemyTrackingInfo(enemy_status* TrackingInfo);
bool IsPlayerInBotFOV(AvHAIPlayer* Observer, edict_t* TargetPlayer);

Vector GetVisiblePointOnPlayerFromObserver(edict_t* Observer, edict_t* TargetPlayer);

void UpdateBotChat(AvHAIPlayer* pBot);

void ClearBotInputs(AvHAIPlayer* pBot);
void StartNewBotFrame(AvHAIPlayer* pBot);

void TestNavThink(AvHAIPlayer* pBot);
void DroneThink(AvHAIPlayer* pBot);

void BotSwitchToWeapon(AvHAIPlayer* pBot, AvHAIWeapon NewWeaponSlot);

bool ShouldBotThink(AvHAIPlayer* pBot);

void BotResumePlay(AvHAIPlayer* pBot);

void UpdateCommanderOrders(AvHAIPlayer* pBot);

#endif