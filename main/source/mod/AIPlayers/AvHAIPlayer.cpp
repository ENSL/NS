
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
	return (PlayerHasWeapon(pBot->Player, WEAPON_SKULK_LEAP)) || (PlayerHasWeapon(pBot->Player, WEAPON_FADE_BLINK));
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

void BotReloadWeapons(AvHAIPlayer* pBot)
{
	// Aliens and commander don't reload
	if (!IsPlayerMarine(pBot->Edict) || !IsPlayerActiveInGame(pBot->Edict)) { return; }

	if (gpGlobals->time - pBot->LastCombatTime > 5.0f)
	{
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

			return;
		}

		if (WeaponCanBeReloaded(SecondaryWeapon) && BotGetSecondaryWeaponClipAmmo(pBot) < BotGetSecondaryWeaponMaxClipSize(pBot) && BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			pBot->DesiredCombatWeapon = SecondaryWeapon;

			if (CurrentWeapon == SecondaryWeapon)
			{
				BotReloadCurrentWeapon(pBot);
			}
			return;
		}
	}
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

	if (CurrentWeapon == WEAPON_NONE) { return; }


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

		if (!WeaponRef->GetMustPressTriggerForEachShot() || WeaponRef->m_flNextPrimaryAttack <= 0.0f)
		{
			pBot->Button |= IN_ATTACK;
		}
	}
}

