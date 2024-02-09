
#include "AvHAIPlayer.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIHelper.h"
#include "AvHAIMath.h"
#include "AvHAIHelper.h"
#include "AvHAINavigation.h"
#include "AvHAIWeaponHelper.h"
#include "AvHAITactical.h"
#include "AvHAITask.h"
#include "AvHAICommander.h"
#include "AvHAIPlayerManager.h"
#include "AvHAIConfig.h"

#include "../AvHGamerules.h"
#include "../AvHMessage.h"

extern nav_mesh NavMeshes[MAX_NAV_MESHES]; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)
extern nav_profile BaseNavProfiles[MAX_NAV_PROFILES]; // Array of nav profiles

void BotJump(AvHAIPlayer* pBot)
{
	if (pBot->BotNavInfo.IsOnGround)
	{
		if (gpGlobals->time - pBot->BotNavInfo.LandedTime >= 0.5f)
		{
			pBot->Button |= IN_JUMP;
			pBot->BotNavInfo.bIsJumping = true;
			pBot->BotNavInfo.bHasAttemptedJump = true;
		}
	}
	else
	{
		if (pBot->BotNavInfo.bIsJumping)
		{
			// Skulks, gorges and lerks can't duck jump...
			if (!IsPlayerSkulk(pBot->Edict) && !IsPlayerGorge(pBot->Edict) && !IsPlayerLerk(pBot->Edict))
			{
				pBot->Button |= IN_DUCK;
			}
		}
	}
}

void BotSuicide(AvHAIPlayer* pBot)
{
	if (pBot && !IsPlayerDead(pBot->Edict) && !pBot->bIsPendingKill)
	{
		pBot->bIsPendingKill = true;
		pBot->Player->Suicide();
	}
}

/* Makes the bot look at the specified position */
void BotLookAt(AvHAIPlayer* pBot, const Vector target)
{

	pBot->LookTargetLocation.x = target.x;
	pBot->LookTargetLocation.y = target.y;
	pBot->LookTargetLocation.z = target.z;

}

void BotMoveLookAt(AvHAIPlayer* pBot, const Vector target)
{
	pBot->MoveLookLocation.x = target.x;
	pBot->MoveLookLocation.y = target.y;
	pBot->MoveLookLocation.z = target.z;
}

void BotDirectLookAt(AvHAIPlayer* pBot, Vector target)
{
	pBot->DesiredLookDirection = ZERO_VECTOR;
	pBot->InterpolatedLookDirection = ZERO_VECTOR;

	edict_t* pEdict = pBot->Edict;

	Vector viewPos = pBot->CurrentEyePosition;

	Vector dir = (target - viewPos);

	pEdict->v.v_angle = UTIL_VecToAngles(dir);

	if (pEdict->v.v_angle.y > 180)
		pEdict->v.v_angle.y -= 360;

	// Paulo-La-Frite - START bot aiming bug fix
	if (pEdict->v.v_angle.x > 180)
		pEdict->v.v_angle.x -= 360;

	// set the body angles to point the gun correctly
	pEdict->v.angles.x = pEdict->v.v_angle.x / 3;
	pEdict->v.angles.y = pEdict->v.v_angle.y;
	pEdict->v.angles.z = 0;

	// adjust the view angle pitch to aim correctly (MUST be after body v.angles stuff)
	pEdict->v.v_angle.x = -pEdict->v.v_angle.x;
	// Paulo-La-Frite - END

	pEdict->v.ideal_yaw = pEdict->v.v_angle.y;

	if (pEdict->v.ideal_yaw > 180)
		pEdict->v.ideal_yaw -= 360;

	if (pEdict->v.ideal_yaw < -180)
		pEdict->v.ideal_yaw += 360;
}

enemy_status* GetTrackedEnemyRefForTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	for (int i = 0; i < 32; i++)
	{
		if (pBot->TrackedEnemies[i].EnemyEdict == Target)
		{
			return &pBot->TrackedEnemies[i];
		}
	}

	return nullptr;
}

void BotLookAt(AvHAIPlayer* pBot, edict_t* target)
{
	if (FNullEnt(target)) { return; }

	pBot->LookTarget = target;

	// For team mates we don't track enemy refs, so just look at the friendly player
	if (!IsEdictPlayer(target) || target->v.team == pBot->Edict->v.team)
	{
		pBot->LookTargetLocation = UTIL_GetCentreOfEntity(pBot->LookTarget);
		pBot->LastTargetTrackUpdate = gpGlobals->time;
		return;
	}

	enemy_status* TrackedEnemyRef = GetTrackedEnemyRefForTarget(pBot, target);

	Vector TargetVelocity = (TrackedEnemyRef) ? TrackedEnemyRef->LastSeenVelocity : pBot->LookTarget->v.velocity;
	Vector TargetLocation = (TrackedEnemyRef) ? TrackedEnemyRef->LastSeenLocation : UTIL_GetCentreOfEntity(pBot->LookTarget);

	AvHAIWeapon CurrentWeapon = GetPlayerCurrentWeapon(pBot->Player);

	Vector NewLoc = UTIL_GetAimLocationToLeadTarget(pBot->CurrentEyePosition, TargetLocation, TargetVelocity, GetProjectileVelocityForWeapon(CurrentWeapon));

	float Offset = frandrange(30.0f, 50.0f);

	float motion_tracking_skill = (IsPlayerMarine(pBot->Edict)) ? pBot->BotSkillSettings.marine_bot_motion_tracking_skill : pBot->BotSkillSettings.alien_bot_motion_tracking_skill;

	Offset -= Offset * motion_tracking_skill;

	if (randbool())
	{
		Offset *= -1.0f;
	}

	float NewDist = vDist3D(TargetLocation, NewLoc) + Offset;


	float MoveSpeed = vSize3D(target->v.velocity);

	Vector MoveVector = (MoveSpeed > 5.0f) ? UTIL_GetVectorNormal(target->v.velocity) : ZERO_VECTOR;

	Vector NewAimLoc = TargetLocation + (MoveVector * NewDist);

	pBot->LookTargetLocation = NewAimLoc;
	pBot->LastTargetTrackUpdate = gpGlobals->time;
}

bool BotUseObject(AvHAIPlayer* pBot, edict_t* Target, bool bContinuous)
{
	if (FNullEnt(Target)) { return false; }

	Vector ClosestPoint = UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, Target);
	Vector TargetCentre = UTIL_GetCentreOfEntity(Target);

	Vector AimPoint = ClosestPoint;

	if (IsEdictStructure(Target))
	{
		AimPoint = TargetCentre;
		AimPoint.z = ClosestPoint.z;
	}

	BotLookAt(pBot, AimPoint);

	if (!bContinuous && ((gpGlobals->time - pBot->LastUseTime) < min_player_use_interval)) { return false; }

	Vector AimDir = UTIL_GetForwardVector2D(pBot->Edict->v.v_angle);
	Vector TargetAimDir = (IsEdictStructure(Target) ? UTIL_GetVectorNormal2D(TargetCentre - pBot->CurrentEyePosition) : UTIL_GetVectorNormal2D(ClosestPoint - pBot->CurrentEyePosition));

	float AimDot = UTIL_GetDotProduct2D(AimDir, TargetAimDir);

	if (AimDot >= 0.95f)
	{
		pBot->Button |= IN_USE;
		pBot->LastUseTime = gpGlobals->time;
		return true;
	}

	return false;
}

AvHAIWeapon GetPlayerCurrentWeapon(const AvHPlayer* Player)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(Player->m_pActiveItem);

	if (theBasePlayerWeapon)
	{
		return (AvHAIWeapon)theBasePlayerWeapon->m_iId;
	}

	return WEAPON_INVALID;
}

AvHBasePlayerWeapon* GetPlayerCurrentWeaponReference(const AvHPlayer* Player)
{
	return dynamic_cast<AvHBasePlayerWeapon*>(Player->m_pActiveItem);
}

bool CanBotLeap(AvHAIPlayer* pBot)
{
	return (PlayerHasWeapon(pBot->Player, WEAPON_SKULK_LEAP) && GetPlayerEnergy(pBot->Edict) >= (float)BALANCE_VAR(kLeapEnergyCost)) || (PlayerHasWeapon(pBot->Player, WEAPON_FADE_BLINK));
}

float GetLeapCost(AvHAIPlayer* pBot)
{
	if (FNullEnt(pBot->Edict)) { return WEAPON_INVALID; }

	AvHUser3 PlayerClass = (AvHUser3)pBot->Edict->v.iuser3;

	switch (PlayerClass)
	{
	case AVH_USER3_ALIEN_PLAYER1:
		return ((PlayerHasWeapon(pBot->Player, WEAPON_SKULK_LEAP)) ? (float)BALANCE_VAR(kLeapEnergyCost) : 0.0f);
	case AVH_USER3_ALIEN_PLAYER4:
		return ((PlayerHasWeapon(pBot->Player, WEAPON_FADE_BLINK)) ? (float)BALANCE_VAR(kBlinkEnergyCost) : 0.0f);
	case AVH_USER3_ALIEN_PLAYER5:
		return ((PlayerHasWeapon(pBot->Player, WEAPON_ONOS_CHARGE)) ? (float)BALANCE_VAR(kChargeEnergyCost) : 0.0f);
	default:
		return 0.0f;
	}
}

void BotLeap(AvHAIPlayer* pBot, const Vector TargetLocation)
{

	if (!CanBotLeap(pBot))
	{
		BotJump(pBot);
		return;
	}

	AvHAIWeapon LeapWeapon = (IsPlayerSkulk(pBot->Edict)) ? WEAPON_SKULK_LEAP : WEAPON_FADE_BLINK;

	if (GetPlayerCurrentWeapon(pBot->Player) != LeapWeapon)
	{
		pBot->DesiredMoveWeapon = LeapWeapon;
		return;
	}

	bool bShouldLeap = !IsPlayerSkulk(pBot->Edict) || (pBot->BotNavInfo.IsOnGround && (gpGlobals->time - pBot->BotNavInfo.LandedTime >= 0.2f && gpGlobals->time - pBot->BotNavInfo.LeapAttemptedTime >= 0.5f));

	if (!bShouldLeap) { return; }

	Vector LookLocation = TargetLocation;

	unsigned char NavArea = UTIL_GetNavAreaAtLocation(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin);

	if (NavArea == SAMPLE_POLYAREA_CROUCH)
	{
		Vector MoveDir = UTIL_GetVectorNormal2D(TargetLocation - pBot->Edict->v.origin);
		LookLocation = (pBot->CurrentEyePosition + (MoveDir * 50.0f) + Vector(0.0f, 0.0f, 10.0f));
	}
	else
	{
		LookLocation = LookLocation + Vector(0.0f, 0.0f, 200.0f);

		if (LeapWeapon == WEAPON_FADE_BLINK)
		{
			float PlayerCurrentSpeed = vSize3D(pBot->Edict->v.velocity);
			float LaunchVelocity = PlayerCurrentSpeed + 255.0f;

			Vector LaunchAngle = GetPitchForProjectile(pBot->CurrentEyePosition, TargetLocation, LaunchVelocity, GOLDSRC_GRAVITY);

			if (LaunchAngle != ZERO_VECTOR)
			{
				LaunchAngle = UTIL_GetVectorNormal(LaunchAngle);
				LookLocation = pBot->CurrentEyePosition + (LaunchAngle * 200.0f);
			}
		}

		if (LeapWeapon == WEAPON_SKULK_LEAP)
		{
			float PlayerCurrentSpeed = vSize3D(pBot->Edict->v.velocity);
			float LaunchVelocity = PlayerCurrentSpeed + 500.0f;

			Vector LaunchAngle = GetPitchForProjectile(pBot->CurrentEyePosition, TargetLocation, LaunchVelocity, GOLDSRC_GRAVITY);

			if (LaunchAngle != ZERO_VECTOR)
			{
				LaunchAngle = UTIL_GetVectorNormal(LaunchAngle);
				LookLocation = pBot->CurrentEyePosition + (LaunchAngle * 200.0f);
			}
		}
	}

	BotMoveLookAt(pBot, LookLocation);

	if (IsPlayerFade(pBot->Edict) && !pBot->BotNavInfo.IsOnGround)
	{
		float RequiredVelocity = UTIL_GetVelocityRequiredToReachTarget(pBot->Edict->v.origin, TargetLocation, GOLDSRC_GRAVITY);
		float CurrentVelocity = vSize3D(pBot->Edict->v.velocity);

		bShouldLeap = (CurrentVelocity <= RequiredVelocity);
	}

	if (bShouldLeap)
	{

		Vector FaceAngle = UTIL_GetForwardVector2D(pBot->Edict->v.v_angle);
		Vector MoveDir = UTIL_GetVectorNormal2D(TargetLocation - pBot->Edict->v.origin);

		float Dot = UTIL_GetDotProduct2D(FaceAngle, MoveDir);

		if (Dot >= 0.98f)
		{
			pBot->Button |= IN_ATTACK2;
			pBot->BotNavInfo.bIsJumping = true;
			pBot->BotNavInfo.LeapAttemptedTime = gpGlobals->time;
		}
	}
	else
	{
		if (pBot->BotNavInfo.bIsJumping)
		{
			// Skulks, gorges and lerks can't duck jump...
			if (!IsPlayerSkulk(pBot->Edict) && !IsPlayerGorge(pBot->Edict) && !IsPlayerLerk(pBot->Edict))
			{
				pBot->Button |= IN_DUCK;
			}
		}
	}
}

bot_msg* GetAvailableBotMsgSlot(AvHAIPlayer* pBot)
{
	for (int i = 0; i < 5; i++)
	{
		if (!pBot->ChatMessages[i].bIsPending) { return &pBot->ChatMessages[i]; }
	}

	return nullptr;
}

void BotSay(AvHAIPlayer* pBot, bool bTeamSay, float Delay, char* textToSay)
{
	bot_msg* msgSlot = GetAvailableBotMsgSlot(pBot);

	if (msgSlot)
	{
		msgSlot->bIsPending = true;
		msgSlot->bIsTeamSay = bTeamSay;
		msgSlot->SendTime = gpGlobals->time + Delay;
		sprintf(msgSlot->msg, textToSay);
	}
}

bool BotReloadWeapons(AvHAIPlayer* pBot)
{
	// Aliens and commander don't reload
	if (!IsPlayerMarine(pBot->Edict) || !IsPlayerActiveInGame(pBot->Edict)) { return false; }

	AvHAIWeapon PrimaryWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);
	AvHAIWeapon SecondaryWeapon = GetBotMarineSecondaryWeapon(pBot);
	AvHAIWeapon CurrentWeapon = GetPlayerCurrentWeapon(pBot->Player);

	if (WeaponCanBeReloaded(PrimaryWeapon) && UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) < UTIL_GetPlayerPrimaryWeaponMaxClipSize(pBot->Player) && UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) > 0)
	{
		pBot->DesiredCombatWeapon = PrimaryWeapon;

		if (CurrentWeapon == PrimaryWeapon)
		{
			BotReloadCurrentWeapon(pBot);
		}

		return true;
	}

	if (WeaponCanBeReloaded(SecondaryWeapon) && BotGetSecondaryWeaponClipAmmo(pBot) < BotGetSecondaryWeaponMaxClipSize(pBot) && BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
	{
		pBot->DesiredCombatWeapon = SecondaryWeapon;

		if (CurrentWeapon == SecondaryWeapon)
		{
			BotReloadCurrentWeapon(pBot);
		}
		return true;
	}

	return false;
}

void BotDropWeapon(AvHAIPlayer* pBot)
{
	// Look straight ahead so we don't accidentally drop the weapon right at our feet and pick it up again instantly

	Vector AimDir = UTIL_GetForwardVector(pBot->Edict->v.v_angle);
	Vector TargetAimDir = Vector(AimDir.x, AimDir.y, 0.0f);

	Vector LookLoc = pBot->CurrentEyePosition + (TargetAimDir * 100.0f);

	BotLookAt(pBot, LookLoc);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	if (AimDot >= 0.95f)
	{
		pBot->Impulse = WEAPON_DROP;
	}
}

void BotAlienAttackNonPlayerTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	AvHAIWeapon Weapon = BotAlienChooseBestWeaponForStructure(pBot, Target);

	BotAttackResult AttackResult = PerformAttackLOSCheck(pBot, Weapon, Target);

	float WeaponRange = GetMaxIdealWeaponRange(Weapon);

	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(Target);

	if (AttackResult == ATTACK_OUTOFRANGE)
	{
		if (vDist2DSq(pBot->Edict->v.origin, Target->v.origin) < sqrf(max_player_use_reach))
		{
			pBot->Button |= IN_DUCK;
		}

		Vector AttackPoint = (IsEdictStructure(Target)) ? Target->v.origin : UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, Target);

		if (StructureType == STRUCTURE_ALIEN_HIVE)
		{
			const AvHAIHiveDefinition* HiveDefinition = AITAC_GetHiveFromEdict(Target);

			if (HiveDefinition)
			{
				AttackPoint = HiveDefinition->FloorLocation;
			}
		}

		if (IsPlayerLerk(pBot->Edict))
		{
			if (AITAC_ShouldBotBeCautious(pBot))
			{
				MoveTo(pBot, AttackPoint, MOVESTYLE_HIDE, 100.0f);
			}
			else
			{
				MoveTo(pBot, AttackPoint, MOVESTYLE_NORMAL, 100.0f);
			}

			return;
		}

		MoveTo(pBot, AttackPoint, MOVESTYLE_NORMAL, WeaponRange);

		return;
	}

	if (AttackResult == ATTACK_BLOCKED)
	{

		// We're attacking a shootable trigger
		if (!IsEdictStructure(Target))
		{
			Vector AttackPoint = UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, Target);
			MoveTo(pBot, AttackPoint, MOVESTYLE_NORMAL, WeaponRange);
			return;
		}

		// If we have regen and are hurt and are attacking a damaging structure, let us heal up a bit
		if ((StructureType == STRUCTURE_MARINE_TURRET || StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER) && GetPlayerOverallHealthPercent(pBot->Edict) < 0.75f && AvHGetAlienUpgradeLevel(pBot->Edict->v.iuser4, MASK_UPGRADE_2) > 0)
		{
			return;
		}

		if (vIsZero(pBot->BotNavInfo.ActualMoveDestination) || UTIL_TraceEntity(pBot->Edict, pBot->BotNavInfo.ActualMoveDestination + Vector(0.0f, 0.0f, 32.0f), UTIL_GetCentreOfEntity(Target)) != Target)
		{
			Vector NewAttackLocation = ZERO_VECTOR;

			if (vIsZero(pBot->BotNavInfo.ActualMoveDestination))
			{
				NewAttackLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_GetEntityGroundLocation(Target), WeaponRange);
			}
			else
			{
				NewAttackLocation = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, 2.0f);

				// Did we find a clear spot we could attack from? If so, make that our new move destination
				if (NewAttackLocation != ZERO_VECTOR && UTIL_TraceEntity(pBot->Edict, NewAttackLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_GetCentreOfEntity(Target)) == Target)
				{
					MoveTo(pBot, NewAttackLocation, MOVESTYLE_NORMAL);
				}
			}
		}
		else
		{
			MoveTo(pBot, pBot->BotNavInfo.TargetDestination, MOVESTYLE_NORMAL);
		}

		return;
	}

	if (AttackResult == ATTACK_SUCCESS)
	{
		// If we were ducking before then keep ducking
		if (pBot->Edict->v.oldbuttons & IN_DUCK)
		{
			pBot->Button |= IN_DUCK;
		}

		BotShootTarget(pBot, Weapon, Target);
	}
}

void BotMarineAttackNonPlayerTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	AvHAIWeapon Weapon = BotMarineChooseBestWeaponForStructure(pBot, Target);

	// Add special logic for grenade launchers since they aren't used like regular marine hitscan weapons
	// This will handle things like firing from around corners, making sure they have cover from allies etc.
	if (Weapon == WEAPON_MARINE_GL)
	{
		BombardierAttackTarget(pBot, Target);
		return;
	}

	BotAttackResult AttackResult = PerformAttackLOSCheck(pBot, Weapon, Target);

	float WeaponRange = GetMaxIdealWeaponRange(Weapon);

	AvHAIDeployableStructureType StructureType = GetStructureTypeFromEdict(Target);

	if (AttackResult == ATTACK_OUTOFRANGE)
	{
		if (vDist2DSq(pBot->Edict->v.origin, Target->v.origin) < sqrf(max_player_use_reach))
		{
			pBot->Button |= IN_DUCK;
		}

		Vector AttackPoint = (IsEdictStructure(Target)) ? Target->v.origin : UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, Target);

		if (StructureType == STRUCTURE_ALIEN_HIVE)
		{
			const AvHAIHiveDefinition* HiveDefinition = AITAC_GetHiveFromEdict(Target);

			if (HiveDefinition)
			{
				AttackPoint = HiveDefinition->FloorLocation;
			}
		}

		MoveTo(pBot, AttackPoint, MOVESTYLE_NORMAL, WeaponRange);

		return;
	}

	if (AttackResult == ATTACK_BLOCKED)
	{
		// Finish reloading, we are probably behind cover
		if (IsPlayerReloading(pBot->Player))
		{
			return;
		}

		// We're attacking a shootable trigger
		if (!IsEdictStructure(Target))
		{
			Vector AttackPoint = UTIL_GetButtonFloorLocation(pBot->Edict->v.origin, Target);
			MoveTo(pBot, AttackPoint, MOVESTYLE_NORMAL, WeaponRange);
			return;
		}

		if (vIsZero(pBot->BotNavInfo.ActualMoveDestination) || UTIL_TraceEntity(pBot->Edict, pBot->BotNavInfo.ActualMoveDestination + Vector(0.0f, 0.0f, 32.0f), UTIL_GetCentreOfEntity(Target)) != Target)
		{
			Vector NewAttackLocation = ZERO_VECTOR;

			if (vIsZero(pBot->BotNavInfo.ActualMoveDestination))
			{
				NewAttackLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_GetEntityGroundLocation(Target), WeaponRange);
			}
			else
			{
				NewAttackLocation = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, 2.0f);

				// Did we find a clear spot we could attack from? If so, make that our new move destination
				if (NewAttackLocation != ZERO_VECTOR && UTIL_TraceEntity(pBot->Edict, NewAttackLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_GetCentreOfEntity(Target)) == Target)
				{
					MoveTo(pBot, NewAttackLocation, MOVESTYLE_NORMAL);
				}
			}
		}
		else
		{
			MoveTo(pBot, pBot->BotNavInfo.TargetDestination, MOVESTYLE_NORMAL);
		}

		return;
	}

	if (AttackResult == ATTACK_SUCCESS)
	{
		if (IsPlayerReloading(pBot->Player))
		{
			if (StructureType == STRUCTURE_MARINE_TURRET || StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
			{
				MoveTo(pBot, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam()), MOVESTYLE_NORMAL);
				return;
			}
		}

		// If we were ducking before then keep ducking
		if (pBot->Edict->v.oldbuttons & IN_DUCK)
		{
			pBot->Button |= IN_DUCK;
		}

		BotShootTarget(pBot, Weapon, Target);
	}

}

void BotAttackNonPlayerTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	AvHAIWeapon Weapon = WEAPON_INVALID;

	if (IsPlayerMarine(pBot->Edict))
	{
		BotMarineAttackNonPlayerTarget(pBot, Target);
	}
	else
	{
		BotAlienAttackNonPlayerTarget(pBot, Target);
	}

}

void BotShootTarget(AvHAIPlayer* pBot, AvHAIWeapon AttackWeapon, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	AvHAIWeapon CurrentWeapon = GetPlayerCurrentWeapon(pBot->Player);

	pBot->DesiredCombatWeapon = AttackWeapon;

	if (CurrentWeapon != AttackWeapon)
	{
		return;
	}

	if (CurrentWeapon == WEAPON_INVALID) { return; }


	if (CurrentWeapon == WEAPON_SKULK_XENOCIDE || CurrentWeapon == WEAPON_LERK_PRIMALSCREAM)
	{
		pBot->Button |= IN_ATTACK;

		return;
	}

	if (AttackWeapon == WEAPON_LERK_SPORES || AttackWeapon == WEAPON_LERK_UMBRA)
	{
		BotLookAt(pBot, Target);

		Vector AimDir = UTIL_GetForwardVector(pBot->Edict->v.v_angle);

		TraceResult Hit;
		Vector TraceEnd = pBot->CurrentEyePosition + (AimDir * 3000.0f);

		UTIL_TraceLine(pBot->CurrentEyePosition, TraceEnd, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &Hit);

		if (Hit.flFraction >= 1.0f) { return; }

		if (vDist3DSq(Hit.vecEndPos, Target->v.origin) <= sqrf(kSporeCloudRadius))
		{
			pBot->Button |= IN_ATTACK;
		}

		return;
	}

	// For charge and stomp, we can go through stuff so don't need to check for being blocked
	if (CurrentWeapon == WEAPON_ONOS_CHARGE || CurrentWeapon == WEAPON_ONOS_STOMP)
	{
		BotLookAt(pBot, Target);

		Vector DirToTarget = UTIL_GetVectorNormal2D(Target->v.origin - pBot->Edict->v.origin);
		float DotProduct = UTIL_GetDotProduct2D(UTIL_GetForwardVector(pBot->Edict->v.v_angle), DirToTarget);

		float MinDotProduct = (CurrentWeapon == WEAPON_ONOS_STOMP) ? 0.95f : 0.75f;

		if (DotProduct >= MinDotProduct)
		{

			if (CurrentWeapon == WEAPON_ONOS_CHARGE)
			{
				pBot->Button |= IN_ATTACK2;
			}
			else
			{
				pBot->Button |= IN_ATTACK;
			}
		}

		return;
	}

	if (IsMeleeWeapon(CurrentWeapon))
	{
		BotLookAt(pBot, Target);
		pBot->Button |= IN_ATTACK;
		return;
	}

	Vector TargetAimDir = ZERO_VECTOR;

	if (CurrentWeapon == WEAPON_MARINE_GL || CurrentWeapon == WEAPON_MARINE_GRENADE || CurrentWeapon == WEAPON_GORGE_BILEBOMB)
	{
		Vector AimLocation = UTIL_GetCentreOfEntity(Target);

		float ProjectileVelocity = UTIL_GetProjectileVelocityForWeapon(CurrentWeapon);

		Vector NewAimAngle = GetPitchForProjectile(pBot->CurrentEyePosition, AimLocation, ProjectileVelocity, GOLDSRC_GRAVITY);

		AimLocation = pBot->CurrentEyePosition + (NewAimAngle * 200.0f);

		BotLookAt(pBot, AimLocation);
		TargetAimDir = UTIL_GetVectorNormal(AimLocation - pBot->CurrentEyePosition);
	}
	else
	{
		BotLookAt(pBot, Target);
		TargetAimDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - pBot->CurrentEyePosition);
	}

	if (WeaponCanBeReloaded(CurrentWeapon))
	{
		bool bShouldReload = (GetPlayerCurrentWeaponReserveAmmo(pBot->Player) > 0);

		if (CurrentWeapon == WEAPON_MARINE_SHOTGUN && IsEdictStructure(Target))
		{
			bShouldReload = bShouldReload && ((float)GetPlayerCurrentWeaponClipAmmo(pBot->Player) / (float)GetPlayerCurrentWeaponMaxClipAmmo(pBot->Player) < 0.5f);
		}
		else
		{
			bShouldReload = bShouldReload && GetPlayerCurrentWeaponClipAmmo(pBot->Player) == 0;
		}

		if (bShouldReload)
		{
			BotReloadCurrentWeapon(pBot);
			return;
		}

	}

	if (IsPlayerReloading(pBot->Player))
	{
		return;
	}

	Vector AimDir = UTIL_GetForwardVector(pBot->Edict->v.v_angle);

	bool bWillHit = false;

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	// We can be less accurate with spores and umbra since they have AoE effects
	float MinAcceptableAccuracy = 0.9f;

	bWillHit = (AimDot >= MinAcceptableAccuracy);

	if (!bWillHit && IsHitscanWeapon(CurrentWeapon))
	{

		edict_t* HitEntity = UTIL_TraceEntity(pBot->Edict, pBot->CurrentEyePosition, pBot->CurrentEyePosition + (AimDir * GetMaxIdealWeaponRange(CurrentWeapon)));

		bWillHit = (HitEntity == Target);
	}

	if (bWillHit)
	{
		AvHBasePlayerWeapon* WeaponRef = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_pActiveItem);

		if (!WeaponRef->GetMustPressTriggerForEachShot() || WeaponRef->m_flNextPrimaryAttack <= 0.0f)
		{
			pBot->Button |= IN_ATTACK;
		}
	}
}

