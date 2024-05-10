
#include "AvHAIPlayerUtil.h"
#include "AvHAIPlayer.h"
#include "AvHAIHelper.h"

#include "AvHPlayerUpgrade.h"
#include "AvHAIMath.h"
#include "AvHGamerules.h"
#include "../pm_shared/pm_shared.h"
#include "../pm_shared/pm_defs.h"

#include <cfloat>

bool IsPlayerSkulk(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER1);
}

bool IsPlayerGorge(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER2);
}

bool IsPlayerLerk(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER3);
}

bool IsPlayerFade(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER4);
}

bool IsPlayerOnos(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER5);
}

bool IsPlayerMarine(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_MARINE_PLAYER);
}

bool IsPlayerMarine(const AvHPlayer* Player)
{
	if (!Player) { return false; }
	return (Player->GetUser3() == AVH_USER3_MARINE_PLAYER);
}

bool IsPlayerAlien(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 != AVH_USER3_MARINE_PLAYER && Player->v.iuser3 != AVH_USER3_COMMANDER_PLAYER);
}

bool IsPlayerCommander(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_COMMANDER_PLAYER);
}

bool IsPlayerClimbingWall(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (IsPlayerSkulk(Player) && (Player->v.iuser4 & MASK_WALLSTICKING));
}

bool IsPlayerInReadyRoom(const edict_t* Player)
{
	return Player->v.playerclass == PLAYMODE_READYROOM;
}

bool IsPlayerActiveInGame(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }

	return !IsPlayerInReadyRoom(Player) && Player->v.team != 0 && !IsPlayerSpectator(Player) && !IsPlayerDead(Player) && !IsPlayerBeingDigested(Player) && !IsPlayerCommander(Player);
}

bool IsPlayerHuman(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (!(Player->v.flags & FL_FAKECLIENT));
}

bool IsPlayerBot(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.flags & FL_FAKECLIENT);
}

bool IsPlayerDead(const edict_t* Player)
{
	if (FNullEnt(Player)) { return true; }
	return (Player->v.deadflag != DEAD_NO || Player->v.health <= 0.0f);
}

bool IsPlayerStunned(const edict_t* Player)
{
	return !FNullEnt(Player) && !IsPlayerDead(Player) && !IsPlayerDigesting(Player) && (Player->v.iuser4 & MASK_PLAYER_STUNNED);
}

bool IsPlayerSpectator(const edict_t* Player)
{
	return !FNullEnt(Player) && (Player->v.playerclass == PLAYMODE_OBSERVER);
}

bool IsPlayerBeingDigested(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_DIGESTING && Player->v.effects & EF_NODRAW);
}

bool IsPlayerDigesting(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_DIGESTING && !(Player->v.effects & EF_NODRAW));
}

bool IsPlayerGestating(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_ALIEN_EMBRYO);
}

bool IsPlayerCharging(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_ALIEN_MOVEMENT);
}

bool IsPlayerBuffed(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_BUFFED);
}

bool IsPlayerOnLadder(const edict_t* Player)
{
	if (IsPlayerSkulk(Player))
	{
		edict_t* NearestLadder = UTIL_GetNearestLadderAtPoint(Player->v.origin);

		if (FNullEnt(NearestLadder)) { return false; }

		Vector NearestPointOnLadder = UTIL_GetClosestPointOnEntityToLocation(Player->v.origin, NearestLadder);
		Vector NearestPointOnPlayer = UTIL_GetClosestPointOnEntityToLocation(NearestPointOnLadder, Player);

		return (vDist2DSq(NearestPointOnLadder, NearestPointOnPlayer) <= sqrf(4.0f));
	}

	return (Player->v.movetype == MOVETYPE_FLY);
}

bool IsPlayerParasited(const edict_t* Player)
{
	return (Player->v.iuser4 & MASK_PARASITED);
}

bool IsPlayerMotionTracked(const edict_t* Player)
{
	return (Player->v.iuser4 & MASK_VIS_DETECTED);
}

float GetPlayerEnergy(const edict_t* Player)
{
	return (Player->v.fuser3 * 0.001f);
}

int GetPlayerMaxArmour(const edict_t* Player)
{
	if (FNullEnt(Player)) { return 0; }

	return AvHPlayerUpgrade::GetMaxArmorLevel(Player->v.iuser4, (AvHUser3)Player->v.iuser3);

}

int GetPlayerResources(const edict_t* Player)
{
	if (FNullEnt(Player)) { return 0; }

	return (int)ceil(Player->v.vuser4.z / kNumericNetworkConstant);
}

