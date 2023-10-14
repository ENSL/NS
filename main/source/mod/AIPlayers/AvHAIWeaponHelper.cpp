
#include "AvHAIWeaponHelper.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIMath.h"
#include "AvHAINavigation.h"
#include "AvHAIHelper.h"
#include "AvHAITactical.h"
#include "AvHAIPlayerManager.h"

#include "../AvHGamerules.h"
#include "../AvHAlienWeaponConstants.h"
#include "../AvHAlienWeapons.h"
#include "../AvHMarineEquipmentConstants.h"
#include "../AvHServerUtil.h"

int BotGetCurrentWeaponClipAmmo(const AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_pActiveItem);

	if (theBasePlayerWeapon)
	{
		return theBasePlayerWeapon->m_iClip;
	}

	return 0;
}

int BotGetCurrentWeaponMaxClipAmmo(const AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_pActiveItem);

	if (theBasePlayerWeapon)
	{
		return theBasePlayerWeapon->iMaxClip();
	}

	return 0;
}

int BotGetCurrentWeaponReserveAmmo(const AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_pActiveItem);

	if (theBasePlayerWeapon)
	{
		return pBot->Player->m_rgAmmo[theBasePlayerWeapon->m_iPrimaryAmmoType];
	}

	return 0;
}

float GetProjectileVelocityForWeapon(const AvHAIWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_GORGE_SPIT:
		return (float)BALANCE_VAR(kSpitVelocity);
	case WEAPON_LERK_SPORES:
		return (float)BALANCE_VAR(kShootCloudVelocity);
	case WEAPON_FADE_ACIDROCKET:
		return (float)BALANCE_VAR(kAcidRocketVelocity);
	case WEAPON_GORGE_BILEBOMB:
		return (float)BALANCE_VAR(kBileBombVelocity);
	case WEAPON_MARINE_GRENADE:
	case WEAPON_MARINE_GL:
		return (float)BALANCE_VAR(kGrenadeForce);
	default:
		return 0.0f; // Hitscan. We don't bother with bile bomb as it's so short range that it doesn't really need leading the target
	}
}

bool CanInterruptWeaponReload(AvHAIWeapon Weapon)
{
	switch (Weapon)
	{
		case WEAPON_MARINE_SHOTGUN:
		case WEAPON_MARINE_GL:
			return true;
		default:
			return false;
	}

	return false;
}

float GetReloadTimeForWeapon(AvHAIWeapon Weapon)
{
	switch (Weapon)
	{
		case WEAPON_MARINE_PISTOL:
		case WEAPON_MARINE_MG:
			return 3.0f;
		case WEAPON_MARINE_HMG:
			return 6.3f;
		case WEAPON_MARINE_SHOTGUN:
			return 0.22f;
		case WEAPON_MARINE_GL:
			return 1.5f;
		default:
			return 0.0f;
	}

	return 0.0f;
}

float GetEnergyCostForWeapon(const AvHAIWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_SKULK_BITE:
		return kBiteEnergyCost;
	case WEAPON_SKULK_PARASITE:
		return kParasiteEnergyCost;
	case WEAPON_SKULK_LEAP:
		return kLeapEnergyCost;
	case WEAPON_SKULK_XENOCIDE:
		return kDivineWindEnergyCost;

	case WEAPON_GORGE_SPIT:
		return kSpitEnergyCost;
	case WEAPON_GORGE_HEALINGSPRAY:
		return kHealingSprayEnergyCost;
	case WEAPON_GORGE_BILEBOMB:
		return kBileBombEnergyCost;
	case WEAPON_GORGE_WEB:
		return kWebEnergyCost;

	case WEAPON_LERK_BITE:
		return kBite2EnergyCost;
	case WEAPON_LERK_SPORES:
		return kSporesEnergyCost;
	case WEAPON_LERK_UMBRA:
		return kUmbraEnergyCost;
	case WEAPON_LERK_PRIMALSCREAM:
		return kPrimalScreamEnergyCost;

	case WEAPON_FADE_SWIPE:
		return kSwipeEnergyCost;
	case WEAPON_FADE_BLINK:
		return kBlinkEnergyCost;
	case WEAPON_FADE_METABOLIZE:
		return kMetabolizeEnergyCost;
	case WEAPON_FADE_ACIDROCKET:
		return kAcidRocketEnergyCost;

	case WEAPON_ONOS_GORE:
		return kClawsEnergyCost;
	case WEAPON_ONOS_DEVOUR:
		return kDevourEnergyCost;
	case WEAPON_ONOS_STOMP:
		return kStompEnergyCost;
	case WEAPON_ONOS_CHARGE:
		return kChargeEnergyCost;

	default:
		return 0.0f;
	}
}

