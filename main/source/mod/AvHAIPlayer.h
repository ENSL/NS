#ifndef AVH_AI_PLAYER_H
#define AVH_AI_PLAYER_H

#include "AvHPlayer.h"
#include "AvHAIConstants.h"

// These define the bot's view frustum sides
#define FRUSTUM_PLANE_TOP 0
#define FRUSTUM_PLANE_BOTTOM 1
#define FRUSTUM_PLANE_LEFT 2
#define FRUSTUM_PLANE_RIGHT 3
#define FRUSTUM_PLANE_NEAR 4
#define FRUSTUM_PLANE_FAR 5

static const float BOT_FOV = 100.0f;  // Bot's field of view;
static const float BOT_MAX_VIEW = 9999.0f; // Bot's maximum view distance;
static const float BOT_MIN_VIEW = 5.0f; // Bot's minimum view distance;
static const float BOT_ASPECT_RATIO = 1.77778f; // Bot's view aspect ratio, 1.333333 for 4:3, 1.777778 for 16:9, 1.6 for 16:10;

static const float f_fnheight = 2.0f * tan((BOT_FOV * 0.0174532925f) * 0.5f) * BOT_MIN_VIEW;
static const float f_fnwidth = f_fnheight * BOT_ASPECT_RATIO;

static const float f_ffheight = 2.0f * tan((BOT_FOV * 0.0174532925f) * 0.5f) * BOT_MAX_VIEW;
static const float f_ffwidth = f_ffheight * BOT_ASPECT_RATIO;


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

// Returns true if the bot needs to reload
bool BotReloadWeapons(AvHAIPlayer* pBot);

// Make the bot type something in either global or team chat
void BotSay(AvHAIPlayer* pBot, bool bTeamSay, float Delay, char* textToSay);
bot_msg* GetAvailableBotMsgSlot(AvHAIPlayer* pBot);

void BotDropWeapon(AvHAIPlayer* pBot);

void BotAttackNonPlayerTarget(AvHAIPlayer* pBot, edict_t* Target);
void BotMarineAttackNonPlayerTarget(AvHAIPlayer* pBot, edict_t* Target);
void BotAlienAttackNonPlayerTarget(AvHAIPlayer* pBot, edict_t* Target);

void BotShootTarget(AvHAIPlayer* pBot, AvHAIWeapon AttackWeapon, edict_t* Target);
void BotShootLocation(AvHAIPlayer* pBot, AvHAIWeapon AttackWeapon, const Vector TargetLocation);
void BombardierAttackTarget(AvHAIPlayer* pBot, edict_t* Target);

void BotEvolveLifeform(AvHAIPlayer* pBot, Vector DesiredEvolveLocation, AvHMessageID TargetLifeform);
void BotEvolveUpgrade(AvHAIPlayer* pBot, Vector DesiredEvolveLocation, AvHMessageID TargetUpgrade);

enemy_status* GetTrackedEnemyRefForTarget(AvHAIPlayer* pBot, edict_t* Target);

void BotUpdateDesiredViewRotation(AvHAIPlayer* pBot);
void BotUpdateViewRotation(AvHAIPlayer* pBot, float DeltaTime);
void BotUpdateView(AvHAIPlayer* pBot);
void BotClearEnemyTrackingInfo(enemy_status* TrackingInfo);
bool IsPlayerInBotFOV(AvHAIPlayer* Observer, edict_t* TargetPlayer);
void UpdateAIPlayerViewFrustum(AvHAIPlayer* pBot);

float BotRateEnemyThreat(AvHAIPlayer* pBot, enemy_status* TrackingInfo);

bool UTIL_IsCloakedPlayerInvisible(edict_t* Observer, AvHPlayer* Player);


Vector GetVisiblePointOnPlayerFromObserver(edict_t* Observer, edict_t* TargetPlayer);

void UpdateBotChat(AvHAIPlayer* pBot);

void ClearBotInputs(AvHAIPlayer* pBot);
void StartNewBotFrame(AvHAIPlayer* pBot);
void EndBotFrame(AvHAIPlayer* pBot);

void AIPlayerThink(AvHAIPlayer* pBot);
// Think routine for regular NS game mode
void AIPlayerNSThink(AvHAIPlayer* pBot);
void AIPlayerNSMarineThink(AvHAIPlayer* pBot);
void AIPlayerNSAlienThink(AvHAIPlayer* pBot);
// Think routine for the combat game mode
void AIPlayerCOThink(AvHAIPlayer* pBot);
void AIPlayerCOMarineThink(AvHAIPlayer* pBot);
void AIPlayerCOAlienThink(AvHAIPlayer* pBot);



// Think routine for the deathmatch game mode (e.g. when playing CS maps)
void AIPlayerDMThink(AvHAIPlayer* pBot);

