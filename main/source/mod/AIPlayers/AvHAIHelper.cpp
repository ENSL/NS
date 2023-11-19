#include "AvHAIHelper.h"
#include "AvHAIMath.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAITactical.h"
#include "AvHAINavigation.h"

#include "../AvHGamerules.h"

int m_spriteTexture;

bool UTIL_CommanderTrace(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, ignore_monsters, ignore_glass, IgnoreEdict, &hit);
	return (hit.flFraction >= 1.0f);
}

bool UTIL_QuickTrace(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, ignore_monsters, ignore_glass, IgnoreEdict, &hit);
	return (hit.flFraction >= 1.0f && !hit.fAllSolid);
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	int hullNum = (!FNullEnt(pEdict)) ? GetPlayerHullIndex(pEdict) : point_hull;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	TraceResult hit;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, IgnoreEdict, &hit);

	return (hit.flFraction >= 1.0f && !hit.fAllSolid);
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, IgnoreEdict, &hit);

	return (hit.flFraction >= 1.0f && !hit.fAllSolid);
}

edict_t* UTIL_TraceEntity(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, IgnoreEdict, &hit);
	return hit.pHit;
}

Vector UTIL_GetTraceHitLocation(const Vector Start, const Vector End)
{
	TraceResult hit;
	UTIL_TraceHull(Start, End, ignore_monsters, point_hull, NULL, &hit);

	if (hit.flFraction < 1.0f && !hit.fAllSolid)
	{
		return hit.vecEndPos;
	}

	return Start;
}

Vector UTIL_GetHullTraceHitLocation(const Vector Start, const Vector End, int HullNum)
{
	TraceResult hit;
	UTIL_TraceHull(Start, End, ignore_monsters, HullNum, NULL, &hit);

	if (hit.flFraction < 1.0f && !hit.fAllSolid)
	{
		return hit.vecEndPos;
	}

	return Start;
}

Vector UTIL_GetGroundLocation(const Vector CheckLocation)
{
	if (vIsZero(CheckLocation)) { return g_vecZero; }

	TraceResult hit;

	UTIL_TraceHull(CheckLocation, (CheckLocation - Vector(0.0f, 0.0f, 1000.0f)), ignore_monsters, head_hull, nullptr, &hit);

	if (hit.flFraction < 1.0f)
	{
		return hit.vecEndPos;
	}

	return CheckLocation;
}

Vector UTIL_GetEntityGroundLocation(const edict_t* pEntity)
{

	if (FNullEnt(pEntity)) { return g_vecZero; }

	bool bIsPlayer = IsEdictPlayer(pEntity);

	if (bIsPlayer)
	{
		if (IsPlayerOnLadder(pEntity))
		{
			return UTIL_GetFloorUnderEntity(pEntity);
		}

		if (pEntity->v.flags & FL_ONGROUND)
		{
			if (FNullEnt(pEntity->v.groundentity))
			{
				return GetPlayerBottomOfCollisionHull(pEntity);
			}

			if (!IsEdictPlayer(pEntity->v.groundentity) && GetDeployableObjectTypeFromEdict(pEntity->v.groundentity) == STRUCTURE_NONE)
			{
				return GetPlayerBottomOfCollisionHull(pEntity);
			}
		}

		return UTIL_GetFloorUnderEntity(pEntity);
	}

	if (GetDeployableObjectTypeFromEdict(pEntity) == STRUCTURE_ALIEN_HIVE)
	{
		return UTIL_GetFloorUnderEntity(pEntity);

		const AvHAIHiveDefinition* Hive = AITAC_GetHiveFromEdict(pEntity);

		if (Hive)
		{
			return Hive->FloorLocation;
		}
		else
		{
			return UTIL_GetFloorUnderEntity(pEntity);
		}

	}

	Vector Centre = UTIL_GetCentreOfEntity(pEntity);
	Centre.z = pEntity->v.absmin.z + 1.0f;

	return Centre;
}

Vector UTIL_GetCentreOfEntity(const edict_t* Entity)
{
	if (!Entity) { return g_vecZero; }
	return (Entity->v.absmin + (Entity->v.size * 0.5f));
}

Vector UTIL_GetFloorUnderEntity(const edict_t* Edict)
{
	if (FNullEnt(Edict)) { return g_vecZero; }

	TraceResult hit;

	Vector EntityCentre = UTIL_GetCentreOfEntity(Edict);

	UTIL_TraceHull(EntityCentre, (EntityCentre - Vector(0.0f, 0.0f, 1000.0f)), ignore_monsters, GetPlayerHullIndex(Edict), Edict->v.pContainingEntity, &hit);

	if (hit.flFraction < 1.0f)
	{
		return (hit.vecEndPos + Vector(0.0f, 0.0f, 1.0f));
	}

	return Edict->v.origin;
}

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector Location, edict_t* Entity)
{
	return Vector(clampf(Location.x, Entity->v.absmin.x, Entity->v.absmax.x), clampf(Location.y, Entity->v.absmin.y, Entity->v.absmax.y), clampf(Location.z, Entity->v.absmin.z, Entity->v.absmax.z));
}

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector Location, edict_t* Entity, const Vector EntityLocation)
{
	Vector MinVec = EntityLocation - (Entity->v.size * 0.5f);
	Vector MaxVec = EntityLocation + (Entity->v.size * 0.5f);

	return Vector(clampf(Location.x, MinVec.x, MaxVec.x), clampf(Location.y, MinVec.y, MaxVec.y), clampf(Location.z, MinVec.z, MaxVec.z));
}