int GetPlayerCombatExperience(const edict_t* Player)
{
	if (FNullEnt(Player)) { return 0; }

	return (int)ceil(Player->v.vuser4.z / kNumericNetworkConstant);
}

int GetPlayerCombatLevel(const AvHPlayer* Player)
{
	if (!Player) { return 0; }

	return Player->GetExperienceLevel();
}

float GetPlayerRadius(const AvHPlayer* Player)
{
	if (!Player) { return 0.0f; }

	int hullnum = GetPlayerHullIndex(ENT(Player->pev));

	switch (hullnum)
	{
	case human_hull:
	case head_hull:
		return 16.0f;
		break;
	case large_hull:
		return 32.0f;
		break;
	default:
		return 16.0f;
		break;

	}
}

float GetPlayerRadius(const edict_t* Player)
{
	if (!Player) { return 0.0f; }

	int hullnum = GetPlayerHullIndex(Player);

	switch (hullnum)
	{
	case human_hull:
	case head_hull:
		return 16.0f;
		break;
	case large_hull:
		return 32.0f;
		break;
	default:
		return 16.0f;
		break;

	}
}

bool CanPlayerClimb(const edict_t* Player)
{
	return IsPlayerSkulk(Player) || IsPlayerFade(Player) || IsPlayerLerk(Player) || PlayerHasJetpack(Player);
}

float GetPlayerMaxJumpHeight(const edict_t* Player)
{
	return (CanPlayerCrouch(Player) ? max_player_crouchjump_height : max_player_normaljump_height);
}

bool CanPlayerCrouch(const edict_t* Player)
{
	if (FNullEnt(Player) || Player->free || !IsEdictPlayer(Player)) { return false; }

	switch (Player->v.iuser3)
	{
		case AVH_USER3_ALIEN_PLAYER1:
		case AVH_USER3_ALIEN_PLAYER2:
		case AVH_USER3_ALIEN_PLAYER3:
			return false;
		default:
			return true;
	}

	return false;
}

int GetPlayerHullIndex(const edict_t* Player, const bool bIsCrouching)
{
	if (!Player) { return 0; }

	AvHUser3 PlayerClass = (AvHUser3)Player->v.iuser3;

	switch (PlayerClass)
	{
	case AVH_USER3_MARINE_PLAYER:
	case AVH_USER3_ALIEN_PLAYER4: // Fade
		return (bIsCrouching) ? head_hull : human_hull;
	case AVH_USER3_COMMANDER_PLAYER:
		return head_hull;
	case AVH_USER3_ALIEN_EMBRYO: // Gestating
		return head_hull;
	case AVH_USER3_ALIEN_PLAYER1: // Skulk
	case AVH_USER3_ALIEN_PLAYER2: // Gorge
	case AVH_USER3_ALIEN_PLAYER3:// Lerk
		return head_hull;
	case AVH_USER3_ALIEN_PLAYER5: // Onos
		return (bIsCrouching) ? human_hull : large_hull;
	default:
		return head_hull;
	}

	return head_hull;
}

int GetPlayerHullIndex(const edict_t* Player)
{
	if (!Player) { return 0; }

	AvHUser3 PlayerClass = (AvHUser3)Player->v.iuser3;

	bool bIsCrouching = (Player->v.flags & FL_DUCKING);

	switch (PlayerClass)
	{
	case AVH_USER3_MARINE_PLAYER:
	case AVH_USER3_ALIEN_PLAYER4: // Fade
		return (bIsCrouching) ? head_hull : human_hull;
	case AVH_USER3_COMMANDER_PLAYER:
		return head_hull;
	case AVH_USER3_ALIEN_EMBRYO: // Gestating
		return head_hull;
	case AVH_USER3_ALIEN_PLAYER1: // Skulk
	case AVH_USER3_ALIEN_PLAYER2: // Gorge
	case AVH_USER3_ALIEN_PLAYER3:// Lerk
		return head_hull;
	case AVH_USER3_ALIEN_PLAYER5: // Onos
		return (bIsCrouching) ? human_hull : large_hull;
	default:
		return head_hull;
	}

	return head_hull;
}

float GetPlayerEnergyRegenPerSecond(edict_t* Player)
{
	int AdrenalineLevel = 0;

	if (Player->v.iuser4 & MASK_UPGRADE_5)
	{
		AdrenalineLevel = 1;

		if (Player->v.iuser4 & MASK_UPGRADE_13)
		{
			AdrenalineLevel = 3;
		}
		else if (Player->v.iuser4 & MASK_UPGRADE_12)
		{
			AdrenalineLevel = 2;
		}
	}

	return kAlienEnergyRate * (1.0f + (AdrenalineLevel * kAdrenalineEnergyPercentPerLevel));
}