void InterruptReload(AvHAIPlayer* pBot)
{
	pBot->Button |= IN_ATTACK;
}

AvHAIWeapon UTIL_GetBotPrimaryWeapon(const AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* Weapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_rgpPlayerItems[1]);

	if (Weapon)
	{
		return (AvHAIWeapon)Weapon->m_iId;
	}

	return WEAPON_INVALID;
}

bool IsHitscanWeapon(AvHAIWeapon Weapon)
{
	switch (Weapon)
	{
		case WEAPON_MARINE_MG:
		case WEAPON_MARINE_HMG:
		case WEAPON_MARINE_PISTOL:
		case WEAPON_MARINE_SHOTGUN:
		case WEAPON_SKULK_PARASITE:
		case WEAPON_MARINE_WELDER:
			return true;
		default:
			return false;
	}

	return false;
}


AvHAIWeapon GetBotMarineSecondaryWeapon(const AvHAIPlayer* pBot)
{
	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_PISTOL))
	{
		return WEAPON_MARINE_PISTOL;
	}

	return WEAPON_INVALID;
}

int BotGetPrimaryWeaponMaxAmmoReserve(AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_rgpPlayerItems[1]);

	if (theBasePlayerWeapon)
	{
		return theBasePlayerWeapon->iMaxAmmo1();
	}

	return 0;
}

int BotGetPrimaryWeaponAmmoReserve(AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_rgpPlayerItems[1]);

	if (theBasePlayerWeapon)
	{
		return pBot->Player->m_rgAmmo[theBasePlayerWeapon->m_iPrimaryAmmoType];
	}

	return 0;
}

int BotGetSecondaryWeaponAmmoReserve(AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_rgpPlayerItems[2]);

	if (theBasePlayerWeapon)
	{
		return pBot->Player->m_rgAmmo[theBasePlayerWeapon->m_iPrimaryAmmoType];
	}

	return 0;
}

int BotGetPrimaryWeaponClipAmmo(const AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_rgpPlayerItems[1]);

	if (theBasePlayerWeapon)
	{
		return theBasePlayerWeapon->m_iClip;
	}

	return 0;
}

int BotGetSecondaryWeaponClipAmmo(const AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_rgpPlayerItems[2]);

	if (theBasePlayerWeapon)
	{
		return theBasePlayerWeapon->m_iClip;
	}

	return 0;
}

int BotGetPrimaryWeaponMaxClipSize(const AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_rgpPlayerItems[1]);

	if (theBasePlayerWeapon)
	{
		return theBasePlayerWeapon->iMaxClip();
	}

	return 0;
}

int BotGetSecondaryWeaponMaxClipSize(const AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_rgpPlayerItems[2]);

	if (theBasePlayerWeapon)
	{
		return theBasePlayerWeapon->iMaxClip();
	}

	return 0;
}

int BotGetSecondaryWeaponMaxAmmoReserve(AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_rgpPlayerItems[2]);

	if (theBasePlayerWeapon)
	{
		return theBasePlayerWeapon->iMaxClip();
	}

	return 0;
}