AvHAIDeployableStructureType IUSER3ToStructureType(const int inIUSER3)
{
	if (inIUSER3 == AVH_USER3_COMMANDER_STATION) { return STRUCTURE_MARINE_COMMCHAIR; }
	if (inIUSER3 == AVH_USER3_RESTOWER) { return STRUCTURE_MARINE_RESTOWER; }
	if (inIUSER3 == AVH_USER3_INFANTRYPORTAL) { return STRUCTURE_MARINE_INFANTRYPORTAL; }
	if (inIUSER3 == AVH_USER3_ARMORY) { return STRUCTURE_MARINE_ARMOURY; }
	if (inIUSER3 == AVH_USER3_ADVANCED_ARMORY) { return STRUCTURE_MARINE_ADVARMOURY; }
	if (inIUSER3 == AVH_USER3_TURRET_FACTORY) { return STRUCTURE_MARINE_TURRETFACTORY; }
	if (inIUSER3 == AVH_USER3_ADVANCED_TURRET_FACTORY) { return STRUCTURE_MARINE_ADVTURRETFACTORY; }
	if (inIUSER3 == AVH_USER3_TURRET) { return STRUCTURE_MARINE_TURRET; }
	if (inIUSER3 == AVH_USER3_SIEGETURRET) { return STRUCTURE_MARINE_SIEGETURRET; }
	if (inIUSER3 == AVH_USER3_ARMSLAB) { return STRUCTURE_MARINE_ARMSLAB; }
	if (inIUSER3 == AVH_USER3_PROTOTYPE_LAB) { return STRUCTURE_MARINE_PROTOTYPELAB; }
	if (inIUSER3 == AVH_USER3_OBSERVATORY) { return STRUCTURE_MARINE_OBSERVATORY; }
	if (inIUSER3 == AVH_USER3_PHASEGATE) { return STRUCTURE_MARINE_PHASEGATE; }
	if (inIUSER3 == AVH_USER3_MINE) { return STRUCTURE_MARINE_DEPLOYEDMINE; }

	if (inIUSER3 == AVH_USER3_HIVE) { return STRUCTURE_ALIEN_HIVE; }
	if (inIUSER3 == AVH_USER3_ALIENRESTOWER) { return STRUCTURE_ALIEN_RESTOWER; }
	if (inIUSER3 == AVH_USER3_DEFENSE_CHAMBER) { return STRUCTURE_ALIEN_DEFENCECHAMBER; }
	if (inIUSER3 == AVH_USER3_SENSORY_CHAMBER) { return STRUCTURE_ALIEN_SENSORYCHAMBER; }
	if (inIUSER3 == AVH_USER3_MOVEMENT_CHAMBER) { return STRUCTURE_ALIEN_MOVEMENTCHAMBER; }
	if (inIUSER3 == AVH_USER3_OFFENSE_CHAMBER) { return STRUCTURE_ALIEN_OFFENCECHAMBER; }

	return STRUCTURE_NONE;

}

AvHAIDeployableStructureType GetDeployableObjectTypeFromEdict(const edict_t* StructureEdict)
{
	if (FNullEnt(StructureEdict)) { return STRUCTURE_NONE; }

	return IUSER3ToStructureType(StructureEdict->v.iuser3);
}

bool IsEdictStructure(const edict_t* edict)
{
	return (GetDeployableObjectTypeFromEdict(edict) != STRUCTURE_NONE);
}

AvHAIDeployableStructureType GetStructureTypeFromEdict(const edict_t* StructureEdict)
{
	if (FNullEnt(StructureEdict)) { return STRUCTURE_NONE; }

	return IUSER3ToStructureType(StructureEdict->v.iuser3);
}

bool GetNearestMapLocationAtPoint(vec3_t SearchLocation, string& outLocation)
{
	bool theSuccess = false;

	const AvHBaseInfoLocationListType& inLocations = GetGameRules()->GetInfoLocations();

	bool bFoundNearest = false;
	float MinDist = 0.0f;

	// Look at our current position, and see if we lie within of the map locations
	for (AvHBaseInfoLocationListType::const_iterator theIter = inLocations.begin(); theIter != inLocations.end(); theIter++)
	{
		if (theIter->GetIsPointInRegion(SearchLocation))
		{
			outLocation = theIter->GetLocationName();
			return true;
		}

		float NearestX = clampf(SearchLocation.x, theIter->GetMinExtent().x, theIter->GetMaxExtent().x);
		float NearestY = clampf(SearchLocation.y, theIter->GetMinExtent().y, theIter->GetMaxExtent().y);

		float ThisDist = vDist2DSq(SearchLocation, Vector(NearestX, NearestY, 0.0f));

		if (!bFoundNearest || ThisDist < MinDist)
		{
			outLocation = theIter->GetLocationName();
			bFoundNearest = true;
			theSuccess = true;
			MinDist = ThisDist;
		}
	}

	return theSuccess;
}

