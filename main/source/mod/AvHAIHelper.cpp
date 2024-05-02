#include "AvHAIHelper.h"
#include "AvHAIMath.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAITactical.h"
#include "AvHAINavigation.h"
#include "AvHAIPlayerManager.h"

#include "AvHGamerules.h"

#include <unordered_map>

int m_spriteTexture;

std::unordered_map<const char*, std::string> LocalizedLocationsMap;

bool UTIL_CommanderTrace(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, ignore_monsters, ignore_glass, IgnoreEdict, &hit);
	return (hit.flFraction >= 1.0f);
}

bool UTIL_QuickTrace(const edict_t* pEdict, const Vector& start, const Vector& end, bool bAllowStartSolid)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, ignore_monsters, ignore_glass, IgnoreEdict, &hit);
	return (hit.flFraction >= 1.0f && !hit.fAllSolid && (bAllowStartSolid || !hit.fStartSolid));
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, bool bAllowStartSolid)
{
	int hullNum = (!FNullEnt(pEdict)) ? GetPlayerHullIndex(pEdict) : point_hull;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	TraceResult hit;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, IgnoreEdict, &hit);

	return (hit.flFraction >= 1.0f && !hit.fAllSolid && (bAllowStartSolid || !hit.fStartSolid));
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum, bool bAllowStartSolid)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, IgnoreEdict, &hit);

	return (hit.flFraction >= 1.0f && !hit.fAllSolid && (bAllowStartSolid || !hit.fStartSolid));
}

edict_t* UTIL_TraceEntity(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, IgnoreEdict, &hit);
	return hit.pHit;
}

edict_t* UTIL_TraceEntityHull(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceHull(start, end, dont_ignore_monsters, head_hull, IgnoreEdict, &hit);
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

	Vector EntityCentre = UTIL_GetCentreOfEntity(Edict) + Vector(0.0f, 0.0f, 1.0f);
	Vector TraceEnd = (EntityCentre - Vector(0.0f, 0.0f, 1000.0f));

	UTIL_TraceHull(EntityCentre, TraceEnd, ignore_monsters, head_hull, Edict->v.pContainingEntity, &hit);

	if (hit.flFraction < 1.0f)
	{
		return (hit.vecEndPos + Vector(0.0f, 0.0f, 1.0f));
	}

	return Edict->v.origin;
}

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector Location, const edict_t* Entity)
{
	return Vector(clampf(Location.x, Entity->v.absmin.x, Entity->v.absmax.x), clampf(Location.y, Entity->v.absmin.y, Entity->v.absmax.y), clampf(Location.z, Entity->v.absmin.z, Entity->v.absmax.z));
}

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector Location, const edict_t* Entity, const Vector EntityLocation)
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

bool IsEdictHive(const edict_t* edict)
{
	if (FNullEnt(edict)) { return false; }
	return (edict->v.iuser3 == AVH_USER3_HIVE);
}

bool IsDamagingStructure(const edict_t* StructureEdict)
{
	return IsDamagingStructure(GetStructureTypeFromEdict(StructureEdict));
}