float GetMaxIdealWeaponRange(const AvHAIWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_LERK_PRIMALSCREAM:
		return UTIL_MetresToGoldSrcUnits(100.0f);
	case WEAPON_LERK_SPORES:
	case WEAPON_LERK_UMBRA:
		return UTIL_MetresToGoldSrcUnits(50.0f);
	case WEAPON_MARINE_GL:
	case WEAPON_MARINE_MG:
	case WEAPON_MARINE_PISTOL:
	case WEAPON_FADE_ACIDROCKET:
	case WEAPON_SKULK_PARASITE:
	case WEAPON_SKULK_LEAP:
	case WEAPON_ONOS_CHARGE:
		return UTIL_MetresToGoldSrcUnits(20.0f);
	case WEAPON_MARINE_HMG:
	case WEAPON_MARINE_GRENADE:
		return UTIL_MetresToGoldSrcUnits(10.0f);
	case WEAPON_MARINE_SHOTGUN:
	case WEAPON_GORGE_BILEBOMB:
	case WEAPON_ONOS_STOMP:
		return UTIL_MetresToGoldSrcUnits(8.0f);
	case WEAPON_SKULK_XENOCIDE:
		return UTIL_MetresToGoldSrcUnits(5.0f);
	case WEAPON_ONOS_GORE:
		return BALANCE_VAR(kClawsRange);
	case WEAPON_ONOS_DEVOUR:
		return BALANCE_VAR(kDevourRange);
	case WEAPON_FADE_SWIPE:
		return BALANCE_VAR(kSwipeRange);
	case WEAPON_SKULK_BITE:
		return BALANCE_VAR(kBiteRange);
	case WEAPON_LERK_BITE:
		return BALANCE_VAR(kBite2Range);
	case WEAPON_GORGE_HEALINGSPRAY:
		return BALANCE_VAR(kHealingSprayRange);
	case WEAPON_MARINE_WELDER:
		return BALANCE_VAR(kWelderRange);
	default:
		return max_player_use_reach;
	}
}

float GetMinIdealWeaponRange(const AvHAIWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_MARINE_GL:
	case WEAPON_MARINE_GRENADE:
	case WEAPON_FADE_ACIDROCKET:
		return UTIL_MetresToGoldSrcUnits(5.0f);
	case WEAPON_SKULK_LEAP:
		return UTIL_MetresToGoldSrcUnits(3.0f);
	case WEAPON_MARINE_MG:
	case WEAPON_MARINE_PISTOL:
	case WEAPON_MARINE_HMG:
	case WEAPON_SKULK_PARASITE:
		return UTIL_MetresToGoldSrcUnits(5.0f);
	case WEAPON_MARINE_SHOTGUN:
		return UTIL_MetresToGoldSrcUnits(2.0f);
	case WEAPON_GORGE_BILEBOMB:
	case WEAPON_ONOS_STOMP:
		return UTIL_MetresToGoldSrcUnits(2.0f);
	default:
		return max_player_use_reach * 0.5f;
	}
}

bool IsMeleeWeapon(const AvHAIWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_MARINE_KNIFE:
	case WEAPON_SKULK_BITE:
	case WEAPON_FADE_SWIPE:
	case WEAPON_ONOS_GORE:
	case WEAPON_ONOS_DEVOUR:
	case WEAPON_LERK_BITE:
		return true;
	default:
		return false;
	}
}

bool WeaponCanBeReloaded(const AvHAIWeapon CheckWeapon)
{
	switch (CheckWeapon)
	{
	case WEAPON_MARINE_GL:
	case WEAPON_MARINE_HMG:
	case WEAPON_MARINE_MG:
	case WEAPON_MARINE_PISTOL:
	case WEAPON_MARINE_SHOTGUN:
		return true;
	default:
		return false;

	}
}