void AIDEBUG_DrawBotPath(AvHAIPlayer* pBot, float DrawTime)
{
	AIDEBUG_DrawPath(pBot->BotNavInfo.CurrentPath, DrawTime);
}

void AIDEBUG_DrawPath(vector<bot_path_node>& path, float DrawTime)
{
	if (path.size() == 0) { return; }

	for (auto it = path.begin(); it != path.end(); it++)
	{
		Vector FromLoc = it->FromLocation;
		Vector ToLoc = it->Location;

		switch (it->flag)
		{
		case SAMPLE_POLYFLAGS_WELD:
		case SAMPLE_POLYFLAGS_DOOR:
			UTIL_DrawLine(INDEXENT(1), FromLoc, ToLoc, DrawTime, 255, 0, 0);
			break;
		case SAMPLE_POLYFLAGS_JUMP:
		case SAMPLE_POLYFLAGS_DUCKJUMP:
			UTIL_DrawLine(INDEXENT(1), FromLoc, ToLoc, DrawTime, 255, 255, 0);
			break;
		case SAMPLE_POLYFLAGS_LADDER:
			UTIL_DrawLine(INDEXENT(1), FromLoc, ToLoc, DrawTime, 0, 0, 255);
			break;
		case SAMPLE_POLYFLAGS_WALLCLIMB:
			UTIL_DrawLine(INDEXENT(1), FromLoc, ToLoc, DrawTime, 0, 128, 0);
			break;
		case SAMPLE_POLYFLAGS_BLOCKED:
			UTIL_DrawLine(INDEXENT(1), FromLoc, ToLoc, DrawTime, 128, 128, 128);
			break;
		case SAMPLE_POLYFLAGS_TEAM1PHASEGATE:
		case SAMPLE_POLYFLAGS_TEAM2PHASEGATE:
			UTIL_DrawLine(INDEXENT(1), FromLoc, ToLoc, DrawTime, 255, 128, 128);
			break;
		default:
			UTIL_DrawLine(INDEXENT(1), FromLoc, ToLoc, DrawTime);
			break;
		}
	}
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end)
{
	if (FNullEnt(pEntity) || pEntity->free) { return; }

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(1);               // framestart
	WRITE_BYTE(10);              // framerate
	WRITE_BYTE(1);              // life in 0.1's
	WRITE_BYTE(5);           // width
	WRITE_BYTE(0);           // noise

	WRITE_BYTE(255);             // r, g, b
	WRITE_BYTE(255);           // r, g, b
	WRITE_BYTE(255);            // r, g, b

	WRITE_BYTE(250);      // brightness
	WRITE_BYTE(5);           // speed
	MESSAGE_END();
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds)
{
	if (FNullEnt(pEntity) || pEntity->free) { return; }

	int timeTenthSeconds = (int)floorf(drawTimeSeconds * 10.0f);
	timeTenthSeconds = fmaxf(timeTenthSeconds, 1);

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(1);               // framestart
	WRITE_BYTE(10);              // framerate
	WRITE_BYTE(timeTenthSeconds);              // life in 0.1's
	WRITE_BYTE(5);           // width
	WRITE_BYTE(0);           // noise

	WRITE_BYTE(255);             // r, g, b
	WRITE_BYTE(255);           // r, g, b
	WRITE_BYTE(255);            // r, g, b

	WRITE_BYTE(250);      // brightness
	WRITE_BYTE(5);           // speed
	MESSAGE_END();
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds, int r, int g, int b)
{
	if (FNullEnt(pEntity) || pEntity->free) { return; }

	int timeTenthSeconds = (int)floorf(drawTimeSeconds * 10.0f);
	timeTenthSeconds = fmaxf(timeTenthSeconds, 1);

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(1);               // framestart
	WRITE_BYTE(10);              // framerate
	WRITE_BYTE(timeTenthSeconds);              // life in 0.1's
	WRITE_BYTE(5);           // width
	WRITE_BYTE(0);           // noise

	WRITE_BYTE(r);             // r, g, b
	WRITE_BYTE(g);           // r, g, b
	WRITE_BYTE(b);            // r, g, b

	WRITE_BYTE(250);      // brightness
	WRITE_BYTE(5);           // speed
	MESSAGE_END();
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, int r, int g, int b)
{
	if (FNullEnt(pEntity) || pEntity->free) { return; }

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(1);               // framestart
	WRITE_BYTE(10);              // framerate
	WRITE_BYTE(1);              // life in 0.1's
	WRITE_BYTE(5);           // width
	WRITE_BYTE(0);           // noise

	WRITE_BYTE(r);             // r, g, b
	WRITE_BYTE(g);           // r, g, b
	WRITE_BYTE(b);            // r, g, b

	WRITE_BYTE(250);      // brightness
	WRITE_BYTE(5);           // speed
	MESSAGE_END();
}