bool IsDamagingStructure(AvHAIDeployableStructureType StructureType)
{
	switch (StructureType)
	{
		case STRUCTURE_ALIEN_OFFENCECHAMBER:
		case STRUCTURE_MARINE_TURRET:
			return true;
		default:
			return false;
	}

	return false;
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

void AIDEBUG_DrawBotPath(edict_t* OutputPlayer, AvHAIPlayer* pBot, float DrawTime)
{
	AIDEBUG_DrawPath(OutputPlayer, pBot->BotNavInfo.CurrentPath, DrawTime);
}

void AIDEBUG_DrawPath(edict_t* OutputPlayer, vector<bot_path_node>& path, float DrawTime)
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
			UTIL_DrawLine(OutputPlayer, FromLoc, ToLoc, DrawTime, 255, 0, 0);
			break;
		case SAMPLE_POLYFLAGS_JUMP:
		case SAMPLE_POLYFLAGS_DUCKJUMP:
			UTIL_DrawLine(OutputPlayer, FromLoc, ToLoc, DrawTime, 255, 255, 0);
			break;
		case SAMPLE_POLYFLAGS_LADDER:
		case SAMPLE_POLYFLAGS_LIFT:
			UTIL_DrawLine(OutputPlayer, FromLoc, ToLoc, DrawTime, 0, 0, 255);
			break;
		case SAMPLE_POLYFLAGS_WALLCLIMB:
			UTIL_DrawLine(OutputPlayer, FromLoc, ToLoc, DrawTime, 0, 128, 0);
			break;
		case SAMPLE_POLYFLAGS_BLOCKED:
			UTIL_DrawLine(OutputPlayer, FromLoc, ToLoc, DrawTime, 128, 128, 128);
			break;
		case SAMPLE_POLYFLAGS_TEAM1PHASEGATE:
		case SAMPLE_POLYFLAGS_TEAM2PHASEGATE:
			UTIL_DrawLine(OutputPlayer, FromLoc, ToLoc, DrawTime, 255, 128, 128);
			break;
		default:
			UTIL_DrawLine(OutputPlayer, FromLoc, ToLoc, DrawTime);
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

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds)
{
	Vector LowerBottomLeftCorner = bMin;
	Vector LowerTopLeftCorner = Vector(bMin.x, bMax.y, bMin.z);
	Vector LowerTopRightCorner = Vector(bMax.x, bMax.y, bMin.z);
	Vector LowerBottomRightCorner = Vector(bMax.x, bMin.y, bMin.z);

	Vector UpperBottomLeftCorner = Vector(bMin.x, bMin.y, bMax.z);
	Vector UpperTopLeftCorner = Vector(bMin.x, bMax.y, bMax.z);
	Vector UpperTopRightCorner = Vector(bMax.x, bMax.y, bMax.z);
	Vector UpperBottomRightCorner = Vector(bMax.x, bMin.y, bMax.z);


	UTIL_DrawLine(pEntity, LowerTopLeftCorner, LowerTopRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, LowerBottomRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, LowerBottomLeftCorner, drawTimeSeconds, 255, 255, 255);

	UTIL_DrawLine(pEntity, UpperBottomLeftCorner, UpperTopLeftCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, UpperTopLeftCorner, UpperTopRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, UpperTopRightCorner, UpperBottomRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, UpperBottomRightCorner, UpperBottomLeftCorner, drawTimeSeconds, 255, 255, 255);

	UTIL_DrawLine(pEntity, LowerBottomLeftCorner, UpperBottomLeftCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerTopLeftCorner, UpperTopLeftCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, UpperTopRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, UpperBottomRightCorner, drawTimeSeconds, 255, 255, 255);
}

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds, int r, int g, int b)
{
	Vector LowerBottomLeftCorner = bMin;
	Vector LowerTopLeftCorner = Vector(bMin.x, bMax.y, bMin.z);
	Vector LowerTopRightCorner = Vector(bMax.x, bMax.y, bMin.z);
	Vector LowerBottomRightCorner = Vector(bMax.x, bMin.y, bMin.z);

	Vector UpperBottomLeftCorner = Vector(bMin.x, bMin.y, bMax.z);
	Vector UpperTopLeftCorner = Vector(bMin.x, bMax.y, bMax.z);
	Vector UpperTopRightCorner = Vector(bMax.x, bMax.y, bMax.z);
	Vector UpperBottomRightCorner = Vector(bMax.x, bMin.y, bMax.z);


	UTIL_DrawLine(pEntity, LowerTopLeftCorner, LowerTopRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, LowerBottomRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, LowerBottomLeftCorner, drawTimeSeconds, r, g, b);

	UTIL_DrawLine(pEntity, UpperBottomLeftCorner, UpperTopLeftCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, UpperTopLeftCorner, UpperTopRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, UpperTopRightCorner, UpperBottomRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, UpperBottomRightCorner, UpperBottomLeftCorner, drawTimeSeconds, r, g, b);

	UTIL_DrawLine(pEntity, LowerBottomLeftCorner, UpperBottomLeftCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerTopLeftCorner, UpperTopLeftCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, UpperTopRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, UpperBottomRightCorner, drawTimeSeconds, r, g, b);
}

void UTIL_DrawHUDText(edict_t* pEntity, char channel, float x, float y, unsigned char r, unsigned char g, unsigned char b, const char* string)
{
	if (FNullEnt(pEntity)) { return; }

	float delta = AIMGR_GetFrameDelta();

	// TODO: Be able to turn these off as a preference
	hudtextparms_t	theTextParms;

	// Init text parms
	theTextParms.x = x;
	theTextParms.y = y;
	theTextParms.effect = 0;
	theTextParms.r1 = 240;
	theTextParms.g1 = 240;
	theTextParms.b1 = 240;
	theTextParms.a1 = 128;
	theTextParms.r2 = 240;
	theTextParms.g2 = 240;
	theTextParms.b2 = 240;
	theTextParms.a2 = 128;
	theTextParms.fadeinTime = .0f;
	theTextParms.fadeoutTime = .0f;
	theTextParms.holdTime = 0.2f;
	theTextParms.channel = channel;

	UTIL_HudMessage(CBaseEntity::Instance(pEntity), theTextParms, string);
}