float GetPlayerOverallHealthPercent(const edict_t* Player)
{
	if (IsEdictStructure(Player)) { return (Player->v.health / Player->v.max_health); }

	float MaxHealthAndArmour = Player->v.max_health + GetPlayerMaxArmour(Player);
	float CurrentHealthAndArmour = Player->v.health + Player->v.armorvalue;

	return (CurrentHealthAndArmour / MaxHealthAndArmour);
}

Vector GetPlayerEyePosition(const edict_t* Player)
{
	if (FNullEnt(Player)) { return g_vecZero; }

	return (Player->v.origin + Player->v.view_ofs);
}

float GetPlayerHeight(const edict_t* Player, const bool bIsCrouching)
{
	if (FNullEnt(Player)) { return 0.0f; }

	return GetPlayerOriginOffsetFromFloor(Player, bIsCrouching).z * 2.0f;
}

Vector GetPlayerOriginOffsetFromFloor(const edict_t* pEdict, const bool bIsCrouching)
{
	if (FNullEnt(pEdict)) { return g_vecZero; }

	int iuser3 = pEdict->v.iuser3;

	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? Vector(0.0f, 0.0f, 18.0f) : Vector(0.0f, 0.0f, 36.0f);
		break;
	case AVH_USER3_COMMANDER_PLAYER:
		return Vector(0.0f, 0.0f, 36.0f);
		break;
	case AVH_USER3_ALIEN_EMBRYO:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER1:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER2:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER3:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? Vector(0.0f, 0.0f, 18.0f) : Vector(0.0f, 0.0f, 36.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? Vector(0.0f, 0.0f, 36.0f) : Vector(0.0f, 0.0f, 54.0f);
		break;
	default:
		return Vector(0.0f, 0.0f, 36.0f);
		break;
	}
}

Vector GetPlayerBottomOfCollisionHull(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return g_vecZero; }

	int iuser3 = pEdict->v.iuser3;
	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);
	Vector origin = pEdict->v.origin;


	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 18.0f)) : (origin - Vector(0.0f, 0.0f, 36.0f));
		break;
	case AVH_USER3_COMMANDER_PLAYER:
		return origin;
		break;
	case AVH_USER3_ALIEN_EMBRYO:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER1:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER2:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER3:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 18.0f)) : (origin - Vector(0.0f, 0.0f, 36.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 36.0f)) : (origin - Vector(0.0f, 0.0f, 54.0f));
		break;
	default:
		return origin;
		break;
	}
}

Vector GetPlayerTopOfCollisionHull(const edict_t* pEdict, const bool bIsCrouching)
{
	if (FNullEnt(pEdict)) { return g_vecZero; }

	int iuser3 = pEdict->v.iuser3;
	Vector origin = pEdict->v.origin;


	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_COMMANDER_PLAYER:
		return origin;
	case AVH_USER3_ALIEN_EMBRYO:
	case AVH_USER3_ALIEN_PLAYER1:
	case AVH_USER3_ALIEN_PLAYER2:
	case AVH_USER3_ALIEN_PLAYER3:
		return (origin + Vector(0.0f, 0.0f, 19.0f));
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 37.0f)) : (origin + Vector(0.0f, 0.0f, 55.0f));
	default:
		return origin;
	}
}

Vector GetPlayerTopOfCollisionHull(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return g_vecZero; }

	if (!IsEdictPlayer(pEdict))
	{
		Vector Centre = UTIL_GetCentreOfEntity(pEdict);
		Centre.z = pEdict->v.absmax.z;

		return Centre;
	}

	int iuser3 = pEdict->v.iuser3;
	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);
	Vector origin = pEdict->v.origin;


	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_COMMANDER_PLAYER:
		return origin;
	case AVH_USER3_ALIEN_EMBRYO:
	case AVH_USER3_ALIEN_PLAYER1:
	case AVH_USER3_ALIEN_PLAYER2:
	case AVH_USER3_ALIEN_PLAYER3:
		return (origin + Vector(0.0f, 0.0f, 19.0f));
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 37.0f)) : (origin + Vector(0.0f, 0.0f, 55.0f));
	default:
		return origin;
	}
}

