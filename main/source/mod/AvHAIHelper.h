#pragma once

#ifndef AVH_AI_HELPER_H
#define AVH_AI_HELPER_H

#include "AvHPlayer.h"
#include "AvHAIConstants.h"

bool UTIL_CommanderTrace(const edict_t* pEdict, const Vector& start, const Vector& end);
bool UTIL_QuickTrace(const edict_t* pEdict, const Vector& start, const Vector& end, bool bAllowStartSolid = false);
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, bool bAllowStartSolid = false);
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum, bool bAllowStartSolid = false);
edict_t* UTIL_TraceEntity(const edict_t* pEdict, const Vector& start, const Vector& end);
edict_t* UTIL_TraceEntityHull(const edict_t* pEdict, const Vector& start, const Vector& end);
Vector UTIL_GetTraceHitLocation(const Vector Start, const Vector End);
Vector UTIL_GetHullTraceHitLocation(const Vector Start, const Vector End, int HullNum);

Vector UTIL_GetGroundLocation(const Vector CheckLocation);
Vector UTIL_GetEntityGroundLocation(const edict_t* pEntity);
Vector UTIL_GetCentreOfEntity(const edict_t* Entity);
Vector UTIL_GetFloorUnderEntity(const edict_t* Edict);

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector UserLocation, const edict_t* Entity);
Vector UTIL_GetClosestPointOnEntityToLocation(const Vector Location, const edict_t* Entity, const Vector EntityLocation);

AvHAIDeployableStructureType IUSER3ToStructureType(const int inIUSER3);

bool IsEdictStructure(const edict_t* edict);
bool IsEdictHive(const edict_t* edict);

AvHAIDeployableStructureType GetStructureTypeFromEdict(const edict_t* StructureEdict);

// Returns true if this structure shoots back (turret or offence chamber)
bool IsDamagingStructure(const edict_t* StructureEdict);
// Returns true if this structure shoots back (turret or offence chamber)
bool IsDamagingStructure(AvHAIDeployableStructureType StructureType);

bool GetNearestMapLocationAtPoint(vec3_t SearchLocation, string& outLocation);

AvHAIDeployableStructureType GetDeployableObjectTypeFromEdict(const edict_t* StructureEdict);

void AIDEBUG_DrawBotPath(edict_t* OutputPlayer, AvHAIPlayer* pBot, float DrawTime = 0.0f);
void AIDEBUG_DrawPath(edict_t* OutputPlayer, vector<bot_path_node>& path, float DrawTime = 0.0f);

// Draws a white line between start and end for the given player (pEntity) for 0.1s
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end);
// Draws a white line between start and end for the given player (pEntity) for given number of seconds
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds);
// Draws a coloured line using RGB input, between start and end for the given player (pEntity) for 0.1s
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, int r, int g, int b);
// Draws a coloured line using RGB input, between start and end for the given player (pEntity) for given number of seconds
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds, int r, int g, int b);

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds);
void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds, int r, int g, int b);

void UTIL_DrawHUDText(edict_t* pEntity, char channel, float x, float y, unsigned char r, unsigned char g, unsigned char b, const char* string);

void UTIL_ClearLocalizations();
void UTIL_LocalizeText(const char* InputText, string& OutputText);

char* UTIL_TaskTypeToChar(const BotTaskType TaskType);

char* UTIL_BotRoleToChar(const AvHAIBotRole Role);

#endif