void BotEvolveLifeform(AvHAIPlayer* pBot, Vector DesiredEvolveLocation, AvHMessageID TargetLifeform)
{
	if (!IsPlayerAlien(pBot->Edict)) { return; }

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

	float EvolveCost = 0.0f;

	switch (TargetLifeform)
	{
	case ALIEN_LIFEFORM_TWO:
		EvolveCost = BALANCE_VAR(kGorgeCost);
		break;
	case ALIEN_LIFEFORM_THREE:
		EvolveCost = BALANCE_VAR(kLerkCost);
		break;
	case ALIEN_LIFEFORM_FOUR:
		EvolveCost = BALANCE_VAR(kFadeCost);
		break;
	case ALIEN_LIFEFORM_FIVE:
		EvolveCost = BALANCE_VAR(kOnosCost);
		break;
	default:
		EvolveCost = 0.0f;
		break;
	}

	if (pBot->Player->GetResources() >= EvolveCost)
	{
		pBot->Impulse = TargetLifeform;
	}
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

		if (FNullEnt(PlayerEdict) || !IsPlayerActiveInGame(PlayerEdict) || PlayerEdict->v.team == pBot->Edict->v.team)
		{
			BotClearEnemyTrackingInfo(&pBot->TrackedEnemies[EnemyIndex]);
			continue;
		}

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

		bool bHasLOS = (VisiblePoint != ZERO_VECTOR);

		bool bIsTracked = (!bHasLOS && (IsPlayerParasited(Enemy) || IsPlayerMotionTracked(Enemy)));

		if (pBot->LastSafeLocation != ZERO_VECTOR && UTIL_PlayerHasLOSToLocation(Enemy, pBot->LastSafeLocation, UTIL_MetresToGoldSrcUnits(50.0f)))
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
			Vector FloorLocation = UTIL_GetFloorUnderEntity(Enemy);
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

	UpdateBotMoveProfile(pBot, pBot->BotNavInfo.MoveStyle);

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

void CustomThink(AvHAIPlayer* pBot)
{
	if (IsPlayerAlien(pBot->Edict))
	{
		if (!vIsZero(AIDEBUG_GetDebugVector1()))
		{
			const AvHAIHiveDefinition* Hive = AITAC_GetHiveNearestLocation(AIDEBUG_GetDebugVector1());

			BotAlienBuildHive(pBot, Hive);
		}
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

	AvHAIWeapon DesiredWeapon = (pBot->DesiredMoveWeapon != WEAPON_NONE) ? pBot->DesiredMoveWeapon : pBot->DesiredCombatWeapon;

	if (DesiredWeapon != WEAPON_NONE && GetPlayerCurrentWeapon(pBot->Player) != DesiredWeapon)
	{
		BotSwitchToWeapon(pBot, DesiredWeapon);
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

bool ShouldAIPlayerTakeCommand(AvHAIPlayer* pBot)
{
	AvHAICommanderMode CurrentCommanderMode = AIMGR_GetCommanderMode();

	// Don't go commander if bots are not allowed to
	if (CurrentCommanderMode == COMMANDERMODE_DISABLED) { return false; }

	AvHTeamNumber BotTeamNumber = pBot->Player->GetTeam();
	AvHTeam* BotTeam = GetGameRules()->GetTeam(BotTeamNumber);

	// Don't go commander if we're not an alien. You never know with the way I structure my logic...
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

	// Don't switch roles if already fade/onos or those resources are potentially wasted
	if (IsPlayerFade(pBot->Edict) || IsPlayerOnos(pBot->Edict))
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_ASSAULT);
		return;
	}

	// Likewise for lerks
	if (IsPlayerLerk(pBot->Edict))
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_HARASS);
		return;
	}


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
	float ResNodeOwnership = AITAC_GetTeamResNodeOwnership(BotTeamNumber);

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

	if (BotTeam->GetTeamType() == AVH_CLASS_TYPE_MARINE)
	{
		AIPlayerNSMarineThink(pBot);
	}
	else
	{
		AIPlayerNSAlienThink(pBot);
	}
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
			return &pBot->CommanderTask;
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

	if (gpGlobals->time < pBot->BotNextTaskEvaluationTime)
	{
		if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
		{
			BotProgressTask(pBot, pBot->CurrentTask);
			return;
		}		
	}

	pBot->BotNextTaskEvaluationTime = gpGlobals->time + frandrange(0.2f, 0.5f);

	AITASK_BotUpdateAndClearTasks(pBot);

	AIPlayerSetPrimaryMarineTask(pBot, &pBot->PrimaryBotTask);
	AIPlayerSetSecondaryMarineTask(pBot, &pBot->SecondaryBotTask);

	pBot->CurrentTask = AIPlayerGetNextTask(pBot);

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}

	if (pBot->DesiredCombatWeapon == WEAPON_NONE)
	{
		pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, nullptr);
	}
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
	if (Task->TaskType == TASK_GUARD) { return; }

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	Vector CommChairLocation = AITAC_GetCommChairLocation(BotTeam);

	DeployableSearchFilter StructureFilter;
	StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
	StructureFilter.DeployableTeam = BotTeam;
	StructureFilter.ReachabilityTeam = BotTeam;
	StructureFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;

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
		AITASK_SetSecureHiveTask(pBot, Task, NearestEmptyHive->HiveEntity->edict(), NearestEmptyHive->FloorLocation, false);
		return;
	}

	// Go to a good siege location if phase gates available

	if (AITAC_PhaseGatesAvailable(pBot->Player->GetTeam()))
	{
		const AvHAIHiveDefinition* ActiveHive = AITAC_GetActiveHiveNearestLocation(pBot->Edict->v.origin);

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

void AIPlayerSetSecondaryMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	// Find any nearby unbuilt structures
	DeployableSearchFilter UnbuiltFilter;
	UnbuiltFilter.DeployableTypes = SEARCH_ALL_MARINE_STRUCTURES;
	UnbuiltFilter.DeployableTeam = pBot->Player->GetTeam();
	UnbuiltFilter.ReachabilityTeam = pBot->Player->GetTeam();
	UnbuiltFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	UnbuiltFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_COMPLETED;
	UnbuiltFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);

	vector <AvHAIBuildableStructure*> BuildableStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &UnbuiltFilter);

	AvHAIBuildableStructure* NearestStructure = nullptr;
	float MinDist = 0.0f;

	for (auto it = BuildableStructures.begin(); it != BuildableStructures.end(); it++)
	{
		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), (*it)->Location, UTIL_MetresToGoldSrcUnits(5.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		int NumDesiredBuilders = (vDist2DSq((*it)->Location, AITAC_GetCommChairLocation(pBot->Player->GetTeam())) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f))) ? 1 : 2;

		if (NumBuilders < NumDesiredBuilders)
		{
			float ThisDist = vDist2DSq((*it)->Location, pBot->Edict->v.origin);
			if (!NearestStructure || ThisDist < MinDist)
			{
				NearestStructure = (*it);
				MinDist = ThisDist;
			}
		}
	}

	if (NearestStructure)
	{
		AITASK_SetBuildTask(pBot, Task, NearestStructure->edict, false);
		return;
	}

}

void AIPlayerNSAlienThink(AvHAIPlayer* pBot)
{
	UpdateAIAlienPlayerNSRole(pBot);
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

	AvHAIWeapon DesiredWeapon = (pBot->DesiredMoveWeapon != WEAPON_NONE) ? pBot->DesiredMoveWeapon : pBot->DesiredCombatWeapon;

	if (DesiredWeapon != WEAPON_NONE && GetPlayerCurrentWeapon(pBot->Player) != DesiredWeapon)
	{
		BotSwitchToWeapon(pBot, DesiredWeapon);
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
	return (IsPlayerActiveInGame(pBot->Edict) || IsPlayerCommander(pBot->Edict)) && !IsPlayerGestating(pBot->Edict);
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
			AITASK_SetSecureHiveTask(pBot, &pBot->CommanderTask, HiveRef->HiveEntity->edict(), Destination, false);
			pBot->CommanderTask.bIssuedByCommander = true;
			return;
		}
	}

	// Otherwise, treat as a normal move order. Go there and wait a bit to see what the commander wants to do next
	AITASK_SetMoveTask(pBot, &pBot->CommanderTask, Destination, true);
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