Vector GetPlayerAttemptedMoveDirection(const edict_t* Player)
{
	if (Player->v.button == 0) { return g_vecZero; }

	Vector ForwardDir = UTIL_GetForwardVector2D(Player->v.angles);
	Vector RightDir = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(ForwardDir, UP_VECTOR));

	if (Player->v.button & IN_FORWARD)
	{
		if (Player->v.button & IN_RIGHT)
		{
			return UTIL_GetVectorNormal2D(ForwardDir + RightDir);
		}

		if (Player->v.button & IN_LEFT)
		{
			return UTIL_GetVectorNormal2D(ForwardDir - RightDir);
		}

		return ForwardDir;
	}

	if (Player->v.button & IN_BACK)
	{
		Vector BackwardDir = -ForwardDir;
		Vector RightDir = UTIL_GetCrossProduct(BackwardDir, UP_VECTOR);

		if (Player->v.button & IN_RIGHT)
		{
			return UTIL_GetVectorNormal2D(BackwardDir - RightDir);
		}

		if (Player->v.button & IN_LEFT)
		{
			return UTIL_GetVectorNormal2D(BackwardDir + RightDir);
		}

		return BackwardDir;
	}

	if (Player->v.button & IN_RIGHT)
	{
		return RightDir;
	}

	if (Player->v.button & IN_LEFT)
	{
		return -RightDir;
	}

	return g_vecZero;
}

int GetPlayerIndex(AvHPlayer* Player)
{
	return Player->entindex();
	
}

bool IsEdictPlayer(const edict_t* edict)
{
	if (FNullEnt(edict)) { return false; }

	return ((edict->v.flags & FL_CLIENT) || (edict->v.flags & FL_FAKECLIENT));
}

bool IsPlayerTouchingEntity(const edict_t* Player, const edict_t* TargetEntity)
{
	CBaseEntity* TouchingEdict = nullptr;

	while ((TouchingEdict = UTIL_FindEntityInSphere(TouchingEdict, Player->v.origin, 5.0f)) != NULL)
	{
		if (TouchingEdict->edict() == TargetEntity) { return true; }
	}

	return false;
}

bool IsPlayerInUseRange(const edict_t* Player, const edict_t* Target)
{
	if (FNullEnt(Player) || FNullEnt(Target)) { return false; }

	//if (vDist3DSq(Player->v.origin, UTIL_GetCentreOfEntity(Target)) > sqrf(vSize3D(Target->v.size) + vSize3D(Player->v.size))) { return false; }

	CBaseEntity* UseObject = nullptr;

	while ((UseObject = UTIL_FindEntityInSphere(UseObject, Player->v.origin, 64.0f)) != NULL)
	{
		if (UseObject->edict() == Target) { return true; }
	}

	return false;
}

bool PlayerHasHeavyArmour(const edict_t* Player)
{
	if (!IsPlayerMarine(Player)) { return false; }
	return (Player->v.iuser4 & MASK_UPGRADE_13);
}

bool PlayerHasJetpack(const edict_t* Player)
{
	if (!IsPlayerMarine(Player)) { return false; }
	return (Player->v.iuser4 & MASK_UPGRADE_7);
}

bool PlayerHasEquipment(edict_t* Player)
{
	if (!IsPlayerMarine(Player)) { return false; }
	return PlayerHasHeavyArmour(Player) || PlayerHasJetpack(Player);
}

bool PlayerHasSpecialWeapon(const AvHPlayer* Player)
{
	if (!IsPlayerMarine(Player)) { return false; }
	return !PlayerHasWeapon(Player, WEAPON_MARINE_MG);
}

bool UTIL_PlayerHasLOSToEntity(const edict_t* Player, const edict_t* Target, const float MaxRange, const bool bUseHullSweep)
{
	if (FNullEnt(Player) || FNullEnt(Target)) { return false; }
	Vector StartTrace = GetPlayerEyePosition(Player);
	Vector EndTrace = UTIL_GetCentreOfEntity(Target);

	float Dist = vDist3D(StartTrace, EndTrace);

	TraceResult hit;

	if (bUseHullSweep)
	{
		UTIL_TraceHull(StartTrace, EndTrace, dont_ignore_monsters, head_hull, Player->v.pContainingEntity, &hit);
	}
	else
	{
		UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, Player->v.pContainingEntity, &hit);
	}



	if (hit.fStartSolid || (hit.flFraction < 1.0f && ((Dist * hit.flFraction) <= MaxRange)))
	{
		return (hit.pHit == Target);
	}
	else
	{
		return false;
	}
}

