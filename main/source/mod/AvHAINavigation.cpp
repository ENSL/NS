//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_navigation.cpp
// 
// Handles all bot path finding and movement
//

#include "AvHAINavigation.h"
#include "AvHAIMath.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIHelper.h"
#include "AvHAIPlayerManager.h"
#include "AvHAITactical.h"
#include "AvHAITask.h"
#include "AvHAIWeaponHelper.h"
#include "AvHAIConfig.h"

#include "AvHWeldable.h"
#include "AvHServerUtil.h"
#include "AvHGamerules.h"

#include "../../dlls/triggers.h"

#include <stdlib.h>
#include <math.h>

#include "../../dlls/plats.h"

#include "DetourNavMesh.h"
#include "DetourCommon.h"
#include "DetourTileCache.h"
#include "DetourTileCacheBuilder.h"
#include "DetourNavMeshBuilder.h"
#include "fastlz/fastlz.c"
#include "DetourAlloc.h"

#include <cfloat>

vector<nav_door> NavDoors;
vector<nav_weldable> NavWeldableObstacles;
vector<AvHAIOffMeshConnection> BaseMapConnections;

nav_mesh NavMeshes[MAX_NAV_MESHES] = { }; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)
nav_profile BaseNavProfiles[MAX_NAV_PROFILES] = { }; // Array of nav profiles

AvHAINavMeshStatus NavmeshStatus = NAVMESH_STATUS_PENDING;

vector<NavHint> MapNavHints;

extern bool bNavMeshModified;

bool bTileCacheUpToDate = false;

#ifdef BOTDEBUG
extern vector<bot_path_node> DebugPath;
#endif

struct NavMeshSetHeader
{
	int magic;
	int version;
	int numTiles;
	dtNavMeshParams params;
	int MeshBuildOffset;
};

struct TileCacheSetExportHeader
{
	int magic;
	int version;

	int numRegularTiles;
	dtNavMeshParams regularMeshParams;
	dtTileCacheParams regularCacheParams;

	int numOnosTiles;
	dtNavMeshParams onosMeshParams;
	dtTileCacheParams onosCacheParams;

	int numBuildingTiles;
	dtNavMeshParams buildingMeshParams;
	dtTileCacheParams buildingCacheParams;

	int regularNavOffset;
	int onosNavOffset;
	int buildingNavOffset;

	int NumOffMeshCons;

	int OffMeshConsOffset;
};

struct TileCacheBuildHeader
{
	int magic;
	int version;
	int numRegularTiles;
	int numOnosTiles;
	int numBuildingTiles;

	dtNavMeshParams regularMeshParams;
	dtTileCacheParams regularCacheParams;

	dtNavMeshParams onosMeshParams;
	dtTileCacheParams onosCacheParams;

	dtNavMeshParams buildingMeshParams;
	dtTileCacheParams buildingCacheParams;

	int NumSurfTypes;
	int SurfTypesOffset;

	int NumOffMeshCons;
	int OffMeshConsOffset;

	int NumConvexVols;
	int ConvexVolOffset;

	int NumNavHints;
	int NavHintOffset;
};

struct TileCacheTileHeader
{
	dtCompressedTileRef tileRef;
	int dataSize;
};

struct NavMeshTileHeader
{
	dtTileRef tileRef;
	int dataSize;
};

struct OffMeshConnectionDef
{
	unsigned int UserID = 0;
	float spos[3] = { 0.0f, 0.0f, 0.0f };
	float epos[3] = { 0.0f, 0.0f, 0.0f };
	bool bBiDir = false;
	float Rad = 0.0f;
	unsigned char Area = 0;
	unsigned int Flag = 0;
	bool bPendingDelete = false;
	bool bDirty = false;
};

struct FastLZCompressor : public dtTileCacheCompressor
{
	virtual int maxCompressedSize(const int bufferSize)
	{
		return (int)(bufferSize * 1.05f);
	}

	virtual dtStatus compress(const unsigned char* buffer, const int bufferSize,
		unsigned char* compressed, const int /*maxCompressedSize*/, int* compressedSize)
	{
		*compressedSize = fastlz_compress((const void* const)buffer, bufferSize, compressed);
		return DT_SUCCESS;
	}

	virtual dtStatus decompress(const unsigned char* compressed, const int compressedSize,
		unsigned char* buffer, const int maxBufferSize, int* bufferSize)
	{
		*bufferSize = fastlz_decompress(compressed, compressedSize, buffer, maxBufferSize);
		return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
	}
};

struct LinearAllocator : public dtTileCacheAlloc
{
	unsigned char* buffer;
	size_t capacity;
	size_t top;
	size_t high;

	LinearAllocator(const size_t cap) : buffer(0), capacity(0), top(0), high(0)
	{
		resize(cap);
	}

	~LinearAllocator()
	{
		dtFree(buffer);
	}

	void resize(const size_t cap)
	{
		if (buffer) dtFree(buffer);
		buffer = (unsigned char*)dtAlloc(cap, DT_ALLOC_PERM);
		capacity = cap;
	}

	virtual void reset()
	{
		high = dtMax(high, top);
		top = 0;
	}

	virtual void* alloc(const size_t size)
	{
		if (!buffer)
			return 0;
		if (top + size > capacity)
			return 0;
		unsigned char* mem = &buffer[top];
		top += size;
		return mem;
	}

	virtual void free(void* /*ptr*/)
	{
		// Empty
	}
};

struct MeshProcess : public dtTileCacheMeshProcess
{

	inline MeshProcess()
	{}

	inline void init(OffMeshConnectionDef* OffMeshConnData, int NumConns)
	{

	}

	virtual void process(struct dtNavMeshCreateParams* params,
		unsigned char* polyAreas, unsigned int* polyFlags)
	{
		// Update poly flags from areas.
		for (int i = 0; i < params->polyCount; ++i)
		{
			if (polyAreas[i] == DT_TILECACHE_WALKABLE_AREA)
			{
				polyAreas[i] = SAMPLE_POLYAREA_GROUND;
				polyFlags[i] = SAMPLE_POLYFLAGS_WALK;
			}
			else if (polyAreas[i] == DT_TILECACHE_CROUCH_AREA)
			{
				polyAreas[i] = SAMPLE_POLYAREA_CROUCH;
				polyFlags[i] = SAMPLE_POLYFLAGS_WALK;
			}
			else if (polyAreas[i] == DT_TILECACHE_WALLCLIMB_AREA)
			{
				polyAreas[i] = SAMPLE_POLYAREA_GROUND;
				polyFlags[i] = SAMPLE_POLYFLAGS_WALLCLIMB;
			}
			else if (polyAreas[i] == DT_TILECACHE_LADDER_AREA)
			{
				polyAreas[i] = SAMPLE_POLYAREA_GROUND;
				polyFlags[i] = SAMPLE_POLYFLAGS_LADDER;
			}
			else if (polyAreas[i] == DT_TILECACHE_BLOCKED_AREA)
			{
				polyAreas[i] = SAMPLE_POLYAREA_BLOCKED;
				polyFlags[i] = SAMPLE_POLYFLAGS_BLOCKED;
			}
			else if (polyAreas[i] == DT_TILECACHE_TEAM1STRUCTURE_AREA)
			{
				polyAreas[i] = SAMPLE_POLYAREA_STRUCTUREBLOCK;
				polyFlags[i] = SAMPLE_POLYFLAGS_TEAM1STRUCTURE;
			}
			else if (polyAreas[i] == DT_TILECACHE_TEAM2STRUCTURE_AREA)
			{
				polyAreas[i] = SAMPLE_POLYAREA_STRUCTUREBLOCK;
				polyFlags[i] = SAMPLE_POLYFLAGS_TEAM2STRUCTURE;
			}
			else if (polyAreas[i] == DT_TILECACHE_WELD_AREA)
			{
				polyAreas[i] = SAMPLE_POLYAREA_OBSTRUCTION;
				polyFlags[i] = SAMPLE_POLYFLAGS_WELD;
			}
			else if (polyAreas[i] == DT_TILECACHE_DOOR_AREA)
			{
				polyAreas[i] = SAMPLE_POLYAREA_OBSTRUCTION;
				polyFlags[i] = SAMPLE_POLYFLAGS_DOOR;
			}
		}

	}
};

void AIDEBUG_DrawTemporaryObstacles(float DrawTime)
{
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE));
	const dtTileCache* m_tileCache = UTIL_GetTileCacheForProfile(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE));

	if (m_navMesh)
	{
		int NumObstacles = m_tileCache->getObstacleCount();

		for (int i = 0; i < NumObstacles; i++)
		{
			const dtTileCacheObstacle* ObstacleRef = m_tileCache->getObstacle(i);

			if (!ObstacleRef || ObstacleRef->state != DT_OBSTACLE_PROCESSED) { continue; }

			int r, g, b;

			

			if (ObstacleRef->type == ObstacleType::DT_OBSTACLE_BOX)
			{
				switch (ObstacleRef->box.area)
				{
					case DT_TILECACHE_NULL_AREA:
						r = 128;
						g = 128;
						b = 128;
						break;
					case DT_TILECACHE_CROUCH_AREA:
						r = 128;
						g = 0;
						b = 0;
						break;
					case DT_TILECACHE_BLOCKED_AREA:
						r = 255;
						g = 255;
						b = 0;
						break;
					case DT_TILECACHE_WALLCLIMB_AREA:
						r = 0;
						g = 128;
						b = 0;
						break;
					case DT_TILECACHE_LADDER_AREA:
						r = 0;
						g = 0;
						b = 255;
						break;
					case DT_TILECACHE_TEAM1STRUCTURE_AREA:
						r = 0;
						g = 0;
						b = 255;
						break;
					case DT_TILECACHE_TEAM2STRUCTURE_AREA:
						r = 0;
						g = 128;
						b = 0;
						break;
					case DT_TILECACHE_WELD_AREA:
						r = 255;
						g = 0;
						b = 0;
						break;
					case DT_TILECACHE_DOOR_AREA:
						r = 0;
						g = 128;
						b = 128;
						break;
				}

				Vector bMin = Vector(ObstacleRef->box.bmin[0], -ObstacleRef->box.bmin[2], ObstacleRef->box.bmin[1]);
				Vector bMax = Vector(ObstacleRef->box.bmax[0], -ObstacleRef->box.bmax[2], ObstacleRef->box.bmax[1]);

				UTIL_DrawBox(INDEXENT(1), bMin, bMax, DrawTime, r, g, b);
				continue;
			}

			if (ObstacleRef->type == ObstacleType::DT_OBSTACLE_CYLINDER)
			{
				switch (ObstacleRef->cylinder.area)
				{
				case DT_TILECACHE_NULL_AREA:
					r = 128;
					g = 128;
					b = 128;
					break;
				case DT_TILECACHE_CROUCH_AREA:
					r = 128;
					g = 0;
					b = 0;
					break;
				case DT_TILECACHE_BLOCKED_AREA:
					r = 255;
					g = 255;
					b = 0;
					break;
				case DT_TILECACHE_WALLCLIMB_AREA:
					r = 0;
					g = 128;
					b = 0;
					break;
				case DT_TILECACHE_LADDER_AREA:
					r = 0;
					g = 0;
					b = 255;
					break;
				case DT_TILECACHE_TEAM1STRUCTURE_AREA:
					r = 0;
					g = 0;
					b = 255;
					break;
				case DT_TILECACHE_TEAM2STRUCTURE_AREA:
					r = 0;
					g = 128;
					b = 0;
					break;
				case DT_TILECACHE_WELD_AREA:
					r = 255;
					g = 0;
					b = 0;
					break;
				case DT_TILECACHE_DOOR_AREA:
					r = 0;
					g = 128;
					b = 128;
					break;
				}

				float Radius = ObstacleRef->cylinder.radius;
				float Height = ObstacleRef->cylinder.height;

				// The location of obstacles in Recast are at the bottom of the shape, not the centre
				Vector Centre = Vector(ObstacleRef->cylinder.pos[0], -ObstacleRef->cylinder.pos[2], ObstacleRef->cylinder.pos[1] + (Height * 0.5f));

				if (vDist2DSq(INDEXENT(1)->v.origin, Centre) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { continue; }

				Vector bMin = Centre - Vector(Radius, Radius, Height * 0.5f);
				Vector bMax = Centre + Vector(Radius, Radius, (Height * 0.5f));

				UTIL_DrawBox(INDEXENT(1), bMin, bMax, DrawTime, r, g, b);
				continue;
			}

		}
	}
}

void AIDEBUG_DrawOffMeshConnections(float DrawTime)
{
	if (NavMeshes[REGULAR_NAV_MESH].tileCache)
	{
		for (int i = 0; i < NavMeshes[REGULAR_NAV_MESH].tileCache->getOffMeshCount(); i++)
		{
			const dtOffMeshConnection* con = NavMeshes[REGULAR_NAV_MESH].tileCache->getOffMeshConnection(i);

			if (con->state == DT_OFFMESH_EMPTY || con->state == DT_OFFMESH_REMOVING) { continue; }

			Vector StartLine = Vector(con->pos[0], -con->pos[2], con->pos[1]);
			Vector EndLine = Vector(con->pos[3], -con->pos[5], con->pos[4]);
			switch (con->flags)
			{
				case SAMPLE_POLYFLAGS_WALK:
					UTIL_DrawLine(INDEXENT(1), StartLine, EndLine, DrawTime, 255, 255, 255);
					break;
				case SAMPLE_POLYFLAGS_JUMP:
					UTIL_DrawLine(INDEXENT(1), StartLine, EndLine, DrawTime, 255, 255, 0);
					break;
				case SAMPLE_POLYFLAGS_WALLCLIMB:
					UTIL_DrawLine(INDEXENT(1), StartLine, EndLine, DrawTime, 0, 255, 0);
					break;
				case SAMPLE_POLYFLAGS_FALL:
					UTIL_DrawLine(INDEXENT(1), StartLine, EndLine, DrawTime, 255, 0, 0);
					break;
				case SAMPLE_POLYFLAGS_LADDER:
					UTIL_DrawLine(INDEXENT(1), StartLine, EndLine, DrawTime, 0, 0, 255);
					break;
				case SAMPLE_POLYFLAGS_TEAM1PHASEGATE:
				case SAMPLE_POLYFLAGS_TEAM2PHASEGATE:
					UTIL_DrawLine(INDEXENT(1), StartLine, EndLine, DrawTime, 255, 128, 128);
					break;
				case SAMPLE_POLYFLAGS_DISABLED:
					UTIL_DrawLine(INDEXENT(1), StartLine, EndLine, DrawTime, 128, 128, 128);
					break;
				case SAMPLE_POLYFLAGS_WELD:
					UTIL_DrawLine(INDEXENT(1), StartLine, EndLine, DrawTime, 255, 165, 0);
					break;
				default:
					UTIL_DrawLine(INDEXENT(1), StartLine, EndLine, DrawTime, 0, 255, 255);
					break;
			}
		}

	}
}

bool UTIL_UpdateTileCache()
{
	bTileCacheUpToDate = true;

	for (int i = 0; i < MAX_NAV_MESHES; i++)
	{
		if (NavMeshes[i].tileCache)
		{
			bool bUpToDate;
			NavMeshes[i].tileCache->update(0.0f, NavMeshes[i].navMesh, &bUpToDate);
			if (!bUpToDate) { bTileCacheUpToDate = false; }
		}
	}

	return bTileCacheUpToDate;
}

Vector UTIL_AdjustPointAwayFromNavWall(const Vector Location, const float MaxDistanceFromWall)
{

	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(BaseNavProfiles[SKULK_BASE_NAV_PROFILE]);
	const dtQueryFilter* m_navFilter = &BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters;

	float Pos[3] = { Location.x, Location.z, -Location.y };

	float HitDist = 0.0f;
	float HitPos[3] = { 0.0f, 0.0f, 0.0f };
	float HitNorm[3] = { 0.0f, 0.0f, 0.0f };

	dtPolyRef StartPoly = UTIL_GetNearestPolyRefForLocation(Location);

	dtStatus Result = m_navQuery->findDistanceToWall(StartPoly, Pos, MaxDistanceFromWall, m_navFilter, &HitDist, HitPos, HitNorm);

	if (dtStatusSucceed(Result))
	{
		float AdjustDistance = MaxDistanceFromWall - HitDist;

		Vector HitPosVector = Vector(HitPos[0], -HitPos[2], HitPos[1]);

		Vector AdjustDir = (HitDist > 0.1f) ? UTIL_GetVectorNormal2D(Location - HitPosVector) : Vector(HitNorm[0], -HitNorm[2], HitNorm[1]);

		Vector AdjustLocation = Location + (AdjustDir * AdjustDistance);

		float AdjustLoc[3] = { AdjustLocation.x, AdjustLocation.z, -AdjustLocation.y };

		if (UTIL_TraceNav(BaseNavProfiles[ALL_NAV_PROFILE], Location, AdjustLocation, 0.1f))
		{
			return AdjustLocation;
		}
		else
		{
			return Location;
		}
	}

	return Location;
}

Vector UTIL_GetNearestPointOnNavWall(AvHAIPlayer* pBot, const float MaxRadius)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	Vector Location = pBot->CurrentFloorPosition;

	float Pos[3] = { Location.x, Location.z, -Location.y };

	float HitDist = 0.0f;
	float HitPos[3] = { 0.0f, 0.0f, 0.0f };
	float HitNorm[3] = { 0.0f, 0.0f, 0.0f };

	dtStatus Result = m_navQuery->findDistanceToWall(pBot->BotNavInfo.CurrentPoly, Pos, MaxRadius, m_navFilter, &HitDist, HitPos, HitNorm);

	if (dtStatusSucceed(Result) && HitDist > 0.0f)
	{
		Vector HitResult = Vector(HitPos[0], -HitPos[2], HitPos[1]);
		return HitResult;
	}

	return g_vecZero;
}

Vector UTIL_GetNearestPointOnNavWall(const nav_profile &NavProfile, const Vector Location, const float MaxRadius)
{

	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	// Invalid nav profile
	if (!m_navQuery) { return g_vecZero; }

	dtPolyRef StartPoly = UTIL_GetNearestPolyRefForLocation(NavProfile, Location);

	// Not on the nav mesh
	if (StartPoly == 0) { return Location; }

	float Pos[3] = { Location.x, Location.z, -Location.y };

	float HitDist = 0.0f;
	float HitPos[3] = { 0.0f, 0.0f, 0.0f };
	float HitNorm[3] = { 0.0f, 0.0f, 0.0f };

	dtStatus Result = m_navQuery->findDistanceToWall(StartPoly, Pos, MaxRadius, m_navFilter, &HitDist, HitPos, HitNorm);

	// We hit something
	if (dtStatusSucceed(Result) && HitDist < MaxRadius)
	{
		Vector HitResult = Vector(HitPos[0], -HitPos[2], HitPos[1]);
		return HitResult;
	}

	// Didn't hit anything
	return g_vecZero;
}

unsigned int UTIL_AddTemporaryObstacle(unsigned int NavMeshIndex, const Vector Location, float Radius, float Height, int area)
{
	unsigned int ObstacleNum = 0;
		
	if (NavMeshes[NavMeshIndex].tileCache)
	{
		float Pos[3] = { Location.x, Location.z - (Height * 0.5f), -Location.y };

		dtObstacleRef ObsRef = 0;
		NavMeshes[NavMeshIndex].tileCache->addObstacle(Pos, Radius, Height, area, &ObsRef);

		ObstacleNum = (unsigned int)ObsRef;

		if (ObstacleNum > 0 && NavMeshIndex != BUILDING_NAV_MESH)
		{
			bNavMeshModified = true;
		}
	}

	return ObstacleNum;
}

void UTIL_AddStructureTemporaryObstacles(AvHAIBuildableStructure* Structure)
{
	if (Structure->StructureType == STRUCTURE_MARINE_DEPLOYEDMINE) { return; }

	bool bCollideWithPlayers = UTIL_ShouldStructureCollide(Structure->StructureType);

	float Radius = UTIL_GetStructureRadiusForObstruction(Structure->StructureType);

	// Not all structures collide with players (e.g. phase gate)
	if (bCollideWithPlayers)
	{
		unsigned int area = UTIL_GetAreaForObstruction(Structure->StructureType, Structure->edict);
		
		// We add an obstacle for the building nav mesh below
		for (int i = 0; i < BUILDING_NAV_MESH; i++)
		{
			unsigned int NewObstacleRef = UTIL_AddTemporaryObstacle(i, UTIL_GetCentreOfEntity(Structure->edict), Radius, 100.0f, area);

			if (NewObstacleRef > 0)
			{
				AvHAITempObstacle NewObstacle;
				NewObstacle.NavMeshIndex = i;
				NewObstacle.ObstacleRef = NewObstacleRef;

				Structure->Obstacles.push_back(NewObstacle);
			}
		}
	}

	// Always cut a hole in the building nav mesh so we don't try to place anything on top of this structure in future
	unsigned int NewObstacleRef = UTIL_AddTemporaryObstacle(BUILDING_NAV_MESH, UTIL_GetCentreOfEntity(Structure->edict), Radius * 1.1f, 100.0f, DT_TILECACHE_NULL_AREA);

	if (NewObstacleRef > 0)
	{
		AvHAITempObstacle NewObstacle;
		NewObstacle.NavMeshIndex = BUILDING_NAV_MESH;
		NewObstacle.ObstacleRef = NewObstacleRef;

		Structure->Obstacles.push_back(NewObstacle);
	}
	
}

void UTIL_RemoveStructureTemporaryObstacles(AvHAIBuildableStructure* Structure)
{
	for (auto it = Structure->Obstacles.begin(); it != Structure->Obstacles.end();)
	{
		int NavMeshIndex = it->NavMeshIndex;

		if (NavMeshes[NavMeshIndex].tileCache)
		{
			const dtTileCacheObstacle* ObstacleToRemove = NavMeshes[NavMeshIndex].tileCache->getObstacleByRef((dtObstacleRef)it->ObstacleRef);

			if (ObstacleToRemove)
			{
				dtStatus RemovalStatus = NavMeshes[NavMeshIndex].tileCache->removeObstacle((dtObstacleRef)it->ObstacleRef);
				
				if (dtStatusSucceed(RemovalStatus))
				{
					bNavMeshModified = true;
				}
			}

			
		}

		it = Structure->Obstacles.erase(it);
	}

}

void UTIL_AddTemporaryObstacles(const Vector Location, float Radius, float Height, int area, unsigned int* ObstacleRefArray)
{

	float Pos[3] = { Location.x, Location.z - (Height * 0.5f), -Location.y };

	for (int i = 0; i < MAX_NAV_MESHES; i++)
	{
		ObstacleRefArray[i] = 0;

		if (NavMeshes[i].tileCache)
		{
			dtObstacleRef ObsRef = 0;
			NavMeshes[i].tileCache->addObstacle(Pos, Radius, Height, area, &ObsRef);

			ObstacleRefArray[i] = (unsigned int)ObsRef;

			if ((unsigned int)ObsRef > 0)
			{
				bNavMeshModified = true;
			}
		}
	}
}

unsigned int UTIL_AddTemporaryBoxObstacle(const Vector bMin, const Vector bMax, int area)
{
	unsigned int ObstacleNum = 0;

	float bMinf[3] = { bMin.x, bMin.z, -bMin.y };
	float bMaxf[3] = { bMax.x, bMax.z, -bMax.y };

	for (int i = 0; i < MAX_NAV_MESHES; i++)
	{
		if (NavMeshes[i].tileCache)
		{
			dtObstacleRef ObsRef = 0;
			NavMeshes[i].tileCache->addBoxObstacle(bMinf, bMaxf, area, &ObsRef);

			ObstacleNum = (unsigned int)ObsRef;

			if (area == DT_TILECACHE_NULL_AREA || area == DT_TILECACHE_WELD_AREA)
			{
				bNavMeshModified = true;
			}

		}
	}

	return ObstacleNum;
}

void UTIL_RemoveTemporaryObstacle(unsigned int ObstacleRef)
{
	if (ObstacleRef == 0) { return; }

	for (int i = 0; i < MAX_NAV_MESHES; i++)
	{
		if (NavMeshes[i].tileCache)
		{
			const dtTileCacheObstacle* ObstacleToRemove = NavMeshes[i].tileCache->getObstacleByRef((dtObstacleRef)ObstacleRef);

			if (ObstacleToRemove && (ObstacleToRemove->cylinder.area == DT_TILECACHE_NULL_AREA || ObstacleToRemove->cylinder.area == DT_TILECACHE_WELD_AREA))
			{
				bNavMeshModified = true;
			}

			NavMeshes[i].tileCache->removeObstacle((dtObstacleRef)ObstacleRef);
		}
	}
}

void UTIL_RemoveTemporaryObstacles(unsigned int* ObstacleRefs)
{

	for (int i = 0; i < MAX_NAV_MESHES; i++)
	{
		if (NavMeshes[i].tileCache)
		{
			const dtTileCacheObstacle* ObstacleToRemove = NavMeshes[i].tileCache->getObstacleByRef((dtObstacleRef)ObstacleRefs[i]);

			if (ObstacleToRemove && (ObstacleToRemove->cylinder.area == DT_TILECACHE_NULL_AREA || ObstacleToRemove->cylinder.area == DT_TILECACHE_WELD_AREA))
			{
				bNavMeshModified = true;
			}

			NavMeshes[i].tileCache->removeObstacle((dtObstacleRef)ObstacleRefs[i]);

		}

		ObstacleRefs[i] = 0;
	}
}

void GetFullFilePath(char* buffer, const char* mapname)
{
	string theMapName = mapname;
	string navPath = string(getModDirectory()) + "/navmeshes/" + mapname + ".nav";
	strcpy(buffer, navPath.c_str());
}


void ReloadNavMeshes()
{
	vector<AvHAIPlayer*> AllBots = AIMGR_GetAllAIPlayers();

	for (auto it = AllBots.begin(); it != AllBots.end(); it++)
	{
		AvHAIPlayer* ThisPlayer = (*it);

		ClearBotMovement(ThisPlayer);
	}
	AITAC_ClearMapAIData(false);
	UnloadNavMeshes();
	bool bSuccess = LoadNavMesh(STRING(gpGlobals->mapname));

	if (bSuccess)
	{
		UTIL_PopulateDoors();
		UTIL_PopulateWeldableObstacles();

		UTIL_UpdateDoors(true);

		bool bTileCacheFullyUpdated = UTIL_UpdateTileCache();

		while (!bTileCacheFullyUpdated)
		{
			bTileCacheFullyUpdated = UTIL_UpdateTileCache();
		}
	}
}

void UnloadNavMeshes()
{
	for (int i = 0; i < MAX_NAV_MESHES; i++)
	{
		if (NavMeshes[i].navMesh)
		{
			dtFreeNavMesh(NavMeshes[i].navMesh);
			NavMeshes[i].navMesh = nullptr;
		}

		if (NavMeshes[i].navQuery)
		{
			dtFreeNavMeshQuery(NavMeshes[i].navQuery);
			NavMeshes[i].navQuery = nullptr;
		}

		if (NavMeshes[i].tileCache)
		{
			dtFreeTileCache(NavMeshes[i].tileCache);
			NavMeshes[i].tileCache = nullptr;
		}
	}

	BaseMapConnections.clear();
	MapNavHints.clear();

	NavmeshStatus = NAVMESH_STATUS_PENDING;
}

void UnloadNavigationData()
{
	UnloadNavMeshes();

	UTIL_ClearDoorData();
	UTIL_ClearWeldablesData();

	memset(BaseNavProfiles, 0, sizeof(nav_profile));

	AIMGR_ClearBotData();

}

bool LoadNavMesh(const char* mapname)
{
	memset(NavMeshes, 0, sizeof(NavMeshes));
	BaseMapConnections.clear();
	MapNavHints.clear();

	char filename[256]; // Full path to BSP file

	GetFullFilePath(filename, mapname);

	FILE* savedFile = fopen(filename, "rb");

	if (!savedFile) 
	{ 
		char ErrMsg[256];
		sprintf(ErrMsg, "No nav file found for %s in the navmeshes folder\n", mapname);
		g_engfuncs.pfnServerPrint(ErrMsg);
		g_engfuncs.pfnServerPrint("You will need to create one using the Nav Editor tool in the navmeshes folder, or download one\n");
		return false; 
	}

	LinearAllocator* m_talloc = new LinearAllocator(32000);
	FastLZCompressor* m_tcomp = new FastLZCompressor;
	MeshProcess* m_tmproc = new MeshProcess;

	// Read header.
	TileCacheBuildHeader header;
	size_t headerReadReturnCode = fread(&header, sizeof(TileCacheBuildHeader), 1, savedFile);
	if (headerReadReturnCode != 1)
	{
		// Error or early EOF
		fclose(savedFile);
		UnloadNavigationData();
		char ErrMsg[256];
		sprintf(ErrMsg, "The nav file found for %s is a different version to the current bot version. Use the Nav Editor to regenerate it\n", mapname);
		g_engfuncs.pfnServerPrint(ErrMsg);
		return false;
	}

	if (header.magic != TILECACHESET_MAGIC || header.version != TILECACHESET_VERSION)
	{
		fclose(savedFile);
		UnloadNavigationData();
		char ErrMsg[256];
		sprintf(ErrMsg, "The nav file found for %s is a different version to the current bot version. Use the Nav Editor to regenerate it\n", mapname);
		g_engfuncs.pfnServerPrint(ErrMsg);
		return false;
	}

	dtNavMeshParams* NavMeshParams[3] = { &header.regularMeshParams, &header.onosMeshParams, &header.buildingMeshParams };
	dtTileCacheParams* TileCacheParams[3] = { &header.regularCacheParams, &header.onosCacheParams, &header.buildingCacheParams };

	for (int i = 0; i <= BUILDING_NAV_MESH; i++)
	{
		NavMeshes[i].navMesh = dtAllocNavMesh();

		if (!NavMeshes[i].navMesh)
		{
			fclose(savedFile);
			UnloadNavigationData();
			g_engfuncs.pfnServerPrint("Unable to allocate memory for the nav mesh\n");
			return false;
		}

		dtStatus status = NavMeshes[i].navMesh->init(NavMeshParams[i]);
		if (dtStatusFailed(status))
		{
			fclose(savedFile);
			UnloadNavigationData();
			g_engfuncs.pfnServerPrint("The nav file has been corrupted or is out of date. Use the Nav Editor to regenerate it\n");
			return false;
		}

		NavMeshes[i].tileCache = dtAllocTileCache();
		if (!NavMeshes[i].tileCache)
		{
			fclose(savedFile);
			UnloadNavigationData();
			return false;
		}

		status = NavMeshes[i].tileCache->init(TileCacheParams[i], m_talloc, m_tcomp, m_tmproc);
		if (dtStatusFailed(status))
		{
			g_engfuncs.pfnServerPrint("The nav file has been corrupted or is out of date. Use the Nav Editor to regenerate it\n");
			fclose(savedFile);
			UnloadNavigationData();
			return false;
		}
	}

	// Read tiles.
	for (int i = 0; i < header.numRegularTiles; ++i)
	{
		TileCacheTileHeader tileHeader;
		size_t tileHeaderReadReturnCode = fread(&tileHeader, sizeof(tileHeader), 1, savedFile);
		if (tileHeaderReadReturnCode != 1)
		{
			fclose(savedFile);
			UnloadNavigationData();
			g_engfuncs.pfnServerPrint("The nav file has been corrupted or is out of date. Use the Nav Editor to regenerate it\n");
			return false;
		}
		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		size_t tileDataReadReturnCode = fread(data, tileHeader.dataSize, 1, savedFile);
		if (tileDataReadReturnCode != 1)
		{
			dtFree(data);
			fclose(savedFile);
			UnloadNavigationData();
			g_engfuncs.pfnServerPrint("The nav file has been corrupted or is out of date. Use the Nav Editor to regenerate it\n");
			return false;
		}

		dtCompressedTileRef tile = 0;
		dtStatus addTileStatus = NavMeshes[REGULAR_NAV_MESH].tileCache->addTile(data, tileHeader.dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tile);
		if (dtStatusFailed(addTileStatus))
		{
			dtFree(data);
		}

		if (tile)
			NavMeshes[REGULAR_NAV_MESH].tileCache->buildNavMeshTile(tile, NavMeshes[REGULAR_NAV_MESH].navMesh);
	}

	for (int i = 0; i < header.numOnosTiles; ++i)
	{
		TileCacheTileHeader tileHeader;
		size_t tileHeaderReadReturnCode = fread(&tileHeader, sizeof(tileHeader), 1, savedFile);
		if (tileHeaderReadReturnCode != 1)
		{
			fclose(savedFile);
			UnloadNavigationData();
			g_engfuncs.pfnServerPrint("The nav file has been corrupted or is out of date. Use the Nav Editor to regenerate it\n");
			return false;
		}
		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		size_t tileDataReadReturnCode = fread(data, tileHeader.dataSize, 1, savedFile);
		if (tileDataReadReturnCode != 1)
		{
			dtFree(data);
			fclose(savedFile);
			UnloadNavigationData();
			g_engfuncs.pfnServerPrint("The nav file has been corrupted or is out of date. Use the Nav Editor to regenerate it\n");
			return false;
		}

		dtCompressedTileRef tile = 0;
		dtStatus addTileStatus = NavMeshes[ONOS_NAV_MESH].tileCache->addTile(data, tileHeader.dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tile);
		if (dtStatusFailed(addTileStatus))
		{
			dtFree(data);
		}

		if (tile)
			NavMeshes[ONOS_NAV_MESH].tileCache->buildNavMeshTile(tile, NavMeshes[ONOS_NAV_MESH].navMesh);
	}

	for (int i = 0; i < header.numBuildingTiles; ++i)
	{
		TileCacheTileHeader tileHeader;
		size_t tileHeaderReadReturnCode = fread(&tileHeader, sizeof(tileHeader), 1, savedFile);
		if (tileHeaderReadReturnCode != 1)
		{
			fclose(savedFile);
			UnloadNavigationData();
			g_engfuncs.pfnServerPrint("The nav file has been corrupted or is out of date. Use the Nav Editor to regenerate it\n");
			return false;
		}
		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		size_t tileDataReadReturnCode = fread(data, tileHeader.dataSize, 1, savedFile);
		if (tileDataReadReturnCode != 1)
		{
			dtFree(data);
			fclose(savedFile);
			UnloadNavigationData();
			g_engfuncs.pfnServerPrint("The nav file has been corrupted or is out of date. Use the Nav Editor to regenerate it\n");
			return false;
		}

		dtCompressedTileRef tile = 0;
		dtStatus addTileStatus = NavMeshes[BUILDING_NAV_MESH].tileCache->addTile(data, tileHeader.dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tile);
		if (dtStatusFailed(addTileStatus))
		{
			dtFree(data);
		}

		if (tile)
			NavMeshes[BUILDING_NAV_MESH].tileCache->buildNavMeshTile(tile, NavMeshes[BUILDING_NAV_MESH].navMesh);
	}

	fseek(savedFile, header.OffMeshConsOffset, SEEK_SET);

	for (int i = 0; i < header.NumOffMeshCons; i++)
	{
		dtOffMeshConnection def;

		fread(&def, sizeof(dtOffMeshConnection), 1, savedFile);

		unsigned char area = def.area;

		if (def.flags & SAMPLE_POLYFLAGS_WALLCLIMB)
		{
			area = SAMPLE_POLYAREA_WALLCLIMB;
		}

		if (def.flags & SAMPLE_POLYFLAGS_LADDER)
		{
			area = SAMPLE_POLYAREA_LADDER;
		}

		if (def.flags & SAMPLE_POLYFLAGS_LIFT)
		{
			area = SAMPLE_POLYAREA_LIFT;
		}

		AvHAIOffMeshConnection NewMapConnection;
		NewMapConnection.ConnectionFlags = def.flags;
		NewMapConnection.DefaultConnectionFlags = def.flags;
		NewMapConnection.TargetObject = nullptr;
		NewMapConnection.FromLocation = Vector(def.pos[0], -def.pos[2], def.pos[1]);
		NewMapConnection.ToLocation = Vector(def.pos[3], -def.pos[5], def.pos[4]);
		NewMapConnection.bBiDirectional = def.bBiDir;

		for (int ii = 0; ii < BUILDING_NAV_MESH; ii++)
		{
			dtOffMeshConnectionRef ref = 0;

			NavMeshes[ii].tileCache->addOffMeshConnection(&def.pos[0], &def.pos[3], 10.0f, area, def.flags, def.bBiDir, &ref);

			NewMapConnection.ConnectionRefs[ii] = (unsigned int)ref;
		}

		BaseMapConnections.push_back(NewMapConnection);
	}

	fseek(savedFile, header.NavHintOffset, SEEK_SET);

	for (int i = 0; i < header.NumNavHints; i++)
	{
		LoadNavHint LoadedHint;
		fread(&LoadedHint, sizeof(LoadNavHint), 1, savedFile);

		NavHint NewHint;
		NewHint.hintType = LoadedHint.hintType;
		NewHint.Position = Vector(LoadedHint.position[0], -LoadedHint.position[2], LoadedHint.position[1]);
		NewHint.OccupyingBuilding = nullptr;

		MapNavHints.push_back(NewHint);
	}

	fclose(savedFile);

	for (int i = 0; i <= BUILDING_NAV_MESH; i++)
	{
		NavMeshes[i].navQuery = dtAllocNavMeshQuery();

		dtStatus initStatus = NavMeshes[i].navQuery->init(NavMeshes[i].navMesh, 65535);

		if (dtStatusFailed(initStatus))
		{
			UnloadNavigationData();
			g_engfuncs.pfnServerPrint("The nav file has been corrupted or is out of date. Use the Nav Editor to regenerate it\n");
			return false;
		}
	}

	char SuccMsg[128];
	sprintf(SuccMsg, "Navigation data for %s loaded successfully\n", mapname);
	g_engfuncs.pfnServerPrint(SuccMsg);


	return true;
}

void OnOffMeshConnectionAdded(dtOffMeshConnection* NewConnection)
{
	for (int i = 0; i <= BUILDING_NAV_MESH; i++)
	{
		if (NavMeshes[i].navMesh && NavMeshes[i].tileCache)
		{
			NavMeshes[i].navMesh->LinkOffMeshConnectionToTiles(NewConnection);

			dtCompressedTile* ModifiedTile = NavMeshes[i].tileCache->getTileAt(NewConnection->FromTileX, NewConnection->FromTileY, NewConnection->FromTileLayer);
			NavMeshes[i].tileCache->buildNavMeshTile(NavMeshes[i].tileCache->getTileRef(ModifiedTile), NavMeshes[i].navMesh);
		}
	}
}

void UTIL_PopulateBaseNavProfiles()
{
	memset(BaseNavProfiles, 0, sizeof(BaseNavProfiles));

	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].NavMeshIndex = REGULAR_NAV_MESH;
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].bFlyingProfile = false;
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].ReachabilityFlag = AI_REACHABILITY_MARINE;
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_OBSTRUCTION, 2.0f);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 2.0f);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_BLOCKED, 2.0f);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_STRUCTUREBLOCK, 20.0f);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_FALLDAMAGE, 10.0f);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LADDER, 1.5f);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LIFT, 3.0f);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setIncludeFlags(SAMPLE_POLYFLAGS_ALL);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_FLY | SAMPLE_POLYFLAGS_WALLCLIMB | SAMPLE_POLYFLAGS_WELD);
	BaseNavProfiles[MARINE_BASE_NAV_PROFILE].Filters.setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);
	

	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].NavMeshIndex = REGULAR_NAV_MESH;
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].bFlyingProfile = false;
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].ReachabilityFlag = AI_REACHABILITY_SKULK;
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_OBSTRUCTION, 2.0f);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_BLOCKED, 2.0f);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_STRUCTUREBLOCK, 20.0f);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LADDER, 1.5f);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_WALLCLIMB, 1.0f);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LIFT, 3.0f);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setIncludeFlags(SAMPLE_POLYFLAGS_ALL);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_DUCKJUMP | SAMPLE_POLYFLAGS_WELD | SAMPLE_POLYFLAGS_FLY);
	BaseNavProfiles[SKULK_BASE_NAV_PROFILE].Filters.addExcludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_DUCKJUMP | SAMPLE_POLYFLAGS_WELD | SAMPLE_POLYFLAGS_FLY);
	


	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].NavMeshIndex = REGULAR_NAV_MESH;
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].bFlyingProfile = false;
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].ReachabilityFlag = AI_REACHABILITY_GORGE;
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_OBSTRUCTION, 2.0f);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_BLOCKED, 2.0f);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_FALLDAMAGE, 10.0f);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_STRUCTUREBLOCK, 20.0f);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LADDER, 1.5f);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LIFT, 3.0f);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setIncludeFlags(SAMPLE_POLYFLAGS_ALL);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_DUCKJUMP | SAMPLE_POLYFLAGS_WELD | SAMPLE_POLYFLAGS_FLY | SAMPLE_POLYFLAGS_WALLCLIMB);
	BaseNavProfiles[GORGE_BASE_NAV_PROFILE].Filters.addExcludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_DUCKJUMP | SAMPLE_POLYFLAGS_WELD | SAMPLE_POLYFLAGS_FLY | SAMPLE_POLYFLAGS_WALLCLIMB);

	BaseNavProfiles[LERK_BASE_NAV_PROFILE].NavMeshIndex = REGULAR_NAV_MESH;
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].bFlyingProfile = true;
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].ReachabilityFlag = AI_REACHABILITY_SKULK;
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_OBSTRUCTION, 2.0f);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_BLOCKED, 2.0f);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_STRUCTUREBLOCK, 20.0f);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LADDER, 1.0f);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LIFT, 3.0f);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.setIncludeFlags(SAMPLE_POLYFLAGS_ALL);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_WELD);
	BaseNavProfiles[LERK_BASE_NAV_PROFILE].Filters.addExcludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_WELD);

	BaseNavProfiles[FADE_BASE_NAV_PROFILE].NavMeshIndex = REGULAR_NAV_MESH;
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].bFlyingProfile = false;
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].ReachabilityFlag = AI_REACHABILITY_SKULK;
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_OBSTRUCTION, 2.0f);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.5f);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_BLOCKED, 2.0f);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_STRUCTUREBLOCK, 20.0f);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LADDER, 1.5f);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LIFT, 3.0f);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setIncludeFlags(SAMPLE_POLYFLAGS_ALL);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_WELD | SAMPLE_POLYFLAGS_FLY);
	BaseNavProfiles[FADE_BASE_NAV_PROFILE].Filters.addExcludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_WELD | SAMPLE_POLYFLAGS_FLY);

	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].NavMeshIndex = ONOS_NAV_MESH;
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].bFlyingProfile = false;
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].ReachabilityFlag = AI_REACHABILITY_ONOS;
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_OBSTRUCTION, 2.0f);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 2.0f);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_FALLDAMAGE, 10.0f);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_BLOCKED, 2.0f);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_STRUCTUREBLOCK, 5.0f); // Onos is a wrecking machine, structures shouldn't be such an obstacle for them!
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LADDER, 1.5f);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setAreaCost(SAMPLE_POLYAREA_LIFT, 3.0f);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setIncludeFlags(SAMPLE_POLYFLAGS_ALL);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_DUCKJUMP | SAMPLE_POLYFLAGS_WELD | SAMPLE_POLYFLAGS_FLY | SAMPLE_POLYFLAGS_WALLCLIMB | SAMPLE_POLYFLAGS_NOONOS);
	BaseNavProfiles[ONOS_BASE_NAV_PROFILE].Filters.addExcludeFlags(SAMPLE_POLYFLAGS_TEAM1PHASEGATE | SAMPLE_POLYFLAGS_TEAM2PHASEGATE | SAMPLE_POLYFLAGS_DUCKJUMP | SAMPLE_POLYFLAGS_WELD | SAMPLE_POLYFLAGS_FLY | SAMPLE_POLYFLAGS_WALLCLIMB | SAMPLE_POLYFLAGS_NOONOS);

	BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE].NavMeshIndex = BUILDING_NAV_MESH;
	BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE].Filters.setIncludeFlags(SAMPLE_POLYFLAGS_ALL);
	BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE].Filters.setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);
	BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE].bFlyingProfile = false;
	BaseNavProfiles[STRUCTURE_BASE_NAV_PROFILE].ReachabilityFlag = AI_REACHABILITY_MARINE;

	BaseNavProfiles[ALL_NAV_PROFILE].NavMeshIndex = REGULAR_NAV_MESH;
	BaseNavProfiles[ALL_NAV_PROFILE].Filters.setIncludeFlags(SAMPLE_POLYFLAGS_ALL);
	BaseNavProfiles[ALL_NAV_PROFILE].Filters.setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);
	BaseNavProfiles[ALL_NAV_PROFILE].bFlyingProfile = false;
	BaseNavProfiles[ALL_NAV_PROFILE].ReachabilityFlag = AI_REACHABILITY_SKULK;
}

bool loadNavigationData(const char* mapname)
{

	UnloadNavigationData();

	if (!LoadNavMesh(mapname))
	{
		NavmeshStatus = NAVMESH_STATUS_FAILED;
		return false;
	}

	NavmeshStatus = NAVMESH_STATUS_SUCCESS;
	
	UTIL_PopulateBaseNavProfiles();

	return true;
}

bool NavmeshLoaded()
{
	return NavMeshes[0].navMesh != nullptr;
}

AvHAINavMeshStatus NAV_GetNavMeshStatus()
{
	return NavmeshStatus;
}

Vector UTIL_GetRandomPointOnNavmesh(const AvHAIPlayer* pBot)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	if (!m_navQuery)
	{
		return g_vecZero;
	}

	Vector Result;

	dtPolyRef refPoly;

	float result[3];
	memset(result, 0, sizeof(result));

	dtStatus status = m_navQuery->findRandomPoint(m_navFilter, frand, &refPoly, result);

	if (dtStatusFailed(status))
	{
		return g_vecZero;
	}

	Result.x = result[0];
	Result.y = -result[2];
	Result.z = result[1];

	return Result;
}

Vector UTIL_GetRandomPointOnNavmeshInRadiusOfAreaType(SamplePolyFlags Flag, const Vector origin, const float MaxRadius)
{
	const dtNavMeshQuery* m_NavQuery = UTIL_GetNavMeshQueryForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);

	if (!m_NavQuery) { return g_vecZero; }

	dtQueryFilter filter;
	filter.setExcludeFlags(0);
	filter.setIncludeFlags(Flag);

	Vector Result = g_vecZero;

	float pCheckLoc[3] = { origin.x, origin.z, -origin.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus foundPolyResult = m_NavQuery->findNearestPoly(pCheckLoc, pExtents, &filter, &FoundPoly, NavNearest);

	if (dtStatusFailed(foundPolyResult))
	{
		return g_vecZero;
	}

	dtPolyRef RandomPoly;
	float RandomPoint[3];

	dtStatus foundRandomPointResult = m_NavQuery->findRandomPointAroundCircle(FoundPoly, NavNearest, MaxRadius, &filter, frand, &RandomPoly, RandomPoint);


	if (dtStatusFailed(foundRandomPointResult))
	{
		return g_vecZero;
	}

	Result.x = RandomPoint[0];
	Result.y = -RandomPoint[2];
	Result.z = RandomPoint[1];

	return Result;
}

Vector UTIL_GetRandomPointOnNavmeshInRadius(const nav_profile &NavProfile, const Vector origin, const float MaxRadius)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return g_vecZero; }

	Vector Result = g_vecZero;

	float pCheckLoc[3] = { origin.x, origin.z, -origin.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus foundPolyResult = m_navQuery->findNearestPoly(pCheckLoc, pExtents, m_navFilter, &FoundPoly, NavNearest);

	if (dtStatusFailed(foundPolyResult))
	{
		return g_vecZero;
	}

	dtPolyRef RandomPoly;
	float RandomPoint[3];

	dtStatus foundRandomPointResult = m_navQuery->findRandomPointAroundCircle(FoundPoly, NavNearest, MaxRadius, m_navFilter, frand, &RandomPoly, RandomPoint);


	if (dtStatusFailed(foundRandomPointResult))
	{
		return g_vecZero;
	}

	Result.x = RandomPoint[0];
	Result.y = -RandomPoint[2];
	Result.z = RandomPoint[1];

	return Result;
}

Vector UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(const nav_profile& NavProfile, const Vector origin, const float MaxRadius)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return g_vecZero; }

	Vector Result = g_vecZero;

	float pCheckLoc[3] = { origin.x, origin.z, -origin.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus foundPolyResult = m_navQuery->findNearestPoly(pCheckLoc, pExtents, m_navFilter, &FoundPoly, NavNearest);

	if (dtStatusFailed(foundPolyResult))
	{
		return g_vecZero;
	}

	dtPolyRef RandomPoly;
	float RandomPoint[3];

	dtStatus foundRandomPointResult = m_navQuery->findRandomPointAroundCircleIgnoreReachability(FoundPoly, NavNearest, MaxRadius, m_navFilter, frand, &RandomPoly, RandomPoint);


	if (dtStatusFailed(foundRandomPointResult))
	{
		return g_vecZero;
	}

	Result.x = RandomPoint[0];
	Result.y = -RandomPoint[2];
	Result.z = RandomPoint[1];

	return Result;
}

Vector UTIL_GetRandomPointOnNavmeshInDonut(const nav_profile& NavProfile, const Vector origin, const float MinRadius, const float MaxRadius)
{
	int maxIterations = 0;
	float MinRadiusSq = sqrf(MinRadius);

	while (maxIterations < 100)
	{
		Vector StartPoint = UTIL_GetRandomPointOnNavmeshInRadius(NavProfile, origin, MaxRadius);

		if (vDist2DSq(StartPoint, origin) > MinRadiusSq)
		{
			return StartPoint;
		}

		maxIterations++;
	}

	return g_vecZero;
}

Vector UTIL_GetRandomPointOnNavmeshInDonutIgnoreReachability(const nav_profile& NavProfile, const Vector origin, const float MinRadius, const float MaxRadius)
{
	int maxIterations = 0;
	float MinRadiusSq = sqrf(MinRadius);

	while (maxIterations < 100)
	{
		Vector StartPoint = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(NavProfile, origin, MaxRadius);

		if (vDist2DSq(StartPoint, origin) > MinRadiusSq)
		{
			return StartPoint;
		}

		maxIterations++;
	}

	return g_vecZero;
}

static float frand()
{
	return (float)rand() / (float)RAND_MAX;
}

Vector AdjustPointForPathfinding(const Vector Point)
{
	Vector ProjectedPoint = UTIL_ProjectPointToNavmesh(Point);

	int PointContents = UTIL_PointContents(ProjectedPoint);

	if (PointContents == CONTENTS_SOLID)
	{
		int PointContents = UTIL_PointContents(ProjectedPoint + Vector(0.0f, 0.0f, 32.0f));

		if (PointContents != CONTENTS_SOLID && PointContents != CONTENTS_LADDER)
		{
			Vector TraceStart = ProjectedPoint + Vector(0.0f, 0.0f, 32.0f);
			Vector TraceEnd = TraceStart - Vector(0.0f, 0.0f, 50.0f);
			Vector NewPoint = UTIL_GetHullTraceHitLocation(TraceStart, TraceEnd, point_hull);

			if (!vIsZero(NewPoint)) { return NewPoint; }
		}
	}
	else
	{
		Vector TraceStart = ProjectedPoint + Vector(0.0f, 0.0f, 5.0f);
		Vector TraceEnd = TraceStart - Vector(0.0f, 0.0f, 32.0f);
		Vector NewPoint = UTIL_GetHullTraceHitLocation(TraceStart, TraceEnd, point_hull);

		if (!vIsZero(NewPoint)) { return NewPoint; }
	}

	return ProjectedPoint;

}

Vector AdjustPointForPathfinding(const Vector Point, const nav_profile& NavProfile)
{
	Vector ProjectedPoint = UTIL_ProjectPointToNavmesh(Point, Vector(400.0f, 100.0f, 400.0f), NavProfile);

	int PointContents = UTIL_PointContents(ProjectedPoint);

	if (PointContents == CONTENTS_SOLID)
	{
		int PointContents = UTIL_PointContents(ProjectedPoint + Vector(0.0f, 0.0f, 32.0f));

		if (PointContents != CONTENTS_SOLID && PointContents != CONTENTS_LADDER)
		{
			Vector TraceStart = ProjectedPoint + Vector(0.0f, 0.0f, 32.0f);
			Vector TraceEnd = TraceStart - Vector(0.0f, 0.0f, 50.0f);
			Vector NewPoint = UTIL_GetHullTraceHitLocation(TraceStart, TraceEnd, point_hull);

			if (!vIsZero(NewPoint)) { return NewPoint; }
		}
	}
	else
	{
		Vector TraceStart = ProjectedPoint + Vector(0.0f, 0.0f, 5.0f);
		Vector TraceEnd = TraceStart - Vector(0.0f, 0.0f, 32.0f);
		Vector NewPoint = UTIL_GetHullTraceHitLocation(TraceStart, TraceEnd, point_hull);

		if (!vIsZero(NewPoint)) { return NewPoint; }
	}

	return ProjectedPoint;
}

// Special path finding that takes flight movement into account
dtStatus FindFlightPathToPoint(const nav_profile &NavProfile, Vector FromLocation, Vector ToLocation, vector<bot_path_node>& path, float MaxAcceptableDistance)
{
	TraceResult directHit;

	if (UTIL_QuickHullTrace(nullptr, FromLocation, ToLocation, head_hull, false))
	{
		path.clear();

		bot_path_node NewPathNode;
		NewPathNode.FromLocation = FromLocation;
		NewPathNode.Location = ToLocation;
		NewPathNode.area = SAMPLE_POLYAREA_GROUND;
		NewPathNode.flag = SAMPLE_POLYFLAGS_WALK;
		NewPathNode.poly = 0;
		NewPathNode.requiredZ = ToLocation.z;

		path.push_back(NewPathNode);

		return DT_SUCCESS;
	}

	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery || !m_navMesh || !m_navFilter || vIsZero(FromLocation) || vIsZero(ToLocation))
	{
		return DT_FAILURE;
	}

	vector<bot_path_node> BaseFlightPath;

	Vector FromFloorLocation = AdjustPointForPathfinding(FromLocation);
	Vector ToFloorLocation = AdjustPointForPathfinding(ToLocation);

	float pStartPos[3] = { FromFloorLocation.x, FromFloorLocation.z, -FromFloorLocation.y };
	float pEndPos[3] = { ToFloorLocation.x, ToFloorLocation.z, -ToFloorLocation.y };

	dtStatus status;
	dtPolyRef StartPoly = 0;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	dtPolyRef EndPoly = 0;
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };
	dtPolyRef PolyPath[MAX_PATH_POLY];
	dtPolyRef StraightPolyPath[MAX_AI_PATH_SIZE];
	int nPathCount = 0;
	float StraightPath[MAX_AI_PATH_SIZE * 3];
	unsigned char straightPathFlags[MAX_AI_PATH_SIZE];
	memset(straightPathFlags, 0, sizeof(straightPathFlags));
	int nVertCount = 0;

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, pExtents, m_navFilter, &StartPoly, StartNearest);
	if (!StartPoly || (status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		//BotSay(pBot, "findNearestPoly start failed!");
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, pExtents, m_navFilter, &EndPoly, EndNearest);
	if (!EndPoly || (status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		//BotSay(pBot, "findNearestPoly end failed!");
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (nPathCount == 0) { return DT_FAILURE; }

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[nPathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(MaxAcceptableDistance))
		{
			return DT_FAILURE;
		}
		else
		{
			dtVcopy(EndNearest, epos);
		}
	}

	status = m_navQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, straightPathFlags, StraightPolyPath, &nVertCount, MAX_AI_PATH_SIZE, DT_STRAIGHTPATH_ALL_CROSSINGS);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't create a path
	}

	if (nVertCount == 0)
	{
		return DT_FAILURE; // couldn't find a path
	}

	BaseFlightPath.clear();

	//vector<bot_path_node> InitialPath;
	//InitialPath.clear();

	unsigned char CurrArea;
	unsigned char ThisArea;

	unsigned int CurrFlags;
	unsigned int ThisFlags;

	m_navMesh->getPolyArea(StraightPolyPath[0], &CurrArea);
	m_navMesh->getPolyFlags(StraightPolyPath[0], &CurrFlags);

	CurrFlags &= ~(SAMPLE_POLYFLAGS_NOONOS);

	// At this point we have our path.  Copy it to the path store
	int nIndex = 0;
	TraceResult hit;
	Vector TraceStart;

	for (int nVert = 0; nVert < nVertCount; nVert++)
	{
		Vector NextPathPoint = g_vecZero;
		Vector PrevPoint = (BaseFlightPath.size() > 0) ? BaseFlightPath.back().Location : FromLocation;

		// The path point output by Detour uses the OpenGL, right-handed coordinate system. Convert to Goldsrc coordinates
		NextPathPoint.x = StraightPath[nIndex++];
		NextPathPoint.z = StraightPath[nIndex++];
		NextPathPoint.y = -StraightPath[nIndex++];		

		m_navMesh->getPolyArea(StraightPolyPath[nVert], &ThisArea);
		m_navMesh->getPolyFlags(StraightPolyPath[nVert], &ThisFlags);

		ThisFlags &= ~(SAMPLE_POLYFLAGS_NOONOS);

		if (ThisArea == SAMPLE_POLYAREA_GROUND || ThisArea == SAMPLE_POLYAREA_CROUCH)
		{
			NextPathPoint = UTIL_AdjustPointAwayFromNavWall(NextPathPoint, 16.0f);
		}

		AdjustPointForPathfinding(NextPathPoint);

		NextPathPoint.z += 20.0f;

		float NewRequiredZ = NextPathPoint.z;

		bot_path_node NextPathNode;

		NextPathNode.flag = CurrFlags;
		
		NextPathNode.area = CurrArea;
		NextPathNode.poly = StraightPolyPath[nVert];


		if (CurrFlags == SAMPLE_POLYFLAGS_WALLCLIMB || CurrFlags == SAMPLE_POLYFLAGS_FLY)
		{
			float MaxHeight = UTIL_FindZHeightForWallClimb(PrevPoint, NextPathPoint, head_hull);

			Vector PotentialNextPoint = PrevPoint + (UTIL_GetVectorNormal2D(NextPathPoint - PrevPoint) * 2.0f);;
			PotentialNextPoint.z = MaxHeight;

			if (UTIL_QuickTrace(nullptr, PrevPoint, PotentialNextPoint))
			{
				NextPathNode.requiredZ = MaxHeight;
				NextPathNode.Location = PotentialNextPoint;
				NextPathNode.FromLocation = PrevPoint;

				PrevPoint = NextPathNode.Location;

				BaseFlightPath.push_back(NextPathNode);

			}

			NextPathNode.requiredZ = MaxHeight;
			NextPathNode.Location = NextPathPoint - (UTIL_GetVectorNormal2D(NextPathPoint - PrevPoint) * 2.0f);
			NextPathNode.Location.z = MaxHeight;
			NextPathNode.FromLocation = PrevPoint;

			PrevPoint = NextPathNode.Location;

			BaseFlightPath.push_back(NextPathNode);
		}
		else if (CurrFlags == SAMPLE_POLYFLAGS_JUMP)
		{
			Vector MoveDir = UTIL_GetVectorNormal2D(NextPathPoint - PrevPoint);

			float MaxHeight = (PrevPoint.z > NextPathPoint.z) ? UTIL_FindZHeightForWallClimb(NextPathPoint, PrevPoint, head_hull) : UTIL_FindZHeightForWallClimb(PrevPoint, NextPathPoint, head_hull);

			NextPathNode.requiredZ = MaxHeight;
			NextPathNode.Location = PrevPoint + (MoveDir * 4.0f);
			NextPathNode.Location.z = MaxHeight;
			NextPathNode.FromLocation = PrevPoint;

			PrevPoint = NextPathNode.Location;

			BaseFlightPath.push_back(NextPathNode);

			NextPathNode.requiredZ = MaxHeight;
			NextPathNode.Location = NextPathPoint - (MoveDir * 4.0f);
			NextPathNode.Location.z = MaxHeight;
			NextPathNode.FromLocation = PrevPoint;

			PrevPoint = NextPathNode.Location;

			BaseFlightPath.push_back(NextPathNode);

			NextPathNode.requiredZ = NextPathPoint.z;
			NextPathNode.Location = NextPathPoint;
			NextPathNode.FromLocation = PrevPoint;

			PrevPoint = NextPathNode.Location;

			BaseFlightPath.push_back(NextPathNode);
		}
		else if (CurrFlags == SAMPLE_POLYFLAGS_FALL)
		{
			Vector MoveDir = UTIL_GetVectorNormal2D(NextPathPoint - PrevPoint);

			float MaxHeight = fmaxf(PrevPoint.z, NextPathPoint.z);

			NextPathNode.requiredZ = MaxHeight;
			NextPathNode.Location = NextPathPoint - (MoveDir * 8.0f);
			NextPathNode.Location.z = PrevPoint.z;
			NextPathNode.FromLocation = PrevPoint;

			PrevPoint = NextPathNode.Location;

			BaseFlightPath.push_back(NextPathNode);
		}
		else if (CurrFlags == SAMPLE_POLYFLAGS_LADDER)
		{
			float MaxHeight = fmaxf(PrevPoint.z, NextPathPoint.z);

			NextPathNode.requiredZ = MaxHeight;
			NextPathNode.Location = (PrevPoint.z < NextPathPoint.z) ? (PrevPoint + (UTIL_GetVectorNormal2D(NextPathPoint - PrevPoint) * 2.0f)) : NextPathPoint;
			NextPathNode.Location.z = MaxHeight;
			NextPathNode.FromLocation = PrevPoint;

			PrevPoint = NextPathNode.Location;

			BaseFlightPath.push_back(NextPathNode);
		}

		NextPathNode.requiredZ = NextPathPoint.z;
		NextPathNode.Location = NextPathPoint;
		NextPathNode.FromLocation = PrevPoint;

		BaseFlightPath.push_back(NextPathNode);

		CurrArea = ThisArea;
		CurrFlags = ThisFlags;

	}

	bot_path_node FinalInitialPathNode;
	FinalInitialPathNode.FromLocation = (path.size() > 0) ? path.back().Location : FromLocation;
	FinalInitialPathNode.Location = ToLocation;
	FinalInitialPathNode.area = SAMPLE_POLYAREA_GROUND;
	FinalInitialPathNode.flag = SAMPLE_POLYFLAGS_WALLCLIMB;
	FinalInitialPathNode.poly = 0;
	FinalInitialPathNode.requiredZ = ToLocation.z;

	BaseFlightPath.push_back(FinalInitialPathNode);

	path.clear();

	RefineFlightPath(BaseFlightPath, path);

	return DT_SUCCESS;
}

Vector UTIL_FindHighestSuccessfulTracePoint(const Vector TraceFrom, const Vector TargetPoint, const Vector NextPoint, const float IterationStep, const float MinIdealHeight, const float MaxHeight)
{
	Vector OriginTrace = TraceFrom;
	float AddedHeight = 0.0f;

	bool bFoundInitialPoint = false;
	Vector CurrentHighest = ZERO_VECTOR;

	int NumIterations = (int)ceilf(MaxHeight / IterationStep);

	Vector CurrentTarget = TargetPoint;

	for (int i = 0; i <= NumIterations; i++)
	{
		if (!UTIL_QuickTrace(nullptr, TargetPoint, CurrentTarget)) { return CurrentHighest; }

		if (!UTIL_QuickHullTrace(nullptr, OriginTrace, CurrentTarget, head_hull, false))
		{
			if (bFoundInitialPoint) { break; }
		}
		else
		{
			bFoundInitialPoint = true;
			if (AddedHeight >= MinIdealHeight)
			{
				return CurrentTarget;
			}
			else
			{
				if (!vIsZero(NextPoint) && UTIL_QuickHullTrace(nullptr, CurrentTarget, NextPoint, head_hull, false))
				{
					CurrentHighest = CurrentTarget;
				}

			}

		}

		CurrentTarget.z += IterationStep;
		AddedHeight += IterationStep;
	}

	return CurrentHighest;
}

dtStatus FindPathClosestToPoint(const nav_profile& NavProfile, const Vector FromLocation, const Vector ToLocation, vector<bot_path_node>& path, float MaxAcceptableDistance)
{
	if (NavProfile.bFlyingProfile)
	{
		return FindFlightPathToPoint(NavProfile, FromLocation, ToLocation, path, MaxAcceptableDistance);
	}

	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery || !m_navMesh || !m_navFilter || vIsZero(FromLocation) || vIsZero(ToLocation))
	{
		return DT_FAILURE;
	}

	Vector FromFloorLocation = AdjustPointForPathfinding(FromLocation);
	Vector ToFloorLocation = AdjustPointForPathfinding(ToLocation);

	float pStartPos[3] = { FromFloorLocation.x, FromFloorLocation.z, -FromFloorLocation.y };
	float pEndPos[3] = { ToFloorLocation.x, ToFloorLocation.z, -ToFloorLocation.y };

	dtStatus status;
	dtPolyRef StartPoly;
	float StartNearest[3];
	dtPolyRef EndPoly;
	float EndNearest[3];
	dtPolyRef PolyPath[MAX_PATH_POLY];
	dtPolyRef StraightPolyPath[MAX_AI_PATH_SIZE];
	int nPathCount = 0;
	float StraightPath[MAX_AI_PATH_SIZE * 3];
	unsigned char straightPathFlags[MAX_AI_PATH_SIZE];
	memset(straightPathFlags, 0, sizeof(straightPathFlags));
	int nVertCount = 0;

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, pExtents, m_navFilter, &StartPoly, StartNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, pExtents, m_navFilter, &EndPoly, EndNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[nPathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(MaxAcceptableDistance))
		{
			return DT_FAILURE;
		}
		else
		{
			dtVcopy(EndNearest, epos);
		}
	}

	status = m_navQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, straightPathFlags, StraightPolyPath, &nVertCount, MAX_AI_PATH_SIZE, DT_STRAIGHTPATH_AREA_CROSSINGS);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't create a path
	}

	if (nVertCount == 0)
	{
		return DT_FAILURE; // couldn't find a path
	}

	path.clear();

	unsigned int CurrFlags;
	unsigned char CurrArea;
	unsigned char ThisArea;
	unsigned int ThisFlags;

	m_navMesh->getPolyFlags(StraightPolyPath[0], &CurrFlags);
	m_navMesh->getPolyArea(StraightPolyPath[0], &CurrArea);

	CurrFlags &= ~(SAMPLE_POLYFLAGS_NOONOS);

	// At this point we have our path.  Copy it to the path store
	int nIndex = 0;
	TraceResult hit;
	Vector TraceStart;

	Vector NodeFromLocation = FromFloorLocation;

	for (int nVert = 0; nVert < nVertCount; nVert++)
	{
		bot_path_node NextPathNode;

		NextPathNode.FromLocation = NodeFromLocation;

		NextPathNode.Location.x = StraightPath[nIndex++];
		NextPathNode.Location.z = StraightPath[nIndex++];
		NextPathNode.Location.y = -StraightPath[nIndex++];

		m_navMesh->getPolyArea(StraightPolyPath[nVert], &ThisArea);
		m_navMesh->getPolyFlags(StraightPolyPath[nVert], &ThisFlags);

		ThisFlags &= ~(SAMPLE_POLYFLAGS_NOONOS);

		if (ThisArea == SAMPLE_POLYAREA_GROUND || ThisArea == SAMPLE_POLYAREA_CROUCH)
		{
			NextPathNode.Location = UTIL_AdjustPointAwayFromNavWall(NextPathNode.Location, 16.0f);
		}

		TraceStart.x = NextPathNode.Location.x;
		TraceStart.y = NextPathNode.Location.y;
		TraceStart.z = NextPathNode.Location.z;

		UTIL_TraceLine(TraceStart, (TraceStart - Vector(0.0f, 0.0f, 100.0f)), ignore_monsters, ignore_glass, nullptr, &hit);

		if (hit.flFraction < 1.0f)
		{
			NextPathNode.Location = hit.vecEndPos;
			
			if (CurrFlags != SAMPLE_POLYFLAGS_JUMP && CurrFlags != SAMPLE_POLYFLAGS_WALLCLIMB)
			{
				NextPathNode.Location.z += 20.0f;
			}
		}

		NextPathNode.requiredZ = NextPathNode.Location.z;

		if (CurrFlags == SAMPLE_POLYFLAGS_WALLCLIMB || CurrFlags == SAMPLE_POLYFLAGS_LADDER)
		{
			float NewRequiredZ = UTIL_FindZHeightForWallClimb(NextPathNode.FromLocation, NextPathNode.Location, head_hull);
			//NextPathNode.requiredZ = fmaxf(NewRequiredZ, NextPathNode.Location.z);
			NextPathNode.requiredZ = NewRequiredZ;

			if (CurrFlags == SAMPLE_POLYFLAGS_LADDER)
			{
				NextPathNode.requiredZ += 5.0f;
			}

		}
		else
		{
			NextPathNode.requiredZ = NextPathNode.Location.z;
		}

		NextPathNode.flag = CurrFlags;
		NextPathNode.area = CurrArea;
		NextPathNode.poly = StraightPolyPath[nVert];

		CurrArea = ThisArea;
		CurrFlags = ThisFlags;

		NodeFromLocation = NextPathNode.Location;

		path.push_back(NextPathNode);
	}

	return DT_SUCCESS;
}

Vector NAV_GetNearestLiftDisembarkPoint(nav_door* LiftReference)
{
	AvHAIOffMeshConnection* NearestConnection = UTIL_GetOffMeshConnectionForLift(LiftReference);

	if (!NearestConnection) { return ZERO_VECTOR; }

	Vector DesiredStartStop = ZERO_VECTOR;
	Vector DesiredEndStop = ZERO_VECTOR;
	float minStartDist = 0.0f;
	float minEndDist = 0.0f;

	// Find the desired stop point for us to get onto the lift
	for (auto it = LiftReference->StopPoints.begin(); it != LiftReference->StopPoints.end(); it++)
	{
		Vector LiftStopPoint = (*it) + Vector(0.0f, 0.0f, LiftReference->DoorEdict->v.size.z * 0.5f);

		float thisStartDist = vDist3DSq(LiftStopPoint, NearestConnection->FromLocation);
		float thisEndDist = vDist3DSq(LiftStopPoint, NearestConnection->ToLocation);
		if (vIsZero(DesiredStartStop) || thisStartDist < minStartDist)
		{
			DesiredStartStop = *it;
			minStartDist = thisStartDist;
		}

		if (vIsZero(DesiredEndStop) || thisEndDist < minEndDist)
		{
			DesiredEndStop = *it;
			minEndDist = thisEndDist;
		}
	}

	Vector LiftPosition = UTIL_GetCentreOfEntity(LiftReference->DoorEdict);

	bool bIsLiftMoving = (LiftReference->DoorEdict->v.velocity.Length() > 0.0f);
	bool bIsLiftMovingToStart = bIsLiftMoving && (vDist3DSq(LiftReference->DoorEntity->m_vecFinalDest, DesiredStartStop) < sqrf(50.0f));
	bool bIsLiftMovingToEnd = bIsLiftMoving && (vDist3DSq(LiftReference->DoorEntity->m_vecFinalDest, DesiredEndStop) < sqrf(50.0f));
	bool bIsLiftAtOrNearStart = (vDist3DSq(LiftPosition, DesiredStartStop) < sqrf(50.0f));
	bool bIsLiftAtOrNearEnd = (vDist3DSq(LiftPosition, DesiredEndStop) < sqrf(50.0f));

	return (bIsLiftAtOrNearStart || bIsLiftMovingToStart) ? NearestConnection->FromLocation : NearestConnection->ToLocation;
}

dtStatus FindPathClosestToPoint(AvHAIPlayer* pBot, const BotMoveStyle MoveStyle, const Vector ToLocation, vector<bot_path_node>& path, float MaxAcceptableDistance)
{
	if (!pBot) { return DT_FAILURE; }

	if (pBot->BotNavInfo.NavProfile.bFlyingProfile)
	{
		return FindFlightPathToPoint(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, ToLocation, path, MaxAcceptableDistance);
	}

	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	bool bHasWelder = (m_navFilter->getIncludeFlags() & SAMPLE_POLYFLAGS_WELD);

	if (!m_navQuery || !m_navMesh || !m_navFilter || vIsZero(ToLocation))
	{
		return DT_FAILURE;
	}

	Vector FromLocation = pBot->CurrentFloorPosition;

	// Add a slight bias towards trying to move forward if on a railing or other narrow bit of navigable terrain
	// rather than potentially dropping back off it the wrong way
	Vector GeneralDir = UTIL_GetVectorNormal2D(ToLocation - pBot->CurrentFloorPosition);
	Vector CheckLocation = FromLocation + (GeneralDir * 16.0f);

	Vector FromFloorLocation = AdjustPointForPathfinding(CheckLocation);

	if (vIsZero(FromFloorLocation))
	{
		FromFloorLocation = AdjustPointForPathfinding(FromLocation);
	}

	nav_door* LiftReference = UTIL_GetLiftReferenceByEdict(pBot->Edict->v.groundentity);
	bool bMustDisembarkLiftFirst = false;
	Vector LiftStart = ZERO_VECTOR;
	Vector LiftEnd = ZERO_VECTOR;

	if (LiftReference)
	{
		LiftEnd = NAV_GetNearestLiftDisembarkPoint(LiftReference);

		if (!vIsZero(LiftEnd))
		{
			FromLocation = LiftEnd;			

			AvHAIOffMeshConnection* LiftOffMesh = UTIL_GetOffMeshConnectionForLift(LiftReference);

			if (LiftOffMesh)
			{
				LiftStart = (vEquals(LiftEnd, LiftOffMesh->ToLocation, 5.0f)) ? LiftOffMesh->FromLocation : LiftOffMesh->ToLocation;
				bMustDisembarkLiftFirst = true;
				FromFloorLocation = LiftEnd;
			}
		}
	}
	
	Vector ToFloorLocation = AdjustPointForPathfinding(ToLocation);

	float pStartPos[3] = { FromFloorLocation.x, FromFloorLocation.z, -FromFloorLocation.y };
	float pEndPos[3] = { ToFloorLocation.x, ToFloorLocation.z, -ToFloorLocation.y };

	dtStatus status;
	dtPolyRef StartPoly;
	float StartNearest[3];
	dtPolyRef EndPoly;
	float EndNearest[3];
	dtPolyRef PolyPath[MAX_PATH_POLY];
	dtPolyRef StraightPolyPath[MAX_AI_PATH_SIZE];
	int nPathCount = 0;
	float StraightPath[MAX_AI_PATH_SIZE * 3];
	unsigned char straightPathFlags[MAX_AI_PATH_SIZE];
	memset(straightPathFlags, 0, sizeof(straightPathFlags));
	int nVertCount = 0;

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, pExtents, m_navFilter, &StartPoly, StartNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, pExtents, m_navFilter, &EndPoly, EndNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[nPathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(MaxAcceptableDistance))
		{
			return DT_FAILURE;
		}
		else
		{
			dtVcopy(EndNearest, epos);
		}
	}

	status = m_navQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, straightPathFlags, StraightPolyPath, &nVertCount, MAX_AI_PATH_SIZE, DT_STRAIGHTPATH_AREA_CROSSINGS);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't create a path
	}

	if (nVertCount == 0)
	{
		return DT_FAILURE; // couldn't find a path
	}

	path.clear();

	unsigned int CurrFlags;
	unsigned char CurrArea;

	m_navMesh->getPolyFlags(StraightPolyPath[0], &CurrFlags);
	m_navMesh->getPolyArea(StraightPolyPath[0], &CurrArea);

	CurrFlags &= ~(SAMPLE_POLYFLAGS_NOONOS);

	// At this point we have our path.  Copy it to the path store
	int nIndex = 0;
	TraceResult hit;

	pBot->BotNavInfo.SpecialMovementFlags = 0;

	Vector NodeFromLocation = FromFloorLocation;

	if (bMustDisembarkLiftFirst)
	{
		bot_path_node StartPathNode;
		StartPathNode.FromLocation = LiftStart;
		StartPathNode.Location = LiftEnd;
		StartPathNode.flag = SAMPLE_POLYFLAGS_LIFT;
		StartPathNode.area = SAMPLE_POLYAREA_LIFT;
		
		path.push_back(StartPathNode);

		NodeFromLocation = LiftEnd;
	}

	for (int nVert = 0; nVert < nVertCount; nVert++)
	{
		bot_path_node NextPathNode;

		NextPathNode.FromLocation = NodeFromLocation;

		// The nav mesh doesn't always align perfectly with the floor, so align each nav point with the floor after generation
		NextPathNode.Location.x = StraightPath[nIndex++];
		NextPathNode.Location.z = StraightPath[nIndex++];
		NextPathNode.Location.y = -StraightPath[nIndex++];

		NextPathNode.Location = UTIL_AdjustPointAwayFromNavWall(NextPathNode.Location, 16.0f);

		NextPathNode.Location = AdjustPointForPathfinding(NextPathNode.Location);

		if ((CurrFlags != SAMPLE_POLYFLAGS_JUMP && CurrFlags != SAMPLE_POLYFLAGS_WALLCLIMB) || NextPathNode.FromLocation.z > NextPathNode.Location.z)
		{
			NextPathNode.Location.z += GetPlayerOriginOffsetFromFloor(pBot->Edict, (CurrArea == SAMPLE_POLYAREA_CROUCH)).z;
		}

		pBot->BotNavInfo.SpecialMovementFlags |= CurrFlags;

		if (pBot->BotNavInfo.SpecialMovementFlags & SAMPLE_POLYFLAGS_WELD)
		{
			bool bPing = true;
		}

		// End alignment to floor

		// For ladders and wall climbing, calculate the climb height needed to complete the move.
		// This what allows bots to climb over railings without having to explicitly place nav points on the railing itself
		NextPathNode.requiredZ = NextPathNode.Location.z;

		if (CurrFlags == SAMPLE_POLYFLAGS_WALLCLIMB || CurrFlags == SAMPLE_POLYFLAGS_LADDER)
		{
			int HullNum = GetPlayerHullIndex(pBot->Edict, false);
			Vector FromLocation = (path.size() > 0) ? path.back().Location : pBot->CurrentFloorPosition;
			float NewRequiredZ = UTIL_FindZHeightForWallClimb(FromLocation, NextPathNode.Location, head_hull);
			NextPathNode.requiredZ = fmaxf(NewRequiredZ, NextPathNode.Location.z);

			if (CurrFlags == SAMPLE_POLYFLAGS_LADDER)
			{
				NextPathNode.requiredZ += 5.0f;
			}

		}
		else
		{
			NextPathNode.requiredZ = NextPathNode.Location.z;
		}

		NextPathNode.flag = CurrFlags;
		NextPathNode.area = CurrArea;
		NextPathNode.poly = StraightPolyPath[nVert];

		m_navMesh->getPolyFlags(StraightPolyPath[nVert], &CurrFlags);
		m_navMesh->getPolyArea(StraightPolyPath[nVert], &CurrArea);

		CurrFlags &= ~(SAMPLE_POLYFLAGS_NOONOS);

		NodeFromLocation = NextPathNode.Location;

		path.push_back(NextPathNode);
	}

	return DT_SUCCESS;
}

bool UTIL_PointIsReachable(const nav_profile &NavProfile, const Vector FromLocation, const Vector ToLocation, const float MaxAcceptableDistance)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery || vIsZero(FromLocation) || vIsZero(ToLocation))
	{
		return false;
	}

	float pStartPos[3] = { FromLocation.x, FromLocation.z, -FromLocation.y };
	float pEndPos[3] = { ToLocation.x, ToLocation.z, -ToLocation.y };

	dtStatus status;
	dtPolyRef StartPoly;
	float StartNearest[3];
	dtPolyRef EndPoly;
	float EndNearest[3];
	dtPolyRef PolyPath[MAX_PATH_POLY];
	int nPathCount = 0;

	float searchExtents[3] = { MaxAcceptableDistance, 50.0f, MaxAcceptableDistance };

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, searchExtents, m_navFilter, &StartPoly, StartNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return false; // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, searchExtents, m_navFilter, &EndPoly, EndNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return false; // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (nPathCount == 0)
	{
		return false; // couldn't find a path
	}

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[nPathCount - 1], EndNearest, epos, 0);

		return (dtVdistSqr(EndNearest, epos) <= sqrf(MaxAcceptableDistance));

	}

	return true;
}

bool HasBotReachedPathPoint(const AvHAIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		return true;
	}

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	SamplePolyFlags CurrentNavFlag = (SamplePolyFlags)CurrentPathNode.flag;
	Vector MoveFrom = CurrentPathNode.FromLocation;
	Vector MoveTo = CurrentPathNode.Location;
	float RequiredClimbHeight = CurrentPathNode.requiredZ;

	Vector NextMoveLocation = ZERO_VECTOR;
	SamplePolyFlags NextMoveFlag = SAMPLE_POLYFLAGS_DISABLED;

	if ((pBot->BotNavInfo.CurrentPathPoint + 1) < pBot->BotNavInfo.CurrentPath.size())
	{
		bot_path_node NextPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint + 1];
		NextMoveLocation = NextPathNode.Location;
		NextMoveFlag = (SamplePolyFlags)NextPathNode.flag;
	}

	switch (CurrentNavFlag)
	{		
		case SAMPLE_POLYFLAGS_WALK:
			return HasBotCompletedWalkMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case SAMPLE_POLYFLAGS_WELD:
		case SAMPLE_POLYFLAGS_DOOR:
		case SAMPLE_POLYFLAGS_TEAM1STRUCTURE:
		case SAMPLE_POLYFLAGS_TEAM2STRUCTURE:
			return HasBotCompletedObstacleMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case SAMPLE_POLYFLAGS_LADDER:
			return HasBotCompletedLadderMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case SAMPLE_POLYFLAGS_FALL:
			return HasBotCompletedFallMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case SAMPLE_POLYFLAGS_WALLCLIMB:
			return HasBotCompletedClimbMove(pBot, MoveFrom, MoveTo, RequiredClimbHeight, NextMoveLocation, NextMoveFlag);
		case SAMPLE_POLYFLAGS_JUMP:
		case SAMPLE_POLYFLAGS_DUCKJUMP:
		case SAMPLE_POLYFLAGS_BLOCKED:
			return HasBotCompletedJumpMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case SAMPLE_POLYFLAGS_TEAM1PHASEGATE:
		case SAMPLE_POLYFLAGS_TEAM2PHASEGATE:
			return HasBotCompletedPhaseGateMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case SAMPLE_POLYFLAGS_LIFT:
			return HasBotCompletedLiftMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		default:
			return HasBotCompletedWalkMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	}

	return HasBotCompletedWalkMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
}

bool HasBotCompletedWalkMove(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	bool bNextPointReachable = false;

	if (NextMoveFlag != SAMPLE_POLYFLAGS_DISABLED)
	{
		bNextPointReachable = UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, NextMoveDestination);
	}

	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax) || (bNextPointReachable && vDist2DSq(pBot->Edict->v.origin, MoveEnd) < sqrf(GetPlayerRadius(pBot->Edict) * 2.0f));
}

bool HasBotCompletedObstacleMove(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

bool HasBotCompletedLadderMove(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	if (IsPlayerOnLadder(pBot->Edict)) { return false; }

	if (NextMoveFlag != SAMPLE_POLYFLAGS_DISABLED)
	{
		if (pBot->BotNavInfo.IsOnGround)
		{
			if (UTIL_PointIsDirectlyReachable(pBot->CollisionHullBottomLocation, NextMoveDestination)) { return true; }
		}
		else
		{
			if (vDist2DSq(pBot->Edict->v.origin, MoveEnd) < sqrf(GetPlayerRadius(pBot->Edict)) && UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, NextMoveDestination)) { return true; }
		}		

	}

	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

bool HasBotCompletedFallMove(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	if (NextMoveFlag != SAMPLE_POLYFLAGS_DISABLED)
	{
		Vector ThisMoveDir = UTIL_GetVectorNormal2D(MoveEnd - MoveStart);
		Vector NextMoveDir = UTIL_GetVectorNormal2D(NextMoveDestination - MoveEnd);

		float MoveDot = UTIL_GetDotProduct2D(ThisMoveDir, NextMoveDir);

		if (MoveDot > 0.0f)
		{
			if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, NextMoveDestination)
				&& UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, NextMoveDestination)
				&& fabsf(pBot->CollisionHullBottomLocation.z - MoveEnd.z) < 100.0f) { return true; }
		}
	}

	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

bool HasBotCompletedClimbMove(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, float RequiredClimbHeight, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	Vector PositionInMove = vClosestPointOnLine2D(MoveStart, MoveEnd, pBot->Edict->v.origin);

	if (!vEquals2D(PositionInMove, MoveEnd, 4.0f)) { return false; }

	if (pBot->BotNavInfo.IsOnGround)
	{
		return UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, NextMoveDestination);
	}
	else
	{
		if (NextMoveFlag != SAMPLE_POLYFLAGS_DISABLED)
		{
			Vector ThisMoveDir = UTIL_GetVectorNormal2D(MoveEnd - MoveStart);
			Vector NextMoveDir = UTIL_GetVectorNormal2D(NextMoveDestination - MoveEnd);

			float MoveDot = UTIL_GetDotProduct2D(ThisMoveDir, NextMoveDir);

			if (MoveDot > 0.0f)
			{
				if (pBot->Edict->v.origin.z >= RequiredClimbHeight && !pBot->BotNavInfo.IsOnGround)
				{
					if (UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, NextMoveDestination)
						&& fabsf(pBot->CollisionHullBottomLocation.z - MoveEnd.z) < 100.0f)
					{
						return true;
					}
				}
			}
		}
		else
		{
			return false;
		}
	}

	return false;
}

bool HasBotCompletedJumpMove(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	Vector PositionInMove = vClosestPointOnLine2D(MoveStart, MoveEnd, pBot->Edict->v.origin);

	if (!vEquals2D(PositionInMove, MoveEnd, 2.0f)) { return false; }

	if (NextMoveFlag != SAMPLE_POLYFLAGS_DISABLED)
	{
		Vector ThisMoveDir = UTIL_GetVectorNormal2D(MoveEnd - MoveStart);
		Vector NextMoveDir = UTIL_GetVectorNormal2D(NextMoveDestination - MoveEnd);

		float MoveDot = UTIL_GetDotProduct2D(ThisMoveDir, NextMoveDir);

		if (MoveDot >= 0.0f)
		{
			Vector HullTraceEnd = MoveEnd;
			HullTraceEnd.z = pBot->Edict->v.origin.z;

			if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, NextMoveDestination)
				&& UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, HullTraceEnd, head_hull, false)
				&& fabsf(pBot->CollisionHullBottomLocation.z - MoveEnd.z) < 100.0f)
			{
				return true;
			}
		}
	}	

	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

bool HasBotCompletedPhaseGateMove(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax) || vDist2DSq(pBot->Edict->v.origin, MoveEnd) < sqrf(32.0f);
}

bool HasBotCompletedLiftMove(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

void CheckAndHandleDoorObstruction(AvHAIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size()) { return; }

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	edict_t* BlockingDoorEdict = UTIL_GetDoorBlockingPathPoint(pBot->Edict->v.origin, CurrentPathNode.Location, CurrentPathNode.flag, nullptr);
	
	if (FNullEnt(BlockingDoorEdict))
	{
		int NumIterations = 0;

		for (int i = (pBot->BotNavInfo.CurrentPathPoint + 1); i < pBot->BotNavInfo.CurrentPath.size(); i++)
		{
			bot_path_node ThisPathNode = pBot->BotNavInfo.CurrentPath[i];

			BlockingDoorEdict = UTIL_GetDoorBlockingPathPoint(ThisPathNode.FromLocation, ThisPathNode.Location, ThisPathNode.flag, nullptr);

			NumIterations++;

			if (!FNullEnt(BlockingDoorEdict) || NumIterations >= 2)
			{
				break;
			}
		}
	}

	if (FNullEnt(BlockingDoorEdict)) { return; }

	CBaseToggle* BlockingDoor = dynamic_cast<CBaseToggle*>(CBaseEntity::Instance(BlockingDoorEdict));

	if (!BlockingDoor)
	{
		AvHWeldable* WeldableRef = dynamic_cast<AvHWeldable*>(CBaseEntity::Instance(BlockingDoorEdict));

		if (!WeldableRef)
		{
			return;
		}

		NAV_SetWeldMovementTask(pBot, BlockingDoorEdict, nullptr);

		return;
	}

	Vector NearestPoint = UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, BlockingDoorEdict);

	// If the door is in the process of opening or closing, let it finish before doing anything else
	if (BlockingDoor->m_toggle_state == TS_GOING_UP || BlockingDoor->m_toggle_state == TS_GOING_DOWN)
	{
		if (IsPlayerTouchingEntity(pBot->Edict, BlockingDoorEdict))
		{
			Vector MoveDir = UTIL_GetVectorNormal2D(CurrentPathNode.Location - CurrentPathNode.FromLocation);

			pBot->desiredMovementDir = MoveDir;
			return;
		}

		if (vDist2DSq(pBot->Edict->v.origin, NearestPoint) < sqrf(UTIL_MetresToGoldSrcUnits(1.5f)))
		{
			// Wait for the door to finish opening
			pBot->desiredMovementDir = g_vecZero;
			BotLookAt(pBot, CurrentPathNode.Location);
		}
		return;
	}

	// If we're blocked by a door that's open, and its wait time isn't infinite (i.e. it will close shortly) then just wait it out
	if (BlockingDoor->m_toggle_state == TS_AT_TOP && BlockingDoor->m_flWait >= 0.0f)
	{
		// Wait for the door to start closing
		if (vDist2DSq(pBot->Edict->v.origin, NearestPoint) < sqrf(UTIL_MetresToGoldSrcUnits(1.5f)))
		{
			// Wait for the door to finish opening
			pBot->desiredMovementDir = g_vecZero;
			BotLookAt(pBot, BlockingDoorEdict);
		}
		return;
	}

	nav_door* Door = UTIL_GetNavDoorByEdict(BlockingDoorEdict);

	if (Door)
	{
		// Door opens just by being directly used
		if (Door->ActivationType == DOOR_USE)
		{
			if (IsPlayerInUseRange(pBot->Edict, Door->DoorEdict))
			{
				if (pBot->Edict->v.oldbuttons & IN_DUCK)
				{
					pBot->Button |= IN_DUCK;
				}

				BotUseObject(pBot, Door->DoorEdict, false);
			}

			return;
		}

		// Door must be shot to open
		if (Door->ActivationType == DOOR_SHOOT)
		{
			BotShootTarget(pBot, GetPlayerCurrentWeapon(pBot->Player), Door->DoorEdict);
			return;
		}


		DoorTrigger* Trigger = UTIL_GetNearestDoorTrigger(pBot->CurrentFloorPosition, Door, nullptr, true);

		// Fail-safe: If the bot cannot reach any trigger for whatever reason, then telepathically trigger one otherwise it will be stuck forever
		if (!Trigger)
		{
			for (auto it = Door->TriggerEnts.begin(); it != Door->TriggerEnts.end(); it++)
			{
				if (it->NextActivationTime > gpGlobals->time)
				{
					Trigger = nullptr;
					break;
				}

				Trigger = &(*it);
			}

			if (Trigger)
			{
				Trigger->Entity->Use(pBot->Player, pBot->Player, USE_TOGGLE, 0.0f);
				return;
			}
		}

		if (Trigger && Trigger->NextActivationTime < gpGlobals->time)
		{
			if (Trigger->TriggerType == DOOR_BUTTON)
			{
				Vector UseLocation = UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, Trigger->Edict);

				NAV_SetUseMovementTask(pBot, Trigger->Edict, Trigger);
			}
			else if (Trigger->TriggerType == DOOR_TRIGGER)
			{
				NAV_SetTouchMovementTask(pBot, Trigger->Edict, Trigger);
			}
			else if (Trigger->TriggerType == DOOR_WELD)
			{
				NAV_SetWeldMovementTask(pBot, Trigger->Edict, Trigger);
			}
			else if (Trigger->TriggerType == DOOR_BREAK)
			{
				NAV_SetBreakMovementTask(pBot, Trigger->Edict, Trigger);
			}

			return;
		}
	}
	
}

edict_t* UTIL_GetDoorBlockingPathPoint(AvHAIPlayer* pBot, bot_path_node* PathNode, edict_t* SearchDoor)
{
	if (!PathNode) { return nullptr; }

	Vector FromLoc = PathNode->FromLocation;
	Vector ToLoc = PathNode->Location;

	TraceResult doorHit;

	if (PathNode->flag == SAMPLE_POLYFLAGS_LADDER || PathNode->flag == SAMPLE_POLYFLAGS_WALLCLIMB)
	{
		Vector TargetLoc = Vector(FromLoc.x, FromLoc.y, PathNode->requiredZ);

		if (!FNullEnt(SearchDoor))
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchDoor->v.absmin, SearchDoor->v.absmax))
			{
				return SearchDoor;
			}
		}
		else
		{
			for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
				{
					return it->DoorEdict;
				}
			}

			for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
				{
					return it->WeldableEdict;
				}
			}
		}

		Vector TargetLoc2 = Vector(ToLoc.x, ToLoc.y, PathNode->requiredZ);

		if (!FNullEnt(SearchDoor))
		{
			if (vlineIntersectsAABB(TargetLoc, TargetLoc2, SearchDoor->v.absmin, SearchDoor->v.absmax))
			{
				return SearchDoor;
			}
		}
		else
		{
			for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
			{
				if (vlineIntersectsAABB(TargetLoc, TargetLoc2, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
				{
					return it->DoorEdict;
				}
			}

			for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
			{
				if (vlineIntersectsAABB(TargetLoc, TargetLoc2, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
				{
					return it->WeldableEdict;
				}
			}
		}

	}
	else if (PathNode->flag == SAMPLE_POLYFLAGS_FALL)
	{
		Vector TargetLoc = Vector(ToLoc.x, ToLoc.y, FromLoc.z);

		if (!FNullEnt(SearchDoor))
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchDoor->v.absmin, SearchDoor->v.absmax))
			{
				return SearchDoor;
			}
		}
		else
		{
			for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
				{
					return it->DoorEdict;
				}
			}

			for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
				{
					return it->WeldableEdict;
				}
			}
		}

		Vector NextTargetLoc = ToLoc + Vector(0.0f, 0.0f, 10.0f);

		if (!FNullEnt(SearchDoor))
		{
			if (vlineIntersectsAABB(TargetLoc, NextTargetLoc, SearchDoor->v.absmin, SearchDoor->v.absmax))
			{
				return SearchDoor;
			}
		}
		else
		{
			for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
			{
				if (vlineIntersectsAABB(TargetLoc, NextTargetLoc, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
				{
					return it->DoorEdict;
				}
			}

			for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
			{
				if (vlineIntersectsAABB(TargetLoc, NextTargetLoc, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
				{
					return it->WeldableEdict;
				}
			}
		}
	}

	Vector StartTrace = FromLoc + Vector(0.0f, 0.0f, 16.0f);
	Vector EndTrace = ToLoc + Vector(0.0f, 0.0f, 16.0f);

	if (!FNullEnt(SearchDoor))
	{
		if (vlineIntersectsAABB(StartTrace, EndTrace, SearchDoor->v.absmin, SearchDoor->v.absmax))
		{
			return SearchDoor;
		}
	}
	else
	{
		for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
		{
			if (vlineIntersectsAABB(StartTrace, EndTrace, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
			{
				return it->DoorEdict;
			}
		}

		for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
		{
			if (vlineIntersectsAABB(StartTrace, EndTrace, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
			{
				return it->WeldableEdict;
			}
		}
	}

	return nullptr;
}

edict_t* UTIL_GetBreakableBlockingPathPoint(AvHAIPlayer* pBot, bot_path_node* PathNode, edict_t* SearchBreakable)
{
	Vector FromLoc = PathNode->FromLocation;
	Vector ToLoc = PathNode->Location;

	TraceResult breakableHit;

	if (PathNode->flag == SAMPLE_POLYFLAGS_LADDER || PathNode->flag == SAMPLE_POLYFLAGS_WALLCLIMB)
	{
		Vector TargetLoc = Vector(FromLoc.x, FromLoc.y, PathNode->requiredZ);

		UTIL_TraceLine(FromLoc, TargetLoc, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

		if (!FNullEnt(SearchBreakable))
		{
			if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
		}
		else
		{
			if (!FNullEnt(breakableHit.pHit))
			{
				if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
				{
					return breakableHit.pHit;
				}
			}

		}

		Vector TargetLoc2 = Vector(ToLoc.x, ToLoc.y, PathNode->requiredZ);

		UTIL_TraceLine(TargetLoc, TargetLoc2, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

		if (!FNullEnt(SearchBreakable))
		{
			if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
		}
		else
		{
			if (!FNullEnt(breakableHit.pHit))
			{
				if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
				{
					return breakableHit.pHit;
				}
			}
		}

	}
	else if (PathNode->flag == SAMPLE_POLYFLAGS_FALL)
	{
		Vector TargetLoc = Vector(ToLoc.x, ToLoc.y, FromLoc.z);

		UTIL_TraceLine(FromLoc, TargetLoc, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

		if (!FNullEnt(SearchBreakable))
		{
			if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
		}
		else
		{
			if (!FNullEnt(breakableHit.pHit))
			{
				if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
				{
					return breakableHit.pHit;
				}
			}
		}

		UTIL_TraceLine(TargetLoc, ToLoc + Vector(0.0f, 0.0f, 10.0f), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

		if (!FNullEnt(SearchBreakable))
		{
			if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
		}
		else
		{
			if (!FNullEnt(breakableHit.pHit))
			{
				if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
				{
					return breakableHit.pHit;
				}
			}
		}
	}

	UTIL_TraceLine(FromLoc, ToLoc, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);


	if (!FNullEnt(SearchBreakable))
	{
		if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
	}
	else
	{
		if (!FNullEnt(breakableHit.pHit))
		{
			if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
			{
				return breakableHit.pHit;
			}
		}
	}


	return nullptr;
}

edict_t* UTIL_GetBreakableBlockingPathPoint(AvHAIPlayer* pBot, const Vector FromLocation, const Vector ToLocation, const unsigned int MovementFlag, edict_t* SearchBreakable)
{
	Vector FromLoc = FromLocation;
	Vector ToLoc = ToLocation;

	TraceResult breakableHit;

	if (MovementFlag == SAMPLE_POLYFLAGS_LADDER || MovementFlag == SAMPLE_POLYFLAGS_WALLCLIMB)
	{
		Vector TargetLoc = Vector(FromLoc.x, FromLoc.y, ToLocation.z);

		UTIL_TraceLine(FromLoc, TargetLoc, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

		if (!FNullEnt(SearchBreakable))
		{
			if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
		}
		else
		{
			if (!FNullEnt(breakableHit.pHit))
			{
				if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
				{
					return breakableHit.pHit;
				}
			}

		}

		Vector TargetLoc2 = Vector(ToLoc.x, ToLoc.y, ToLocation.z);

		UTIL_TraceLine(TargetLoc, TargetLoc2, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

		if (!FNullEnt(SearchBreakable))
		{
			if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
		}
		else
		{
			if (!FNullEnt(breakableHit.pHit))
			{
				if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
				{
					return breakableHit.pHit;
				}
			}
		}

	}
	else if (MovementFlag == SAMPLE_POLYFLAGS_FALL)
	{
		Vector TargetLoc = Vector(ToLoc.x, ToLoc.y, FromLoc.z);

		UTIL_TraceLine(FromLoc, TargetLoc, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

		if (!FNullEnt(SearchBreakable))
		{
			if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
		}
		else
		{
			if (!FNullEnt(breakableHit.pHit))
			{
				if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
				{
					return breakableHit.pHit;
				}
			}
		}

		UTIL_TraceLine(TargetLoc, ToLoc + Vector(0.0f, 0.0f, 10.0f), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

		if (!FNullEnt(SearchBreakable))
		{
			if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
		}
		else
		{
			if (!FNullEnt(breakableHit.pHit))
			{
				if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
				{
					return breakableHit.pHit;
				}
			}
		}
	}

	UTIL_TraceLine(FromLoc, ToLoc, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

	if (!FNullEnt(SearchBreakable))
	{
		if (breakableHit.pHit == SearchBreakable) { return breakableHit.pHit; }
	}
	else
	{
		if (!FNullEnt(breakableHit.pHit))
		{
			if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
			{
				return breakableHit.pHit;
			}
		}
	}


	return nullptr;
}

edict_t* UTIL_GetDoorBlockingPathPoint(const Vector FromLocation, const Vector ToLocation, const unsigned int MovementFlag, edict_t* SearchDoor)
{

	Vector FromLoc = FromLocation;
	Vector ToLoc = ToLocation;

	TraceResult doorHit;

	if (MovementFlag == SAMPLE_POLYFLAGS_LADDER || MovementFlag == SAMPLE_POLYFLAGS_WALLCLIMB)
	{
		Vector TargetLoc = Vector(FromLoc.x, FromLoc.y, ToLocation.z);

		if (!FNullEnt(SearchDoor))
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchDoor->v.absmin, SearchDoor->v.absmax))
			{
				return SearchDoor;
			}
		}
		else
		{
			for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
				{
					return it->DoorEdict;
				}
			}

			for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
				{
					return it->WeldableEdict;
				}
			}
		}

		Vector TargetLoc2 = Vector(ToLoc.x, ToLoc.y, ToLocation.z);

		if (!FNullEnt(SearchDoor))
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc2, SearchDoor->v.absmin, SearchDoor->v.absmax))
			{
				return SearchDoor;
			}
		}
		else
		{
			for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc2, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
				{
					return it->DoorEdict;
				}
			}

			for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc2, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
				{
					return it->WeldableEdict;
				}
			}
		}

	}
	else if (MovementFlag == SAMPLE_POLYFLAGS_FALL)
	{
		Vector TargetLoc = Vector(ToLoc.x, ToLoc.y, FromLoc.z);

		if (!FNullEnt(SearchDoor))
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchDoor->v.absmin, SearchDoor->v.absmax))
			{
				return SearchDoor;
			}
		}
		else
		{
			for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
				{
					return it->DoorEdict;
				}
			}

			for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
				{
					return it->WeldableEdict;
				}
			}
		}

		if (!FNullEnt(SearchDoor))
		{
			if (vlineIntersectsAABB(TargetLoc, ToLoc, SearchDoor->v.absmin, SearchDoor->v.absmax))
			{
				return SearchDoor;
			}
		}
		else
		{
			for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
			{
				if (vlineIntersectsAABB(TargetLoc, ToLoc, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
				{
					return it->DoorEdict;
				}
			}

			for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
			{
				if (vlineIntersectsAABB(TargetLoc, ToLoc, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
				{
					return it->WeldableEdict;
				}
			}
		}

	}

	Vector TargetLoc = ToLoc + Vector(0.0f, 0.0f, 10.0f);

	if (!FNullEnt(SearchDoor))
	{
		if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchDoor->v.absmin, SearchDoor->v.absmax))
		{
			return SearchDoor;
		}
	}
	else
	{
		for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, it->DoorEdict->v.absmin, it->DoorEdict->v.absmax))
			{
				return it->DoorEdict;
			}
		}

		for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, it->WeldableEdict->v.absmin, it->WeldableEdict->v.absmax))
			{
				return it->WeldableEdict;
			}
		}
	}

	return nullptr;
}

bool UTIL_IsPathBlockedByDoor(const Vector StartLoc, const Vector EndLoc, edict_t* SearchDoor)
{
	Vector ValidNavmeshPoint = UTIL_ProjectPointToNavmesh(EndLoc, BaseNavProfiles[ALL_NAV_PROFILE]);

	if (!ValidNavmeshPoint)
	{
		return false;
	}

	vector<bot_path_node> TestPath;
	TestPath.clear();

	// Now we find a path backwards from the valid nav mesh point to our location, trying to get as close as we can to it

	dtStatus PathFindingStatus = FindPathClosestToPoint(BaseNavProfiles[ALL_NAV_PROFILE], StartLoc, ValidNavmeshPoint, TestPath, 50.0f);

	if (dtStatusSucceed(PathFindingStatus))
	{
		for (auto it = TestPath.begin(); it != TestPath.end(); it++)
		{
			if (UTIL_GetDoorBlockingPathPoint(nullptr, &(*it), SearchDoor) != nullptr)
			{
				return true;
			}
		}

		return false;
	}

	return true;
}

DoorTrigger* UTIL_GetNearestDoorTriggerFromLift(edict_t* LiftEdict, nav_door* Door, CBaseEntity* IgnoreTrigger)
{
	if (!Door) { return nullptr; }

	if (Door->TriggerEnts.size() == 0) { return nullptr; }

	DoorTrigger* NearestTrigger = nullptr;
	float NearestDist = 0.0f;

	for (auto it = Door->TriggerEnts.begin(); it != Door->TriggerEnts.end(); it++)
	{
		if (!FNullEnt(it->Edict) && it->Entity != IgnoreTrigger && it->bIsActivated)
		{
			Vector ButtonLocation = UTIL_GetClosestPointOnEntityToLocation(UTIL_GetCentreOfEntity(LiftEdict), it->Edict);
			Vector NearestPointOnLift = UTIL_GetClosestPointOnEntityToLocation(ButtonLocation, LiftEdict);

			float thisDist = vDist3DSq(ButtonLocation, NearestPointOnLift);

			if (thisDist < sqrf(64.0f))
			{
				if (!NearestTrigger || thisDist < NearestDist)
				{
					NearestTrigger = &(*it);
					NearestDist = thisDist;
				}

			}
		}
	}

	return NearestTrigger;
}

DoorTrigger* UTIL_GetNearestDoorTrigger(const Vector Location, nav_door* Door, CBaseEntity* IgnoreTrigger, bool bCheckBlockedByDoor)
{
	if (!Door) { return nullptr; }

	if (Door->TriggerEnts.size() == 0) { return nullptr; }

	DoorTrigger* NearestTrigger = nullptr;
	float NearestDist = 0.0f;

	Vector DoorLocation = UTIL_GetCentreOfEntity(Door->DoorEdict);

	for (auto it = Door->TriggerEnts.begin(); it != Door->TriggerEnts.end(); it++)
	{
		if (!FNullEnt(it->Edict) && it->Entity != IgnoreTrigger && it->bIsActivated)
		{
			Vector ButtonLocation = UTIL_GetButtonFloorLocation(Location, it->Edict);

			if ((!bCheckBlockedByDoor || !UTIL_IsPathBlockedByDoor(Location, ButtonLocation, Door->DoorEdict)) && UTIL_PointIsReachable(GetBaseNavProfile(MARINE_BASE_NAV_PROFILE), Location, ButtonLocation, 64.0f))
			{
				float ThisDist = vDist3DSq(Location, ButtonLocation);

				if (!NearestTrigger || ThisDist < NearestDist)
				{
					NearestTrigger = &(*it);
					NearestDist = ThisDist;
				}

			}
		}
	}

	return NearestTrigger;
}

void CheckAndHandleBreakableObstruction(AvHAIPlayer* pBot, const Vector MoveFrom, const Vector MoveTo, unsigned int MovementFlags)
{
	if (pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size()) { return; }

	Vector MoveDir = UTIL_GetVectorNormal2D(MoveTo - pBot->Edict->v.origin);

	if (vIsZero(MoveDir))
	{
		MoveDir = UTIL_GetForwardVector2D(pBot->Edict->v.angles);
	}

	TraceResult breakableHit;

	edict_t* BlockingBreakableEdict = nullptr;

	UTIL_TraceLine(pBot->Edict->v.origin, pBot->Edict->v.origin + (MoveDir * 100.0f), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

	if (!FNullEnt(breakableHit.pHit))
	{
		if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
		{
			BlockingBreakableEdict = breakableHit.pHit;
		}
	}

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	if (FNullEnt(BlockingBreakableEdict))
	{
		BlockingBreakableEdict = UTIL_GetBreakableBlockingPathPoint(pBot, &CurrentPathNode, nullptr);
	}

	if (FNullEnt(BlockingBreakableEdict))
	{
		int NumIterations = 0;

		for (int i = (pBot->BotNavInfo.CurrentPathPoint + 1); i < pBot->BotNavInfo.CurrentPath.size(); i++)
		{
			bot_path_node ThisPathNode = pBot->BotNavInfo.CurrentPath[i];
			BlockingBreakableEdict = UTIL_GetBreakableBlockingPathPoint(pBot, &ThisPathNode, nullptr);

			NumIterations++;

			if (!FNullEnt(BlockingBreakableEdict) || NumIterations >= 2)
			{
				break;
			}
		}
	}

	if (FNullEnt(BlockingBreakableEdict)) { return; }

	Vector ClosestPoint = UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, BlockingBreakableEdict);

	AvHAIWeapon DesiredWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);

	if (IsPlayerMarine(pBot->Player))
	{
		DesiredWeapon = BotMarineChooseBestWeapon(pBot, nullptr);
	}
	else
	{
		if (IsPlayerSkulk(pBot->Edict))
		{
			DesiredWeapon = (BlockingBreakableEdict->v.health <= 30) ? WEAPON_SKULK_PARASITE : WEAPON_SKULK_BITE;
		}
	}

	float DesiredRange = GetMaxIdealWeaponRange(DesiredWeapon);

	if (vDist2DSq(pBot->Edict->v.origin, ClosestPoint) < sqrf(16.0f))
	{
		if (pBot->Edict->v.oldbuttons & IN_DUCK)
		{
			pBot->Button |= IN_DUCK;
		}
		else
		{
			if (pBot->CurrentEyePosition.z - ClosestPoint.z > 32.0f)
			{
				pBot->Button |= IN_DUCK;
			}
		}
	}

	if (vDist3DSq(ClosestPoint, pBot->CurrentEyePosition) < sqrf(DesiredRange))
	{
		BotLookAt(pBot, BlockingBreakableEdict);

		pBot->DesiredMoveWeapon = DesiredWeapon;

		if (GetPlayerCurrentWeapon(pBot->Player) == DesiredWeapon)
		{
			pBot->Button |= IN_ATTACK;
		}
	}
	
}

void NewMove(AvHAIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		return;
	}

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	Vector MoveFrom = CurrentPathNode.FromLocation;
	Vector MoveTo = CurrentPathNode.Location;

	SamplePolyAreas CurrentNavArea = (SamplePolyAreas)CurrentPathNode.area;
	SamplePolyFlags CurrentNavFlags = (SamplePolyFlags)CurrentPathNode.flag;

	// Used to anticipate if we're about to enter a crouch area so we can start crouching early
	unsigned char NextArea = SAMPLE_POLYAREA_GROUND;
	SamplePolyFlags NextNavFlags = SAMPLE_POLYFLAGS_DISABLED;

	if (pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size() - 1)
	{
		bot_path_node NextPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint + 1];

		NextArea = NextPathNode.area;
		NextNavFlags = (SamplePolyFlags)NextPathNode.flag;

		bool bIsNearNextPoint = (vDist2DSq(pBot->Edict->v.origin, NextPathNode.FromLocation) <= sqrf(50.0f));

		// Start crouching early if we're about to enter a crouch path point
		if (CanPlayerCrouch(pBot->Edict) && (CurrentNavArea == SAMPLE_POLYAREA_CROUCH || (NextArea == SAMPLE_POLYAREA_CROUCH && bIsNearNextPoint)))
		{
			pBot->Button |= IN_DUCK;
		}
	}

	switch (CurrentNavFlags)
	{
	case SAMPLE_POLYFLAGS_WALK:
		GroundMove(pBot, MoveFrom, MoveTo);
		break;
	case SAMPLE_POLYFLAGS_FALL:
		FallMove(pBot, MoveFrom, MoveTo);
		break;
	case SAMPLE_POLYFLAGS_JUMP:
		JumpMove(pBot, MoveFrom, MoveTo);
		break;
	case SAMPLE_POLYFLAGS_BLOCKED:
		BlockedMove(pBot, MoveFrom, MoveTo);
		break;
	case SAMPLE_POLYFLAGS_TEAM1STRUCTURE:
	case SAMPLE_POLYFLAGS_TEAM2STRUCTURE:
		StructureBlockedMove(pBot, MoveFrom, MoveTo);
		break;
	case SAMPLE_POLYFLAGS_WALLCLIMB:
	{
		if (PlayerHasWeapon(pBot->Player, WEAPON_FADE_BLINK))
		{
			BlinkClimbMove(pBot, MoveFrom, MoveTo, CurrentPathNode.requiredZ);
		}
		else
		{
			WallClimbMove(pBot, MoveFrom, MoveTo, CurrentPathNode.requiredZ);
		}
	}
	break;
	case SAMPLE_POLYFLAGS_LADDER:
		LadderMove(pBot, MoveFrom, MoveTo, CurrentPathNode.requiredZ, NextArea);
		break;
	case SAMPLE_POLYFLAGS_TEAM1PHASEGATE:
	case SAMPLE_POLYFLAGS_TEAM2PHASEGATE:
		PhaseGateMove(pBot, MoveFrom, MoveTo);
		break;
	case SAMPLE_POLYFLAGS_LIFT:
		LiftMove(pBot, MoveFrom, MoveTo);
		break;
	default:
		GroundMove(pBot, MoveFrom, MoveTo);
		break;
	}

	if (vIsZero(pBot->LookTargetLocation) && vIsZero(pBot->MoveLookLocation))
	{
		Vector FurthestView = UTIL_GetFurthestVisiblePointOnPath(pBot);

		if (vIsZero(FurthestView) || vDist2DSq(FurthestView, pBot->CurrentEyePosition) < sqrf(200.0f))
		{
			FurthestView = MoveTo;

			Vector LookNormal = UTIL_GetVectorNormal2D(FurthestView - pBot->CurrentEyePosition);

			FurthestView = FurthestView + (LookNormal * 1000.0f);
		}

		BotLookAt(pBot, FurthestView);
	}

	// While moving, check to make sure we're not obstructed by a func_breakable, e.g. vent or window.
	CheckAndHandleBreakableObstruction(pBot, MoveFrom, MoveTo, CurrentNavFlags);

	if (CurrentNavFlags != SAMPLE_POLYFLAGS_LIFT && NextNavFlags != SAMPLE_POLYFLAGS_LIFT)
	{
		CheckAndHandleDoorObstruction(pBot);
	}

}

void GroundMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	edict_t* pEdict = pBot->Edict;

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	Vector CurrentPos = (pBot->BotNavInfo.IsOnGround) ? pBot->Edict->v.origin : pBot->CurrentFloorPosition;

	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - CurrentPos);

	// If we are over our current path point and can't get to it, try walking towards the next path point if we have one, or just directly forwards
	if (vIsZero(vForward))
	{
		if (pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size() - 1)
		{
			bot_path_node NextPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint + 1];

			vForward = UTIL_GetVectorNormal2D(NextPathNode.Location - CurrentPos);
		}
		else
		{
			vForward = UTIL_GetForwardVector2D(pBot->Edict->v.angles);
		}
	}

	// Same goes for the right vector, might not be the same as the bot's right
	Vector vRight = UTIL_GetVectorNormal(UTIL_GetCrossProduct(vForward, UP_VECTOR));

	bool bAdjustingForCollision = false;

	float PlayerRadius = GetPlayerRadius(pBot->Player) + 2.0f;

	Vector stTrcLft = CurrentPos - (vRight * PlayerRadius);
	Vector stTrcRt = CurrentPos + (vRight * PlayerRadius);
	Vector endTrcLft = stTrcLft + (vForward * 24.0f);
	Vector endTrcRt = stTrcRt + (vForward * 24.0f);

	bool bumpLeft = !UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, stTrcLft, endTrcLft);
	bool bumpRight = !UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, stTrcRt, endTrcRt);

	pBot->desiredMovementDir = vForward;

	if (bumpRight && !bumpLeft)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
	}
	else if (bumpLeft && !bumpRight)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
	}
	else if (bumpLeft && bumpRight)
	{
		stTrcLft.z = pBot->Edict->v.origin.z;
		stTrcRt.z = pBot->Edict->v.origin.z;
		endTrcLft.z = pBot->Edict->v.origin.z;
		endTrcRt.z = pBot->Edict->v.origin.z;

		if (!UTIL_QuickTrace(pBot->Edict, stTrcLft, endTrcLft))
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
		}
		else
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
		}
	}
	else
	{
		float DistFromLine = vDistanceFromLine2D(StartPoint, EndPoint, CurrentPos);

		if (DistFromLine > 18.0f)
		{
			float modifier = (float)vPointOnLine(StartPoint, EndPoint, CurrentPos);
			pBot->desiredMovementDir = pBot->desiredMovementDir + (vRight * modifier);
		}

		float LeapDist = (IsPlayerSkulk(pBot->Edict)) ? UTIL_MetresToGoldSrcUnits(5.0f) : UTIL_MetresToGoldSrcUnits(2.0f);

		if (IsPlayerFade(pBot->Edict) && CurrentPathNode.area == SAMPLE_POLYAREA_CROUCH)
		{
			LeapDist = UTIL_MetresToGoldSrcUnits(1.0f);
		}

		bool bIsAmbush = (pBot->BotNavInfo.MoveStyle == MOVESTYLE_AMBUSH);

		if (!bIsAmbush && CanBotLeap(pBot) && vDist2DSq(pBot->Edict->v.origin, EndPoint) > sqrf(LeapDist) && UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, EndPoint))
		{
			float CombatWeaponEnergyCost = GetEnergyCostForWeapon(pBot->DesiredCombatWeapon);
			float RequiredEnergy = (CombatWeaponEnergyCost + GetLeapCost(pBot)) - (GetPlayerEnergyRegenPerSecond(pEdict) * 0.5f); // We allow for around .5s of regen time as well

			if (GetPlayerEnergy(pBot->Edict) >= RequiredEnergy)
			{
				Vector CurrVelocity = UTIL_GetVectorNormal2D(pBot->Edict->v.velocity);

				float MoveDot = UTIL_GetDotProduct2D(CurrVelocity, vForward);

				if (MoveDot >= 0.95f)
				{
					BotLeap(pBot, EndPoint);
				}
			}
		}
	}

	pBot->desiredMovementDir = UTIL_GetVectorNormal2D(pBot->desiredMovementDir);

	if (CanPlayerCrouch(pEdict))
	{
		Vector HeadLocation = GetPlayerTopOfCollisionHull(pEdict, false);

		// Crouch if we have something in our way at head height
		if (!UTIL_QuickTrace(pBot->Edict, HeadLocation, (HeadLocation + (pBot->desiredMovementDir * 50.0f))))
		{
			pBot->Button |= IN_DUCK;
		}
	}
}

void FallMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	Vector vBotOrientation = UTIL_GetVectorNormal2D(EndPoint - pBot->Edict->v.origin);
	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);

	if (pBot->BotNavInfo.IsOnGround)
	{
		if (vDist2DSq(pBot->Edict->v.origin, EndPoint) > sqrf(GetPlayerRadius(pBot->Player)))
		{
			pBot->desiredMovementDir = vBotOrientation;
		}
		else
		{
			pBot->desiredMovementDir = vForward;
		}

		bool bCanDuck = (IsPlayerMarine(pBot->Edict) || IsPlayerFade(pBot->Edict) || IsPlayerOnos(pBot->Edict));

		if (!bCanDuck) { return; }

		Vector HeadLocation = GetPlayerTopOfCollisionHull(pBot->Edict, false);

		if (!UTIL_QuickTrace(pBot->Edict, HeadLocation, (HeadLocation + (pBot->desiredMovementDir * 50.0f))))
		{
			pBot->Button |= IN_DUCK;
		}
	}
	else
	{
		pBot->desiredMovementDir = vBotOrientation;
	}



}

void StructureBlockedMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);

	pBot->desiredMovementDir = vForward;

	DeployableSearchFilter BlockingFilter;
	BlockingFilter.DeployableTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());
	BlockingFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(3.0f);

	vector<AvHAIBuildableStructure> BlockingStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &BlockingFilter);

	AvHAIBuildableStructure CulpritStructure;
	float MinDist = 0.0f;

	for (auto it = BlockingStructures.begin(); it != BlockingStructures.end(); it++)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		float ThisDist = vDistanceFromLine2DSq(StartPoint, EndPoint, ThisStructure.Location);

		if (FNullEnt(CulpritStructure.edict) || ThisDist < MinDist)
		{
			CulpritStructure = ThisStructure;
		}
	}

	if (CulpritStructure.IsValid())
	{
		BotMoveLookAt(pBot, CulpritStructure.Location);

		AvHAIWeapon AttackWeapon = (IsPlayerAlien(pBot->Edict)) ? BotAlienChooseBestWeaponForStructure(pBot, CulpritStructure.edict) : BotMarineChooseBestWeaponForStructure(pBot, CulpritStructure.edict);

		if (GetPlayerCurrentWeapon(pBot->Player) != AttackWeapon)
		{
			pBot->DesiredMoveWeapon = AttackWeapon;
		}
		else
		{
			BotShootTarget(pBot, AttackWeapon, CulpritStructure.edict);
		}
	}
}

void BlockedMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - pBot->Edict->v.origin);

	if (vIsZero(vForward))
	{
		vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);

		if (vIsZero(vForward))
		{
			vForward = UTIL_GetForwardVector2D(pBot->Edict->v.angles);
		}
	}

	pBot->desiredMovementDir = vForward;

	Vector CurrVelocity = UTIL_GetVectorNormal2D(pBot->Edict->v.velocity);

	float Dot = UTIL_GetDotProduct2D(vForward, CurrVelocity);

	Vector FaceDir = UTIL_GetForwardVector2D(pBot->Edict->v.angles);

	float FaceDot = UTIL_GetDotProduct2D(FaceDir, vForward);

	// Yes this is cheating, but is it not cheating for humans to have millions of years of evolution
	// driving their ability to judge a jump, while the bots have a single year of coding from a moron?
	if (FaceDot < 0.95f)
	{
		float MoveSpeed = vSize2D(pBot->Edict->v.velocity);
		if (MoveSpeed < 20.0f)
		{
			MoveSpeed = 100.0f;
		}
		Vector NewVelocity = vForward * MoveSpeed;
		NewVelocity.z = pBot->Edict->v.velocity.z;

		pBot->Edict->v.velocity = NewVelocity;
	}

	BotJump(pBot);
}

void JumpMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - pBot->Edict->v.origin);

	if (vIsZero(vForward))
	{
		vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);

		if (vIsZero(vForward))
		{
			vForward = UTIL_GetForwardVector2D(pBot->Edict->v.angles);
		}
	}

	pBot->desiredMovementDir = vForward;

	Vector CurrVelocity = UTIL_GetVectorNormal2D(pBot->Edict->v.velocity);

	float Dot = UTIL_GetDotProduct2D(vForward, CurrVelocity);

	Vector FaceDir = UTIL_GetForwardVector2D(pBot->Edict->v.angles);

	float FaceDot = UTIL_GetDotProduct2D(FaceDir, vForward);

	// Yes this is cheating, but is it not cheating for humans to have millions of years of evolution
	// driving their ability to judge a jump, while the bots have a single year of coding from a moron?
	if (FaceDot < 0.95f)
	{
		float MoveSpeed = vSize2D(pBot->Edict->v.velocity);
		if (MoveSpeed < 20.0f)
		{
			MoveSpeed = 100.0f;
		}
		Vector NewVelocity = vForward * MoveSpeed;
		NewVelocity.z = pBot->Edict->v.velocity.z;

		pBot->Edict->v.velocity = NewVelocity;
	}

	BotJump(pBot);

	bool bCanDuck = (IsPlayerMarine(pBot->Edict) || IsPlayerFade(pBot->Edict) || IsPlayerOnos(pBot->Edict));

	if (!bCanDuck) { return; }

	Vector HeadLocation = GetPlayerTopOfCollisionHull(pBot->Edict, false);

	if (!UTIL_QuickTrace(pBot->Edict, HeadLocation, (HeadLocation + (pBot->desiredMovementDir * 50.0f))))
	{
		pBot->Button |= IN_DUCK;
	}
}

void LadderMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint, float RequiredClimbHeight, unsigned char NextArea)
{
	edict_t* pEdict = pBot->Edict;
	AvHPlayer* AIPlayer = pBot->Player;

	const Vector vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);

	bool bIsGoingUpLadder = (EndPoint.z > StartPoint.z);

	// Stop holding crouch if we're a skulk so we can climb up/down
	if (IsPlayerSkulk(pBot->Edict))
	{
		pBot->Button &= ~IN_DUCK;

		edict_t* Ladder = UTIL_GetNearestLadderAtPoint(StartPoint);

		if (!FNullEnt(Ladder))
		{
			Vector LadderStart = Ladder->v.absmin;
			Vector LadderEnd = Ladder->v.absmax;
			LadderEnd.z = LadderStart.z;

			// Basically, if we're directly climbing up or down the ladder, treat it like a wall. The below test will be false if the ladder is to one side
			if (vIntersects2D(StartPoint, EndPoint, LadderStart, LadderEnd))
			{
				if (bIsGoingUpLadder)
				{
					WallClimbMove(pBot, StartPoint, EndPoint, RequiredClimbHeight);
				}
				else
				{
					FallMove(pBot, StartPoint, EndPoint);
				}
				return;
			}
		}
	}

	if (IsPlayerOnLadder(pBot->Edict))
	{
		// We're on the ladder and actively climbing
		Vector CurrentLadderNormal;
		
		if (IsPlayerSkulk(pBot->Edict))
		{
			CurrentLadderNormal = UTIL_GetNearestSurfaceNormal(pBot->Edict->v.origin);
		}
		else
		{
			CurrentLadderNormal = UTIL_GetNearestLadderNormal(pBot->CollisionHullBottomLocation + Vector(0.0f, 0.0f, 5.0f));
		} 

		CurrentLadderNormal = UTIL_GetVectorNormal2D(CurrentLadderNormal);

		if (vIsZero(CurrentLadderNormal))
		{

			if (EndPoint.z > StartPoint.z)
			{
				CurrentLadderNormal = UTIL_GetVectorNormal2D(StartPoint - EndPoint);
			}
			else
			{
				CurrentLadderNormal = UTIL_GetVectorNormal2D(EndPoint - StartPoint);
			}
		}

		const Vector LadderRightNormal = UTIL_GetVectorNormal(UTIL_GetCrossProduct(CurrentLadderNormal, UP_VECTOR));

		Vector ClimbRightNormal = LadderRightNormal;

		if (bIsGoingUpLadder)
		{
			ClimbRightNormal = -LadderRightNormal;
		}
		else
		{
			if (UTIL_GetDotProduct(CurrentLadderNormal, UP_VECTOR) > 0.5f)
			{
				FallMove(pBot, StartPoint, EndPoint);
				return;
			}
		}

		if (bIsGoingUpLadder)
		{

			Vector HullTraceTo = EndPoint;
			HullTraceTo.z = pBot->CollisionHullBottomLocation.z;


			// We have reached our desired climb height and want to get off the ladder
			if ((pBot->Edict->v.origin.z >= RequiredClimbHeight) && UTIL_QuickHullTrace(pEdict, pEdict->v.origin, Vector(EndPoint.x, EndPoint.y, pEdict->v.origin.z), head_hull))
			{
				// Move directly towards the desired get-off point, looking slightly up still
				pBot->desiredMovementDir = vForward;

				Vector LookLocation = EndPoint;
				LookLocation.z = pBot->CurrentEyePosition.z + 64.0f;

				BotMoveLookAt(pBot, LookLocation);

				// If the get-off point is opposite the ladder, then jump to get to it
				if (UTIL_GetDotProduct(CurrentLadderNormal, vForward) > 0.75f)
				{
					BotJump(pBot);
				}

				if (!IsPlayerGorge(pEdict) && !IsPlayerLerk(pEdict) && !IsPlayerSkulk(pEdict))
				{
					Vector HeadTraceLocation = GetPlayerTopOfCollisionHull(pEdict, false);

					bool bHittingHead = !UTIL_QuickTrace(pBot->Edict, HeadTraceLocation, HeadTraceLocation + Vector(0.0f, 0.0f, 10.0f));
					bool bClimbIntoVent = (NextArea == SAMPLE_POLYAREA_CROUCH);

					if (!IsPlayerSkulk(pBot->Edict) && (bHittingHead || bClimbIntoVent))
					{
						pBot->Button |= IN_DUCK;
					}
				}

				return;
			}
			else
			{
				// This is for cases where the ladder physically doesn't reach the desired get-off point and the bot kind of has to "jump" up off the ladder.
				if (pBot->CollisionHullTopLocation.z >= UTIL_GetNearestLadderTopPoint(pEdict).z)
				{
					pBot->desiredMovementDir = vForward;
					// We look up really far to get maximum launch
					BotMoveLookAt(pBot, EndPoint + Vector(0.0f, 0.0f, 100.0f));
					return;
				}

				// Still climbing the ladder. Look up, and move left/right on the ladder to avoid any blockages

				Vector StartLeftTrace = pBot->CollisionHullTopLocation - (ClimbRightNormal * GetPlayerRadius(pBot->Player));
				Vector StartRightTrace = pBot->CollisionHullTopLocation + (ClimbRightNormal * GetPlayerRadius(pBot->Player));

				bool bBlockedLeft = !UTIL_QuickTrace(pEdict, StartLeftTrace, StartLeftTrace + Vector(0.0f, 0.0f, 32.0f));
				bool bBlockedRight = !UTIL_QuickTrace(pEdict, StartRightTrace, StartRightTrace + Vector(0.0f, 0.0f, 32.0f));

				// Look up at the top of the ladder

				// If we are blocked going up the ladder, face the ladder and slide left/right to avoid blockage
				if (bBlockedLeft && !bBlockedRight)
				{
					Vector LookLocation = pBot->Edict->v.origin - (CurrentLadderNormal * 50.0f);
					LookLocation.z = RequiredClimbHeight + 100.0f;
					BotMoveLookAt(pBot, LookLocation);

					pBot->desiredMovementDir = ClimbRightNormal;
					return;
				}

				if (bBlockedRight && !bBlockedLeft)
				{
					Vector LookLocation = pBot->Edict->v.origin - (CurrentLadderNormal * 50.0f);
					LookLocation.z = RequiredClimbHeight + 100.0f;
					BotMoveLookAt(pBot, LookLocation);

					pBot->desiredMovementDir = -ClimbRightNormal;
					return;
				}

				// Crouch if we're hitting our head on a ceiling
				

				if (!IsPlayerGorge(pEdict) && !IsPlayerLerk(pEdict) && !IsPlayerSkulk(pEdict))
				{
					Vector HeadTraceLocation = GetPlayerTopOfCollisionHull(pEdict, false);

					bool bHittingHead = !UTIL_QuickTrace(pBot->Edict, HeadTraceLocation, HeadTraceLocation + Vector(0.0f, 0.0f, 10.0f));

					if (bHittingHead)
					{
						pBot->Button |= IN_DUCK;
					}
				}

				// We're not blocked by anything

				// If the get-off point is to the side, look to the side and climb. Otherwise, face the ladder

				Vector LookLocation = EndPoint;

				if (!IsPlayerSkulk(pBot->Edict))
				{
					float dot = UTIL_GetDotProduct2D(vForward, LadderRightNormal);

					// Get-off point is to the side of the ladder rather than right at the top
					if (fabsf(dot) > 0.5f)
					{
						if (dot > 0.0f)
						{
							LookLocation = pBot->Edict->v.origin + (LadderRightNormal * 50.0f);
						}
						else
						{
							LookLocation = pBot->Edict->v.origin - (LadderRightNormal * 50.0f);
						}

					}
					else
					{
						// Get-off point is at the top of the ladder, so face the ladder
						LookLocation = EndPoint - (CurrentLadderNormal * 50.0f);
					}

					LookLocation.z += 100.0f;
				}
				else
				{
					// Get-off point is at the top of the ladder, so face the ladder
					LookLocation = UTIL_GetNearestLadderTopPoint(pBot->Edict->v.origin) - (CurrentLadderNormal * 50.0f);
					LookLocation.z += 100.0f;
				}

				BotMoveLookAt(pBot, LookLocation);

				if (RequiredClimbHeight > pBot->Edict->v.origin.z || IsPlayerSkulk(pBot->Edict))
				{
					pBot->desiredMovementDir = -CurrentLadderNormal;
				}
				else
				{
					pBot->desiredMovementDir = CurrentLadderNormal;
				}
			}


		}
		else
		{

			// We're going down the ladder

			Vector StartLeftTrace = pBot->CollisionHullBottomLocation - (LadderRightNormal * (GetPlayerRadius(pBot->Player) + 2.0f));
			Vector StartRightTrace = pBot->CollisionHullBottomLocation + (LadderRightNormal * (GetPlayerRadius(pBot->Player) + 2.0f));

			bool bBlockedLeft = !UTIL_QuickTrace(pEdict, StartLeftTrace, StartLeftTrace - Vector(0.0f, 0.0f, 32.0f));
			bool bBlockedRight = !UTIL_QuickTrace(pEdict, StartRightTrace, StartRightTrace - Vector(0.0f, 0.0f, 32.0f));

			if (bBlockedLeft && !bBlockedRight)
			{
				pBot->desiredMovementDir = LadderRightNormal;
				return;
			}

			if (bBlockedRight && !bBlockedLeft)
			{
				pBot->desiredMovementDir = -LadderRightNormal;
				return;
			}

			if (EndPoint.z > pBot->Edict->v.origin.z || IsPlayerSkulk(pBot->Edict))
			{
				pBot->desiredMovementDir = -CurrentLadderNormal;
			}
			else
			{
				pBot->desiredMovementDir = CurrentLadderNormal;
			}

			// We're going down the ladder, look ahead on the path or at the bottom of the ladder if we can't

			Vector LookLocation = EndPoint;

			if (!IsPlayerSkulk(pBot->Edict))
			{
				Vector FurthestView = UTIL_GetFurthestVisiblePointOnPath(pBot);

				if (vIsZero(FurthestView))
				{
					LookLocation = EndPoint + (CurrentLadderNormal * 100.0f);
				}

				// We're close enough to the end that we can jump off the ladder
				if (UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, EndPoint) && (pBot->CollisionHullBottomLocation.z - EndPoint.z < 100.0f))
				{
					BotJump(pBot);
				}
			}
			else
			{
				LookLocation = pBot->CurrentEyePosition - (CurrentLadderNormal * 50.0f);
				LookLocation.z -= 100.0f;
			}

			BotMoveLookAt(pBot, LookLocation);
		}

		return;
	}

	// We're not yet on the ladder

	// We're swimming
	if (pBot->Edict->v.flags & FL_INWATER)
	{
		Vector TargetPoint = UTIL_GetNearestLadderCentrePoint(pBot->Edict->v.origin);
		TargetPoint.z = pBot->Edict->v.origin.z;

		pBot->desiredMovementDir = UTIL_GetVectorNormal2D(TargetPoint - pBot->Edict->v.origin);
		Vector LookPoint = TargetPoint + Vector(0.0f, 0.0f, 100.0f);

		BotMoveLookAt(pBot, LookPoint);
		return;
	}

	if (!pBot->BotNavInfo.IsOnGround && !bIsGoingUpLadder)
	{
		// We're close enough to the end that we can jump off the ladder
		if (UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, EndPoint) && (pBot->CollisionHullBottomLocation.z - EndPoint.z < 100.0f))
		{
			pBot->desiredMovementDir = UTIL_GetVectorNormal2D(EndPoint - pBot->Edict->v.origin);
			return;
		}
	}

	// If we're going down the ladder and are approaching it, just keep moving towards it
	if ((pBot->BotNavInfo.IsOnGround || IsPlayerSkulk(pBot->Edict)) && !bIsGoingUpLadder)
	{
		if (vDist2DSq(pEdict->v.origin, StartPoint) < sqrf(32.0f))
		{
			pBot->BotNavInfo.bShouldWalk = true;
		}

		Vector LadderStart = UTIL_GetNearestLadderTopPoint(StartPoint);

		Vector ApproachDir = UTIL_GetVectorNormal2D(LadderStart - pBot->Edict->v.origin);
		Vector IdealApproachDir = UTIL_GetVectorNormal2D(LadderStart - StartPoint);

		float DistFromLine = vDistanceFromLine2DSq(StartPoint, LadderStart, pBot->Edict->v.origin);

		pBot->desiredMovementDir = IdealApproachDir;

		if (DistFromLine > sqrf(4.0f))
		{
			Vector vRight = UTIL_GetCrossProduct(IdealApproachDir, UP_VECTOR);

			float modifier = (float)vPointOnLine(StartPoint, LadderStart, pBot->Edict->v.origin);
			pBot->desiredMovementDir = IdealApproachDir + (vRight * modifier);
		}

		return;
	}

	Vector nearestLadderTop = UTIL_GetNearestLadderTopPoint(pEdict);

	if (bIsGoingUpLadder && ((pBot->CollisionHullTopLocation.z > EndPoint.z) || (pBot->Edict->v.origin.z > nearestLadderTop.z)))
	{
		pBot->desiredMovementDir = vForward;

		if (!UTIL_QuickHullTrace(pEdict, pEdict->v.origin, Vector(EndPoint.x, EndPoint.y, pEdict->v.origin.z), head_hull))
		{
			// Gorges can't duck, so they have to jump to get over any barrier
			if (!IsPlayerGorge(pEdict))
			{
				pBot->Button |= IN_DUCK;
			}
			else
			{
				BotJump(pBot);
			}
		}

		return;
	}
	

	if (pBot->Edict->v.origin.z < nearestLadderTop.z)
	{

		Vector nearestLadderPoint = UTIL_GetNearestLadderCentrePoint(pEdict);
		nearestLadderPoint.z = pEdict->v.origin.z;
		pBot->desiredMovementDir = UTIL_GetVectorNormal2D(nearestLadderPoint - pEdict->v.origin);

		Vector LookPoint = nearestLadderPoint + Vector(0.0f, 0.0f, 20.0f);

		BotMoveLookAt(pBot, LookPoint);
	}
}

bool UTIL_TriggerHasBeenRecentlyActivated(edict_t* TriggerEntity)
{
	return true;
}

DoorTrigger* UTIL_GetDoorTriggerByEntity(edict_t* TriggerEntity)
{
	for (auto door = NavDoors.begin(); door != NavDoors.end(); door++)
	{
		for (auto trig = door->TriggerEnts.begin(); trig != door->TriggerEnts.end(); trig++)
		{
			if (trig->Edict == TriggerEntity) { return &(*trig); }
		}
	}

	return nullptr;
}

bool HasDoorBeenTriggered(nav_door* DoorRef)
{
	if (!DoorRef) { return false; }

	for (auto it = DoorRef->TriggerEnts.begin(); it != DoorRef->TriggerEnts.end(); it++)
	{
		if (it->bIsActivated && gpGlobals->time < it->NextActivationTime)
		{
			return true;
		}
	}

	return false;
}

void NAV_ForceActivateTrigger(AvHAIPlayer* pBot, DoorTrigger* TriggerRef)
{
	if (!TriggerRef || !TriggerRef->Entity) { return; }

	

	switch (TriggerRef->TriggerType)
	{
		case DOOR_TRIGGER:
			TriggerRef->Entity->Touch(pBot->Player);
			break;
		case DOOR_USE:
		case DOOR_BUTTON:
			TriggerRef->Entity->Use(pBot->Player, pBot->Player, USE_TOGGLE, 0.0f);
			break;
		default:
			break;
	}
}

DoorTrigger* NAV_GetTriggerReachableFromLift(float LiftHeight, nav_door* Lift)
{
	DoorTrigger* Result = nullptr;

	for (auto it = Lift->TriggerEnts.begin(); it != Lift->TriggerEnts.end(); it++)
	{
		if (!it->bIsActivated) { continue; }

		Vector ClosestPointOnButton = UTIL_GetClosestPointOnEntityToLocation(UTIL_GetCentreOfEntity(Lift->DoorEdict), (*it).Edict);
		Vector ClosestPointOnLift = UTIL_GetClosestPointOnEntityToLocation(ClosestPointOnButton, Lift->DoorEdict);

		if (vDist2DSq(ClosestPointOnButton, ClosestPointOnLift) < sqrf(max_player_use_reach) && fabs(LiftHeight - ClosestPointOnButton.z) < 64.0f)
		{
			return &(*it);
		}
	}

	return nullptr;
}

void LiftMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	nav_door* NearestLift = UTIL_GetClosestLiftToPoints(StartPoint, EndPoint);

	if (!NearestLift)
	{
		GroundMove(pBot, StartPoint, EndPoint);
		return;
	}

	Vector LiftPosition = UTIL_GetCentreOfEntity(NearestLift->DoorEdict);

	pBot->desiredMovementDir = ZERO_VECTOR;

	Vector DesiredStartStop = ZERO_VECTOR;
	Vector DesiredEndStop = ZERO_VECTOR;
	float minStartDist = 0.0f;
	float minEndDist = 0.0f;

	// Find the desired stop point for us to get onto the lift
	for (auto it = NearestLift->StopPoints.begin(); it != NearestLift->StopPoints.end(); it++)
	{
		Vector LiftStopPoint = (*it) + Vector(0.0f, 0.0f, NearestLift->DoorEdict->v.size.z * 0.5f);

		float thisStartDist = vDist3DSq(LiftStopPoint, StartPoint);
		float thisEndDist = vDist3DSq(LiftStopPoint, EndPoint);
		if (vIsZero(DesiredStartStop) || thisStartDist < minStartDist)
		{
			DesiredStartStop = *it;
			minStartDist = thisStartDist;
		}

		if (vIsZero(DesiredEndStop) || thisEndDist < minEndDist)
		{
			DesiredEndStop = *it;
			minEndDist = thisEndDist;
		}
	}

	bool bIsLiftMoving = (NearestLift->DoorEdict->v.velocity.Length() > 0.0f);
	bool bIsLiftMovingToStart = bIsLiftMoving && (vDist3DSq(NearestLift->DoorEntity->m_vecFinalDest, DesiredStartStop) < sqrf(50.0f));
	bool bIsLiftMovingToEnd = bIsLiftMoving && (vDist3DSq(NearestLift->DoorEntity->m_vecFinalDest, DesiredEndStop) < sqrf(50.0f));
	bool bIsLiftAtOrNearStart = (vDist3DSq(LiftPosition, DesiredStartStop) < sqrf(50.0f));
	bool bIsLiftAtOrNearEnd = (vDist3DSq(LiftPosition, DesiredEndStop) < sqrf(50.0f));

	bool bIsOnLift = (pBot->Edict->v.groundentity == NearestLift->DoorEdict);
	bool bWaitingToEmbark = (!bIsOnLift && vDist3DSq(pBot->Edict->v.origin, StartPoint) < vDist3DSq(pBot->Edict->v.origin, EndPoint)) || (bIsOnLift && !bIsLiftMoving && bIsLiftAtOrNearStart);

	if (bIsOnLift)
	{
		// We're approaching our destination, get off
		if (bIsLiftAtOrNearEnd)
		{
			MoveToWithoutNav(pBot, EndPoint);
			return;
		}

		// Either the lift is moving, or is about to start moving
		if (bIsLiftMoving || (bIsLiftAtOrNearStart && HasDoorBeenTriggered(NearestLift)))
		{
			Vector LiftEdge = UTIL_GetClosestPointOnEntityToLocation(StartPoint, NearestLift->DoorEdict);

			bool bFullyOnLift = vDist2DSq(pBot->Edict->v.origin, LiftEdge) > sqrf(GetPlayerRadius(pBot->Player) * 2.0f);

			if (!bFullyOnLift)
			{
				pBot->desiredMovementDir = UTIL_GetVectorNormal2D(LiftPosition - pBot->Edict->v.origin);
			}

			if (!UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, pBot->CollisionHullTopLocation + Vector(0.0f, 0.0f, 64.0f)))
			{
				pBot->desiredMovementDir = UTIL_GetVectorNormal2D(LiftPosition - pBot->Edict->v.origin);
			}

			return;
		}

		// We're on the lift, but it's not about to start moving.

		
		// We're trapped half-way. Some dick must have hit the button and stopped the lift part-way
		// Activate force powers!
		if (!bIsLiftAtOrNearStart)
		{
			for (auto it = NearestLift->TriggerEnts.begin(); it != NearestLift->TriggerEnts.end(); it++)
			{
				if (it->bIsActivated)
				{
					NAV_ForceActivateTrigger(pBot, &(*it));
					return;
				}
			}
		}

		// We're not trapped, see if there is a button we can reach from where we are

		DoorTrigger* NearestTrigger = NAV_GetTriggerReachableFromLift(pBot->Edict->v.origin.z, NearestLift);

		if (NearestTrigger)
		{
			if (IsPlayerInUseRange(pBot->Edict, NearestTrigger->Edict))
			{
				BotUseObject(pBot, NearestTrigger->Edict, false);
				return;
			}
			else
			{
				pBot->desiredMovementDir = UTIL_GetVectorNormal2D(UTIL_GetCentreOfEntity(NearestTrigger->Edict) - pBot->Edict->v.origin);
			}

			return;
		}

		// Otherwise, we're able to get back to the start, so do that
		MoveToWithoutNav(pBot, StartPoint);
		return;

	}

	// if we've reached our stop, or we can directly get to the end point. Move straight there
	if (UTIL_PointIsDirectlyReachable(pBot->CollisionHullBottomLocation, EndPoint))
	{
		MoveToWithoutNav(pBot, EndPoint);
		return;
	}

	// Lift is at the embark point
	if (bIsLiftAtOrNearStart)
	{
		// Lift has been triggered or started moving, get on board before it leaves
		if (HasDoorBeenTriggered(NearestLift) || bIsLiftMovingToEnd)
		{
			if (vDistanceFromLine2D(StartPoint, LiftPosition, pBot->Edict->v.origin) > sqrf(50.0f))
			{
				NAV_SetMoveMovementTask(pBot, StartPoint, nullptr);
			}
			else
			{
				MoveToWithoutNav(pBot, LiftPosition);
			}

			
			return;
		}

		if (bIsLiftMovingToStart)
		{
			if (vDist2DSq(pBot->Edict->v.origin, StartPoint) > sqrf(100.0f))
			{
				NAV_SetMoveMovementTask(pBot, StartPoint, nullptr);
			}
			else
			{
				if (vBBOverlaps2D(pBot->Edict->v.absmin, pBot->Edict->v.absmax, NearestLift->DoorEdict->v.absmin, NearestLift->DoorEdict->v.absmax))
				{
					Vector NearestPointOnLift = UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, NearestLift->DoorEdict);
					pBot->desiredMovementDir = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - LiftPosition);
				}
			}
			return;
		}

		DoorTrigger* TriggerToActivate = NAV_GetTriggerReachableFromLift(pBot->Edict->v.origin.z, NearestLift);

		if (TriggerToActivate)
		{
			if (vDistanceFromLine2D(StartPoint, LiftPosition, pBot->Edict->v.origin) > sqrf(50.0f))
			{
				NAV_SetMoveMovementTask(pBot, StartPoint, nullptr);
			}
			else
			{
				MoveToWithoutNav(pBot, LiftPosition);
			}

			return;
		}
		
		TriggerToActivate = UTIL_GetNearestDoorTrigger(pBot->Edict->v.origin, NearestLift, nullptr, false);

		if (!TriggerToActivate)
		{
			for (auto it = NearestLift->TriggerEnts.begin(); it != NearestLift->TriggerEnts.end(); it++)
			{
				if (it->bIsActivated)
				{
					NAV_ForceActivateTrigger(pBot, &(*it));
					return;
				}
			}
		}
		else
		{
			if (TriggerToActivate->TriggerType == DOOR_BUTTON)
			{
				Vector UseLocation = UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, TriggerToActivate->Edict);

				NAV_SetUseMovementTask(pBot, TriggerToActivate->Edict, TriggerToActivate);
			}
			else if (TriggerToActivate->TriggerType == DOOR_TRIGGER)
			{
				NAV_SetTouchMovementTask(pBot, TriggerToActivate->Edict, TriggerToActivate);
			}
			else if (TriggerToActivate->TriggerType == DOOR_WELD)
			{
				NAV_SetWeldMovementTask(pBot, TriggerToActivate->Edict, TriggerToActivate);
			}
			else if (TriggerToActivate->TriggerType == DOOR_BREAK)
			{
				NAV_SetBreakMovementTask(pBot, TriggerToActivate->Edict, TriggerToActivate);
			}
		}
		
		return;
	}

	// Lift is at the end point, summon it

	// Lift is on its way
	if (HasDoorBeenTriggered(NearestLift) || bIsLiftMoving)
	{
		if (vDist2DSq(pBot->Edict->v.origin, StartPoint) > sqrf(100.0f))
		{
			NAV_SetMoveMovementTask(pBot, StartPoint, nullptr);
		}
		else
		{
			if (vBBOverlaps2D(pBot->Edict->v.absmin, pBot->Edict->v.absmax, NearestLift->DoorEdict->v.absmin, NearestLift->DoorEdict->v.absmax))
			{
				Vector NearestPointOnLift = UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, NearestLift->DoorEdict);
				pBot->desiredMovementDir = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - LiftPosition);
			}
		}
		return;
	}

	DoorTrigger* TriggerToActivate = UTIL_GetNearestDoorTrigger(pBot->Edict->v.origin, NearestLift, nullptr, false);

	if (!TriggerToActivate)
	{
		for (auto it = NearestLift->TriggerEnts.begin(); it != NearestLift->TriggerEnts.end(); it++)
		{
			if (it->bIsActivated)
			{
				NAV_ForceActivateTrigger(pBot, &(*it));
				return;
			}
		}
	}
	else
	{
		if (TriggerToActivate->TriggerType == DOOR_BUTTON)
		{
			Vector UseLocation = UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, TriggerToActivate->Edict);

			NAV_SetUseMovementTask(pBot, TriggerToActivate->Edict, TriggerToActivate);
		}
		else if (TriggerToActivate->TriggerType == DOOR_TRIGGER)
		{
			NAV_SetTouchMovementTask(pBot, TriggerToActivate->Edict, TriggerToActivate);
		}
		else if (TriggerToActivate->TriggerType == DOOR_WELD)
		{
			NAV_SetWeldMovementTask(pBot, TriggerToActivate->Edict, TriggerToActivate);
		}
		else if (TriggerToActivate->TriggerType == DOOR_BREAK)
		{
			NAV_SetBreakMovementTask(pBot, TriggerToActivate->Edict, TriggerToActivate);
		}
	}

}

void PhaseGateMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	DeployableSearchFilter PGFilter;
	PGFilter.DeployableTeam = pBot->Player->GetTeam();
	PGFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
	PGFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(2.0f);
	PGFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;

	AvHAIBuildableStructure NearestPhaseGate = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &PGFilter);

	if (!NearestPhaseGate.IsValid()) { return; }

	if (IsPlayerInUseRange(pBot->Edict, NearestPhaseGate.edict))
	{
		BotMoveLookAt(pBot, NearestPhaseGate.edict->v.origin);
		pBot->desiredMovementDir = g_vecZero;
		BotUseObject(pBot, NearestPhaseGate.edict, false);

		if (vDist2DSq(pBot->Edict->v.origin, NearestPhaseGate.edict->v.origin) < sqrf(16.0f))
		{
			pBot->desiredMovementDir = UTIL_GetForwardVector2D(NearestPhaseGate.edict->v.angles);
		}

		return;
	}
	else
	{
		pBot->desiredMovementDir = UTIL_GetVectorNormal2D(NearestPhaseGate.edict->v.origin - pBot->Edict->v.origin);
	}
}

bool IsBotOffPath(const AvHAIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		return true;
	}

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	SamplePolyFlags CurrentNavFlag = (SamplePolyFlags)CurrentPathNode.flag;
	Vector MoveFrom = CurrentPathNode.FromLocation;
	Vector MoveTo = CurrentPathNode.Location;

	Vector NextMoveLocation = ZERO_VECTOR;
	SamplePolyFlags NextMoveFlag = SAMPLE_POLYFLAGS_DISABLED;

	if (pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size() - 1)
	{
		bot_path_node NextPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

		NextMoveLocation = NextPathNode.Location;
		NextMoveFlag = (SamplePolyFlags)NextPathNode.flag;
	}


	switch (CurrentNavFlag)
	{
	case SAMPLE_POLYFLAGS_WALK:
		return IsBotOffWalkNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case SAMPLE_POLYFLAGS_WELD:
	case SAMPLE_POLYFLAGS_DOOR:
	case SAMPLE_POLYFLAGS_TEAM1STRUCTURE:
	case SAMPLE_POLYFLAGS_TEAM2STRUCTURE:
		return IsBotOffObstacleNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case SAMPLE_POLYFLAGS_LADDER:
		return IsBotOffLadderNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case SAMPLE_POLYFLAGS_FALL:
		return IsBotOffFallNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case SAMPLE_POLYFLAGS_WALLCLIMB:
		return IsBotOffClimbNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case SAMPLE_POLYFLAGS_JUMP:
	case SAMPLE_POLYFLAGS_DUCKJUMP:
	case SAMPLE_POLYFLAGS_BLOCKED:
		return IsBotOffJumpNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case SAMPLE_POLYFLAGS_TEAM1PHASEGATE:
	case SAMPLE_POLYFLAGS_TEAM2PHASEGATE:
		return IsBotOffPhaseGateNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case SAMPLE_POLYFLAGS_LIFT:
		return IsBotOffLiftNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	default:
		return IsBotOffFallNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	}

	return IsBotOffFallNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);

}

bool IsBotOffLadderNode(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	if (!IsPlayerOnLadder(pBot->Edict))
	{
		if (IsPlayerClimbingWall(pBot->Edict)) { return true; }

		if (pBot->BotNavInfo.IsOnGround)
		{
			if (!UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveStart) && !UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveEnd)) { return true; }
		}
	}

	return false;
}

bool IsBotOffWalkNode(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	if (!pBot->BotNavInfo.IsOnGround) { return false; }

	// This shouldn't happen... but does occasionally. Walk moves should always be directly reachable from start to end
	//if (!UTIL_PointIsDirectlyReachable(MoveStart, MoveEnd)) { return true; }

	Vector NearestPointOnLine = vClosestPointOnLine2D(MoveStart, MoveEnd, pBot->Edict->v.origin);

	if (vDist2DSq(pBot->Edict->v.origin, NearestPointOnLine) > sqrf(GetPlayerRadius(pBot->Edict) * 3.0f)) { return true; }
	if (vEquals2D(NearestPointOnLine, MoveStart) && !UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveStart)) { return true; }
	if (vEquals2D(NearestPointOnLine, MoveEnd) && !UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveEnd)) { return true; }

	return false;

}

bool IsBotOffFallNode(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	if (!pBot->BotNavInfo.IsOnGround) { return false; }

	Vector NearestPointOnLine = vClosestPointOnLine2D(MoveStart, MoveEnd, pBot->Edict->v.origin);

	if (vDist2DSq(pBot->Edict->v.origin, NearestPointOnLine) > sqrf(GetPlayerRadius(pBot->Edict) * 3.0f)) { return true; }

	if (!FNullEnt(pBot->Edict->v.groundentity))
	{
		nav_door* Door = UTIL_GetNavDoorByEdict(pBot->Edict->v.groundentity);

		if (Door) { return false; }
	}

	if (!UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveStart) && !UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveEnd)) { return true; }

	return false;
}

bool IsBotOffClimbNode(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	if (!IsPlayerClimbingWall(pBot->Edict) && (pBot->Edict->v.flags & FL_ONGROUND))
	{
		return (!UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveStart) && !UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveEnd));
	}

	Vector ClosestPointOnLine = vClosestPointOnLine2D(MoveStart, MoveEnd, pBot->Edict->v.origin);

	return vDist2DSq(pBot->Edict->v.origin, ClosestPointOnLine) > sqrf(GetPlayerRadius(pBot->Edict) * 3.0f);
}

bool IsBotOffJumpNode(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	if (!pBot->BotNavInfo.IsOnGround) { return false; }

	Vector ClosestPointOnLine = vClosestPointOnLine2D(MoveStart, MoveEnd, pBot->Edict->v.origin);

	if (vEquals2D(ClosestPointOnLine, MoveStart) || vEquals2D(ClosestPointOnLine, MoveEnd))
	{
		return (!UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveStart) && !UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveEnd));
	}

	return vDist2DSq(pBot->Edict->v.origin, ClosestPointOnLine) > sqrf(GetPlayerRadius(pBot->Edict) * 2.0f);
}

bool IsBotOffPhaseGateNode(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	if (vDist2DSq(pBot->Edict->v.origin, MoveStart) > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)) && vDist2DSq(pBot->Edict->v.origin, MoveEnd) > sqrf(UTIL_MetresToGoldSrcUnits(2.0f))) { return true; }

	DeployableSearchFilter PGFilter;
	PGFilter.DeployableTeam = pBot->Player->GetTeam();
	PGFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	PGFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	PGFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(2.0f);

	bool StartPGExists = AITAC_DeployableExistsAtLocation(MoveStart, &PGFilter);

	if (!StartPGExists) { return true; }
	
	bool EndPGExists = AITAC_DeployableExistsAtLocation(MoveEnd, &PGFilter);

	if (!EndPGExists) { return true; }

	return false;
}

bool IsBotOffLiftNode(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	return false;
}

bool IsBotOffObstacleNode(const AvHAIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, SamplePolyFlags NextMoveFlag)
{
	return IsBotOffJumpNode(pBot, MoveStart, MoveEnd, NextMoveDestination, NextMoveFlag);
}

void BlinkClimbMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint, float RequiredClimbHeight)
{
	edict_t* pEdict = pBot->Edict;

	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);
	Vector CheckLine = StartPoint + (vForward * 1000.0f);
	Vector MoveDir = UTIL_GetVectorNormal2D(EndPoint - pBot->Edict->v.origin);

	Vector PointOnMove = vClosestPointOnLine2D(StartPoint, EndPoint, pEdict->v.origin);
	float DistFromLineSq = vDist2DSq(PointOnMove, pEdict->v.origin);

	if (vEquals(PointOnMove, StartPoint, 2.0f) && DistFromLineSq > sqrf(8.0f))
	{
		pBot->desiredMovementDir = UTIL_GetVectorNormal2D(StartPoint - pBot->Edict->v.origin);
		return;
	}

	pBot->desiredMovementDir = MoveDir;

	// Always duck. It doesn't have any downsides and means we don't have to separately handle vent climbing
	pBot->Button |= IN_DUCK;

	pBot->DesiredMoveWeapon = WEAPON_FADE_BLINK;

	// Wait until we have blink equipped before proceeding
	if (GetPlayerCurrentWeapon(pBot->Player) != WEAPON_FADE_BLINK) { return; }

	// Only blink if we're below the target climb height
	if (pEdict->v.origin.z < RequiredClimbHeight + 4.0f && !UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, Vector(EndPoint.x, EndPoint.y, pBot->Edict->v.origin.z)))
	{
		Vector CurrVelocity = UTIL_GetVectorNormal2D(pBot->Edict->v.velocity);

		float Dot = UTIL_GetDotProduct2D(MoveDir, CurrVelocity);

		Vector FaceDir = UTIL_GetForwardVector2D(pEdict->v.angles);

		float FaceDot = UTIL_GetDotProduct2D(FaceDir, MoveDir);

		// Yes this is cheating, but the fades were struggling with zipping off-target when trying to blink
		// Better this than fades getting constantly chewed up by marines because they can't escape properly
		if (FaceDot < 0.95f)
		{
			float MoveSpeed = vSize2D(pBot->Edict->v.velocity);

			if (MoveSpeed < 20.0f)
			{
				MoveSpeed = 100.0f;
			}

			Vector NewVelocity = MoveDir * MoveSpeed;
			NewVelocity.z = pBot->Edict->v.velocity.z;

			pBot->Edict->v.velocity = NewVelocity;
		}

		float ZDiff = fabs(pEdict->v.origin.z - (RequiredClimbHeight + 16.0f));

		// We don't want to blast off like a rocket, so only apply enough blink until our upwards velocity is enough to carry us to the desired height
		float DesiredZVelocity = sqrtf(2.0f * GOLDSRC_GRAVITY * (ZDiff + 10.0f));

		if (pBot->Edict->v.velocity.z < DesiredZVelocity)
		{
			bool bHasHeadroom = UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, pBot->Edict->v.origin + Vector(0.0f, 0.0f, 4.0f));
			// We're going to cheat and give the bot the necessary energy to make the move. Better the fade cheats a bit than gets stuck somewhere
			if (GetPlayerEnergy(pBot->Edict) < 0.1f)
			{
				pBot->Player->Energize(0.1f);
			}

			if (!bHasHeadroom || pBot->Edict->v.origin.z >= RequiredClimbHeight)
			{
				Vector LookPoint = EndPoint;
				LookPoint.z = pBot->CurrentEyePosition.z + 5.0f;
				BotMoveLookAt(pBot, LookPoint);
			}
			else
			{
				BotMoveLookAt(pBot, EndPoint + Vector(0.0f, 0.0f, 100.0f));
			}

			
			pBot->Button |= IN_ATTACK2;
		}
		else
		{
			Vector LookAtTarget = EndPoint;
			LookAtTarget.z = pBot->CurrentEyePosition.z;
			BotMoveLookAt(pBot, LookAtTarget);
		}
	}
}

void WallClimbMove(AvHAIPlayer* pBot, const Vector StartPoint, const Vector EndPoint, float RequiredClimbHeight)
{
	edict_t* pEdict = pBot->Edict;

	if (UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, EndPoint))
	{
		Vector PointOnMoveLine = vClosestPointOnLine2D(StartPoint, EndPoint, pBot->Edict->v.origin);

		if (vEquals2D(PointOnMoveLine, EndPoint, 4.0f))
		{

			// Stop holding crouch if we're a skulk so we can actually climb
			if (IsPlayerSkulk(pBot->Edict))
			{
				pBot->Button &= ~IN_DUCK;
			}

			pBot->desiredMovementDir = UTIL_GetVectorNormal2D(EndPoint - pBot->CurrentFloorPosition);

			return;
		}
	}

	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);
	Vector vRight = UTIL_GetVectorNormal(UTIL_GetCrossProduct(vForward, UP_VECTOR));

	pBot->desiredMovementDir = vForward;

	Vector CheckLine = StartPoint + (vForward * 1000.0f);

	float DistFromLine = vDistanceFromLine2D(StartPoint, CheckLine, pEdict->v.origin);

	// Draw an imaginary 2D line between from and to movement, and make sure we're aligned. If we've drifted off to one side, readjust.
	if (DistFromLine > 18.0f)
	{
		float modifier = (float)vPointOnLine(StartPoint, CheckLine, pEdict->v.origin);

		pBot->desiredMovementDir = UTIL_GetVectorNormal2D(pBot->desiredMovementDir + (vRight * modifier));
	}

	// Stop holding crouch if we're a skulk so we can actually climb
	if (IsPlayerSkulk(pBot->Edict))
	{
		pBot->Button &= ~IN_DUCK;
	}



	float ZDiff = fabs(pEdict->v.origin.z - RequiredClimbHeight);
	Vector AdjustedTargetLocation = EndPoint + (UTIL_GetVectorNormal2D(EndPoint - StartPoint) * 1000.0f);
	Vector DirectAheadView = pBot->CurrentEyePosition + (UTIL_GetVectorNormal2D(AdjustedTargetLocation - pBot->CurrentEyePosition) * 100.0f);

	Vector ClimbSurfaceNormal = UTIL_GetVectorNormal(EndPoint - StartPoint);

	Vector LookLocation = g_vecZero;

	if (ZDiff < 1.0f)
	{
		LookLocation = DirectAheadView;
	}
	else
	{
		// Don't look up/down quite so much as we reach the desired height so we slow down a bit, reduces the chance of over-shooting and climbing right over a vent
		if (pEdict->v.origin.z > RequiredClimbHeight)
		{
			if (ZDiff > 32.0f)
			{
				ClimbSurfaceNormal = ClimbSurfaceNormal - (2.0f * (UTIL_GetDotProduct(ClimbSurfaceNormal, UP_VECTOR) * ClimbSurfaceNormal));
				LookLocation = pBot->CurrentEyePosition + (ClimbSurfaceNormal * 100.0f);
			}
			else
			{
				LookLocation = DirectAheadView - Vector(0.0f, 0.0f, 20.0f);
			}
		}
		else
		{
			ClimbSurfaceNormal = ClimbSurfaceNormal;
			LookLocation = pBot->CurrentEyePosition + (ClimbSurfaceNormal * 100.0f);
		}
	}

	if (IsPlayerClimbingWall(pBot->Edict))
	{
		Vector RightDir = UTIL_GetCrossProduct(vForward, UP_VECTOR);

		Vector LeftCheckStart = pBot->Edict->v.origin - (RightDir * (GetPlayerRadius(pBot->Player) + 2.0f));
		Vector LeftCheckEnd = LeftCheckStart + Vector(0.0f, 0.0f, 50.0f);

		Vector RightCheckStart = pBot->Edict->v.origin + (RightDir * (GetPlayerRadius(pBot->Player) + 2.0f));
		Vector RightCheckEnd = RightCheckStart + Vector(0.0f, 0.0f, 50.0f);

		if (!UTIL_QuickTrace(pBot->Edict, LeftCheckStart, LeftCheckEnd))
		{
			if (UTIL_QuickTrace(pBot->Edict, RightCheckStart, RightCheckEnd))
			{
				pBot->desiredMovementDir = UTIL_GetVectorNormal2D(vForward + RightDir);
			}
		}
		else if (!UTIL_QuickTrace(pBot->Edict, RightCheckStart, RightCheckEnd))
		{
			pBot->desiredMovementDir = UTIL_GetVectorNormal2D(vForward - RightDir);
		}
	}

	BotMoveLookAt(pBot, LookLocation);

}

void MoveToWithoutNav(AvHAIPlayer* pBot, const Vector Destination)
{
	Vector CurrentPos = (pBot->BotNavInfo.IsOnGround) ? pBot->Edict->v.origin : pBot->CurrentFloorPosition;

	const Vector vForward = UTIL_GetVectorNormal2D(Destination - CurrentPos);
	// Same goes for the right vector, might not be the same as the bot's right
	const Vector vRight = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(vForward, UP_VECTOR));

	const float PlayerRadius = GetPlayerRadius(pBot->Player);

	Vector stTrcLft = pBot->Edict->v.origin - (vRight * PlayerRadius);
	Vector stTrcRt = pBot->Edict->v.origin + (vRight * PlayerRadius);
	stTrcLft.z += 2.0f;
	stTrcRt.z += 2.0f;

	Vector endTrcLft = stTrcLft + (vForward * (PlayerRadius * 2.0f));
	Vector endTrcRt = stTrcRt + (vForward * (PlayerRadius * 2.0f));

	bool bumpLeft = !UTIL_QuickHullTrace(pBot->Edict, stTrcLft, endTrcLft, head_hull);
	bool bumpRight = !UTIL_QuickHullTrace(pBot->Edict, stTrcRt, endTrcRt, head_hull);

	pBot->desiredMovementDir = vForward;

	if (bumpRight && !bumpLeft)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
	}
	else if (bumpLeft && !bumpRight)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
	}
	else if (bumpLeft && bumpRight)
	{
		endTrcLft = endTrcLft - (vRight * PlayerRadius);
		endTrcRt = endTrcRt + (vRight * PlayerRadius);

		if (!UTIL_QuickTrace(pBot->Edict, stTrcLft, endTrcLft))
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
		}
		else
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
		}
	}

	float DistFromDestination = vDist2DSq(pBot->Edict->v.origin, Destination);

	if (vIsZero(pBot->LookTargetLocation))
	{
		Vector LookTarget = Destination;

		if (DistFromDestination < sqrf(200.0f))
		{
			Vector LookNormal = UTIL_GetVectorNormal2D(LookTarget - pBot->CurrentEyePosition);

			LookTarget = LookTarget + (LookNormal * 1000.0f);
		}

		BotLookAt(pBot, LookTarget);
	}

	HandlePlayerAvoidance(pBot, Destination);
	BotMovementInputs(pBot);
}

void MoveDirectlyTo(AvHAIPlayer* pBot, const Vector Destination)
{
	pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;

	if (vIsZero(Destination)) { return; }

	Vector CurrentPos = (pBot->BotNavInfo.IsOnGround) ? pBot->Edict->v.origin : pBot->CurrentFloorPosition;

	const Vector vForward = UTIL_GetVectorNormal2D(Destination - CurrentPos);
	// Same goes for the right vector, might not be the same as the bot's right
	const Vector vRight = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(vForward, UP_VECTOR));

	const float PlayerRadius = GetPlayerRadius(pBot->Player);

	Vector stTrcLft = CurrentPos - (vRight * PlayerRadius);
	Vector stTrcRt = CurrentPos + (vRight * PlayerRadius);
	Vector endTrcLft = stTrcLft + (vForward * 24.0f);
	Vector endTrcRt = stTrcRt + (vForward * 24.0f);

	const bool bumpLeft = !UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, stTrcLft, endTrcLft);
	const bool bumpRight = !UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, stTrcRt, endTrcRt);

	pBot->desiredMovementDir = vForward;

	if (bumpRight && !bumpLeft)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
	}
	else if (bumpLeft && !bumpRight)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
	}
	else if (bumpLeft && bumpRight)
	{
		stTrcLft.z = pBot->Edict->v.origin.z;
		stTrcRt.z = pBot->Edict->v.origin.z;
		endTrcLft.z = pBot->Edict->v.origin.z;
		endTrcRt.z = pBot->Edict->v.origin.z;

		if (!UTIL_QuickTrace(pBot->Edict, stTrcLft, endTrcLft))
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
		}
		else
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
		}
	}

	float DistFromDestination = vDist2DSq(pBot->Edict->v.origin, Destination);

	if (vIsZero(pBot->LookTargetLocation))
	{
		Vector LookTarget = Destination;

		if (DistFromDestination < sqrf(200.0f))
		{
			Vector LookNormal = UTIL_GetVectorNormal2D(LookTarget - pBot->CurrentEyePosition);

			LookTarget = LookTarget + (LookNormal * 1000.0f);
		}

		BotLookAt(pBot, LookTarget);
	}

	HandlePlayerAvoidance(pBot, Destination);
	BotMovementInputs(pBot);

}


bool UTIL_PointIsDirectlyReachable(const AvHAIPlayer* pBot, const Vector targetPoint)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	if (!m_navQuery) { return false; }

	edict_t* pEdict = pBot->Edict;

	Vector CurrentPos = (pBot->BotNavInfo.IsOnGround) ? pBot->Edict->v.origin : pBot->CurrentFloorPosition;

	float pStartPos[3] = { CurrentPos.x, CurrentPos.z, -CurrentPos.y };
	float pEndPos[3] = { targetPoint.x, targetPoint.z, -targetPoint.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3];
	float EndNearest[3];

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;


	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_navFilter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_navFilter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_navFilter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f) { return false; }

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);

}

bool UTIL_PointIsDirectlyReachable(const AvHAIPlayer* pBot, const Vector start, const Vector target)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	if (!m_navQuery) { return false; }

	if (vIsZero(start) || vIsZero(target)) { return false; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3];
	float EndNearest[3];

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;

	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_navFilter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_navFilter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_navFilter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f) { return false; }

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);

}

const dtNavMesh* UTIL_GetNavMeshForProfile(const nav_profile& NavProfile)
{
	if (NavProfile.NavMeshIndex < 0 || NavProfile.NavMeshIndex >= MAX_NAV_MESHES) { return nullptr; }

	return NavMeshes[NavProfile.NavMeshIndex].navMesh;
}

const dtNavMeshQuery* UTIL_GetNavMeshQueryForProfile(const nav_profile& NavProfile)
{
	if (NavProfile.NavMeshIndex < 0 || NavProfile.NavMeshIndex >= MAX_NAV_MESHES) { return nullptr; }

	return NavMeshes[NavProfile.NavMeshIndex].navQuery;
}

const dtTileCache* UTIL_GetTileCacheForProfile(const nav_profile& NavProfile)
{
	if (NavProfile.NavMeshIndex < 0 || NavProfile.NavMeshIndex >= MAX_NAV_MESHES) { return nullptr; }

	return NavMeshes[NavProfile.NavMeshIndex].tileCache;
}

bool UTIL_PointIsDirectlyReachable(const nav_profile &NavProfile, const Vector start, const Vector target)
{
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navMesh) { return false; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;


	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_navFilter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_navFilter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_navFilter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f)
	{
		if (pathCount == 0) { return false; }

		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[pathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(max_ai_use_reach))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);
}

bool UTIL_TraceNav(const nav_profile &NavProfile, const Vector start, const Vector target, const float MaxAcceptableDistance)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_Filter = &NavProfile.Filters;

	if (!m_navQuery) { return false; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;

	float MaxReachableExtents[3] = { MaxAcceptableDistance, 50.0f, MaxAcceptableDistance };

	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, MaxReachableExtents, m_Filter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, MaxReachableExtents, m_Filter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_Filter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f)
	{
		if (pathCount == 0) { return false; }

		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[pathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(MaxAcceptableDistance))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);
}

void UTIL_TraceNavLine(const nav_profile &NavProfile, const Vector Start, const Vector End, nav_hitresult* HitResult)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_Filter = &NavProfile.Filters;

	if (!m_navQuery)
	{
		HitResult->flFraction = 0.0f;
		HitResult->bStartOffMesh = true;
		HitResult->TraceEndPoint = Start;
		return;
	}

	float pStartPos[3] = { Start.x, Start.z, -Start.y };
	float pEndPos[3] = { End.x, End.z, -End.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;

	float MaxReachableExtents[3] = { 18.0f, 32.0f, 18.0f };

	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, MaxReachableExtents, m_Filter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		HitResult->flFraction = 0.0f;
		HitResult->bStartOffMesh = true;
		HitResult->TraceEndPoint = Start;
		return;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, MaxReachableExtents, m_Filter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		HitResult->flFraction = 0.0f;
		HitResult->bStartOffMesh = true;
		HitResult->TraceEndPoint = Start;
		return;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly)
	{

		HitResult->flFraction = 1.0f;
		HitResult->bStartOffMesh = false;
		HitResult->TraceEndPoint = Vector(EndNearest[0], -EndNearest[2], EndNearest[1]);
		return;
	}

	

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_Filter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	HitResult->flFraction = hitDist;
	HitResult->bStartOffMesh = false;

	Vector HitLocation = g_vecZero;

	if (hitDist >= 1.0f)
	{
		HitLocation = Vector(EndNearest[0], -EndNearest[2], EndNearest[1]);
	}
	else
	{
		Vector Dir = UTIL_GetVectorNormal(End - Start);
		Vector Point = Start + (Dir * HitResult->flFraction);

		HitLocation = UTIL_ProjectPointToNavmesh(Point, Vector(100.0f, 100.0f, 100.0f), NavProfile);
	}

	HitResult->TraceEndPoint = HitLocation;
}

bool UTIL_PointIsDirectlyReachable(const Vector start, const Vector target)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtQueryFilter* m_Filter = &BaseNavProfiles[ALL_NAV_PROFILE].Filters;

	if (!m_navQuery) { return false; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;


	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_Filter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_Filter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_Filter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f)
	{
		if (pathCount == 0) { return false; }

		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[pathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(max_ai_use_reach))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);
}

float UTIL_PointIsDirectlyReachable_DEBUG(const Vector start, const Vector target)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtQueryFilter* m_Filter = &BaseNavProfiles[ALL_NAV_PROFILE].Filters;

	if (!m_navQuery) { return 0.0f; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3];
	float EndNearest[3];

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;


	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_Filter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return 1.1f;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_Filter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return 1.2f;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return 2.1f; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_Filter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);

	float Dist = dtVdistSqr(EndNearest, ClosestPoint);

	if (hitDist < 1.0f)
	{
		if (pathCount == 0) { return 1.3f; }

		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[pathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(max_ai_use_reach))
		{
			return 1.5f;
		}
		else
		{
			return 2.4f;
		}


		return 1.3f;
	}

	if (EndPoly != PolyPath[pathCount - 1])
	{
		if (Height == 0.0f || Height == EndNearest[1])
		{
			return 2.3f;
		}
		return 1.4f;
	}

	return 2.2f;
}

dtPolyRef UTIL_GetNearestPolyRefForLocation(const nav_profile& NavProfile, const Vector Location)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return 0; }

	float ConvertedFloorCoords[3] = { Location.x, Location.z, -Location.y };

	float pPolySearchExtents[3] = { 50.0f, 50.0f, 50.0f };

	dtPolyRef result;
	float nearestPoint[3] = { 0.0f, 0.0f, 0.0f };

	m_navQuery->findNearestPoly(ConvertedFloorCoords, pPolySearchExtents, m_navFilter, &result, nearestPoint);

	return result;
}

dtPolyRef UTIL_GetNearestPolyRefForLocation(const Vector Location)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtQueryFilter* m_navFilter = &BaseNavProfiles[ALL_NAV_PROFILE].Filters;

	if (!m_navQuery) { return 0; }

	float ConvertedFloorCoords[3] = { Location.x, Location.z, -Location.y };

	float pPolySearchExtents[3] = { 50.0f, 50.0f, 50.0f };

	dtPolyRef result;
	float nearestPoint[3] = { 0.0f, 0.0f, 0.0f };

	m_navQuery->findNearestPoly(ConvertedFloorCoords, pPolySearchExtents, m_navFilter, &result, nearestPoint);

	return result;
}

dtPolyRef UTIL_GetNearestPolyRefForEntity(const edict_t* Edict)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtQueryFilter* m_navFilter = &BaseNavProfiles[ALL_NAV_PROFILE].Filters;

	if (!m_navQuery) { return 0; }

	Vector Floor = UTIL_GetFloorUnderEntity(Edict);

	float ConvertedFloorCoords[3] = { Floor.x, Floor.z, -Floor.y };

	float pPolySearchExtents[3] = { 50.0f, 50.0f, 50.0f };

	dtPolyRef result;
	float nearestPoint[3] = { 0.0f, 0.0f, 0.0f };

	m_navQuery->findNearestPoly(ConvertedFloorCoords, pPolySearchExtents, m_navFilter, &result, nearestPoint);

	return result;
}

unsigned char UTIL_GetNavAreaAtLocation(const nav_profile &NavProfile, const Vector Location)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return (unsigned char)SAMPLE_POLYAREA_BLOCKED; }

	Vector TraceHit = UTIL_GetTraceHitLocation(Location + Vector(0.0f, 0.0f, 10.0f), Location - Vector(0.0f, 0.0f, 500.0f));

	Vector PointToProject = (TraceHit != g_vecZero) ? TraceHit : Location;

	float pCheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, pReachableExtents, m_navFilter, &FoundPoly, NavNearest);

	if (dtStatusSucceed(success))
	{
		unsigned char area = 0;
		m_navMesh->getPolyArea(FoundPoly, &area);
		return area;
	}
	else
	{
		return (unsigned char)SAMPLE_POLYAREA_BLOCKED;
	}
}

unsigned char UTIL_GetNavAreaAtLocation(const Vector Location)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtQueryFilter* m_navFilter = &BaseNavProfiles[ALL_NAV_PROFILE].Filters;

	if (!m_navQuery) { return 0; }

	Vector TraceHit = UTIL_GetTraceHitLocation(Location + Vector(0.0f, 0.0f, 10.0f), Location - Vector(0.0f, 0.0f, 500.0f));

	Vector PointToProject = (TraceHit != g_vecZero) ? TraceHit : Location;

	float pCheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, pReachableExtents, m_navFilter, &FoundPoly, NavNearest);

	if (dtStatusSucceed(success))
	{
		unsigned char area = 0;
		m_navMesh->getPolyArea(FoundPoly, &area);
		return area;
	}
	else
	{
		return 0;
	}
}

const char* UTIL_NavmeshAreaToChar(const unsigned char Area)
{
	switch (Area)
	{
	case SAMPLE_POLYAREA_BLOCKED:
		return "Blocked";
	case SAMPLE_POLYAREA_CROUCH:
		return "Crouch";
	case SAMPLE_POLYAREA_GROUND:
		return "Ground";
	default:
		return "INVALID";

	}

	return "INVALID";
}

void UTIL_UpdateBotMovementStatus(AvHAIPlayer* pBot)
{
	if (pBot->Edict->v.movetype != pBot->BotNavInfo.CurrentMoveType)
	{
		if (pBot->BotNavInfo.CurrentMoveType == MOVETYPE_FLY)
		{
			OnBotEndLadder(pBot);
		}


		if (pBot->Edict->v.movetype == MOVETYPE_FLY)
		{
			OnBotStartLadder(pBot);
		}

		pBot->BotNavInfo.CurrentMoveType = pBot->Edict->v.movetype;
	}

	pBot->BotNavInfo.CurrentPoly = UTIL_GetNearestPolyRefForEntity(pBot->Edict);

	pBot->CollisionHullBottomLocation = GetPlayerBottomOfCollisionHull(pBot->Edict);
	pBot->CollisionHullTopLocation = GetPlayerTopOfCollisionHull(pBot->Edict);
}


bool AbortCurrentMove(AvHAIPlayer* pBot, const Vector NewDestination)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size() || pBot->BotNavInfo.NavProfile.bFlyingProfile) { return true; }

	if (IsBotOffPath(pBot) || HasBotReachedPathPoint(pBot)) { return true; }

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	Vector MoveFrom = CurrentPathNode.FromLocation;
	Vector MoveTo = CurrentPathNode.Location;
	unsigned int flag = CurrentPathNode.flag;

	if (flag == SAMPLE_POLYFLAGS_TEAM1PHASEGATE || flag == SAMPLE_POLYFLAGS_TEAM2PHASEGATE || flag == SAMPLE_POLYFLAGS_TEAM1STRUCTURE || flag == SAMPLE_POLYFLAGS_TEAM2STRUCTURE) { return true; }

	Vector ClosestPointOnLine = vClosestPointOnLine2D(MoveFrom, MoveTo, pBot->Edict->v.origin);

	bool bAtOrPastMovement = (vEquals2D(ClosestPointOnLine, MoveFrom, 1.0f) && fabsf(pBot->Edict->v.origin.z - MoveFrom.z) <= 50.0f ) || (vEquals2D(ClosestPointOnLine, MoveTo, 1.0f) && fabsf(pBot->Edict->v.origin.z - MoveTo.z) <= 50.0f) ;

	if ((pBot->Edict->v.flags & FL_ONGROUND) && bAtOrPastMovement)
	{
		return true;
	}

	Vector DestinationPointOnLine = vClosestPointOnLine(MoveFrom, MoveTo, NewDestination);

	bool bReverseCourse = (vDist3DSq(DestinationPointOnLine, MoveFrom) < vDist3DSq(DestinationPointOnLine, MoveTo));

	if (flag == SAMPLE_POLYFLAGS_LIFT)
	{
		if (pBot->BotNavInfo.MovementTask.TaskType != MOVE_TASK_NONE && !vEquals(NewDestination, pBot->BotNavInfo.MovementTask.TaskLocation))
		{
			if (NAV_IsMovementTaskStillValid(pBot))
			{
				NAV_ProgressMovementTask(pBot);
				return false;
			}
			else
			{
				NAV_ClearMovementTask(pBot);
				ClearBotPath(pBot);
				return true;
			}
		}

		if (bReverseCourse)
		{
			if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveFrom)) { return true; }
			LiftMove(pBot, MoveTo, MoveFrom);
		}
		else
		{
			if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveTo)) { return true; }
			LiftMove(pBot, MoveFrom, MoveTo);
		}
	}

	if (flag == SAMPLE_POLYFLAGS_WALK || flag == SAMPLE_POLYFLAGS_WELD || flag == SAMPLE_POLYFLAGS_DOOR)
	{
		if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveFrom) || UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveTo))
		{
			return true;
		}

		if (bReverseCourse)
		{
			GroundMove(pBot, MoveTo, MoveFrom);
		}
		else
		{
			GroundMove(pBot, MoveFrom, MoveTo);
		}
	}

	if (flag == SAMPLE_POLYFLAGS_WALLCLIMB)
	{
		if (bReverseCourse)
		{
			FallMove(pBot, MoveTo, MoveFrom);
		}
		else
		{
			if (PlayerHasWeapon(pBot->Player, WEAPON_FADE_BLINK))
			{
				BlinkClimbMove(pBot, MoveFrom, MoveTo, CurrentPathNode.requiredZ);
			}
			else
			{
				WallClimbMove(pBot, MoveFrom, MoveTo, CurrentPathNode.requiredZ);
			}
			
		}
	}

	if (flag == SAMPLE_POLYFLAGS_LADDER)
	{
		if (bReverseCourse)
		{

			LadderMove(pBot, MoveTo, MoveFrom, CurrentPathNode.requiredZ, (unsigned char)SAMPLE_POLYAREA_CROUCH);

			// We're going DOWN the ladder
			if (MoveTo.z > MoveFrom.z)
			{
				if (pBot->Edict->v.origin.z - MoveFrom.z < 150.0f)
				{
					BotJump(pBot);
				}
			}
		}
		else
		{

			LadderMove(pBot, MoveFrom, MoveTo, CurrentPathNode.requiredZ, (unsigned char)SAMPLE_POLYAREA_CROUCH);

			// We're going DOWN the ladder
			if (MoveFrom.z > MoveTo.z)
			{
				if (pBot->Edict->v.origin.z - MoveFrom.z < 150.0f)
				{
					BotJump(pBot);
				}
			}
		}
	}

	if (flag == SAMPLE_POLYFLAGS_TEAM1PHASEGATE || flag == SAMPLE_POLYFLAGS_TEAM2PHASEGATE)
	{
		return true;
	}

	if (flag == SAMPLE_POLYFLAGS_JUMP || flag == SAMPLE_POLYFLAGS_DUCKJUMP || flag == SAMPLE_POLYFLAGS_BLOCKED)
	{
		if (bReverseCourse)
		{
			JumpMove(pBot, MoveTo, MoveFrom);
		}
		else
		{
			JumpMove(pBot, MoveFrom, MoveTo);
		}
	}

	if (flag == SAMPLE_POLYFLAGS_FALL)
	{
		FallMove(pBot, MoveFrom, MoveTo);
	}

	BotMovementInputs(pBot);

	return false;
}

void UpdateBotStuck(AvHAIPlayer* pBot)
{
	if (vIsZero(pBot->desiredMovementDir) && !pBot->BotNavInfo.StuckInfo.bPathFollowFailed)
	{
		return;
	}

	if (!pBot->BotNavInfo.StuckInfo.bPathFollowFailed)
	{

		bool bIsFollowingPath = (pBot->BotNavInfo.CurrentPath.size() > 0 && pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size());

		bool bDist3D = pBot->BotNavInfo.NavProfile.bFlyingProfile || (bIsFollowingPath && (pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint].flag == SAMPLE_POLYFLAGS_LADDER || pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint].flag == SAMPLE_POLYFLAGS_WALLCLIMB));

		float DistFromLastPoint = (bDist3D) ? vDist3DSq(pBot->Edict->v.origin, pBot->BotNavInfo.StuckInfo.LastBotPosition) : vDist2DSq(pBot->Edict->v.origin, pBot->BotNavInfo.StuckInfo.LastBotPosition);

		if (DistFromLastPoint >= sqrf(8.0f))
		{
			pBot->BotNavInfo.StuckInfo.TotalStuckTime = 0.0f;
			pBot->BotNavInfo.StuckInfo.LastBotPosition = pBot->Edict->v.origin;
		}
		else
		{
			pBot->BotNavInfo.StuckInfo.TotalStuckTime += pBot->ThinkDelta;
		}
	}
	else
	{
		pBot->BotNavInfo.StuckInfo.TotalStuckTime += pBot->ThinkDelta;
	}

	if (pBot->BotNavInfo.StuckInfo.TotalStuckTime > 0.25f)
	{
		if (pBot->BotNavInfo.StuckInfo.TotalStuckTime > CONFIG_GetMaxStuckTime())
		{
			BotSuicide(pBot);
			return;
		}

		if (pBot->BotNavInfo.StuckInfo.TotalStuckTime > 5.0f)
		{
			if (pBot->BotNavInfo.MovementTask.TaskType != MOVE_TASK_NONE)
			{
				NAV_ClearMovementTask(pBot);
			}

			ClearBotPath(pBot);
		}

		if (!vIsZero(pBot->desiredMovementDir))
		{
			edict_t* BlockingEntity = UTIL_TraceEntity(pBot->Edict, pBot->Edict->v.origin, (pBot->Edict->v.origin + pBot->desiredMovementDir * 50.0f));

			if (IsEdictStructure(BlockingEntity))
			{
				pBot->desiredMovementDir = UTIL_GetVectorNormal2D(pBot->desiredMovementDir + UTIL_GetCrossProduct(pBot->desiredMovementDir, UP_VECTOR));

				BotMovementInputs(pBot);
			}

			BotJump(pBot);

			if (!IsPlayerSkulk(pBot->Edict))
			{
				pBot->Button |= IN_DUCK;
			}
			else
			{
				pBot->Button &= ~IN_DUCK;
			}
		}		
	}
}

void SetBaseNavProfile(AvHAIPlayer* pBot)
{
	pBot->BotNavInfo.bNavProfileChanged = true;

	if (IsPlayerMarine(pBot->Player))
	{
		memcpy(&pBot->BotNavInfo.NavProfile, &BaseNavProfiles[MARINE_BASE_NAV_PROFILE], sizeof(nav_profile));
		return;
	}

	switch (pBot->Edict->v.iuser3)
	{
	case AVH_USER3_ALIEN_PLAYER1:
		memcpy(&pBot->BotNavInfo.NavProfile, &BaseNavProfiles[SKULK_BASE_NAV_PROFILE], sizeof(nav_profile));
		return;
	case AVH_USER3_ALIEN_PLAYER2:
		memcpy(&pBot->BotNavInfo.NavProfile, &BaseNavProfiles[GORGE_BASE_NAV_PROFILE], sizeof(nav_profile));
		return;
	case AVH_USER3_ALIEN_PLAYER3:
		memcpy(&pBot->BotNavInfo.NavProfile, &BaseNavProfiles[LERK_BASE_NAV_PROFILE], sizeof(nav_profile));
		return;
	case AVH_USER3_ALIEN_PLAYER4:
		memcpy(&pBot->BotNavInfo.NavProfile, &BaseNavProfiles[FADE_BASE_NAV_PROFILE], sizeof(nav_profile));
		return;
	case AVH_USER3_ALIEN_PLAYER5:
		memcpy(&pBot->BotNavInfo.NavProfile, &BaseNavProfiles[ONOS_BASE_NAV_PROFILE], sizeof(nav_profile));
		return;

	}
}

void UpdateBotMoveProfile(AvHAIPlayer* pBot, BotMoveStyle MoveStyle)
{
	switch (pBot->Edict->v.iuser3)
	{
		case AVH_USER3_MARINE_PLAYER:
			MarineUpdateBotMoveProfile(pBot, MoveStyle);
			break;
		case AVH_USER3_ALIEN_PLAYER1:
			SkulkUpdateBotMoveProfile(pBot, MoveStyle);
			break;
		case AVH_USER3_ALIEN_PLAYER2:
			GorgeUpdateBotMoveProfile(pBot, MoveStyle);
			break;
		case AVH_USER3_ALIEN_PLAYER3:
			LerkUpdateBotMoveProfile(pBot, MoveStyle);
			break;
		case AVH_USER3_ALIEN_PLAYER4:
			FadeUpdateBotMoveProfile(pBot, MoveStyle);
			break;
		case AVH_USER3_ALIEN_PLAYER5:
			OnosUpdateBotMoveProfile(pBot, MoveStyle);
			break;
	}

	if (pBot->Player->GetTeam() == GetGameRules()->GetTeamANumber())
	{
		pBot->BotNavInfo.NavProfile.Filters.removeExcludeFlags(SAMPLE_POLYFLAGS_TEAM2STRUCTURE);
		pBot->BotNavInfo.NavProfile.Filters.addExcludeFlags(SAMPLE_POLYFLAGS_TEAM1STRUCTURE);
		pBot->BotNavInfo.NavProfile.Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_TEAM1STRUCTURE);
		pBot->BotNavInfo.NavProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_TEAM2STRUCTURE);
	}
	else
	{
		pBot->BotNavInfo.NavProfile.Filters.removeExcludeFlags(SAMPLE_POLYFLAGS_TEAM1STRUCTURE);
		pBot->BotNavInfo.NavProfile.Filters.addExcludeFlags(SAMPLE_POLYFLAGS_TEAM2STRUCTURE);
		pBot->BotNavInfo.NavProfile.Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_TEAM2STRUCTURE);
		pBot->BotNavInfo.NavProfile.Filters.addIncludeFlags(SAMPLE_POLYFLAGS_TEAM1STRUCTURE);
	}

}

void MarineUpdateBotMoveProfile(AvHAIPlayer* pBot, BotMoveStyle MoveStyle)
{
	nav_profile* NavProfile = &pBot->BotNavInfo.NavProfile;

	bool bHasWelder = PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER);

	if (!bHasWelder)
	{
		AvHAIDroppedItem* NearbyWelder = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_WELDER, pBot->Player->GetTeam(), pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);

		bHasWelder = (NearbyWelder != nullptr);
	}

	// Did our nav profile previously indicate we could go through weldable doors?
	bool bHadWelder = (NavProfile->Filters.getIncludeFlags() & SAMPLE_POLYFLAGS_WELD);

	if (bHasWelder != bHadWelder)
	{
		pBot->BotNavInfo.bNavProfileChanged = true;

		if (bHasWelder)
		{
			NavProfile->Filters.addIncludeFlags(SAMPLE_POLYFLAGS_WELD);
			NavProfile->ReachabilityFlag = AI_REACHABILITY_WELDER;
		}
		else
		{
			NavProfile->Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_WELD);
			NavProfile->ReachabilityFlag = AI_REACHABILITY_MARINE;
		}
	}

	SamplePolyFlags ExcludePhaseGateFlag = (pBot->Player->GetTeam() == GetGameRules()->GetTeamANumber()) ? SAMPLE_POLYFLAGS_TEAM2PHASEGATE : SAMPLE_POLYFLAGS_TEAM1PHASEGATE;
	SamplePolyFlags IncludePhaseGateFlag = (ExcludePhaseGateFlag & SAMPLE_POLYFLAGS_TEAM1PHASEGATE) ? SAMPLE_POLYFLAGS_TEAM2PHASEGATE : SAMPLE_POLYFLAGS_TEAM1PHASEGATE;

	if (!(NavProfile->Filters.getIncludeFlags() & IncludePhaseGateFlag))
	{
		pBot->BotNavInfo.bNavProfileChanged = true;

		NavProfile->Filters.addIncludeFlags(IncludePhaseGateFlag);
		NavProfile->Filters.removeIncludeFlags(ExcludePhaseGateFlag);
	}

	if (MoveStyle == pBot->BotNavInfo.PreviousMoveStyle) { return; }

	pBot->BotNavInfo.PreviousMoveStyle = MoveStyle;
	pBot->BotNavInfo.bNavProfileChanged = true;
	pBot->BotNavInfo.MoveStyle = MoveStyle;

	if (MoveStyle == MOVESTYLE_NORMAL)
	{
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 2.0f);
		return;
	}

	if (MoveStyle == MOVESTYLE_HIDE || MoveStyle == MOVESTYLE_AMBUSH)
	{
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 3.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
		return;
	}
}

void SkulkUpdateBotMoveProfile(AvHAIPlayer* pBot, BotMoveStyle MoveStyle)
{
	if (MoveStyle == pBot->BotNavInfo.PreviousMoveStyle) { return; }

	pBot->BotNavInfo.MoveStyle = MoveStyle;
	pBot->BotNavInfo.PreviousMoveStyle = MoveStyle;

	pBot->BotNavInfo.bNavProfileChanged = true;	

	nav_profile* NavProfile = &pBot->BotNavInfo.NavProfile;

	if (MoveStyle == MOVESTYLE_NORMAL)
	{
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
		return;
	}

	if (MoveStyle == MOVESTYLE_HIDE || MoveStyle == MOVESTYLE_AMBUSH)
	{
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 10.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_WALLCLIMB, 1.0f);
		return;
	}
}

void GorgeUpdateBotMoveProfile(AvHAIPlayer* pBot, BotMoveStyle MoveStyle)
{
	if (MoveStyle == pBot->BotNavInfo.PreviousMoveStyle) { return; }

	pBot->BotNavInfo.PreviousMoveStyle = MoveStyle;

	pBot->BotNavInfo.bNavProfileChanged = true;
	pBot->BotNavInfo.MoveStyle = MoveStyle;

	nav_profile* NavProfile = &pBot->BotNavInfo.NavProfile;

	if (MoveStyle == MOVESTYLE_NORMAL)
	{
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
		return;
	}

	if (MoveStyle == MOVESTYLE_HIDE || MoveStyle == MOVESTYLE_AMBUSH)
	{
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 10.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
		return;
	}
}

void LerkUpdateBotMoveProfile(AvHAIPlayer* pBot, BotMoveStyle MoveStyle)
{
	if (MoveStyle == pBot->BotNavInfo.PreviousMoveStyle) { return; }

	pBot->BotNavInfo.PreviousMoveStyle = MoveStyle;

	pBot->BotNavInfo.bNavProfileChanged = true;
	pBot->BotNavInfo.MoveStyle = MoveStyle;

	nav_profile* NavProfile = &pBot->BotNavInfo.NavProfile;

	if (MoveStyle == MOVESTYLE_NORMAL)
	{
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
		NavProfile->bFlyingProfile = true;
		return;
	}

	if (MoveStyle == MOVESTYLE_HIDE || MoveStyle == MOVESTYLE_AMBUSH)
	{
		NavProfile->bFlyingProfile = false;
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 10.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
		return;
	}
}

void FadeUpdateBotMoveProfile(AvHAIPlayer* pBot, BotMoveStyle MoveStyle)
{
	if (MoveStyle == pBot->BotNavInfo.PreviousMoveStyle) { return; }

	pBot->BotNavInfo.PreviousMoveStyle = MoveStyle;

	pBot->BotNavInfo.bNavProfileChanged = true;
	pBot->BotNavInfo.MoveStyle = MoveStyle;

	nav_profile* NavProfile = &pBot->BotNavInfo.NavProfile;

	if (MoveStyle == MOVESTYLE_NORMAL)
	{
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
		return;
	}

	if (MoveStyle == MOVESTYLE_HIDE || MoveStyle == MOVESTYLE_AMBUSH)
	{
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_GROUND, 10.0f);
		NavProfile->Filters.setAreaCost(SAMPLE_POLYAREA_CROUCH, 1.0f);
		return;
	}
}

void OnosUpdateBotMoveProfile(AvHAIPlayer* pBot, BotMoveStyle MoveStyle)
{
	// Onos doesn't really do much other than the usual movement
	return;
}

bool NAV_MergeAndUpdatePath(AvHAIPlayer* pBot, std::vector<bot_path_node>& NewPath)
{
	if (pBot->BotNavInfo.NavProfile.bFlyingProfile || pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		pBot->BotNavInfo.CurrentPath.clear();
		pBot->BotNavInfo.CurrentPath.insert(pBot->BotNavInfo.CurrentPath.end(), NewPath.begin(), NewPath.end());
		pBot->BotNavInfo.CurrentPathPoint = 0;
		return true;
	}

	std::vector<bot_path_node>::iterator OldPathStart = (pBot->BotNavInfo.CurrentPath.begin() + pBot->BotNavInfo.CurrentPathPoint);
	std::vector<bot_path_node>::iterator OldPathEnd;
	std::vector<bot_path_node>::iterator NewPathStart;

	for (OldPathEnd = OldPathStart; OldPathEnd != pBot->BotNavInfo.CurrentPath.end(); OldPathEnd++)
	{
		if (OldPathEnd->flag != SAMPLE_POLYFLAGS_WALK)
		{
			break;
		}
	}

	if (OldPathEnd == pBot->BotNavInfo.CurrentPath.end())
	{
		return false;
	}

	for (NewPathStart = NewPath.begin(); NewPathStart != NewPath.end(); NewPathStart++)
	{
		if (NewPathStart->flag != SAMPLE_POLYFLAGS_WALK)
		{
			break;
		}
	}

	if (NewPathStart == NewPath.end())
	{
		return false;
	}

	if (!vEquals(OldPathEnd->FromLocation, NewPathStart->FromLocation, 16.0f) || !vEquals(OldPathEnd->Location, NewPathStart->Location, 16.0f))
	{
		return false;
	}

	OldPathEnd = next(OldPathEnd);
	NewPathStart = next(NewPathStart);

	for (auto it = OldPathEnd; it != pBot->BotNavInfo.CurrentPath.end();)
	{
		it = pBot->BotNavInfo.CurrentPath.erase(it);
	}

	pBot->BotNavInfo.CurrentPath.insert(pBot->BotNavInfo.CurrentPath.end(), NewPathStart, NewPath.end());
	return true;
}

bool NAV_GenerateNewBasePath(AvHAIPlayer* pBot, const Vector NewDestination, const BotMoveStyle MoveStyle, const float MaxAcceptableDist)
{
	nav_status* BotNavInfo = &pBot->BotNavInfo;

	dtStatus PathFindingStatus = DT_FAILURE;

	vector<bot_path_node> PendingPath;
	bool bIsFlyingProfile = BotNavInfo->NavProfile.bFlyingProfile;
	

	if (bIsFlyingProfile)
	{
		PathFindingStatus = FindFlightPathToPoint(BotNavInfo->NavProfile, pBot->CurrentFloorPosition, NewDestination, PendingPath, MaxAcceptableDist);
	}
	else
	{
		Vector NavAdjustedDestination = AdjustPointForPathfinding(NewDestination);
		if (vIsZero(NavAdjustedDestination)) { return false; }

		PathFindingStatus = FindPathClosestToPoint(pBot, BotNavInfo->MoveStyle, NavAdjustedDestination, PendingPath, MaxAcceptableDist);
	}

	BotNavInfo->NextForceRecalc = 0.0f;
	BotNavInfo->bNavProfileChanged = false;

	if (dtStatusSucceed(PathFindingStatus))
	{
		if (!NAV_MergeAndUpdatePath(pBot, PendingPath))
		{
			if (!AbortCurrentMove(pBot, NewDestination))
			{
				return true;
			}
			else
			{
				ClearBotPath(pBot);
				NAV_ClearMovementTask(pBot);
				pBot->BotNavInfo.CurrentPath.insert(pBot->BotNavInfo.CurrentPath.begin(), PendingPath.begin(), PendingPath.end());
				BotNavInfo->CurrentPathPoint = 0;
			}
		}

		BotNavInfo->ActualMoveDestination = BotNavInfo->CurrentPath.back().Location;
		BotNavInfo->TargetDestination = NewDestination;

		pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;
		ClearBotStuckMovement(pBot);
		pBot->BotNavInfo.TotalStuckTime = 0.0f;
		BotNavInfo->PathDestination = NewDestination;

		return true;
	}

	return false;
}

bool NAV_GenerateNewMoveTaskPath(AvHAIPlayer* pBot, const Vector NewDestination, const BotMoveStyle MoveStyle)
{
	nav_status* BotNavInfo = &pBot->BotNavInfo;

	dtStatus PathFindingStatus = DT_FAILURE;

	vector<bot_path_node> PendingPath;
	bool bIsFlyingProfile = BotNavInfo->NavProfile.bFlyingProfile;

	if (bIsFlyingProfile)
	{
		PathFindingStatus = FindFlightPathToPoint(BotNavInfo->NavProfile, pBot->CurrentFloorPosition, NewDestination, PendingPath, max_player_use_reach);
	}
	else
	{
		Vector NavAdjustedDestination = AdjustPointForPathfinding(NewDestination);
		if (vIsZero(NavAdjustedDestination)) { return false; }

		PathFindingStatus = FindPathClosestToPoint(pBot, BotNavInfo->MoveStyle, NavAdjustedDestination, PendingPath, max_player_use_reach);
	}

	if (dtStatusSucceed(PathFindingStatus))
	{
		pBot->BotNavInfo.CurrentPath.clear();
		pBot->BotNavInfo.CurrentPathPoint = 0;
		pBot->BotNavInfo.SpecialMovementFlags = 0;

		pBot->BotNavInfo.CurrentPath.insert(pBot->BotNavInfo.CurrentPath.begin(), PendingPath.begin(), PendingPath.end());
		BotNavInfo->CurrentPathPoint = 0;

		pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;
		ClearBotStuckMovement(pBot);
		pBot->BotNavInfo.TotalStuckTime = 0.0f;
		BotNavInfo->PathDestination = NewDestination;

		return true;
	}

	return false;
}

bool MoveTo(AvHAIPlayer* pBot, const Vector Destination, const BotMoveStyle MoveStyle, const float MaxAcceptableDist)
{
	// Trying to move nowhere, or our current location. Do nothing
	if (vIsZero(Destination) || (vDist2D(pBot->Edict->v.origin, Destination) <= 6.0f && (fabs(pBot->CollisionHullBottomLocation.z - Destination.z) < 50.0f)))
	{
		pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;
		ClearBotMovement(pBot);

		return true; 
	}

	nav_status* BotNavInfo = &pBot->BotNavInfo;

	pBot->BotNavInfo.MoveStyle = MoveStyle;
	UTIL_UpdateBotMovementStatus(pBot);

	UpdateBotMoveProfile(pBot, MoveStyle);

	bool bIsFlyingProfile = pBot->BotNavInfo.NavProfile.bFlyingProfile;
	bool bNavProfileChanged = pBot->BotNavInfo.bNavProfileChanged;
	bool bForceRecalculation = (pBot->BotNavInfo.NextForceRecalc > 0.0f && gpGlobals->time >= pBot->BotNavInfo.NextForceRecalc);
	bool bIsPerformingMoveTask = (BotNavInfo->MovementTask.TaskType != MOVE_TASK_NONE && vEquals(Destination, BotNavInfo->MovementTask.TaskLocation, GetPlayerRadius(pBot->Player)));
	bool bEndGoalChanged = (!vEquals(Destination, BotNavInfo->TargetDestination, GetPlayerRadius(pBot->Player)) && !bIsPerformingMoveTask);
	
	bool bShouldGenerateMainPath = (bEndGoalChanged || bNavProfileChanged || bForceRecalculation);
	bool bShouldGenerateMoveTaskPath = (bIsPerformingMoveTask && !vEquals(BotNavInfo->PathDestination, BotNavInfo->MovementTask.TaskLocation, GetPlayerRadius(pBot->Player)));

	if (bShouldGenerateMainPath || bShouldGenerateMoveTaskPath)
	{
		if (!bIsFlyingProfile && !pBot->BotNavInfo.IsOnGround && !IsPlayerClimbingWall(pBot->Edict))
		{
			if (pBot->BotNavInfo.CurrentPath.size() > 0)
			{
				BotFollowPath(pBot);
			}
			return true;
		}

		if (bShouldGenerateMainPath)
		{
			bool bSucceeded = NAV_GenerateNewBasePath(pBot, Destination, MoveStyle, MaxAcceptableDist);

			if (!bSucceeded)
			{				
				pBot->BotNavInfo.StuckInfo.bPathFollowFailed = true;

				if (!UTIL_PointIsOnNavmesh(pBot->CollisionHullBottomLocation, pBot->BotNavInfo.NavProfile))
				{
					if (!vIsZero(BotNavInfo->LastNavMeshPosition))
					{
						MoveDirectlyTo(pBot, BotNavInfo->LastNavMeshPosition);

						if (vDist2DSq(pBot->CurrentFloorPosition, BotNavInfo->LastNavMeshPosition) < sqrf(8.0f))
						{
							BotNavInfo->LastNavMeshPosition = g_vecZero;
						}

						return false;
					}
					else
					{
						if (!vIsZero(BotNavInfo->UnstuckMoveLocation) && vDist2DSq(pBot->CurrentFloorPosition, BotNavInfo->UnstuckMoveLocation) < sqrf(8.0f))
						{
							BotNavInfo->UnstuckMoveLocation = ZERO_VECTOR;
						}

						if (vIsZero(BotNavInfo->UnstuckMoveLocation))
						{
							BotNavInfo->UnstuckMoveLocation = FindClosestPointBackOnPath(pBot, Destination);
						}

						if (!vIsZero(BotNavInfo->UnstuckMoveLocation))
						{
							MoveDirectlyTo(pBot, BotNavInfo->UnstuckMoveLocation);
							return false;
						}
					}
				}
				else
				{
					MoveDirectlyTo(pBot, Destination);
					return false;
				}

				return false;
			}
		}
		else
		{
			bool bSucceeded = NAV_GenerateNewMoveTaskPath(pBot, BotNavInfo->MovementTask.TaskLocation, MoveStyle);

			if (!bSucceeded)
			{
				if (!FNullEnt(BotNavInfo->MovementTask.TaskTarget))
				{
					CBaseEntity* TargetEntity = CBaseEntity::Instance(BotNavInfo->MovementTask.TaskTarget);

					if (!TargetEntity) { return false; }

					switch (BotNavInfo->MovementTask.TaskType)
					{
						case MOVE_TASK_TOUCH:
							TargetEntity->Touch(pBot->Player);
							return true;
							break;
						case MOVE_TASK_USE:
							TargetEntity->Use(pBot->Player, pBot->Player, USE_TOGGLE, 0.0f);
							return true;
							break;
						default:
							break;

					}
				}
			}
		}
		
	}

	if (!bIsPerformingMoveTask && BotNavInfo->MovementTask.TaskType != MOVE_TASK_NONE)
	{
		if (NAV_IsMovementTaskStillValid(pBot))
		{
			NAV_ProgressMovementTask(pBot);
			return true;
		}
		else
		{
			NAV_ClearMovementTask(pBot);
			ClearBotPath(pBot);
			return true;
		}
	}

	if (BotNavInfo->CurrentPath.size() > 0)
	{
		// If this path requires use of a welder and we don't have one, then find one
		if ((pBot->BotNavInfo.SpecialMovementFlags & SAMPLE_POLYFLAGS_WELD) && !PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
		{
			if (pBot->BotNavInfo.MovementTask.TaskType != MOVE_TASK_PICKUP)
			{
				nav_profile BaseProfile = GetBaseNavProfile(MARINE_BASE_NAV_PROFILE);

				AvHAIDroppedItem* NearestWelder = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_WELDER, pBot->Player->GetTeam(), BaseProfile.ReachabilityFlag, 0.0f, 0.0f, true);

				if (NearestWelder)
				{
					NAV_SetPickupMovementTask(pBot, NearestWelder->edict, nullptr);
					return true;
				}
			}
		}

		if (pBot->Edict->v.flags & FL_INWATER)
		{
			BotFollowSwimPath(pBot);
		}
		else
		{
			if (bIsFlyingProfile)
			{
				BotFollowFlightPath(pBot, true);
			}
			else
			{
				BotFollowPath(pBot);
			}
		}

		// Check to ensure BotFollowFlightPath or BotFollowPath haven't cleared the path (will happen if reached end of path)
		if (BotNavInfo->CurrentPathPoint < BotNavInfo->CurrentPath.size())
		{
			HandlePlayerAvoidance(pBot, BotNavInfo->CurrentPath[BotNavInfo->CurrentPathPoint].Location);
			BotMovementInputs(pBot);
		}

		return true;
	}

	return false;

}

Vector FindClosestPointBackOnPath(AvHAIPlayer* pBot, Vector Destination)
{
	Vector GeneralMoveDir = UTIL_GetVectorNormal2D(Destination - pBot->Edict->v.origin);
	Vector CheckDir = pBot->Edict->v.origin + (GeneralMoveDir * 16.0f);

	Vector ValidNavmeshPoint = AdjustPointForPathfinding(CheckDir);

	if (vIsZero(ValidNavmeshPoint))
	{
		DeployableSearchFilter ResNodeFilter;
		ResNodeFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

		AvHAIResourceNode* NearestResNode = AITAC_FindNearestResourceNodeToLocation(pBot->Edict->v.origin, &ResNodeFilter);

		Vector ValidNavmeshPoint = AITAC_GetTeamStartingLocation(pBot->Player->GetTeam());

		if (NearestResNode && vDist2D(pBot->Edict->v.origin, NearestResNode->Location) < vDist2D(pBot->Edict->v.origin, ValidNavmeshPoint))
		{
			ValidNavmeshPoint = NearestResNode->Location;
		}

		ValidNavmeshPoint = UTIL_ProjectPointToNavmesh(ValidNavmeshPoint, pBot->BotNavInfo.NavProfile);

		if (vIsZero(ValidNavmeshPoint))
		{
			return g_vecZero;
		}
	}

	vector<bot_path_node> BackwardsPath;
	BackwardsPath.clear();

	// Now we find a path backwards from the valid nav mesh point to our location, trying to get as close as we can to it

	dtStatus BackwardFindingStatus = FindPathClosestToPoint(pBot->BotNavInfo.NavProfile, ValidNavmeshPoint, pBot->CurrentFloorPosition, BackwardsPath, UTIL_MetresToGoldSrcUnits(50.0f));

	if (dtStatusSucceed(BackwardFindingStatus))
	{

		Vector NewMoveLocation = prev(BackwardsPath.end())->Location;
		Vector NewMoveFromLocation = prev(BackwardsPath.end())->FromLocation;

		for (auto it = BackwardsPath.rbegin(); it != BackwardsPath.rend(); it++)
		{
			if (vDist2DSq(pBot->Edict->v.origin, it->Location) > sqrf(GetPlayerRadius(pBot->Edict)) && UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, it->Location))
			{
				NewMoveLocation = it->Location;
				NewMoveFromLocation = it->FromLocation;
				break;
			}
		}

		if (!vIsZero(NewMoveLocation))
		{
			if (vDist2DSq(pBot->Edict->v.origin, NewMoveLocation) < sqrf(GetPlayerRadius(pBot->Player)))
			{
				NewMoveLocation = NewMoveLocation - (UTIL_GetVectorNormal2D(NewMoveLocation - NewMoveFromLocation) * 100.0f);
			}
		}

		return NewMoveLocation;
	}

	return g_vecZero;
}

Vector FindClosestNavigablePointToDestination(const nav_profile& NavProfile, const Vector FromLocation, const Vector ToLocation, float MaxAcceptableDistance)
{
	vector<bot_path_node> Path;
	Path.clear();

	// Now we find a path backwards from the valid nav mesh point to our location, trying to get as close as we can to it

	dtStatus PathFindingResult = FindPathClosestToPoint(NavProfile, FromLocation, ToLocation, Path, MaxAcceptableDistance);

	if (dtStatusSucceed(PathFindingResult) && Path.size() > 0)
	{
		return Path.back().Location;
	}

	return g_vecZero;
}

void SkipAheadInFlightPath(AvHAIPlayer* pBot)
{
	nav_status* BotNavInfo = &pBot->BotNavInfo;

	// Early exit if we don't have a path, or we're already on the last path point
	if (BotNavInfo->CurrentPath.size() == 0 || BotNavInfo->CurrentPathPoint >= (pBot->BotNavInfo.CurrentPath.size() - 1)) { return; }

	vector<bot_path_node>::iterator CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

	if (UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, prev(BotNavInfo->CurrentPath.end())->Location, head_hull, false))
	{
		pBot->BotNavInfo.CurrentPathPoint = (BotNavInfo->CurrentPath.size() - 1);
		BotNavInfo->CurrentPath[pBot->BotNavInfo.CurrentPathPoint].FromLocation = pBot->Edict->v.origin;
		return;
	}

	// If we are approaching a low area or lift, don't try to skip ahead in case it screws us up
	if (CurrentPathPoint->area == SAMPLE_POLYAREA_CROUCH
		|| CurrentPathPoint->area == SAMPLE_POLYAREA_LIFT
		|| (next(CurrentPathPoint) != BotNavInfo->CurrentPath.end() && (next(CurrentPathPoint)->area == SAMPLE_POLYAREA_CROUCH || next(CurrentPathPoint)->area == SAMPLE_POLYAREA_LIFT))) { return; }

	for (auto it = prev(BotNavInfo->CurrentPath.end()); it != next(CurrentPathPoint); it--)
	{
		if (it->area == SAMPLE_POLYAREA_CROUCH) { continue; }

		Vector NextFlightPoint = UTIL_FindHighestSuccessfulTracePoint(pBot->Edict->v.origin, it->FromLocation, it->Location, 5.0f, 50.0f, 200.0f);

		// If we can directly reach the end point, set our path point to the end of the path and go for it
		if (!vIsZero(NextFlightPoint))
		{
			it->FromLocation = NextFlightPoint;

			pBot->BotNavInfo.CurrentPathPoint = distance(BotNavInfo->CurrentPath.begin(), prev(it));
			CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

			CurrentPathPoint->FromLocation = pBot->Edict->v.origin;
			CurrentPathPoint->Location = it->FromLocation;

			

			return;
		}
	}
}

LerkFlightBehaviour BotFlightWalkMove(AvHAIPlayer* pBot, Vector FromLocation, Vector ToLocation, SamplePolyFlags MoveFlag, SamplePolyAreas MoveArea)
{
	BotMoveLookAt(pBot, ToLocation);

	Vector ThisMoveDir = UTIL_GetVectorNormal(ToLocation - pBot->Edict->v.origin);

	if (vDist3DSq(pBot->Edict->v.origin, ToLocation) < sqrf(4.0f))
	{
		ThisMoveDir = UTIL_GetVectorNormal(ToLocation - FromLocation);
	}

	bool bNeedsDrop = false;
	bool bBlockedTopLeft = false;
	bool bBlockedBottomLeft = false;
	bool bBlockedTopRight = false;
	bool bBlockedBottomRight = false;

	Vector RightVector = UTIL_GetCrossProduct(ThisMoveDir, UP_VECTOR).Normalize();

	if (!pBot->BotNavInfo.IsOnGround)
	{
		TraceResult hit;

		float PlayerRadius = GetPlayerRadius(pBot->Edict);

		Vector TopLeft = pBot->CollisionHullTopLocation - (RightVector * PlayerRadius);
		Vector TopRight = pBot->CollisionHullTopLocation + (RightVector * PlayerRadius);
		Vector BottomLeft = pBot->CollisionHullBottomLocation - (RightVector * PlayerRadius);
		Vector BottomRight = pBot->CollisionHullBottomLocation + (RightVector * PlayerRadius);

		UTIL_TraceLine(TopLeft, TopLeft + (ThisMoveDir * (PlayerRadius * 1.5f)), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

		if (hit.flFraction < 1.0f || hit.fAllSolid > 0 || hit.fStartSolid > 0)
		{
			ThisMoveDir = ThisMoveDir + RightVector - UP_VECTOR;
			ThisMoveDir.Normalize();
			bBlockedTopLeft = true;
		}

		UTIL_TraceLine(TopRight, TopRight + (ThisMoveDir * (PlayerRadius * 1.5f)), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

		if (hit.flFraction < 1.0f || hit.fAllSolid > 0 || hit.fStartSolid > 0)
		{
			ThisMoveDir = ThisMoveDir - RightVector - UP_VECTOR;
			ThisMoveDir.Normalize();
			bBlockedTopRight = true;
		}

		UTIL_TraceLine(BottomLeft, BottomLeft + (ThisMoveDir * (PlayerRadius * 1.5f)), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

		if (hit.flFraction < 1.0f || hit.fAllSolid > 0 || hit.fStartSolid > 0)
		{
			ThisMoveDir = ThisMoveDir + RightVector + UP_VECTOR;
			ThisMoveDir.Normalize();
			bBlockedBottomLeft = true;
		}

		UTIL_TraceLine(BottomRight, BottomRight + (ThisMoveDir * (PlayerRadius * 1.5f)), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

		if (hit.flFraction < 1.0f || hit.fAllSolid > 0 || hit.fStartSolid > 0)
		{
			ThisMoveDir = ThisMoveDir + UP_VECTOR - RightVector;
			ThisMoveDir.Normalize();
			bBlockedBottomRight = true;
		}

	}

	if (bBlockedTopLeft && bBlockedBottomLeft)
	{
		ThisMoveDir = RightVector;
	}
	else if (bBlockedTopRight && bBlockedBottomRight)
	{
		ThisMoveDir = -RightVector;
	}
	else if (bBlockedBottomLeft && bBlockedBottomRight && (!bBlockedTopLeft || !bBlockedTopRight))
	{
		ThisMoveDir = ThisMoveDir + UP_VECTOR;
		ThisMoveDir.Normalize();
	}
	else
	{
		Vector ClosestPoint = vClosestPointOnLine(FromLocation, ToLocation, pBot->Edict->v.origin);

		if (vDist3DSq(pBot->Edict->v.origin, ClosestPoint) > sqrf(8.0f))
		{
			ThisMoveDir = UTIL_GetVectorNormal(ThisMoveDir + UTIL_GetVectorNormal(ClosestPoint - pBot->Edict->v.origin));
		}
	}

	if (MoveArea != SAMPLE_POLYAREA_CROUCH && !bBlockedTopLeft && !bBlockedTopRight)
	{
		float CurrentSpeed = pBot->Edict->v.velocity.Length();

		Vector NewVelocity = ThisMoveDir * fmaxf(CurrentSpeed, 100.0f);

		pBot->Edict->v.velocity = NewVelocity;
	}

	pBot->desiredMovementDir = UTIL_GetVectorNormal2D(ThisMoveDir);

	if (bBlockedTopLeft || bBlockedTopRight) { return FLIGHT_DROP; }

	return (vSize3D(pBot->Edict->v.velocity) < 500.0f && GetPlayerEnergy(pBot->Edict) > 0.1f) ? FLIGHT_FLAP : FLIGHT_GLIDE;
}

LerkFlightBehaviour BotFlightFallMove(AvHAIPlayer* pBot, Vector FromLocation, Vector ToLocation)
{
	Vector LookLocation = ToLocation;

	BotMoveLookAt(pBot, LookLocation);

	pBot->desiredMovementDir = UTIL_GetVectorNormal2D(ToLocation - FromLocation);

	return FLIGHT_DROP;
}

LerkFlightBehaviour BotFlightClimbMove(AvHAIPlayer* pBot, Vector FromLocation, Vector ToLocation, float RequiredZ)
{

	Vector LookLocation = ToLocation;

	pBot->desiredMovementDir = UTIL_GetVectorNormal2D(ToLocation - pBot->Edict->v.origin);

	if (pBot->Edict->v.origin.z < (RequiredZ - 4.0f))
	{
		LookLocation = pBot->Edict->v.origin + (pBot->desiredMovementDir * 32.0f);
		LookLocation.z += 50.0f;

		BotMoveLookAt(pBot, LookLocation);

		Vector CurrentDir = UTIL_GetVectorNormal2D(pBot->Edict->v.velocity);
		Vector DesiredDir = pBot->desiredMovementDir;

		if (UTIL_GetDotProduct2D(CurrentDir, DesiredDir) < 0.5f)
		{
			float CurrentXYSpeed = pBot->Edict->v.velocity.Length2D();

			Vector NewVelocity = DesiredDir * CurrentXYSpeed;
			NewVelocity.z = pBot->Edict->v.velocity.z;

			pBot->Edict->v.velocity = NewVelocity;
		}

		return FLIGHT_FLAP;
	}
	else
	{
		BotMoveLookAt(pBot, LookLocation);

		float PlayerRadius = GetPlayerRadius(pBot->Edict);

		bool bBlockedTopLeft = false;
		bool bBlockedTopRight = false;
		bool bBlockedBottomLeft = false;
		bool bBlockedBottomRight = false;

		Vector ClimbDir = UTIL_GetVectorNormal(ToLocation - FromLocation);

		Vector RightVector = UTIL_GetCrossProduct(UTIL_GetVectorNormal2D(ClimbDir), UP_VECTOR).Normalize();

		Vector TopLeft = pBot->CollisionHullTopLocation - (RightVector * PlayerRadius);
		Vector TopRight = pBot->CollisionHullTopLocation + (RightVector * PlayerRadius);
		Vector BottomLeft = pBot->CollisionHullBottomLocation - (RightVector * PlayerRadius);
		Vector BottomRight = pBot->CollisionHullBottomLocation + (RightVector * PlayerRadius);

		TraceResult hit;

		UTIL_TraceLine(TopLeft, TopLeft + (pBot->desiredMovementDir * (PlayerRadius * 1.5f)), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

		if (hit.flFraction < 1.0f || hit.fAllSolid || hit.fStartSolid)
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir + RightVector - UP_VECTOR;
			pBot->desiredMovementDir.Normalize();
			bBlockedTopLeft = true;
		}

		UTIL_TraceLine(TopRight, TopRight + (pBot->desiredMovementDir * (PlayerRadius * 1.5f)), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

		if (hit.flFraction < 1.0f || hit.fAllSolid || hit.fStartSolid)
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir - RightVector - UP_VECTOR;
			pBot->desiredMovementDir.Normalize();
			bBlockedTopRight = true;
		}

		UTIL_TraceLine(BottomLeft, BottomLeft + (pBot->desiredMovementDir * (PlayerRadius * 1.5f)), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

		if (hit.flFraction < 1.0f || hit.fAllSolid || hit.fStartSolid)
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir + RightVector + UP_VECTOR;
			pBot->desiredMovementDir.Normalize();
			bBlockedBottomLeft = true;
		}

		UTIL_TraceLine(BottomRight, BottomRight + (pBot->desiredMovementDir * (PlayerRadius * 1.5f)), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

		if (hit.flFraction < 1.0f || hit.fAllSolid || hit.fStartSolid)
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir + UP_VECTOR - RightVector;
			pBot->desiredMovementDir.Normalize();
			bBlockedBottomRight = true;
		}

		if (bBlockedTopLeft || bBlockedTopRight)
		{
			return FLIGHT_DROP;
		}

		return FLIGHT_FLAP;
	}
	
	return FLIGHT_FLAP;

}

void BotFollowFlightPath(AvHAIPlayer* pBot, bool bAllowSkip)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		ClearBotPath(pBot);
		return;
	}

	nav_status* BotNavInfo = &pBot->BotNavInfo;
	edict_t* pEdict = pBot->Edict;

	vector<bot_path_node>::iterator CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

	Vector CurrentMoveDest = CurrentPathPoint->Location;
	Vector ClosestPointToPath = vClosestPointOnLine(CurrentPathPoint->FromLocation, CurrentMoveDest, pEdict->v.origin);
	bool bAtOrPastDestination = vEquals(ClosestPointToPath, CurrentMoveDest, 32.0f);

	// If we've reached our current path point
	if (bAtOrPastDestination)
	{
		// End of the whole path, stop all movement
		if (BotNavInfo->CurrentPathPoint >= (pBot->BotNavInfo.CurrentPath.size() - 1))
		{
			ClearBotMovement(pBot);
			return;
		}
		else
		{
			// Pick the next point in the path
			BotNavInfo->CurrentPathPoint++;
			ClearBotStuck(pBot);
			CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);
		}
	}

	if (CurrentPathPoint->flag == SAMPLE_POLYFLAGS_LIFT)
	{
		LiftMove(pBot, CurrentPathPoint->FromLocation, CurrentMoveDest);
		return;
	}

	bool bShouldSkipAhead = (CurrentPathPoint->flag != SAMPLE_POLYFLAGS_WALLCLIMB);

	if (bAllowSkip)
	{
		std::vector<bot_path_node>::iterator NextPathPoint = next(CurrentPathPoint);

		if (NextPathPoint != pBot->BotNavInfo.CurrentPath.end())
		{
			if (UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, NextPathPoint->Location, head_hull, false))
			{
				CurrentPathPoint->Location = pBot->Edict->v.origin;
				NextPathPoint->FromLocation = pBot->Edict->v.origin;

				BotNavInfo->CurrentPathPoint++;
				ClearBotStuck(pBot);
				CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);
			}
		}
	}

	Vector ClosestPoint = vClosestPointOnLine(CurrentPathPoint->FromLocation, CurrentPathPoint->Location, pBot->Edict->v.origin);

	if (vDist3DSq(pBot->Edict->v.origin, ClosestPoint) > sqrf(64.0f))
	{
		ClearBotPath(pBot);
		return;
	}

	if (CurrentPathPoint->flag == SAMPLE_POLYFLAGS_WALK)
	{
		if (!UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, CurrentPathPoint->FromLocation) && !UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, CurrentPathPoint->Location))
		{
			ClearBotPath(pBot);
			return;
		}
	}

	CurrentMoveDest = CurrentPathPoint->Location;
	Vector MoveFrom = CurrentPathPoint->FromLocation;

	unsigned char CurrentMoveArea = CurrentPathPoint->area;
	unsigned char NextMoveArea = (next(CurrentPathPoint) != BotNavInfo->CurrentPath.end()) ? next(CurrentPathPoint)->area : CurrentMoveArea;

	LerkFlightBehaviour FlightBehaviour = FLIGHT_FLAP;

	switch (CurrentPathPoint->flag)
	{
		case SAMPLE_POLYFLAGS_WALLCLIMB:
		case SAMPLE_POLYFLAGS_LADDER:
			FlightBehaviour = BotFlightClimbMove(pBot, CurrentPathPoint->FromLocation, CurrentPathPoint->Location, CurrentPathPoint->requiredZ);
			break;
		case SAMPLE_POLYFLAGS_FALL:
			FlightBehaviour = BotFlightFallMove(pBot, CurrentPathPoint->FromLocation, CurrentPathPoint->Location);
			break;
		default:
			FlightBehaviour = BotFlightWalkMove(pBot, CurrentPathPoint->FromLocation, CurrentPathPoint->Location, (SamplePolyFlags)CurrentPathPoint->flag, (SamplePolyAreas)CurrentPathPoint->area);
			break;

	}
		
	if (FlightBehaviour == FLIGHT_GLIDE)
	{
		pBot->Button |= IN_JUMP;
	}
	else if (FlightBehaviour == FLIGHT_FLAP)
	{		
		if (gpGlobals->time - BotNavInfo->LastFlapTime > 0.2f)
		{
			pBot->Button |= IN_JUMP;
			BotNavInfo->LastFlapTime = gpGlobals->time;
		}
	}

	Vector StartTrace = pBot->Edict->v.origin;
	Vector EndTrace = StartTrace + (UTIL_GetVectorNormal2D(pBot->Edict->v.angles) * 32.0f);

	edict_t* BlockingEdict = UTIL_TraceEntityHull(pBot->Edict, StartTrace, EndTrace);

	if (!FNullEnt(BlockingEdict) && (IsEdictStructure(BlockingEdict) || IsEdictPlayer(BlockingEdict)))
	{
		pBot->Edict->v.velocity.z = 150.0f;
	}

	CheckAndHandleBreakableObstruction(pBot, MoveFrom, CurrentMoveDest, SAMPLE_POLYFLAGS_WALK);

	CheckAndHandleDoorObstruction(pBot);

	BotMovementInputs(pBot);

}

void BotFollowSwimPath(AvHAIPlayer* pBot)
{

	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		ClearBotPath(pBot);
		NAV_ClearMovementTask(pBot);
		return;
	}

	nav_status* BotNavInfo = &pBot->BotNavInfo;
	edict_t* pEdict = pBot->Edict;

	vector<bot_path_node>::iterator CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

	// If we've reached our current path point
	if (vPointOverlaps3D(CurrentPathPoint->Location, pBot->Edict->v.absmin, pBot->Edict->v.absmax))
	{
		ClearBotStuck(pBot);

		pBot->BotNavInfo.CurrentPathPoint++;

		// No more path points, we've reached the end of our path
		if (pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
		{
			ClearBotPath(pBot);
			return;
		}
		else
		{
			CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

			if (CurrentPathPoint->flag == SAMPLE_POLYFLAGS_WALK)
			{
				CurrentPathPoint->FromLocation = pBot->Edict->v.origin;
			}
		}
	}

	bool TargetPointIsInWater = (UTIL_PointContents(CurrentPathPoint->Location) == CONTENTS_WATER || UTIL_PointContents(CurrentPathPoint->Location) == CONTENTS_SLIME);

	bool bHasNextPoint = (next(CurrentPathPoint) != BotNavInfo->CurrentPath.end());
	bool NextPointInWater = (bHasNextPoint) ? UTIL_PointContents(next(CurrentPathPoint)->Location) == CONTENTS_WATER : TargetPointIsInWater;

	bool bShouldSurface = (bHasNextPoint && !NextPointInWater && vDist2DSq(pEdict->v.origin, next(CurrentPathPoint)->FromLocation) < sqrf(100.0f));

	if (TargetPointIsInWater && !bShouldSurface)
	{
		BotMoveLookAt(pBot, CurrentPathPoint->Location);
		pBot->desiredMovementDir = UTIL_GetVectorNormal2D(CurrentPathPoint->Location - pEdict->v.origin);

		unsigned char NextArea = (next(CurrentPathPoint) != BotNavInfo->CurrentPath.end()) ? next(CurrentPathPoint)->area : SAMPLE_POLYAREA_GROUND;

		if (CurrentPathPoint->area == SAMPLE_POLYAREA_CROUCH || (NextArea == SAMPLE_POLYAREA_CROUCH && vDist2DSq(pEdict->v.origin, next(CurrentPathPoint)->FromLocation) < sqrf(50.0f)))
		{
			pBot->Button |= IN_DUCK;
		}

		return;
	}

	float WaterLevel = UTIL_WaterLevel(pEdict->v.origin, pEdict->v.origin.z, pEdict->v.origin.z + 500.0f);

	float WaterDiff = WaterLevel - pEdict->v.origin.z;

	// If we're below the waterline by a significant amount, then swim up to surface before we move on
	if (WaterDiff > 5.0f)
	{
		Vector MoveDir = UTIL_GetVectorNormal2D(CurrentPathPoint->Location - pEdict->v.origin);
		pBot->desiredMovementDir = MoveDir;

		if (WaterDiff > 10.0f)
		{
			BotMoveLookAt(pBot, pEdict->v.origin + (MoveDir * 5.0f) + Vector(0.0f, 0.0f, 100.0f));
		}
		else
		{
			BotMoveLookAt(pBot, pBot->CurrentEyePosition + (MoveDir * 50.0f) + Vector(0.0f, 0.0f, 50.0f));
		}
		
		return;
	}

	// We're at the surface, now tackle the path the usual way
	if (pBot->BotNavInfo.NavProfile.bFlyingProfile)
	{
		BotFollowFlightPath(pBot, true);
	}
	else
	{
		BotFollowPath(pBot);
	}

}

void BotFollowPath(AvHAIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		ClearBotPath(pBot);
		return;
	}

	nav_status* BotNavInfo = &pBot->BotNavInfo;
	edict_t* pEdict = pBot->Edict;


	// If we've reached our current path point
	if (HasBotReachedPathPoint(pBot))
	{
		ClearBotStuck(pBot);

		pBot->BotNavInfo.CurrentPathPoint++;

		// No more path points, we've reached the end of our path
		if (pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
		{
			ClearBotPath(pBot);
			return;
		}
	}

	bot_path_node CurrentNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	if (IsPlayerStandingOnPlayer(pBot->Edict) && CurrentNode.flag != SAMPLE_POLYFLAGS_WALLCLIMB && CurrentNode.flag != SAMPLE_POLYFLAGS_LADDER)
	{
		Vector ForwardDir = UTIL_GetForwardVector2D(pBot->Edict->v.angles);
		
		bool bCanMoveBackwards = UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, pBot->Edict->v.origin - (ForwardDir * 50.0f));

		if (bCanMoveBackwards)
		{
			pBot->desiredMovementDir = -ForwardDir;
			return;
		}
		
		bool bCanMoveForward = UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, pBot->Edict->v.origin + (ForwardDir * 50.0f));

		if (bCanMoveForward)
		{
			pBot->desiredMovementDir = ForwardDir;
			return;
		}

		// If we have a point we can go back to, and we can reach it, then go for it. Otherwise, keep pushing on and hope the other guy moves
		if (!vIsZero(pBot->BotNavInfo.LastOpenLocation))
		{
			if (UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, pBot->BotNavInfo.LastOpenLocation, GetPlayerRadius(pBot->Edict)))
			{
				NAV_SetMoveMovementTask(pBot, pBot->BotNavInfo.LastOpenLocation, nullptr);
				return;
			}
		}
	}

	if (IsPlayerLerk(pBot->Edict))
	{
		if (CurrentNode.flag != SAMPLE_POLYFLAGS_WALK && CurrentNode.flag != SAMPLE_POLYFLAGS_LIFT)
		{
			BotFollowFlightPath(pBot, false);
			return;
		}
	}

	if (IsBotOffPath(pBot))
	{
		MoveToWithoutNav(pBot, CurrentNode.Location);
		pBot->BotNavInfo.StuckInfo.bPathFollowFailed = true;
		ClearBotPath(pBot);
		NAV_ClearMovementTask(pBot);
		return;
	}

	pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;

	Vector MoveTo = CurrentNode.Location;

	NewMove(pBot);

}

void PerformUnstuckMove(AvHAIPlayer* pBot, const Vector MoveDestination)
{

	Vector FwdDir = UTIL_GetVectorNormal2D(MoveDestination - pBot->Edict->v.origin);
	pBot->desiredMovementDir = FwdDir;

	Vector HeadLocation = GetPlayerTopOfCollisionHull(pBot->Edict, false);

	bool bMustCrouch = false;

	if (!IsPlayerSkulk(pBot->Edict) && !IsPlayerGorge(pBot->Edict) && !UTIL_QuickTrace(pBot->Edict, HeadLocation, (HeadLocation + (FwdDir * 50.0f))))
	{
		pBot->Button |= IN_DUCK;
		bMustCrouch = true;
	}

	Vector MoveRightVector = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(FwdDir, UP_VECTOR));

	Vector BotRightSide = (pBot->Edict->v.origin + (MoveRightVector * GetPlayerRadius(pBot->Player)));
	Vector BotLeftSide = (pBot->Edict->v.origin - (MoveRightVector * GetPlayerRadius(pBot->Player)));

	bool bBlockedLeftSide = !UTIL_QuickTrace(pBot->Edict, BotRightSide, BotRightSide + (FwdDir * 50.0f));
	bool bBlockedRightSide = !UTIL_QuickTrace(pBot->Edict, BotLeftSide, BotLeftSide + (FwdDir * 50.0f));

	if (!bMustCrouch)
	{
		BotJump(pBot);
	}

	if (bBlockedRightSide && !bBlockedLeftSide)
	{
		pBot->desiredMovementDir = MoveRightVector;
		return;
	}
	else if (!bBlockedRightSide && bBlockedLeftSide)
	{
		pBot->desiredMovementDir = -MoveRightVector;
		return;
	}
	else
	{
		bBlockedLeftSide = !UTIL_QuickTrace(pBot->Edict, BotRightSide, BotRightSide - (MoveRightVector * 50.0f));
		bBlockedRightSide = !UTIL_QuickTrace(pBot->Edict, BotLeftSide, BotLeftSide + (MoveRightVector * 50.0f));

		if (bBlockedRightSide)
		{
			pBot->desiredMovementDir = -MoveRightVector;
		}
		else if (bBlockedLeftSide)
		{
			pBot->desiredMovementDir = MoveRightVector;
		}
		else
		{
			pBot->desiredMovementDir = FwdDir;
		}

	}

}

bool BotIsAtLocation(const AvHAIPlayer* pBot, const Vector Destination)
{
	if (vIsZero(Destination) || !(pBot->Edict->v.flags & FL_ONGROUND)) { return false; }

	return (vDist2DSq(pBot->Edict->v.origin, Destination) < sqrf(GetPlayerRadius(pBot->Player)) && fabs(pBot->CurrentFloorPosition.z - Destination.z) <= GetPlayerHeight(pBot->Edict, false));
}

Vector UTIL_ProjectPointToNavmesh(const Vector Location)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtQueryFilter* m_navFilter = &BaseNavProfiles[ALL_NAV_PROFILE].Filters;

	if (!m_navQuery) { return g_vecZero; }

	Vector PointToProject = Location;

	float pCheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];
	float Extents[3] = { 400.0f, 400.0f, 400.0f };

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, Extents, m_navFilter, &FoundPoly, NavNearest);

	if (FoundPoly > 0 && dtStatusSucceed(success))
	{
		return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
	}
	else
	{
		int PointContents = UTIL_PointContents(PointToProject);

		if (PointContents != CONTENTS_SOLID && PointContents != CONTENTS_LADDER)
		{
			Vector TraceHit = UTIL_GetTraceHitLocation(PointToProject + Vector(0.0f, 0.0f, 1.0f), PointToProject - Vector(0.0f, 0.0f, 1000.0f));

			PointToProject = (TraceHit != g_vecZero) ? TraceHit : Location;
		}

		float pRecheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

		dtStatus successRetry = m_navQuery->findNearestPoly(pRecheckLoc, Extents, m_navFilter, &FoundPoly, NavNearest);

		if (FoundPoly > 0 && dtStatusSucceed(success))
		{
			return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
		}
		else
		{
			return g_vecZero;
		}
		
	}
}

Vector UTIL_ProjectPointToNavmesh(const Vector Location, const Vector Extents)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(BaseNavProfiles[ALL_NAV_PROFILE]);
	const dtQueryFilter* m_navFilter = &BaseNavProfiles[ALL_NAV_PROFILE].Filters;

	if (!m_navQuery) { return g_vecZero; }

	Vector PointToProject = Location;

	float pCheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];
	float fExtents[3] = { Extents.x, Extents.z, Extents.y };

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, fExtents, m_navFilter, &FoundPoly, NavNearest);

	if (FoundPoly > 0 && dtStatusSucceed(success))
	{
		return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
	}
	else
	{
		int PointContents = UTIL_PointContents(PointToProject);

		if (PointContents != CONTENTS_SOLID && PointContents != CONTENTS_LADDER)
		{
			Vector TraceHit = UTIL_GetTraceHitLocation(PointToProject + Vector(0.0f, 0.0f, 1.0f), PointToProject - Vector(0.0f, 0.0f, 1000.0f));

			PointToProject = (TraceHit != g_vecZero) ? TraceHit : Location;
		}

		float pRecheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

		dtStatus successRetry = m_navQuery->findNearestPoly(pRecheckLoc, fExtents, m_navFilter, &FoundPoly, NavNearest);

		if (FoundPoly > 0 && dtStatusSucceed(success))
		{
			return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
		}
		else
		{
			return g_vecZero;
		}

	}
}

Vector UTIL_ProjectPointToNavmesh(const Vector Location, const nav_profile &NavProfile)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return g_vecZero; }

	Vector PointToProject = Location;

	float pCheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, pExtents, m_navFilter, &FoundPoly, NavNearest);

	if (FoundPoly > 0 && dtStatusSucceed(success))
	{
		return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
	}
	else
	{
		int PointContents = UTIL_PointContents(PointToProject);

		if (PointContents != CONTENTS_SOLID && PointContents != CONTENTS_LADDER)
		{
			Vector TraceHit = UTIL_GetTraceHitLocation(PointToProject + Vector(0.0f, 0.0f, 1.0f), PointToProject - Vector(0.0f, 0.0f, 1000.0f));

			PointToProject = (TraceHit != g_vecZero) ? TraceHit : Location;
		}

		float pRecheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

		dtStatus successRetry = m_navQuery->findNearestPoly(pRecheckLoc, pExtents, m_navFilter, &FoundPoly, NavNearest);

		if (FoundPoly > 0 && dtStatusSucceed(success))
		{
			return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
		}
		else
		{
			return g_vecZero;
		}

	}
}

Vector UTIL_ProjectPointToNavmesh(const Vector Location, const Vector Extents, const nav_profile& NavProfile)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return g_vecZero; }

	Vector PointToProject = Location;

	float pCheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	float fExtents[3] = { Extents.x, Extents.z, Extents.y };

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, fExtents, m_navFilter, &FoundPoly, NavNearest);

	if (FoundPoly > 0 && dtStatusSucceed(success))
	{
		return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
	}
	else
	{
		int PointContents = UTIL_PointContents(PointToProject);

		if (PointContents != CONTENTS_SOLID && PointContents != CONTENTS_LADDER)
		{
			Vector TraceHit = UTIL_GetTraceHitLocation(PointToProject + Vector(0.0f, 0.0f, 1.0f), PointToProject - Vector(0.0f, 0.0f, 1000.0f));

			PointToProject = (TraceHit != g_vecZero) ? TraceHit : Location;
		}

		float pRecheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

		dtStatus successRetry = m_navQuery->findNearestPoly(pRecheckLoc, fExtents, m_navFilter, &FoundPoly, NavNearest);

		if (FoundPoly > 0 && dtStatusSucceed(success))
		{
			return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
		}
		else
		{
			return g_vecZero;
		}

	}
}

bool UTIL_PointIsOnNavmesh(const Vector Location, const nav_profile &NavProfile)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return false; }

	float pCheckLoc[3] = { Location.x, Location.z, -Location.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	float pCheckExtents[3] = { 5.0f, 50.0f, 5.0f };

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, pCheckExtents, m_navFilter, &FoundPoly, NavNearest);

	return dtStatusSucceed(success) && FoundPoly > 0;

}

bool UTIL_PointIsOnNavmesh(const nav_profile& NavProfile, const Vector Location, const Vector SearchExtents)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return false; }

	float pCheckLoc[3] = { Location.x, Location.z, -Location.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	float pCheckExtents[3] = { SearchExtents.x, SearchExtents.z, SearchExtents.y };

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, pCheckExtents, m_navFilter, &FoundPoly, NavNearest);

	return dtStatusSucceed(success) && FoundPoly > 0;

}

void HandlePlayerAvoidance(AvHAIPlayer* pBot, const Vector MoveDestination)
{
	// Don't handle player avoidance if climbing a wall, ladder or in the air, as it will mess up the move and cause them to get stuck most likely
	if (pBot->Player->IsOnLadder() || IsPlayerClimbingWall(pBot->Edict) || !pBot->BotNavInfo.IsOnGround) { return; }

	float MyRadius = GetPlayerRadius(pBot->Edict);
		
	const Vector BotLocation = pBot->Edict->v.origin;
	const Vector MoveDir = UTIL_GetVectorNormal2D((MoveDestination - pBot->Edict->v.origin));

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* OtherPlayer = INDEXENT(i);

		if (!FNullEnt(OtherPlayer) && OtherPlayer != pBot->Edict && IsPlayerActiveInGame(OtherPlayer))
		{
			float OtherPlayerRadius = GetPlayerRadius(OtherPlayer);

			float avoidDistSq = sqrf(MyRadius + OtherPlayerRadius + 16.0f);

			// Don't do avoidance for a player if they're moving in broadly the same direction as us
			Vector OtherMoveDir = GetPlayerAttemptedMoveDirection(OtherPlayer);

			if (vDist3DSq(BotLocation, OtherPlayer->v.origin) <= avoidDistSq)
			{
				Vector BlockAngle = UTIL_GetVectorNormal2D(OtherPlayer->v.origin - BotLocation);
				float MoveBlockDot = UTIL_GetDotProduct2D(MoveDir, BlockAngle);

				// If other player is between us and our destination
				if (MoveBlockDot > 0.0f)
				{
					// If the other player is in the air or on top of us, back up and let them land
					if (!(OtherPlayer->v.flags & FL_ONGROUND) || OtherPlayer->v.groundentity == pBot->Edict)
					{
						pBot->desiredMovementDir = UTIL_GetVectorNormal2D(BotLocation - OtherPlayer->v.origin);
						return;
					}

					// Determine if we should move left or right to clear them
					Vector MoveRightVector = UTIL_GetCrossProduct(MoveDir, UP_VECTOR);

					int modifier = vPointOnLine(pBot->Edict->v.origin, MoveDestination, OtherPlayer->v.origin);

					float OtherPersonDistFromLine = vDistanceFromLine2D(pBot->Edict->v.origin, MoveDestination, OtherPlayer->v.origin);

					if (modifier == 0) { modifier = 1; }

					Vector PreferredMoveDir = (MoveRightVector * modifier);

					float TraceLength = OtherPersonDistFromLine + (fmaxf(MyRadius, OtherPlayerRadius) * 2.0f);


					// First see if we have enough room to move in our preferred avoidance direction
					if (UTIL_TraceNav(pBot->BotNavInfo.NavProfile, BotLocation, BotLocation + (PreferredMoveDir * TraceLength), 0.0f))
					{
						pBot->desiredMovementDir = PreferredMoveDir;
						return;
					}

					// Then try the opposite direction
					if (UTIL_TraceNav(pBot->BotNavInfo.NavProfile, BotLocation, BotLocation - (PreferredMoveDir * TraceLength), 0.0f))
					{
						pBot->desiredMovementDir = -PreferredMoveDir;
						return;
					}

					// If we have a point we can go back to, and we can reach it, then go for it. Otherwise, keep pushing on and hope the other guy moves
					if (!vIsZero(pBot->BotNavInfo.LastOpenLocation))
					{
						if (UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, pBot->BotNavInfo.LastOpenLocation, GetPlayerRadius(pBot->Edict)))
						{
							NAV_SetMoveMovementTask(pBot, pBot->BotNavInfo.LastOpenLocation, nullptr);
							return;
						}
					}
				}
			}
		}
	}
}

float UTIL_GetPathCostBetweenLocations(const nav_profile &NavProfile , const Vector FromLocation, const Vector ToLocation)
{
	vector<bot_path_node> path;
	path.clear();

	dtStatus pathFindResult = FindPathClosestToPoint(NavProfile, FromLocation, ToLocation, path, max_ai_use_reach);

	if (!dtStatusSucceed(pathFindResult)) { return 0.0f; }

	int currPathPoint = 1;
	float result = 0.0f;

	for (auto it = path.begin(); it != path.end(); it++)
	{
		result += vDist2DSq(it->FromLocation, it->Location) * NavProfile.Filters.getAreaCost(it->area);
	}

	return sqrtf(result);
}

void ClearBotMovement(AvHAIPlayer* pBot)
{
	pBot->BotNavInfo.TargetDestination = g_vecZero;
	pBot->BotNavInfo.ActualMoveDestination = g_vecZero;

	ClearBotPath(pBot);
	ClearBotStuck(pBot);
	ClearBotStuckMovement(pBot);

	pBot->LastPosition = pBot->Edict->v.origin;
	pBot->TimeSinceLastMovement = 0.0f;
}

void ClearBotStuck(AvHAIPlayer* pBot)
{
	pBot->BotNavInfo.LastDistanceFromDestination = 0.0f;
	pBot->BotNavInfo.LastStuckCheckTime = gpGlobals->time;
	pBot->BotNavInfo.TotalStuckTime = 0.0f;
	pBot->BotNavInfo.UnstuckMoveLocation = g_vecZero;
	pBot->BotNavInfo.StuckCheckMoveLocation = g_vecZero;
}

bool BotRecalcPath(AvHAIPlayer* pBot, const Vector Destination)
{
	ClearBotPath(pBot);
	NAV_ClearMovementTask(pBot);

	Vector ValidNavmeshPoint = UTIL_ProjectPointToNavmesh(Destination, Vector(max_ai_use_reach, max_ai_use_reach, max_ai_use_reach), pBot->BotNavInfo.NavProfile);

	// We can't actually get close enough to this point to consider it "reachable"
	if (vIsZero(ValidNavmeshPoint))
	{
		sprintf(pBot->PathStatus, "Could not project destination to navmesh");
		return false;
	}

	dtStatus FoundPath = FindPathClosestToPoint(pBot, pBot->BotNavInfo.MoveStyle, ValidNavmeshPoint, pBot->BotNavInfo.CurrentPath, max_ai_use_reach);

	if (dtStatusSucceed(FoundPath) && pBot->BotNavInfo.CurrentPath.size() > 0)
	{
		pBot->BotNavInfo.TargetDestination = Destination;
		pBot->BotNavInfo.ActualMoveDestination = pBot->BotNavInfo.CurrentPath.back().Location;

		if (next(pBot->BotNavInfo.CurrentPath.begin()) == pBot->BotNavInfo.CurrentPath.end() || vDist2DSq(pBot->BotNavInfo.CurrentPath.front().Location, pBot->Edict->v.origin) > sqrf(GetPlayerRadius(pBot->Player)))
		{
			pBot->BotNavInfo.CurrentPathPoint = 0;
		}
		else
		{
			pBot->BotNavInfo.CurrentPathPoint = 1;
		}


		return true;
	}

	return false;
}



float UTIL_FindZHeightForWallClimb(const Vector ClimbStart, const Vector ClimbEnd, const int HullNum)
{
	TraceResult hit;

	Vector StartTrace = ClimbEnd;

	UTIL_TraceLine(ClimbEnd, ClimbEnd - Vector(0.0f, 0.0f, 50.0f), ignore_monsters, nullptr, &hit);

	if (hit.fAllSolid || hit.fStartSolid || hit.flFraction < 1.0f)
	{
		StartTrace.z = hit.vecEndPos.z + 12.0f;
	}

	Vector EndTrace = ClimbStart;
	EndTrace.z = StartTrace.z;

	Vector CurrTraceStart = StartTrace;

	UTIL_TraceHull(StartTrace, EndTrace, ignore_monsters, HullNum, nullptr, &hit);

	if (hit.flFraction >= 1.0f && !hit.fAllSolid && !hit.fStartSolid)
	{
		return StartTrace.z;
	}
	else
	{
		int maxTests = 100;
		int testCount = 0;

		while ((hit.flFraction < 1.0f || hit.fStartSolid || hit.fAllSolid) && testCount < maxTests)
		{
			CurrTraceStart.z += 1.0f;
			EndTrace.z = CurrTraceStart.z;
			UTIL_TraceHull(CurrTraceStart, EndTrace, ignore_monsters, HullNum, nullptr, &hit);
			testCount++;
		}

		if (hit.flFraction >= 1.0f && !hit.fStartSolid)
		{
			if (UTIL_QuickHullTrace(nullptr, EndTrace, EndTrace + Vector(0.0f, 0.0f, 8.0f), false))
			{
				CurrTraceStart.z += 8.0f;
			}

			return CurrTraceStart.z;
		}
		else
		{
			return StartTrace.z;
		}
	}

	return StartTrace.z;
}

void ClearBotPath(AvHAIPlayer* pBot)
{
	pBot->BotNavInfo.CurrentPath.clear();
	pBot->BotNavInfo.CurrentPathPoint = 0;

	pBot->BotNavInfo.SpecialMovementFlags = 0;

	pBot->BotNavInfo.bNavProfileChanged = false;

	pBot->BotNavInfo.TargetDestination = g_vecZero;
	pBot->BotNavInfo.PathDestination = g_vecZero;
}

void ClearBotStuckMovement(AvHAIPlayer* pBot)
{
	pBot->BotNavInfo.UnstuckMoveLocation = g_vecZero;
}

void BotMovementInputs(AvHAIPlayer* pBot)
{
	if (vIsZero(pBot->desiredMovementDir)) { return; }

	edict_t* pEdict = pBot->Edict;

	UTIL_NormalizeVector2D(&pBot->desiredMovementDir);

	float currentYaw = pBot->Edict->v.v_angle.y;
	float moveDelta = UTIL_VecToAngles(pBot->desiredMovementDir).y;
	float angleDelta = currentYaw - moveDelta;

	float botSpeed = (pBot->BotNavInfo.bShouldWalk) ? (pBot->Edict->v.maxspeed * 0.4f) : pBot->Edict->v.maxspeed;

	if (angleDelta < -180.0f)
	{
		angleDelta += 360.0f;
	}
	else if (angleDelta > 180.0f)
	{
		angleDelta -= 360.0f;
	}

	if (angleDelta >= -22.5f && angleDelta < 22.5f)
	{
		pBot->ForwardMove = botSpeed;
		pBot->SideMove = 0.0f;
		pBot->Button |= IN_FORWARD;
	}
	else if (angleDelta >= 22.5f && angleDelta < 67.5f)
	{
		pBot->ForwardMove = botSpeed;
		pBot->SideMove = botSpeed;
		pBot->Button |= IN_FORWARD;
		pBot->Button |= IN_MOVERIGHT;
	}
	else if (angleDelta >= 67.5f && angleDelta < 112.5f)
	{
		pBot->ForwardMove = 0.0f;
		pBot->SideMove = botSpeed;
		pBot->Button |= IN_MOVERIGHT;
	}
	else if (angleDelta >= 112.5f && angleDelta < 157.5f)
	{
		pBot->ForwardMove = -botSpeed;
		pBot->SideMove = botSpeed;
		pBot->Button |= IN_BACK;
		pBot->Button |= IN_MOVERIGHT;
	}
	else if (angleDelta >= 157.5f || angleDelta <= -157.5f)
	{
		pBot->ForwardMove = -botSpeed;
		pBot->SideMove = 0.0f;
		pBot->Button |= IN_BACK;
	}
	else if (angleDelta >= -157.5f && angleDelta < -112.5f)
	{
		pBot->ForwardMove = -botSpeed;
		pBot->SideMove = -botSpeed;
		pBot->Button |= IN_BACK;
		pBot->Button |= IN_MOVELEFT;
	}
	else if (angleDelta >= -112.5f && angleDelta < -67.5f)
	{
		pBot->ForwardMove = 0.0f;
		pBot->SideMove = -botSpeed;
		pBot->Button |= IN_MOVELEFT;
	}
	else if (angleDelta >= -67.5f && angleDelta < -22.5f)
	{
		pBot->ForwardMove = botSpeed;
		pBot->SideMove = -botSpeed;
		pBot->Button |= IN_FORWARD;
		pBot->Button |= IN_MOVELEFT;
	}

	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size() || pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint].flag != SAMPLE_POLYFLAGS_LADDER)
	{
		if (pBot->Player->IsOnLadder())
		{
			BotJump(pBot);
		}
	}
}

void OnBotStartLadder(AvHAIPlayer* pBot)
{
	pBot->CurrentLadderNormal = UTIL_GetNearestLadderNormal(pBot->Edict);
}

void OnBotEndLadder(AvHAIPlayer* pBot)
{
	pBot->CurrentLadderNormal = g_vecZero;
}

Vector UTIL_GetFurthestVisiblePointOnPath(const AvHAIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size()) { return g_vecZero; }

	vector<bot_path_node>::const_iterator CurrentPathPoint = (pBot->BotNavInfo.CurrentPath.begin() + pBot->BotNavInfo.CurrentPathPoint);

	if (CurrentPathPoint == prev(pBot->BotNavInfo.CurrentPath.end()))
	{
		Vector MoveDir = UTIL_GetVectorNormal2D(CurrentPathPoint->Location - pBot->Edict->v.origin);
		return CurrentPathPoint->Location + (MoveDir * 300.0f);
	}

	Vector FurthestVisiblePoint = CurrentPathPoint->Location;
	FurthestVisiblePoint.z = pBot->CurrentEyePosition.z;

	for (auto it = next(CurrentPathPoint); it != pBot->BotNavInfo.CurrentPath.end(); it++)
	{
		Vector CheckPoint = it->Location + Vector(0.0f, 0.0f, 32.0f);

		if (UTIL_QuickTrace(pBot->Edict, pBot->CurrentEyePosition, CheckPoint))
		{
			FurthestVisiblePoint = CheckPoint;
		}
		else
		{
			break;
		}
	}

	return FurthestVisiblePoint;
}

Vector UTIL_GetFurthestVisiblePointOnLineWithHull(const Vector ViewerLocation, const Vector LineStart, const Vector LineEnd, int HullNumber)
{
	Vector Dir = UTIL_GetVectorNormal(LineEnd - LineStart);

	float Dist = vDist3D(LineStart, LineEnd);
	int Steps = (int)floorf(Dist / 10.0f);

	if (Steps == 0) { return g_vecZero; }

	Vector FinalView = g_vecZero;
	Vector ThisView = LineStart;

	for (int i = 0; i < Steps; i++)
	{
		if (UTIL_QuickHullTrace(NULL, ViewerLocation, ThisView, HullNumber))
		{
			FinalView = ThisView;
		}

		ThisView = ThisView + (Dir * 10.0f);
	}

	return FinalView;
}

Vector UTIL_GetFurthestVisiblePointOnPath(const Vector ViewerLocation, vector<bot_path_node>& path, bool bPrecise)
{
	if (path.size() == 0) { return g_vecZero; }

	for (auto it = path.rbegin(); it != path.rend(); it++)
	{

		if (UTIL_QuickTrace(NULL, ViewerLocation, it->Location))
		{
			if (!bPrecise || it == path.rbegin())
			{
				return it->Location;
			}
			else
			{
				Vector FromLoc = it->Location;
				Vector ToLoc = prev(it)->Location;

				Vector Dir = UTIL_GetVectorNormal(ToLoc - FromLoc);

				float Dist = vDist3D(FromLoc, ToLoc);
				int Steps = (int)floorf(Dist / 50.0f);

				if (Steps == 0) { return FromLoc; }

				Vector FinalView = FromLoc;
				Vector ThisView = FromLoc + (Dir * 50.0f);

				for (int i = 0; i < Steps; i++)
				{
					if (UTIL_QuickTrace(NULL, ViewerLocation, ThisView))
					{
						FinalView = ThisView;
					}

					ThisView = ThisView + (Dir * 50.0f);
				}

				return FinalView;
			}
		}
		else
		{
			if (bPrecise && it != path.rbegin())
			{
				Vector FromLoc = it->Location;
				Vector ToLoc = prev(it)->Location;

				Vector Dir = UTIL_GetVectorNormal(ToLoc - FromLoc);

				float Dist = vDist3D(FromLoc, ToLoc);
				int Steps = (int)floorf(Dist / 50.0f);

				if (Steps == 0) { continue; }

				Vector FinalView = g_vecZero;
				Vector ThisView = FromLoc + (Dir * 50.0f);

				for (int i = 0; i < Steps; i++)
				{
					if (UTIL_QuickTrace(NULL, ViewerLocation, ThisView))
					{
						FinalView = ThisView;
					}

					ThisView = ThisView + (Dir * 50.0f);
				}

				if (FinalView != g_vecZero)
				{
					return FinalView;
				}
				
			}
		}
	}

	return g_vecZero;
}

Vector UTIL_GetButtonFloorLocation(const Vector UserLocation, edict_t* ButtonEdict)
{
	Vector ClosestPoint = g_vecZero;
		
	if (ButtonEdict->v.size.x > 64.0f || ButtonEdict->v.size.y > 64.0f)
	{
		ClosestPoint = UTIL_GetClosestPointOnEntityToLocation(UserLocation, ButtonEdict);
	}
	else
	{
		ClosestPoint = UTIL_GetCentreOfEntity(ButtonEdict);
	}

	nav_profile ButtonNavProfile;
	memcpy(&ButtonNavProfile, &BaseNavProfiles[ALL_NAV_PROFILE], sizeof(nav_profile));

	ButtonNavProfile.Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_WELD);
	ButtonNavProfile.Filters.removeIncludeFlags(SAMPLE_POLYFLAGS_DOOR);

	Vector ButtonAccessPoint = UTIL_ProjectPointToNavmesh(ClosestPoint, Vector(100.0f, 100.0f, 100.0f), ButtonNavProfile);

	if (vIsZero(ButtonAccessPoint))
	{
		ButtonAccessPoint = ClosestPoint;
	}

	Vector PlayerAccessLoc = ButtonAccessPoint;

	if (ButtonAccessPoint.z > ClosestPoint.z)
	{
		PlayerAccessLoc.z += 18.0f;
	}
	else
	{
		PlayerAccessLoc.z += 36.0f;
	}
	
	if (fabsf(PlayerAccessLoc.z - ClosestPoint.z) <= max_player_use_reach)
	{
		return ButtonAccessPoint;
	}

	Vector NewProjection = ClosestPoint;

	if (ButtonAccessPoint.z > ClosestPoint.z)
	{
		NewProjection = ClosestPoint - Vector(0.0f, 0.0f, 100.0f);
	}
	else
	{
		NewProjection = ClosestPoint + Vector(0.0f, 0.0f, 100.0f);
	}

	Vector NewButtonAccessPoint = UTIL_ProjectPointToNavmesh(NewProjection, ButtonNavProfile);

	if (vIsZero(NewButtonAccessPoint))
	{
		NewButtonAccessPoint = ClosestPoint;
	}

	return NewButtonAccessPoint;
}

bool UTIL_IsTriggerLinkedToDoor(CBaseEntity* TriggerEntity, vector<CBaseEntity*>& CheckedTriggers, CBaseEntity* Door)
{
	if (!TriggerEntity || !Door) { return false; }

	if (TriggerEntity == Door) { return true; }

	CheckedTriggers.push_back(TriggerEntity);

	const char* DoorName = STRING(Door->pev->targetname);
	const char* TriggerName = STRING(TriggerEntity->pev->targetname);
	const char* TriggerTarget = STRING(TriggerEntity->pev->target);

	if (FStrEq(STRING(TriggerEntity->pev->target), DoorName)) { return true; }

	AvHWeldable* WeldableRef = dynamic_cast<AvHWeldable*>(TriggerEntity);
	
	if (WeldableRef)
	{
		string targetString = WeldableRef->GetTargetOnFinish();
		const char* targetOnFinish = targetString.c_str();

		CBaseEntity* TargetEntity = UTIL_FindEntityByTargetname(NULL, targetOnFinish);

		if (TargetEntity && TargetEntity != TriggerEntity && UTIL_IsTriggerLinkedToDoor(TargetEntity, CheckedTriggers, Door)) { return true; }

		return false;
	}

	CMultiManager* MMRef = dynamic_cast<CMultiManager*>(TriggerEntity);

	if (MMRef)
	{
		for (int i = 0; i < MMRef->m_cTargets; i++)
		{
			CBaseEntity* MMTargetEntity = UTIL_FindEntityByTargetname(NULL, STRING(MMRef->m_iTargetName[i]));

			if (!MMTargetEntity) { continue; }

			if (MMTargetEntity == Door) { return true; }

			// Already checked this one!
			if (std::find(CheckedTriggers.begin(), CheckedTriggers.end(), MMTargetEntity) != CheckedTriggers.end()) { continue; }

			if (UTIL_IsTriggerLinkedToDoor(MMTargetEntity, CheckedTriggers, Door)) { return true; }
		}

		return false;
	}

	CEnvGlobal* EnvGlobalRef = dynamic_cast<CEnvGlobal*>(TriggerEntity);

	if (EnvGlobalRef && EnvGlobalRef->m_globalstate)
	{
		const char* EnvGlobalState = STRING(EnvGlobalRef->m_globalstate);

		FOR_ALL_ENTITIES("multisource", CMultiSource*)
			const char* SourceGlobalState = STRING(theEntity->m_globalstate);
			if (FStrEq(EnvGlobalState, SourceGlobalState))
			{
				if (UTIL_IsTriggerLinkedToDoor(theEntity, CheckedTriggers, Door)) { return true; }
			}
		END_FOR_ALL_ENTITIES("multisource")

		return false;
	}

	CMultiSource* MSRef = dynamic_cast<CMultiSource*>(TriggerEntity);

	if (MSRef && MSRef->m_globalstate)
	{
		const char* targetName = STRING(MSRef->pev->targetname);

		FOR_ALL_ENTITIES("func_button", CBaseButton*)
			if (theEntity->m_sMaster && FStrEq(STRING(theEntity->m_sMaster), targetName))
			{
				if (std::find(CheckedTriggers.begin(), CheckedTriggers.end(), theEntity) == CheckedTriggers.end() && UTIL_IsTriggerLinkedToDoor(theEntity, CheckedTriggers, Door)) { return true; }
			}
		END_FOR_ALL_ENTITIES("func_button")

		FOR_ALL_ENTITIES("trigger_once", CBaseTrigger*)
			if (theEntity->m_sMaster && FStrEq(STRING(theEntity->m_sMaster), targetName))
			{
				if (std::find(CheckedTriggers.begin(), CheckedTriggers.end(), theEntity) == CheckedTriggers.end() && UTIL_IsTriggerLinkedToDoor(theEntity, CheckedTriggers, Door)) { return true; }
			}
		END_FOR_ALL_ENTITIES("trigger_once")

		FOR_ALL_ENTITIES("trigger_multiple", CBaseTrigger*)
			if (theEntity->m_sMaster && FStrEq(STRING(theEntity->m_sMaster), targetName))
			{
				if (std::find(CheckedTriggers.begin(), CheckedTriggers.end(), theEntity) == CheckedTriggers.end() && UTIL_IsTriggerLinkedToDoor(theEntity, CheckedTriggers, Door)) { return true; }
			}
		END_FOR_ALL_ENTITIES("trigger_multiple")

		return false;
	}

	CTriggerChangeTarget* TCTRef = dynamic_cast<CTriggerChangeTarget*>(TriggerEntity);

	if (TCTRef)
	{
		return FStrEq(STRING(TCTRef->GetNewTargetName()), STRING(Door->pev->targetname));
	}

	CBaseDelay* ToggleRef = dynamic_cast<CBaseDelay*>(TriggerEntity);

	if (ToggleRef && ToggleRef->pev->target)
	{
		CBaseEntity* TargetEntity = UTIL_FindEntityByTargetname(NULL, STRING(ToggleRef->pev->target));

		const char* TestTriggerTargetname = STRING(TriggerEntity->pev->targetname);
		const char* ThisTriggerTarget = STRING(TargetEntity->pev->target);

		// Don't check this if it's targeting a trigger we've already checked
		if (TargetEntity && std::find(CheckedTriggers.begin(), CheckedTriggers.end(), TargetEntity) == CheckedTriggers.end())
		{ 
			if (TargetEntity && UTIL_IsTriggerLinkedToDoor(TargetEntity, CheckedTriggers, Door)) { return true; }
		}

		FOR_ALL_ENTITIES("trigger_changetarget", CTriggerChangeTarget*)
			if (theEntity->GetNextTarget() && theEntity->GetNextTarget()->edict() == TriggerEntity->edict() && FStrEq(STRING(theEntity->GetNewTargetName()), STRING(Door->pev->targetname)))
			{
				return true;
			}
		END_FOR_ALL_ENTITIES("trigger_changetarget")
	}

	return false;
}

void UTIL_PopulateAffectedConnectionsForDoor(nav_door* Door)
{
	Door->AffectedConnections.clear();

	Vector HalfExtents = (Door->DoorEdict->v.size * 0.5f);
	HalfExtents.x += 16.0f;
	HalfExtents.y += 16.0f;
	HalfExtents.z += 16.0f;

	for (auto it = BaseMapConnections.begin(); it != BaseMapConnections.end(); it++)
	{
		if (it->ConnectionFlags == SAMPLE_POLYFLAGS_TEAM1PHASEGATE || it->ConnectionFlags == SAMPLE_POLYFLAGS_TEAM2PHASEGATE) { continue; }

		Vector ConnStart = it->FromLocation + Vector(0.0f, 0.0f, 15.0f);
		Vector ConnEnd = it->ToLocation + Vector(0.0f, 0.0f, 15.0f);
		Vector MidPoint = ConnStart + ((ConnEnd - ConnStart) * 0.5f);
		MidPoint.z = fmaxf(ConnStart.z, ConnEnd.z);

		for (auto stopIt = Door->StopPoints.begin(); stopIt != Door->StopPoints.end(); stopIt++)
		{
			Vector DoorCentre = (*stopIt);

			if (vlineIntersectsAABB(ConnStart, MidPoint, DoorCentre - HalfExtents, DoorCentre + HalfExtents))
			{
				Door->AffectedConnections.push_back(&(*it));
				break;
			}

			if (vlineIntersectsAABB(MidPoint, ConnEnd, DoorCentre - HalfExtents, DoorCentre + HalfExtents))
			{
				Door->AffectedConnections.push_back(&(*it));
				break;
			}
		}
			
	}
}

void UTIL_PopulateTriggersForEntity(edict_t* Entity, vector<DoorTrigger>& TriggerList)
{
	CBaseEntity* TriggerRef = NULL;
	CBaseEntity* DoorRef = CBaseEntity::Instance(Entity);

	if (!DoorRef) { return; }

	vector<CBaseEntity*> CheckedTriggerList;

	while ((TriggerRef = UTIL_FindEntityByClassname(TriggerRef, "func_button")) != NULL)
	{
		CheckedTriggerList.clear();
		if (UTIL_IsTriggerLinkedToDoor(TriggerRef, CheckedTriggerList, DoorRef))
		{
			DoorActivationType NewTriggerType = DOOR_BUTTON;

			DoorTrigger NewTrigger;
			NewTrigger.Entity = TriggerRef;
			NewTrigger.Edict = TriggerRef->edict();
			NewTrigger.ToggleEnt = dynamic_cast<CBaseToggle*>(TriggerRef);
			NewTrigger.TriggerType = NewTriggerType;
			NewTrigger.bIsActivated = (!NewTrigger.ToggleEnt || !NewTrigger.ToggleEnt->IsLockedByMaster());

			TriggerList.push_back(NewTrigger);			
		}
	}

	TriggerRef = NULL;
	

	while ((TriggerRef = UTIL_FindEntityByClassname(TriggerRef, "avhweldable")) != NULL)
	{
		CheckedTriggerList.clear();
		if (UTIL_IsTriggerLinkedToDoor(TriggerRef, CheckedTriggerList, DoorRef))
		{			
			DoorActivationType NewTriggerType = DOOR_WELD;

			DoorTrigger NewTrigger;
			NewTrigger.Entity = TriggerRef;
			NewTrigger.Edict = TriggerRef->edict();
			NewTrigger.ToggleEnt = dynamic_cast<CBaseToggle*>(TriggerRef);
			NewTrigger.TriggerType = NewTriggerType;
			NewTrigger.bIsActivated = (!NewTrigger.ToggleEnt || !NewTrigger.ToggleEnt->IsLockedByMaster());

			TriggerList.push_back(NewTrigger);
		}
	}

	TriggerRef = NULL;
	CheckedTriggerList.clear();

	while ((TriggerRef = UTIL_FindEntityByClassname(TriggerRef, "func_breakable")) != NULL)
	{
		CheckedTriggerList.clear();
		if (UTIL_IsTriggerLinkedToDoor(TriggerRef, CheckedTriggerList, DoorRef))
		{
			DoorActivationType NewTriggerType = DOOR_BREAK;

			DoorTrigger NewTrigger;
			NewTrigger.Entity = TriggerRef;
			NewTrigger.Edict = TriggerRef->edict();
			NewTrigger.ToggleEnt = dynamic_cast<CBaseToggle*>(TriggerRef);
			NewTrigger.TriggerType = NewTriggerType;
			NewTrigger.bIsActivated = (!NewTrigger.ToggleEnt || !NewTrigger.ToggleEnt->IsLockedByMaster());

			TriggerList.push_back(NewTrigger);
		}
	}

	TriggerRef = NULL;

	while ((TriggerRef = UTIL_FindEntityByClassname(TriggerRef, "trigger_once")) != NULL)
	{
		CheckedTriggerList.clear();
		if (UTIL_IsTriggerLinkedToDoor(TriggerRef, CheckedTriggerList, DoorRef))
		{
			DoorActivationType NewTriggerType = DOOR_TRIGGER;

			DoorTrigger NewTrigger;
			NewTrigger.Entity = TriggerRef;
			NewTrigger.Edict = TriggerRef->edict();
			NewTrigger.ToggleEnt = dynamic_cast<CBaseToggle*>(TriggerRef);
			NewTrigger.TriggerType = NewTriggerType;
			NewTrigger.bIsActivated = (!NewTrigger.ToggleEnt || !NewTrigger.ToggleEnt->IsLockedByMaster());

			TriggerList.push_back(NewTrigger);
		}
	}

	TriggerRef = NULL;

	while ((TriggerRef = UTIL_FindEntityByClassname(TriggerRef, "trigger_multiple")) != NULL)
	{
		CheckedTriggerList.clear();
		if (UTIL_IsTriggerLinkedToDoor(TriggerRef, CheckedTriggerList, DoorRef))
		{
			DoorActivationType NewTriggerType = DOOR_TRIGGER;

			DoorTrigger NewTrigger;
			NewTrigger.Entity = TriggerRef;
			NewTrigger.Edict = TriggerRef->edict();
			NewTrigger.ToggleEnt = dynamic_cast<CBaseToggle*>(TriggerRef);
			NewTrigger.TriggerType = NewTriggerType;
			NewTrigger.bIsActivated = (!NewTrigger.ToggleEnt || !NewTrigger.ToggleEnt->IsLockedByMaster());

			TriggerList.push_back(NewTrigger);
		}
	}
}



void UTIL_PopulateWeldableObstacles()
{

	CBaseEntity* currWeldable = NULL;
	while (((currWeldable = UTIL_FindEntityByClassname(currWeldable, "avhweldable")) != NULL))
	{
		if (currWeldable->pev->solid == SOLID_BSP)
		{
			nav_weldable NewWeldable;
			NewWeldable.WeldableEdict = currWeldable->edict();

			float SizeX = currWeldable->pev->size.x;
			float SizeY = currWeldable->pev->size.y;
			float SizeZ = currWeldable->pev->size.z;

			bool bUseXAxis = (SizeX >= SizeY);

			float CylinderRadius = fminf(SizeX, SizeY) * 0.5f;

			CylinderRadius = fmaxf(CylinderRadius, 16.0f);

			float Ratio = (bUseXAxis) ? (SizeX / (CylinderRadius * 2.0f)) : (SizeY / (CylinderRadius * 2.0f));

			int NumObstacles = (int)ceil(Ratio);

			if (NumObstacles > 32) { NumObstacles = 32; }

			Vector Dir = (bUseXAxis) ? RIGHT_VECTOR : FWD_VECTOR;

			Vector StartPoint = UTIL_GetCentreOfEntity(currWeldable->edict());

			if (bUseXAxis)
			{
				StartPoint.x = currWeldable->pev->absmin.x + CylinderRadius;
			}
			else
			{
				StartPoint.y = currWeldable->pev->absmin.y + CylinderRadius;
			}

			StartPoint.z -= 2.0f;

			Vector CurrentPoint = StartPoint;

			NewWeldable.NumObstacles = NumObstacles;

			for (int ii = 0; ii < NumObstacles; ii++)
			{
				UTIL_AddTemporaryObstacles(CurrentPoint, CylinderRadius, SizeZ, DT_TILECACHE_WELD_AREA, NewWeldable.ObstacleRefs[ii]);

				if (bUseXAxis)
				{
					CurrentPoint.x += CylinderRadius * 2.0f;
				}
				else
				{
					CurrentPoint.y += CylinderRadius * 2.0f;
				}
			}

			NavWeldableObstacles.push_back(NewWeldable);
		}
	}
}

void UTIL_ModifyOffMeshConnectionFlag(AvHAIOffMeshConnection* Connection, const unsigned int NewFlag)
{
	if (!Connection || Connection->ConnectionFlags == NewFlag) { return; }

	Connection->ConnectionFlags = NewFlag;

	for (int i = 0; i < BUILDING_NAV_MESH; i++)
	{
		if (NavMeshes[i].tileCache && Connection->ConnectionRefs[i])
		{
			NavMeshes[i].tileCache->modifyOffMeshConnection(Connection->ConnectionRefs[i], NewFlag);
		}
	}

	bNavMeshModified = true;
}

void UTIL_UpdateDoors(bool bInitial)
{
	for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
	{
		nav_door* NavDoor = &(*it);
		DoorActivationType PrevType = it->ActivationType;

		const char* DoorName = STRING(NavDoor->DoorEdict->v.targetname);

		UTIL_UpdateDoorTriggers(NavDoor);
				
		CBaseToggle* DoorRef = it->DoorEntity;

		if (!DoorRef) { continue; }

		if (bInitial)
		{
			UTIL_PopulateAffectedConnectionsForDoor(NavDoor);
		}

		if (DoorRef->m_toggle_state == TS_GOING_UP || DoorRef->m_toggle_state == TS_GOING_DOWN)
		{
			if (it->NumObstacles > 0)
			{
				for (int ii = 0; ii < it->NumObstacles; ii++)
				{
					UTIL_RemoveTemporaryObstacles(it->ObstacleRefs[ii]);
				}

				it->NumObstacles = 0;

			}
			continue;
		}

		if (bInitial || DoorRef->m_toggle_state != it->CurrentState || PrevType != it->ActivationType)
		{
			if (it->NumObstacles > 0)
			{
				for (int ii = 0; ii < it->NumObstacles; ii++)
				{
					UTIL_RemoveTemporaryObstacles(it->ObstacleRefs[ii]);
				}

				it->NumObstacles = 0;

			}

			if (it->ActivationType == DOOR_NONE)
			{
				Vector HalfExtents = (NavDoor->DoorEdict->v.size) * 0.5f;
				HalfExtents.x += 16.0f;
				HalfExtents.y += 16.0f;
				HalfExtents.z += 16.0f;

				for (auto conIt = NavDoor->AffectedConnections.begin(); conIt != NavDoor->AffectedConnections.end(); conIt++)
				{
					AvHAIOffMeshConnection* ThisConnection = (*conIt);

					Vector ConnStart = ThisConnection->FromLocation + Vector(0.0f, 0.0f, 15.0f);
					Vector ConnEnd = ThisConnection->ToLocation + Vector(0.0f, 0.0f, 15.0f);
					Vector MidPoint = ConnStart + ((ConnEnd - ConnStart) * 0.5f);
					MidPoint.z = fmaxf(ConnStart.z, ConnEnd.z);

					Vector DoorCentre = UTIL_GetCentreOfEntity(NavDoor->DoorEdict);
					DoorCentre.z -= 16.0f;

					bool bThisConnectionAffected = false;

					Vector NearestPointOnLine = vClosestPointOnLine(ConnStart, MidPoint, DoorCentre);
					if (vPointOverlaps3D(NearestPointOnLine, DoorCentre - HalfExtents, DoorCentre + HalfExtents))
					{
						UTIL_ModifyOffMeshConnectionFlag(ThisConnection, SAMPLE_POLYFLAGS_DISABLED);
						bThisConnectionAffected = true;
					}
					else
					{
						NearestPointOnLine = vClosestPointOnLine(MidPoint, ConnEnd, DoorCentre);
						if (vPointOverlaps3D(NearestPointOnLine, DoorCentre - HalfExtents, DoorCentre + HalfExtents))
						{
							UTIL_ModifyOffMeshConnectionFlag(ThisConnection, SAMPLE_POLYFLAGS_DISABLED);
							bThisConnectionAffected = true;
						}
					}

					if (!bThisConnectionAffected)
					{
						if (ThisConnection->ConnectionFlags != ThisConnection->DefaultConnectionFlags)
						{
							UTIL_ModifyOffMeshConnectionFlag(ThisConnection, ThisConnection->DefaultConnectionFlags);
						}
					}

				}

				Vector DoorCentre = UTIL_GetCentreOfEntity(it->DoorEdict);
				DoorCentre.z -= 24.0f;

				dtNavMeshQuery* Query = NavMeshes[BUILDING_NAV_MESH].navQuery;
				nav_profile StructureProfile = GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE);

				dtPolyRef Polys[8];
				int polyCount;

				float DoorHalfExtents[3] = { HalfExtents.x, HalfExtents.z, HalfExtents.y };
				float DoorCentreFlt[3] = { DoorCentre.x, DoorCentre.z, -DoorCentre.y };

				Query->queryPolygons(DoorCentreFlt, DoorHalfExtents, &StructureProfile.Filters, Polys, &polyCount, 8);

				if (polyCount > 0)
				{
					UTIL_ApplyTempObstaclesToDoor(NavDoor, DT_TILECACHE_NULL_AREA);
				}

				
			}
			else if (it->ActivationType == DOOR_WELD)
			{
				Vector HalfExtents = (NavDoor->DoorEdict->v.size) * 0.5f;
				HalfExtents.x += 16.0f;
				HalfExtents.y += 16.0f;
				HalfExtents.z += 16.0f;

				for (auto conIt = NavDoor->AffectedConnections.begin(); conIt != NavDoor->AffectedConnections.end(); conIt++)
				{
					AvHAIOffMeshConnection* ThisConnection = (*conIt);

					Vector ConnStart = ThisConnection->FromLocation + Vector(0.0f, 0.0f, 15.0f);
					Vector ConnEnd = ThisConnection->ToLocation + Vector(0.0f, 0.0f, 15.0f);
					Vector MidPoint = ConnStart + ((ConnEnd - ConnStart) * 0.5f);
					MidPoint.z = fmaxf(ConnStart.z, ConnEnd.z);

					Vector DoorCentre = UTIL_GetCentreOfEntity(NavDoor->DoorEdict);

					bool bThisConnectionAffected = (vlineIntersectsAABB(ConnStart, MidPoint, NavDoor->DoorEdict->v.absmin, NavDoor->DoorEdict->v.absmax) || vlineIntersectsAABB(MidPoint, ConnEnd, NavDoor->DoorEdict->v.absmin, NavDoor->DoorEdict->v.absmax));

					if (bThisConnectionAffected)
					{
						if (ThisConnection->bBiDirectional)
						{
							UTIL_ModifyOffMeshConnectionFlag(ThisConnection, SAMPLE_POLYFLAGS_WELD);
						}
						else
						{
							DoorTrigger* NearestTrigger = UTIL_GetNearestDoorTrigger(ConnStart, NavDoor, nullptr, true);

							if (!NearestTrigger)
							{
								UTIL_ModifyOffMeshConnectionFlag(ThisConnection, SAMPLE_POLYFLAGS_DISABLED);
							}
							else
							{
								if (NearestTrigger->TriggerType == DOOR_WELD)
								{
									UTIL_ModifyOffMeshConnectionFlag(ThisConnection, SAMPLE_POLYFLAGS_WELD);
								}
								else
								{
									UTIL_ModifyOffMeshConnectionFlag(ThisConnection, ThisConnection->DefaultConnectionFlags);
								}
							}
						}
					}
					else
					{
						if (ThisConnection->ConnectionFlags != ThisConnection->DefaultConnectionFlags)
						{
							UTIL_ModifyOffMeshConnectionFlag(ThisConnection, ThisConnection->DefaultConnectionFlags);
						}
					}

				}

				Vector DoorCentre = UTIL_GetCentreOfEntity(it->DoorEdict);
				DoorCentre.z -= 24.0f;

				dtNavMeshQuery* Query = NavMeshes[BUILDING_NAV_MESH].navQuery;
				nav_profile StructureProfile = GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE);

				dtPolyRef Polys[8];
				int polyCount;

				float DoorHalfExtents[3] = { HalfExtents.x, HalfExtents.z, HalfExtents.y};
				float DoorCentreFlt[3] = { DoorCentre.x, DoorCentre.z, -DoorCentre.y };
				
				Query->queryPolygons(DoorCentreFlt, DoorHalfExtents, &StructureProfile.Filters, Polys, &polyCount, 8);

				if (polyCount > 0)
				{
					UTIL_ApplyTempObstaclesToDoor(NavDoor, DT_TILECACHE_WELD_AREA);
				}

			}
			else
			{
				Vector DoorCentre = UTIL_GetCentreOfEntity(it->DoorEdict);
				DoorCentre.z -= 24.0f;

				dtNavMeshQuery* Query = NavMeshes[BUILDING_NAV_MESH].navQuery;
				nav_profile StructureProfile = GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE);

				dtPolyRef Polys[8];
				int polyCount;

				float DoorHalfExtents[3] = { it->DoorEdict->v.size.x, it->DoorEdict->v.size.z, it->DoorEdict->v.size.y };
				float DoorCentreFlt[3] = { DoorCentre.x, DoorCentre.z, -DoorCentre.y };

				Query->queryPolygons(DoorCentreFlt, DoorHalfExtents, &StructureProfile.Filters, Polys, &polyCount, 8);

				if (polyCount > 0)
				{
					UTIL_ApplyTempObstaclesToDoor(NavDoor, DT_TILECACHE_DOOR_AREA);
				}				
			}

			it->CurrentState = DoorRef->m_toggle_state;
		}
	}

}

void UTIL_UpdateWeldableObstacles()
{
	for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end();)
	{
		edict_t* WeldableEdict = it->WeldableEdict;

		if (FNullEnt(WeldableEdict) || WeldableEdict->v.deadflag != DEAD_NO || WeldableEdict->v.solid != SOLID_BSP)
		{
			for (int ii = 0; ii < it->NumObstacles; ii++)
			{
				UTIL_RemoveTemporaryObstacles(it->ObstacleRefs[ii]);
			}

			it->NumObstacles = 0;

			it = NavWeldableObstacles.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void UTIL_ApplyTempObstaclesToDoor(nav_door* DoorRef, const int Area)
{
	if (!DoorRef) { return; }

	if (DoorRef->NumObstacles > 0)
	{
		for (int ii = 0; ii < DoorRef->NumObstacles; ii++)
		{
			UTIL_RemoveTemporaryObstacles(DoorRef->ObstacleRefs[ii]);
		}

		DoorRef->NumObstacles = 0;

	}

	if (FNullEnt(DoorRef->DoorEdict) || DoorRef->DoorEdict->free)
	{
		return;
	}

	float SizeX = DoorRef->DoorEdict->v.size.x;
	float SizeY = DoorRef->DoorEdict->v.size.y;
	float SizeZ = DoorRef->DoorEdict->v.size.z;

	bool bUseXAxis = (SizeX >= SizeY);

	float CylinderRadius = fminf(SizeX, SizeY) * 0.5f;

	float Ratio = (bUseXAxis) ? (SizeX / (CylinderRadius * 2.0f)) : (SizeY / (CylinderRadius * 2.0f));

	int NumObstacles = (int)ceil(Ratio);

	if (NumObstacles > 32) { NumObstacles = 32; }

	Vector Dir = (bUseXAxis) ? RIGHT_VECTOR : FWD_VECTOR;

	Vector StartPoint = UTIL_GetCentreOfEntity(DoorRef->DoorEdict);

	if (bUseXAxis)
	{
		StartPoint.x = DoorRef->DoorEdict->v.absmin.x + CylinderRadius;
	}
	else
	{
		StartPoint.y = DoorRef->DoorEdict->v.absmin.y + CylinderRadius;
	}

	StartPoint.z -= 25.0f;

	Vector CurrentPoint = StartPoint;

	DoorRef->NumObstacles = NumObstacles;

	for (int ii = 0; ii < NumObstacles; ii++)
	{
		UTIL_AddTemporaryObstacles(CurrentPoint, CylinderRadius, SizeZ, Area, DoorRef->ObstacleRefs[ii]);

		if (bUseXAxis)
		{
			CurrentPoint.x += CylinderRadius * 2.0f;
		}
		else
		{
			CurrentPoint.y += CylinderRadius * 2.0f;
		}
	}

}

void UTIL_UpdateDoorTriggers(nav_door* Door)
{
	// Don't need to do anything if the door can be shot or opened by using it
	if (!Door || Door->ActivationType == DOOR_USE || Door->ActivationType == DOOR_SHOOT) { return; }

	if (Door->TriggerEnts.size() == 0)
	{
		// No more triggers left, door is dormant
		Door->ActivationType = DOOR_NONE;
		return;
	}

	DoorActivationType NewActivationType = DOOR_NONE;

	bool bButtonHasBeenPressed = false;

	for (auto it = Door->TriggerEnts.begin(); it != Door->TriggerEnts.end();)
	{
		if (FNullEnt(it->Edict) || it->Edict->free)
		{
			it = Door->TriggerEnts.erase(it);
			continue;
		}

		if (it->TriggerType == DOOR_WELD)
		{
			AvHWeldable* WeldableRef = dynamic_cast<AvHWeldable*>(it->Entity);

			if (WeldableRef && WeldableRef->GetIsWelded())
			{
				it = Door->TriggerEnts.erase(it);
				continue;
			}
		}
				
		if (FStrEq(STRING(it->Edict->v.target), STRING(Door->DoorEdict->v.targetname)))
		{
			it->bIsActivated = (it->ToggleEnt) ? !it->ToggleEnt->IsLockedByMaster() : true;
		}
		else
		{
			// Weldables and breakables can't be "deactivated" so assume they are always actived
			if (it->TriggerType == DOOR_WELD || it->TriggerType == DOOR_BREAK)
			{
				it->bIsActivated = true;
			}
			else
			{
				CBaseEntity* ActivationTarget = UTIL_FindEntityByString(NULL, "targetname", STRING(it->Edict->v.target));

				if (!ActivationTarget)
				{
					it->bIsActivated = true;
				}
				else
				{
					const char* classname = STRING(ActivationTarget->pev->classname);
					vector<CBaseEntity*> CheckedTriggerList;
					it->bIsActivated = UTIL_IsTriggerLinkedToDoor(ActivationTarget, CheckedTriggerList, Door->DoorEntity);
				}
			}
		}

		if (it->bIsActivated)
		{
			if (it->TriggerType == DOOR_WELD)
			{
				NewActivationType = DOOR_WELD;
			}
			else
			{
				if (NewActivationType != DOOR_WELD)
				{
					NewActivationType = DOOR_TRIGGER;
				}
			}
		}

		float BaseTriggerDelay = 0.0f;
		float BaseTriggerResetTime = 0.0f;

		bool bButtonIsToggle = FBitSet(it->Edict->v.spawnflags, SF_DOOR_NO_AUTO_RETURN);

		if (it->ToggleEnt)
		{
			BaseTriggerDelay = it->ToggleEnt->m_flDelay;
			BaseTriggerResetTime = (bButtonIsToggle) ? 1.0f : it->ToggleEnt->GetDelay();
		}		

		float DoorDelay = (FBitSet(Door->DoorEdict->v.spawnflags, SF_DOOR_NO_AUTO_RETURN)) ? 0.0f : Door->DoorEntity->GetDelay();
		it->ActivationDelay = fmaxf(BaseTriggerDelay, BaseTriggerResetTime) + DoorDelay + 1.0f;

		if (it->ToggleEnt && it->ToggleEnt->GetToggleState() != it->LastToggleState)
		{
			TOGGLE_STATE NewState = (TOGGLE_STATE)it->ToggleEnt->GetToggleState();

			if (it->LastToggleState == TS_AT_BOTTOM || (bButtonIsToggle && it->LastToggleState == TS_AT_TOP))
			{
				it->NextActivationTime = gpGlobals->time + fmaxf(it->ActivationDelay, 1.0f);
				bButtonHasBeenPressed = true;
			}

			it->LastToggleState = NewState;
		}

		it++;
	}

	if (bButtonHasBeenPressed)
	{
		for (auto it = Door->TriggerEnts.begin(); it != Door->TriggerEnts.end(); it++)
		{
			it->NextActivationTime = gpGlobals->time + fmaxf(it->ActivationDelay, 1.0f);
		}
	}

	Door->ActivationType = NewActivationType;
}

void UTIL_ClearDoorData()
{
	for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
	{
		if (it->NumObstacles > 0)
		{
			for (int ii = 0; ii < it->NumObstacles; ii++)
			{
				UTIL_RemoveTemporaryObstacles(it->ObstacleRefs[ii]);
			}

			it->NumObstacles = 0;

		}

		it->StopPoints.clear();
	}

	NavDoors.clear();
}

void UTIL_ClearWeldablesData()
{
	for (auto it = NavWeldableObstacles.begin(); it != NavWeldableObstacles.end(); it++)
	{
		if (it->NumObstacles > 0)
		{
			for (int ii = 0; ii < it->NumObstacles; ii++)
			{
				UTIL_RemoveTemporaryObstacles(it->ObstacleRefs[ii]);
			}

			it->NumObstacles = 0;

		}
	}

	NavWeldableObstacles.clear();
}

// TODO: This
void UTIL_PopulateTrainStopPoints(nav_door* TrainDoor)
{
	CBasePlatTrain* TrainRef = dynamic_cast<CBasePlatTrain*>(TrainDoor->DoorEntity);

	if (!TrainRef) { return; }

	CBaseEntity* StartCorner = TrainRef->GetNextTarget();

	if (!StartCorner)
	{
		// We aren't using path corners, so we're probably a func_plat
		TrainDoor->StopPoints.push_back(UTIL_GetCentreOfEntity(TrainDoor->DoorEdict) + TrainRef->m_vecPosition1);
		TrainDoor->StopPoints.push_back(UTIL_GetCentreOfEntity(TrainDoor->DoorEdict) + TrainRef->m_vecPosition2);
		return;
	}

	// If the "door" is a func_train, then a path corner is considered a "stop" if flagged to wait for retrigger, or has a delay associated with it
	// Eventually, we probably want to remove this expectation so the bot can use platforms which continuously move
	if (StartCorner->pev->spawnflags & SF_TRAIN_WAIT_RETRIGGER || StartCorner->GetDelay() > 0.0f)
	{
		TrainDoor->StopPoints.push_back(StartCorner->pev->origin);
	}

	// Populate all path corners at which this func_train stops. Bot will use this to determine when to board the train

	CBaseEntity* CurrentCorner = StartCorner->GetNextTarget();

	while (CurrentCorner != NULL && CurrentCorner != StartCorner)
	{
		// Check if the train stops at this path corner, and if so, add it to the stop points array
		if (CurrentCorner->pev->spawnflags & SF_TRAIN_WAIT_RETRIGGER || CurrentCorner->GetDelay() > 0.0f)
		{
			TrainDoor->StopPoints.push_back(CurrentCorner->pev->origin);
		}

		CurrentCorner = CurrentCorner->GetNextTarget();
	}

}

void UTIL_PopulateDoors()
{

	UTIL_ClearDoorData();

	vector<CBaseEntity*> DoorsToPopulate;
	DoorsToPopulate.clear();

	CBaseEntity* currDoor = NULL;
	while ((currDoor = UTIL_FindEntityByClassname(currDoor, "func_door")) != NULL)
	{
		DoorsToPopulate.push_back(currDoor);
	}

	currDoor = NULL;
	while ((currDoor = UTIL_FindEntityByClassname(currDoor, "func_seethroughdoor")) != NULL)
	{
		DoorsToPopulate.push_back(currDoor);
	}

	currDoor = NULL;
	while ((currDoor = UTIL_FindEntityByClassname(currDoor, "func_door_rotating")) != NULL)
	{
		DoorsToPopulate.push_back(currDoor);
	}

	currDoor = NULL;
	while ((currDoor = UTIL_FindEntityByClassname(currDoor, "func_plat")) != NULL)
	{
		DoorsToPopulate.push_back(currDoor);
	}

	currDoor = NULL;
	while ((currDoor = UTIL_FindEntityByClassname(currDoor, "func_train")) != NULL)
	{
		DoorsToPopulate.push_back(currDoor);
	}

	for (auto it = DoorsToPopulate.begin(); it != DoorsToPopulate.end(); it++)
	{
		CBaseEntity* DoorEnt = *it;

		CBaseToggle* ToggleRef = dynamic_cast<CBaseToggle*>(DoorEnt);
		if (!ToggleRef) { continue; }

		nav_door NewDoor;
		NewDoor.NumObstacles = 0;

		NewDoor.DoorEntity = ToggleRef;
		NewDoor.DoorEdict = DoorEnt->edict();
		NewDoor.CurrentState = ToggleRef->m_toggle_state;
		NewDoor.DoorName = STRING(NewDoor.DoorEdict->v.targetname);

		const char* DoorName = STRING(NewDoor.DoorEdict->v.targetname);

		if (DoorEnt->pev->spawnflags & DOOR_USE_ONLY)
		{
			NewDoor.ActivationType = DOOR_USE;
		}
		else
		{
			NewDoor.TriggerEnts.clear();
			UTIL_PopulateTriggersForEntity(DoorEnt->edict(), NewDoor.TriggerEnts);
		}

		CBasePlatTrain* TrainRef = dynamic_cast<CBasePlatTrain*>(DoorEnt);

		if (TrainRef)
		{
			NewDoor.DoorType = DOORTYPE_TRAIN;
			UTIL_PopulateTrainStopPoints(&NewDoor);
		}
		else
		{
			NewDoor.DoorType = DOORTYPE_DOOR;
			if (NewDoor.DoorEdict->v.spawnflags & DOOR_START_OPEN)
			{
				NewDoor.StopPoints.push_back(UTIL_GetCentreOfEntity(NewDoor.DoorEdict) + ToggleRef->m_vecPosition2);
				NewDoor.StopPoints.push_back(UTIL_GetCentreOfEntity(NewDoor.DoorEdict) - ToggleRef->m_vecPosition1);
			}
			else
			{
				NewDoor.StopPoints.push_back(UTIL_GetCentreOfEntity(NewDoor.DoorEdict) + ToggleRef->m_vecPosition1);
				NewDoor.StopPoints.push_back(UTIL_GetCentreOfEntity(NewDoor.DoorEdict) + ToggleRef->m_vecPosition2);
			}
		}
		

		NavDoors.push_back(NewDoor);
	}

	for (auto it = BaseMapConnections.begin(); it != BaseMapConnections.end(); it++)
	{
		if (!(it->ConnectionFlags & SAMPLE_POLYFLAGS_LIFT)) { continue; }

		nav_door* CorrespondingLift = UTIL_GetClosestLiftToPoints(it->FromLocation, it->ToLocation);

		if (CorrespondingLift)
		{
			it->TargetObject = CorrespondingLift->DoorEdict;
		}
	}

	UTIL_UpdateDoors(true);
}

nav_door* UTIL_GetNavDoorByEdict(const edict_t* DoorEdict)
{
	if (FNullEnt(DoorEdict)) { return nullptr; }

	for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
	{
		if (it->DoorEdict == DoorEdict)
		{
			return &(*it);
		}
	}

	return nullptr;
}

nav_door* UTIL_GetLiftReferenceByEdict(const edict_t* DoorEdict)
{
	if (FNullEnt(DoorEdict)) { return nullptr; }

	for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
	{
		if (it->DoorEdict == DoorEdict)
		{
			if (UTIL_GetOffMeshConnectionForLift(&(*it)) != nullptr)
			{
				return &(*it);
			}
			else
			{
				return nullptr;
			}
		}
	}

	return nullptr;
}

AvHAIOffMeshConnection* UTIL_GetOffMeshConnectionForLift(nav_door* LiftRef)
{
	if (!LiftRef) { return nullptr; }

	AvHAIOffMeshConnection* NearestConnection = nullptr;
	float MinDist = 0.0f;

	for (auto it = BaseMapConnections.begin(); it != BaseMapConnections.end(); it++)
	{
		if (!(it->ConnectionFlags & SAMPLE_POLYFLAGS_LIFT)) { continue; }

		if (it->TargetObject == LiftRef->DoorEdict)
		{
			return &(*it);
		}

		//Vector LiftLocation = UTIL_GetCentreOfEntity(LiftRef->DoorEdict);

		//float ThisDist = fminf(vDist3DSq(it->FromLocation, LiftLocation), vDist3DSq(it->ToLocation, LiftLocation));

		//if (!NearestConnection || ThisDist < MinDist)
		//{
		//	NearestConnection = &(*it);
		//	MinDist = ThisDist;
		//}
	}

	return NearestConnection;
}

// TODO: Find the topmost point when open, and topmost point when closed, and see how closely they align to the top and bottom point parameters
nav_door* UTIL_GetClosestLiftToPoints(const Vector StartPoint, const Vector EndPoint)
{
	nav_door* Result = nullptr;

	float minDist = 0.0f;

	for (auto it = NavDoors.begin(); it != NavDoors.end(); it++)
	{
		float distTopPoint = FLT_MAX;
		float distBottomPoint = FLT_MAX;

		for (auto stop = it->StopPoints.begin(); stop != it->StopPoints.end(); stop++)
		{
			distTopPoint = fminf(distTopPoint, vDist3DSq(UTIL_GetClosestPointOnEntityToLocation(StartPoint, it->DoorEdict, *stop), StartPoint));
			distBottomPoint = fminf(distBottomPoint, vDist3DSq(UTIL_GetClosestPointOnEntityToLocation(EndPoint, it->DoorEdict, *stop), EndPoint));
		}

		// Get the average distance from our desired start and end points, whichever scores lowest is probably the lift/train/door we want to ride
		float thisDist = ((distTopPoint + distBottomPoint) * 0.5f);

		if (!Result || thisDist < minDist)
		{
			Result = &(*it);
			minDist = thisDist;
		}
	}

	return Result;
}

void UTIL_AddOffMeshConnection(Vector StartLoc, Vector EndLoc, unsigned char area, unsigned int flags, bool bBiDirectional, AvHAIOffMeshConnection* RemoveConnectionDef)
{
	Vector ConnStart, ConnEnd;

	TraceResult hit;
	UTIL_TraceLine(StartLoc + Vector(0.0f, 0.0f, 5.0f), StartLoc - Vector(0.0f, 0.0f, 100.0f), ignore_monsters, ignore_glass, nullptr, &hit);

	ConnStart = (hit.flFraction < 1.0f) ? hit.vecEndPos : StartLoc;

	UTIL_TraceLine(EndLoc + Vector(0.0f, 0.0f, 5.0f), EndLoc - Vector(0.0f, 0.0f, 100.0f), ignore_monsters, ignore_glass, nullptr, &hit);

	ConnEnd = (hit.flFraction < 1.0f) ? hit.vecEndPos : EndLoc;

	ConnStart = Vector(ConnStart.x, ConnStart.z, -ConnStart.y);
	ConnEnd = Vector(ConnEnd.x, ConnEnd.z, -ConnEnd.y);

	for (int i = 0; i < BUILDING_NAV_MESH; i++)
	{
		dtOffMeshConnectionRef ref = 0;

		NavMeshes[i].tileCache->addOffMeshConnection(ConnStart, ConnEnd, 18.0f, area, flags, bBiDirectional, &ref);

		RemoveConnectionDef->ConnectionRefs[i] = (unsigned int)ref;
	}

	bNavMeshModified = true;
}

void UTIL_RemoveOffMeshConnections(AvHAIOffMeshConnection* RemoveConnectionDef)
{
	for (int i = 0; i < BUILDING_NAV_MESH; i++)
	{
		NavMeshes[i].tileCache->removeOffMeshConnection(RemoveConnectionDef->ConnectionRefs[i]);

		RemoveConnectionDef->ConnectionRefs[i] = 0;
	}

	bNavMeshModified = true;
}

const nav_profile GetBaseNavProfile(const int index)
{
	return BaseNavProfiles[index];
}

const dtOffMeshConnection* DEBUG_FindNearestOffMeshConnectionToPoint(const Vector Point, unsigned int FilterFlags)
{
	const dtOffMeshConnection* Result = nullptr;

	if (NavMeshes[REGULAR_NAV_MESH].tileCache)
	{
		float PointConverted[3] = { Point.x, Point.z, -Point.y };

		float minDist = 0.0f;
		

		for (int i = 0; i < NavMeshes[REGULAR_NAV_MESH].tileCache->getOffMeshCount(); i++)
		{
			const dtOffMeshConnection* con = NavMeshes[REGULAR_NAV_MESH].tileCache->getOffMeshConnection(i);

			if (!con || con->state == DT_OFFMESH_EMPTY || con->state == DT_OFFMESH_REMOVING || !(con->flags & FilterFlags)) { continue; }

			float distSpos = dtVdistSqr(PointConverted, &con->pos[0]);
			float distEpos = dtVdistSqr(PointConverted, &con->pos[3]);

			float thisDist = dtMin(distSpos, distEpos);

			if (!Result || thisDist < minDist)
			{
				Result = con;
				minDist = thisDist;
			}
		}
	}

	return Result;
}

dtStatus DEBUG_TestFindPath(const nav_profile& NavProfile, const Vector FromLocation, const Vector ToLocation, vector<bot_path_node>& path, float MaxAcceptableDistance)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery || !m_navMesh || !m_navFilter || vIsZero(FromLocation) || vIsZero(ToLocation))
	{
		return DT_FAILURE;
	}

	Vector FromFloorLocation = AdjustPointForPathfinding(FromLocation);
	Vector ToFloorLocation = AdjustPointForPathfinding(ToLocation);

	float pStartPos[3] = { FromFloorLocation.x, FromFloorLocation.z, -FromFloorLocation.y };
	float pEndPos[3] = { ToFloorLocation.x, ToFloorLocation.z, -ToFloorLocation.y };

	dtStatus status;
	dtPolyRef StartPoly;
	float StartNearest[3];
	dtPolyRef EndPoly;
	float EndNearest[3];
	dtPolyRef PolyPath[MAX_PATH_POLY];
	dtPolyRef StraightPolyPath[MAX_AI_PATH_SIZE];
	int nPathCount = 0;
	float StraightPath[MAX_AI_PATH_SIZE * 3];
	unsigned char straightPathFlags[MAX_AI_PATH_SIZE];
	memset(straightPathFlags, 0, sizeof(straightPathFlags));
	int nVertCount = 0;

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, pExtents, m_navFilter, &StartPoly, StartNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		//BotSay(pBot, "findNearestPoly start failed!");
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, pExtents, m_navFilter, &EndPoly, EndNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		//BotSay(pBot, "findNearestPoly end failed!");
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		return DT_FAILURE;
	}

	status = m_navQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, straightPathFlags, StraightPolyPath, &nVertCount, MAX_AI_PATH_SIZE, DT_STRAIGHTPATH_AREA_CROSSINGS);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't create a path
	}

	if (nVertCount == 0)
	{
		return DT_FAILURE; // couldn't find a path
	}

	path.clear();

	// At this point we have our path.  Copy it to the path store
	int nIndex = 0;

	Vector NodeFromLocation = FromFloorLocation;

	for (int nVert = 0; nVert < nVertCount; nVert++)
	{
		bot_path_node NextPathNode;

		NextPathNode.FromLocation = NodeFromLocation;

		NextPathNode.Location.x = StraightPath[nIndex++];
		NextPathNode.Location.z = StraightPath[nIndex++];
		NextPathNode.Location.y = -StraightPath[nIndex++];
		NextPathNode.area = SAMPLE_POLYAREA_GROUND;
		NextPathNode.flag = SAMPLE_POLYFLAGS_WALK;

		path.push_back(NextPathNode);

		NodeFromLocation = NextPathNode.Location;
	}



	return DT_SUCCESS;
}

void NAV_SetMoveMovementTask(AvHAIPlayer* pBot, Vector MoveLocation, DoorTrigger* TriggerToActivate)
{
	AvHAIPlayerMoveTask* MoveTask = &pBot->BotNavInfo.MovementTask;

	if (MoveTask->TaskType == MOVE_TASK_MOVE && vEquals(MoveTask->TaskLocation, MoveLocation)) { return; }

	if (vDist2DSq(pBot->CurrentFloorPosition, MoveLocation) < sqrf(GetPlayerRadius(pBot->Player)) && fabsf(pBot->CollisionHullBottomLocation.z - MoveLocation.z) < 50.0f) { return; }

	MoveTask->TaskType = MOVE_TASK_MOVE;
	MoveTask->TaskLocation = MoveLocation;

	vector<bot_path_node> Path;
	dtStatus PathStatus = FindPathClosestToPoint(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, MoveLocation, Path, 200.0f);

	if (dtStatusSucceed(PathStatus) && Path.size() > 0)
	{
		MoveTask->TaskLocation = Path.back().Location;
	}
}

void NAV_SetTouchMovementTask(AvHAIPlayer* pBot, edict_t* EntityToTouch, DoorTrigger* TriggerToActivate)
{
	AvHAIPlayerMoveTask* MoveTask = &pBot->BotNavInfo.MovementTask;

	if (MoveTask->TaskType == MOVE_TASK_TOUCH && MoveTask->TaskTarget == EntityToTouch) { return; }

	MoveTask->TaskType = MOVE_TASK_TOUCH;
	MoveTask->TaskTarget = EntityToTouch;
	MoveTask->TriggerToActivate = TriggerToActivate;

	vector<bot_path_node> Path;
	dtStatus PathStatus = FindPathClosestToPoint(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_GetCentreOfEntity(EntityToTouch), Path, 200.0f);

	if (dtStatusSucceed(PathStatus) && Path.size() > 0)
	{
		MoveTask->TaskLocation = Path.back().Location;
	}
}

void NAV_SetUseMovementTask(AvHAIPlayer* pBot, edict_t* EntityToUse, DoorTrigger* TriggerToActivate)
{
	AvHAIPlayerMoveTask* MoveTask = &pBot->BotNavInfo.MovementTask;

	if (MoveTask->TaskType == MOVE_TASK_USE && MoveTask->TaskTarget == EntityToUse) { return; }

	NAV_ClearMovementTask(pBot);

	MoveTask->TaskType = MOVE_TASK_USE;
	MoveTask->TaskTarget = EntityToUse;
	MoveTask->TriggerToActivate = TriggerToActivate;
	MoveTask->TaskLocation = UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, EntityToUse);
}

void NAV_SetBreakMovementTask(AvHAIPlayer* pBot, edict_t* EntityToBreak, DoorTrigger* TriggerToActivate)
{
	AvHAIPlayerMoveTask* MoveTask = &pBot->BotNavInfo.MovementTask;

	if (MoveTask->TaskType == MOVE_TASK_BREAK && MoveTask->TaskTarget == EntityToBreak) { return; }

	NAV_ClearMovementTask(pBot);

	MoveTask->TaskType = MOVE_TASK_BREAK;
	MoveTask->TaskTarget = EntityToBreak;
	MoveTask->TriggerToActivate = TriggerToActivate;

	MoveTask->TaskLocation = UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, EntityToBreak);
}

void NAV_SetWeldMovementTask(AvHAIPlayer* pBot, edict_t* EntityToWeld, DoorTrigger* TriggerToActivate)
{
	if (IsPlayerAlien(pBot->Edict)) { return; }

	AvHAIPlayerMoveTask* MoveTask = &pBot->BotNavInfo.MovementTask;

	if (MoveTask->TaskType == MOVE_TASK_WELD && MoveTask->TaskTarget == EntityToWeld) { return; }

	NAV_ClearMovementTask(pBot);

	MoveTask->TaskType = MOVE_TASK_WELD;
	MoveTask->TaskTarget = EntityToWeld;
	MoveTask->TriggerToActivate = TriggerToActivate;
	MoveTask->TaskLocation = UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, EntityToWeld);
}

void NAV_ClearMovementTask(AvHAIPlayer* pBot)
{
	pBot->BotNavInfo.MovementTask.TaskType = MOVE_TASK_NONE;
	pBot->BotNavInfo.MovementTask.TaskLocation = ZERO_VECTOR;
	pBot->BotNavInfo.MovementTask.TaskTarget = nullptr;
	pBot->BotNavInfo.MovementTask.TriggerToActivate = nullptr;
}

void NAV_ProgressMovementTask(AvHAIPlayer* pBot)
{
	AvHAIPlayerMoveTask* MoveTask = &pBot->BotNavInfo.MovementTask;

	if (MoveTask->TaskType == MOVE_TASK_NONE) { return; }

	if (MoveTask->TaskType == MOVE_TASK_USE)
	{
		if (IsPlayerInUseRange(pBot->Edict, MoveTask->TaskTarget))
		{
			BotUseObject(pBot, MoveTask->TaskTarget, false);
			ClearBotStuck(pBot);
			return;
		}
	}

	if (MoveTask->TaskType == MOVE_TASK_BREAK)
	{
		AvHAIWeapon Weapon = WEAPON_INVALID;

		if (IsPlayerMarine(pBot->Edict))
		{
			Weapon = BotMarineChooseBestWeaponForStructure(pBot, MoveTask->TaskTarget);
		}
		else
		{
			Weapon = BotAlienChooseBestWeaponForStructure(pBot, MoveTask->TaskTarget);
		}

		BotAttackResult AttackResult = PerformAttackLOSCheck(pBot, Weapon, MoveTask->TaskTarget);

		if (AttackResult == ATTACK_SUCCESS)
		{
			// If we were ducking before then keep ducking
			if (pBot->Edict->v.oldbuttons & IN_DUCK)
			{
				pBot->Button |= IN_DUCK;
			}

			BotShootTarget(pBot, Weapon, MoveTask->TaskTarget);

			ClearBotStuck(pBot);

			return;
		}
	}

	if (MoveTask->TaskType == MOVE_TASK_WELD)
	{
		if (IsPlayerInUseRange(pBot->Edict, MoveTask->TaskTarget))
		{
			Vector AimLocation;

			Vector EntityCentre = UTIL_GetCentreOfEntity(MoveTask->TaskTarget);

			if (MoveTask->TaskTarget->v.size.Length() < 100.0f)
			{
				AimLocation = EntityCentre;
			}
			else
			{
				Vector BBMin = MoveTask->TaskTarget->v.absmin + Vector(5.0f, 5.0f, 5.0f);
				Vector BBMax = MoveTask->TaskTarget->v.absmax - Vector(5.0f, 5.0f, 5.0f);

				vScaleBB(BBMin, BBMax, 0.75f);

				AimLocation = vClosestPointOnBB(pBot->CurrentEyePosition, BBMin, BBMax);

				if (MoveTask->TaskTarget->v.absmax.z - MoveTask->TaskTarget->v.absmin.z < 100.0f)
				{
					AimLocation.z = EntityCentre.z;
				}

			}

			BotMoveLookAt(pBot, AimLocation);
			pBot->DesiredMoveWeapon = WEAPON_MARINE_WELDER;

			if (GetPlayerCurrentWeapon(pBot->Player) != WEAPON_MARINE_WELDER)
			{
				return;
			}

			pBot->Button |= IN_ATTACK;

			ClearBotStuck(pBot);

			return;
		}
	}

	bool bSuccess = MoveTo(pBot, MoveTask->TaskLocation, MOVESTYLE_NORMAL);


}

bool NAV_IsMovementTaskStillValid(AvHAIPlayer* pBot)
{
	AvHAIPlayerMoveTask* MoveTask = &pBot->BotNavInfo.MovementTask;

	if (MoveTask->TaskType == MOVE_TASK_NONE) { return false; }

	if (MoveTask->TriggerToActivate)
	{
		if (!MoveTask->TriggerToActivate->bIsActivated) { return false; }
		if (MoveTask->TriggerToActivate->NextActivationTime > gpGlobals->time) { return false; }
	}

	if (MoveTask->TaskType == MOVE_TASK_MOVE)
	{
		return (vDist2DSq(pBot->Edict->v.origin, MoveTask->TaskLocation) > sqrf(GetPlayerRadius(pBot->Player)) || fabsf(pBot->Edict->v.origin.z - MoveTask->TaskLocation.z) > 50.0f)
			&& UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, MoveTask->TaskLocation, GetPlayerRadius(pBot->Edict));
	}

	if (MoveTask->TaskType == MOVE_TASK_USE)
	{
		if (MoveTask->TriggerToActivate)
		return MoveTask->TriggerToActivate && MoveTask->TriggerToActivate->bIsActivated && MoveTask->TriggerToActivate->NextActivationTime < gpGlobals->time;
	}

	if (MoveTask->TaskType == MOVE_TASK_PICKUP)
	{
		return (!FNullEnt(MoveTask->TaskTarget) && !(MoveTask->TaskTarget->v.effects & EF_NODRAW));
	}

	if (MoveTask->TaskType == MOVE_TASK_TOUCH)
	{
		return (!FNullEnt(MoveTask->TaskTarget) && !IsPlayerTouchingEntity(pBot->Edict, MoveTask->TaskTarget));
	}

	if (MoveTask->TaskType == MOVE_TASK_BREAK)
	{
		return (!FNullEnt(MoveTask->TaskTarget) && MoveTask->TaskTarget->v.deadflag == DEAD_NO && MoveTask->TaskTarget->v.health > 0.0f);
	}

	if (MoveTask->TaskType == MOVE_TASK_WELD)
	{
		AvHWeldable* WeldableRef = dynamic_cast<AvHWeldable*>(CBaseEntity::Instance(MoveTask->TaskTarget));

		if (WeldableRef)
		{
			return !WeldableRef->GetIsWelded();
		}
	}

	return false;

}

void NAV_SetPickupMovementTask(AvHAIPlayer* pBot, edict_t* ThingToPickup, DoorTrigger* TriggerToActivate)
{
	AvHAIPlayerMoveTask* MoveTask = &pBot->BotNavInfo.MovementTask;

	if (MoveTask->TaskType == MOVE_TASK_PICKUP && MoveTask->TaskTarget == ThingToPickup) { return; }

	NAV_ClearMovementTask(pBot);

	MoveTask->TaskType = MOVE_TASK_PICKUP;
	MoveTask->TaskTarget = ThingToPickup;
	MoveTask->TriggerToActivate = TriggerToActivate;
	MoveTask->TaskLocation = ThingToPickup->v.origin;
}

vector<NavHint*> NAV_GetHintsOfType(unsigned int HintType, bool bUnoccupiedOnly)
{
	vector<NavHint*> Result;

	Result.clear();

	for (auto it = MapNavHints.begin(); it != MapNavHints.end(); it++)
	{
		if (HintType != STRUCTURE_NONE && !(it->hintType & HintType)) { continue; }

		if (bUnoccupiedOnly && !FNullEnt(it->OccupyingBuilding)) { continue; }

		Result.push_back(&(*it));
	}

	return Result;

}

vector<NavHint*> NAV_GetHintsOfTypeInRadius(unsigned int HintType, Vector SearchLocation, float Radius, bool bUnoccupiedOnly)
{
	vector<NavHint*> Result;

	Result.clear();

	float SearchRadius = sqrf(Radius);

	for (auto it = MapNavHints.begin(); it != MapNavHints.end(); it++)
	{
		if (HintType != STRUCTURE_NONE && !(it->hintType & HintType)) { continue; }

		if (bUnoccupiedOnly && !FNullEnt(it->OccupyingBuilding)) { continue; }

		if (vDist3DSq(it->Position, SearchLocation) < SearchRadius)
		{
			Result.push_back(&(*it));
		}
	}

	return Result;

}

void RefineFlightPath(vector<bot_path_node>& InputPath, vector<bot_path_node>& RefinedPath)
{
	RefinedPath.clear();

	if (InputPath.size() == 0) { return; }

	if (InputPath.size() == 1)
	{
		RefinedPath.push_back(InputPath.front());
		return;
	}

	Vector PrevLoc = InputPath.front().FromLocation;

	for (auto it = InputPath.begin(); it != InputPath.end(); it++)
	{
		if (it->flag == SAMPLE_POLYFLAGS_FALL || next(it) == InputPath.end())
		{
			bot_path_node NewNode = (*it);
			NewNode.FromLocation = PrevLoc;
			PrevLoc = NewNode.Location;
			RefinedPath.push_back(NewNode);

			continue;
		}

		Vector Dir = UTIL_GetVectorNormal(it->Location - PrevLoc);
		
		float Dist = vDist3D(PrevLoc, it->Location);
		float StepSize = Dist * 0.1f;
		int CurrStep = 0;

		bool bSkippedAhead = false;

		vector<bot_path_node>::iterator StartIterator = next(it);
		int NodesSkipped = 0;

		while (NodesSkipped < 5 && next(StartIterator) != InputPath.end() && next(StartIterator)->flag != SAMPLE_POLYFLAGS_FALL)
		{
			StartIterator = next(StartIterator);

			NodesSkipped++;
		}

		for (auto nIt = StartIterator; nIt != next(it) && nIt != it; nIt--)
		{
			Vector TracePoint = ZERO_VECTOR;
			Vector TraceFrom = PrevLoc;
			CurrStep = 0;

			while (vIsZero(TracePoint) && CurrStep < 10)
			{
				TraceFrom = (PrevLoc + Vector(0.0f, 0.0f, 10.0f)) + (Dir * (StepSize * CurrStep));

				TracePoint = UTIL_FindHighestSuccessfulTracePoint(TraceFrom, nIt->FromLocation, nIt->Location, 5.0f, 50.0f, 200.0f);

				CurrStep++;
			}

			if (!vIsZero(TracePoint))
			{
				bot_path_node StubNode = (*it);
				StubNode.FromLocation = PrevLoc;
				StubNode.Location = TraceFrom;
				PrevLoc = StubNode.Location;
				RefinedPath.push_back(StubNode);

				bot_path_node NewNode = (*it);
				NewNode.FromLocation = PrevLoc;
				NewNode.Location = TracePoint;
				PrevLoc = NewNode.Location;
				RefinedPath.push_back(NewNode);
				bSkippedAhead = true;

				it = prev(nIt);

				it->FromLocation = TracePoint;

				break;
			}
		}

		if (!bSkippedAhead)
		{
			bot_path_node NewNode = (*it);
			NewNode.FromLocation = PrevLoc;
			PrevLoc = NewNode.Location;
			RefinedPath.push_back(NewNode);

			continue;
		}


	}
}