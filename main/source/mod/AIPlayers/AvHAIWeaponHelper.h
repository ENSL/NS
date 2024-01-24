#pragma once

#ifndef AVH_AI_WEAPON_HELPER_H
#define AVH_AI_WEAPON_HELPER_H

#include "AvHAIPlayer.h"

int GetPlayerCurrentWeaponClipAmmo(const AvHPlayer* Player);
int GetPlayerCurrentWeaponMaxClipAmmo(const AvHPlayer* Player);
int GetPlayerCurrentWeaponReserveAmmo(const AvHPlayer* Player);
AvHAIWeapon GetPlayerCurrentWeapon(const AvHPlayer* Player);
AvHBasePlayerWeapon* GetPlayerCurrentWeaponReference(const AvHPlayer* Player);


AvHAIWeapon UTIL_GetPlayerPrimaryWeapon(const AvHPlayer* Player);
AvHAIWeapon UTIL_GetPlayerSecondaryWeapon(const AvHPlayer* Player);

int UTIL_GetPlayerPrimaryWeaponClipAmmo(const AvHPlayer* Player);
int UTIL_GetPlayerPrimaryWeaponMaxClipSize(const AvHPlayer* Player);
int UTIL_GetPlayerPrimaryAmmoReserve(AvHPlayer* Player);
int UTIL_GetPlayerPrimaryMaxAmmoReserve(AvHPlayer* Player);

int UTIL_GetPlayerSecondaryWeaponClipAmmo(const AvHPlayer* Player);
int UTIL_GetPlayerSecondaryWeaponMaxClipSize(const AvHPlayer* Player);
int UTIL_GetPlayerSecondaryAmmoReserve(AvHPlayer* Player);
int UTIL_GetPlayerSecondaryMaxAmmoReserve(AvHPlayer* Player);

AvHAIWeapon GetBotMarineSecondaryWeapon(const AvHAIPlayer* pBot);
int BotGetSecondaryWeaponClipAmmo(const AvHAIPlayer* pBot);
int BotGetSecondaryWeaponMaxClipSize(const AvHAIPlayer* pBot);
int BotGetSecondaryWeaponAmmoReserve(AvHAIPlayer* pBot);
int BotGetSecondaryWeaponMaxAmmoReserve(AvHAIPlayer* pBot);

float GetEnergyCostForWeapon(const AvHAIWeapon Weapon);
float GetProjectileVelocityForWeapon(const AvHAIWeapon Weapon);

float GetMaxIdealWeaponRange(const AvHAIWeapon Weapon);
float GetMinIdealWeaponRange(const AvHAIWeapon Weapon);

bool WeaponCanBeReloaded(const AvHAIWeapon CheckWeapon);
bool IsMeleeWeapon(const AvHAIWeapon Weapon);

Vector UTIL_GetGrenadeThrowTarget(edict_t* Player, const Vector TargetLocation, const float ExplosionRadius, bool bPrecise);

AvHAIWeapon BotMarineChooseBestWeaponForStructure(AvHAIPlayer* pBot, edict_t* target);
AvHAIWeapon BotAlienChooseBestWeaponForStructure(AvHAIPlayer* pBot, edict_t* target);

// Helper function to pick the best weapon for any given situation and target type.
AvHAIWeapon BotMarineChooseBestWeapon(AvHAIPlayer* pBot, edict_t* target);
AvHAIWeapon BotAlienChooseBestWeapon(AvHAIPlayer* pBot, edict_t* target);

AvHAIWeapon FadeGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target);
AvHAIWeapon OnosGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target);
AvHAIWeapon SkulkGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target);
AvHAIWeapon GorgeGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target);
AvHAIWeapon LerkGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target);

void BotReloadCurrentWeapon(AvHAIPlayer* pBot);

float GetReloadTimeForWeapon(AvHAIWeapon Weapon);

bool CanInterruptWeaponReload(AvHAIWeapon Weapon);

void InterruptReload(AvHAIPlayer* pBot);

bool IsHitscanWeapon(AvHAIWeapon Weapon);
float GetTimeUntilPlayerNextRefire(const AvHPlayer* Player);

BotAttackResult PerformAttackLOSCheck(AvHAIPlayer* pBot, const AvHAIWeapon Weapon, const edict_t* Target);
BotAttackResult PerformAttackLOSCheck(AvHAIPlayer* pBot, const AvHAIWeapon Weapon, const Vector TargetLocation, const edict_t* Target);
BotAttackResult PerformAttackLOSCheck(const Vector Location, const AvHAIWeapon Weapon, const edict_t* Target);

float UTIL_GetProjectileVelocityForWeapon(const AvHAIWeapon Weapon);
bool IsAreaAffectedBySpores(const Vector Location);

char* UTIL_WeaponTypeToClassname(const AvHAIWeapon WeaponType);

#endif