Vector UTIL_GetGrenadeThrowTarget(edict_t* Player, const Vector TargetLocation, const float ExplosionRadius, bool bPrecise)
{
	if (UTIL_PlayerHasLOSToLocation(Player, TargetLocation, UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		return TargetLocation;
	}

	if (UTIL_PointIsDirectlyReachable(Player->v.origin, TargetLocation))
	{
		Vector Orientation = UTIL_GetVectorNormal(Player->v.origin - TargetLocation);

		Vector NewSpot = TargetLocation + (Orientation * UTIL_MetresToGoldSrcUnits(1.5f));

		NewSpot = UTIL_ProjectPointToNavmesh(NewSpot);

		if (NewSpot != ZERO_VECTOR)
		{
			NewSpot.z += 10.0f;
		}

		return NewSpot;
	}

	bot_path_node CheckPath[MAX_AI_PATH_SIZE];
	int PathSize = 0;

	dtStatus Status = FindPathClosestToPoint(BaseNavProfiles[ALL_NAV_PROFILE], Player->v.origin, TargetLocation, CheckPath, &PathSize, ExplosionRadius);

	if (dtStatusSucceed(Status))
	{
		Vector FurthestPointVisible = UTIL_GetFurthestVisiblePointOnPath(GetPlayerEyePosition(Player), CheckPath, PathSize, bPrecise);

		if (vDist3DSq(FurthestPointVisible, TargetLocation) <= sqrf(ExplosionRadius))
		{
			return FurthestPointVisible;
		}

		Vector ThrowDir = UTIL_GetVectorNormal(FurthestPointVisible - Player->v.origin);

		Vector LineEnd = FurthestPointVisible + (ThrowDir * UTIL_MetresToGoldSrcUnits(5.0f));

		Vector ClosestPointInTrajectory = vClosestPointOnLine(FurthestPointVisible, LineEnd, TargetLocation);

		ClosestPointInTrajectory = UTIL_ProjectPointToNavmesh(ClosestPointInTrajectory);
		ClosestPointInTrajectory.z += 10.0f;

		if (vDist2DSq(ClosestPointInTrajectory, TargetLocation) < sqrf(ExplosionRadius) && UTIL_PlayerHasLOSToLocation(Player, ClosestPointInTrajectory, UTIL_MetresToGoldSrcUnits(10.0f)) && UTIL_PointIsDirectlyReachable(ClosestPointInTrajectory, TargetLocation))
		{
			return ClosestPointInTrajectory;
		}
		else
		{
			return ZERO_VECTOR;
		}
	}
	else
	{
		return ZERO_VECTOR;
	}
}

AvHAIWeapon BotMarineChooseBestWeapon(AvHAIPlayer* pBot, edict_t* target)
{

	if (FNullEnt(target))
	{
		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			return UTIL_GetBotPrimaryWeapon(pBot);
		}
		else if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			return GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			return UTIL_GetBotPrimaryWeapon(pBot);
		}
	}

	if (IsEdictPlayer(target))
	{
		float DistFromEnemy = vDist2DSq(pBot->Edict->v.origin, target->v.origin);

		if (UTIL_GetBotPrimaryWeapon(pBot) == WEAPON_MARINE_GL)
		{
			if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 && DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
			{
				return WEAPON_MARINE_GL;
			}

			if (BotGetSecondaryWeaponClipAmmo(pBot) > 0)
			{
				return GetBotMarineSecondaryWeapon(pBot);
			}

			return WEAPON_MARINE_KNIFE;
		}

		if (DistFromEnemy <= sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
			if (BotGetPrimaryWeaponClipAmmo(pBot) == 0)
			{
				if (BotGetSecondaryWeaponClipAmmo(pBot) > 0)
				{
					return GetBotMarineSecondaryWeapon(pBot);
				}
				else
				{
					return WEAPON_MARINE_KNIFE;
				}
			}
			else
			{
				return UTIL_GetBotPrimaryWeapon(pBot);
			}
		}
		else
		{
			AvHAIWeapon PrimaryWeapon = UTIL_GetBotPrimaryWeapon(pBot);

			if (PrimaryWeapon == WEAPON_MARINE_SHOTGUN)
			{
				if (DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
				{
					if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
					{
						return GetBotMarineSecondaryWeapon(pBot);
					}
					else
					{
						if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
						{
							return PrimaryWeapon;
						}
						else
						{
							return WEAPON_MARINE_KNIFE;
						}
					}
				}
				else
				{
					if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
					{
						return PrimaryWeapon;
					}
					else
					{
						if (BotGetSecondaryWeaponClipAmmo(pBot) > 0)
						{
							return GetBotMarineSecondaryWeapon(pBot);
						}
						else
						{
							return WEAPON_MARINE_KNIFE;
						}
					}
				}
			}
			else
			{
				if (DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
					{
						return PrimaryWeapon;
					}

					if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
					{
						return GetBotMarineSecondaryWeapon(pBot);
					}

					return WEAPON_MARINE_KNIFE;
				}
				else
				{
					if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || (DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && BotGetPrimaryWeaponAmmoReserve(pBot) > 0))
					{
						return PrimaryWeapon;
					}
					else
					{
						if (BotGetSecondaryWeaponClipAmmo(pBot) > 0)
						{
							return GetBotMarineSecondaryWeapon(pBot);
						}
						else
						{
							return WEAPON_MARINE_KNIFE;
						}
					}
				}
			}
		}
	}
	else
	{
		return BotMarineChooseBestWeaponForStructure(pBot, target);
	}
}