void UTIL_ClearLocalizations()
{
	LocalizedLocationsMap.clear();
}

void UTIL_LocalizeText(const char* InputText, string& OutputText)
{
	// Don't localize empty strings
	if (!strcmp(InputText, ""))
	{
		OutputText = "";
	}

	char theInputString[1024];

	sprintf(theInputString, "%s", InputText);

	std::unordered_map<const char*, std::string>::const_iterator FoundLocalization = LocalizedLocationsMap.find(theInputString);

	if (FoundLocalization != LocalizedLocationsMap.end())
	{
		OutputText = FoundLocalization->second;
		return;
	}

	char filename[256];

	std::string localizedString(theInputString);

	string titlesPath = string(getModDirectory()) + "/titles.txt";
	strcpy(filename, titlesPath.c_str());

	std::ifstream cFile(filename);
	if (cFile.is_open())
	{
		std::string line;
		while (getline(cFile, line))
		{
			line.erase(std::remove_if(line.begin(), line.end(), ::isspace),
				line.end());
			if (line[0] == '/' || line.empty())
				continue;

			if (line.compare(theInputString) == 0)
			{
				getline(cFile, line);
				getline(cFile, localizedString);
				break;

			}
		}
	}

	char theOutputString[1024];

	sprintf(theOutputString, "%s", localizedString.c_str());

	string Delimiter = "Hive -";
	auto delimiterPos = localizedString.find(Delimiter);

	if (delimiterPos == std::string::npos)
	{
		Delimiter = "Hive Location -";
		delimiterPos = localizedString.find(Delimiter);
	}

	if (delimiterPos == std::string::npos)
	{
		Delimiter = "Hive Location  -";
		delimiterPos = localizedString.find("Hive Location  -");
	}

	if (delimiterPos != std::string::npos)
	{
		auto AreaName = localizedString.substr(delimiterPos + Delimiter.length());

		AreaName.erase(0, AreaName.find_first_not_of(" \r\n\t\v\f"));

		sprintf(theOutputString, "%s", AreaName.c_str());
	}

	OutputText = theOutputString;

	LocalizedLocationsMap[InputText] = OutputText;

}

char* UTIL_TaskTypeToChar(const BotTaskType TaskType)
{
	switch (TaskType)
	{
	case TASK_ATTACK:
		return "Attack";
	case TASK_BUILD:
		return "Build";
	case TASK_CAP_RESNODE:
		return "Cap Res Node";
	case TASK_COMMAND:
		return "Take Command";
	case TASK_DEFEND:
		return "Defend Structure";
	case TASK_EVOLVE:
		return "Evolve";
	case TASK_GET_AMMO:
		return "Get Ammo Pack";
	case TASK_GET_EQUIPMENT:
		return "Get Equipment";
	case TASK_GET_HEALTH:
		return "Get Health Pack";
	case TASK_GET_WEAPON:
		return "Get Weapon";
	case TASK_GUARD:
		return "Guard";
	case TASK_HEAL:
		return "Heal Target";
	case TASK_MOVE:
		return "Move to Location";
	case TASK_PLACE_MINE:
		return "Place Mine";
	case TASK_REINFORCE_STRUCTURE:
		return "Reinforce Structure";
	case TASK_RESUPPLY:
		return "Resupply";
	case TASK_SECURE_HIVE:
		return "Secure Hive";
	case TASK_TOUCH:
		return "Touch Trigger";
	case TASK_WELD:
		return "Weld Target";
	case TASK_ATTACK_BASE:
		return "Attack Enemy Base";
	default:
		return "None";
	}

	return "None";
}

char* UTIL_BotRoleToChar(const AvHAIBotRole Role)
{
	switch (Role)
	{
	case BOT_ROLE_ASSAULT:
		return "Assault";
	case BOT_ROLE_BOMBARDIER:
		return "Bombardier";
	case BOT_ROLE_BUILDER:
		return "Builder";
	case BOT_ROLE_COMMAND:
		return "Commander";
	case BOT_ROLE_FIND_RESOURCES:
		return "Res Capper";
	case BOT_ROLE_HARASS:
		return "Harrasser";
	case BOT_ROLE_SWEEPER:
		return "Sweeper";
	default:
		return "None";
	}

	return "None";
}