bool UTIL_PlayerHasLOSToLocation(const edict_t* Player, const Vector Target, const float MaxRange)
{
	if (FNullEnt(Player)) { return false; }
	Vector StartTrace = GetPlayerEyePosition(Player);

	if (vDist3DSq(StartTrace, Target) > sqrf(MaxRange)) { return false; }

	TraceResult hit;

	UTIL_TraceLine(StartTrace, Target, ignore_monsters, ignore_glass, Player->v.pContainingEntity, &hit);

	return (hit.flFraction >= 1.0f);

}

bool PlayerHasWeapon(const AvHPlayer* Player, const AvHAIWeapon DesiredCombatWeapon)
{
	bool HasWeaponInInventory = (Player->pev->weapons & (1 << DesiredCombatWeapon));

	// Marines don't have a fixed inventory, so we can just do a simple check for them. Same goes to confirm the alien has the weapon in their inventory
	if (IsPlayerMarine(Player) || !HasWeaponInInventory)
	{
		if (DesiredCombatWeapon == WEAPON_MARINE_GRENADE && HasWeaponInInventory)
		{
			AvHBasePlayerWeapon* Weapon = dynamic_cast<AvHBasePlayerWeapon*>(Player->m_rgpPlayerItems[5]);

			return Weapon->m_iClip > 0;
		}

		return HasWeaponInInventory;
	}

	// Aliens always have all weapons in their inventory, but they are enabled/disabled based on hive count (or combat unlocks).
	// Now we check to see if the weapon is enabled for them.

	edict_t* pEdict = ENT(Player->pev);

	// Which slot the weapon sits in
	int DesiredWeaponIndex = -1;

	switch (DesiredCombatWeapon)
	{
		case WEAPON_SKULK_BITE:
		case WEAPON_GORGE_SPIT:
		case WEAPON_LERK_BITE:
		case WEAPON_FADE_SWIPE:
		case WEAPON_ONOS_GORE:
			DesiredWeaponIndex = 1;
			break;
		case WEAPON_SKULK_PARASITE:
		case WEAPON_GORGE_HEALINGSPRAY:
		case WEAPON_LERK_SPORES:
		case WEAPON_FADE_BLINK:
		case WEAPON_ONOS_DEVOUR:
			DesiredWeaponIndex = 2;
			break;
		case WEAPON_SKULK_LEAP:
		case WEAPON_GORGE_BILEBOMB:
		case WEAPON_LERK_UMBRA:
		case WEAPON_FADE_METABOLIZE:
		case WEAPON_ONOS_STOMP:
			DesiredWeaponIndex = 3;
			break;
		case WEAPON_SKULK_XENOCIDE:
		case WEAPON_GORGE_WEB:
		case WEAPON_LERK_PRIMALSCREAM:
		case WEAPON_FADE_ACIDROCKET:
		case WEAPON_ONOS_CHARGE:
			DesiredWeaponIndex = 4;
			break;
		default:
			DesiredWeaponIndex = -1;
			break;
	}

	if (DesiredWeaponIndex < 0) { return false; }

	AvHBasePlayerWeapon* Weapon = dynamic_cast<AvHBasePlayerWeapon*>(Player->m_rgpPlayerItems[DesiredWeaponIndex]);

	return (Weapon && Weapon->m_iEnabled);
}

bool PlayerHasAlienUpgradeOfType(const edict_t* Player, const HiveTechStatus TechType)
{
	if (!IsPlayerAlien(Player)) { return false; }

	switch (TechType)
	{
	case HIVE_TECH_DEFENCE:
		return ((Player->v.iuser4 & MASK_UPGRADE_1) || (Player->v.iuser4 & MASK_UPGRADE_2) || (Player->v.iuser4 & MASK_UPGRADE_3));
	case HIVE_TECH_MOVEMENT:
		return ((Player->v.iuser4 & MASK_UPGRADE_4) || (Player->v.iuser4 & MASK_UPGRADE_5) || (Player->v.iuser4 & MASK_UPGRADE_6));
	case HIVE_TECH_SENSORY:
		return ((Player->v.iuser4 & MASK_UPGRADE_7) || (Player->v.iuser4 & MASK_UPGRADE_8) || (Player->v.iuser4 & MASK_UPGRADE_9));
	default:
		return false;
	}
}

float GetPlayerCloakAmount(const edict_t* Player)
{
	if (!(Player->v.iuser4 & MASK_UPGRADE_7) && !(Player->v.iuser4 & MASK_SENSORY_NEARBY)) { return 0.0f; }

	if (Player->v.iuser4 & MASK_VIS_SIGHTED)
	{
		return 0.0f;
	}
	else
	{
		return 1.0f;
	}
}