AvHAIWeapon BotAlienChooseBestWeaponForStructure(AvHAIPlayer* pBot, edict_t* target)
{
	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(target);

	if (StructureType == STRUCTURE_NONE)
	{
		return UTIL_GetBotPrimaryWeapon(pBot);
	}

	if (PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB))
	{
		return WEAPON_GORGE_BILEBOMB;
	}

	// If we have xenocide, then choose it if we have lots of good targets in blast radius
	if (PlayerHasWeapon(pBot->Player, WEAPON_SKULK_XENOCIDE))
	{
		AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());

		int NumEnemyTargetsInArea = AITAC_GetNumPlayersOfTeamInArea(EnemyTeam, target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), false, nullptr, AVH_USER3_NONE);

		AvHTeam* EnemyTeamRef = GetGameRules()->GetTeam(EnemyTeam);

		if (EnemyTeamRef)
		{
			AvHAIDeployableStructureType StructureSearchType = (EnemyTeamRef->GetTeamType() == AVH_CLASS_TYPE_MARINE) ? SEARCH_ALL_MARINE_STRUCTURES : SEARCH_ALL_ALIEN_STRUCTURES;

			DeployableSearchFilter SearchFilter;
			SearchFilter.DeployableTypes = StructureSearchType;
			SearchFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);
			SearchFilter.Team = EnemyTeam;

			NumEnemyTargetsInArea += AITAC_GetNumDeployablesNearLocation(target->v.origin, &SearchFilter);
		}
		
		if (NumEnemyTargetsInArea > 2)
		{
			return WEAPON_SKULK_XENOCIDE;
		}
	}

	return UTIL_GetBotPrimaryWeapon(pBot);
}

AvHAIWeapon BotMarineChooseBestWeaponForStructure(AvHAIPlayer* pBot, edict_t* target)
{
	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(target);

	if (StructureType == STRUCTURE_NONE)
	{
		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			return UTIL_GetBotPrimaryWeapon(pBot);
		}
		else if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			return GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			return WEAPON_MARINE_KNIFE;
		}
	}

	if (StructureType == STRUCTURE_ALIEN_HIVE || StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
	{
		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			return UTIL_GetBotPrimaryWeapon(pBot);
		}
		else if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			return GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			return WEAPON_MARINE_KNIFE;
		}
	}
	else
	{
		AvHAIWeapon PrimaryWeapon = UTIL_GetBotPrimaryWeapon(pBot);

		if ((PrimaryWeapon == WEAPON_MARINE_GL || PrimaryWeapon == WEAPON_MARINE_SHOTGUN) && (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0))
		{
			return PrimaryWeapon;
		}

		return WEAPON_MARINE_KNIFE;
	}

	return UTIL_GetBotPrimaryWeapon(pBot);
}

AvHAIWeapon GorgeGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	// Apparently I only imagined bile bomb doing damage to marine armour. Leaving it commented out in case we want to enable it again in future
	/*if (Target->v.armorvalue > 0.0f && PlayerHasWeapon(pBot->Edict, WEAPON_GORGE_BILEBOMB) && vDist2DSq(pBot->Edict->v.origin, Target->v.origin) < sqrf(GetMaxIdealWeaponRange(WEAPON_GORGE_BILEBOMB)))
	{
		return WEAPON_GORGE_BILEBOMB;
	}*/

	return WEAPON_GORGE_SPIT;
}