void BombardierAttackTarget(AvHAIPlayer* pBot, edict_t* Target)
{

	if (!IsPlayerReloading(pBot->Player))
	{
		if (vDist3DSq(pBot->Edict->v.origin, Target->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			Vector BackDir = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - Target->v.origin);
			pBot->desiredMovementDir = BackDir;
		}

		Vector GrenadeLoc = UTIL_GetGrenadeThrowTarget(pBot->Edict, Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), true);

		if (GrenadeLoc != ZERO_VECTOR)
		{
			BotShootLocation(pBot, WEAPON_MARINE_GL, GrenadeLoc);
		}
		else
		{
			Vector AttackPoint = Target->v.origin;

			if (GetStructureTypeFromEdict(Target) == STRUCTURE_ALIEN_HIVE)
			{
				const AvHAIHiveDefinition* HiveDefinition = AITAC_GetHiveFromEdict(Target);

				if (HiveDefinition)
				{
					AttackPoint = HiveDefinition->FloorLocation;
				}
			}

			MoveTo(pBot, AttackPoint, MOVESTYLE_NORMAL);
		}

		return;
	}

	// Back off to reload
	if (GetStructureTypeFromEdict(Target) == STRUCTURE_ALIEN_OFFENCECHAMBER && UTIL_QuickTrace(pBot->Edict, pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(Target)))
	{
		BotLookAt(pBot, Target);
		MoveTo(pBot, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam()), MOVESTYLE_NORMAL);
		return;
	}
}

void BotShootLocation(AvHAIPlayer* pBot, AvHAIWeapon AttackWeapon, const Vector TargetLocation)
{
	if (vIsZero(TargetLocation)) { return; }

	AvHAIWeapon CurrentWeapon = GetPlayerCurrentWeapon(pBot->Player);

	pBot->DesiredCombatWeapon = AttackWeapon;

	if (CurrentWeapon != AttackWeapon)
	{
		return;
	}

	if (CurrentWeapon == WEAPON_NONE) { return; }

	if (CurrentWeapon == WEAPON_SKULK_XENOCIDE)
	{
		pBot->Button |= IN_ATTACK;

		return;
	}

	if (AttackWeapon == WEAPON_LERK_SPORES || AttackWeapon == WEAPON_LERK_UMBRA)
	{
		BotLookAt(pBot, TargetLocation);

		Vector AimDir = UTIL_GetForwardVector(pBot->Edict->v.v_angle);

		TraceResult Hit;
		Vector TraceEnd = pBot->CurrentEyePosition + (AimDir * 3000.0f);

		UTIL_TraceLine(pBot->CurrentEyePosition, TraceEnd, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &Hit);

		if (Hit.flFraction >= 1.0f) { return; }

		if (vDist3DSq(Hit.vecEndPos, TargetLocation) <= sqrf(kSporeCloudRadius))
		{
			pBot->Button |= IN_ATTACK;
		}

		return;
	}

	// For charge and stomp, we can go through stuff so don't need to check for being blocked
	if (CurrentWeapon == WEAPON_ONOS_CHARGE || CurrentWeapon == WEAPON_ONOS_STOMP)
	{
		BotLookAt(pBot, TargetLocation);

		Vector DirToTarget = UTIL_GetVectorNormal2D(TargetLocation - pBot->Edict->v.origin);
		float DotProduct = UTIL_GetDotProduct2D(UTIL_GetForwardVector(pBot->Edict->v.v_angle), DirToTarget);

		float MinDotProduct = (CurrentWeapon == WEAPON_ONOS_STOMP) ? 0.95f : 0.75f;

		if (DotProduct >= MinDotProduct)
		{
			if (CurrentWeapon == WEAPON_ONOS_CHARGE)
			{
				pBot->Button |= IN_ATTACK2;
			}
			else
			{
				pBot->Button |= IN_ATTACK;
			}
		}

		return;
	}

	if (IsMeleeWeapon(CurrentWeapon))
	{
		BotLookAt(pBot, TargetLocation);
		pBot->Button |= IN_ATTACK;
		return;
	}

	Vector TargetAimDir = ZERO_VECTOR;

	if (CurrentWeapon == WEAPON_MARINE_GL || CurrentWeapon == WEAPON_MARINE_GRENADE)
	{
		Vector AimLocation = TargetLocation;
		Vector NewAimAngle = GetPitchForProjectile(pBot->CurrentEyePosition, AimLocation, UTIL_GetProjectileVelocityForWeapon(CurrentWeapon), GOLDSRC_GRAVITY);

		AimLocation = pBot->CurrentEyePosition + (NewAimAngle * 200.0f);

		BotLookAt(pBot, AimLocation);
		TargetAimDir = UTIL_GetVectorNormal(AimLocation - pBot->CurrentEyePosition);
	}
	else
	{
		BotLookAt(pBot, TargetLocation);
		TargetAimDir = UTIL_GetVectorNormal(TargetLocation - pBot->CurrentEyePosition);
	}

	if (WeaponCanBeReloaded(CurrentWeapon))
	{
		bool bShouldReload = (GetPlayerCurrentWeaponReserveAmmo(pBot->Player) > 0);

		if (CurrentWeapon == WEAPON_MARINE_SHOTGUN)
		{
			bShouldReload = bShouldReload && ((float)GetPlayerCurrentWeaponClipAmmo(pBot->Player) / (float)GetPlayerCurrentWeaponMaxClipAmmo(pBot->Player) < 0.5f);
		}
		else
		{
			bShouldReload = bShouldReload && GetPlayerCurrentWeaponClipAmmo(pBot->Player) == 0;
		}

		if (bShouldReload)
		{
			BotReloadCurrentWeapon(pBot);
			return;
		}

	}

	if (IsPlayerReloading(pBot->Player))
	{
		return;
	}

	Vector AimDir = UTIL_GetForwardVector(pBot->Edict->v.v_angle);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	float MinAcceptableAccuracy = (CurrentWeapon == WEAPON_LERK_SPORES || CurrentWeapon == WEAPON_LERK_UMBRA) ? 0.8f : 0.9f;
	if (CurrentWeapon == WEAPON_MARINE_GRENADE || CurrentWeapon == WEAPON_MARINE_GL) { MinAcceptableAccuracy = 0.95f; }

	if (AimDot >= MinAcceptableAccuracy)
	{
		AvHBasePlayerWeapon* WeaponRef = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_pActiveItem);

		if (CurrentWeapon == WEAPON_MARINE_GRENADE)
		{
			if (!WeaponRef->m_flStartThrow && WeaponRef->m_flReleaseThrow == -1)
			{
				pBot->Button |= IN_ATTACK;
			}
		}
		else
		{
			if (!WeaponRef->GetMustPressTriggerForEachShot() || WeaponRef->m_flNextPrimaryAttack <= 0.0f)
			{
				pBot->Button |= IN_ATTACK;
			}
		}

		
	}
}

void BotEvolveLifeform(AvHAIPlayer* pBot, Vector DesiredEvolveLocation, AvHMessageID TargetLifeform)
{
	if (!IsPlayerAlien(pBot->Edict)) { return; }

	float EvolveCost = 0.0f;

	AvHUser3 TargetUser3 = pBot->Player->GetUser3();

	switch (TargetLifeform)
	{
	case ALIEN_LIFEFORM_TWO:
		TargetUser3 = AVH_USER3_ALIEN_PLAYER2;
		EvolveCost = BALANCE_VAR(kGorgeCost);
		break;
	case ALIEN_LIFEFORM_THREE:
		TargetUser3 = AVH_USER3_ALIEN_PLAYER3;
		EvolveCost = BALANCE_VAR(kLerkCost);
		break;
	case ALIEN_LIFEFORM_FOUR:
		TargetUser3 = AVH_USER3_ALIEN_PLAYER4;
		EvolveCost = BALANCE_VAR(kFadeCost);
		break;
	case ALIEN_LIFEFORM_FIVE:
		TargetUser3 = AVH_USER3_ALIEN_PLAYER5;
		EvolveCost = BALANCE_VAR(kOnosCost);
		break;
	default:
		TargetUser3 = AVH_USER3_ALIEN_PLAYER1;
		EvolveCost = 0.0f;
		break;
	}

	// We're already the target lifeform, don't do anything
	if (TargetUser3 == pBot->Player->GetUser3()) { return; }

	Vector EvolvePoint = UTIL_ProjectPointToNavmesh(DesiredEvolveLocation, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

	if (vIsZero(EvolvePoint))
	{
		EvolvePoint = DesiredEvolveLocation;
	}

	if (vDist2DSq(pBot->Edict->v.origin, EvolvePoint) > sqrf(32.0f))
	{
		MoveTo(pBot, EvolvePoint, MOVESTYLE_NORMAL);
		return;
	}



	if (pBot->Player->GetResources() >= EvolveCost)
	{
		pBot->Impulse = TargetLifeform;
	}
}

void BotEvolveUpgrade(AvHAIPlayer* pBot, Vector DesiredEvolveLocation, AvHMessageID TargetUpgrade)
{
	Vector EvolvePoint = UTIL_ProjectPointToNavmesh(DesiredEvolveLocation, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

	if (vIsZero(EvolvePoint))
	{
		EvolvePoint = DesiredEvolveLocation;
	}

	if (vDist2DSq(pBot->Edict->v.origin, EvolvePoint) > sqrf(32.0f))
	{
		MoveTo(pBot, EvolvePoint, MOVESTYLE_NORMAL);
		return;
	}

	pBot->Impulse = TargetUpgrade;
}

void BotUpdateDesiredViewRotation(AvHAIPlayer* pBot)
{
	// We always prioritise MoveLookLocation if it is set so the bot doesn't screw up wall climbing or ladder movement
	Vector NewLookLocation = (!vIsZero(pBot->MoveLookLocation)) ? pBot->MoveLookLocation : pBot->LookTargetLocation;

	// We make an exception for Lerks, they always look where they need to go when flying UNLESS they're aiming at someone/something
	if (IsPlayerLerk(pBot->Edict))
	{
		NewLookLocation = (!vIsZero(pBot->LookTargetLocation)) ? pBot->LookTargetLocation : pBot->MoveLookLocation;
	}

	bool bIsMoveLook = !vIsZero(pBot->MoveLookLocation);

	// We're already interpolating to an existing desired look direction (see BotUpdateViewRotation()) or we don't have a desired look target
	if (!vIsZero(pBot->DesiredLookDirection) || vIsZero(NewLookLocation)) { return; }

	edict_t* pEdict = pBot->Edict;

	Vector dir = UTIL_GetVectorNormal(NewLookLocation - pBot->CurrentEyePosition);

	// Obtain the desired view angles the bot needs to look directly at the target position
	pBot->DesiredLookDirection = UTIL_VecToAngles(dir);

	// Sanity check to make sure we don't end up with NaN values. This causes the bot to start slowly rotating like they're adrift in space
	if (isnan(pBot->DesiredLookDirection.x))
	{
		pBot->DesiredLookDirection = ZERO_VECTOR;
	} 

	// Clamp the pitch and yaw to valid ranges

	if (pBot->DesiredLookDirection.y > 180)
		pBot->DesiredLookDirection.y -= 360;

	// Paulo-La-Frite - START bot aiming bug fix
	if (pBot->DesiredLookDirection.y < -180)
		pBot->DesiredLookDirection.y += 360;

	if (pBot->DesiredLookDirection.x > 180)
		pBot->DesiredLookDirection.x -= 360;

	// Now figure out how far we have to turn to reach our desired target
	float yDelta = pBot->DesiredLookDirection.y - pBot->InterpolatedLookDirection.y;
	float xDelta = pBot->DesiredLookDirection.x - pBot->InterpolatedLookDirection.x;

	// This prevents them turning the long way around

	if (yDelta > 180.0f)
		yDelta -= 360.0f;
	if (yDelta < -180.0f)
		yDelta += 360.0f;

	float maxDelta = fmaxf(fabsf(yDelta), fabsf(xDelta));

	float motion_tracking_skill = (IsPlayerMarine(pBot->Edict)) ? pBot->BotSkillSettings.marine_bot_motion_tracking_skill : pBot->BotSkillSettings.alien_bot_motion_tracking_skill;
	float bot_view_speed = (IsPlayerMarine(pBot->Edict)) ? pBot->BotSkillSettings.marine_bot_view_speed : pBot->BotSkillSettings.alien_bot_view_speed;
	float bot_aim_skill = (IsPlayerMarine(pBot->Edict)) ? pBot->BotSkillSettings.marine_bot_aim_skill : pBot->BotSkillSettings.alien_bot_aim_skill;

	// We add a random offset to the view angles based on how far the bot has to move its view
	// This simulates the fact that humans can't spin and lock their cross-hair exactly on the target, the further you have the spin, the more off your view will be first attempt
	if (fabsf(maxDelta) >= 45.0f)
	{
		pBot->ViewInterpolationSpeed = 500.0f;

		if (!bIsMoveLook)
		{
			pBot->ViewInterpolationSpeed *= bot_view_speed;
			float xOffset = frandrange(10.0f, 20.0f);
			xOffset -= xOffset * bot_aim_skill;

			float yOffset = frandrange(10.0f, 20.0f);
			yOffset -= yOffset * bot_aim_skill;

			if (randbool())
			{
				xOffset *= -1.0f;
			}

			if (randbool())
			{
				yOffset *= -1.0f;
			}



			pBot->DesiredLookDirection.x += xOffset;
			pBot->DesiredLookDirection.y += yOffset;
		}
	}
	else if (fabsf(maxDelta) >= 25.0f)
	{
		pBot->ViewInterpolationSpeed = 250.0f;

		if (!bIsMoveLook)
		{
			pBot->ViewInterpolationSpeed *= bot_view_speed;
			float xOffset = frandrange(5.0f, 10.0f);
			xOffset -= xOffset * bot_aim_skill;

			float yOffset = frandrange(5.0f, 10.0f);
			yOffset -= yOffset * bot_aim_skill;

			if (randbool())
			{
				xOffset *= -1.0f;
			}

			if (randbool())
			{
				yOffset *= -1.0f;
			}

			pBot->DesiredLookDirection.x += xOffset;
			pBot->DesiredLookDirection.y += yOffset;
		}
	}
	else if (fabsf(maxDelta) >= 5.0f)
	{
		pBot->ViewInterpolationSpeed = 50.0f;

		if (!bIsMoveLook)
		{
			pBot->ViewInterpolationSpeed *= bot_view_speed;
			float xOffset = frandrange(2.0f, 5.0f);
			xOffset -= xOffset * bot_aim_skill;

			float yOffset = frandrange(2.0f, 5.0f);
			yOffset -= yOffset * bot_aim_skill;

			if (randbool())
			{
				xOffset *= -1.0f;
			}

			if (randbool())
			{
				yOffset *= -1.0f;
			}

			pBot->DesiredLookDirection.x += xOffset;
			pBot->DesiredLookDirection.y += yOffset;
		}
	}
	else
	{
		pBot->ViewInterpolationSpeed = 50.0f * bot_view_speed;


	}

	if (IsPlayerLerk(pBot->Edict))
	{
		pBot->ViewInterpolationSpeed *= 2.0f;
	}

	// We once again clamp everything to valid values in case the offsets we applied above took us above that

	if (pBot->DesiredLookDirection.y > 180)
		pBot->DesiredLookDirection.y -= 360;

	// Paulo-La-Frite - START bot aiming bug fix
	if (pBot->DesiredLookDirection.y < -180)
		pBot->DesiredLookDirection.y += 360;

	if (pBot->DesiredLookDirection.x > 180)
		pBot->DesiredLookDirection.x -= 360;

	// We finally have our desired turn movement, ready for BotUpdateViewRotation() to pick up and make happen
	pBot->ViewInterpStartedTime = gpGlobals->time;
}

void BotUpdateViewRotation(AvHAIPlayer* pBot, float DeltaTime)
{
	if (!vIsZero(pBot->DesiredLookDirection))
	{
		edict_t* pEdict = pBot->Edict;

		float Delta = pBot->DesiredLookDirection.y - pBot->InterpolatedLookDirection.y;

		if (Delta > 180.0f)
			Delta -= 360.0f;
		if (Delta < -180.0f)
			Delta += 360.0f;

		pBot->InterpolatedLookDirection.x = fInterpConstantTo(pBot->InterpolatedLookDirection.x, pBot->DesiredLookDirection.x, DeltaTime, (IsPlayerClimbingWall(pEdict) ? 400.0f : pBot->ViewInterpolationSpeed));

		float DeltaInterp = fInterpConstantTo(0.0f, Delta, DeltaTime, pBot->ViewInterpolationSpeed);

		pBot->InterpolatedLookDirection.y += DeltaInterp;

		if (pBot->InterpolatedLookDirection.y > 180.0f)
			pBot->InterpolatedLookDirection.y -= 360.0f;
		if (pBot->InterpolatedLookDirection.y < -180.0f)
			pBot->InterpolatedLookDirection.y += 360.0f;

		if (fNearlyEqual(pBot->InterpolatedLookDirection.x, pBot->DesiredLookDirection.x) && fNearlyEqual(pBot->InterpolatedLookDirection.y, pBot->DesiredLookDirection.y))
		{
			pBot->DesiredLookDirection = ZERO_VECTOR;
		}
		else
		{
			// If the interp gets stuck for some reason then abandon it after 2 seconds. It should have completed way before then anyway
			if (gpGlobals->time - pBot->ViewInterpStartedTime > 2.0f)
			{
				pBot->DesiredLookDirection = ZERO_VECTOR;
			}
		}

		pEdict->v.v_angle.x = pBot->InterpolatedLookDirection.x;
		pEdict->v.v_angle.y = pBot->InterpolatedLookDirection.y;

		// set the body angles to point the gun correctly
		pEdict->v.angles.x = pEdict->v.v_angle.x / 3;
		pEdict->v.angles.y = pEdict->v.v_angle.y;
		pEdict->v.angles.z = 0;

		// adjust the view angle pitch to aim correctly (MUST be after body v.angles stuff)
		pEdict->v.v_angle.x = -pEdict->v.v_angle.x;
		// Paulo-La-Frite - END

		pEdict->v.ideal_yaw = pEdict->v.v_angle.y;

		if (pEdict->v.ideal_yaw > 180)
			pEdict->v.ideal_yaw -= 360;

		if (pEdict->v.ideal_yaw < -180)
			pEdict->v.ideal_yaw += 360;
	}

	if (!IsPlayerCommander(pBot->Edict) && (gpGlobals->time - pBot->LastViewUpdateTime) > pBot->ViewUpdateRate)
	{
		BotUpdateView(pBot);
		pBot->LastViewUpdateTime = gpGlobals->time;
	}
}

void BotUpdateView(AvHAIPlayer* pBot)
{
	int visibleCount = 0;

	bool bHasLOSToAnyEnemy = false;

	int EnemyTeam = 0;

	pBot->ViewForwardVector = UTIL_GetForwardVector(pBot->Edict->v.v_angle);

	// Update list of currently visible players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);
		int EnemyIndex = i - 1;

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || !IsPlayerActiveInGame(PlayerEdict) || PlayerEdict->v.team == pBot->Edict->v.team)
		{
			BotClearEnemyTrackingInfo(&pBot->TrackedEnemies[EnemyIndex]);
			continue;
		}

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (!PlayerRef)
		{
			BotClearEnemyTrackingInfo(&pBot->TrackedEnemies[EnemyIndex]);
			continue;
		}

		pBot->TrackedEnemies[EnemyIndex].EnemyPlayer = PlayerRef;
		pBot->TrackedEnemies[EnemyIndex].EnemyEdict = PlayerEdict;

		enemy_status* TrackingInfo = &pBot->TrackedEnemies[EnemyIndex];

		if (gpGlobals->time < TrackingInfo->NextUpdateTime)
		{
			continue;
		}

		edict_t* Enemy = PlayerEdict;

		bool bInFOV = IsPlayerInBotFOV(pBot, Enemy);

		if (!TrackingInfo->bIsAwareOfPlayer && !bInFOV)
		{
			continue;
		}

		Vector VisiblePoint = GetVisiblePointOnPlayerFromObserver(pBot->Edict, Enemy);

		bool bHasLOS = !vIsZero(VisiblePoint);

		bool bIsTracked = (!bHasLOS && (IsPlayerParasited(Enemy) || IsPlayerMotionTracked(Enemy)));

		if (!vIsZero(pBot->LastSafeLocation) && UTIL_PlayerHasLOSToLocation(Enemy, pBot->LastSafeLocation, UTIL_MetresToGoldSrcUnits(50.0f)))
		{
			pBot->LastSafeLocation = ZERO_VECTOR;
		}

		if (bHasLOS)
		{
			bHasLOSToAnyEnemy = true;
		}

		float bot_reaction_time = (IsPlayerMarine(pBot->Edict)) ? pBot->BotSkillSettings.marine_bot_reaction_time : pBot->BotSkillSettings.alien_bot_reaction_time;

		bool bIsVisible = (bInFOV && (bHasLOS || bIsTracked));

		if (bIsVisible != TrackingInfo->bIsVisible)
		{
			if (TrackingInfo->bIsVisible)
			{
				TrackingInfo->EndTrackingTime = gpGlobals->time + 1.0f;
			}

			TrackingInfo->bIsVisible = bIsVisible;
			TrackingInfo->bHasLOS = bHasLOS;

			TrackingInfo->NextUpdateTime = gpGlobals->time + bot_reaction_time;
			continue;
		}

		TrackingInfo->bHasLOS = bHasLOS;

		if (bInFOV && (bHasLOS || bIsTracked))
		{
			Vector FloorLocation = UTIL_GetEntityGroundLocation(Enemy);
			Vector BotVelocity = Enemy->v.velocity;

			if (gpGlobals->time >= TrackingInfo->NextVelocityUpdateTime)
			{
				TrackingInfo->LastSeenVelocity = TrackingInfo->PendingSeenVelocity;
			}

			if (BotVelocity != TrackingInfo->LastSeenVelocity)
			{
				TrackingInfo->PendingSeenVelocity = BotVelocity;
				TrackingInfo->NextVelocityUpdateTime = gpGlobals->time + bot_reaction_time;
			}

			TrackingInfo->bIsAwareOfPlayer = true;
			TrackingInfo->LastSeenLocation = (bHasLOS) ? VisiblePoint : Enemy->v.origin;
			
			if (bHasLOS)
			{
				TrackingInfo->LastVisibleLocation = Enemy->v.origin;
			}

			TrackingInfo->LastFloorPosition = FloorLocation;

			if (bHasLOS)
			{
				TrackingInfo->LastLOSPosition = pBot->CurrentFloorPosition + Vector(0.0f, 0.0f, 5.0f);
				TrackingInfo->LastSeenTime = gpGlobals->time;

				if (vDist2DSq(pBot->Edict->v.origin, TrackingInfo->LastHiddenPosition) < sqrf(18.0f))
				{
					TrackingInfo->LastHiddenPosition = ZERO_VECTOR;
				}

			}
			else
			{
				TrackingInfo->LastHiddenPosition = pBot->CurrentFloorPosition + Vector(0.0f, 0.0f, 5.0f);
				TrackingInfo->LastTrackedTime = gpGlobals->time;
			}

			continue;
		}

		if (!bInFOV || !bHasLOS)
		{
			if (gpGlobals->time < TrackingInfo->EndTrackingTime)
			{
				TrackingInfo->LastSeenLocation = Enemy->v.origin;
			}
		}

		if (bHasLOS)
		{
			TrackingInfo->LastLOSPosition = pBot->CurrentFloorPosition + Vector(0.0f, 0.0f, 5.0f);

			if (TrackingInfo->LastHiddenPosition != ZERO_VECTOR && UTIL_QuickTrace(pBot->Edict, TrackingInfo->LastHiddenPosition, Enemy->v.origin))
			{
				TrackingInfo->LastHiddenPosition = ZERO_VECTOR;
			}
		}
		else
		{
			TrackingInfo->LastHiddenPosition = pBot->Edict->v.origin;

			if (TrackingInfo->LastLOSPosition != ZERO_VECTOR && !UTIL_QuickTrace(pBot->Edict, TrackingInfo->LastLOSPosition, Enemy->v.origin))
			{
				TrackingInfo->LastLOSPosition = ZERO_VECTOR;
			}

		}

		// If we've not been aware of the enemy's location for over 10 seconds, forget about them
		float LastDetectedTime = fmaxf(TrackingInfo->LastSeenTime, TrackingInfo->LastTrackedTime);

		if ((gpGlobals->time - LastDetectedTime) > 10.0f)
		{
			BotClearEnemyTrackingInfo(TrackingInfo);
			continue;
		}
	}

	if (!bHasLOSToAnyEnemy)
	{
		pBot->LastSafeLocation = pBot->Edict->v.origin;
	}
}

void BotClearEnemyTrackingInfo(enemy_status* TrackingInfo)
{
	TrackingInfo->bIsVisible = false;
	TrackingInfo->bHasLOS = false;
	TrackingInfo->LastSeenLocation = ZERO_VECTOR;
	TrackingInfo->LastSeenVelocity = ZERO_VECTOR;
	TrackingInfo->bIsAwareOfPlayer = false;
	TrackingInfo->LastSeenTime = 0.0f;
	TrackingInfo->LastLOSPosition = ZERO_VECTOR;
	TrackingInfo->LastHiddenPosition = ZERO_VECTOR;
}