bool IsPlayerReloading(const AvHPlayer* Player)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(Player->m_pActiveItem);

	if (!theBasePlayerWeapon) { return false; }

	return (theBasePlayerWeapon->m_fInReload > 0 || theBasePlayerWeapon->m_fInSpecialReload > 0);
}

bool IsPlayerStandingOnPlayer(const edict_t* Player)
{
	return (IsEdictPlayer(Player->v.groundentity));
}

AvHUser3 GetPlayerActiveClass(const AvHPlayer* Player)
{
	if (Player->pev->iuser3 == AVH_USER3_ALIEN_EMBRYO) // If player is gestating...
	{		
		switch (Player->GetEvolution()) // If they're targeting a new life form (i.e. not getting upgrade), then return the new life form. Otherwise, return previous
		{
			case ALIEN_LIFEFORM_ONE:
				return AVH_USER3_ALIEN_PLAYER1;
			case ALIEN_LIFEFORM_TWO:
				return AVH_USER3_ALIEN_PLAYER2;
			case ALIEN_LIFEFORM_THREE:
				return AVH_USER3_ALIEN_PLAYER3;
			case ALIEN_LIFEFORM_FOUR:
				return AVH_USER3_ALIEN_PLAYER4;
			case ALIEN_LIFEFORM_FIVE:
				return AVH_USER3_ALIEN_PLAYER5;
			default: // Player is gestating an upgrade
				return Player->GetPreviousUser3(); 
		}
	}

	// Player isn't gestating, just return whatever they are now
	return (AvHUser3)Player->pev->iuser3;
}

edict_t* UTIL_GetNearestLadderAtPoint(const Vector SearchLocation)
{
	CBaseEntity* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	CBaseEntity* closestLadderRef = entity;
	float lowestDist = FLT_MAX;

	while (entity)
	{
		Vector LadderMin = entity->pev->absmin;
		Vector LadderMax = entity->pev->absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, SearchLocation);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	return (closestLadderRef) ? closestLadderRef->edict() : nullptr;
}

Vector UTIL_GetNearestLadderNormal(edict_t* pEdict)
{
	return UTIL_GetNearestLadderNormal(pEdict->v.origin);
}

Vector UTIL_GetNearestSurfaceNormal(Vector SearchLocation)
{

		Vector Trace1End, Trace2End, Trace3End, Trace4End, Trace5End, Trace6End, Trace7End, Trace8End;
		Trace1End = Trace2End = Trace3End = Trace4End = Trace5End = Trace6End = Trace7End = Trace8End = SearchLocation;

		Trace1End.x += 32.0f;
		Trace1End.y += 32.0f;

		Trace2End.x += 32.0f;
		Trace2End.y -= 32.0f;

		Trace3End.x -= 32.0f;
		Trace3End.y -= 32.0f;

		Trace4End.x -= 32.0f;
		Trace4End.y += 32.0f;

		Trace5End.x += 32.0f;

		Trace6End.x -= 32.0f;

		Trace7End.y -= 32.0f;

		Trace8End.y += 32.0f;

		Vector ClosestNormal = ZERO_VECTOR;
		float MinDist = 0.0f;

		trace_t TraceResult;
		NS_TraceLine(SearchLocation, Trace1End, 1, PM_NORMAL, -1, true, TraceResult);

		if (TraceResult.fraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(TraceResult.endpos);

			if (vIsZero(ClosestNormal) || TraceResult.fraction < MinDist)
			{
				ClosestNormal = TraceResult.plane.normal;
				MinDist = TraceResult.fraction;
			}
		}

		NS_TraceLine(SearchLocation, Trace2End, 1, PM_NORMAL, -1, true, TraceResult);

		if (TraceResult.fraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(TraceResult.endpos);

			if (vIsZero(ClosestNormal) || TraceResult.fraction < MinDist)
			{
				ClosestNormal = TraceResult.plane.normal;
				MinDist = TraceResult.fraction;
			}
		}

		NS_TraceLine(SearchLocation, Trace3End, 1, PM_NORMAL, -1, true, TraceResult);

		if (TraceResult.fraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(TraceResult.endpos);

			if (vIsZero(ClosestNormal) || TraceResult.fraction < MinDist)
			{
				ClosestNormal = TraceResult.plane.normal;
				MinDist = TraceResult.fraction;
			}
		}

		NS_TraceLine(SearchLocation, Trace4End, 1, PM_NORMAL, -1, true, TraceResult);

		if (TraceResult.fraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(TraceResult.endpos);

			if (vIsZero(ClosestNormal) || TraceResult.fraction < MinDist)
			{
				ClosestNormal = TraceResult.plane.normal;
				MinDist = TraceResult.fraction;
			}
		}

		NS_TraceLine(SearchLocation, Trace5End, 1, PM_NORMAL, -1, true, TraceResult);

		if (TraceResult.fraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(TraceResult.endpos);

			if (vIsZero(ClosestNormal) || TraceResult.fraction < MinDist)
			{
				ClosestNormal = TraceResult.plane.normal;
				MinDist = TraceResult.fraction;
			}
		}

		NS_TraceLine(SearchLocation, Trace6End, 1, PM_NORMAL, -1, true, TraceResult);

		if (TraceResult.fraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(TraceResult.endpos);

			if (vIsZero(ClosestNormal) || TraceResult.fraction < MinDist)
			{
				ClosestNormal = TraceResult.plane.normal;
				MinDist = TraceResult.fraction;
			}
		}

		NS_TraceLine(SearchLocation, Trace7End, 1, PM_NORMAL, -1, true, TraceResult);

		if (TraceResult.fraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(TraceResult.endpos);

			if (vIsZero(ClosestNormal) || TraceResult.fraction < MinDist)
			{
				ClosestNormal = TraceResult.plane.normal;
				MinDist = TraceResult.fraction;
			}
		}

		NS_TraceLine(SearchLocation, Trace8End, 1, PM_NORMAL, -1, true, TraceResult);

		if (TraceResult.fraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(TraceResult.endpos);

			if (vIsZero(ClosestNormal) || TraceResult.fraction < MinDist)
			{
				ClosestNormal = TraceResult.plane.normal;
				MinDist = TraceResult.fraction;
			}
		}

		return ClosestNormal;
}

