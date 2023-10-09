#pragma once

#ifndef AVH_AI_HELPER_H
#define AVH_AI_HELPER_H

#include "../AvHPlayer.h"
#include "AvHAIConstants.h"

bool UTIL_CommanderTrace(const edict_t* pEdict, const Vector& start, const Vector& end);
bool UTIL_QuickTrace(const edict_t* pEdict, const Vector& start, const Vector& end);
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end);
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum);
edict_t* UTIL_TraceEntity(const edict_t* pEdict, const Vector& start, const Vector& end);
Vector UTIL_GetTraceHitLocation(const Vector Start, const Vector End);

Vector UTIL_GetGroundLocation(const Vector CheckLocation);
Vector UTIL_GetEntityGroundLocation(const edict_t* pEntity);
Vector UTIL_GetCentreOfEntity(const edict_t* Entity);
Vector UTIL_GetFloorUnderEntity(const edict_t* Edict);

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector UserLocation, edict_t* Entity);

AvHAIDeployableStructureType IUSER3ToStructureType(const int inIUSER3);

bool IsEdictStructure(const edict_t* edict);

AvHAIDeployableStructureType GetStructureTypeFromEdict(const edict_t* StructureEdict);

bool GetNearestMapLocationAtPoint(vec3_t SearchLocation, string& outLocation);

AvHAIDeployableStructureType GetDeployableObjectTypeFromEdict(const edict_t* StructureEdict);

#endif