bool IsPlayerInBotFOV(AvHAIPlayer* Observer, edict_t* TargetPlayer)
{
	Vector TargetVector = (TargetPlayer->v.origin - Observer->CurrentEyePosition).Normalize();

	float DotProduct = UTIL_GetDotProduct(Observer->ViewForwardVector, TargetVector);

	return DotProduct > 0.65f;

}

Vector GetVisiblePointOnPlayerFromObserver(edict_t* Observer, edict_t* TargetPlayer)
{
	Vector TargetCentre = UTIL_GetCentreOfEntity(TargetPlayer);

	TraceResult hit;
	UTIL_TraceLine(GetPlayerEyePosition(Observer), TargetCentre, ignore_monsters, ignore_glass, Observer->v.pContainingEntity, &hit);

	if (hit.flFraction >= 1.0f) { return TargetCentre; }

	AvHUser3 TargetClass = (AvHUser3)TargetPlayer->v.iuser3;

	// Only check the head and feet if we're not a short-arse (i.e. marine, fade or onos)
	if (TargetClass == AVH_USER3_MARINE_PLAYER || TargetClass == AVH_USER3_ALIEN_PLAYER4 || TargetClass == AVH_USER3_ALIEN_PLAYER5)
	{

		UTIL_TraceLine(GetPlayerEyePosition(Observer), GetPlayerEyePosition(TargetPlayer), ignore_monsters, ignore_glass, Observer->v.pContainingEntity, &hit);

		if (hit.flFraction >= 1.0f) { return GetPlayerEyePosition(TargetPlayer); }

		UTIL_TraceLine(GetPlayerEyePosition(Observer), GetPlayerBottomOfCollisionHull(TargetPlayer) + Vector(0.0f, 0.0f, 5.0f), ignore_monsters, ignore_glass, Observer->v.pContainingEntity, &hit);

		if (hit.flFraction >= 1.0f) { return GetPlayerBottomOfCollisionHull(TargetPlayer) + Vector(0.0f, 0.0f, 5.0f); }
	}

	// Skulks are long bois, so check to make sure they don't have their little legs poking out round a corner...
	if (TargetClass == AVH_USER3_ALIEN_PLAYER1)
	{
		Vector ForwardVector = UTIL_GetForwardVector(TargetPlayer->v.angles);

		Vector MinLoc = TargetCentre - (ForwardVector * 55.0f);

		UTIL_TraceLine(GetPlayerEyePosition(Observer), MinLoc, ignore_monsters, ignore_glass, Observer->v.pContainingEntity, &hit);

		if (hit.flFraction >= 1.0f) { return MinLoc; }

		Vector MaxLoc = TargetCentre + (ForwardVector * 55.0f);

		UTIL_TraceLine(GetPlayerEyePosition(Observer), MaxLoc, ignore_monsters, ignore_glass, Observer->v.pContainingEntity, &hit);

		return (hit.flFraction >= 1.0f) ? MaxLoc : ZERO_VECTOR;
	}

	return ZERO_VECTOR;
}

void UpdateBotChat(AvHAIPlayer* pBot)
{
	for (int i = 0; i < 5; i++)
	{
		if (pBot->ChatMessages[i].bIsPending && gpGlobals->time >= pBot->ChatMessages[i].SendTime)
		{
			if (pBot->ChatMessages[i].bIsTeamSay)
			{
				CLIENT_COMMAND(pBot->Edict, "say_team %s", pBot->ChatMessages[i].msg);
			}
			else
			{
				CLIENT_COMMAND(pBot->Edict, "say %s", pBot->ChatMessages[i].msg);
			}
			pBot->ChatMessages[i].bIsPending = false;
			break;
		}
	}
}

void ClearBotInputs(AvHAIPlayer* pBot)
{
	pBot->Button = 0;
	pBot->ForwardMove = 0.0f;
	pBot->SideMove = 0.0f;
	pBot->UpMove = 0.0f;
	pBot->Impulse = 0;
	pBot->Button = 0;
}

void StartNewBotFrame(AvHAIPlayer* pBot)
{
	edict_t* pEdict = pBot->Edict;

	ClearBotInputs(pBot);
	pBot->CurrentEyePosition = GetPlayerEyePosition(pEdict);

	pBot->CurrentFloorPosition = UTIL_GetEntityGroundLocation(pEdict);

	if (vDist3DSq(pBot->BotNavInfo.LastNavMeshCheckPosition, pBot->CurrentFloorPosition) > sqrf(16.0f))
	{
		if (UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam()), pBot->CurrentFloorPosition, 16.0f))
		{
			pBot->BotNavInfo.LastNavMeshPosition = pBot->CurrentFloorPosition;
		}

		pBot->BotNavInfo.LastNavMeshCheckPosition = pBot->CurrentFloorPosition;
	}

	pBot->LookTargetLocation = ZERO_VECTOR;
	pBot->MoveLookLocation = ZERO_VECTOR;
	pBot->LookTarget = nullptr;
	pBot->desiredMovementDir = ZERO_VECTOR;

	pBot->DesiredCombatWeapon = WEAPON_INVALID;
	pBot->DesiredMoveWeapon = WEAPON_INVALID;

	if (IsPlayerSkulk(pEdict))
	{
		pBot->Button |= IN_DUCK;
	}

	if ((pEdict->v.flags & FL_ONGROUND) || IsPlayerOnLadder(pEdict))
	{
		if (!pBot->BotNavInfo.IsOnGround || pBot->BotNavInfo.bHasAttemptedJump)
		{
			pBot->BotNavInfo.LandedTime = gpGlobals->time;
		}

		pBot->BotNavInfo.IsOnGround = true;
		pBot->BotNavInfo.bIsJumping = false;
	}
	else
	{
		pBot->BotNavInfo.IsOnGround = false;

	}

	pBot->BotNavInfo.bHasAttemptedJump = false;

	pBot->BotNavInfo.bShouldWalk = false;

	if (pBot->BotNavInfo.NavProfile.ReachabilityFlag == AI_REACHABILITY_NONE)
	{
		SetBaseNavProfile(pBot);
	}

	if (IsPlayerMarine(pBot->Edict))
	{
		UpdateCommanderOrders(pBot);
	}

	// If we tried placing a building as gorge, and nothing has appeared within the expected time, then mark it as a failed attempt.
	if (pBot->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING)
	{
		if (pBot->ActiveBuildInfo.AttemptedStructureType == STRUCTURE_ALIEN_HIVE)
		{
			// Give a 3-second grace period to check if the hive placement was successful
			if ((gpGlobals->time - pBot->ActiveBuildInfo.BuildAttemptTime) > 3.0f)
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(pBot->ActiveBuildInfo.AttemptedLocation);

				pBot->ActiveBuildInfo.BuildStatus = (NearestHive->Status != HIVE_STATUS_UNBUILT) ? BUILD_ATTEMPT_SUCCESS : BUILD_ATTEMPT_FAILED;
			}
		}
		else
		{
			// All other structures should appear near-instantly
			if ((gpGlobals->time - pBot->ActiveBuildInfo.BuildAttemptTime) > 0.5f)
			{
				pBot->ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_FAILED;
			}
		}

	}

}

void EndBotFrame(AvHAIPlayer* pBot)
{
	UpdateBotStuck(pBot);

	AvHAIWeapon DesiredWeapon = (pBot->DesiredMoveWeapon != WEAPON_INVALID) ? pBot->DesiredMoveWeapon : pBot->DesiredCombatWeapon;

	if (DesiredWeapon != WEAPON_INVALID && GetPlayerCurrentWeapon(pBot->Player) != DesiredWeapon)
	{
		BotSwitchToWeapon(pBot, DesiredWeapon);
	}
}

void CustomThink(AvHAIPlayer* pBot)
{
	if (IsPlayerMarine(pBot->Player))
	{
		if (!PlayerHasWeapon(pBot->Player, WEAPON_MARINE_MINES))
		{
			AvHAIDroppedItem* NearestMines = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_MINES, pBot->Player->GetTeam(), AI_REACHABILITY_MARINE, 0.0f, 5000.0f, false);

			if (NearestMines)
			{
				AITASK_SetPickupTask(pBot, &pBot->PrimaryBotTask, NearestMines->edict, true);
			}
		}
		else
		{
			DeployableSearchFilter MineStructureFilter;
			MineStructureFilter.DeployableTeam = pBot->Player->GetTeam();
			MineStructureFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
			MineStructureFilter.ReachabilityTeam = pBot->Player->GetTeam();
			MineStructureFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;

			AvHAIBuildableStructure* NearestIP = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &MineStructureFilter);

			if (NearestIP)
			{
				AITASK_SetMineStructureTask(pBot, &pBot->PrimaryBotTask, NearestIP->edict, true);
			}
		}

		BotProgressTask(pBot, &pBot->PrimaryBotTask);

		return;
	}


	if (IsPlayerMarine(pBot->Player)) 
	{
		pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

		if (pBot->CurrentEnemy < 0)
		{
			MoveTo(pBot, AITAC_GetTeamStartingLocation(AIMGR_GetEnemyTeam(pBot->Player->GetTeam())), MOVESTYLE_NORMAL);
		}
		else
		{
			MarineCombatThink(pBot);
		}

		return;
	}

	if (!IsPlayerOnos(pBot->Edict))
	{
		if (pBot->Player->GetResources() < BALANCE_VAR(kOnosCost))
		{
			pBot->Player->GiveResources(70.0f);
		}

		BotEvolveLifeform(pBot, pBot->Edict->v.origin, ALIEN_LIFEFORM_FIVE);

		return;
	}

	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

	if (pBot->CurrentEnemy < 0)
	{
		MoveTo(pBot, AITAC_GetTeamStartingLocation(AIMGR_GetEnemyTeam(pBot->Player->GetTeam())), MOVESTYLE_NORMAL);
	}
	else
	{
		AlienCombatThink(pBot);
	}	

}

void DroneThink(AvHAIPlayer* pBot)
{
	AITASK_BotUpdateAndClearTasks(pBot);

	pBot->CurrentTask = &pBot->PrimaryBotTask;

	if (pBot->CommanderTask.TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, &pBot->CommanderTask);
		AITASK_ClearBotTask(pBot, &pBot->PrimaryBotTask);
	}
	else if (pBot->PrimaryBotTask.TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, &pBot->PrimaryBotTask);
	}

	AIDEBUG_DrawBotPath(pBot);

	if (pBot->BotNavInfo.CurrentPathPoint != pBot->BotNavInfo.CurrentPath.end())
	{
		UTIL_DrawLine(INDEXENT(1), pBot->Edict->v.origin, pBot->BotNavInfo.CurrentPathPoint->Location, 0, 255, 255);
	}

}

void SetNewAIPlayerRole(AvHAIPlayer* pBot, AvHAIBotRole NewRole)
{
	if (NewRole != pBot->BotRole)
	{
		AITASK_ClearBotTask(pBot, &pBot->PrimaryBotTask);
		AITASK_ClearBotTask(pBot, &pBot->SecondaryBotTask);

		pBot->BotRole = NewRole;
	}
}

void UpdateAIPlayerCORole(AvHAIPlayer* pBot)
{

}

void UpdateAIPlayerDMRole(AvHAIPlayer* pBot)
{

}

void AIPlayerTakeDamage(AvHAIPlayer* pBot, int damageTaken, edict_t* aggressor)
{
	int aggressorIndex = ENTINDEX(aggressor) - 1;

	if (aggressorIndex > -1 && aggressor->v.team != pBot->Edict->v.team && IsPlayerActiveInGame(aggressor))
	{
		pBot->TrackedEnemies[aggressorIndex].LastSeenTime = gpGlobals->time;

		// If the bot can't see the enemy (bCurrentlyVisible is false) then set the last seen location to a random point in the vicinity so the bot doesn't immediately know where they are
		if (pBot->TrackedEnemies[aggressorIndex].bIsVisible || vDist2DSq(pBot->TrackedEnemies[aggressorIndex].EnemyEdict->v.origin, pBot->Edict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
		{
			pBot->TrackedEnemies[aggressorIndex].LastSeenLocation = aggressor->v.origin;
		}
		else
		{
			// The further the enemy is, the more inaccurate the bot's guess will be where they are
			pBot->TrackedEnemies[aggressorIndex].LastSeenLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(SKULK_BASE_NAV_PROFILE), aggressor->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
		}

		pBot->TrackedEnemies[aggressorIndex].LastSeenVelocity = aggressor->v.velocity;
		pBot->TrackedEnemies[aggressorIndex].bIsAwareOfPlayer = true;
		pBot->TrackedEnemies[aggressorIndex].bHasLOS = true;
	}
}

bool ShouldAIPlayerTakeCommand(AvHAIPlayer* pBot)
{
	AvHAICommanderMode CurrentCommanderMode = AIMGR_GetCommanderMode();

	// Don't go commander if bots are not allowed to
	if (CurrentCommanderMode == COMMANDERMODE_DISABLED) { return false; }

	AvHTeamNumber BotTeamNumber = pBot->Player->GetTeam();
	AvHTeam* BotTeam = GetGameRules()->GetTeam(BotTeamNumber);

	// Don't go commander if we're an alien. You never know with the way I structure my logic...
	if (!BotTeam || BotTeam->GetTeamType() != AVH_CLASS_TYPE_MARINE) { return false; }

	// Don't go commander if we're only supposed to command when there aren't any humans and we have one
	if (CurrentCommanderMode == COMMANDERMODE_IFNOHUMAN && AIMGR_GetNumHumanPlayersOnTeam(BotTeamNumber) > 0) { return false; }

	AvHPlayer* CurrentCommander = BotTeam->GetCommanderPlayer();

	// Don't go commander if we already have one, and it's not us
	if (CurrentCommander)
	{
		return CurrentCommander == pBot->Player;
	}

	// Don't go commander if there is another bot already taking command
	if (AIMGR_GetNumAIPlayersWithRoleOnTeam(BotTeamNumber, BOT_ROLE_COMMAND, pBot) > 0) { return false; }

	float ThisBotDist = vDist2DSq(pBot->Edict->v.origin, AITAC_GetCommChairLocation(BotTeamNumber));

	// Only go commander if we're the closest bot to the chair
	vector <AvHAIPlayer*> BotList = AIMGR_GetAIPlayersOnTeam(BotTeamNumber);

	for (auto it = BotList.begin(); it != BotList.end(); it++)
	{
		AvHAIPlayer* OtherBot = (*it);

		float OtherBotDist = vDist2DSq(OtherBot->Edict->v.origin, AITAC_GetCommChairLocation(BotTeamNumber));

		if (OtherBot != pBot && IsPlayerActiveInGame(pBot->Edict) && OtherBotDist < ThisBotDist)
		{
			// We aren't the closest, let the other guy take command
			return false;
		}
	}

	// We must be the closest!
	return true;
}

void UpdateAIAlienPlayerNSRole(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeamNumber = pBot->Player->GetTeam();

	if (BotTeamNumber == TEAM_IND)
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_NONE);

		return;
	}

	if (AITAC_IsAlienCapperNeeded(pBot))
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_FIND_RESOURCES);
		return;
	}

	if (AITAC_IsAlienBuilderNeeded(pBot))
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_BUILDER);
		return;
	}

	if (AITAC_IsAlienHarasserNeeded(pBot))
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_HARASS);
		return;
	}

	SetNewAIPlayerRole(pBot, BOT_ROLE_ASSAULT);

}

void UpdateAIMarinePlayerNSRole(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeamNumber = pBot->Player->GetTeam();

	if (BotTeamNumber == TEAM_IND)
	{ 
		SetNewAIPlayerRole(pBot, BOT_ROLE_NONE);
		
		return;
	}
		
	if (ShouldAIPlayerTakeCommand(pBot))
	{
		// We're going to go commander!
		SetNewAIPlayerRole(pBot, BOT_ROLE_COMMAND);
		return;
	}

	int NumSweeperBots = AIMGR_GetNumAIPlayersWithRoleOnTeam(BotTeamNumber, BOT_ROLE_SWEEPER, pBot);

	// Always have a sweeper
	if (NumSweeperBots < 1)
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_SWEEPER);
		return;
	}

	// Always go bombardier if we have a grenade launcher
	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL))
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_BOMBARDIER);
		return;
	}

	// If we own less than half the res nodes in the map, then we want 2 marines to cap them. Otherwise, have 1
	float ResNodeOwnership = AITAC_GetTeamResNodeOwnership(BotTeamNumber, true);

	int DesiredResCappers = (ResNodeOwnership < 0.5f) ? 2 : 1;

	int NumCappers = AIMGR_GetNumAIPlayersWithRoleOnTeam(BotTeamNumber, BOT_ROLE_FIND_RESOURCES, pBot);

	if (NumCappers < DesiredResCappers)
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_FIND_RESOURCES);
		return;
	}

	// Everyone else goes assault
	SetNewAIPlayerRole(pBot, BOT_ROLE_ASSAULT);

}

void AIPlayerNSThink(AvHAIPlayer* pBot)
{
	AvHTeam* BotTeam = GetGameRules()->GetTeam(pBot->Player->GetTeam());

	if (!BotTeam) { return; }

	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

	if (BotTeam->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		AIPlayerNSMarineThink(pBot);
	}
	else
	{
		AIPlayerNSAlienThink(pBot);
	}
}

int BotGetNextEnemyTarget(AvHAIPlayer* pBot)
{
	int NearestVisibleEnemy = -1;
	int NearestUnseenEnemy = -1;

	float ClosestVisibleDist = 0.0f;
	float ClosestUnseenDist = 0.0f;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		if (!pBot->TrackedEnemies[i].bIsAwareOfPlayer) { continue; }

		enemy_status* TrackingInfo = &pBot->TrackedEnemies[i];

		float Dist = vDist2DSq(TrackingInfo->LastSeenLocation, pBot->Edict->v.origin);

		if (TrackingInfo->bHasLOS)
		{
			if (NearestVisibleEnemy < 0 || Dist < ClosestVisibleDist)
			{
				NearestVisibleEnemy = i;
				ClosestVisibleDist = Dist;
			}
		}
		else
		{
			// Ignore tracked enemies if we've not seen them in a while and we have something important to do
			if (pBot->CurrentTask && (pBot->CurrentTask->bTaskIsUrgent || pBot->CurrentTask->bIssuedByCommander))
			{
				if ((gpGlobals->time - TrackingInfo->LastSeenTime) > 5.0f) { continue; }
			}

			if (Dist > sqrf(UTIL_MetresToGoldSrcUnits(15.0f)) && (gpGlobals->time - TrackingInfo->LastSeenTime) > 10.0f)
			{
				continue;
			}

			if (NearestUnseenEnemy < 0 || Dist < ClosestUnseenDist)
			{
				NearestUnseenEnemy = i;
				ClosestUnseenDist = Dist;
			}
		}
	}

	return (NearestVisibleEnemy > -1) ? NearestVisibleEnemy : NearestUnseenEnemy;

}

AvHAICombatStrategy GetBotCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	if (FNullEnt(CurrentEnemy->EnemyEdict) || !IsPlayerActiveInGame(CurrentEnemy->EnemyEdict)) { return COMBAT_STRATEGY_IGNORE; }

	if (IsPlayerAlien(pBot->Edict))
	{
		return GetAlienCombatStrategyForTarget(pBot, CurrentEnemy);
	}
	else
	{
		return GetMarineCombatStrategyForTarget(pBot, CurrentEnemy);
	}
}

AvHAICombatStrategy GetAlienCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	AvHUser3 PlayerUser3 = pBot->Player->GetUser3();

	switch (PlayerUser3)
	{
		case AVH_USER3_ALIEN_PLAYER1:
			return GetSkulkCombatStrategyForTarget(pBot, CurrentEnemy);
		case AVH_USER3_ALIEN_PLAYER2:
			return GetGorgeCombatStrategyForTarget(pBot, CurrentEnemy);
		case AVH_USER3_ALIEN_PLAYER3:
			return GetLerkCombatStrategyForTarget(pBot, CurrentEnemy);
		case AVH_USER3_ALIEN_PLAYER4:
			return GetFadeCombatStrategyForTarget(pBot, CurrentEnemy);
		case AVH_USER3_ALIEN_PLAYER5:
			return GetOnosCombatStrategyForTarget(pBot, CurrentEnemy);
		default:
			return COMBAT_STRATEGY_IGNORE;
	}
}

AvHAICombatStrategy GetSkulkCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	float CurrentHealthPercent = GetPlayerOverallHealthPercent(pBot->Edict);

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		if (CurrentHealthPercent < 0.99f)
		{
			return COMBAT_STRATEGY_RETREAT;
		}
	}

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, CurrentEnemy->LastSeenLocation);

	bool bInAmbushRange = DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)) && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f));

	// If we are rushing to defend something, ignore enemies who are not a threat to our target
	if (pBot->CurrentTask && pBot->CurrentTask->TaskType == TASK_DEFEND)
	{
		if ((!CurrentEnemy->bHasLOS || DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) && !UTIL_PlayerHasLOSToEntity(CurrentEnemy->EnemyEdict, pBot->CurrentTask->TaskTarget, UTIL_MetresToGoldSrcUnits(30.0f), false))
		{
			return COMBAT_STRATEGY_IGNORE;
		}

		return COMBAT_STRATEGY_ATTACK;
	}

	// Jig's up, just get in there
	if (CurrentEnemy->bHasLOS && DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		return COMBAT_STRATEGY_ATTACK;
	}

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	int NumEnemyAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, CurrentEnemy->EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), CurrentEnemy->EnemyEdict);
	int NumFriends = AITAC_GetNumPlayersOnTeamWithLOS(BotTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), pBot->Edict);

	Vector EnemyFacing = UTIL_GetForwardVector2D(CurrentEnemy->EnemyEdict->v.angles);
	Vector OurOrientation = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - CurrentEnemy->EnemyEdict->v.origin);

	float FacingDot = UTIL_GetDotProduct2D(EnemyFacing, OurOrientation);

	bool bIsEnemyDistracted = FacingDot < 0.4f || CurrentEnemy->EnemyEdict->v.weaponmodel == 0 || IsPlayerReloading(pBot->Player);

	if ((NumEnemyAllies <= NumFriends || NumFriends >= 2) || (NumFriends >= NumEnemyAllies - 1 && bIsEnemyDistracted))
	{
		return COMBAT_STRATEGY_ATTACK;
	}

	Vector EnemyVelocity = UTIL_GetVectorNormal2D(CurrentEnemy->LastSeenVelocity);

	float MoveDot = UTIL_GetDotProduct2D(EnemyVelocity, OurOrientation);

	if (MoveDot > 0.0f)
	{
		return COMBAT_STRATEGY_AMBUSH;
	}
	else
	{
		return COMBAT_STRATEGY_SKIRMISH;
	}

	return COMBAT_STRATEGY_ATTACK;
}

AvHAICombatStrategy GetGorgeCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	float CurrentHealthPercent = GetPlayerOverallHealthPercent(pBot->Edict);

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		if (CurrentHealthPercent < 0.99f)
		{
			return COMBAT_STRATEGY_RETREAT;
		}
	}

	vector<AvHPlayer*> Allies = AITAC_GetAllPlayersOnTeamWithLOS(BotTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), pBot->Edict);
	bool bHasBackup = false;

	Vector EnemyLocation = (CurrentEnemy->bHasLOS) ? CurrentEnemy->EnemyEdict->v.origin : CurrentEnemy->LastVisibleLocation;

	for (auto it = Allies.begin(); it != Allies.end(); it++)
	{
		AvHPlayer* ThisAlly = (*it);
		edict_t* ThisAllyEdict = ThisAlly->edict();

		if (ThisAllyEdict->v.iuser3 == AVH_USER3_ALIEN_PLAYER2) { continue; }

		if (vDist2DSq(ThisAllyEdict->v.origin, EnemyLocation) < vDist2DSq(pBot->Edict->v.origin, EnemyLocation))
		{
			if (UTIL_PlayerHasLOSToLocation(ThisAllyEdict, EnemyLocation, UTIL_MetresToGoldSrcUnits(20.0f)))
			{
				bHasBackup = true;
			}
		}		
	}

	if (bHasBackup)
	{
		return (CurrentHealthPercent > 0.5f) ? COMBAT_STRATEGY_ATTACK : COMBAT_STRATEGY_SKIRMISH;
	}
	else
	{
		return (CurrentHealthPercent > 0.5f) ? COMBAT_STRATEGY_SKIRMISH : COMBAT_STRATEGY_RETREAT;
	}
}

AvHAICombatStrategy GetLerkCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = CurrentEnemy->EnemyPlayer->GetTeam();

	float CurrentHealthPercent = GetPlayerOverallHealthPercent(pBot->Edict);

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		if (CurrentHealthPercent < 0.99f)
		{
			return COMBAT_STRATEGY_RETREAT;
		}
	}

	edict_t* EnemyEdict = CurrentEnemy->EnemyEdict;

	float EnemyHealthPercent = GetPlayerOverallHealthPercent(EnemyEdict);
	int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, EnemyEdict->v.origin);

	float RetreatHealthPercent = (NumAllies > 1) ? 0.5f : 0.35f;

	if (CurrentHealthPercent < RetreatHealthPercent)
	{
		return COMBAT_STRATEGY_RETREAT;
	}

	// Player has a deadly weapon if they have a shotgun, or they have an HMG with ammo in the chamber and are not reloading (we can strike if they are!)
	bool bEnemyHasDeadlyWeapon = (PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_HMG) && UTIL_GetPlayerPrimaryWeaponClipAmmo(CurrentEnemy->EnemyPlayer) > 10 && !IsPlayerReloading(CurrentEnemy->EnemyPlayer)) || PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_SHOTGUN);

	// Should we YOLO?
	if (NumAllies == 0 && !bEnemyHasDeadlyWeapon && !PlayerHasHeavyArmour(EnemyEdict))
	{
		Vector EnemyFacing = UTIL_GetForwardVector2D(EnemyEdict->v.angles);
		Vector OurOrientation = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - EnemyEdict->v.origin);

		float FacingDot = UTIL_GetDotProduct2D(EnemyFacing, OurOrientation);

		if (CurrentHealthPercent > 0.7f || DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(5.0)) || FacingDot < 0.0f)
		{
			return COMBAT_STRATEGY_ATTACK;
		}
	}

	// If we are getting low on health, or the player has a weapon that would shred us...
	if (CurrentHealthPercent < 0.6f || bEnemyHasDeadlyWeapon)
	{
		Vector EnemyFacing = UTIL_GetForwardVector2D(EnemyEdict->v.angles);
		Vector OurOrientation = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - EnemyEdict->v.origin);

		float FacingDot = UTIL_GetDotProduct2D(EnemyFacing, OurOrientation);

		// Only close in for the kill if the enemy is alone, weak that they won't have time to fight back, we can close the distance quickly, and they're not expecting us
		if (EnemyHealthPercent < 0.5f && DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)) && NumAllies == 0 && FacingDot < 0.0f)
		{
			return COMBAT_STRATEGY_ATTACK;
		}
		else
		{
			return COMBAT_STRATEGY_SKIRMISH;
		}		
	}

	// We are in good shape and the enemy doesn't have a nasty weapon that could hurt us. Go for the kill if they're low on health and are alone

	if (EnemyHealthPercent < 0.5f && NumAllies == 0)
	{
		return COMBAT_STRATEGY_ATTACK;
	}
	else
	{
		return COMBAT_STRATEGY_SKIRMISH;
	}
}