// What to do when the game hasn't OFFICIALLY ended, but basically is (i.e. one side has no hive/CC/infantry portal)
void AIPlayerEndMatchThink(AvHAIPlayer* pBot);

void TestNavThink(AvHAIPlayer* pBot);
void DroneThink(AvHAIPlayer* pBot);
void CustomThink(AvHAIPlayer* pBot);

AvHMessageID GetNextAIPlayerCOUpgrade(AvHAIPlayer* pBot);
AvHMessageID GetNextAIPlayerCOMarineUpgrade(AvHAIPlayer* pBot);
AvHMessageID GetNextAIPlayerCOAlienUpgrade(AvHAIPlayer* pBot);

bool AIPlayerMustFinishCurrentTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
AvHAIPlayerTask* AIPlayerGetNextTask(AvHAIPlayer* pBot);
void AIPlayerSetPrimaryMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetMarineSweeperPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetMarineCapperPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetMarineAssaultPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetMarineBombardierPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void AIPlayerSetSecondaryMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetWantsAndNeedsMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void AIPlayerSetPrimaryAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetAlienBuilderPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetAlienCapperPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetAlienAssaultPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetAlienHarasserPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void AIPlayerSetSecondaryAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetWantsAndNeedsAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void AIPlayerSetPrimaryCOMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetSecondaryCOMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetPrimaryCOAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);
void AIPlayerSetSecondaryCOAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task);

void BotSwitchToWeapon(AvHAIPlayer* pBot, AvHAIWeapon NewWeaponSlot);

bool ShouldBotThink(AvHAIPlayer* pBot);

void BotResumePlay(AvHAIPlayer* pBot);

void UpdateCommanderOrders(AvHAIPlayer* pBot);
void AIPlayerReceiveMoveOrder(AvHAIPlayer* pBot, Vector Destination);
void AIPlayerReceiveBuildOrder(AvHAIPlayer* pBot, edict_t* BuildTarget);

void AIPlayerRequestHealth(AvHAIPlayer* pBot);
void AIPlayerRequestAmmo(AvHAIPlayer* pBot);
void AIPlayerRequestOrder(AvHAIPlayer* pBot);

void BotStopCommanderMode(AvHAIPlayer* pBot);

void SetNewAIPlayerRole(AvHAIPlayer* pBot, AvHAIBotRole NewRole);
void UpdateAIMarinePlayerNSRole(AvHAIPlayer* pBot);
void UpdateAIAlienPlayerNSRole(AvHAIPlayer* pBot);
void UpdateAIPlayerCORole(AvHAIPlayer* pBot);
void UpdateAIPlayerDMRole(AvHAIPlayer* pBot);

bool ShouldAIPlayerTakeCommand(AvHAIPlayer* pBot);

void AIPlayerTakeDamage(AvHAIPlayer* pBot, int damageTaken, edict_t* aggressor);
void AIPlayerHearEnemy(AvHAIPlayer* pBot, edict_t* HeardEnemy, float SoundVolume);

int BotGetNextEnemyTarget(AvHAIPlayer* pBot);

AvHMessageID AlienGetDesiredUpgrade(AvHAIPlayer* pBot, HiveTechStatus DesiredTech);

AvHAICombatStrategy GetBotCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy);
AvHAICombatStrategy GetAlienCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy);
AvHAICombatStrategy GetSkulkCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy);
AvHAICombatStrategy GetGorgeCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy);
AvHAICombatStrategy GetLerkCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy);
AvHAICombatStrategy GetFadeCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy);
AvHAICombatStrategy GetOnosCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy);
AvHAICombatStrategy GetMarineCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy);

bool MarineCombatThink(AvHAIPlayer* pBot);
void MarineHuntEnemy(AvHAIPlayer* pBot, enemy_status* TrackedEnemy);
void BotThrowGrenadeAtTarget(AvHAIPlayer* pBot, const Vector TargetPoint);

bool AlienCombatThink(AvHAIPlayer* pBot);
bool SkulkCombatThink(AvHAIPlayer* pBot);
bool GorgeCombatThink(AvHAIPlayer* pBot);
bool LerkCombatThink(AvHAIPlayer* pBot);
bool FadeCombatThink(AvHAIPlayer* pBot);
bool OnosCombatThink(AvHAIPlayer* pBot);

bool BombardierCombatThink(AvHAIPlayer* pBot);
bool RegularMarineCombatThink(AvHAIPlayer* pBot);

void DEBUG_PrintBotDebugInfo(edict_t* OutputPlayer, AvHAIPlayer* pBot);
void DEBUG_PrintTaskInfo(edict_t* OutputPlayer, AvHAIPlayer* pBot);
void DEBUG_PrintCombatInfo(edict_t* OutputPlayer, AvHAIPlayer* pBot);

#endif