AvHAIWeapon SkulkGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || IsPlayerDead(Target))
	{
		return WEAPON_SKULK_BITE;
	}

	// If we have xenocide, then choose it if we have lots of good targets in blast radius
	if (PlayerHasWeapon(pBot->Player, WEAPON_SKULK_XENOCIDE))
	{
		AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());

		int NumEnemyTargetsInArea = AITAC_GetNumPlayersOfTeamInArea(EnemyTeam, Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), false, nullptr, AVH_USER3_NONE);

		AvHTeam* EnemyTeamRef = GetGameRules()->GetTeam(EnemyTeam);

		if (EnemyTeamRef)
		{
			AvHAIDeployableStructureType StructureSearchType = (EnemyTeamRef->GetTeamType() == AVH_CLASS_TYPE_MARINE) ? SEARCH_ALL_MARINE_STRUCTURES : SEARCH_ALL_ALIEN_STRUCTURES;

			DeployableSearchFilter SearchFilter;
			SearchFilter.DeployableTypes = StructureSearchType;
			SearchFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);
			SearchFilter.Team = EnemyTeam;

			NumEnemyTargetsInArea += AITAC_GetNumDeployablesNearLocation(Target->v.origin, &SearchFilter);
		}

		if (NumEnemyTargetsInArea > 2)
		{
			return WEAPON_SKULK_XENOCIDE;
		}
	}

	if (!IsPlayerParasited(Target))
	{
		float DistFromTarget = vDist2DSq(pBot->Edict->v.origin, Target->v.origin);

		if (DistFromTarget >= sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			Vector EnemyFacing = UTIL_GetForwardVector2D(Target->v.angles);
			Vector BotFacing = UTIL_GetVectorNormal2D(Target->v.origin - pBot->Edict->v.origin);

			float Dot = UTIL_GetDotProduct2D(EnemyFacing, BotFacing);

			// Only use parasite if the enemy is facing towards us. Means we don't ruin the element of surprise if sneaking up on an enemy
			if (Dot < 0.0f)
			{
				return WEAPON_SKULK_PARASITE;
			}
		}
	}

	return WEAPON_SKULK_BITE;

}

AvHAIWeapon LerkGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	if (!IsPlayerBuffed(pBot->Edict) && PlayerHasWeapon(pBot->Player, WEAPON_LERK_PRIMALSCREAM) && GetPlayerEnergy(pBot->Edict) > (GetEnergyCostForWeapon(WEAPON_LERK_PRIMALSCREAM) * 1.25f))
	{
		int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(pBot->Player->GetTeam(), Target->v.origin, UTIL_MetresToGoldSrcUnits(15.0f), pBot->Edict);

		if (NumAllies > 0)
		{
			return WEAPON_LERK_PRIMALSCREAM;
		}
	}

	float DistFromEnemy = vDist2DSq(pBot->Edict->v.origin, Target->v.origin);

	if (DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && PlayerHasWeapon(pBot->Player, WEAPON_LERK_UMBRA) && GetPlayerEnergy(pBot->Edict) > (GetEnergyCostForWeapon(WEAPON_LERK_UMBRA) * 1.25f))
	{
		int NumAllies = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), Target->v.origin, BALANCE_VAR(kUmbraCloudRadius), false, pBot->Edict, AVH_USER3_NONE);

		if (NumAllies > 0)
		{
			return WEAPON_LERK_UMBRA;
		}
	}

	if (DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && GetPlayerEnergy(pBot->Edict) > GetEnergyCostForWeapon(WEAPON_LERK_SPORES) && !IsAreaAffectedBySpores(Target->v.origin))
	{
		return WEAPON_LERK_SPORES;
	}

	return WEAPON_LERK_BITE;
}