Vector UTIL_GetNearestLadderNormal(Vector SearchLocation)
{
	TraceResult result;
	CBaseEntity* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	CBaseEntity* closestLadderRef = entity;
	float lowestDist = FLT_MAX;

	while (entity)
	{
		Vector LadderMin = entity->pev->absmin;
		Vector LadderMax = entity->pev->absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, SearchLocation);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	if (closestLadderRef)
	{
		if (vPointOverlaps3D(SearchLocation, closestLadderRef->pev->absmin, closestLadderRef->pev->absmax))
		{
			return UTIL_GetNearestSurfaceNormal(SearchLocation);
		}
		else
		{
			Vector CentrePoint = closestLadderRef->pev->absmin + (closestLadderRef->pev->size * 0.5f);
			CentrePoint.z = SearchLocation.z;

			trace_t TraceResult;
			NS_TraceLine(SearchLocation, CentrePoint, 1, PM_WORLD_ONLY, -1, true, TraceResult);

			if (TraceResult.fraction < 1.0f)
			{
				return TraceResult.plane.normal;
			}
		}
	}

	return ZERO_VECTOR;
}

Vector UTIL_GetNearestLadderBottomPoint(edict_t* pEdict)
{
	TraceResult result;
	CBaseEntity* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	CBaseEntity* closestLadderRef = entity;
	float lowestDist = 999999.0f;

	while (entity)
	{
		Vector LadderMin = entity->pev->absmin;
		Vector LadderMax = entity->pev->absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, pEdict->v.origin);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	if (closestLadderRef)
	{
		Vector Centre = (closestLadderRef->pev->absmin + (closestLadderRef->pev->size * 0.5f));
		Centre.z = closestLadderRef->pev->absmin.z;
		return Centre;

	}

	return pEdict->v.origin;
}

Vector UTIL_GetNearestLadderTopPoint(const Vector SearchLocation)
{
	TraceResult result;
	CBaseEntity* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	CBaseEntity* closestLadderRef = entity;
	float lowestDist = 999999.0f;

	while (entity)
	{
		Vector LadderMin = entity->pev->absmin;
		Vector LadderMax = entity->pev->absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, SearchLocation);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	if (closestLadderRef)
	{
		Vector Centre = (closestLadderRef->pev->absmin + ((closestLadderRef->pev->absmax - closestLadderRef->pev->absmin) * 0.5f));
		Centre.z = closestLadderRef->pev->absmax.z;
		return Centre;

	}

	return SearchLocation;
}

Vector UTIL_GetNearestLadderTopPoint(edict_t* pEdict)
{
	return UTIL_GetNearestLadderTopPoint(pEdict->v.origin);
}

Vector UTIL_GetNearestLadderCentrePoint(edict_t* pEdict)
{
	return UTIL_GetNearestLadderCentrePoint(pEdict->v.origin);
}