AvHAICombatStrategy GetFadeCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	edict_t* EnemyEdict = CurrentEnemy->EnemyEdict;

	float CurrentHealthPercent = GetPlayerOverallHealthPercent(pBot->Edict);

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		if (CurrentHealthPercent < 0.99f)
		{
			// We must be fade or onos, be more nuanced about when we decide to jump back into combat

			float MinHealthPercent = 0.5f;

			// Generally don't return to combat until we're at least at 50% capacity
			if (CurrentHealthPercent < MinHealthPercent) { return COMBAT_STRATEGY_RETREAT; }

			// If our enemy has a more painful weapon like a shotgun or HMG, make sure we're a little more healed up
			if (PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_HMG) || PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_SHOTGUN))
			{
				MinHealthPercent += 0.15f;
			}

			int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(CurrentEnemy->EnemyPlayer->GetTeam(), EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

			if (NumAllies > 0)
			{
				MinHealthPercent += (0.15f * (float)NumAllies);
			}

			MinHealthPercent = clampf(MinHealthPercent, 0.0f, 0.99f);

			// We don't feel strong enough to tackle the challenge yet
			if (CurrentHealthPercent < MinHealthPercent) { return COMBAT_STRATEGY_RETREAT; }
		}
	}

	// Player has a deadly weapon if they have a shotgun, or they have an HMG with ammo in the chamber and are not reloading (we can strike if they are!)
	bool bEnemyHasDeadlyWeapon = (PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_HMG) && UTIL_GetPlayerPrimaryWeaponClipAmmo(CurrentEnemy->EnemyPlayer) > 10 && !IsPlayerReloading(CurrentEnemy->EnemyPlayer)) || PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_SHOTGUN);

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, EnemyEdict->v.origin);

	int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

	// If we are rushing to defend something, ignore enemies who are not a threat to our target
	if (pBot->CurrentTask && pBot->CurrentTask->TaskType == TASK_DEFEND)
	{
		if ((!CurrentEnemy->bHasLOS || DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) && !UTIL_PlayerHasLOSToEntity(CurrentEnemy->EnemyEdict, pBot->CurrentTask->TaskTarget, UTIL_MetresToGoldSrcUnits(30.0f), false))
		{
			return COMBAT_STRATEGY_IGNORE;
		}
	}

	// First, check if we should get the hell out of dodge
	float RetreatHealth = 0.33f;

	if ((NumAllies > 1 || bEnemyHasDeadlyWeapon) && vDist2DSq(pBot->Edict->v.origin, pBot->LastSafeLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		RetreatHealth = 0.5f;
	}

	if (CurrentEnemy->bHasLOS && CurrentHealthPercent < RetreatHealth)
	{
		return COMBAT_STRATEGY_RETREAT;
	}

	Vector EnemyFacing = UTIL_GetForwardVector2D(EnemyEdict->v.angles);
	Vector OurOrientation = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - EnemyEdict->v.origin);

	float FacingDot = UTIL_GetDotProduct2D(EnemyFacing, OurOrientation);

	// Can we skirmish?
	if (PlayerHasWeapon(pBot->Player, WEAPON_FADE_ACIDROCKET))
	{
		if (DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			if ((bEnemyHasDeadlyWeapon && (FacingDot > 0.5f || NumAllies > 0)) || NumAllies > 2)
			{
				return COMBAT_STRATEGY_SKIRMISH;
			}
		}
	}
	else
	{
		if ((bEnemyHasDeadlyWeapon && (FacingDot > 0.5f || NumAllies > 0)) || NumAllies > 2)
		{
			Vector EnemyVelocity = UTIL_GetVectorNormal2D(CurrentEnemy->LastSeenVelocity);

			float MoveDot = UTIL_GetDotProduct2D(EnemyVelocity, OurOrientation);

			if (MoveDot > 0.0f)
			{
				return COMBAT_STRATEGY_AMBUSH;
			}
		}
	}

	return COMBAT_STRATEGY_ATTACK;

}

AvHAICombatStrategy GetOnosCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	edict_t* EnemyEdict = CurrentEnemy->EnemyEdict;

	float CurrentHealthPercent = GetPlayerOverallHealthPercent(pBot->Edict);

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		if (CurrentHealthPercent < 0.99f)
		{
			// We must be fade or onos, be more nuanced about when we decide to jump back into combat

			float MinHealthPercent = 0.4f;

			// Generally don't return to combat until we're at least at 50% capacity
			if (CurrentHealthPercent < MinHealthPercent) { return COMBAT_STRATEGY_RETREAT; }

			// If our enemy has a more painful weapon like a shotgun or HMG, make sure we're a little more healed up
			if (PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_HMG) || PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_SHOTGUN))
			{
				MinHealthPercent += 0.1f;
			}

			int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(CurrentEnemy->EnemyPlayer->GetTeam(), EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

			if (NumAllies > 0)
			{
				MinHealthPercent += (0.1f * (float)NumAllies);
			}

			MinHealthPercent = clampf(MinHealthPercent, 0.0f, 0.99f);

			// We don't feel strong enough to tackle the challenge yet
			if (CurrentHealthPercent < MinHealthPercent) { return COMBAT_STRATEGY_RETREAT; }
		}
	}

	// Player has a deadly weapon if they have a shotgun, or they have an HMG with ammo in the chamber and are not reloading (we can strike if they are!)
	bool bEnemyHasDeadlyWeapon = (PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_HMG) && UTIL_GetPlayerPrimaryWeaponClipAmmo(CurrentEnemy->EnemyPlayer) > 10 && !IsPlayerReloading(CurrentEnemy->EnemyPlayer)) || PlayerHasWeapon(CurrentEnemy->EnemyPlayer, WEAPON_MARINE_SHOTGUN);

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, EnemyEdict->v.origin);

	int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

	// If we are rushing to defend something, ignore enemies who are not a threat to our target
	if (pBot->CurrentTask && pBot->CurrentTask->TaskType == TASK_DEFEND)
	{
		if ((!CurrentEnemy->bHasLOS || DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) && !UTIL_PlayerHasLOSToEntity(CurrentEnemy->EnemyEdict, pBot->CurrentTask->TaskTarget, UTIL_MetresToGoldSrcUnits(30.0f), false))
		{
			return COMBAT_STRATEGY_IGNORE;
		}
	}

	// First, check if we should get the hell out of dodge
	float RetreatHealth = 0.25f;

	if (DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && (bEnemyHasDeadlyWeapon || NumAllies > 1))
	{
		RetreatHealth = 0.35f;
	}

	if (CurrentEnemy->bHasLOS && CurrentHealthPercent < RetreatHealth)
	{
		return COMBAT_STRATEGY_RETREAT;
	}

	return COMBAT_STRATEGY_ATTACK;
}

AvHAICombatStrategy GetMarineCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	edict_t* EnemyEdict = CurrentEnemy->EnemyEdict;

	float CurrentHealthPercent = (pBot->Edict->v.health / pBot->Edict->v.max_health);

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, CurrentEnemy->LastSeenLocation);

	// If we are doing something important, don't get distracted by enemies that aren't an immediate threat
	if (pBot->CurrentTask && pBot->CurrentTask->TaskType == TASK_DEFEND || pBot->CommanderTask.TaskType != TASK_NONE)
	{
		if ((!CurrentEnemy->bHasLOS || DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) && (!vIsZero(pBot->CurrentTask->TaskLocation) && !UTIL_PlayerHasLOSToLocation(CurrentEnemy->EnemyEdict, pBot->CurrentTask->TaskLocation, UTIL_MetresToGoldSrcUnits(30.0f))))
		{
			return COMBAT_STRATEGY_IGNORE;
		}
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		int MinDesiredAmmo = imini(UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player), UTIL_GetPlayerPrimaryWeaponMaxClipSize(pBot->Player) * 2);

		if (CurrentHealthPercent < 0.5f || UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < MinDesiredAmmo)
		{
			return COMBAT_STRATEGY_RETREAT;
		}
	}

	int NumEnemyAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), EnemyEdict);
	int NumFriendlies = AITAC_GetNumPlayersOnTeamWithLOS(BotTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), pBot->Edict);

	if (CurrentHealthPercent < 0.3f || (CurrentHealthPercent < 0.5f && NumEnemyAllies > 0) || UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryWeaponMaxClipSize(pBot->Player))
	{
		return COMBAT_STRATEGY_RETREAT;
	}

	// Shotty users should attack, can't really skirmish with a shotgun
	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_SHOTGUN) && (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) > 0 || UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0))
	{
		return COMBAT_STRATEGY_ATTACK;
	}

	bool bIsEnemyRanged = IsPlayerMarine(CurrentEnemy->EnemyPlayer);

	if (bIsEnemyRanged || PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL))
	{
		return COMBAT_STRATEGY_SKIRMISH;
	}

	// If we're up against a stronger enemy than us, skirmish instead to avoid getting wiped out
	if (!PlayerHasHeavyArmour(pBot->Edict) && (CurrentEnemy->EnemyPlayer->GetUser3() > AVH_USER3_ALIEN_PLAYER3 || NumEnemyAllies > 0 || PlayerHasHeavyArmour(EnemyEdict)))
	{
		return COMBAT_STRATEGY_SKIRMISH;
	}

	return COMBAT_STRATEGY_ATTACK;
}

AvHAIPlayerTask* AIPlayerGetNextTask(AvHAIPlayer* pBot)
{

	// Any orders issued by the commander take priority over everything else
	if (pBot->CommanderTask.TaskType != TASK_NONE)
	{
		if (pBot->SecondaryBotTask.bTaskIsUrgent)
		{
			return &pBot->SecondaryBotTask;
		}
		else
		{
			if (pBot->WantsAndNeedsTask.bTaskIsUrgent)
			{
				return &pBot->WantsAndNeedsTask;
			}
			else
			{
				return &pBot->CommanderTask;
			}
		}
	}

	// Prioritise healing our friends (heal tasks are only valid if the target is close by anyway)
	if (pBot->SecondaryBotTask.TaskType == TASK_HEAL)
	{
		return &pBot->SecondaryBotTask;
	}

	if (AITASK_IsTaskUrgent(pBot, &pBot->WantsAndNeedsTask))
	{
		return &pBot->WantsAndNeedsTask;
	}

	if (AITASK_IsTaskUrgent(pBot, &pBot->PrimaryBotTask))
	{
		return &pBot->PrimaryBotTask;
	}

	if (AITASK_IsTaskUrgent(pBot, &pBot->SecondaryBotTask))
	{
		return &pBot->SecondaryBotTask;
	}

	if (pBot->WantsAndNeedsTask.TaskType != TASK_NONE)
	{
		return &pBot->WantsAndNeedsTask;
	}

	if (pBot->SecondaryBotTask.TaskType != TASK_NONE)
	{
		return &pBot->SecondaryBotTask;
	}

	return &pBot->PrimaryBotTask;
}

void AIPlayerNSMarineThink(AvHAIPlayer* pBot)
{
	UpdateAIMarinePlayerNSRole(pBot);

	if (pBot->BotRole == BOT_ROLE_COMMAND)
	{
		AICOMM_CommanderThink(pBot);
		return;
	}

	if (!pBot->CurrentTask) { pBot->CurrentTask = &pBot->PrimaryBotTask; }

	if (gpGlobals->time >= pBot->BotNextTaskEvaluationTime)
	{
		pBot->BotNextTaskEvaluationTime = gpGlobals->time + frandrange(0.2f, 0.5f);

		AITASK_BotUpdateAndClearTasks(pBot);

		AIPlayerSetPrimaryMarineTask(pBot, &pBot->PrimaryBotTask);
		AIPlayerSetSecondaryMarineTask(pBot, &pBot->SecondaryBotTask);
		AIPlayerSetWantsAndNeedsMarineTask(pBot, &pBot->WantsAndNeedsTask);
	}

	pBot->CurrentTask = AIPlayerGetNextTask(pBot);

	if (pBot->CurrentEnemy > -1)
	{
		if (MarineCombatThink(pBot)) { return; }
	}

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}

	if (pBot->DesiredCombatWeapon == WEAPON_NONE)
	{
		pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, nullptr);
	}
}

void MarineHuntEnemy(AvHAIPlayer* pBot, enemy_status* TrackedEnemy)
{
	edict_t* CurrentEnemy = TrackedEnemy->EnemyEdict;

	if (FNullEnt(CurrentEnemy) || IsPlayerDead(CurrentEnemy)) { return; }

	float TimeSinceLastSighting = (gpGlobals->time - TrackedEnemy->LastSeenTime);

	// If the enemy is being motion tracked, or the last seen time was within the last 5 seconds, and the suspected location is close enough, then throw a grenade!
	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GRENADE) || ((PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL) && (UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0 || UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) > 0))))
	{
		if (TimeSinceLastSighting < 5.0f && vDist3DSq(pBot->Edict->v.origin, TrackedEnemy->LastSeenLocation) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			Vector GrenadeThrowLocation = UTIL_GetGrenadeThrowTarget(pBot->Edict, TrackedEnemy->LastSeenLocation, UTIL_MetresToGoldSrcUnits(5.0f), false);

			if (GrenadeThrowLocation != ZERO_VECTOR)
			{
				BotThrowGrenadeAtTarget(pBot, GrenadeThrowLocation);
				return;
			}
		}
	}

	pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, CurrentEnemy);

	if (GetPlayerCurrentWeapon(pBot->Player) != pBot->DesiredCombatWeapon) { return; }

	if (UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, TrackedEnemy->LastSeenLocation, max_player_use_reach))
	{
		MoveTo(pBot, TrackedEnemy->LastFloorPosition, MOVESTYLE_NORMAL);
	}

	return;
}

void BotThrowGrenadeAtTarget(AvHAIPlayer* pBot, const Vector TargetPoint)
{
	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL) && (UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0 || UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) > 0))
	{
		pBot->DesiredCombatWeapon = WEAPON_MARINE_GL;

	}
	else
	{
		pBot->DesiredCombatWeapon = WEAPON_MARINE_GRENADE;
	}

	if (GetPlayerCurrentWeapon(pBot->Player) != pBot->DesiredCombatWeapon)
	{
		return;
	}


	Vector ThrowAngle = GetPitchForProjectile(pBot->CurrentEyePosition, TargetPoint, UTIL_GetProjectileVelocityForWeapon(GetPlayerCurrentWeapon(pBot->Player)), GOLDSRC_GRAVITY);

	ThrowAngle = UTIL_GetVectorNormal(ThrowAngle);

	Vector ThrowTargetLocation = pBot->CurrentEyePosition + (ThrowAngle * 200.0f);

	BotLookAt(pBot, ThrowTargetLocation);

	if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_MARINE_GL && UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) == 0)
	{
		BotReloadCurrentWeapon(pBot);
		return;
	}

	BotShootLocation(pBot, GetPlayerCurrentWeapon(pBot->Player), ThrowTargetLocation);
}
bool BombardierCombatThink(AvHAIPlayer* pBot)
{
	return false;
}

bool RegularMarineCombatThink(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	edict_t* pEdict = pBot->Edict;

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	AvHAIDroppedItem* NearestHealthPack = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_HEALTHPACK, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);
	AvHAIDroppedItem* NearestAmmoPack = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_AMMO, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);

	AvHAIWeapon DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, CurrentEnemy);

	bool bBotIsGrenadier = (DesiredCombatWeapon == WEAPON_MARINE_GL);

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, CurrentEnemy->v.origin);

	bool bEnemyIsRanged = IsPlayerMarine(TrackedEnemyRef->EnemyPlayer) || ((GetPlayerCurrentWeapon(TrackedEnemyRef->EnemyPlayer) == WEAPON_FADE_ACIDROCKET) && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));

	float LastEnemySeenTime = (TrackedEnemyRef->LastTrackedTime > 0.0f) ? TrackedEnemyRef->LastTrackedTime : TrackedEnemyRef->LastSeenTime;
	Vector LastEnemySeenLocation = TrackedEnemyRef->LastSeenLocation;

	// Run away and restock
	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		if (NearestHealthPack && (pBot->Edict->v.health < pBot->Edict->v.max_health * 0.7f))
		{
			MoveTo(pBot, NearestHealthPack->Location, MOVESTYLE_NORMAL);
		}
		else if (NearestAmmoPack && UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player))
		{
			MoveTo(pBot, NearestAmmoPack->Location, MOVESTYLE_NORMAL);
		}
		else
		{
			DeployableSearchFilter NearestArmoury;
			NearestArmoury.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
			NearestArmoury.DeployableTeam = BotTeam;
			NearestArmoury.ReachabilityTeam = BotTeam;
			NearestArmoury.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
			NearestArmoury.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			NearestArmoury.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

			AvHAIBuildableStructure* NearestArmouryRef = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &NearestArmoury);

			if (NearestArmouryRef && !IsAreaAffectedBySpores(NearestArmouryRef->Location))
			{
				if (!TrackedEnemyRef->bHasLOS || (IsPlayerAlien(pBot->Edict) && vDist2DSq(NearestArmouryRef->Location, CurrentEnemy->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))))
				{
					if (IsPlayerInUseRange(pBot->Edict, NearestArmouryRef->edict))
					{
						BotUseObject(pBot, NearestArmouryRef->edict, true);
						return true;
					}
				}

				MoveTo(pBot, NearestArmouryRef->Location, MOVESTYLE_NORMAL);

			}
			else
			{
				MoveTo(pBot, AITAC_GetTeamStartingLocation(BotTeam), MOVESTYLE_NORMAL);
			}
		}

		if (TrackedEnemyRef->bHasLOS)
		{
			BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, CurrentEnemy);

			if (DesiredCombatWeapon != WEAPON_MARINE_KNIFE)
			{
				if (DistToEnemy < sqrf(100.0f))
				{
					if (IsPlayerReloading(pBot->Player) && CanInterruptWeaponReload(GetPlayerCurrentWeapon(pBot->Player)) && GetPlayerCurrentWeaponClipAmmo(pBot->Player) > 0)
					{
						InterruptReload(pBot);
					}
					BotJump(pBot);
				}
			}

			if (LOSCheck == ATTACK_SUCCESS)
			{
				if (!bBotIsGrenadier || DistToEnemy > sqrf(BALANCE_VAR(kGrenadeRadius)))
				{
					BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
					return true;
				}
			}
		}
		
		if (UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) == 0 && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
		{
			BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, CurrentEnemy);

			if (LOSCheck == ATTACK_SUCCESS)
			{
				BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
			}
			else
			{
				BotReloadWeapons(pBot);
			}
		}
		
		return true;
	}

	// Maintain distance, pop and shoot
	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_SKIRMISH || pBot->CurrentCombatStrategy == COMBAT_STRATEGY_AMBUSH)
	{
		if (vIsZero(pBot->LastSafeLocation))
		{
			pBot->LastSafeLocation = AITAC_GetTeamStartingLocation(BotTeam);
		}

		if (TrackedEnemyRef->bHasLOS)
		{
			if (GetPlayerCurrentWeaponClipAmmo(pBot->Player) == 0)
			{
				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
				BotReloadWeapons(pBot);
				return true;
			}

			if (vDist2DSq(pBot->Edict->v.origin, pBot->LastSafeLocation) > sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
			{
				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
			}
			else
			{
				if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GRENADE) && DesiredCombatWeapon != WEAPON_MARINE_GL)
				{
					// Plus 1 to include the target themselves
					int NumTargets = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, CurrentEnemy->v.origin, BALANCE_VAR(kGrenadeRadius), nullptr);

					if (NumTargets > 1)
					{
						BotThrowGrenadeAtTarget(pBot, CurrentEnemy->v.origin);
						return true;
					}
				}

				if (bEnemyIsRanged)
				{
					Vector EnemyOrientation = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->Edict->v.origin);

					Vector RightDir = UTIL_GetCrossProduct(EnemyOrientation, UP_VECTOR);

					pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(RightDir) : UTIL_GetVectorNormal2D(-RightDir);

					// Let's get ziggy with it
					if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
					{
						pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
						pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
					}

					BotMovementInputs(pBot);
				}
				else
				{
					if (DesiredCombatWeapon != WEAPON_MARINE_KNIFE)
					{
						if (DistToEnemy < sqrf(100.0f))
						{
							if (IsPlayerReloading(pBot->Player) && CanInterruptWeaponReload(GetPlayerCurrentWeapon(pBot->Player)) && GetPlayerCurrentWeaponClipAmmo(pBot->Player) > 0)
							{
								InterruptReload(pBot);
							}
							BotJump(pBot);
						}
					}
				}
			}

			BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
		}
		else
		{
			if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GRENADE) || (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL) && UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0))
			{
				Vector GrenadeTarget = UTIL_GetGrenadeThrowTarget(pBot->Edict, LastEnemySeenLocation, BALANCE_VAR(kGrenadeRadius), true);

				if (!vIsZero(GrenadeTarget))
				{
					BotThrowGrenadeAtTarget(pBot, GrenadeTarget);
					return true;
				}
			}

			if (BotReloadWeapons(pBot)) { return true; }

			MoveTo(pBot, LastEnemySeenLocation, MOVESTYLE_NORMAL);
		}

		return true;
	}

	// Go for the kill. Maintain desired distance and pursue when needed
	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_ATTACK)
	{
		AvHAIWeapon IdealAttackWeapon = (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) > 0 || UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0) ? UTIL_GetPlayerPrimaryWeapon(pBot->Player) : DesiredCombatWeapon;

		float DesiredDistance = GetMinIdealWeaponRange(IdealAttackWeapon) + ((GetMaxIdealWeaponRange(IdealAttackWeapon) - GetMinIdealWeaponRange(IdealAttackWeapon)) * 0.5f);

		bool bCanReloadCurrentWeapon = (WeaponCanBeReloaded(DesiredCombatWeapon) && GetPlayerCurrentWeaponClipAmmo(pBot->Player) < GetPlayerCurrentWeaponMaxClipAmmo(pBot->Player) && GetPlayerCurrentWeaponReserveAmmo(pBot->Player) > 0);
		bool bMustReloadCurrentWeapon = bCanReloadCurrentWeapon && GetPlayerCurrentWeaponClipAmmo(pBot->Player) == 0;

		if (vIsZero(pBot->LastSafeLocation))
		{
			pBot->LastSafeLocation = AITAC_GetTeamStartingLocation(BotTeam);
		}

		if (!TrackedEnemyRef->bHasLOS)
		{
			if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GRENADE) || (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL) && UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0))
			{
				Vector GrenadeTarget = UTIL_GetGrenadeThrowTarget(pBot->Edict, LastEnemySeenLocation, BALANCE_VAR(kGrenadeRadius), true);

				if (!vIsZero(GrenadeTarget))
				{
					BotThrowGrenadeAtTarget(pBot, GrenadeTarget);
					return true;
				}
			}

			if ((IdealAttackWeapon != DesiredCombatWeapon || bCanReloadCurrentWeapon) && gpGlobals->time - TrackedEnemyRef->LastSeenTime > 3.0f)
			{
				BotReloadWeapons(pBot);
				if (vDist2DSq(pBot->Edict->v.origin, TrackedEnemyRef->LastVisibleLocation) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					MoveTo(pBot, AITAC_GetTeamStartingLocation(BotTeam), MOVESTYLE_NORMAL);
				}
				return true;
			}

			MoveTo(pBot, TrackedEnemyRef->LastSeenLocation, MOVESTYLE_NORMAL);

			return true;
		}

		BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, CurrentEnemy);

		if (bMustReloadCurrentWeapon)
		{
			MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
			BotReloadWeapons(pBot);
			return true;
		}

		if (DistToEnemy > sqrf(DesiredDistance))
		{
			if (IdealAttackWeapon != DesiredCombatWeapon)
			{
				BotReloadWeapons(pBot);
				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
				return true;
			}

			MoveTo(pBot, LastEnemySeenLocation, MOVESTYLE_NORMAL);

		}
		else
		{

			if (bEnemyIsRanged)
			{
				Vector EnemyOrientation = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->Edict->v.origin);

				Vector RightDir = UTIL_GetCrossProduct(EnemyOrientation, UP_VECTOR);

				pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(RightDir) : UTIL_GetVectorNormal2D(-RightDir);

				// Let's get ziggy with it
				if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
				{
					pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
					pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
				}

				BotMovementInputs(pBot);
			}
			else
			{

				float MinDesiredDist = GetMinIdealWeaponRange(DesiredCombatWeapon);
				Vector Orientation = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->Edict->v.origin);

				float EnemyMoveDot = UTIL_GetDotProduct2D(UTIL_GetVectorNormal2D(CurrentEnemy->v.velocity), -Orientation);

				// Enemy is too close for comfort, or is moving towards us. Back up
				if (DistToEnemy < MinDesiredDist || EnemyMoveDot > 0.7f)
				{
					Vector RetreatLocation = pBot->CurrentFloorPosition - (Orientation * 50.0f);

					if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, RetreatLocation))
					{
						MoveDirectlyTo(pBot, RetreatLocation);
					}

					if (DesiredCombatWeapon != WEAPON_MARINE_KNIFE)
					{
						if (DistToEnemy < sqrf(100.0f))
						{
							if (IsPlayerReloading(pBot->Player) && CanInterruptWeaponReload(GetPlayerCurrentWeapon(pBot->Player)) && GetPlayerCurrentWeaponClipAmmo(pBot->Player) > 0)
							{
								InterruptReload(pBot);
								return true;
							}
							BotJump(pBot);
						}
					}

				}
				else
				{
					MoveTo(pBot, TrackedEnemyRef->LastSeenLocation, MOVESTYLE_NORMAL);
				}
			}

			BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
		}
	}

	return false;
}


bool MarineCombatThink(AvHAIPlayer* pBot)
{
	if (pBot->CurrentEnemy > -1)
	{
		edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

		pBot->CurrentCombatStrategy = GetBotCombatStrategyForTarget(pBot, &pBot->TrackedEnemies[pBot->CurrentEnemy]);

		if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_IGNORE) { return false; }

		pBot->LastCombatTime = gpGlobals->time;

		return RegularMarineCombatThink(pBot);
	}

	return false;
}

void AIPlayerSetPrimaryMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	switch (pBot->BotRole)
	{
	case BOT_ROLE_SWEEPER:
		AIPlayerSetMarineSweeperPrimaryTask(pBot, Task);
		return;
	case BOT_ROLE_FIND_RESOURCES:
		AIPlayerSetMarineCapperPrimaryTask(pBot, Task);
		return;
	case BOT_ROLE_ASSAULT:
		AIPlayerSetMarineAssaultPrimaryTask(pBot, Task);
		return;
	case BOT_ROLE_BOMBARDIER:
		AIPlayerSetMarineBombardierPrimaryTask(pBot, Task);
		return;
	default:
		return;
	}

}

void AIPlayerSetMarineSweeperPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	Vector CommChairLocation = AITAC_GetCommChairLocation(BotTeam);

	// Always built IPs first, so we don't end up getting wiped right at the start

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
	StructureFilter.DeployableTeam = BotTeam;
	StructureFilter.ReachabilityTeam = BotTeam;
	StructureFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	StructureFilter.ExcludeStatusFlags = (STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_COMPLETED);

	AvHAIBuildableStructure* UnbuiltIP = AITAC_FindClosestDeployableToLocation(CommChairLocation, &StructureFilter);

	if (UnbuiltIP)
	{
		AITASK_SetBuildTask(pBot, Task, UnbuiltIP->edict, true);
		return;
	}

	StructureFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	AvHAIBuildableStructure* UnbuiltStructure = AITAC_FindClosestDeployableToLocation(CommChairLocation, &StructureFilter);

	if (UnbuiltStructure)
	{
		AITASK_SetBuildTask(pBot, Task, UnbuiltStructure->edict, true);
		return;
	}

	DeployableSearchFilter AttackedStructureFilter;
	AttackedStructureFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
	AttackedStructureFilter.DeployableTeam = BotTeam;
	AttackedStructureFilter.ReachabilityTeam = BotTeam;
	AttackedStructureFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	AttackedStructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_UNDERATTACK;
	AttackedStructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	AttackedStructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
	AttackedStructureFilter.bConsiderPhaseDistance = true;

	AvHAIBuildableStructure* AttackedStructure = AITAC_FindClosestDeployableToLocation(CommChairLocation, &AttackedStructureFilter);

	if (AttackedStructure)
	{
		AITASK_SetDefendTask(pBot, Task, AttackedStructure->edict, true);
		return;
	}

	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
	{
		AttackedStructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_DAMAGED;

		AvHAIBuildableStructure* AttackedStructure = AITAC_FindClosestDeployableToLocation(CommChairLocation, &AttackedStructureFilter);

		if (AttackedStructure)
		{
			AITASK_SetWeldTask(pBot, Task, AttackedStructure->edict, true);
			return;
		}
	}


	StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
	StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	if (AITAC_GetNumDeployablesNearLocation(CommChairLocation, &StructureFilter) < 2)
	{
		Task->TaskType = TASK_GUARD;
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, CommChairLocation, UTIL_MetresToGoldSrcUnits(10.0f));
		Task->bTaskIsUrgent = false;
		Task->TaskLength = frandrange(20.0f, 30.0f);
		return;
	}

	AvHAIBuildableStructure* NearestPG = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &StructureFilter);

	vector<AvHAIBuildableStructure*> AllPG = AITAC_FindAllDeployables(pBot->Edict->v.origin, &StructureFilter);

	AvHAIBuildableStructure* RandomPG = nullptr;
	int HighestRand = 0;

	for (auto it = AllPG.begin(); it != AllPG.end(); it++)
	{
		AvHAIBuildableStructure* ThisStruct = (*it);

		if (ThisStruct == NearestPG) { continue; }

		int ThisRand = irandrange(0, 100);

		if (!RandomPG || ThisRand > HighestRand)
		{
			RandomPG = ThisStruct;
			HighestRand = ThisRand;
		}
	}

	if (RandomPG)
	{
		Task->TaskType = TASK_GUARD;
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, RandomPG->Location, UTIL_MetresToGoldSrcUnits(5.0f));
		Task->bTaskIsUrgent = false;
		Task->TaskLength = frandrange(20.0f, 30.0f);
		return;
	}

}

void AIPlayerSetMarineCapperPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	DeployableSearchFilter NodeFilter;
	NodeFilter.DeployableTeam = TEAM_IND;
	NodeFilter.ReachabilityTeam = pBot->Player->GetTeam();
	NodeFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

	AvHAIResourceNode* NearestNode = nullptr;
	float MinDist = 0.0f;

	vector<AvHAIResourceNode*> UnclaimedResourceNodes = AITAC_GetAllMatchingResourceNodes(pBot->Edict->v.origin, &NodeFilter);
	
	for (auto it = UnclaimedResourceNodes.begin(); it != UnclaimedResourceNodes.end(); it++)
	{
		AvHAIResourceNode* ResNode = (*it);
		int NumCappers = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), ResNode->Location, UTIL_MetresToGoldSrcUnits(4.0), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		// Only want one capper to grab an empty one
		if (NumCappers == 0)
		{
			float ThisDist = vDist2DSq(pBot->Edict->v.origin, ResNode->Location);

			if (!NearestNode || ThisDist < MinDist)
			{
				NearestNode = ResNode;
				MinDist = ThisDist;
			}
		}
	}

	if (NearestNode)
	{
		AITASK_SetCapResNodeTask(pBot, Task, NearestNode, false);
		return;
	}

	MinDist = 0.0f;

	NodeFilter.DeployableTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());

	vector<AvHAIResourceNode*> EnemyResourceNodes = AITAC_GetAllMatchingResourceNodes(pBot->Edict->v.origin, &NodeFilter);

	for (auto it = EnemyResourceNodes.begin(); it != EnemyResourceNodes.end(); it++)
	{
		AvHAIResourceNode* ResNode = (*it);
		int NumCappers = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), ResNode->Location, UTIL_MetresToGoldSrcUnits(4.0), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		// Allow for 2 cappers to attack an enemy resource node
		if (NumCappers < 2)
		{
			float ThisDist = vDist2DSq(pBot->Edict->v.origin, ResNode->Location);

			if (!NearestNode || ThisDist < MinDist)
			{
				NearestNode = ResNode;
				MinDist = ThisDist;
			}
		}
	}

	if (NearestNode)
	{
		AITASK_SetCapResNodeTask(pBot, Task, NearestNode, false);
		return;
	}

	// No res nodes to cap, go do assault stuff
	AIPlayerSetMarineAssaultPrimaryTask(pBot, Task);
}

void AIPlayerSetMarineAssaultPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	// Go attack sieged hive
	const AvHAIHiveDefinition* ActiveSiegeHive = AITAC_GetNearestHiveUnderActiveSiege(pBot->Player->GetTeam(), pBot->Edict->v.origin);

	if (ActiveSiegeHive)
	{
		AITASK_SetAttackTask(pBot, Task, ActiveSiegeHive->HiveEntity->edict(), false);
		return;
	}

	// Go to empty hive without other marines in it

	vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllHives();

	AvHAIHiveDefinition* NearestEmptyHive = nullptr;
	float MinDist = 0.0f;

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		AvHAIHiveDefinition* ThisHive = (*it);
		if (ThisHive->Status != HIVE_STATUS_UNBUILT) { continue; }

		int NumMarinesSecuring = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), ThisHive->Location, UTIL_MetresToGoldSrcUnits(15.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		if (NumMarinesSecuring < 2)
		{
			float ThisDist = vDist2DSq(ThisHive->Location, pBot->Edict->v.origin);

			if (!NearestEmptyHive || ThisDist < MinDist)
			{
				NearestEmptyHive = ThisHive;
				MinDist = ThisDist;
			}
		}
	}

	if (NearestEmptyHive)
	{
		Vector ActualMoveLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, NearestEmptyHive->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vIsZero(ActualMoveLocation))
		{
			AITASK_SetSecureHiveTask(pBot, Task, NearestEmptyHive->HiveEntity->edict(), NearestEmptyHive->FloorLocation, false);
			return;
		}
	}

	// Go to a good siege location if phase gates available

	if (AITAC_PhaseGatesAvailable(pBot->Player->GetTeam()))
	{
		const AvHAIHiveDefinition* ActiveHive = AITAC_GetActiveHiveNearestLocation(AIMGR_GetEnemyTeam(pBot->Player->GetTeam()), pBot->Edict->v.origin);

		if (ActiveHive)
		{
			if (Task->TaskType != TASK_MOVE)
			{
				AITASK_SetMoveTask(pBot, Task, UTIL_GetRandomPointOnNavmeshInDonut(pBot->BotNavInfo.NavProfile, ActiveHive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f), UTIL_MetresToGoldSrcUnits(20.0f)), false);
			}

			return;
			
		}
	}
	
}

void AIPlayerSetMarineBombardierPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	// Go attack sieged hive

	// Go clear res nodes


}

void AIPlayerSetWantsAndNeedsMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (Task->TaskType == TASK_RESUPPLY || Task->TaskType == TASK_GET_HEALTH || Task->TaskType == TASK_GET_AMMO) { return; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	bool bNeedsHealth = pBot->Edict->v.health < (pBot->Edict->v.max_health * 0.9f);
	bool bNeedsAmmo = UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < (UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player) * 0.9f);

	if (bNeedsHealth || bNeedsAmmo)
	{
		bool bTaskIsUrgent = (pBot->Edict->v.health < (pBot->Edict->v.max_health * 0.7f)) || (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryWeaponMaxClipSize(pBot->Player));

		float SearchRadius = (bTaskIsUrgent) ? UTIL_MetresToGoldSrcUnits(5.0f) : UTIL_MetresToGoldSrcUnits(10.0f);

		AvHAIDroppedItem* NearestHealthPack = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_HEALTHPACK, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, SearchRadius, false);
		AvHAIDroppedItem* NearestAmmoPack = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_AMMO, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, SearchRadius, false);

		if (bNeedsHealth && NearestHealthPack)
		{
			AITASK_SetPickupTask(pBot, Task, NearestHealthPack->edict, bTaskIsUrgent);
			return;
		}

		if (bNeedsAmmo && NearestAmmoPack)
		{
			AITASK_SetPickupTask(pBot, Task, NearestAmmoPack->edict, bTaskIsUrgent);
			return;
		}

		DeployableSearchFilter NearestArmouryFilter;
		NearestArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
		NearestArmouryFilter.DeployableTeam = pBot->Player->GetTeam();
		NearestArmouryFilter.ReachabilityTeam = pBot->Player->GetTeam();
		NearestArmouryFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		NearestArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		NearestArmouryFilter.MaxSearchRadius = (bTaskIsUrgent) ? UTIL_MetresToGoldSrcUnits(20.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

		AvHAIBuildableStructure* NearestArmoury = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &NearestArmouryFilter);

		// We really need some health or ammo, either hit the armoury, or ask for a resupply
		if (NearestArmoury)
		{
			Task->TaskType = TASK_RESUPPLY;
			Task->bTaskIsUrgent = true;
			Task->TaskLocation = NearestArmoury->Location;
			Task->TaskTarget = NearestArmoury->edict;
			return;
		}
		else
		{
			if (bTaskIsUrgent)
			{
				if (bNeedsHealth)
				{
					AIPlayerRequestHealth(pBot);
				}

				if (bNeedsAmmo)
				{
					AIPlayerRequestAmmo(pBot);
				}
			}
		}
	}	

	if (!PlayerHasEquipment(pBot->Edict))
	{
		AvHAIDroppedItem* NearbyHA = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_HEAVYARMOUR, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);

		if (NearbyHA)
		{
			vector<AvHPlayer*> NearbyPlayers = AITAC_GetAllPlayersOfTeamInArea(BotTeam, NearbyHA->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);
			bool bHumanNearby = false;

			for (auto it = NearbyPlayers.begin(); it != NearbyPlayers.end(); it++)
			{
				AvHPlayer* ThisPlayer = (*it);
				edict_t* PlayerEdict = ThisPlayer->edict();

				if (IsPlayerActiveInGame(PlayerEdict) && !PlayerHasEquipment(PlayerEdict) && !IsPlayerBot(PlayerEdict))
				{
					bHumanNearby = true;
					break;
				}
			}

			if (!bHumanNearby)
			{
				AITASK_SetPickupTask(pBot, Task, NearbyHA->edict, vDist2DSq(pBot->Edict->v.origin, NearbyHA->Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));
				return;
			}
		}
	}

	if (PlayerHasEquipment(pBot->Edict))
	{
		if (!PlayerHasSpecialWeapon(pBot->Player))
		{
			AvHAIDroppedItem* NearbyHMG = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_HMG, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);
			AvHAIDroppedItem* NearbyGL = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_GRENADELAUNCHER, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);

			AvHAIDroppedItem* NearbyWeapon = (NearbyHMG != nullptr) ? NearbyHMG : NearbyGL;

			if (NearbyWeapon)
			{
				vector<AvHPlayer*> NearbyPlayers = AITAC_GetAllPlayersOfTeamInArea(BotTeam, NearbyWeapon->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);
				bool bHumanNearby = false;

				for (auto it = NearbyPlayers.begin(); it != NearbyPlayers.end(); it++)
				{
					AvHPlayer* ThisPlayer = (*it);
					edict_t* PlayerEdict = ThisPlayer->edict();

					if (IsPlayerActiveInGame(PlayerEdict) && !PlayerHasSpecialWeapon(ThisPlayer) && !IsPlayerBot(PlayerEdict))
					{
						bHumanNearby = true;
						break;
					}
				}

				if (!bHumanNearby)
				{
					AITASK_SetPickupTask(pBot, Task, NearbyWeapon->edict, vDist2DSq(pBot->Edict->v.origin, NearbyWeapon->Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));
					return;
				}
			}
		}
	}

	if (!PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
	{
		AvHAIDroppedItem* NearbyWeapon = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_WELDER, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);

		if (NearbyWeapon)
		{
			vector<AvHPlayer*> NearbyPlayers = AITAC_GetAllPlayersOfTeamInArea(BotTeam, NearbyWeapon->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);
			bool bHumanNearby = false;

			for (auto it = NearbyPlayers.begin(); it != NearbyPlayers.end(); it++)
			{
				AvHPlayer* ThisPlayer = (*it);
				edict_t* PlayerEdict = ThisPlayer->edict();

				if (IsPlayerActiveInGame(PlayerEdict) && !PlayerHasWeapon(ThisPlayer, WEAPON_MARINE_WELDER) && !IsPlayerBot(PlayerEdict))
				{
					bHumanNearby = true;
					break;
				}
			}

			if (!bHumanNearby)
			{
				AITASK_SetPickupTask(pBot, Task, NearbyWeapon->edict, vDist2DSq(pBot->Edict->v.origin, NearbyWeapon->Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));
				return;
			}
		}
	}

	if (!PlayerHasSpecialWeapon(pBot->Player))
	{
		AvHAIDroppedItem* NearbyWeapon = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_SHOTGUN, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);

		if (NearbyWeapon)
		{
			vector<AvHPlayer*> NearbyPlayers = AITAC_GetAllPlayersOfTeamInArea(BotTeam, NearbyWeapon->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);
			bool bHumanNearby = false;

			for (auto it = NearbyPlayers.begin(); it != NearbyPlayers.end(); it++)
			{
				AvHPlayer* ThisPlayer = (*it);
				edict_t* PlayerEdict = ThisPlayer->edict();

				if (IsPlayerActiveInGame(PlayerEdict) && !PlayerHasSpecialWeapon(ThisPlayer) && !IsPlayerBot(PlayerEdict))
				{
					bHumanNearby = true;
					break;
				}
			}

			if (!bHumanNearby)
			{
				AITASK_SetPickupTask(pBot, Task, NearbyWeapon->edict, vDist2DSq(pBot->Edict->v.origin, NearbyWeapon->Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));
				return;
			}
		}
	}

	if (!PlayerHasWeapon(pBot->Player, WEAPON_MARINE_MINES))
	{
		AvHAIDroppedItem* NearbyWeapon = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_MINES, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);

		if (NearbyWeapon)
		{
			vector<AvHPlayer*> NearbyPlayers = AITAC_GetAllPlayersOfTeamInArea(BotTeam, NearbyWeapon->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);
			bool bHumanNearby = false;

			for (auto it = NearbyPlayers.begin(); it != NearbyPlayers.end(); it++)
			{
				AvHPlayer* ThisPlayer = (*it);
				edict_t* PlayerEdict = ThisPlayer->edict();

				if (IsPlayerActiveInGame(PlayerEdict) && !PlayerHasWeapon(ThisPlayer, WEAPON_MARINE_MINES) && !IsPlayerBot(PlayerEdict))
				{
					bHumanNearby = true;
					break;
				}
			}

			if (!bHumanNearby)
			{
				AITASK_SetPickupTask(pBot, Task, NearbyWeapon->edict, vDist2DSq(pBot->Edict->v.origin, NearbyWeapon->Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));
				return;
			}
		}
	}
}

void AIPlayerRequestHealth(AvHAIPlayer* pBot)
{
	if (gpGlobals->time - pBot->LastRequestTime < min_request_spam_time) { return; }

	pBot->Impulse = SAYING_4;
	pBot->LastRequestTime = gpGlobals->time;
}

void AIPlayerRequestAmmo(AvHAIPlayer* pBot)
{
	if (gpGlobals->time - pBot->LastRequestTime < min_request_spam_time) { return; }

	pBot->Impulse = SAYING_5;
	pBot->LastRequestTime = gpGlobals->time;
}

void AIPlayerRequestOrder(AvHAIPlayer* pBot)
{
	if (gpGlobals->time - pBot->LastRequestTime < min_request_spam_time) { return; }

	pBot->Impulse = SAYING_6;
	pBot->LastRequestTime = gpGlobals->time;
}

void AIPlayerSetSecondaryMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	// If we're building, finish that before doing anything else
	if (Task->TaskType == TASK_BUILD && (Task->StructureType == STRUCTURE_MARINE_INFANTRYPORTAL || vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(3.0f))))
	{
		return;
	}

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	// Find any nearby unbuilt structures
	DeployableSearchFilter UnbuiltFilter;
	UnbuiltFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
	UnbuiltFilter.DeployableTeam = BotTeam;
	UnbuiltFilter.ReachabilityTeam = BotTeam;
	UnbuiltFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	UnbuiltFilter.ExcludeStatusFlags = (STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_COMPLETED);
	UnbuiltFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);

	AvHAIBuildableStructure* UnbuiltIP = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &UnbuiltFilter);

	if (UnbuiltIP)
	{
		float ThisDist = vDist2D(UnbuiltIP->Location, pBot->Edict->v.origin);
		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, UnbuiltIP->Location, ThisDist - 5.0f, false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		if (NumBuilders < 1)
		{
			AITASK_SetBuildTask(pBot, Task, UnbuiltIP->edict, true);
			return;
		}
	}

	UnbuiltFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;

	vector <AvHAIBuildableStructure*> BuildableStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &UnbuiltFilter);

	AvHAIBuildableStructure* NearestStructure = nullptr;
	float MinDist = 0.0f;

	for (auto it = BuildableStructures.begin(); it != BuildableStructures.end(); it++)
	{
		float ThisDist = vDist2D((*it)->Location, pBot->Edict->v.origin);

		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, (*it)->Location, ThisDist - 5.0f, false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		int NumDesiredBuilders = (vDist2DSq((*it)->Location, AITAC_GetCommChairLocation(BotTeam)) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f))) ? 1 : 2;

		if (NumBuilders < NumDesiredBuilders)
		{
			if (!NearestStructure || ThisDist < MinDist)
			{
				NearestStructure = (*it);
				MinDist = ThisDist;
			}
		}
	}

	if (NearestStructure)
	{
		AITASK_SetBuildTask(pBot, Task, NearestStructure->edict, true);
		return;
	}

	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
	{
		bool bWeldIsUrgent = false;
		edict_t* ThingToWeld = nullptr;

		vector<AvHPlayer*> NearbyPlayers = AITAC_GetAllPlayersOfTeamInArea(BotTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);
		AvHPlayer* NearestWeldablePlayer = nullptr;
		AvHPlayer* NearestBadlyDamagedPlayer = nullptr;
		float MinDist = 0.0f;
		float MinBadDist = 0.0f;

		for (auto it = NearbyPlayers.begin(); it != NearbyPlayers.end(); it++)
		{
			AvHPlayer* ThisPlayer = (*it);
			edict_t* PlayerEdict = ThisPlayer->edict();
			
			float ArmourPercent = PlayerEdict->v.armorvalue / (float)GetPlayerMaxArmour(PlayerEdict);

			if (ArmourPercent < 1.0f)
			{
				float ThisDist = vDist2DSq(pBot->Edict->v.origin, PlayerEdict->v.origin);

				if (ArmourPercent < 0.75f)
				{
					if (!NearestBadlyDamagedPlayer || ThisDist < MinBadDist)
					{
						NearestBadlyDamagedPlayer = ThisPlayer;
						MinBadDist = ThisDist;
					}
				}
				else
				{
					if (!NearestWeldablePlayer || ThisDist < MinDist)
					{
						NearestWeldablePlayer = ThisPlayer;
						MinDist = ThisDist;
					}
				}
			}
		}

		// Basically, we won't prioritise welding players over structures unless they're low on armour, otherwise we prefer structures. This avoids
		// situations where the bot constantly keeps topping up nearby players when there are more important weld targets to worry about
		if (NearestBadlyDamagedPlayer)
		{
			AITASK_SetWeldTask(pBot, Task, NearestBadlyDamagedPlayer->edict(), false);
			return;
		}

		DeployableSearchFilter WeldableStructures;
		WeldableStructures.DeployableTypes = SEARCH_ALL_STRUCTURES;
		WeldableStructures.DeployableTeam = BotTeam;
		WeldableStructures.ReachabilityTeam = BotTeam;
		WeldableStructures.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		WeldableStructures.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		WeldableStructures.IncludeStatusFlags = STRUCTURE_STATUS_DAMAGED;
		WeldableStructures.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		WeldableStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
		WeldableStructures.bConsiderPhaseDistance = true;

		AvHAIBuildableStructure* NearestDamagedStructure = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &WeldableStructures);

		if (NearestDamagedStructure)
		{
			bool bIsUrgent = (NearestDamagedStructure->healthPercent < 0.5f);

			AITASK_SetWeldTask(pBot, Task, NearestDamagedStructure->edict, bIsUrgent);
			return;
		}


		if (NearestWeldablePlayer)
		{
			AITASK_SetWeldTask(pBot, Task, NearestWeldablePlayer->edict(), false);
			return;
		}
	}

	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_MINES))
	{
		if (Task->TaskType == TASK_PLACE_MINE) { return; }

		DeployableSearchFilter MineableStructures;
		MineableStructures.DeployableTypes = (STRUCTURE_MARINE_INFANTRYPORTAL | STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
		MineableStructures.DeployableTeam = BotTeam;
		MineableStructures.ReachabilityTeam = BotTeam;
		MineableStructures.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		MineableStructures.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		MineableStructures.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		MineableStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
		MineableStructures.bConsiderPhaseDistance = true;

		vector<AvHAIBuildableStructure*> AllMineableStructures = AITAC_FindAllDeployables(AITAC_GetTeamStartingLocation(BotTeam), &MineableStructures);
		AvHAIBuildableStructure* StructureToMine = nullptr;

		DeployableSearchFilter MineFilter;
		MineFilter.DeployableTypes = STRUCTURE_MARINE_DEPLOYEDMINE;
		MineFilter.DeployableTeam = BotTeam;
		MineFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(3.0f);

		float FarDist = 0.0f;

		for (auto it = AllMineableStructures.begin(); it != AllMineableStructures.end(); it++)
		{
			AvHAIBuildableStructure* ThisStructure = (*it);

			int NumMines = AITAC_GetNumDeployablesNearLocation(ThisStructure->Location, &MineFilter);

			if (NumMines < 4)
			{
				float ThisDist = AITAC_GetPhaseDistanceBetweenPoints(ThisStructure->Location, AITAC_GetTeamStartingLocation(BotTeam));

				if (!StructureToMine || ThisDist > FarDist)
				{
					StructureToMine = ThisStructure;
					FarDist = ThisDist;
				}
			}
		}
		
		if (StructureToMine)
		{
			AITASK_SetMineStructureTask(pBot, Task, StructureToMine->edict, true);
		}
	}

}

bool AIPlayerMustFinishCurrentTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || Task->TaskType == TASK_NONE) { return false; }

	if (!AITASK_IsTaskStillValid(pBot, Task)) { return false; }

	if (IsPlayerGorge(pBot->Edict))
	{
		AvHTeamNumber BotTeam = pBot->Player->GetTeam();

		if (pBot->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING) { return true; }

		// If we're already capping a node, are at the node and there is an unfinished tower on there, then finish the job and don't move on yet
		if (Task->TaskType == TASK_CAP_RESNODE)
		{
			const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

			if (ResNodeIndex && ResNodeIndex->OwningTeam == BotTeam)
			{
				if (!FNullEnt(ResNodeIndex->ActiveTowerEntity) && !UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void AIPlayerNSAlienThink(AvHAIPlayer* pBot)
{
	if (!pBot->CurrentTask) { pBot->CurrentTask = &pBot->PrimaryBotTask; }

	if (pBot->CurrentEnemy > -1)
	{
		if (AlienCombatThink(pBot)) { return; }
	}
	
	if (AIPlayerMustFinishCurrentTask(pBot, pBot->CurrentTask))
	{
		BotProgressTask(pBot, pBot->CurrentTask);
		return;
	}

	UpdateAIAlienPlayerNSRole(pBot);

	AvHAIHiveDefinition* HiveToBuild = nullptr;

	if (AITAC_ShouldBotBuildHive(pBot, &HiveToBuild))
	{
		BotAlienBuildHive(pBot, HiveToBuild);
		return;
	}	

	if (gpGlobals->time >= pBot->BotNextTaskEvaluationTime)
	{
		pBot->BotNextTaskEvaluationTime = gpGlobals->time + frandrange(0.2f, 0.5f);

		AITASK_BotUpdateAndClearTasks(pBot);

		AIPlayerSetPrimaryAlienTask(pBot, &pBot->PrimaryBotTask);
		AIPlayerSetSecondaryAlienTask(pBot, &pBot->SecondaryBotTask);
	}

	pBot->CurrentTask = AIPlayerGetNextTask(pBot);

	if (pBot->LastCombatTime > 5.0f)
	{
		if (!PlayerHasAlienUpgradeOfType(pBot->Edict, HIVE_TECH_DEFENCE) && AITAC_IsAlienUpgradeAvailableForTeam(pBot->Player->GetTeam(), HIVE_TECH_DEFENCE))
		{
			BotEvolveUpgrade(pBot, pBot->Edict->v.origin, AlienGetDesiredUpgrade(pBot, HIVE_TECH_DEFENCE));
			return;
		}

		if (!PlayerHasAlienUpgradeOfType(pBot->Edict, HIVE_TECH_MOVEMENT) && AITAC_IsAlienUpgradeAvailableForTeam(pBot->Player->GetTeam(), HIVE_TECH_MOVEMENT))
		{
			BotEvolveUpgrade(pBot, pBot->Edict->v.origin, AlienGetDesiredUpgrade(pBot, HIVE_TECH_MOVEMENT));
			return;
		}

		if (!PlayerHasAlienUpgradeOfType(pBot->Edict, HIVE_TECH_SENSORY) && AITAC_IsAlienUpgradeAvailableForTeam(pBot->Player->GetTeam(), HIVE_TECH_SENSORY))
		{
			BotEvolveUpgrade(pBot, pBot->Edict->v.origin, AlienGetDesiredUpgrade(pBot, HIVE_TECH_SENSORY));
			return;
		}
	}

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}

	if (pBot->DesiredCombatWeapon == WEAPON_NONE)
	{
		pBot->DesiredCombatWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);
	}
}

AvHMessageID AlienGetDesiredUpgrade(AvHAIPlayer* pBot, HiveTechStatus DesiredTech)
{
	if (DesiredTech == HIVE_TECH_DEFENCE)
	{
		return ALIEN_EVOLUTION_ONE;
	}

	if (DesiredTech == HIVE_TECH_MOVEMENT)
	{
		return ALIEN_EVOLUTION_SEVEN;
	}

	if (DesiredTech == HIVE_TECH_SENSORY)
	{
		return ALIEN_EVOLUTION_ELEVEN;
	}

	return MESSAGE_NULL;
}

void AIPlayerCOThink(AvHAIPlayer* pBot)
{

}

void AIPlayerDMThink(AvHAIPlayer* pBot)
{

}

void AIPlayerThink(AvHAIPlayer* pBot)
{
	if (pBot == AIMGR_GetDebugAIPlayer())
	{
		bool bBreak = true;

		AIDEBUG_DrawBotPath(pBot);
	}

	switch (GetGameRules()->GetMapMode())
	{
		case MAP_MODE_NS:
			AIPlayerNSThink(pBot);
			break;
		case MAP_MODE_CO:
			AIPlayerCOThink(pBot);
			break;
		default:
			AIPlayerDMThink(pBot);
			break;
	}
}

void TestNavThink(AvHAIPlayer* pBot)
{
	AITASK_BotUpdateAndClearTasks(pBot);

	pBot->CurrentTask = &pBot->PrimaryBotTask;

	if (pBot->PrimaryBotTask.TaskType == TASK_MOVE)
	{
		if (vDist2DSq(pBot->Edict->v.origin, pBot->PrimaryBotTask.TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
			AITASK_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			return;
		}

		BotProgressTask(pBot, &pBot->PrimaryBotTask);
	}
	else
	{
		AvHAIResourceNode* RandomNode = AITAC_GetRandomResourceNode((AvHTeamNumber)pBot->Edict->v.team, pBot->BotNavInfo.NavProfile.ReachabilityFlag);

		if (!RandomNode) { return; }

		Vector RandomPoint = RandomNode->Location;

		if (RandomPoint != ZERO_VECTOR && UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, RandomPoint, max_player_use_reach))
		{
			AITASK_SetMoveTask(pBot, &pBot->PrimaryBotTask, RandomPoint, true);
		}
		else
		{
			AITASK_ClearBotTask(pBot, &pBot->PrimaryBotTask);
		}
	}
}

void BotSwitchToWeapon(AvHAIPlayer* pBot, AvHAIWeapon NewWeaponSlot)
{
	char* WeaponName = UTIL_WeaponTypeToClassname(NewWeaponSlot);
	pBot->Player->SwitchWeapon(WeaponName);
}

bool ShouldBotThink(AvHAIPlayer* pBot)
{
	return GetGameRules()->GetGameStarted() && (IsPlayerActiveInGame(pBot->Edict) || IsPlayerCommander(pBot->Edict)) && !IsPlayerGestating(pBot->Edict);
}

void BotResumePlay(AvHAIPlayer* pBot)
{
	ClearBotMovement(pBot);
	SetBaseNavProfile(pBot);

	pBot->bIsInactive = false;
}

void UpdateCommanderOrders(AvHAIPlayer* pBot)
{
	OrderListType ActiveOrders = pBot->Player->GetActiveOrders();

	for (auto it = ActiveOrders.begin(); it != ActiveOrders.end(); it++)
	{
		if (it->GetOrderActive() && it->GetReceiver() && ENTINDEX(pBot->Edict) == it->GetReceiver())
		{
			Vector OrderLocation = g_vecZero;
			it->GetLocation(OrderLocation);

			switch (it->GetOrderType())
			{
				case ORDERTYPEL_MOVE:
					AIPlayerReceiveMoveOrder(pBot, OrderLocation);
					break;
				case ORDERTYPET_BUILD:
					AIPlayerReceiveBuildOrder(pBot, INDEXENT(it->GetTargetIndex()));
					break;
				default:
					break;
			}
		}
	}
}

void AIPlayerReceiveBuildOrder(AvHAIPlayer* pBot, edict_t* BuildTarget)
{
	AITASK_SetBuildTask(pBot, &pBot->CommanderTask, BuildTarget, true);
}

void AIPlayerReceiveMoveOrder(AvHAIPlayer* pBot, Vector Destination)
{
	Vector NavMoveLocation = AdjustPointForPathfinding(Destination);

	Vector ActualMoveLocation = FindClosestNavigablePointToDestination(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, NavMoveLocation, UTIL_MetresToGoldSrcUnits(5.0f));

	if (vIsZero(ActualMoveLocation)) // Don't try to follow an invalid move order
	{
		return;
	}

	const AvHAIResourceNode* ResNodeRef = AITAC_GetNearestResourceNodeToLocation(Destination);

	// We've been asked to go to a resource node if the movement order is near it
	if (ResNodeRef && vDist2DSq(ResNodeRef->Location, Destination) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		// If this resource node doesn't belong to us, or the tower isn't fully built, interpret the order as a "cap this node" order
		if (ResNodeRef->OwningTeam != pBot->Player->GetTeam() || FNullEnt(ResNodeRef->ActiveTowerEntity) || !UTIL_StructureIsFullyBuilt(ResNodeRef->ActiveTowerEntity))
		{
			AITASK_SetCapResNodeTask(pBot, &pBot->CommanderTask, ResNodeRef, false);
			pBot->CommanderTask.bIssuedByCommander = true;
			return;
		}
	}

	const AvHAIHiveDefinition* HiveRef = AITAC_GetHiveNearestLocation(Destination);

	// Have we been asked to go to an empty hive? If so, then treat the order as a "help secure this hive" command
	if (HiveRef && HiveRef->Status == HIVE_STATUS_UNBUILT && vDist2DSq(HiveRef->Location, Destination) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
	{
		if (!AICOMM_IsHiveFullySecured(pBot, HiveRef, false))
		{
			AITASK_SetSecureHiveTask(pBot, &pBot->CommanderTask, HiveRef->HiveEntity->edict(), ActualMoveLocation, false);
			pBot->CommanderTask.bIssuedByCommander = true;
			return;
		}
	}

	// Otherwise, treat as a normal move order. Go there and wait a bit to see what the commander wants to do next
	AITASK_SetMoveTask(pBot, &pBot->CommanderTask, ActualMoveLocation, true);
	pBot->CommanderTask.bIssuedByCommander = true;
	
}

void BotStopCommanderMode(AvHAIPlayer* pBot)
{
	// Thanks EterniumDev (Alien) for logic to allow commander AI to leave the chair and build structures when needed

	if (IsPlayerCommander(pBot->Edict))
	{
		pBot->Player->SetUser3(AVH_USER3_MARINE_PLAYER);

		// Cheesy way to make sure player class change is sent to everyone
		pBot->Player->EffectivePlayerClassChanged();
	}
}

void AIPlayerSetPrimaryAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	switch (pBot->BotRole)
	{
	case BOT_ROLE_BUILDER:
		AIPlayerSetAlienBuilderPrimaryTask(pBot, Task);
		return;
	case BOT_ROLE_FIND_RESOURCES:
		AIPlayerSetAlienCapperPrimaryTask(pBot, Task);
		return;
	case BOT_ROLE_ASSAULT:
		AIPlayerSetAlienAssaultPrimaryTask(pBot, Task);
		return;
	case BOT_ROLE_HARASS:
		AIPlayerSetAlienHarasserPrimaryTask(pBot, Task);
		return;
	default:
		return;
	}
}

void AIPlayerSetAlienBuilderPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	// Do we have any missing upgrade chambers (should have 3 of each if we can build them)
	AvHAIDeployableStructureType MissingStructure = AITAC_GetNextMissingUpgradeChamberForTeam(BotTeam);

	// If we do have a missing upgrade chamber, built it at the nearest hive or resource node that we own, whichever is nearest
	if (MissingStructure != STRUCTURE_NONE)
	{
		if (Task->TaskType == TASK_BUILD && Task->StructureType == MissingStructure) { return; }

		vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllHives();
		
		DeployableSearchFilter ResNodeFilter;
		ResNodeFilter.DeployableTeam = BotTeam;
		ResNodeFilter.ReachabilityTeam = BotTeam;
		ResNodeFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

		AvHAIResourceNode* NearestNode = AITAC_FindNearestResourceNodeToLocation(pBot->Edict->v.origin, &ResNodeFilter);

		Vector BuildOrigin = ZERO_VECTOR;

		float MinDist = 0.0f;

		for (auto it = AllHives.begin(); it != AllHives.end(); it++)
		{
			AvHAIHiveDefinition* ThisHive = (*it);

			if (ThisHive->OwningTeam != BotTeam) { continue; }

			float ThisDist = vDist2DSq(pBot->Edict->v.origin, ThisHive->FloorLocation);

			if (vIsZero(BuildOrigin) || ThisDist < MinDist)
			{
				BuildOrigin = ThisHive->FloorLocation;
				MinDist = ThisDist;
			}
		}

		if (NearestNode)
		{
			float ThisDist = vDist2DSq(pBot->Edict->v.origin, NearestNode->Location);

			if (vIsZero(BuildOrigin) || ThisDist < MinDist)
			{
				BuildOrigin = NearestNode->Location;
			}
		}

		if (vIsZero(BuildOrigin))
		{
			BuildOrigin = pBot->CurrentFloorPosition;
		}

		if (Task->TaskType == TASK_BUILD && vDist2DSq(Task->TaskLocation, BuildOrigin) <= UTIL_MetresToGoldSrcUnits(5.0f))
		{
			Task->StructureType = MissingStructure;
			return;
		}

		Vector ActualBuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE), BuildOrigin, UTIL_MetresToGoldSrcUnits(3.0f));

		if (vIsZero(ActualBuildLocation))
		{
			ActualBuildLocation = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(GetBaseNavProfile(GORGE_BASE_NAV_PROFILE), BuildOrigin, UTIL_MetresToGoldSrcUnits(3.0f));
		}

		AITASK_SetBuildTask(pBot, Task, MissingStructure, ActualBuildLocation, false);
		return;
	}

	// No missing upgrade chambers to drop, let's look for empty hives we can start staking a claim to, to deny to the enemy
	vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllHives();

	AvHAIHiveDefinition* HiveToSecure = nullptr;

	float MaxDist = 0.0f;

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		AvHAIHiveDefinition* ThisHive = (*it);

		if (ThisHive->Status == HIVE_STATUS_UNBUILT)
		{
			unsigned int StructureTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);

			if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
			{
				StructureTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;
			}

			DeployableSearchFilter EnemyStructureFilter;
			EnemyStructureFilter.DeployableTeam = EnemyTeam;
			EnemyStructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			EnemyStructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			EnemyStructureFilter.DeployableTypes = StructureTypes;
			EnemyStructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

			bool bEnemyHaveFoothold = AITAC_DeployableExistsAtLocation(ThisHive->FloorLocation, &EnemyStructureFilter);

			if (bEnemyHaveFoothold) { continue; }

			if (AITAC_GetNumPlayersOfTeamInArea(EnemyTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_COMMANDER_PLAYER) > 1) { continue; }

			int OtherBuilders = AITAC_GetNumPlayersOfTeamAndClassInArea(EnemyTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f), false, nullptr, AVH_USER3_ALIEN_PLAYER2);

			if (OtherBuilders >= 2) { continue; }

			DeployableSearchFilter ExistingReinforcementFilter;
			ExistingReinforcementFilter.DeployableTeam = BotTeam;
			ExistingReinforcementFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
			ExistingReinforcementFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;

			vector<AvHAIBuildableStructure*> AllReinforcingStructures = AITAC_FindAllDeployables(ThisHive->FloorLocation, &ExistingReinforcementFilter);

			int NumOCs = 0;
			int NumDCs = 0;
			int NumMCs = 0;
			int NumSCs = 0;

			for (auto it = AllReinforcingStructures.begin(); it != AllReinforcingStructures.end(); it++)
			{
				switch ((*it)->StructureType)
				{
				case STRUCTURE_ALIEN_OFFENCECHAMBER:
					NumOCs++;
					break;
				case STRUCTURE_ALIEN_DEFENCECHAMBER:
					NumDCs++;
					break;
				case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
					NumMCs++;
					break;
				case STRUCTURE_ALIEN_SENSORYCHAMBER:
					NumSCs++;
					break;
				default:
					break;
				}
			}

			if (NumOCs < 3
				|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_DEFENSE_CHAMBER) && NumDCs < 2)
				|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_MOVEMENT_CHAMBER) && NumMCs < 1)
				|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_SENSORY_CHAMBER) && NumSCs < 1))
			{
				float ThisDist = vDist2DSq(AITAC_GetTeamStartingLocation(EnemyTeam), ThisHive->FloorLocation);

				if (ThisDist > MaxDist)
				{
					HiveToSecure = ThisHive;
					MaxDist = ThisDist;
				}
			}
			
		}
	}

	if (HiveToSecure)
	{
		AITASK_SetReinforceStructureTask(pBot, Task, HiveToSecure->HiveEntity->edict(), false);
		return;
	}

	DeployableSearchFilter ResNodeFilter;
	ResNodeFilter.DeployableTeam = BotTeam;
	ResNodeFilter.DeployableTypes = STRUCTURE_ALIEN_RESTOWER;
	ResNodeFilter.ReachabilityTeam = BotTeam;
	ResNodeFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

	vector<AvHAIBuildableStructure*> AllMatchingTowers = AITAC_FindAllDeployables(pBot->Edict->v.origin, &ResNodeFilter);

	edict_t* TowerToReinforce = nullptr;
	float MinDist = 0.0f;

	for (auto it = AllMatchingTowers.begin(); it != AllMatchingTowers.end(); it++)
	{
		AvHAIBuildableStructure* ThisResTower = (*it);

		DeployableSearchFilter ExistingReinforcementFilter;
		ExistingReinforcementFilter.DeployableTeam = BotTeam;
		ExistingReinforcementFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);
		ExistingReinforcementFilter.DeployableTypes = SEARCH_ALL_ALIEN_STRUCTURES;

		vector<AvHAIBuildableStructure*> AllReinforcingStructures = AITAC_FindAllDeployables(ThisResTower->Location, &ExistingReinforcementFilter);

		int NumOCs = 0;
		int NumDCs = 0;
		int NumMCs = 0;
		int NumSCs = 0;

		for (auto it = AllReinforcingStructures.begin(); it != AllReinforcingStructures.end(); it++)
		{
			switch ((*it)->StructureType)
			{
			case STRUCTURE_ALIEN_OFFENCECHAMBER:
				NumOCs++;
				break;
			case STRUCTURE_ALIEN_DEFENCECHAMBER:
				NumDCs++;
				break;
			case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
				NumMCs++;
				break;
			case STRUCTURE_ALIEN_SENSORYCHAMBER:
				NumSCs++;
				break;
			default:
				break;
			}
		}

		if (NumOCs < 3
			|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_DEFENSE_CHAMBER) && NumDCs < 2)
			|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_MOVEMENT_CHAMBER) && NumMCs < 1)
			|| (AITAC_TeamHiveWithTechExists(BotTeam, ALIEN_BUILD_SENSORY_CHAMBER) && NumSCs < 1))
		{
			float ThisDist = vDist2DSq(AITAC_GetTeamStartingLocation(EnemyTeam), ThisResTower->Location);

			if (!TowerToReinforce || ThisDist < MinDist)
			{
				TowerToReinforce = ThisResTower->edict;
				MaxDist = ThisDist;
			}
		}
	}

	if (!FNullEnt(TowerToReinforce))
	{
		AITASK_SetReinforceStructureTask(pBot, Task, TowerToReinforce, false);
		return;
	}
}

void AIPlayerSetAlienCapperPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	AvHAIResourceNode* NodeToCap = nullptr;

	// If we're already capping a node, are at the node and there is an unfinished tower on there, then finish the job and don't move on yet
	if (Task->TaskType == TASK_CAP_RESNODE)
	{
		const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

		if (ResNodeIndex && ResNodeIndex->OwningTeam == BotTeam)
		{
			if (!FNullEnt(ResNodeIndex->ActiveTowerEntity) && !UTIL_StructureIsFullyBuilt(ResNodeIndex->ActiveTowerEntity))
			{
				if (vDist2DSq(pBot->Edict->v.origin, ResNodeIndex->Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					return;
				}
			}
		}
	}

	float ResourcesRequired = BALANCE_VAR(kResourceTowerCost);

	if (!IsPlayerGorge(pBot->Edict))
	{
		ResourcesRequired += BALANCE_VAR(kGorgeCost);
	}

	bool bCanPlaceTower = pBot->Player->GetResources() >= (BALANCE_VAR(kResourceTowerCost) * 0.8f);

	if (IsPlayerLerk(pBot->Edict) || IsPlayerFade(pBot->Edict) || IsPlayerOnos(pBot->Edict))
	{
		bCanPlaceTower = pBot->Player->GetResources() >= 75 && AITAC_GetTeamResNodeOwnership(BotTeam, true) >= 0.5f;
	}

	bool bCanAttackTowers = (!IsPlayerGorge(pBot->Edict) || PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB));

	// If we have enough resources to cap a node, then find an empty one we can slap one down in
	if (bCanPlaceTower || !bCanAttackTowers)
	{
		DeployableSearchFilter EmptyNodeFilter;
		EmptyNodeFilter.DeployableTeam = TEAM_IND;
		EmptyNodeFilter.ReachabilityTeam = BotTeam;
		EmptyNodeFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

		vector<AvHAIResourceNode*> EligibleNodes = AITAC_GetAllMatchingResourceNodes(pBot->Edict->v.origin, &EmptyNodeFilter);

		float MaxDist = 0.0f;

		for (auto it = EligibleNodes.begin(); it != EligibleNodes.end(); it++)
		{
			AvHAIResourceNode* ThisNode = (*it);

			if (ThisNode->bIsBaseNode)
			{
				if (FNullEnt(ThisNode->ParentHive)) { continue; } // This node must belong to marine base, don't try to cap it
			}

			if (!FNullEnt(ThisNode->ParentHive))
			{
				AvHAIHiveDefinition* ParentHiveRef = AITAC_GetHiveFromEdict(ThisNode->ParentHive);

				// Don't try to cap resource nodes in an enemy hive.
				if (ParentHiveRef->OwningTeam == EnemyTeam) { continue; }

				DeployableSearchFilter EnemyStructuresFilter;
				EnemyStructuresFilter.DeployableTeam = EnemyTeam;
				EnemyStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
				EnemyStructuresFilter.ExcludeStatusFlags = (IsPlayerSkulk(pBot->Edict)) ? (STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_ELECTRIFIED) : STRUCTURE_STATUS_RECYCLING;
				EnemyStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

				if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE)
				{
					EnemyStructuresFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
				}
				else
				{
					EnemyStructuresFilter.DeployableTypes = (STRUCTURE_ALIEN_OFFENCECHAMBER);
				}

				// Enemy has started fortifying the hive we want to build a RT in, don't try to cap it
				if (AITAC_DeployableExistsAtLocation(ThisNode->Location, &EnemyStructuresFilter)) { continue; }
			}

			edict_t* ExistingBuilder = AITAC_GetNearestPlayerOfClassInArea(BotTeam, ThisNode->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

			if (!FNullEnt(ExistingBuilder) && vDist2DSq(ExistingBuilder->v.origin, ThisNode->Location) < vDist2DSq(pBot->Edict->v.origin, ThisNode->Location) && GetPlayerResources(ExistingBuilder) >= (BALANCE_VAR(kResourceTowerCost) * 0.8f)) { continue; }

			vector<AvHAIPlayer*> OtherAITeam = AIMGR_GetAIPlayersOnTeam(BotTeam);
			bool bNodeClaimed = false;

			for (auto BotIt = OtherAITeam.begin(); BotIt != OtherAITeam.end(); BotIt++)
			{
				AvHAIPlayer* OtherBot = (*BotIt);

				if (OtherBot != pBot && OtherBot->PrimaryBotTask.TaskType == TASK_CAP_RESNODE && OtherBot->PrimaryBotTask.TaskTarget == ThisNode->ResourceEdict)
				{
					bNodeClaimed = true;
					break;
				}
			}

			if (bNodeClaimed) { continue; }

			float ThisDist = vDist2DSq(AITAC_GetTeamStartingLocation(EnemyTeam), ThisNode->Location);

			if (ThisDist > MaxDist)
			{
				NodeToCap = ThisNode;
				MaxDist = ThisDist;
			}
		}

		if (NodeToCap)
		{
			AITASK_SetCapResNodeTask(pBot, Task, NodeToCap, false);
			return;
		}
	}

	// Let's find an enemy tower to take out

	DeployableSearchFilter EnemyNodeFilter;
	EnemyNodeFilter.DeployableTeam = AIMGR_GetEnemyTeam(BotTeam);
	EnemyNodeFilter.ReachabilityTeam = BotTeam;
	EnemyNodeFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

	vector<AvHAIResourceNode*> EligibleNodes = AITAC_GetAllMatchingResourceNodes(pBot->Edict->v.origin, &EnemyNodeFilter);

	float MaxDist = 0.0f;

	NodeToCap = nullptr;

	for (auto it = EligibleNodes.begin(); it != EligibleNodes.end(); it++)
	{
		AvHAIResourceNode* ThisNode = (*it);

		if (ThisNode->bIsBaseNode)
		{
			if (FNullEnt(ThisNode->ParentHive)) { continue; } // This node must belong to marine base, leave that tower alone
		}

		if (!FNullEnt(ThisNode->ParentHive))
		{
			AvHAIHiveDefinition* ParentHiveRef = AITAC_GetHiveFromEdict(ThisNode->ParentHive);

			// Don't try to attack RTs inside enemy hives
			if (ParentHiveRef->OwningTeam == EnemyTeam) { continue; }

			// Don't attack an empty hive RT if the enemy has fortified the area
			DeployableSearchFilter EnemyStructuresFilter;
			EnemyStructuresFilter.DeployableTeam = EnemyTeam;
			EnemyStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			EnemyStructuresFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			EnemyStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

			if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE)
			{
				// Don't attack if there are turrets or a phase gate in the area.
				EnemyStructuresFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
			}
			else
			{
				EnemyStructuresFilter.DeployableTypes = (STRUCTURE_ALIEN_OFFENCECHAMBER);
			}

			// Enemy has started fortifying the hive we want to build a RT in, leave that tower alone
			if (AITAC_DeployableExistsAtLocation(ThisNode->Location, &EnemyStructuresFilter)) { continue; }
		}

		float ThisDist = vDist2DSq(ThisNode->Location, AITAC_GetTeamStartingLocation(EnemyTeam));

		if (!NodeToCap || ThisDist > MaxDist)
		{
			NodeToCap = ThisNode;
			MaxDist = ThisDist;
		}

	}

	if (NodeToCap)
	{
		AITASK_SetAttackTask(pBot, Task, NodeToCap->ActiveTowerEntity, false);
		return;
	}

	// If we have nothing to do as a capper, then revert to assault
	AIPlayerSetAlienAssaultPrimaryTask(pBot, Task);

}

void AIPlayerSetAlienAssaultPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	if (IsPlayerGorge(pBot->Edict) && gpGlobals->time - pBot->LastCombatTime > 5.0f)
	{
		BotEvolveLifeform(pBot, pBot->Edict->v.origin, ALIEN_LIFEFORM_ONE);
		return;
	}

	if (!IsPlayerOnos(pBot->Edict) && pBot->Player->GetResources() >= BALANCE_VAR(kOnosCost))
	{
		int NumOnos = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER5, pBot->Edict);

		if (NumOnos < 2)
		{
			const AvHAIHiveDefinition* NearestHive = AITAC_GetNearestTeamHive(BotTeam, pBot->Edict->v.origin, true);

			if (NearestHive)
			{
				AITASK_SetEvolveTask(pBot, Task, NearestHive->HiveEntity->edict(), ALIEN_LIFEFORM_FIVE, true);
				return;
			}
		}
	}

	if (!IsPlayerFade(pBot->Edict) && pBot->Player->GetResources() >= BALANCE_VAR(kFadeCost))
	{
		int NumFades = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER4, pBot->Edict);
		int NumOnos = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER5, pBot->Edict);

		if (NumFades < 2 || NumOnos >= 2)
		{
			const AvHAIHiveDefinition* NearestHive = AITAC_GetNearestTeamHive(BotTeam, pBot->Edict->v.origin, true);

			if (NearestHive)
			{
				AITASK_SetEvolveTask(pBot, Task, NearestHive->HiveEntity->edict(), ALIEN_LIFEFORM_FOUR, true);
				return;
			}

		}
	}

	const AvHAIHiveDefinition* NearestSiegedHive = AITAC_GetNearestHiveUnderActiveSiege(EnemyTeam, pBot->Edict->v.origin);

	if (NearestSiegedHive)
	{
		DeployableSearchFilter EnemyStuffFilter;
		EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
		EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		EnemyStuffFilter.DeployableTeam = EnemyTeam;
		EnemyStuffFilter.ReachabilityTeam = BotTeam;
		EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(25.0f);

		vector<AvHAIBuildableStructure*> AllSiegingStructures = AITAC_FindAllDeployables(NearestSiegedHive->Location, &EnemyStuffFilter);

		AvHAIBuildableStructure* StructureToTarget = nullptr;

		float MinDist = 0.0f;

		for (auto it = AllSiegingStructures.begin(); it != AllSiegingStructures.end(); it++)
		{
			AvHAIBuildableStructure* ThisStructure = (*it);

			// Always go for the phase gate first to prevent reinforcements
			if (ThisStructure->StructureType == STRUCTURE_MARINE_PHASEGATE)
			{
				StructureToTarget = ThisStructure;
				continue;
			}

			// Then go for any turret factories, especially advanced ones to cut off siege turrets
			if (ThisStructure->StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
			{
				StructureToTarget = ThisStructure;
				continue;
			}

			if (ThisStructure->StructureType == STRUCTURE_MARINE_TURRETFACTORY)
			{
				StructureToTarget = ThisStructure;
				continue;
			}

			// Pick up anything else
			float ThisDist = vDist2DSq(ThisStructure->Location, pBot->Edict->v.origin);

			if (!StructureToTarget || ThisDist < MinDist)
			{
				StructureToTarget = ThisStructure;
				MinDist = ThisDist;
			}
		}

		if (StructureToTarget)
		{
			AITASK_SetAttackTask(pBot, Task, StructureToTarget->edict, true);
			return;
		}
	}

	// If we're up against marines, look out for any siege stuff
	if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE)
	{
		vector<AvHAIHiveDefinition*> AllTeamHives = AITAC_GetAllTeamHives(BotTeam, false);

		for (auto it = AllTeamHives.begin(); it != AllTeamHives.end(); it++)
		{
			AvHAIHiveDefinition* ThisHive = (*it);

			DeployableSearchFilter EnemyStuffFilter;
			EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
			EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			EnemyStuffFilter.DeployableTeam = EnemyTeam;
			EnemyStuffFilter.ReachabilityTeam = BotTeam;
			EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
			EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(25.0f);

			vector<AvHAIBuildableStructure*> AllSiegingStructures = AITAC_FindAllDeployables(ThisHive->Location, &EnemyStuffFilter);

			AvHAIBuildableStructure* StructureToTarget = nullptr;

			float MinDist = 0.0f;

			for (auto it = AllSiegingStructures.begin(); it != AllSiegingStructures.end(); it++)
			{
				AvHAIBuildableStructure* ThisStructure = (*it);

				// Always go for the phase gate first to prevent reinforcements
				if (ThisStructure->StructureType == STRUCTURE_MARINE_PHASEGATE)
				{
					StructureToTarget = ThisStructure;
					continue;
				}

				// Then go for any turret factories, especially advanced ones to cut off siege turrets
				if (ThisStructure->StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
				{
					StructureToTarget = ThisStructure;
					continue;
				}

				if (ThisStructure->StructureType == STRUCTURE_MARINE_TURRETFACTORY)
				{
					StructureToTarget = ThisStructure;
					continue;
				}

				// Pick up anything else
				float ThisDist = vDist2DSq(ThisStructure->Location, pBot->Edict->v.origin);

				if (!StructureToTarget || ThisDist < MinDist)
				{
					StructureToTarget = ThisStructure;
					MinDist = ThisDist;
				}
			}

			if (StructureToTarget)
			{
				int NumAttackers = AITAC_GetNumPlayersOfTeamInArea(BotTeam, StructureToTarget->Location, UTIL_MetresToGoldSrcUnits(10.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

				if (NumAttackers < 2)
				{
					AITASK_SetAttackTask(pBot, Task, StructureToTarget->edict, true);
					return;
				}
			}
		}

	}


	Vector EnemyBaseLocation = AITAC_GetTeamStartingLocation(EnemyTeam);

	AvHAIHiveDefinition* HiveToGuard = nullptr;
	AvHAIHiveDefinition* HiveToSecure = nullptr;

	vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllHives();

	float MaxGuardDist = 0.0f;
	float MaxSecureDist = 0.0f;

	bool bEnemyIsMarines = (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE);

	DeployableSearchFilter EnemyStuffFilter;

	EnemyStuffFilter.DeployableTeam = EnemyTeam;
	EnemyStuffFilter.ReachabilityTeam = BotTeam;
	EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	if (bEnemyIsMarines)
	{
		EnemyStuffFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY | STRUCTURE_MARINE_COMMCHAIR);
		EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		if (IsPlayerSkulk(pBot->Edict) || IsPlayerLerk(pBot->Edict))
		{
			EnemyStuffFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_ELECTRIFIED;
		}
	}
	else
	{
		EnemyStuffFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;
	}

	bool bShouldGuardEmptyHive = pBot->Player->GetUser3() < AVH_USER3_ALIEN_PLAYER3;

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		AvHAIHiveDefinition* ThisHive = (*it);

		if (ThisHive->OwningTeam != TEAM_IND) { continue; }

		bool bEnemyIsSecuring = AITAC_DeployableExistsAtLocation(ThisHive->FloorLocation, &EnemyStuffFilter);

		if (bEnemyIsSecuring)
		{
			float ThisDist = vDist2DSq(ThisHive->FloorLocation, EnemyBaseLocation);

			if (ThisDist > MaxSecureDist)
			{
				HiveToSecure = ThisHive;
				MaxSecureDist = ThisDist;
			}
		}
		else
		{
			if (!bShouldGuardEmptyHive) { continue; }

			DeployableSearchFilter FriendlyStuffFilter;

			FriendlyStuffFilter.DeployableTeam = BotTeam;
			FriendlyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
			FriendlyStuffFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;

			if (AITAC_DeployableExistsAtLocation(ThisHive->FloorLocation, &FriendlyStuffFilter)) { continue; }

			if (AITAC_GetNumPlayersOfTeamAndClassInArea(BotTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(20.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2) == 0) { continue; }

			bool bNeedsExtraGuards = true;
			int NumGuards = 0;

			vector<AvHPlayer*> HumanPlayers = AIMGR_GetNonAIPlayersOnTeam(BotTeam);
			vector<AvHAIPlayer*> AITeamPlayers = AIMGR_GetAIPlayersOnTeam(BotTeam);

			for (auto AIIt = AITeamPlayers.begin(); AIIt != AITeamPlayers.end(); AIIt++)
			{
				if ((*AIIt) == pBot) { continue; }

				if ((*AIIt)->PrimaryBotTask.TaskType == TASK_GUARD && (*AIIt)->PrimaryBotTask.TaskTarget == ThisHive->HiveEntity->edict())
				{
					if ((*AIIt)->Player->GetUser3() >= AVH_USER3_ALIEN_PLAYER3) { bNeedsExtraGuards = false; }
					NumGuards++;
				}
			}

			for (auto GuardIt = HumanPlayers.begin(); GuardIt != HumanPlayers.end(); GuardIt++)
			{
				AvHPlayer* ThisGuard = (*GuardIt);

				if (IsPlayerActiveInGame(ThisGuard->edict()) && vDist2DSq(ThisGuard->edict()->v.origin, ThisHive->FloorLocation) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
				{
					if (ThisGuard->GetUser3() >= AVH_USER3_ALIEN_PLAYER3) { bNeedsExtraGuards = false; }
					NumGuards++;
				}				
			}

			bNeedsExtraGuards = bNeedsExtraGuards && NumGuards < 2;

			if (bNeedsExtraGuards)
			{
				float ThisDist = vDist2DSq(ThisHive->FloorLocation, EnemyBaseLocation);

				if (ThisDist > MaxSecureDist)
				{
					HiveToGuard = ThisHive;
					MaxGuardDist = ThisDist;
				}
			}
			else
			{
				// The purpose of this is to ensure we only guard one empty hive at a time, otherwise all the assault bots will be sitting around in empty hives and not pressuring marines
				// If we have an empty hive already being guarded, then this bool will ensure the bot doesn't go guard an empty hive even if there are 2
				bShouldGuardEmptyHive = false;
			}
		}
	}

	if (bShouldGuardEmptyHive && HiveToGuard)
	{
		Task->TaskType = TASK_GUARD;
		Task->TaskLocation = HiveToGuard->FloorLocation;
		Task->TaskTarget = HiveToGuard->HiveEntity->edict();
		return;
	}
	else if (HiveToSecure)
	{
		EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;

		// Don't attack electrified structures as skulk
		if (pBot->Player->GetUser3() < AVH_USER3_ALIEN_PLAYER4)
		{
			EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_ELECTRIFIED;
		}

		vector<AvHAIBuildableStructure*> AllEnemyThings = AITAC_FindAllDeployables(HiveToSecure->FloorLocation, &EnemyStuffFilter);

		AvHAIBuildableStructure* StructureToAttack = nullptr;

		for (auto it = AllEnemyThings.begin(); it != AllEnemyThings.end(); it++)
		{
			AvHAIBuildableStructure* ThisStructure = (*it);

			// First prioritise phase gates or alien OCs
			if (ThisStructure->StructureType == STRUCTURE_MARINE_PHASEGATE || ThisStructure->StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
			{
				if (!StructureToAttack || StructureToAttack->StructureType != ThisStructure->StructureType || vDist2DSq(pBot->Edict->v.origin, ThisStructure->Location) < vDist2DSq(pBot->Edict->v.origin, StructureToAttack->Location))
				{
					StructureToAttack = ThisStructure;
					continue;
				}
			}

			if (StructureToAttack && (StructureToAttack->StructureType == STRUCTURE_MARINE_PHASEGATE || ThisStructure->StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)) { continue; }

			// Then prioritise turret factories
			if (ThisStructure->StructureType == STRUCTURE_MARINE_TURRETFACTORY || ThisStructure->StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
			{
				if (!StructureToAttack || StructureToAttack->StructureType != ThisStructure->StructureType || vDist2DSq(pBot->Edict->v.origin, ThisStructure->Location) < vDist2DSq(pBot->Edict->v.origin, StructureToAttack->Location))
				{
					StructureToAttack = ThisStructure;
					continue;
				}
			}

			if (StructureToAttack && (StructureToAttack->StructureType == STRUCTURE_MARINE_TURRETFACTORY || ThisStructure->StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)) { continue; }

			// Then target any other structures
			if (!StructureToAttack || vDist2DSq(pBot->Edict->v.origin, ThisStructure->Location) < vDist2DSq(pBot->Edict->v.origin, StructureToAttack->Location))
			{
				StructureToAttack = ThisStructure;
			}
		}

		if (StructureToAttack)
		{
			AITASK_SetAttackTask(pBot, Task, StructureToAttack->edict, false);
			return;
		}
	}

	DeployableSearchFilter EnemyInfPortalFilter;
	EnemyInfPortalFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
	EnemyInfPortalFilter.DeployableTeam = EnemyTeam;
	EnemyInfPortalFilter.ReachabilityTeam = BotTeam;
	EnemyInfPortalFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyInfPortalFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	EnemyInfPortalFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	AvHAIBuildableStructure* EnemyInfPortal = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &EnemyInfPortalFilter);

	if (EnemyInfPortal)
	{
		AITASK_SetAttackTask(pBot, Task, EnemyInfPortal->edict, false);
		return;
	}

	// TODO: Attack enemy hive/base
	edict_t* EnemyChair = AITAC_GetCommChair(EnemyTeam);

	if (!FNullEnt(EnemyChair))
	{
		AITASK_SetAttackTask(pBot, Task, EnemyChair, false);
		return;
	}

	vector<AvHPlayer*> AllEnemyPlayers = AIMGR_GetAllPlayersOnTeam(EnemyTeam);
	edict_t* TargetPlayer = nullptr;

	float MinDist = 0.0f;

	for (auto it = AllEnemyPlayers.begin(); it != AllEnemyPlayers.end(); it++)
	{
		AvHPlayer* ThisPlayer = (*it);

		if (!ThisPlayer) { continue; }

		edict_t* PlayerEdict = ThisPlayer->edict();

		if (!IsPlayerActiveInGame(PlayerEdict)) { continue; }

		float ThisDist = vDist2DSq(PlayerEdict->v.origin, pBot->Edict->v.origin);

		if (FNullEnt(TargetPlayer) || ThisDist < MinDist)
		{
			TargetPlayer = PlayerEdict;
			MinDist = ThisDist;
		}
	}

	if (!FNullEnt(TargetPlayer))
	{
		MoveTo(pBot, UTIL_GetEntityGroundLocation(TargetPlayer), MOVESTYLE_NORMAL);
	}

}

void AIPlayerSetAlienHarasserPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	// If we aren't a lerk, go evolve into one. If we can't, then act like a regular assault alien until we can
	if (!IsPlayerLerk(pBot->Edict))
	{
		if (pBot->Player->GetResources() >= BALANCE_VAR(kLerkCost))
		{
			if (Task->TaskType == TASK_EVOLVE && Task->Evolution == ALIEN_LIFEFORM_THREE) { return; }

			vector<AvHAIHiveDefinition*> AllTeamHives = AITAC_GetAllTeamHives(pBot->Player->GetTeam(), false);

			AvHAIHiveDefinition* NearestHive = nullptr;
			float MinDist = 0.0f;

			for (auto it = AllTeamHives.begin(); it != AllTeamHives.end(); it++)
			{
				float ThisDist = vDist2DSq(pBot->Edict->v.origin, (*it)->FloorLocation);

				if (!NearestHive || ThisDist < MinDist)
				{
					NearestHive = (*it);
					MinDist = ThisDist;
				}
			}

			if (NearestHive)
			{
				AITASK_SetEvolveTask(pBot, Task, NearestHive->HiveEntity->edict(), ALIEN_LIFEFORM_THREE, true);
				return;
			}
			else
			{
				AITASK_SetEvolveTask(pBot, Task, pBot->Edict->v.origin, ALIEN_LIFEFORM_THREE, true);
				return;
			}
		}

		if (IsPlayerGorge(pBot->Edict))
		{
			BotEvolveLifeform(pBot, pBot->Edict->v.origin, ALIEN_LIFEFORM_ONE);
			return;
		}

		AIPlayerSetAlienAssaultPrimaryTask(pBot, Task);

		return;
	}

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	DeployableSearchFilter EnemyStructureFilter;
	EnemyStructureFilter.DeployableTeam = EnemyTeam;
	EnemyStructureFilter.ReachabilityTeam = BotTeam;
	EnemyStructureFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

	Vector EnemyBaseLocation = AITAC_GetTeamStartingLocation(EnemyTeam);

	AvHAIBuildableStructure* EnemyStructureToAttack = nullptr;

	bool bEnemyIsMarines = (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE);

	if (bEnemyIsMarines)
	{
		EnemyStructureFilter.DeployableTypes = (STRUCTURE_MARINE_ARMSLAB | STRUCTURE_MARINE_OBSERVATORY | STRUCTURE_MARINE_INFANTRYPORTAL);
		EnemyStructureToAttack = AITAC_FindFurthestDeployableFromLocation(EnemyBaseLocation, &EnemyStructureFilter);
	}
	else
	{
		EnemyStructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);
		EnemyStructureFilter.DeployableTypes = (STRUCTURE_ALIEN_RESTOWER | STRUCTURE_ALIEN_DEFENCECHAMBER | STRUCTURE_ALIEN_MOVEMENTCHAMBER | STRUCTURE_ALIEN_SENSORYCHAMBER);
		EnemyStructureToAttack = AITAC_FindClosestDeployableToLocation(EnemyBaseLocation, &EnemyStructureFilter);
	}

	if (EnemyStructureToAttack)
	{
		AITASK_SetAttackTask(pBot, Task, EnemyStructureToAttack->edict, false);
		return;
	}

	if (bEnemyIsMarines)
	{
		edict_t* CommChair = AITAC_GetCommChair(EnemyTeam);

		if (!FNullEnt(CommChair))
		{
			AITASK_SetAttackTask(pBot, Task, CommChair, false);
			return;
		}
	}
	else
	{
		const AvHAIHiveDefinition* EnemyHive = AITAC_GetActiveHiveNearestLocation(EnemyTeam, pBot->Edict->v.origin);

		AITASK_SetAttackTask(pBot, Task, EnemyHive->HiveEntity->edict(), false);
		return;
	}

	vector<AvHPlayer*> AllEnemyPlayers = AIMGR_GetAllPlayersOnTeam(EnemyTeam);
	edict_t* TargetPlayer = nullptr;

	float MinDist = 0.0f;

	for (auto it = AllEnemyPlayers.begin(); it != AllEnemyPlayers.end(); it++)
	{
		AvHPlayer* ThisPlayer = (*it);

		if (!ThisPlayer) { continue; }

		edict_t* PlayerEdict = ThisPlayer->edict();

		if (!IsPlayerActiveInGame(PlayerEdict)) { continue; }

		float ThisDist = vDist2DSq(PlayerEdict->v.origin, pBot->Edict->v.origin);

		if (FNullEnt(TargetPlayer) || ThisDist < MinDist)
		{
			TargetPlayer = PlayerEdict;
			MinDist = ThisDist;
		}
	}

	if (!FNullEnt(TargetPlayer))
	{
		MoveTo(pBot, UTIL_GetEntityGroundLocation(TargetPlayer), MOVESTYLE_NORMAL);
	}


}

void AIPlayerSetSecondaryAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	if (IsPlayerGorge(pBot->Edict))
	{
		edict_t* TeamMateToHeal = nullptr;

		vector<AvHPlayer*> AllNearbyTeammates = AITAC_GetAllPlayersOfTeamInArea(BotTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

		float MinDist = 0.0f;

		for (auto it = AllNearbyTeammates.begin(); it != AllNearbyTeammates.end(); it++)
		{
			edict_t* ThisPlayer = (*it)->edict();

			if (!FNullEnt(ThisPlayer) && IsPlayerActiveInGame(ThisPlayer) && GetPlayerOverallHealthPercent(ThisPlayer) < 0.99f)
			{
				float ThisDist = vDist2DSq(pBot->Edict->v.origin, ThisPlayer->v.origin);
				if (FNullEnt(TeamMateToHeal) || ThisDist < MinDist)
				{
					TeamMateToHeal = ThisPlayer;
				}
			}
		}

		if (!FNullEnt(TeamMateToHeal))
		{
			Task->TaskType = TASK_HEAL;
			Task->TaskTarget = TeamMateToHeal;
			Task->bTaskIsUrgent = true;
			return;
		}

		DeployableSearchFilter DamagedStructuresFilter;
		DamagedStructuresFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
		DamagedStructuresFilter.DeployableTeam = BotTeam;
		DamagedStructuresFilter.ReachabilityTeam = BotTeam;
		DamagedStructuresFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		DamagedStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		DamagedStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);

		vector<AvHAIBuildableStructure*> AllNearbyStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &DamagedStructuresFilter);

		edict_t* StructureToHeal = nullptr;

		MinDist = 0.0f;

		for (auto it = AllNearbyStructures.begin(); it != AllNearbyStructures.end(); it++)
		{
			AvHAIBuildableStructure* ThisStructure = (*it);

			if (ThisStructure && ThisStructure->healthPercent < 0.99f)
			{
				float ThisDist = vDist2DSq(pBot->Edict->v.origin, ThisStructure->Location);
				if (FNullEnt(StructureToHeal) || ThisDist < MinDist)
				{
					StructureToHeal = ThisStructure->edict;
					MinDist = ThisDist;
				}
			}
		}

		if (!FNullEnt(StructureToHeal))
		{
			Task->TaskType = TASK_HEAL;
			Task->TaskTarget = StructureToHeal;
			Task->bTaskIsUrgent = true;
			return;
		}

		AITASK_ClearBotTask(pBot, Task);

		return;
	}

	vector<AvHAIHiveDefinition*> AllHives = AITAC_GetAllTeamHives(BotTeam, false);

	AvHAIHiveDefinition* HiveToDefend = nullptr;
	float MinDist = 0.0f;

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		AvHAIHiveDefinition* ThisHive = (*it);

		if (ThisHive && ThisHive->bIsUnderAttack)
		{
			int AttackerStrength = 0;
			int DefenderStrength = 0;

			vector<AvHPlayer*> AttackingPlayers = AITAC_GetAllPlayersOfTeamInArea(EnemyTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), false, nullptr, AVH_USER3_NONE);
			
			for (auto AttackerIt = AttackingPlayers.begin(); AttackerIt != AttackingPlayers.end(); AttackerIt++)
			{
				AvHPlayer* ThisPlayer = (*AttackerIt);
				edict_t* ThisPlayerEdict = ThisPlayer->edict();

				int ThisAttackerStrength = 1;

				if (PlayerHasWeapon(ThisPlayer, WEAPON_MARINE_HMG) || PlayerHasHeavyArmour(ThisPlayerEdict))
				{
					ThisAttackerStrength = 2;
				}

				if (IsPlayerFade(ThisPlayerEdict))
				{
					ThisAttackerStrength = 2;
				}

				if (IsPlayerOnos(ThisPlayerEdict))
				{
					ThisAttackerStrength = 3;
				}

				AttackerStrength += ThisAttackerStrength;
			}

			vector<AvHPlayer*> DefendingPlayers = AITAC_GetAllPlayersOfTeamInArea(EnemyTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

			for (auto DefenderIt = DefendingPlayers.begin(); DefenderIt != DefendingPlayers.end(); DefenderIt++)
			{
				AvHPlayer* ThisPlayer = (*DefenderIt);
				edict_t* ThisPlayerEdict = ThisPlayer->edict();

				int ThisDefenderStrength = 1;

				if (IsPlayerFade(ThisPlayerEdict))
				{
					ThisDefenderStrength = 2;
				}

				if (IsPlayerOnos(ThisPlayerEdict))
				{
					ThisDefenderStrength = 3;
				}

				DefenderStrength += ThisDefenderStrength;
			}

			vector<AvHAIPlayer*> AllOtherBots = AIMGR_GetAIPlayersOnTeam(BotTeam);

			for (auto BotIt = AllOtherBots.begin(); BotIt != AllOtherBots.end(); BotIt++)
			{
				AvHAIPlayer* ThisBot = (*BotIt);

				if (ThisBot != pBot && IsPlayerActiveInGame(ThisBot->Edict) && !IsPlayerGorge(ThisBot->Edict) && vDist2DSq(ThisBot->Edict->v.origin, ThisHive->FloorLocation) > sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
				{
					if (ThisBot->SecondaryBotTask.TaskType == TASK_DEFEND && ThisBot->SecondaryBotTask.TaskTarget == ThisHive->HiveEntity->edict())
					{
						int ThisDefenderStrength = 1;

						if (IsPlayerFade(ThisBot->Edict))
						{
							ThisDefenderStrength = 2;
						}

						if (IsPlayerOnos(ThisBot->Edict))
						{
							ThisDefenderStrength = 3;
						}

						DefenderStrength += ThisDefenderStrength;
					}
				}
			}

			if (AttackerStrength >= DefenderStrength)
			{
				float ThisDist = vDist2DSq(ThisHive->FloorLocation, pBot->Edict->v.origin);

				if (!HiveToDefend || ThisDist < MinDist)
				{
					HiveToDefend = ThisHive;
					MinDist = ThisDist;
				}
			}
		}
	}

	if (HiveToDefend)
	{
		AITASK_SetDefendTask(pBot, Task, HiveToDefend->HiveEntity->edict(), true);
		return;
	}

	DeployableSearchFilter AttackedStructuresFilter;
	AttackedStructuresFilter.DeployableTypes = (IsPlayerLerk(pBot->Edict)) ? SEARCH_ALL_STRUCTURES : STRUCTURE_ALIEN_RESTOWER;
	AttackedStructuresFilter.DeployableTeam = BotTeam;
	AttackedStructuresFilter.ReachabilityTeam = BotTeam;
	AttackedStructuresFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	AttackedStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_UNDERATTACK;
	AttackedStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(30.0f);

	vector<AvHAIBuildableStructure*> AllAttackedStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &AttackedStructuresFilter);

	AvHAIBuildableStructure* StructureToDefend = nullptr;
	MinDist = 0.0f;

	for (auto it = AllAttackedStructures.begin(); it != AllAttackedStructures.end(); it++)
	{
		AvHAIBuildableStructure* ThisStructure = (*it);

		float ThisDist = vDist2D(pBot->Edict->v.origin, ThisStructure->edict->v.origin);

		int NumAttackers = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, ThisStructure->Location, UTIL_MetresToGoldSrcUnits(15.0f), nullptr);

		if (NumAttackers == 0) { continue; }

		int NumExistingDefenders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, ThisStructure->Location, ThisDist - 10.0f, false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

		if (NumExistingDefenders < 2)
		{
			if (!StructureToDefend || ThisDist < MinDist)
			{
				StructureToDefend = ThisStructure;
				MinDist = ThisDist;
			}
		}
	}

	if (StructureToDefend)
	{
		AITASK_SetDefendTask(pBot, Task, StructureToDefend->edict, true);
		return;
	}

	AITASK_ClearBotTask(pBot, Task);

}

bool AlienCombatThink(AvHAIPlayer* pBot)
{
	if (pBot->CurrentEnemy > -1)
	{
		edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

		pBot->CurrentCombatStrategy = GetBotCombatStrategyForTarget(pBot, &pBot->TrackedEnemies[pBot->CurrentEnemy]);

		if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_IGNORE) { return false; }

		pBot->LastCombatTime = gpGlobals->time;

		switch (pBot->Player->GetUser3())
		{
		case AVH_USER3_ALIEN_PLAYER1:
			return SkulkCombatThink(pBot);
		case AVH_USER3_ALIEN_PLAYER2:
			return GorgeCombatThink(pBot);
		case AVH_USER3_ALIEN_PLAYER3:
			return LerkCombatThink(pBot);
		case AVH_USER3_ALIEN_PLAYER4:
			return FadeCombatThink(pBot);
		case AVH_USER3_ALIEN_PLAYER5:
			return OnosCombatThink(pBot);
		default:
			return false;
		}
	}

	return false;
}