AvHAIWeapon OnosGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || IsPlayerDead(Target))
	{
		return WEAPON_ONOS_GORE;
	}

	float DistFromTarget = vDist2DSq(pBot->Edict->v.origin, Target->v.origin);

	if (DistFromTarget > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		if (PlayerHasWeapon(pBot->Player, WEAPON_ONOS_CHARGE) && UTIL_PointIsDirectlyReachable(pBot->Edict->v.origin, Target->v.origin))
		{
			return WEAPON_ONOS_CHARGE;
		}

		return WEAPON_ONOS_GORE;
	}

	if (PlayerHasWeapon(pBot->Player, WEAPON_ONOS_STOMP) && !IsPlayerStunned(Target) && DistFromTarget > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)) && DistFromTarget < sqrf(UTIL_MetresToGoldSrcUnits(8.0f)))
	{
		return WEAPON_ONOS_STOMP;
	}

	if (!IsPlayerDigesting(pBot->Edict) && DistFromTarget < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
	{
		return WEAPON_ONOS_DEVOUR;
	}

	return WEAPON_ONOS_GORE;
}

AvHAIWeapon FadeGetBestWeaponForCombatTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || IsPlayerDead(Target))
	{
		return WEAPON_FADE_SWIPE;
	}

	if (!PlayerHasWeapon(pBot->Player, WEAPON_FADE_ACIDROCKET))
	{
		return WEAPON_FADE_SWIPE;
	}

	float DistFromTarget = vDist2DSq(pBot->Edict->v.origin, Target->v.origin);

	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());

	int NumEnemyAllies = AITAC_GetNumPlayersOfTeamInArea(EnemyTeam, Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), false, nullptr, AVH_USER3_NONE);

	if (NumEnemyAllies > 2)
	{
		return WEAPON_FADE_ACIDROCKET;
	}

	AvHPlayer* EnemyPlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(Target));

	if (EnemyPlayerRef && PlayerHasWeapon(EnemyPlayerRef, WEAPON_MARINE_SHOTGUN))
	{
		if (DistFromTarget > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			return WEAPON_FADE_ACIDROCKET;
		}
	}

	return WEAPON_FADE_SWIPE;

}

void BotReloadCurrentWeapon(AvHAIPlayer* pBot)
{
	AvHAIWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

	if (!WeaponCanBeReloaded(CurrentWeapon)) { return; }

	if (!IsPlayerReloading(pBot->Player))
	{
		if (gpGlobals->time - pBot->LastUseTime > 1.0f)
		{
			pBot->Button |= IN_RELOAD;
			pBot->LastUseTime = gpGlobals->time;
		}
	}
}

BotAttackResult PerformAttackLOSCheck(AvHAIPlayer* pBot, const AvHAIWeapon Weapon, const edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return ATTACK_INVALIDTARGET; }

	if (Weapon == WEAPON_NONE) { return ATTACK_NOWEAPON; }

	// Don't need aiming or special LOS checks for primal scream as it's AoE buff
	if (Weapon == WEAPON_LERK_PRIMALSCREAM)
	{
		return ATTACK_SUCCESS;
	}

	// Add a LITTLE bit of give to avoid edge cases where the bot is a smidge out of range
	float MaxWeaponRange = GetMaxIdealWeaponRange(Weapon) - 5.0f;

	// Don't need aiming or special LOS checks for Xenocide as it's an AOE attack, just make sure we're close enough and don't have a wall in the way
	if (Weapon == WEAPON_SKULK_XENOCIDE)
	{
		if (vDist3DSq(pBot->Edict->v.origin, Target->v.origin) <= sqrf(MaxWeaponRange) && UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, Target->v.origin))
		{
			return ATTACK_SUCCESS;
		}
		else
		{
			return ATTACK_OUTOFRANGE;
		}
	}

	// For charge and stomp, we can go through stuff so don't need to check for being blocked
	if (Weapon == WEAPON_ONOS_CHARGE || Weapon == WEAPON_ONOS_STOMP)
	{
		if (vDist3DSq(pBot->Edict->v.origin, Target->v.origin) > sqrf(MaxWeaponRange)) { return ATTACK_OUTOFRANGE; }

		if (!UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, Target->v.origin) || fabsf(Target->v.origin.z - Target->v.origin.z) > 50.0f) { return ATTACK_OUTOFRANGE; }

		return ATTACK_SUCCESS;
	}

	TraceResult hit;

	Vector StartTrace = pBot->CurrentEyePosition;

	Vector AttackDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - StartTrace);

	Vector EndTrace = pBot->CurrentEyePosition + (AttackDir * MaxWeaponRange);

	UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

	if (FNullEnt(hit.pHit)) { return ATTACK_OUTOFRANGE; }

	if (hit.pHit != Target)
	{
		if (vDist3DSq(pBot->CurrentEyePosition, Target->v.origin) > sqrf(MaxWeaponRange))
		{
			return ATTACK_OUTOFRANGE;
		}
		else
		{
			return ATTACK_BLOCKED;
		}
	}

	return ATTACK_SUCCESS;
}