Vector UTIL_GetNearestLadderCentrePoint(const Vector SearchLocation)
{
	TraceResult result;
	CBaseEntity* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	CBaseEntity* closestLadderRef = entity;
	float lowestDist = 999999.0f;

	while (entity)
	{
		Vector LadderMin = entity->pev->absmin;
		Vector LadderMax = entity->pev->absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, SearchLocation);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	if (closestLadderRef)
	{
		return (closestLadderRef->pev->absmin + ((closestLadderRef->pev->absmax - closestLadderRef->pev->absmin) * 0.5f));

	}

	return SearchLocation;
}

void AIPlayer_Say(edict_t* pEntity, int teamonly, const char* Msg)
{
	AvHPlayer* client;
	AvHPlayer* theTalkingPlayer = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(pEntity));
	int			j;
	char* p;
	char		text[256];
	bool		theTalkerInReadyRoom = theTalkingPlayer->GetInReadyRoom();

	// We can get a raw string now, without the "say " prepended
	if (!Msg)
		return;

	//Not yet.
	if (theTalkingPlayer->m_flNextChatTime > gpGlobals->time)
		return;

	p = (char*)Msg;

	// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p) - 1] = 0;
	}

	// make sure the text has content
	char* pc = p;
	for (pc = p; pc != NULL && *pc != 0; pc++)
	{
		if (isprint(*pc) && !isspace(*pc))
		{
			pc = NULL;	// we've found an alphanumeric character,  so text is valid
			break;
		}
	}
	if (pc != NULL)
		return;  // no character found, so say nothing

	// turn on color set 2  (color on,  no sound)
	if (teamonly)
		sprintf(text, "%c(TEAM) %s: ", 2, STRING(pEntity->v.netname));
	else
		sprintf(text, "%c%s: ", 2, STRING(pEntity->v.netname));

	j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
	if ((int)strlen(p) > j)
		p[j] = 0;

	strcat(text, p);
	strcat(text, "\n");

	theTalkingPlayer->m_flNextChatTime = gpGlobals->time + CHAT_INTERVAL;
	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = NULL;
	while (((client = (AvHPlayer*)UTIL_FindEntityByClassname(client, "player")) != NULL) && (!FNullEnt(client->edict())))
	{
		if (!client->pev)
			continue;

		if (client->edict() == pEntity)
			continue;

		if (!(client->IsNetClient()))	// Not a client ? (should never be true)
			continue;

		// Don't differentiate between team and non-team when not playing
		bool theTalkingPlayerIsPlaying = ((theTalkingPlayer->GetPlayMode() == PLAYMODE_PLAYING) || (theTalkingPlayer->GetPlayMode() == PLAYMODE_AWAITINGREINFORCEMENT) || (theTalkingPlayer->GetPlayMode() == PLAYMODE_REINFORCING));
		bool theClientIsPlaying = ((client->GetPlayMode() == PLAYMODE_PLAYING) || (client->GetPlayMode() == PLAYMODE_AWAITINGREINFORCEMENT) || (client->GetPlayMode() == PLAYMODE_REINFORCING));
		bool theTalkerIsObserver = theTalkingPlayer->IsObserver();
		bool theClientIsObserver = client->IsObserver();
		bool theClientIsHLTV = (client->pev->flags & FL_PROXY);

		bool theClientInReadyRoom = client->GetInReadyRoom();

		if (theClientInReadyRoom != theTalkerInReadyRoom && !theClientIsHLTV)
		{
			continue;
		}

		if (!theClientIsObserver || theClientIsPlaying) // Non-playing Observers hear everything.
		{

			if (theTalkingPlayerIsPlaying && teamonly && g_pGameRules->PlayerRelationship(client, CBaseEntity::Instance(pEntity)) != GR_TEAMMATE)
				continue;

			// chat can never go between play area and non-play area
			if (theTalkingPlayerIsPlaying != theClientIsPlaying && !theClientIsHLTV)
				continue;

			// chat of any kind doesn't go from ready room to play area in tournament mode
			if (theTalkerInReadyRoom && GetGameRules()->GetIsTournamentMode() && theClientIsPlaying && !theClientIsHLTV)
				continue;

		}

		UTIL_SayText(text, client, ENTINDEX(pEntity));

	}

	// print to the sending client
	UTIL_SayText(text, CBaseEntity::Instance(ENT(pEntity)));

	// echo to server console
	g_engfuncs.pfnServerPrint(text);

	//	UTIL_LogPrintf( "%s %s \"%s\"\n", GetLogStringForPlayer( pEntity ).c_str(), temp, p );
}