bool SkulkCombatThink(AvHAIPlayer* pBot)
{
	edict_t* pEdict = pBot->Edict;

	AvHPlayer* EnemyPlayer = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyPlayer;
	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = EnemyPlayer->GetTeam();

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, CurrentEnemy->v.origin);

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		edict_t* NearestHealingSource = AITAC_AlienFindNearestHealingSource(pBot->Player->GetTeam(), pBot->Edict->v.origin, pBot->Edict, true);

		// Run away if low on health and have a healing spot
		if (!FNullEnt(NearestHealingSource))
		{
			float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

			bool bOutOfEnemyLOS = !UTIL_PlayerHasLOSToEntity(CurrentEnemy, pBot->Edict, UTIL_GoldSrcUnitsToMetres(30.0f), false);

			float DistFromHealingSourceSq = vDist2DSq(pBot->Edict->v.origin, NearestHealingSource->v.origin);

			bool bInHealingRange = (DistFromHealingSourceSq <= sqrf(DesiredDistFromHealingSource));

			if (!bInHealingRange)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				return true;
			}

			if (bOutOfEnemyLOS)
			{
				if (bInHealingRange)
				{
					BotLookAt(pBot, TrackedEnemyRef->LastLOSPosition);
				}
				else
				{
					MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				}

				return true;
			}

			if (!UTIL_PlayerHasLOSToLocation(TrackedEnemyRef->EnemyEdict, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				return true;
			}

		}

		return false;
	}

	bool bShouldBreakAmbush = false;

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_AMBUSH)
	{
		bShouldBreakAmbush = DistToEnemy < ((TrackedEnemyRef->bHasLOS) ? sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) : sqrf(UTIL_MetresToGoldSrcUnits(3.0f)));
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_ATTACK || (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_AMBUSH && bShouldBreakAmbush))
	{	

		AvHAIWeapon DesiredWeapon = WEAPON_SKULK_BITE;

		// If we have xenocide, then choose it if we have lots of good targets in blast radius
		if (PlayerHasWeapon(pBot->Player, WEAPON_SKULK_XENOCIDE))
		{
			AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(pBot->Player->GetTeam());
			float XenocideRadius = GetMaxIdealWeaponRange(WEAPON_SKULK_XENOCIDE);

			// Add one to include the target themselves
			int NumEnemyTargetsInArea = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, CurrentEnemy->v.origin, XenocideRadius, CurrentEnemy) + 1;

			if (NumEnemyTargetsInArea <= 2)
			{
				AvHTeam* EnemyTeamRef = GetGameRules()->GetTeam(EnemyTeam);

				if (EnemyTeamRef)
				{
					AvHAIDeployableStructureType StructureSearchType = (EnemyTeamRef->GetTeamType() == AVH_CLASS_TYPE_MARINE) ? SEARCH_ALL_MARINE_STRUCTURES : SEARCH_ALL_ALIEN_STRUCTURES;

					DeployableSearchFilter SearchFilter;
					SearchFilter.DeployableTypes = StructureSearchType;
					SearchFilter.MaxSearchRadius = XenocideRadius;
					SearchFilter.DeployableTeam = EnemyTeam;

					NumEnemyTargetsInArea += AITAC_GetNumDeployablesNearLocation(CurrentEnemy->v.origin, &SearchFilter);
				}
			}

			// We're going to use Xenocide
			if (NumEnemyTargetsInArea > 2)
			{
				DesiredWeapon = WEAPON_SKULK_XENOCIDE;
			}
		}

		if (DesiredWeapon != WEAPON_SKULK_XENOCIDE)
		{
			if (!IsPlayerParasited(CurrentEnemy) && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
			{
				DesiredWeapon = WEAPON_SKULK_PARASITE;
			}
		}

		BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredWeapon, CurrentEnemy);
		Vector MoveTarget = UTIL_GetEntityGroundLocation(CurrentEnemy);

		if (LOSCheck == ATTACK_SUCCESS)
		{
			BotShootTarget(pBot, DesiredWeapon, CurrentEnemy);

			Vector EnemyFacing = UTIL_GetForwardVector2D(CurrentEnemy->v.angles);
			Vector BotFacing = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->Edict->v.origin);

			float Dot = UTIL_GetDotProduct2D(EnemyFacing, BotFacing);

			if (Dot < 0.0f)
			{
				Vector TargetLocation = MoveTarget;
				Vector BehindPlayer = TargetLocation - (UTIL_GetForwardVector2D(CurrentEnemy->v.v_angle) * 50.0f);

				if (UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, BehindPlayer))
				{
					MoveTarget = BehindPlayer;
				}
			}
		}

		MoveTo(pBot, MoveTarget, MOVESTYLE_NORMAL);

		if (DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			if (CanBotLeap(pBot))
			{
				BotLeap(pBot, CurrentEnemy->v.origin);
			}
			else
			{
				if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint == pBot->BotNavInfo.CurrentPath.end()) { return true; }

				// EVASIVE MANOEUVRES! Only do this if we're running along the floor and aren't approaching a path point (so we don't stray off the path)
				if (pBot->BotNavInfo.CurrentPathPoint->flag == SAMPLE_POLYFLAGS_WALK && vDist2DSq(pBot->Edict->v.origin, pBot->BotNavInfo.CurrentPathPoint->Location) > sqrf(50.0f))
				{
					Vector RightDir = UTIL_GetCrossProduct(pBot->desiredMovementDir, UP_VECTOR);

					pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(pBot->desiredMovementDir + RightDir) : UTIL_GetVectorNormal2D(pBot->desiredMovementDir - RightDir);

					// Let's get ziggy with it
					if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
					{
						pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
						pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
					}

					BotMovementInputs(pBot);
				}
			}
		}

		return true;
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_AMBUSH)
	{
		if (TrackedEnemyRef->bHasLOS)
		{
			if (vIsZero(pBot->LastSafeLocation))
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetActiveHiveNearestLocation(pBot->Player->GetTeam(), pBot->Edict->v.origin);

				if (NearestHive)
				{
					pBot->LastSafeLocation = NearestHive->FloorLocation;
				}
			}

			MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
			BotLookAt(pBot, TrackedEnemyRef->LastSeenLocation);

			if (!IsPlayerParasited(CurrentEnemy))
			{
				BotShootTarget(pBot, WEAPON_SKULK_PARASITE, CurrentEnemy);
			}

			return true;
		}
		else
		{
			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastSeenLocation);
		}

		return true;
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_SKIRMISH)
	{
		if (TrackedEnemyRef->bHasLOS)
		{
			if (vIsZero(pBot->LastSafeLocation))
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetActiveHiveNearestLocation(pBot->Player->GetTeam(), pBot->Edict->v.origin);

				if (NearestHive)
				{
					pBot->LastSafeLocation = NearestHive->FloorLocation;
				}
			}

			if (GetPlayerEnergy(pBot->Edict) < GetEnergyCostForWeapon(WEAPON_SKULK_PARASITE))
			{
				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
			}

			BotLookAt(pBot, TrackedEnemyRef->LastSeenLocation);

			BotShootTarget(pBot, WEAPON_SKULK_PARASITE, CurrentEnemy);

			return true;
		}
		else
		{
			if (GetPlayerEnergy(pBot->Edict) >= 0.9f)
			{
				MoveTo(pBot, TrackedEnemyRef->LastSeenLocation, MOVESTYLE_NORMAL);
				return true;
			}
			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastSeenLocation);
		}

		return true;
	}

	return false;
}

bool GorgeCombatThink(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy) || !IsPlayerActiveInGame(CurrentEnemy)) { return false; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	float CurrentHealthPercent = GetPlayerOverallHealthPercent(pBot->Edict);

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		BotAttackResult AttackResult = PerformAttackLOSCheck(pBot, WEAPON_GORGE_SPIT, CurrentEnemy);

		edict_t* NearestHealingSource = AITAC_AlienFindNearestHealingSource(pBot->Player->GetTeam(), pBot->Edict->v.origin, pBot->Edict, true);

		// Run away if low on health and have a healing spot
		if (!FNullEnt(NearestHealingSource))
		{
			float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

			bool bOutOfEnemyLOS = !TrackedEnemyRef->bHasLOS;

			float DistFromHealingSourceSq = vDist2DSq(pBot->Edict->v.origin, NearestHealingSource->v.origin);

			bool bInHealingRange = (DistFromHealingSourceSq <= sqrf(DesiredDistFromHealingSource));

			if (!bInHealingRange)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

				if (CurrentHealthPercent > 0.5f && AttackResult == ATTACK_SUCCESS)
				{
					BotShootTarget(pBot, WEAPON_GORGE_SPIT, CurrentEnemy);
				}
				else
				{
					pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;

					if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_GORGE_HEALINGSPRAY)
					{
						pBot->Button |= IN_ATTACK;
					}
				}

				Vector EnemyOrientation = UTIL_GetVectorNormal2D(pBot->desiredMovementDir);
				Vector RightDir = UTIL_GetCrossProduct(EnemyOrientation, UP_VECTOR);

				pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(pBot->desiredMovementDir + RightDir) : UTIL_GetVectorNormal2D(pBot->desiredMovementDir - RightDir);

				// Let's get ziggy with it
				if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
				{
					pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
					pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
				}

				BotMovementInputs(pBot);

				return true;
			}

			if (bOutOfEnemyLOS)
			{
				if (bInHealingRange)
				{
					BotLookAt(pBot, TrackedEnemyRef->LastLOSPosition);
				}
				else
				{
					MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				}

				pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;

				if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_GORGE_HEALINGSPRAY)
				{
					pBot->Button |= IN_ATTACK;
				}

				return true;
			}

			if (!UTIL_PlayerHasLOSToLocation(TrackedEnemyRef->EnemyEdict, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

				if (CurrentHealthPercent > 0.5f && AttackResult == ATTACK_SUCCESS)
				{
					BotShootTarget(pBot, WEAPON_GORGE_SPIT, CurrentEnemy);
				}
				else
				{
					pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;

					if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_GORGE_HEALINGSPRAY)
					{
						pBot->Button |= IN_ATTACK;
					}

				}

				Vector EnemyOrientation = UTIL_GetVectorNormal2D(pBot->desiredMovementDir);
				Vector RightDir = UTIL_GetCrossProduct(EnemyOrientation, UP_VECTOR);

				pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(pBot->desiredMovementDir + RightDir) : UTIL_GetVectorNormal2D(pBot->desiredMovementDir - RightDir);

				// Let's get ziggy with it
				if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
				{
					pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
					pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
				}

				BotMovementInputs(pBot);

				return true;
			}
			else
			{
				if (AttackResult == ATTACK_SUCCESS)
				{
					BotShootTarget(pBot, WEAPON_GORGE_SPIT, CurrentEnemy);
				}

				Vector EnemyOrientation = UTIL_GetVectorNormal2D(pBot->desiredMovementDir);
				Vector RightDir = UTIL_GetCrossProduct(EnemyOrientation, UP_VECTOR);

				pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(pBot->desiredMovementDir + RightDir) : UTIL_GetVectorNormal2D(pBot->desiredMovementDir - RightDir);

				// Let's get ziggy with it
				if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
				{
					pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
					pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
				}

				BotMovementInputs(pBot);
			}

		}

		return false;
	}

	// Just zig-zag and bombard the enemy with spit if they have LOS, or go hunt them if not
	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_ATTACK)
	{
		BotAttackResult AttackResult = PerformAttackLOSCheck(pBot, WEAPON_GORGE_SPIT, CurrentEnemy);

		if (CurrentHealthPercent < 0.5f)
		{
			pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;

			if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_GORGE_HEALINGSPRAY)
			{
				pBot->Button |= IN_ATTACK;
			}

			return true;
		}

		if (AttackResult == ATTACK_SUCCESS)
		{
			BotShootTarget(pBot, WEAPON_GORGE_SPIT, CurrentEnemy);

			Vector EnemyOrientation = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->Edict->v.origin);
			Vector RightDir = UTIL_GetCrossProduct(EnemyOrientation, UP_VECTOR);

			pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(pBot->desiredMovementDir + RightDir) : UTIL_GetVectorNormal2D(pBot->desiredMovementDir - RightDir);

			// Let's get ziggy with it
			if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
			{
				pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
				pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
			}

			BotMovementInputs(pBot);

			return true;
		}
		else
		{
			MoveTo(pBot, TrackedEnemyRef->LastSeenLocation, MOVESTYLE_NORMAL);
		}

		return false;
	}

	// Just zig-zag and bombard the enemy with spit if they have LOS, or go hunt them if not
	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_SKIRMISH)
	{
		BotAttackResult AttackResult = PerformAttackLOSCheck(pBot, WEAPON_GORGE_SPIT, CurrentEnemy);

		if (vDist2DSq(CurrentEnemy->v.origin, pBot->LastSafeLocation) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			pBot->LastSafeLocation = ZERO_VECTOR;
		}

		if (vIsZero(pBot->LastSafeLocation))
		{
			const AvHAIHiveDefinition* NearestHive = AITAC_GetActiveHiveNearestLocation(pBot->Player->GetTeam(), pBot->Edict->v.origin);

			if (NearestHive)
			{
				pBot->LastSafeLocation = NearestHive->FloorLocation;
			}
		}

		if (TrackedEnemyRef->bHasLOS)
		{
			if (CurrentHealthPercent < 0.7f || vDist2DSq(CurrentEnemy->v.origin, pBot->Edict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) || vDist2DSq(pBot->Edict->v.origin, pBot->LastSafeLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
			{
				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);

				if (CurrentHealthPercent < 0.5f)
				{
					pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;

					if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_GORGE_HEALINGSPRAY)
					{
						pBot->Button |= IN_ATTACK;
					}

					return true;
				}
			}

			BotLookAt(pBot, TrackedEnemyRef->LastSeenLocation);

			if (AttackResult == ATTACK_SUCCESS)
			{
				BotShootTarget(pBot, WEAPON_GORGE_SPIT, CurrentEnemy);
			}
			return true;
		}
		else
		{
			if (CurrentHealthPercent >= 0.99f)
			{
				MoveTo(pBot, TrackedEnemyRef->LastSeenLocation, MOVESTYLE_NORMAL);
				return true;
			}

			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastSeenLocation);

			pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;

			if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_GORGE_HEALINGSPRAY)
			{
				pBot->Button |= IN_ATTACK;
			}
		}

		return true;
	}

	return false;
}

bool LerkCombatThink(AvHAIPlayer* pBot)
{

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy) || !IsPlayerActiveInGame(CurrentEnemy)) { return false; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		edict_t* NearestHealingSource = AITAC_AlienFindNearestHealingSource(pBot->Player->GetTeam(), pBot->Edict->v.origin, pBot->Edict, true);

		// Run away if low on health and have a healing spot
		if (!FNullEnt(NearestHealingSource))
		{
			float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

			bool bOutOfEnemyLOS = !UTIL_PlayerHasLOSToEntity(CurrentEnemy, pBot->Edict, UTIL_GoldSrcUnitsToMetres(30.0f), false);

			float DistFromHealingSourceSq = vDist2DSq(pBot->Edict->v.origin, NearestHealingSource->v.origin);

			bool bInHealingRange = (DistFromHealingSourceSq <= sqrf(DesiredDistFromHealingSource));

			Vector SporeLocation = (TrackedEnemyRef->bHasLOS) ? TrackedEnemyRef->LastSeenLocation : TrackedEnemyRef->LastLOSPosition;

			// We will cover our tracks with spores if we have a valid target location, we have enough energy, the area isn't affected by spores already and we have LOS to the spore location
			bool bCanSpore = (SporeLocation != ZERO_VECTOR && GetPlayerEnergy(pBot->Edict) > (GetEnergyCostForWeapon(WEAPON_LERK_SPORES) * 1.1f) && !IsAreaAffectedBySpores(SporeLocation) && UTIL_QuickTrace(pBot->Edict, pBot->CurrentEyePosition, SporeLocation));

			// If we are super low on health then just get the hell out of there
			if (GetPlayerOverallHealthPercent(pBot->Edict) <= 0.2) { bCanSpore = false; }

			if (!bInHealingRange)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

				if (bCanSpore)
				{
					BotShootLocation(pBot, WEAPON_LERK_SPORES, SporeLocation);
				}

				return true;
			}

			if (bOutOfEnemyLOS)
			{
				if (bInHealingRange)
				{
					BotLookAt(pBot, TrackedEnemyRef->LastLOSPosition);

					if (bCanSpore)
					{
						BotShootLocation(pBot, WEAPON_LERK_SPORES, SporeLocation);
					}
				}
				else
				{
					MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

					if (bCanSpore)
					{
						BotShootLocation(pBot, WEAPON_LERK_SPORES, SporeLocation);
					}
				}

				return true;
			}

			if (!UTIL_PlayerHasLOSToLocation(TrackedEnemyRef->EnemyEdict, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				return true;
			}

		}

		return true;
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_ATTACK)
	{
		AvHAIWeapon DesiredWeapon = WEAPON_LERK_BITE;

		if (vDist2DSq(pBot->Edict->v.origin, CurrentEnemy->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			if (!IsAreaAffectedBySpores(CurrentEnemy->v.origin))
			{
				DesiredWeapon = WEAPON_LERK_SPORES;
			}
		}

		MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);

		BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredWeapon, CurrentEnemy);

		if (LOSCheck == ATTACK_SUCCESS)
		{
			BotShootTarget(pBot, DesiredWeapon, CurrentEnemy);
		}

		return true;		
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_SKIRMISH)
	{
		pBot->DesiredCombatWeapon = WEAPON_LERK_SPORES;

		if (GetPlayerCurrentWeapon(pBot->Player) != WEAPON_LERK_SPORES) { return true; }

		if (GetPlayerEnergy(pBot->Edict) < (GetEnergyCostForWeapon(WEAPON_LERK_SPORES) * 1.1f)
			|| IsAreaAffectedBySpores(CurrentEnemy->v.origin)
			|| GetTimeUntilPlayerNextRefire(pBot->Player) > 0.0f)
		{
			BotMoveStyle DesiredMoveStyle = (vDist2DSq(pBot->Edict->v.origin, pBot->LastSafeLocation) < sqrf(UTIL_MetresToGoldSrcUnits(3.0f))) ? MOVESTYLE_AMBUSH : MOVESTYLE_NORMAL;

			if (vIsZero(pBot->LastSafeLocation))
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetActiveHiveNearestLocation(pBot->Player->GetTeam(), pBot->Edict->v.origin);

				if (NearestHive)
				{
					pBot->LastSafeLocation = NearestHive->FloorLocation;
				}
			}

			MoveTo(pBot, pBot->LastSafeLocation, DesiredMoveStyle);

			if (DesiredMoveStyle != MOVESTYLE_NORMAL)
			{
				BotLookAt(pBot, TrackedEnemyRef->LastSeenLocation);
			}
			return true;
		}

		BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, pBot->DesiredCombatWeapon, CurrentEnemy);

		if (LOSCheck == ATTACK_SUCCESS)
		{
			BotShootTarget(pBot, pBot->DesiredCombatWeapon, CurrentEnemy);
		}
		else
		{
			MoveTo(pBot, TrackedEnemyRef->LastSeenLocation, MOVESTYLE_AMBUSH);
		}

		return true;
	}

	return true;
}

bool FadeCombatThink(AvHAIPlayer* pBot)
{
	edict_t* pEdict = pBot->Edict;

	AvHPlayer* EnemyPlayer = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyPlayer;
	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = EnemyPlayer->GetTeam();

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, CurrentEnemy->v.origin);

	bool bShouldBreakRetreat = false;

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		edict_t* NearestHealingSource = AITAC_AlienFindNearestHealingSource(pBot->Player->GetTeam(), pBot->Edict->v.origin, pBot->Edict, true);

		// Run away if low on health and have a healing spot
		if (!FNullEnt(NearestHealingSource))
		{
			float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

			bool bOutOfEnemyLOS = !UTIL_PlayerHasLOSToEntity(CurrentEnemy, pBot->Edict, UTIL_GoldSrcUnitsToMetres(30.0f), false);

			float DistFromHealingSourceSq = vDist2DSq(pBot->Edict->v.origin, NearestHealingSource->v.origin);

			bool bInHealingRange = (DistFromHealingSourceSq <= sqrf(DesiredDistFromHealingSource));

			if (!bInHealingRange)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

				// If we're still in danger while retreating, do extra leaping to get the hell out
				if (TrackedEnemyRef->bHasLOS)
				{
					if (pBot->BotNavInfo.CurrentPathPoint != pBot->BotNavInfo.CurrentPath.end() && pBot->BotNavInfo.CurrentPathPoint->flag != SAMPLE_POLYFLAGS_WALLCLIMB && pBot->BotNavInfo.CurrentPathPoint->flag != SAMPLE_POLYFLAGS_LIFT)
					{
						BotLeap(pBot, pBot->BotNavInfo.CurrentPathPoint->Location);
					}
				}

				return true;
			}

			if (bOutOfEnemyLOS)
			{
				BotLookAt(pBot, TrackedEnemyRef->LastLOSPosition);
				if (PlayerHasWeapon(pBot->Player, WEAPON_FADE_METABOLIZE))
				{
					pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;

					if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_FADE_METABOLIZE)
					{
						pBot->Button |= IN_ATTACK;
					}
				}
				return true;
			}

			if (!UTIL_PlayerHasLOSToLocation(TrackedEnemyRef->EnemyEdict, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				return true;
			}

			// If the enemy can see the healing source, then we must go on the attack
			bShouldBreakRetreat = true;
		}
	}

	bool bShouldBreakAmbush = false;

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_AMBUSH)
	{
		bShouldBreakAmbush = DistToEnemy < ((TrackedEnemyRef->bHasLOS) ? sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) : sqrf(UTIL_MetresToGoldSrcUnits(3.0f)));
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_ATTACK || bShouldBreakAmbush || bShouldBreakRetreat)
	{
		AvHAIWeapon DesiredWeapon = WEAPON_FADE_SWIPE;

		BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredWeapon, CurrentEnemy);
		Vector MoveTarget = UTIL_GetEntityGroundLocation(CurrentEnemy);

		float EnemySpeed = vSize2D(CurrentEnemy->v.velocity);

		if (LOSCheck == ATTACK_SUCCESS)
		{
			BotShootTarget(pBot, DesiredWeapon, CurrentEnemy);

			Vector EnemyFacing = UTIL_GetForwardVector2D(CurrentEnemy->v.angles);
			Vector BotFacing = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->Edict->v.origin);

			float Dot = UTIL_GetDotProduct2D(EnemyFacing, BotFacing);

			if (EnemySpeed < 16.0f && Dot < 0.0f)
			{
				Vector TargetLocation = MoveTarget;
				Vector BehindPlayer = TargetLocation - (UTIL_GetForwardVector2D(CurrentEnemy->v.v_angle) * 50.0f);

				if (UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, BehindPlayer))
				{
					MoveTarget = BehindPlayer;
				}
			}
		}

		MoveTarget = MoveTarget + (CurrentEnemy->v.velocity * 0.1f);

		MoveTo(pBot, MoveTarget, MOVESTYLE_NORMAL);

		if (LOSCheck == ATTACK_OUTOFRANGE && UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveTarget))
		{
			if (CanBotLeap(pBot))
			{
				BotLeap(pBot, MoveTarget);
			}
		}

		return true;
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_AMBUSH)
	{
		if (TrackedEnemyRef->bHasLOS)
		{
			if (vIsZero(pBot->LastSafeLocation))
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetActiveHiveNearestLocation(pBot->Player->GetTeam(), pBot->Edict->v.origin);

				if (NearestHive)
				{
					pBot->LastSafeLocation = NearestHive->FloorLocation;
				}
			}

			MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
			BotLookAt(pBot, TrackedEnemyRef->LastSeenLocation);

			if (PlayerHasWeapon(pBot->Player, WEAPON_FADE_ACIDROCKET))
			{
				BotShootTarget(pBot, WEAPON_FADE_ACIDROCKET, CurrentEnemy);
			}

			return true;
		}
		else
		{
			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastSeenLocation);

			if (GetPlayerOverallHealthPercent(pBot->Edict) < 1.0f && PlayerHasWeapon(pBot->Player, WEAPON_FADE_METABOLIZE))
			{
				pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;

				if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_FADE_METABOLIZE)
				{
					pBot->Button |= IN_ATTACK;
				}
			}
		}

		return true;
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_SKIRMISH)
	{
		if (TrackedEnemyRef->bHasLOS)
		{
			if (vIsZero(pBot->LastSafeLocation))
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetActiveHiveNearestLocation(pBot->Player->GetTeam(), pBot->Edict->v.origin);

				if (NearestHive)
				{
					pBot->LastSafeLocation = NearestHive->FloorLocation;
				}
			}

			if (GetPlayerEnergy(pBot->Edict) < (0.9f - (GetEnergyCostForWeapon(WEAPON_FADE_ACIDROCKET) * 4.0f)) || GetPlayerOverallHealthPercent(pBot->Edict) < 6.0f)
			{
				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
			}
			else
			{
				Vector EnemyOrientation = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->Edict->v.origin);
				Vector RightDir = UTIL_GetCrossProduct(EnemyOrientation, UP_VECTOR);

				pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(pBot->desiredMovementDir + RightDir) : UTIL_GetVectorNormal2D(pBot->desiredMovementDir - RightDir);

				// Let's get ziggy with it
				if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
				{
					pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
					pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
				}

				BotMovementInputs(pBot);
			}

			BotLookAt(pBot, TrackedEnemyRef->LastSeenLocation);

			BotShootTarget(pBot, WEAPON_FADE_ACIDROCKET, CurrentEnemy);

			return true;
		}
		else
		{
			if (GetPlayerEnergy(pBot->Edict) >= 0.9f && GetPlayerOverallHealthPercent(pBot->Edict) > 0.8f)
			{
				MoveTo(pBot, TrackedEnemyRef->LastSeenLocation, MOVESTYLE_NORMAL);
				return true;
			}

			if (GetPlayerOverallHealthPercent(pBot->Edict) < 1.0f && PlayerHasWeapon(pBot->Player, WEAPON_FADE_METABOLIZE))
			{
				pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;

				if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_FADE_METABOLIZE)
				{
					pBot->Button |= IN_ATTACK;
				}
			}

			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastSeenLocation);
		}

		return true;
	}

	return true;
}

bool OnosCombatThink(AvHAIPlayer* pBot)
{
	edict_t* pEdict = pBot->Edict;

	AvHPlayer* EnemyPlayer = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyPlayer;
	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = EnemyPlayer->GetTeam();

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, CurrentEnemy->v.origin);

	bool bShouldBreakRetreat = false;

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		edict_t* NearestHealingSource = AITAC_AlienFindNearestHealingSource(pBot->Player->GetTeam(), pBot->Edict->v.origin, pBot->Edict, true);

		// Run away if low on health and have a healing spot
		if (!FNullEnt(NearestHealingSource))
		{
			float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

			bool bOutOfEnemyLOS = !UTIL_PlayerHasLOSToEntity(CurrentEnemy, pBot->Edict, UTIL_GoldSrcUnitsToMetres(30.0f), false);

			float DistFromHealingSourceSq = vDist2DSq(pBot->Edict->v.origin, NearestHealingSource->v.origin);

			bool bInHealingRange = (DistFromHealingSourceSq <= sqrf(DesiredDistFromHealingSource));

			if (!bInHealingRange)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				return true;
			}

			if (bOutOfEnemyLOS)
			{
				BotLookAt(pBot, TrackedEnemyRef->LastLOSPosition);
				return true;
			}

			if (!UTIL_PlayerHasLOSToLocation(TrackedEnemyRef->EnemyEdict, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				return true;
			}

			// If the enemy can see the healing source, then we must go on the attack
			bShouldBreakRetreat = true;
		}
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_ATTACK || bShouldBreakRetreat)
	{
		Vector MoveTarget = UTIL_GetEntityGroundLocation(CurrentEnemy);

		MoveTarget = MoveTarget + (UTIL_GetVectorNormal2D(CurrentEnemy->v.velocity) * 0.1f);

		MoveTo(pBot, MoveTarget, MOVESTYLE_NORMAL);

		AvHAIWeapon DesiredWeapon = OnosGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

		if (DesiredWeapon == WEAPON_ONOS_CHARGE)
		{
			BotShootTarget(pBot, DesiredWeapon, CurrentEnemy);
			return true;
		}

		BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredWeapon, CurrentEnemy);

		if (LOSCheck == ATTACK_SUCCESS)
		{
			BotShootTarget(pBot, DesiredWeapon, CurrentEnemy);
		}
	}

	return true;
}