bool IsAreaAffectedBySpores(const Vector Location)
{
	bool Result = false;

	FOR_ALL_ENTITIES(kwsSporeProjectile, AvHSporeProjectile*)
		
		if (vDist2DSq(theEntity->pev->origin, Location) <= BALANCE_VAR(kSporeCloudRadius))
		{
			Result = true;
			break;
		}

	END_FOR_ALL_ENTITIES(kwsSporeProjectile)

	return Result;
}

float UTIL_GetProjectileVelocityForWeapon(const AvHAIWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_GORGE_SPIT:
		return (float)kSpitVelocity;
	case WEAPON_LERK_SPORES:
		return (float)kShootCloudVelocity;
	case WEAPON_FADE_ACIDROCKET:
		return (float)kAcidRocketVelocity;
	case WEAPON_GORGE_BILEBOMB:
		return (float)kBileBombVelocity;
	case WEAPON_LERK_SPIKE:
		return (float)kSpikeVelocity;
	case WEAPON_MARINE_GRENADE:
	case WEAPON_MARINE_GL:
		return (float)BALANCE_VAR(kGrenadeForce);
	default:
		return 0.0f; // Hitscan.
	}
}

char* UTIL_WeaponTypeToClassname(const AvHAIWeapon WeaponType)
{
	switch (WeaponType)
	{
	case WEAPON_MARINE_MG:
		return kwsMachineGun;
	case WEAPON_MARINE_PISTOL:
		return kwsPistol;
	case WEAPON_MARINE_KNIFE:
		return kwsKnife;
	case WEAPON_MARINE_SHOTGUN:
		return kwsShotGun;
	case WEAPON_MARINE_HMG:
		return kwsHeavyMachineGun;
	case WEAPON_MARINE_WELDER:
		return kwsWelder;
	case WEAPON_MARINE_MINES:
		return kwsMine;
	case WEAPON_MARINE_GRENADE:
		return kwsGrenade;
	case WEAPON_MARINE_GL:
		return kwsGrenadeGun;

	case WEAPON_SKULK_BITE:
		return kwsBiteGun;
	case WEAPON_SKULK_PARASITE:
		return kwsParasiteGun;
	case WEAPON_SKULK_LEAP:
		return kwsLeap;
	case WEAPON_SKULK_XENOCIDE:
		return kwsDivineWind;

	case WEAPON_GORGE_SPIT:
		return kwsSpitGun;
	case WEAPON_GORGE_HEALINGSPRAY:
		return kwsHealingSpray;
	case WEAPON_GORGE_BILEBOMB:
		return kwsBileBombGun;
	case WEAPON_GORGE_WEB:
		return kwsWebSpinner;

	case WEAPON_LERK_BITE:
		return kwsBite2Gun;
	case WEAPON_LERK_SPORES:
		return kwsSporeGun;
	case WEAPON_LERK_UMBRA:
		return kwsUmbraGun;
	case WEAPON_LERK_PRIMALSCREAM:
		return kwsPrimalScream;

	case WEAPON_FADE_SWIPE:
		return kwsSwipe;
	case WEAPON_FADE_BLINK:
		return kwsBlinkGun;
	case WEAPON_FADE_METABOLIZE:
		return kwsMetabolize;
	case WEAPON_FADE_ACIDROCKET:
		return kwsAcidRocketGun;

	case WEAPON_ONOS_GORE:
		return kwsClaws;
	case WEAPON_ONOS_DEVOUR:
		return kwsDevour;
	case WEAPON_ONOS_STOMP:
		return kwsStomp;
	case WEAPON_ONOS_CHARGE:
		return kwsCharge;
	default:
		return "";
	}

	return "";
}