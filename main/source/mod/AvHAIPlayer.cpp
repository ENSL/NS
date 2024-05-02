
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

#include "AvHGamerules.h"
#include "AvHMessage.h"
#include "AvHTurret.h"

extern nav_mesh NavMeshes[MAX_NAV_MESHES]; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)
extern nav_profile BaseNavProfiles[MAX_NAV_PROFILES]; // Array of nav profiles

extern cvar_t avh_botdebugmode;

#ifdef BOTDEBUG
extern edict_t* DebugBots[MAX_PLAYERS];
#endif

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
		if (pBot->TrackedEnemies[i].PlayerEdict == Target)
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

	//Vector TargetVelocity = (TrackedEnemyRef) ? TrackedEnemyRef->LastSeenVelocity : pBot->LookTarget->v.velocity;
	//Vector TargetLocation = (TrackedEnemyRef) ? TrackedEnemyRef->LastSeenLocation : UTIL_GetCentreOfEntity(pBot->LookTarget);

	Vector TargetVelocity = pBot->LookTarget->v.velocity;
	Vector TargetLocation = UTIL_GetCentreOfEntity(pBot->LookTarget);

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
		//pBot->Button |= IN_USE;
		pBot->LastUseTime = gpGlobals->time;

		CBaseEntity* UsedObject = CBaseEntity::Instance(Target);

		if (UsedObject)
		{
			UsedObject->Use(pBot->Player, pBot->Player, USE_TOGGLE, 0);
		}

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

		if (Dot >= 0.95f)
		{
			// Just give the bot a nudge and make sure they don't miss and end up somewhere they don't want to be
			float MoveSpeed = vSize2D(pBot->Edict->v.velocity);
			Vector NewVelocity = MoveDir * MoveSpeed;
			NewVelocity.z = pBot->Edict->v.velocity.z;

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

	if (IsPlayerReloading(pBot->Player))
	{
		pBot->DesiredCombatWeapon = GetPlayerCurrentWeapon(pBot->Player);
		return true;
	}

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

		bool bPlayerCloaked = pBot->Player->GetOpacity() < 0.5f && !GetHasUpgrade(pBot->Edict->v.iuser4, MASK_SENSORY_NEARBY);

		if (IsPlayerLerk(pBot->Edict) || bPlayerCloaked)
		{
			if (AITAC_ShouldBotBeCautious(pBot))
			{
				if (bPlayerCloaked)
				{
					pBot->BotNavInfo.bShouldWalk = true;
				}

				MoveTo(pBot, AttackPoint, MOVESTYLE_AMBUSH, 100.0f);
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

	Vector GetPosition = pBot->Player->GetGunPosition();

	bWillHit = (AimDot >= MinAcceptableAccuracy);

	if (!bWillHit && IsHitscanWeapon(CurrentWeapon))
	{

		edict_t* HitEntity = UTIL_TraceEntity(pBot->Edict, pBot->Player->GetGunPosition(), pBot->Player->GetGunPosition() + (AimDir * GetMaxIdealWeaponRange(CurrentWeapon)));

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
		if (vDist3DSq(pBot->Edict->v.origin, Target->v.origin) < sqrf(BALANCE_VAR(kGrenadeRadius)))
		{
			Vector BackDir = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - Target->v.origin);
			pBot->desiredMovementDir = BackDir;
		}

		Vector GrenadeLoc = UTIL_GetGrenadeThrowTarget(pBot->Edict, Target->v.origin, BALANCE_VAR(kGrenadeRadius), true);

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

	if (CurrentWeapon == WEAPON_INVALID) { return; }

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

	Vector EvolvePoint = AdjustPointForPathfinding(DesiredEvolveLocation, GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

	if (vIsZero(EvolvePoint))
	{
		EvolvePoint = UTIL_ProjectPointToNavmesh(DesiredEvolveLocation, Vector(500.0f, 500.0f, 500.0f), GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));
	}

	if (vIsZero(EvolvePoint))
	{
		EvolvePoint = UTIL_AdjustPointAwayFromNavWall(pBot->CurrentFloorPosition, 50.0f);
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
	Vector EvolvePoint = UTIL_ProjectPointToNavmesh(DesiredEvolveLocation, Vector(500.0f, 500.0f, 500.0f), GetBaseNavProfile(STRUCTURE_BASE_NAV_PROFILE));

	if (vIsZero(EvolvePoint))
	{
		EvolvePoint = UTIL_AdjustPointAwayFromNavWall(pBot->CurrentFloorPosition, (GetPlayerRadius(pBot->Edict) * 2.0f));
	}

	if (vDist2DSq(pBot->CurrentFloorPosition, EvolvePoint) > sqrf(8.0f))
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
		pBot->ViewInterpolationSpeed = 350.0f;

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
		pBot->ViewInterpolationSpeed = 175.0f;

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
		pBot->ViewInterpolationSpeed = 35.0f;

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

float BotRateEnemyThreat(AvHAIPlayer* pBot, enemy_status* TrackingInfo)
{
	if (TrackingInfo->AwarenessOfPlayer < 0.01f) { return 0.0f; }

	float Result = TrackingInfo->AwarenessOfPlayer;

	if (gpGlobals->time - TrackingInfo->LastVisibleTime < 2.0f)
	{
		Result += 1.0f;
	}

	if (TrackingInfo->bEnemyHasLOS)
	{
		Result += 1.0f;
	}

	Result += 1.0f - (vDist3D(TrackingInfo->LastDetectedLocation, pBot->Edict->v.origin) / UTIL_MetresToGoldSrcUnits(20.0f));

	return Result;
}

void BotUpdateView(AvHAIPlayer* pBot)
{
	int visibleCount = 0;

	bool bEnemyHasLOSToBot = false;

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	pBot->ViewForwardVector = UTIL_GetForwardVector(pBot->Edict->v.v_angle);

	UpdateAIPlayerViewFrustum(pBot);

	float ViewUpdateDelta = gpGlobals->time - pBot->LastViewUpdateTime;
	ViewUpdateDelta = clampf(ViewUpdateDelta, 0.0f, 0.2f);

	// Update list of currently visible players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);
		int EnemyIndex = i - 1;

		enemy_status* TrackingInfo = &pBot->TrackedEnemies[EnemyIndex];

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || !IsPlayerActiveInGame(PlayerEdict) || PlayerEdict->v.team == pBot->Edict->v.team)
		{
			BotClearEnemyTrackingInfo(TrackingInfo);
			continue;
		}

		AvHPlayer* PlayerRef = dynamic_cast<AvHPlayer*>(CBaseEntity::Instance(PlayerEdict));

		if (!PlayerRef)
		{
			BotClearEnemyTrackingInfo(TrackingInfo);
			continue;
		}

		TrackingInfo->PlayerRef = PlayerRef;
		TrackingInfo->PlayerEdict = PlayerEdict;
		
		TrackingInfo->AwarenessOfPlayer -= (ViewUpdateDelta * 0.1f);
		TrackingInfo->AwarenessOfPlayer = clampf(TrackingInfo->AwarenessOfPlayer, 0.0f, 1.0f);

		bool bInFOV = IsPlayerInBotFOV(pBot, PlayerEdict);

		if (TrackingInfo->AwarenessOfPlayer < 0.01f)
		{
			BotClearEnemyTrackingInfo(TrackingInfo);

			if (!bInFOV)
			{
				continue;
			}
		}

		Vector VisiblePoint = GetVisiblePointOnPlayerFromObserver(pBot->Edict, PlayerEdict);
		TrackingInfo->VisiblePointOnPlayer = VisiblePoint;

		Vector EnemyVisiblePoint = GetVisiblePointOnPlayerFromObserver(PlayerEdict, pBot->Edict);
		bool bEnemyCanSee = !vIsZero(EnemyVisiblePoint);
		if (!bEnemyCanSee)
		{
			TrackingInfo->LastCoverPosition = pBot->CurrentFloorPosition;
		}

		bool bHasLOS = !vIsZero(VisiblePoint);
		bool bIsPlayerInvisible = UTIL_IsCloakedPlayerInvisible(pBot->Edict, PlayerRef);

		bool bIsTracked = PlayerRef->GetOpacity() > 0.1f && (!bHasLOS && (IsPlayerParasited(PlayerEdict) || (GetHasUpgrade(pBot->Edict->v.iuser4, MASK_UPGRADE_8) && IsPlayerMotionTracked(PlayerEdict))));		

		TrackingInfo->bHasLOS = bHasLOS;
		TrackingInfo->bEnemyHasLOS = bEnemyCanSee;

		// Enemy is visible if they're in our FOV, and either they're pinged by motion tracking, or they are uncloaked and in our line of sight
		bool bIsSighted = bInFOV && ((bHasLOS && !bIsPlayerInvisible) || bIsTracked);

		if (bIsSighted)
		{
			TrackingInfo->LastDetectedTime = gpGlobals->time;
			TrackingInfo->LastDetectedLocation = PlayerEdict->v.origin;
			TrackingInfo->AwarenessOfPlayer = 1.0f;
			
			if (bHasLOS && !bIsPlayerInvisible)
			{
				TrackingInfo->LastVisibleLocation = PlayerEdict->v.origin;
				TrackingInfo->LastLOSPosition = pBot->Edict->v.origin;
				TrackingInfo->LastVisibleTime = gpGlobals->time;
			}

			if (TrackingInfo->AwarenessOfPlayer < 0.5f)
			{
				TrackingInfo->InitialAwarenessTime = gpGlobals->time;
			}
		}
		else
		{
			if (!bHasLOS)
			{
				TrackingInfo->LastCoverPosition = pBot->CurrentFloorPosition;
			}
		}

		bool bIsBotCloaked = UTIL_IsCloakedPlayerInvisible(PlayerEdict, pBot->Player);

		if (!bIsBotCloaked && bEnemyCanSee)
		{
			pBot->LastSafeLocation = ZERO_VECTOR;
			bEnemyHasLOSToBot = true;
		}

		TrackingInfo->EnemyThreatLevel = BotRateEnemyThreat(pBot, TrackingInfo);

	}

	pBot->DangerTurrets.clear();

	bool bIsInRangeOfEnemyTurret = false;

	DeployableSearchFilter EnemyTurretFilter;
	EnemyTurretFilter.DeployableTeam = EnemyTeam;
	EnemyTurretFilter.ExcludeStatusFlags = (STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_DISABLED);

	if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
	{
		EnemyTurretFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;
		EnemyTurretFilter.MaxSearchRadius = 700.0f; // For some reason, offence chambers have a hard-coded range
	}
	else
	{
		EnemyTurretFilter.DeployableTypes = STRUCTURE_MARINE_TURRET;
		EnemyTurretFilter.MaxSearchRadius = BALANCE_VAR(kTurretRange);
	}

	vector<AvHAIBuildableStructure> EligibleTurrets = AITAC_FindAllDeployables(pBot->Edict->v.origin, &EnemyTurretFilter);

	for (auto it = EligibleTurrets.begin(); it != EligibleTurrets.end(); it++)
	{
		AvHAIBuildableStructure ThisTurret = (*it);
		AvHTurret* TurretRef = dynamic_cast<AvHTurret*>(ThisTurret.EntityRef);

		if (TurretRef && TurretRef->GetIsValidTarget(pBot->Player))
		{
			bIsInRangeOfEnemyTurret = true;
			pBot->DangerTurrets.push_back(ThisTurret);
		}

	}	

	if (!bEnemyHasLOSToBot && !bIsInRangeOfEnemyTurret)
	{
		pBot->LastSafeLocation = pBot->CurrentFloorPosition;
	}

	
}

bool UTIL_IsCloakedPlayerInvisible(edict_t* Observer, AvHPlayer* Player)
{
	if (Player->GetOpacity() > 0.6f) { return false; }

	if (Player->GetIsCloaked()) { return true; }

	switch (Player->GetUser3())
	{
		case AVH_USER3_ALIEN_PLAYER1:
		case AVH_USER3_ALIEN_PLAYER2:
		case AVH_USER3_ALIEN_PLAYER3:
		{
			if (Player->GetOpacity() < 0.3f) { return true; }

			return (vDist3DSq(Observer->v.origin, Player->pev->origin) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)) || Player->pev->velocity.Length2D() < 50.0f);
		}
		case AVH_USER3_ALIEN_PLAYER4:
		case AVH_USER3_ALIEN_PLAYER5:
		{
			if (Player->GetOpacity() > 0.4f) { return false; }
			if (Player->GetOpacity() < 0.2f) { return true; }

			return vDist3DSq(Observer->v.origin, Player->pev->origin) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f));
		}
	}

	return false;
}

void BotClearEnemyTrackingInfo(enemy_status* TrackingInfo)
{
	AvHPlayer* PlayerRef = TrackingInfo->PlayerRef;
	edict_t* PlayerEdict = TrackingInfo->PlayerEdict;

	memset(TrackingInfo, 0, sizeof(enemy_status));

	TrackingInfo->PlayerRef = PlayerRef;
	TrackingInfo->PlayerEdict = PlayerEdict;
}

void UpdateAIPlayerViewFrustum(AvHAIPlayer* pBot)
{
	MAKE_VECTORS(pBot->Edict->v.v_angle);
	Vector up = gpGlobals->v_up;
	Vector forward = gpGlobals->v_forward;
	Vector right = gpGlobals->v_right;

	Vector fc = (pBot->Edict->v.origin + pBot->Edict->v.view_ofs) + (forward * BOT_MAX_VIEW);

	Vector fbl = fc + (up * f_ffheight / 2.0f) - (right * f_ffwidth / 2.0f);
	Vector fbr = fc + (up * f_ffheight / 2.0f) + (right * f_ffwidth / 2.0f);
	Vector ftl = fc - (up * f_ffheight / 2.0f) - (right * f_ffwidth / 2.0f);
	Vector ftr = fc - (up * f_ffheight / 2.0f) + (right * f_ffwidth / 2.0f);

	Vector nc = (pBot->Edict->v.origin + pBot->Edict->v.view_ofs) + (forward * BOT_MIN_VIEW);

	Vector nbl = nc + (up * f_fnheight / 2.0f) - (right * f_fnwidth / 2.0f);
	Vector nbr = nc + (up * f_fnheight / 2.0f) + (right * f_fnwidth / 2.0f);
	Vector ntl = nc - (up * f_fnheight / 2.0f) - (right * f_fnwidth / 2.0f);
	Vector ntr = nc - (up * f_fnheight / 2.0f) + (right * f_fnwidth / 2.0f);

	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_TOP], ftl, ntl, ntr);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_BOTTOM], fbr, nbr, nbl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_LEFT], fbl, nbl, ntl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_RIGHT], ftr, ntr, nbr);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_NEAR], nbr, ntr, ntl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_FAR], fbl, ftl, ftr);
}

bool IsPlayerInBotFOV(AvHAIPlayer* Observer, edict_t* TargetPlayer)
{
	/*Vector TargetVector = (TargetPlayer->v.origin - Observer->CurrentEyePosition).Normalize();

	float DotProduct = UTIL_GetDotProduct(Observer->ViewForwardVector, TargetVector);

	return DotProduct > 0.65f;*/

	if (FNullEnt(TargetPlayer) || !IsPlayerActiveInGame(TargetPlayer)) { return false; }
	// To make things a little more accurate, we're going to treat players as cylinders rather than boxes
	for (int i = 0; i < 6; i++)
	{
		// Our cylinder must be inside all planes to be visible, otherwise return false
		if (!UTIL_CylinderInsidePlane(&Observer->viewFrustum[i], TargetPlayer->v.origin - Vector(0, 0, 5), 60.0f, 16.0f))
		{
			return false;
		}
	}

	return true;

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

	// Skulks and Onos are long bois, so check to make sure they don't have their boots or snoots poking out round a corner...
	if (TargetClass == AVH_USER3_ALIEN_PLAYER1 || TargetClass == AVH_USER3_ALIEN_PLAYER5)
	{
		float Length = (TargetClass == AVH_USER3_ALIEN_PLAYER1) ? 55.0f : 110.0f;

		Vector ForwardVector = UTIL_GetForwardVector(TargetPlayer->v.angles);

		Vector MinLoc = TargetCentre - (ForwardVector * Length);

		UTIL_TraceLine(GetPlayerEyePosition(Observer), MinLoc, ignore_monsters, ignore_glass, Observer->v.pContainingEntity, &hit);

		if (hit.flFraction >= 1.0f) { return MinLoc; }

		Vector MaxLoc = TargetCentre + (ForwardVector * Length);

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
			 	//CLIENT_COMMAND(pBot->Edict, "say_team %s", pBot->ChatMessages[i].msg);
				AIPlayer_Say(pBot->Edict, 1, pBot->ChatMessages[i].msg);
			}
			else
			{
				//CLIENT_COMMAND(pBot->Edict, "say %s", pBot->ChatMessages[i].msg);
				AIPlayer_Say(pBot->Edict, 0, pBot->ChatMessages[i].msg);
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

	if (!pBot->CurrentTask)	{ pBot->CurrentTask = &pBot->PrimaryBotTask; }

	ClearBotInputs(pBot);
	pBot->CurrentEyePosition = GetPlayerEyePosition(pEdict);

	pBot->CurrentFloorPosition = UTIL_GetEntityGroundLocation(pEdict);

	if (vDist3DSq(pBot->BotNavInfo.LastNavMeshCheckPosition, pBot->CurrentFloorPosition) > sqrf(16.0f))
	{
		if (UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, AITAC_GetTeamStartingLocation(pBot->Player->GetTeam()), pBot->CurrentFloorPosition, 16.0f))
		{
			pBot->BotNavInfo.LastNavMeshPosition = pBot->CurrentFloorPosition;

			if (pBot->BotNavInfo.IsOnGround || IsPlayerLerk(pBot->Edict))
			{
				Vector ForwardVector = UTIL_GetForwardVector2D(pBot->Edict->v.angles);
				Vector RightVector = UTIL_GetCrossProduct(ForwardVector, UP_VECTOR);

				Vector TraceEndPoints[4];

				TraceEndPoints[0] = pBot->Edict->v.origin + (ForwardVector * (GetPlayerRadius(pBot->Edict) * 2.0f));
				TraceEndPoints[1] = pBot->Edict->v.origin - (ForwardVector * (GetPlayerRadius(pBot->Edict) * 2.0f));
				TraceEndPoints[2] = pBot->Edict->v.origin + (RightVector * (GetPlayerRadius(pBot->Edict) * 2.0f));
				TraceEndPoints[3] = pBot->Edict->v.origin - (RightVector * (GetPlayerRadius(pBot->Edict) * 2.0f));

				int NumDirectionsChecked = 0;
				bool bHasRoom = true;

				while (NumDirectionsChecked < 4 && bHasRoom)
				{
					Vector EndTrace = TraceEndPoints[NumDirectionsChecked];
					Vector EndNavTrace = EndTrace;
					EndNavTrace.z = pBot->CollisionHullBottomLocation.z;

					if (!UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, EndTrace))
					{
						bHasRoom = false;
						break;
					}

					if (!UTIL_TraceNav(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, EndNavTrace, 0.0f))
					{
						bHasRoom = false;
						break;
					}

					NumDirectionsChecked++;
				}

				if (bHasRoom)
				{
					pBot->BotNavInfo.LastOpenLocation = pBot->CurrentFloorPosition;
				}

			}
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
	if (pBot->PrimaryBotTask.ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING)
	{
		if (pBot->PrimaryBotTask.ActiveBuildInfo.AttemptedStructureType == STRUCTURE_ALIEN_HIVE)
		{
			// Give a 3-second grace period to check if the hive placement was successful
			if ((gpGlobals->time - pBot->PrimaryBotTask.ActiveBuildInfo.BuildAttemptTime) > 3.0f)
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(pBot->PrimaryBotTask.ActiveBuildInfo.AttemptedLocation);

				pBot->PrimaryBotTask.ActiveBuildInfo.BuildStatus = (NearestHive->Status != HIVE_STATUS_UNBUILT) ? BUILD_ATTEMPT_SUCCESS : BUILD_ATTEMPT_FAILED;
			}
		}
		else
		{
			// All other structures should appear near-instantly
			if ((gpGlobals->time - pBot->PrimaryBotTask.ActiveBuildInfo.BuildAttemptTime) > 1.0f)
			{
				pBot->PrimaryBotTask.ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_FAILED;
			}
		}
	}

	// If we tried placing a building as gorge, and nothing has appeared within the expected time, then mark it as a failed attempt.
	if (pBot->SecondaryBotTask.ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING)
	{
		if (pBot->SecondaryBotTask.ActiveBuildInfo.AttemptedStructureType == STRUCTURE_ALIEN_HIVE)
		{
			// Give a 3-second grace period to check if the hive placement was successful
			if ((gpGlobals->time - pBot->SecondaryBotTask.ActiveBuildInfo.BuildAttemptTime) > 3.0f)
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(pBot->SecondaryBotTask.ActiveBuildInfo.AttemptedLocation);

				pBot->SecondaryBotTask.ActiveBuildInfo.BuildStatus = (NearestHive->Status != HIVE_STATUS_UNBUILT) ? BUILD_ATTEMPT_SUCCESS : BUILD_ATTEMPT_FAILED;
			}
		}
		else
		{
			// All other structures should appear near-instantly
			if ((gpGlobals->time - pBot->SecondaryBotTask.ActiveBuildInfo.BuildAttemptTime) > 1.0f)
			{
				pBot->SecondaryBotTask.ActiveBuildInfo.BuildStatus = BUILD_ATTEMPT_FAILED;
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
	// Test Combat Stuff
	//DEBUG_PrintCombatInfo(INDEXENT(1), pBot);

	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

	if (pBot->CurrentEnemy > -1)
	{
		if (IsPlayerMarine(pBot->Edict))
		{
			MarineCombatThink(pBot);
		}
		else
		{
			AlienCombatThink(pBot);
		}
	}
	else
	{
		if (AITAC_ShouldBotBeCautious(pBot))
		{
			MoveTo(pBot, AITAC_GetTeamStartingLocation(AIMGR_GetEnemyTeam(pBot->Player->GetTeam())), MOVESTYLE_AMBUSH);
		}
		else
		{
			MoveTo(pBot, AITAC_GetTeamStartingLocation(AIMGR_GetEnemyTeam(pBot->Player->GetTeam())), MOVESTYLE_NORMAL);
		}
		
	}

	/*AITASK_BotUpdateAndClearTasks(pBot);

	if (pBot->PrimaryBotTask.TaskType != TASK_SECURE_HIVE)
	{
		AvHTeamNumber BotTeam = pBot->Player->GetTeam();
		AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

		AvHAIHiveDefinition* HiveToClear = nullptr;
		float MinDist = 0.0f;

		vector<AvHAIHiveDefinition*> Hives = AITAC_GetAllHives();

		for (auto it = Hives.begin(); it != Hives.end(); it++)
		{
			AvHAIHiveDefinition* ThisHive = (*it);

			if (ThisHive->Status == HIVE_STATUS_UNBUILT)
			{
				DeployableSearchFilter EnemyStuffFilter;
				EnemyStuffFilter.DeployableTeam = EnemyTeam;
				EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
				EnemyStuffFilter.ReachabilityTeam = BotTeam;
				EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
				EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

				if (AITAC_DeployableExistsAtLocation(ThisHive->FloorLocation, &EnemyStuffFilter))
				{
					float ThisDist = vDist2DSq(pBot->Edict->v.origin, ThisHive->FloorLocation);

					if (!HiveToClear || ThisDist < MinDist)
					{
						HiveToClear = ThisHive;
						MinDist = ThisDist;
					}
				}
			}
		}

		if (HiveToClear)
		{
			AITASK_SetSecureHiveTask(pBot, &pBot->PrimaryBotTask, HiveToClear->HiveEdict, HiveToClear->FloorLocation, true);
		}
	}
	else
	{
		BotProgressTask(pBot, &pBot->PrimaryBotTask);
	}*/
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
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	if (AIMGR_GetNumAIPlayersWithRoleOnTeam(BotTeam, BOT_ROLE_SWEEPER, pBot) == 0)
	{
		SetNewAIPlayerRole(pBot, BOT_ROLE_SWEEPER);
		return;
	}

	if (IsPlayerAlien(pBot->Edict))
	{
		if (IsPlayerLerk(pBot->Edict))
		{
			SetNewAIPlayerRole(pBot, BOT_ROLE_HARASS);
			return;
		}

		if (CONFIG_IsLerkAllowed())
		{

			int NumLerks = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER3, pBot->Edict);
			int NumHarassers = AIMGR_GetNumAIPlayersWithRoleOnTeam(BotTeam, BOT_ROLE_HARASS, pBot);

			if (NumLerks + NumHarassers == 0)
			{
				float LastSeenTime;
				edict_t* PreviousLerk = AITAC_GetLastSeenLerkForTeam(BotTeam, LastSeenTime);

				// We only go lerk if the last lerk we had in the match was either us, or we've not had another lerk in 30 seconds
				// This prevents a situation where a human is lerk, gets killed, and a bot immediately takes over.
				// This is undesireable as it pressures the human to pick something else to avoid too many lerks
				if (FNullEnt(PreviousLerk) || (gpGlobals->time - LastSeenTime > CONFIG_GetLerkCooldown()) || PreviousLerk == pBot->Edict)
				{
					SetNewAIPlayerRole(pBot, BOT_ROLE_HARASS);
					return;
				}
			}
		}

		if (CONFIG_IsOnosAllowed())
		{
			int MaxOnos = (int)(ceilf((float)(AIMGR_GetNumPlayersOnTeam(BotTeam) - 2)) * 0.3f);

			if (AIMGR_GetNumAIPlayersWithRoleOnTeam(BotTeam, BOT_ROLE_BOMBARDIER, pBot) < MaxOnos)
			{
				SetNewAIPlayerRole(pBot, BOT_ROLE_BOMBARDIER);
				return;
			}
		}
	}

	SetNewAIPlayerRole(pBot, BOT_ROLE_ASSAULT);
		
}

void UpdateAIPlayerDMRole(AvHAIPlayer* pBot)
{

}

void AIPlayerHearEnemy(AvHAIPlayer* pBot, edict_t* HeardEnemy, float SoundVolume)
{
	int heardIndex = ENTINDEX(HeardEnemy) - 1;

	if (heardIndex < 0 || heardIndex >= 32 || HeardEnemy->v.team == pBot->Edict->v.team) { return; }

	enemy_status* HeardEnemyStatus = &pBot->TrackedEnemies[heardIndex];
	
	HeardEnemyStatus->AwarenessOfPlayer += SoundVolume;

	HeardEnemyStatus->AwarenessOfPlayer = clampf(HeardEnemyStatus->AwarenessOfPlayer, 0.0f, 1.0f);

	if (HeardEnemyStatus->AwarenessOfPlayer < 0.15f)
	{
		return;
	}

	HeardEnemyStatus->LastDetectedTime = gpGlobals->time;

	bool bRecentlySeenEnemy = (gpGlobals->time - HeardEnemyStatus->LastVisibleTime) < 3.0f;

	// If the bot can't see the enemy (bCurrentlyVisible is false) then set the last seen location to a random point in the vicinity so the bot doesn't immediately know where they are
	if (bRecentlySeenEnemy || HeardEnemyStatus->AwarenessOfPlayer > 0.75f || vDist2DSq(HeardEnemy->v.origin, pBot->Edict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
	{
		HeardEnemyStatus->LastDetectedLocation = HeardEnemy->v.origin;
	}
	else
	{
		// The further the enemy is, the more inaccurate the bot's guess will be where they are
		HeardEnemyStatus->LastDetectedLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(SKULK_BASE_NAV_PROFILE), HeardEnemy->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
	}

}

void AIPlayerTakeDamage(AvHAIPlayer* pBot, int damageTaken, edict_t* aggressor)
{
	int aggressorIndex = ENTINDEX(aggressor) - 1;

	if (aggressorIndex < 0 || aggressorIndex >= 32) { return; }

	if (aggressor->v.team != pBot->Edict->v.team && IsPlayerActiveInGame(aggressor))
	{
		enemy_status* TrackingInfo = &pBot->TrackedEnemies[aggressorIndex];

		TrackingInfo->LastDetectedTime = gpGlobals->time;

		bool bRecentlySeenEnemy = gpGlobals->time - TrackingInfo->LastVisibleTime < 3.0f;

		// If the bot can't see the enemy (bCurrentlyVisible is false) then set the last seen location to a random point in the vicinity so the bot doesn't immediately know where they are
		if (bRecentlySeenEnemy || vDist2DSq(aggressor->v.origin, pBot->Edict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
		{
			TrackingInfo->LastDetectedLocation = aggressor->v.origin;
		}
		else
		{
			// The further the enemy is, the more inaccurate the bot's guess will be where they are
			TrackingInfo->LastDetectedLocation = UTIL_GetRandomPointOnNavmeshInRadius(GetBaseNavProfile(SKULK_BASE_NAV_PROFILE), aggressor->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
		}

		TrackingInfo->AwarenessOfPlayer = 1.0f;
		TrackingInfo->bHasLOS = true;
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

	// If we have enough to go Fade or Onos, then always go assault if we're the only one capable of going that class.
	// This check is to ensure we don't go lerk, or go cap resources, or build stuff when our team doesn't have any big hitters and we could fill that gap
	if ((CONFIG_IsFadeAllowed() || CONFIG_IsOnosAllowed()) && pBot->Player->GetResources() > BALANCE_VAR(kFadeCost))
	{
		bool bCheckOnos = CONFIG_IsOnosAllowed() && pBot->Player->GetResources() > BALANCE_VAR(kOnosCost);

		// First check we don't have any fades or onos already on our team
		bool bOtherPlayer = AITAC_GetNumPlayersOnTeamOfClass(pBot->Player->GetTeam(), (bCheckOnos) ? AVH_USER3_ALIEN_PLAYER5 : AVH_USER3_ALIEN_PLAYER4, pBot->Edict);

		if (!bOtherPlayer)
		{
			// If not, then check all the other bots to see if any of them plan to go fade/onos
			vector<AvHAIPlayer*> TeamBots = AIMGR_GetAIPlayersOnTeam(pBot->Player->GetTeam());

			bool bOtherPotentialPlayer = false;

			float Cost = (bCheckOnos) ? BALANCE_VAR(kOnosCost) : BALANCE_VAR(kFadeCost);

			for (auto it = TeamBots.begin(); it != TeamBots.end(); it++)
			{
				AvHAIPlayer* ThisPlayer = (*it);
				if (ThisPlayer->Edict == pBot->Edict) { continue; }

				if (ThisPlayer->Player->GetResources() > Cost && ThisPlayer->BotRole == BOT_ROLE_ASSAULT)
				{
					bOtherPotentialPlayer = true;
				}
			}

			// Nobody else is planning to, let's skip all other checks and go straight for assault class (which will make us evolve to fade/onos)
			if (!bOtherPotentialPlayer)
			{
				SetNewAIPlayerRole(pBot, BOT_ROLE_ASSAULT);
				return;
			}
		}
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

	int NumDesiredSweeperBots = 1;

	if (NumSweeperBots < 2)
	{
		DeployableSearchFilter DefenceStructuresFilter;
		DefenceStructuresFilter.DeployableTeam = BotTeamNumber;
		DefenceStructuresFilter.DeployableTypes = (STRUCTURE_MARINE_TURRET | STRUCTURE_MARINE_OBSERVATORY);
		DefenceStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		DefenceStructuresFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		DefenceStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

		if (!AITAC_DeployableExistsAtLocation(AITAC_GetTeamStartingLocation(BotTeamNumber), &DefenceStructuresFilter))
		{
			NumDesiredSweeperBots = 2;
		}
	}

	// Always have a sweeper
	if (NumSweeperBots < NumDesiredSweeperBots)
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
	int Result = -1;

	float HighestThreat = 0.0f;

	for (int i = 1; i < gpGlobals->maxClients; i++)
	{
		enemy_status* TrackingInfo = &pBot->TrackedEnemies[i - 1];

		if (TrackingInfo->EnemyThreatLevel < 0.1f) { continue; }

		float ThisThreat = TrackingInfo->EnemyThreatLevel;

		if (ThisThreat > HighestThreat)
		{
			Result = (i - 1);
			HighestThreat = ThisThreat;
		}
	}

	return Result;
}

AvHAICombatStrategy GetBotCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	if (FNullEnt(CurrentEnemy->PlayerEdict) || !IsPlayerActiveInGame(CurrentEnemy->PlayerEdict)) { return COMBAT_STRATEGY_IGNORE; }

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

	if (CurrentEnemy->EnemyThreatLevel < 1.0f)
	{
		return COMBAT_STRATEGY_IGNORE;
	}

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		if (pBot->CurrentTask->TaskType == TASK_DEFEND && UTIL_PlayerHasLOSToEntity(CurrentEnemy->PlayerEdict, pBot->CurrentTask->TaskTarget, UTIL_MetresToGoldSrcUnits(30.0f), false))
		{
			return COMBAT_STRATEGY_ATTACK;
		}

		if (pBot->CurrentTask->TaskType == TASK_EVOLVE)
		{
			if (CurrentEnemy->EnemyThreatLevel < 3.0f || !CurrentEnemy->bHasLOS)
			{
				return COMBAT_STRATEGY_IGNORE;
			}
		}

		if (CurrentEnemy->EnemyThreatLevel < 2.0f && gpGlobals->time - CurrentEnemy->LastVisibleTime > 5.0f)
		{
			return COMBAT_STRATEGY_IGNORE;
		}
	}


	float DistToEnemy = vDist3DSq(pBot->Edict->v.origin, CurrentEnemy->LastDetectedLocation);

	// Jig's up, just get in there
	if (CurrentEnemy->bHasLOS && DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		return COMBAT_STRATEGY_ATTACK;
	}

	// We're invisible, so go get them
	if (pBot->Player->GetOpacity() < 0.1f) { return COMBAT_STRATEGY_ATTACK; }

	bool bInAmbushRange = DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)) && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f));

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	int NumEnemyAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, CurrentEnemy->PlayerEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), CurrentEnemy->PlayerEdict);
	int NumFriends = AITAC_GetNumPlayersOnTeamWithLOS(BotTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), pBot->Edict);

	Vector EnemyFacing = UTIL_GetForwardVector2D(CurrentEnemy->PlayerEdict->v.angles);
	Vector OurOrientation = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - CurrentEnemy->PlayerEdict->v.origin);

	float FacingDot = UTIL_GetDotProduct2D(EnemyFacing, OurOrientation);

	bool bIsEnemyDistracted = FacingDot < 0.4f || CurrentEnemy->PlayerEdict->v.weaponmodel == 0 || IsPlayerReloading(pBot->Player);

	if ((NumEnemyAllies <= NumFriends || NumFriends >= 2) || (NumFriends >= NumEnemyAllies - 1 && bIsEnemyDistracted))
	{
		return COMBAT_STRATEGY_ATTACK;
	}

	Vector EnemyVelocity = UTIL_GetVectorNormal2D(CurrentEnemy->PlayerEdict->v.velocity);

	float MoveDot = UTIL_GetDotProduct2D(EnemyVelocity, OurOrientation);

	if (DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) || MoveDot > 0.0f)
	{
		return COMBAT_STRATEGY_AMBUSH;
	}
	else
	{
		return COMBAT_STRATEGY_SKIRMISH;
	}
}

AvHAICombatStrategy GetGorgeCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	float CurrentHealthPercent = GetPlayerOverallHealthPercent(pBot->Edict);

	bool bPlayerCloaked = pBot->Player->GetOpacity() <= 0.5f;

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		if (CurrentHealthPercent < 0.99f)
		{
			return COMBAT_STRATEGY_RETREAT;
		}
	}

	if (CurrentEnemy->EnemyThreatLevel < 2.0f)
	{
		return COMBAT_STRATEGY_IGNORE;
	}

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		if (pBot->CurrentTask->TaskType == TASK_DEFEND && UTIL_PlayerHasLOSToEntity(CurrentEnemy->PlayerEdict, pBot->CurrentTask->TaskTarget, UTIL_MetresToGoldSrcUnits(30.0f), false))
		{
			return COMBAT_STRATEGY_ATTACK;
		}

		if (gpGlobals->time - CurrentEnemy->LastVisibleTime > 5.0f)
		{
			return COMBAT_STRATEGY_IGNORE;
		}
	}

	vector<AvHPlayer*> Allies = AITAC_GetAllPlayersOnTeamWithLOS(BotTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), pBot->Edict);
	bool bHasBackup = false;

	Vector EnemyLocation = CurrentEnemy->LastDetectedLocation;

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
		return (CurrentHealthPercent > 0.5f) ? COMBAT_STRATEGY_ATTACK : ((bPlayerCloaked) ? COMBAT_STRATEGY_RETREAT : COMBAT_STRATEGY_SKIRMISH);
	}
	else
	{
		return (CurrentHealthPercent > 0.7f) ? ((bPlayerCloaked) ? COMBAT_STRATEGY_RETREAT : COMBAT_STRATEGY_SKIRMISH) : COMBAT_STRATEGY_RETREAT;
	}
}

AvHAICombatStrategy GetLerkCombatStrategyForTarget(AvHAIPlayer* pBot, enemy_status* CurrentEnemy)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = CurrentEnemy->PlayerRef->GetTeam();

	float CurrentHealthPercent = GetPlayerOverallHealthPercent(pBot->Edict);

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		if (CurrentHealthPercent < 0.99f)
		{
			return COMBAT_STRATEGY_RETREAT;
		}
	}

	if (CurrentEnemy->EnemyThreatLevel < 0.75f)
	{
		return COMBAT_STRATEGY_IGNORE;
	}

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType == TASK_DEFEND)
	{
		if (CurrentEnemy->EnemyThreatLevel < 2.0f) { return COMBAT_STRATEGY_IGNORE; }

		if (UTIL_PlayerHasLOSToEntity(CurrentEnemy->PlayerEdict, pBot->CurrentTask->TaskTarget, UTIL_MetresToGoldSrcUnits(30.0f), false))
		{
			return COMBAT_STRATEGY_ATTACK;
		}
	}

	edict_t* EnemyEdict = CurrentEnemy->PlayerEdict;

	float EnemyHealthPercent = GetPlayerOverallHealthPercent(EnemyEdict);
	int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

	DeployableSearchFilter TurretFilter;
	TurretFilter.DeployableTeam = EnemyTeam;
	TurretFilter.DeployableTypes = (STRUCTURE_MARINE_TURRET | STRUCTURE_ALIEN_OFFENCECHAMBER);
	TurretFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	TurretFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
	TurretFilter.MaxSearchRadius = BALANCE_VAR(kTurretRange);

	vector<AvHAIBuildableStructure> Turrets = AITAC_FindAllDeployables(EnemyEdict->v.origin, &TurretFilter);

	for (auto it = Turrets.begin(); it != Turrets.end(); it++)
	{
		if (UTIL_QuickTrace(pBot->Edict, GetPlayerTopOfCollisionHull(it->edict), EnemyEdict->v.origin))
		{
			NumAllies++;
		}
	}

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, EnemyEdict->v.origin);

	float RetreatHealthPercent = (NumAllies > 1) ? 0.5f : 0.35f;

	if (CurrentHealthPercent < RetreatHealthPercent)
	{
		return COMBAT_STRATEGY_RETREAT;
	}

	// Player has a deadly weapon if they have a shotgun, or they have an HMG with ammo in the chamber and are not reloading (we can strike if they are!)
	bool bEnemyHasDeadlyWeapon = (PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_HMG) && UTIL_GetPlayerPrimaryWeaponClipAmmo(CurrentEnemy->PlayerRef) > 10 && !IsPlayerReloading(CurrentEnemy->PlayerRef)) || PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_SHOTGUN);

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

	edict_t* EnemyEdict = CurrentEnemy->PlayerEdict;

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
			if (PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_HMG) || PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_SHOTGUN))
			{
				MinHealthPercent += 0.15f;
			}

			int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(CurrentEnemy->PlayerRef->GetTeam(), EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

			if (NumAllies > 0)
			{
				MinHealthPercent += (0.15f * (float)NumAllies);
			}

			MinHealthPercent = clampf(MinHealthPercent, 0.0f, 0.99f);

			// We don't feel strong enough to tackle the challenge yet
			if (CurrentHealthPercent < MinHealthPercent) { return COMBAT_STRATEGY_RETREAT; }
		}
	}

	if (CurrentEnemy->EnemyThreatLevel < 2.0f)
	{
		return COMBAT_STRATEGY_IGNORE;
	}

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		if (pBot->CurrentTask->TaskType == TASK_DEFEND && UTIL_PlayerHasLOSToEntity(CurrentEnemy->PlayerEdict, pBot->CurrentTask->TaskTarget, UTIL_MetresToGoldSrcUnits(30.0f), false))
		{
			return COMBAT_STRATEGY_ATTACK;
		}

		if (gpGlobals->time - CurrentEnemy->LastVisibleTime > 5.0f)
		{
			return COMBAT_STRATEGY_IGNORE;
		}
	}

	// Player has a deadly weapon if they have a shotgun, or they have an HMG with ammo in the chamber and are not reloading (we can strike if they are!)
	bool bEnemyHasDeadlyWeapon = (PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_HMG) && UTIL_GetPlayerPrimaryWeaponClipAmmo(CurrentEnemy->PlayerRef) > 10 && !IsPlayerReloading(CurrentEnemy->PlayerRef)) || PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_SHOTGUN);

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, EnemyEdict->v.origin);

	int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

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
			if ((bEnemyHasDeadlyWeapon && (FacingDot > 0.5f || NumAllies > 0)) || NumAllies > 1)
			{
				return COMBAT_STRATEGY_SKIRMISH;
			}
		}
	}
	else
	{
		if ((bEnemyHasDeadlyWeapon && (FacingDot > 0.5f || NumAllies > 0)) || NumAllies > 1)
		{
			Vector EnemyVelocity = UTIL_GetVectorNormal2D(EnemyEdict->v.velocity);

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

	edict_t* EnemyEdict = CurrentEnemy->PlayerEdict;

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
			if (PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_HMG) || PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_SHOTGUN))
			{
				MinHealthPercent += 0.1f;
			}

			int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(CurrentEnemy->PlayerRef->GetTeam(), EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

			if (NumAllies > 0)
			{
				MinHealthPercent += (0.1f * (float)NumAllies);
			}

			MinHealthPercent = clampf(MinHealthPercent, 0.0f, 0.99f);

			// We don't feel strong enough to tackle the challenge yet
			if (CurrentHealthPercent < MinHealthPercent) { return COMBAT_STRATEGY_RETREAT; }
		}
	}

	if (CurrentEnemy->EnemyThreatLevel < 2.0f)
	{
		return COMBAT_STRATEGY_IGNORE;
	}

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		if (pBot->CurrentTask->TaskType == TASK_DEFEND && UTIL_PlayerHasLOSToEntity(CurrentEnemy->PlayerEdict, pBot->CurrentTask->TaskTarget, UTIL_MetresToGoldSrcUnits(30.0f), false))
		{
			return COMBAT_STRATEGY_ATTACK;
		}

		if (gpGlobals->time - CurrentEnemy->LastVisibleTime > 5.0f)
		{
			return COMBAT_STRATEGY_IGNORE;
		}
	}

	// Player has a deadly weapon if they have a shotgun, or they have an HMG with ammo in the chamber and are not reloading (we can strike if they are!)
	bool bEnemyHasDeadlyWeapon = (PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_HMG) && UTIL_GetPlayerPrimaryWeaponClipAmmo(CurrentEnemy->PlayerRef) > 10 && !IsPlayerReloading(CurrentEnemy->PlayerRef)) || PlayerHasWeapon(CurrentEnemy->PlayerRef, WEAPON_MARINE_SHOTGUN);

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, EnemyEdict->v.origin);

	int NumAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), EnemyEdict);

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

	edict_t* EnemyEdict = CurrentEnemy->PlayerEdict;

	float CurrentHealthPercent = (pBot->Edict->v.health / pBot->Edict->v.max_health);

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, CurrentEnemy->LastDetectedLocation);

	bool bCanRetreat = AITAC_IsCompletedStructureOfTypeNearLocation(BotTeam, (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY), ZERO_VECTOR, 0.0f);

	if (bCanRetreat && pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		int MinDesiredAmmo = imini(UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player), UTIL_GetPlayerPrimaryWeaponMaxClipSize(pBot->Player) * 2);

		if (CurrentHealthPercent < 0.5f || UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < MinDesiredAmmo)
		{
			return COMBAT_STRATEGY_RETREAT;
		}
	}

	// The idea here is to decide when to engage an enemy we're aware of. If we are doing a task that involves idling
	// such as guarding or securing an area, we're much more likely to go investigate potential threats
	// than if we have a more direct job to do like building something
	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		if (CurrentEnemy->EnemyThreatLevel < 1.0f) { return COMBAT_STRATEGY_IGNORE; }

		if (pBot->CurrentTask->TaskType == TASK_DEFEND && UTIL_PlayerHasLOSToEntity(CurrentEnemy->PlayerEdict, pBot->CurrentTask->TaskTarget, UTIL_MetresToGoldSrcUnits(30.0f), false))
		{
			return COMBAT_STRATEGY_ATTACK;
		}

		float ThreatThreshold = 2.0f;

		switch (pBot->CurrentTask->TaskType)
		{
			case TASK_CAP_RESNODE:
			case TASK_GUARD:
			case TASK_SECURE_HIVE:
				ThreatThreshold = (IsPlayerMarine(pBot->Edict)) ? 1.0f : 2.0f;
				break;
			default:
				break;

		}

		if (CurrentEnemy->EnemyThreatLevel < ThreatThreshold)
		{
			return COMBAT_STRATEGY_IGNORE;
		}
	}

	int NumEnemyAllies = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, EnemyEdict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), EnemyEdict);
	int NumFriendlies = AITAC_GetNumPlayersOnTeamWithLOS(BotTeam, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), pBot->Edict);

	if (bCanRetreat && (CurrentHealthPercent < 0.3f || (CurrentHealthPercent < 0.5f && NumEnemyAllies > 0) || UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryWeaponMaxClipSize(pBot->Player)))
	{
		return COMBAT_STRATEGY_RETREAT;
	}

	// We're out of ammo entirely and can't retreat, DEATH OR GLORY
	if (UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) == 0 && UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) == 0 && UTIL_GetPlayerSecondaryWeaponClipAmmo(pBot->Player) == 0 && UTIL_GetPlayerSecondaryAmmoReserve(pBot->Player) == 0)
	{
		return COMBAT_STRATEGY_ATTACK;
	}

	// Shotty users should attack, can't really skirmish with a shotgun
	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_SHOTGUN) && (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) > 0 || UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0))
	{
		return COMBAT_STRATEGY_ATTACK;
	}

	bool bIsEnemyRanged = IsPlayerMarine(CurrentEnemy->PlayerRef);

	// If we're up against a stronger enemy than us, skirmish instead to avoid getting stomped
	if (!PlayerHasHeavyArmour(pBot->Edict) && (CurrentEnemy->PlayerRef->GetUser3() > AVH_USER3_ALIEN_PLAYER3 || NumEnemyAllies > 0 || PlayerHasHeavyArmour(EnemyEdict)))
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

	if (pBot->CurrentEnemy > -1)
	{
		if (MarineCombatThink(pBot)) { return; }
	}

	if (gpGlobals->time >= pBot->BotNextTaskEvaluationTime)
	{
		pBot->BotNextTaskEvaluationTime = gpGlobals->time + frandrange(0.2f, 0.5f);

		AITASK_BotUpdateAndClearTasks(pBot);

		AIPlayerSetPrimaryMarineTask(pBot, &pBot->PrimaryBotTask);
		AIPlayerSetSecondaryMarineTask(pBot, &pBot->SecondaryBotTask);
		AIPlayerSetWantsAndNeedsMarineTask(pBot, &pBot->WantsAndNeedsTask);
	}

	pBot->CurrentTask = AIPlayerGetNextTask(pBot);

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}

	if (pBot->DesiredCombatWeapon == WEAPON_INVALID)
	{
		pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, nullptr);
	}
}

void MarineHuntEnemy(AvHAIPlayer* pBot, enemy_status* TrackedEnemy)
{
	/*edict_t* CurrentEnemy = TrackedEnemy->EnemyEdict;

	if (FNullEnt(CurrentEnemy) || IsPlayerDead(CurrentEnemy)) { return; }

	float TimeSinceLastSighting = (gpGlobals->time - TrackedEnemy->LastSeenTime);

	// If the enemy is being motion tracked, or the last seen time was within the last 5 seconds, and the suspected location is close enough, then throw a grenade!
	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GRENADE) || ((PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL) && (UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0 || UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) > 0))))
	{
		if (TimeSinceLastSighting < 5.0f && vDist3DSq(pBot->Edict->v.origin, TrackedEnemy->LastSeenLocation) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			Vector GrenadeThrowLocation = UTIL_GetGrenadeThrowTarget(pBot->Edict, TrackedEnemy->LastSeenLocation, BALANCE_VAR(kGrenadeRadius), false);

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

	return;*/
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

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].PlayerEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	AvHAIDroppedItem* NearestHealthPack = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_HEALTHPACK, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);
	AvHAIDroppedItem* NearestAmmoPack = AITAC_FindClosestItemToLocation(pBot->Edict->v.origin, DEPLOYABLE_ITEM_AMMO, BotTeam, pBot->BotNavInfo.NavProfile.ReachabilityFlag, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f), true);

	AvHAIWeapon DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, CurrentEnemy);

	bool bBotIsGrenadier = (DesiredCombatWeapon == WEAPON_MARINE_GL);

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, TrackedEnemyRef->LastDetectedLocation);

	bool bEnemyIsRanged = IsPlayerMarine(TrackedEnemyRef->PlayerRef) || ((GetPlayerCurrentWeapon(TrackedEnemyRef->PlayerRef) == WEAPON_FADE_ACIDROCKET) && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));

	float TimeLastDetected = gpGlobals->time - TrackedEnemyRef->LastDetectedTime;
	float TimeSinceLastSeenEnemy = gpGlobals->time - TrackedEnemyRef->LastVisibleTime;

	bool bEnemyVisible = TrackedEnemyRef->bHasLOS && TrackedEnemyRef->AwarenessOfPlayer >= 0.8f;

	// Run away and restock
	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_RETREAT)
	{
		pBot->DesiredCombatWeapon = DesiredCombatWeapon;

		if (NearestHealthPack && (pBot->Edict->v.health < pBot->Edict->v.max_health * 0.7f))
		{
			MoveTo(pBot, NearestHealthPack->Location, MOVESTYLE_NORMAL);
		}
		else if (NearestAmmoPack && UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player))
		{
			if (vDist2DSq(pBot->Edict->v.origin, NearestAmmoPack->Location) < sqrf(150.0f))
			{
				pBot->DesiredCombatWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);
			}
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

			AvHAIBuildableStructure NearestArmouryRef = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &NearestArmoury);

			if (NearestArmouryRef.IsValid() && (!IsAreaAffectedBySpores(NearestArmouryRef.Location) || PlayerHasHeavyArmour(pBot->Edict)))
			{
				if (!TrackedEnemyRef->bHasLOS || (IsPlayerAlien(pBot->Edict) && vDist2DSq(NearestArmouryRef.Location, CurrentEnemy->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))))
				{
					if (IsPlayerInUseRange(pBot->Edict, NearestArmouryRef.edict))
					{
						if (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player) || UTIL_GetPlayerSecondaryAmmoReserve(pBot->Player) >= UTIL_GetPlayerPrimaryWeaponMaxClipSize(pBot->Player))
						{
							pBot->DesiredCombatWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);
						}
						else
						{
							pBot->DesiredCombatWeapon = UTIL_GetPlayerSecondaryWeapon(pBot->Player);
						}
						BotUseObject(pBot, NearestArmouryRef.edict, true);
						BotReloadWeapons(pBot);						

						return true;
					}
				}

				MoveTo(pBot, NearestArmouryRef.Location, MOVESTYLE_NORMAL);

			}
			else
			{
				MoveTo(pBot, AITAC_GetTeamStartingLocation(BotTeam), MOVESTYLE_NORMAL);
			}
		}

		if (bEnemyVisible)
		{
			BotLookAt(pBot, CurrentEnemy);

			BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, TrackedEnemyRef->LastDetectedLocation, CurrentEnemy);

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
				if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GRENADE) || (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL) && UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0))
				{
					int NumViableTargets = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, CurrentEnemy->v.origin, BALANCE_VAR(kGrenadeRadius), CurrentEnemy);

					if (NumViableTargets > 0)
					{
						Vector GrenadeTarget = UTIL_GetGrenadeThrowTarget(pBot->Edict, TrackedEnemyRef->LastDetectedLocation, BALANCE_VAR(kGrenadeRadius), true);

						if (!vIsZero(GrenadeTarget))
						{
							BotThrowGrenadeAtTarget(pBot, GrenadeTarget);
							return true;
						}
					}
				}

				if (!bBotIsGrenadier || DistToEnemy > sqrf(BALANCE_VAR(kGrenadeRadius)))
				{
					BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
					return true;
				}
			}
		}
		else
		{
			if (TimeSinceLastSeenEnemy < 3.0f && !vIsZero(TrackedEnemyRef->LastVisibleLocation))
			{
				BotLookAt(pBot, TrackedEnemyRef->LastVisibleLocation);
				
			}
		}
		
		
		return true;
	}

	// Maintain distance, pop and shoot
	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_SKIRMISH)
	{
		pBot->DesiredCombatWeapon = DesiredCombatWeapon;

		if (vIsZero(pBot->LastSafeLocation))
		{
			pBot->LastSafeLocation = AITAC_GetTeamStartingLocation(BotTeam);
		}

		float DesiredDistance = GetMinIdealWeaponRange(DesiredCombatWeapon) + ((GetMaxIdealWeaponRange(DesiredCombatWeapon) - GetMinIdealWeaponRange(DesiredCombatWeapon)) * 0.5f);

		bool bCanReloadCurrentWeapon = (WeaponCanBeReloaded(DesiredCombatWeapon) && GetPlayerCurrentWeaponClipAmmo(pBot->Player) < GetPlayerCurrentWeaponMaxClipAmmo(pBot->Player) && GetPlayerCurrentWeaponReserveAmmo(pBot->Player) > 0);
		bool bMustReloadCurrentWeapon = bCanReloadCurrentWeapon && GetPlayerCurrentWeaponClipAmmo(pBot->Player) == 0;
		bool bCanReloadAnyWeapon = BotAnyWeaponNeedsReloading(pBot);

		if (bEnemyVisible)
		{
			if (bMustReloadCurrentWeapon)
			{
				if (TrackedEnemyRef->bHasLOS)
				{
					BotLookAt(pBot, TrackedEnemyRef->LastDetectedLocation);
				}
				else
				{
					Vector LookLocation = TrackedEnemyRef->LastLOSPosition;
					LookLocation.z = pBot->CurrentEyePosition.z;
					BotLookAt(pBot, LookLocation);
				}

				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
				BotReloadWeapons(pBot);
				return true;
			}

			if (vDist2DSq(pBot->Edict->v.origin, pBot->LastSafeLocation) > sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
			{
				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
			}
			
			if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GRENADE) || (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL) && UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0))
			{
				int NumViableTargets = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, CurrentEnemy->v.origin, BALANCE_VAR(kGrenadeRadius), CurrentEnemy);

				if (NumViableTargets > 0)
				{
					Vector GrenadeTarget = UTIL_GetGrenadeThrowTarget(pBot->Edict, TrackedEnemyRef->LastDetectedLocation, BALANCE_VAR(kGrenadeRadius), true);

					if (!vIsZero(GrenadeTarget))
					{
						BotThrowGrenadeAtTarget(pBot, GrenadeTarget);
						return true;
					}
				}
			}

			if (bEnemyIsRanged && !IsMeleeWeapon(DesiredCombatWeapon))
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
					if (!bEnemyIsRanged && DistToEnemy < sqrf(100.0f))
					{
						if (IsPlayerReloading(pBot->Player) && CanInterruptWeaponReload(GetPlayerCurrentWeapon(pBot->Player)) && GetPlayerCurrentWeaponClipAmmo(pBot->Player) > 0)
						{
							InterruptReload(pBot);
						}
						BotJump(pBot);
					}
				}
			}

			BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
		}
		else
		{
			if (BotReloadWeapons(pBot)) { return true; }

			if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GRENADE) || (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL) && UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0))
			{
				Vector GrenadeTarget = UTIL_GetGrenadeThrowTarget(pBot->Edict, TrackedEnemyRef->LastDetectedLocation, BALANCE_VAR(kGrenadeRadius), true);

				if (!vIsZero(GrenadeTarget))
				{
					BotThrowGrenadeAtTarget(pBot, GrenadeTarget);
					return true;
				}
			}			

			if (UTIL_PlayerHasLOSToLocation(pBot->Edict, TrackedEnemyRef->LastVisibleLocation, UTIL_MetresToGoldSrcUnits(30.0f)))
			{
				BotLookAt(pBot, TrackedEnemyRef->LastVisibleLocation);
			}
			MoveTo(pBot, TrackedEnemyRef->LastDetectedLocation, MOVESTYLE_NORMAL);
		}

		return true;
	}

	// Go for the kill. Maintain desired distance and pursue when needed
	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_ATTACK)
	{
		bool bCanUsePrimaryWeapon = UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0;
		pBot->DesiredCombatWeapon = (!bEnemyVisible && bCanUsePrimaryWeapon) ? UTIL_GetPlayerPrimaryWeapon(pBot->Player) : DesiredCombatWeapon;

		if (vIsZero(pBot->LastSafeLocation))
		{
			pBot->LastSafeLocation = AITAC_GetTeamStartingLocation(BotTeam);
		}

		float DesiredDistance = GetMinIdealWeaponRange(DesiredCombatWeapon) + ((GetMaxIdealWeaponRange(DesiredCombatWeapon) - GetMinIdealWeaponRange(DesiredCombatWeapon)) * 0.5f);

		bool bCanReloadCurrentWeapon = (WeaponCanBeReloaded(DesiredCombatWeapon) && GetPlayerCurrentWeaponClipAmmo(pBot->Player) < GetPlayerCurrentWeaponMaxClipAmmo(pBot->Player) && GetPlayerCurrentWeaponReserveAmmo(pBot->Player) > 0);
		bool bMustReloadCurrentWeapon = bCanReloadCurrentWeapon && GetPlayerCurrentWeaponClipAmmo(pBot->Player) == 0;
		bool bCanReloadAnyWeapon = BotAnyWeaponNeedsReloading(pBot);

		if (bMustReloadCurrentWeapon)
		{
			BotReloadWeapons(pBot);

			if (TrackedEnemyRef->bHasLOS)
			{
				BotLookAt(pBot, TrackedEnemyRef->LastDetectedLocation);
			}
			else
			{
				Vector LookLocation = TrackedEnemyRef->LastLOSPosition;
				LookLocation.z = pBot->CurrentEyePosition.z;
				BotLookAt(pBot, LookLocation);
			}

			if (bEnemyVisible)
			{
				MoveTo(pBot, pBot->LastSafeLocation, MOVESTYLE_NORMAL);
			}

			return true;
		}


		if (bEnemyVisible)
		{
			if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GRENADE) || (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_GL) && UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) > 0))
			{
				int NumViableTargets = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, CurrentEnemy->v.origin, BALANCE_VAR(kGrenadeRadius), CurrentEnemy);

				if (NumViableTargets > 0)
				{
					Vector GrenadeTarget = UTIL_GetGrenadeThrowTarget(pBot->Edict, TrackedEnemyRef->LastDetectedLocation, BALANCE_VAR(kGrenadeRadius), true);

					if (!vIsZero(GrenadeTarget))
					{
						BotThrowGrenadeAtTarget(pBot, GrenadeTarget);
						return true;
					}
				}
			}

			if (DesiredCombatWeapon != WEAPON_MARINE_KNIFE)
			{
				// If we're being attacked by skulks, then jump to get away
				if (CurrentEnemy->v.iuser3 == AVH_USER3_ALIEN_PLAYER1 && DistToEnemy < sqrf(64.0f))
				{
					if (IsPlayerReloading(pBot->Player) && CanInterruptWeaponReload(GetPlayerCurrentWeapon(pBot->Player)) && GetPlayerCurrentWeaponClipAmmo(pBot->Player) > 0)
					{
						InterruptReload(pBot);
					}
					BotJump(pBot);
				}
			}

			BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, CurrentEnemy);

			if (LOSCheck == ATTACK_SUCCESS)
			{
				BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);

				if (bEnemyIsRanged && !IsMeleeWeapon(DesiredCombatWeapon))
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

				if (!bEnemyIsRanged && !IsMeleeWeapon(DesiredCombatWeapon))
				{
					Vector EnemyMoveDir = UTIL_GetVectorNormal2D(pBot->Edict->v.velocity);
					Vector Orientation = UTIL_GetVectorNormal2D(pBot->Edict->v.origin - TrackedEnemyRef->LastDetectedLocation);

					if (UTIL_GetDotProduct2D(EnemyMoveDir, Orientation) > 0.5f)
					{
						if (UTIL_TraceNav(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, pBot->CurrentFloorPosition + (Orientation * 50.0f), 32.0f))
						{
							MoveDirectlyTo(pBot, pBot->CurrentFloorPosition + (Orientation * 50.0f));
						}
					}
				}

				return true;

			}
			else
			{
				if (bCanReloadCurrentWeapon && !bEnemyIsRanged && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
				{
					BotReloadWeapons(pBot);
				}
				else
				{
					MoveTo(pBot, TrackedEnemyRef->LastDetectedLocation, MOVESTYLE_NORMAL);
				}
			}

			return true;
		}

		if (bCanReloadCurrentWeapon && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)) && TimeSinceLastSeenEnemy > 2.0f)
		{
			BotReloadWeapons(pBot);
			return true;
		}

		if (!UTIL_PlayerHasLOSToLocation(pBot->Edict, TrackedEnemyRef->LastDetectedLocation, UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			MoveTo(pBot, TrackedEnemyRef->LastDetectedLocation, MOVESTYLE_NORMAL);
		}
		else
		{
			BotGuardLocation(pBot, TrackedEnemyRef->LastDetectedLocation);
		}		
		
		return true;
	}

	return false;
}


bool MarineCombatThink(AvHAIPlayer* pBot)
{
	if (pBot->CurrentEnemy > -1)
	{
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

	AvHAIBuildableStructure UnbuiltIP = AITAC_FindClosestDeployableToLocation(CommChairLocation, &StructureFilter);

	if (UnbuiltIP.IsValid() && (!IsAreaAffectedBySpores(UnbuiltIP.Location) || PlayerHasHeavyArmour(pBot->Edict)))
	{
		AITASK_SetBuildTask(pBot, Task, UnbuiltIP.edict, true);
		return;
	}

	StructureFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
	StructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

	AvHAIBuildableStructure UnbuiltStructure = AITAC_FindClosestDeployableToLocation(CommChairLocation, &StructureFilter);

	if (UnbuiltStructure.IsValid() && (!IsAreaAffectedBySpores(UnbuiltStructure.Location) || PlayerHasHeavyArmour(pBot->Edict)))
	{
		AITASK_SetBuildTask(pBot, Task, UnbuiltStructure.edict, true);
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

	AvHAIBuildableStructure AttackedStructure = AITAC_FindClosestDeployableToLocation(CommChairLocation, &AttackedStructureFilter);

	if (AttackedStructure.IsValid())
	{
		AITASK_SetDefendTask(pBot, Task, AttackedStructure.edict, true);
		return;
	}

	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
	{
		AttackedStructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_DAMAGED;

		AvHAIBuildableStructure AttackedStructure = AITAC_FindClosestDeployableToLocation(CommChairLocation, &AttackedStructureFilter);

		if (AttackedStructure.IsValid() && (!IsAreaAffectedBySpores(AttackedStructure.Location) || PlayerHasHeavyArmour(pBot->Edict)))
		{
			AITASK_SetWeldTask(pBot, Task, AttackedStructure.edict, true);
			return;
		}
	}


	StructureFilter.DeployableTypes = STRUCTURE_MARINE_PHASEGATE;
	StructureFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	StructureFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	if (AITAC_GetNumDeployablesNearLocation(CommChairLocation, &StructureFilter) < 2)
	{
		if (Task->TaskType != TASK_GUARD || vDist2DSq(Task->TaskLocation, CommChairLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			Task->TaskType = TASK_GUARD;
			Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, CommChairLocation, UTIL_MetresToGoldSrcUnits(10.0f));
			Task->bTaskIsUrgent = false;
			Task->TaskLength = frandrange(20.0f, 30.0f);
			Task->TaskStartedTime = 0.0f;
		}
		return;
	}

	AvHAIBuildableStructure NearestPG = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &StructureFilter);

	vector<AvHAIBuildableStructure> AllPG = AITAC_FindAllDeployables(pBot->Edict->v.origin, &StructureFilter);

	AvHAIBuildableStructure RandomPG;
	int HighestRand = 0;

	for (auto it = AllPG.begin(); it != AllPG.end(); it++)
	{
		AvHAIBuildableStructure ThisStruct = (*it);

		if (ThisStruct.edict == NearestPG.edict) { continue; }

		int ThisRand = irandrange(0, 100);

		if (FNullEnt(RandomPG.edict) || ThisRand > HighestRand)
		{
			RandomPG = ThisStruct;
			HighestRand = ThisRand;
		}
	}

	if (RandomPG.IsValid())
	{
		if (Task->TaskType != TASK_GUARD)
		{
			Task->TaskType = TASK_GUARD;
			Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, RandomPG.Location, UTIL_MetresToGoldSrcUnits(5.0f));
			Task->bTaskIsUrgent = false;
			Task->TaskLength = frandrange(20.0f, 30.0f);
			Task->TaskStartedTime = 0.0f;
		}
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
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	// Go attack sieged hive
	const AvHAIHiveDefinition* ActiveSiegeHive = AITAC_GetNearestHiveUnderActiveSiege(BotTeam, pBot->Edict->v.origin);

	if (ActiveSiegeHive)
	{
		AITASK_SetAttackTask(pBot, Task, ActiveSiegeHive->HiveEdict, false);
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

		if (AICOMM_IsHiveFullySecured(pBot, ThisHive, false)) { continue; }

		int NumMarinesSecuring = AITAC_GetNumPlayersOfTeamInArea(BotTeam, ThisHive->Location, UTIL_MetresToGoldSrcUnits(15.0f), false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

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
			AITASK_SetSecureHiveTask(pBot, Task, NearestEmptyHive->HiveEdict, NearestEmptyHive->FloorLocation, false);
			return;
		}
	}

	// Go to a good siege location if phase gates available

	if (AITAC_PhaseGatesAvailable(BotTeam))
	{
		const AvHAIHiveDefinition* ActiveHive = AITAC_GetActiveHiveNearestLocation(AIMGR_GetEnemyTeam(BotTeam), pBot->Edict->v.origin);

		if (ActiveHive)
		{
			DeployableSearchFilter EnemyDefences;
			EnemyDefences.DeployableTeam = EnemyTeam;
			EnemyDefences.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;
			EnemyDefences.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
			EnemyDefences.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);

			if (!AITAC_DeployableExistsAtLocation(ActiveHive->FloorLocation, &EnemyDefences) && AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, ActiveHive->Location, UTIL_MetresToGoldSrcUnits(10.0f), nullptr) < 3)
			{
				AITASK_SetAttackTask(pBot, Task, ActiveHive->HiveEdict, false);
				return;
			}

			if (Task->TaskType != TASK_GUARD || vDist2DSq(Task->TaskLocation, ActiveHive->Location) > sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
			{
				Vector GuardLocation = UTIL_GetRandomPointOnNavmeshInDonut(pBot->BotNavInfo.NavProfile, ActiveHive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), UTIL_MetresToGoldSrcUnits(25.0f));

				if (!vIsZero(GuardLocation))
				{

					Task->TaskType = TASK_GUARD;
					Task->TaskLength = 60.0f;
					Task->TaskLocation = GuardLocation;
					Task->bTaskIsUrgent = false;
					Task->TaskStartedTime = 0.0f;
					return;
				}
				
			}

			return;
			
		}
	}

	if (Task->TaskType != TASK_ATTACK)
	{
		DeployableSearchFilter AnyEnemyStuff;
		AnyEnemyStuff.DeployableTeam = EnemyTeam;
		AnyEnemyStuff.DeployableTypes = SEARCH_ANY_RES_TOWER;
		AnyEnemyStuff.ReachabilityTeam = BotTeam;
		AnyEnemyStuff.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

		AvHAIBuildableStructure EnemyRT = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &AnyEnemyStuff);

		if (EnemyRT.IsValid())
		{
			AITASK_SetAttackTask(pBot, Task, EnemyRT.edict, false);
			return;
		}

		if (AIMGR_GetEnemyTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
		{
			const AvHAIHiveDefinition* ActiveHive = AITAC_GetActiveHiveNearestLocation(AIMGR_GetEnemyTeam(BotTeam), pBot->Edict->v.origin);

			if (ActiveHive)
			{
				AITASK_SetAttackTask(pBot, Task, ActiveHive->HiveEdict, false);
				return;
			}
		}
		else
		{
			AnyEnemyStuff.DeployableTypes = STRUCTURE_MARINE_COMMCHAIR;

			AvHAIBuildableStructure EnemyCommChair = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &AnyEnemyStuff);

			if (EnemyCommChair.IsValid())
			{
				AITASK_SetAttackTask(pBot, Task, EnemyCommChair.edict, false);
				return;
			}
		}

		AnyEnemyStuff.DeployableTypes = SEARCH_ALL_STRUCTURES;

		AvHAIBuildableStructure EnemyStructure = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &AnyEnemyStuff);

		if (EnemyStructure.IsValid())
		{
			AITASK_SetAttackTask(pBot, Task, EnemyStructure.edict, false);
			return;
		}
	}
}

void AIPlayerSetMarineBombardierPrimaryTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	// Go attack sieged hive

	// Go clear res nodes


}

void AIPlayerSetWantsAndNeedsCOMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{

	if (Task->TaskType == TASK_RESUPPLY) { return; }

	bool bNeedsHealth = pBot->Edict->v.health < (pBot->Edict->v.max_health * 0.9f);
	bool bNeedsAmmo = UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < (UTIL_GetPlayerPrimaryMaxAmmoReserve(pBot->Player) * 0.9f);

	if (bNeedsHealth || bNeedsAmmo)
	{
		bool bTaskIsUrgent = (pBot->Edict->v.health < (pBot->Edict->v.max_health * 0.6f)) || (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) < UTIL_GetPlayerPrimaryWeaponMaxClipSize(pBot->Player));

		float SearchRadius = (bTaskIsUrgent) ? UTIL_MetresToGoldSrcUnits(20.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

		// If we're completely out of primary weapon ammo then always head back regardless of where we are in the map
		if (UTIL_GetPlayerPrimaryAmmoReserve(pBot->Player) == 0 && UTIL_GetPlayerPrimaryWeaponClipAmmo(pBot->Player) == 0)
		{
			SearchRadius = 0.0f;
		}

		DeployableSearchFilter NearestArmouryFilter;
		NearestArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
		NearestArmouryFilter.DeployableTeam = pBot->Player->GetTeam();
		NearestArmouryFilter.ReachabilityTeam = pBot->Player->GetTeam();
		NearestArmouryFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		NearestArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		NearestArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		NearestArmouryFilter.MaxSearchRadius = SearchRadius;

		AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &NearestArmouryFilter);

		// We really need some health or ammo, hit the armoury
		if (NearestArmoury.IsValid() && (bTaskIsUrgent || UTIL_PlayerHasLOSToEntity(pBot->Edict, NearestArmoury.edict, SearchRadius, false)))
		{
			if (!IsAreaAffectedBySpores(NearestArmoury.Location) || PlayerHasHeavyArmour(pBot->Edict))
			{
				Task->TaskType = TASK_RESUPPLY;
				Task->bTaskIsUrgent = true;
				Task->TaskLocation = NearestArmoury.Location;
				Task->TaskTarget = NearestArmoury.edict;
				return;
			}
		}
	}

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
		NearestArmouryFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		NearestArmouryFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		NearestArmouryFilter.MaxSearchRadius = (bTaskIsUrgent) ? UTIL_MetresToGoldSrcUnits(20.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

		AvHAIBuildableStructure NearestArmoury = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &NearestArmouryFilter);

		// We really need some health or ammo, either hit the armoury, or ask for a resupply
		if (NearestArmoury.IsValid() && (bTaskIsUrgent || UTIL_PlayerHasLOSToEntity(pBot->Edict, NearestArmoury.edict, SearchRadius, false)))
		{
			if (!IsAreaAffectedBySpores(NearestArmoury.Location) || PlayerHasHeavyArmour(pBot->Edict))
			{
				Task->TaskType = TASK_RESUPPLY;
				Task->bTaskIsUrgent = true;
				Task->TaskLocation = NearestArmoury.Location;
				Task->TaskTarget = NearestArmoury.edict;
				return;
			}
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
			bool bHumanWaitingRespawn = false;

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
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	if (pBot->DangerTurrets.size() > 0)
	{
		AvHAIBuildableStructure NearestDangerTurret;
		float MinDist = 0.0f;

		for (auto it = pBot->DangerTurrets.begin(); it != pBot->DangerTurrets.end(); it++)
		{
			float ThisDist = vDist2DSq(pBot->Edict->v.origin, (*it).Location);

			if (FNullEnt(NearestDangerTurret.edict) || ThisDist < MinDist)
			{
				NearestDangerTurret = (*it);
			}
		}

		if (NearestDangerTurret.IsValid())
		{
			if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
			{
				AITASK_SetAttackTask(pBot, Task, NearestDangerTurret.edict, true);
				return;
			}
			else
			{
				DeployableSearchFilter EnemyTFFilter;
				EnemyTFFilter.DeployableTeam = EnemyTeam;
				EnemyTFFilter.DeployableTypes = (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
				EnemyTFFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
				EnemyTFFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
				EnemyTFFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
				EnemyTFFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
				EnemyTFFilter.ReachabilityTeam = BotTeam;

				AvHAIBuildableStructure EnemyTF = AITAC_FindClosestDeployableToLocation(NearestDangerTurret.Location, &EnemyTFFilter);

				if (EnemyTF.IsValid())
				{
					AITASK_SetAttackTask(pBot, Task, EnemyTF.edict, true);
					return;
				}
				else
				{
					AITASK_SetAttackTask(pBot, Task, NearestDangerTurret.edict, true);
					return;
				}
			}
		}
	}

	// If we're engaging an enemy turret then finish the job
	if (Task->TaskType == TASK_ATTACK && (GetStructureTypeFromEdict(Task->TaskTarget) & (STRUCTURE_ALIEN_OFFENCECHAMBER | STRUCTURE_MARINE_TURRET | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_TURRETFACTORY)))
	{
		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			return;
		}
	}

	// Find any nearby unbuilt structures
	DeployableSearchFilter UnbuiltFilter;
	UnbuiltFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
	UnbuiltFilter.DeployableTeam = BotTeam;
	UnbuiltFilter.ReachabilityTeam = BotTeam;
	UnbuiltFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	UnbuiltFilter.ExcludeStatusFlags = (STRUCTURE_STATUS_RECYCLING | STRUCTURE_STATUS_COMPLETED);
	UnbuiltFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);

	AvHAIBuildableStructure UnbuiltIP = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &UnbuiltFilter);

	if (UnbuiltIP.IsValid() && (!IsAreaAffectedBySpores(UnbuiltIP.Location) || PlayerHasHeavyArmour(pBot->Edict)))
	{
		float ThisDist = vDist2D(UnbuiltIP.Location, pBot->Edict->v.origin);
		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, UnbuiltIP.Location, ThisDist - 5.0f, false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		if (NumBuilders < 1)
		{
			AITASK_SetBuildTask(pBot, Task, UnbuiltIP.edict, true);
			return;
		}
	}

	UnbuiltFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;

	vector<AvHAIBuildableStructure> BuildableStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &UnbuiltFilter);

	AvHAIBuildableStructure NearestStructure;
	float MinDist = 0.0f;

	for (auto it = BuildableStructures.begin(); it != BuildableStructures.end(); it++)
	{
		float ThisDist = vDist2D((*it).Location, pBot->Edict->v.origin);

		int NumBuilders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, (*it).Location, ThisDist - 5.0f, false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		// Two builders if we're not in the marine base, one to guard and keep lookout while the other builds
		int NumDesiredBuilders = (vDist2DSq((*it).Location, AITAC_GetCommChairLocation(BotTeam)) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f))) ? 1 : 2;

		if (NumBuilders < NumDesiredBuilders)
		{
			if (FNullEnt(NearestStructure.edict) || ThisDist < MinDist)
			{
				NearestStructure = (*it);
				MinDist = ThisDist;
			}
		}
	}

	if (NearestStructure.IsValid() && (!IsAreaAffectedBySpores(NearestStructure.Location) || PlayerHasHeavyArmour(pBot->Edict)))
	{
		AITASK_SetBuildTask(pBot, Task, NearestStructure.edict, true);
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

		AvHAIBuildableStructure NearestDamagedStructure = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &WeldableStructures);

		if (NearestDamagedStructure.IsValid())
		{
			bool bIsUrgent = (NearestDamagedStructure.healthPercent < 0.5f);

			AITASK_SetWeldTask(pBot, Task, NearestDamagedStructure.edict, bIsUrgent);
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

		vector<AvHAIBuildableStructure> AllMineableStructures = AITAC_FindAllDeployables(AITAC_GetTeamStartingLocation(BotTeam), &MineableStructures);
		AvHAIBuildableStructure StructureToMine;

		DeployableSearchFilter MineFilter;
		MineFilter.DeployableTypes = STRUCTURE_MARINE_DEPLOYEDMINE;
		MineFilter.DeployableTeam = BotTeam;
		MineFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(3.0f);

		float FarDist = 0.0f;

		for (auto it = AllMineableStructures.begin(); it != AllMineableStructures.end(); it++)
		{
			AvHAIBuildableStructure ThisStructure = (*it);

			if (ThisStructure.edict->v.waterlevel > 0) { continue; }

			int NumMines = AITAC_GetNumDeployablesNearLocation(ThisStructure.Location, &MineFilter);

			if (NumMines < 4)
			{
				float ThisDist = AITAC_GetPhaseDistanceBetweenPoints(ThisStructure.Location, AITAC_GetTeamStartingLocation(BotTeam));

				if (FNullEnt(StructureToMine.edict) || ThisDist > FarDist)
				{
					StructureToMine = ThisStructure;
					FarDist = ThisDist;
				}
			}
		}
		
		if (StructureToMine.IsValid())
		{
			AITASK_SetMineStructureTask(pBot, Task, StructureToMine.edict, true);
		}
	}

	// If we're engaging an enemy turret then finish the job
	if (Task->TaskType == TASK_ATTACK)
	{
		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			return;
		}
	}

	DeployableSearchFilter EnemyStructures;
	EnemyStructures.DeployableTypes = SEARCH_ALL_STRUCTURES;
	EnemyStructures.DeployableTeam = BotTeam;
	EnemyStructures.ReachabilityTeam = BotTeam;
	EnemyStructures.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

	vector<AvHAIBuildableStructure> NearbyEnemyStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &EnemyStructures);

	for (auto it = NearbyEnemyStructures.begin(); it != NearbyEnemyStructures.end(); it++)
	{
		if (UTIL_PlayerHasLOSToEntity(pBot->Edict, it->edict, UTIL_MetresToGoldSrcUnits(20.0f), false))
		{
			AITASK_SetAttackTask(pBot, Task, it->edict, false);
			return;
		}
	}

}

bool AIPlayerMustFinishCurrentTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	if (!Task || Task->TaskType == TASK_NONE) { return false; }

	if (!AITASK_IsTaskStillValid(pBot, Task)) { return false; }

	if (GetPlayerActiveClass(pBot->Player) == AVH_USER3_ALIEN_PLAYER2)
	{
		AvHTeamNumber BotTeam = pBot->Player->GetTeam();

		if (Task->ActiveBuildInfo.BuildStatus == BUILD_ATTEMPT_PENDING) { return true; }

		// If we're already capping a node, are at the node and there is an unfinished tower on there, then finish the job and don't move on yet
		if (Task->TaskType == TASK_CAP_RESNODE && vDist2DSq(pBot->Edict->v.origin, Task->TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			return true;
		}
	}

	return false;
}

void AIPlayerNSAlienThink(AvHAIPlayer* pBot)
{

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
		if (pBot->PrimaryBotTask.TaskType != TASK_BUILD || pBot->PrimaryBotTask.StructureType != STRUCTURE_ALIEN_HIVE)
		{
			pBot->PrimaryBotTask.TaskType = TASK_BUILD;
			pBot->PrimaryBotTask.StructureType = STRUCTURE_ALIEN_HIVE;
			char msg[128];
			sprintf(msg, "I'm going to drop the hive at %s", HiveToBuild->HiveName);
			BotSay(pBot, true, 1.0f, msg);
		}
		BotAlienBuildHive(pBot, &pBot->PrimaryBotTask, HiveToBuild);
		return;
	}	

	if (gpGlobals->time >= pBot->BotNextTaskEvaluationTime)
	{
		pBot->BotNextTaskEvaluationTime = gpGlobals->time + frandrange(0.2f, 0.5f);

		AITASK_BotUpdateAndClearTasks(pBot);

		AIPlayerSetPrimaryAlienTask(pBot, &pBot->PrimaryBotTask);
		AIPlayerSetSecondaryAlienTask(pBot, &pBot->SecondaryBotTask);
		AIPlayerSetWantsAndNeedsAlienTask(pBot, &pBot->WantsAndNeedsTask);
	}

	pBot->CurrentTask = AIPlayerGetNextTask(pBot);

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}

	if (pBot->DesiredCombatWeapon == WEAPON_INVALID)
	{
		pBot->DesiredCombatWeapon = UTIL_GetPlayerPrimaryWeapon(pBot->Player);
	}
}

AvHMessageID AlienGetDesiredUpgrade(AvHAIPlayer* pBot, HiveTechStatus DesiredTech)
{

	if (DesiredTech == HIVE_TECH_DEFENCE)
	{
		switch (pBot->Player->GetUser3())
		{
		case AVH_USER3_ALIEN_PLAYER1:
		case AVH_USER3_ALIEN_PLAYER2:
		case AVH_USER3_ALIEN_PLAYER3: // Gorges can heal themselves so regen not worth it. Redemption probably not worth it at 10 res cost to evolve
			return ALIEN_EVOLUTION_ONE; // Lerks are fragile so best get carapace while the bot is still not great at staying alive with them...
		case AVH_USER3_ALIEN_PLAYER4:
		case AVH_USER3_ALIEN_PLAYER5:
		{
			if (randbool())
			{
				return ALIEN_EVOLUTION_ONE;
			}
			else
			{
				if (!PlayerHasWeapon(pBot->Player, WEAPON_FADE_METABOLIZE) && randbool())
				{
					return ALIEN_EVOLUTION_TWO;
				}
				else
				{
					return ALIEN_EVOLUTION_THREE;
				}


			}
		}
		default:
			return MESSAGE_NULL;
		}
	}

	if (DesiredTech == HIVE_TECH_MOVEMENT)
	{
		switch (pBot->Player->GetUser3())
		{
		case AVH_USER3_ALIEN_PLAYER1:
		{
			
			if (randbool() || GetHasUpgrade(pBot->Player->pev->iuser4, MASK_UPGRADE_7))
			{
				return ALIEN_EVOLUTION_NINE;
			}
			else
			{
				return ALIEN_EVOLUTION_SEVEN;
			}
		}
		case AVH_USER3_ALIEN_PLAYER2:
		case AVH_USER3_ALIEN_PLAYER3:
		case AVH_USER3_ALIEN_PLAYER4:
		case AVH_USER3_ALIEN_PLAYER5:
			return ALIEN_EVOLUTION_EIGHT;
		default:
			return MESSAGE_NULL;
		}

	}

	if (DesiredTech == HIVE_TECH_SENSORY)
	{
		switch (pBot->Player->GetUser3())
		{
		case AVH_USER3_ALIEN_PLAYER1:
		{
			if (randbool() || GetHasUpgrade(pBot->Player->pev->iuser4, MASK_UPGRADE_7))
			{
				return ALIEN_EVOLUTION_TEN;
			}
			else
			{
				return ALIEN_EVOLUTION_ELEVEN;
			}
		}
		case AVH_USER3_ALIEN_PLAYER2:
			return ALIEN_EVOLUTION_TEN;
		case AVH_USER3_ALIEN_PLAYER3:
		case AVH_USER3_ALIEN_PLAYER4:
		case AVH_USER3_ALIEN_PLAYER5:
			return ALIEN_EVOLUTION_ELEVEN;
		default:
			return MESSAGE_NULL;
		}
	}

	return MESSAGE_NULL;
}

AvHMessageID GetNextAIPlayerCOUpgrade(AvHAIPlayer* pBot)
{
	if (IsPlayerMarine(pBot->Player))
	{
		return GetNextAIPlayerCOMarineUpgrade(pBot);
	}
	else
	{
		return GetNextAIPlayerCOAlienUpgrade(pBot);
	}

}

AvHMessageID GetNextAIPlayerCOMarineUpgrade(AvHAIPlayer* pBot)
{
	// Make sure sweeper always has a welder to repair CC / armouries / unlock new armouries
	if (pBot->BotRole == BOT_ROLE_SWEEPER)
	{
		if (!pBot->Player->GetHasCombatModeUpgrade(BUILD_WELDER))
		{
			return BUILD_WELDER;
		}
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(RESEARCH_GRENADES))
	{
		return RESEARCH_GRENADES;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(RESEARCH_ARMOR_ONE))
	{
		return RESEARCH_ARMOR_ONE;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(RESEARCH_WEAPONS_ONE))
	{
		return RESEARCH_WEAPONS_ONE;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(BUILD_SHOTGUN))
	{
		return BUILD_SHOTGUN;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(BUILD_SHOTGUN))
	{
		return BUILD_SHOTGUN;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(BUILD_HMG))
	{
		return BUILD_HMG;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(RESEARCH_ARMOR_TWO))
	{
		return RESEARCH_ARMOR_TWO;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(RESEARCH_WEAPONS_TWO))
	{
		return RESEARCH_WEAPONS_TWO;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(BUILD_HEAVY))
	{
		return BUILD_HEAVY;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(RESEARCH_ARMOR_THREE))
	{
		return RESEARCH_ARMOR_TWO;
	}

	if (!pBot->Player->GetHasCombatModeUpgrade(RESEARCH_WEAPONS_THREE))
	{
		return RESEARCH_WEAPONS_TWO;
	}

	return MESSAGE_NULL;
}

AvHMessageID GetNextAIPlayerCOAlienUpgrade(AvHAIPlayer* pBot)
{
	int NumPointsAvailable = pBot->ExperiencePointsAvailable;

	if (IsPlayerGorge(pBot->Edict))
	{
		NumPointsAvailable += GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_TWO);
	}

	if (IsPlayerLerk(pBot->Edict))
	{
		NumPointsAvailable += GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_THREE);
	}

	if (IsPlayerFade(pBot->Edict))
	{
		NumPointsAvailable += GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_FOUR);
	}

	if (IsPlayerOnos(pBot->Edict))
	{
		NumPointsAvailable += GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_FIVE);
	}

	// Always start off getting carapace, to improve viability early game
	if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_ONE))
	{
		return ALIEN_EVOLUTION_ONE;
	}

	// If we are a harasser, always ensure we have enough resources to go lerk
	if (pBot->BotRole == BOT_ROLE_HARASS)
	{
		if (NumPointsAvailable <= GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_THREE))
		{
			return MESSAGE_NULL;
		}

		// Lerks need adrenaline
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_EIGHT))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		// Unlock umbra
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_HIVE_TWO_UNLOCK))
		{
			return ALIEN_HIVE_TWO_UNLOCK;
		}

		// Get that sweet primal scream
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_HIVE_THREE_UNLOCK))
		{
			return ALIEN_HIVE_THREE_UNLOCK;
		}

		// Unlock focus
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_ELEVEN))
		{
			return ALIEN_EVOLUTION_ELEVEN;
		}

		// Unlock regeneration
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_TWO))
		{
			return ALIEN_EVOLUTION_TWO;
		}

		// Unlock celerity
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_SEVEN))
		{
			return ALIEN_EVOLUTION_SEVEN;
		}


		return MESSAGE_NULL;
	}

	
	if (pBot->BotRole == BOT_ROLE_SWEEPER)
	{
		// If we are a sweeper, always ensure we have enough resources to go gorge in case we want to heal the hive
		if (NumPointsAvailable <= GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_TWO))
		{
			return MESSAGE_NULL;
		}

		// Gorges need adrenaline
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_EIGHT))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		// Regen
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_TWO))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		// Celerity
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_SEVEN))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		// Redemption
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_THREE))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		// Focus
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_ELEVEN))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		// Silence
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_NINE))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		// Cloak
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_TEN))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		return MESSAGE_NULL;
	}

	// ASSAULT and BOMBARDIER STUFF BELOW
	// ASSAULT = jacked-up fade
	// BOMBARDIER = Onos

	// Unlock leap
	if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_HIVE_TWO_UNLOCK))
	{
		return ALIEN_HIVE_TWO_UNLOCK;
	}

	// If we're going assault then make sure we've saved up enough for fade
	if (pBot->BotRole == BOT_ROLE_ASSAULT)
	{
		if (CONFIG_IsFadeAllowed() && NumPointsAvailable <= GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_FOUR))
		{
			return MESSAGE_NULL;
		}

		// Unlock adrenaline
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_EIGHT))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_HIVE_THREE_UNLOCK))
		{
			return ALIEN_HIVE_THREE_UNLOCK;
		}
	}

	// As a bombardier, we can still go fade if we can't afford Onos yet, so calculate our points savings accordingly
	if (pBot->BotRole == BOT_ROLE_BOMBARDIER)
	{
		if (CONFIG_IsOnosAllowed() && NumPointsAvailable <= GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_FIVE))
		{
			return MESSAGE_NULL;
		}

		// Unlock adrenaline
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_EIGHT))
		{
			return ALIEN_EVOLUTION_EIGHT;
		}

		// Unlock regeneration
		if (!pBot->Player->GetHasCombatModeUpgrade(ALIEN_EVOLUTION_TWO))
		{
			return ALIEN_EVOLUTION_TWO;
		}
	}

	return MESSAGE_NULL;	
}

void AIPlayerCOThink(AvHAIPlayer* pBot)
{
	pBot->ExperiencePointsAvailable = (pBot->Player->GetExperienceLevel() - pBot->Player->GetExperienceLevelsSpent() - 1);

	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

	if (pBot->CurrentEnemy > -1)
	{
		if (IsPlayerMarine(pBot->Player))
		{
			if (MarineCombatThink(pBot)) { return; }
		}
		else
		{
			if (AlienCombatThink(pBot)) { return; }
		}
	}

	UpdateAIPlayerCORole(pBot);

	if (IsPlayerMarine(pBot->Edict))
	{
		AIPlayerCOMarineThink(pBot);
	}
	else
	{
		AIPlayerCOAlienThink(pBot);
	}
}

void AIPlayerCOMarineThink(AvHAIPlayer* pBot)
{
	AvHMessageID NextCombatUpgrade = GetNextAIPlayerCOMarineUpgrade(pBot);

	if (NextCombatUpgrade != MESSAGE_NULL)
	{
		int Cost = GetGameRules()->GetCostForMessageID(NextCombatUpgrade);

		if (pBot->ExperiencePointsAvailable >= Cost)
		{
			if (gpGlobals->time - pBot->LastRequestTime > 1.0f)
			{
				pBot->Impulse = (int)NextCombatUpgrade;
				pBot->LastRequestTime = gpGlobals->time;
				return;
			}
		}
	}

	if (gpGlobals->time >= pBot->BotNextTaskEvaluationTime)
	{
		pBot->BotNextTaskEvaluationTime = gpGlobals->time + frandrange(0.2f, 0.5f);

		AITASK_BotUpdateAndClearTasks(pBot);

		AIPlayerSetPrimaryCOMarineTask(pBot, &pBot->PrimaryBotTask);
		AIPlayerSetSecondaryCOMarineTask(pBot, &pBot->SecondaryBotTask);
		AIPlayerSetWantsAndNeedsCOMarineTask(pBot, &pBot->WantsAndNeedsTask);
	}

	pBot->CurrentTask = AIPlayerGetNextTask(pBot);

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}
}

void AIPlayerCOAlienThink(AvHAIPlayer* pBot)
{
	AvHMessageID NextCombatUpgrade = GetNextAIPlayerCOAlienUpgrade(pBot);

	if (NextCombatUpgrade != MESSAGE_NULL)
	{
		int Cost = GetGameRules()->GetCostForMessageID(NextCombatUpgrade);

		if (pBot->ExperiencePointsAvailable >= Cost)
		{
			if (gpGlobals->time - pBot->LastCombatTime > 5.0f)
			{
				if (gpGlobals->time - pBot->LastRequestTime > 1.0f)
				{
					pBot->Impulse = (int)NextCombatUpgrade;
					pBot->LastRequestTime = gpGlobals->time;
					return;
				}
			}
		}
	}

	if (gpGlobals->time >= pBot->BotNextTaskEvaluationTime)
	{
		pBot->BotNextTaskEvaluationTime = gpGlobals->time + frandrange(0.2f, 0.5f);

		AITASK_BotUpdateAndClearTasks(pBot);

		AIPlayerSetPrimaryCOAlienTask(pBot, &pBot->PrimaryBotTask);
		AIPlayerSetSecondaryCOAlienTask(pBot, &pBot->SecondaryBotTask);
		AIPlayerSetWantsAndNeedsAlienTask(pBot, &pBot->WantsAndNeedsTask);
	}

	pBot->CurrentTask = AIPlayerGetNextTask(pBot);

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}

}

void AIPlayerSetPrimaryCOMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	DeployableSearchFilter EnemyStuffFilter;
	EnemyStuffFilter.DeployableTeam = EnemyTeam;
	EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
	EnemyStuffFilter.ReachabilityTeam = BotTeam;
	EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

	AvHAIBuildableStructure EnemyStructure = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &EnemyStuffFilter);

	edict_t* StructureToAttack = nullptr;

	if (EnemyStructure.IsValid())
	{
		StructureToAttack = EnemyStructure.edict;
	}
	else
	{
		if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
		{
			const AvHAIHiveDefinition* EnemyHive = AITAC_GetActiveHiveNearestLocation(EnemyTeam, pBot->Edict->v.origin);

			if (EnemyHive)
			{
				StructureToAttack = EnemyHive->HiveEdict;
			}
		}
	}

	// Nothing to attack, just hunt down remaining enemy players. Shouldn't happen in vanilla combat mode, but a plugin might change behaviour
	if (!StructureToAttack || FNullEnt(StructureToAttack))
	{

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

		return;
	}

	// If we're close to the enemy base then just attack. We don't want bots marching through the enemy base and ignoring the hive/comm chair
	if (vDist2DSq(pBot->Edict->v.origin, StructureToAttack->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
	{
		AITASK_SetAttackTask(pBot, Task, StructureToAttack, false);
		return;
	}

	// At this point we already know what we want to do, just crack on
	if (Task->TaskType != TASK_NONE) { return; }

	// Decide if we're going to attack right away, or take a little detour first. Helps mix things up and prevents all bots just gang-rushing the base endlessly

	if (randbool())
	{
		Vector RandomVisitPoint = UTIL_GetRandomPointOnNavmeshInDonut(pBot->BotNavInfo.NavProfile, StructureToAttack->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), UTIL_MetresToGoldSrcUnits(40.0f));

		if (!vIsZero(RandomVisitPoint))
		{
			AITASK_SetMoveTask(pBot, Task, RandomVisitPoint, false);
			return;
		}
	}

	AITASK_SetAttackTask(pBot, Task, StructureToAttack, false);


}

void AIPlayerSetSecondaryCOMarineTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	edict_t* CommChair = AITAC_GetCommChair(BotTeam);

	DeployableSearchFilter AttackedStructuresFilter;
	AttackedStructuresFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
	AttackedStructuresFilter.DeployableTeam = BotTeam;
	AttackedStructuresFilter.ReachabilityTeam = BotTeam;
	AttackedStructuresFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	AttackedStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_UNDERATTACK;
	AttackedStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(30.0f);

	vector<AvHAIBuildableStructure> AllAttackedStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &AttackedStructuresFilter);

	AvHAIBuildableStructure StructureToDefend;
	float MinDist = 0.0f;

	for (auto it = AllAttackedStructures.begin(); it != AllAttackedStructures.end(); it++)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		float ThisDist = vDist2D(pBot->Edict->v.origin, ThisStructure.edict->v.origin);

		int NumAttackers = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, ThisStructure.Location, UTIL_MetresToGoldSrcUnits(15.0f), nullptr);

		if (NumAttackers == 0) { continue; }

		int NumExistingDefenders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, ThisStructure.Location, ThisDist - 10.0f, false, pBot->Edict, AVH_USER3_COMMANDER_PLAYER);

		if (NumExistingDefenders < 2)
		{
			if (FNullEnt(StructureToDefend.edict) || ThisDist < MinDist)
			{
				StructureToDefend = ThisStructure;
				MinDist = ThisDist;
			}
		}
	}

	if (StructureToDefend.IsValid())
	{
		AITASK_SetDefendTask(pBot, Task, StructureToDefend.edict, true);
		return;
	}

	if (PlayerHasWeapon(pBot->Player, WEAPON_MARINE_WELDER))
	{
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


		DeployableSearchFilter DamagedStructuresFilter;
		DamagedStructuresFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
		DamagedStructuresFilter.DeployableTeam = BotTeam;
		DamagedStructuresFilter.ReachabilityTeam = BotTeam;
		DamagedStructuresFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		DamagedStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_DAMAGED;
		DamagedStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(20.0f);

		AvHAIBuildableStructure StructureToRepair;
		vector<AvHAIBuildableStructure> AllDamagedStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &DamagedStructuresFilter);

		MinDist = 0.0f;

		for (auto it = AllDamagedStructures.begin(); it != AllDamagedStructures.end(); it++)
		{
			AvHAIBuildableStructure ThisStructure = (*it);

			if (ThisStructure.StructureType == STRUCTURE_MARINE_COMMCHAIR && ThisStructure.healthPercent < 0.7f)
			{
				StructureToRepair = ThisStructure;
				break;
			}

			float ThisDist = vDist2DSq(ThisStructure.Location, pBot->Edict->v.origin);

			if (FNullEnt(StructureToRepair.edict) || ThisDist < MinDist)
			{
				StructureToRepair = ThisStructure;
				MinDist = ThisDist;
			}
		}

		if (StructureToRepair.IsValid())
		{
			AITASK_SetWeldTask(pBot, Task, StructureToRepair.edict, true);
			return;
		}


		if (NearestWeldablePlayer)
		{
			AITASK_SetWeldTask(pBot, Task, NearestWeldablePlayer->edict(), false);
			return;
		}

		DeployableSearchFilter NearbyArmouryFilter;
		NearbyArmouryFilter.DeployableTypes = (STRUCTURE_MARINE_ARMOURY | STRUCTURE_MARINE_ADVARMOURY);
		NearbyArmouryFilter.DeployableTeam = BotTeam;
		NearbyArmouryFilter.ReachabilityTeam = BotTeam;
		NearbyArmouryFilter.ReachabilityFlags = AI_REACHABILITY_MARINE;

		AvHAIBuildableStructure NearestEasyAccessArmoury = AITAC_FindClosestDeployableToLocation(ZERO_VECTOR, &NearbyArmouryFilter);

		if (!NearestEasyAccessArmoury.IsValid())
		{
			NearbyArmouryFilter.ReachabilityFlags = AI_REACHABILITY_WELDER;

			AvHAIBuildableStructure NearestWeldableAccessArmoury = AITAC_FindClosestDeployableToLocation(ZERO_VECTOR, &NearbyArmouryFilter);

			if (NearestWeldableAccessArmoury.IsValid())
			{
				AITASK_SetMoveTask(pBot, Task, NearestWeldableAccessArmoury.Location, true);
				return;
			}
		}
	}

	AITASK_ClearBotTask(pBot, Task);
}

void AIPlayerSetPrimaryCOAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{

	if (IsPlayerGorge(pBot->Edict))
	{
		const AvHAIHiveDefinition* TheHive = AITAC_GetNearestTeamHive(pBot->Player->GetTeam(), pBot->Edict->v.origin, true);

		if (TheHive)
		{
			AITASK_ClearBotTask(pBot, Task);
			BotGuardLocation(pBot, TheHive->FloorLocation);
			return;
		}
	}

	if (Task->TaskType == TASK_ATTACK && vDist2DSq(Task->TaskTarget->v.origin, pBot->Edict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		return;
	}

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	if (pBot->BotRole == BOT_ROLE_HARASS)
	{
		if (!IsPlayerLerk(pBot->Edict) && pBot->ExperiencePointsAvailable >= GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_THREE))
		{
			if (Task->TaskType != TASK_EVOLVE)
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetActiveHiveNearestLocation(BotTeam, pBot->Edict->v.origin);

				if (NearestHive)
				{
					AITASK_SetEvolveTask(pBot, Task, NearestHive->HiveEdict, ALIEN_LIFEFORM_THREE, true);
					
				}
			}

			return;
		}
	}

	if (pBot->BotRole == BOT_ROLE_ASSAULT)
	{
		if (CONFIG_IsFadeAllowed() && !IsPlayerFade(pBot->Edict) && pBot->ExperiencePointsAvailable >= GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_FOUR))
		{
			if (Task->TaskType != TASK_EVOLVE)
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetActiveHiveNearestLocation(BotTeam, pBot->Edict->v.origin);

				if (NearestHive)
				{
					AITASK_SetEvolveTask(pBot, Task, NearestHive->HiveEdict, ALIEN_LIFEFORM_FOUR, true);
				}
			}
			return;
		}
	}

	if (pBot->BotRole == BOT_ROLE_BOMBARDIER)
	{
		if (!IsPlayerOnos(pBot->Edict) && !IsPlayerFade(pBot->Edict))
		{
			AvHMessageID DesiredEvolution = MESSAGE_NULL;

			if (CONFIG_IsOnosAllowed() && pBot->ExperiencePointsAvailable >= GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_FIVE))
			{
				DesiredEvolution = ALIEN_LIFEFORM_FIVE;
			}
			else if (CONFIG_IsFadeAllowed() && pBot->ExperiencePointsAvailable >= GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_FOUR))
			{
				DesiredEvolution = ALIEN_LIFEFORM_FOUR;
			}

			if (DesiredEvolution != MESSAGE_NULL)
			{
				if (Task->TaskType != TASK_EVOLVE)
				{
					const AvHAIHiveDefinition* NearestHive = AITAC_GetActiveHiveNearestLocation(BotTeam, pBot->Edict->v.origin);

					if (NearestHive)
					{
						AITASK_SetEvolveTask(pBot, Task, NearestHive->HiveEdict, DesiredEvolution, true);
					}
				}
				return;
			}
		}
	}

	DeployableSearchFilter EnemyStuffFilter;
	EnemyStuffFilter.DeployableTeam = EnemyTeam;
	EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
	EnemyStuffFilter.ReachabilityTeam = BotTeam;
	EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

	AvHAIBuildableStructure EnemyStructure = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &EnemyStuffFilter);

	edict_t* StructureToAttack = nullptr;

	if (EnemyStructure.IsValid())
	{
		StructureToAttack = EnemyStructure.edict;
	}
	else
	{
		if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
		{
			const AvHAIHiveDefinition* EnemyHive = AITAC_GetActiveHiveNearestLocation(EnemyTeam, pBot->Edict->v.origin);

			if (EnemyHive)
			{
				StructureToAttack = EnemyHive->HiveEdict;
			}
		}
	}

	// Nothing to attack, just hunt down remaining enemy players. Shouldn't happen in vanilla combat mode, but a plugin might change behaviour
	if (!StructureToAttack || FNullEnt(StructureToAttack))
	{
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

		return;
	}

	// If we're close to the enemy base then just attack. We don't want bots marching through the enemy base and ignoring the hive/comm chair
	if (vDist2DSq(pBot->Edict->v.origin, StructureToAttack->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
	{
		AITASK_SetAttackTask(pBot, Task, StructureToAttack, false);
		return;
	}

	// At this point we already know what we want to do, just crack on
	if (Task->TaskType != TASK_NONE) { return; }

	// Decide if we're going to attack right away, or take a little detour first. Helps mix things up and prevents all bots just gang-rushing the base endlessly

	if (randbool())
	{
		Vector RandomVisitPoint = UTIL_GetRandomPointOnNavmeshInDonut(pBot->BotNavInfo.NavProfile, StructureToAttack->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), UTIL_MetresToGoldSrcUnits(40.0f));

		if (!vIsZero(RandomVisitPoint))
		{
			AITASK_SetMoveTask(pBot, Task, RandomVisitPoint, false);
			return;
		}
	}

	AITASK_SetAttackTask(pBot, Task, StructureToAttack, false);
	
}

void AIPlayerSetSecondaryCOAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	const AvHAIHiveDefinition* TheHive = AITAC_GetNearestTeamHive(BotTeam, pBot->Edict->v.origin, true);

	if (TheHive->bIsUnderAttack)
	{
		int NumAttackers = AITAC_GetNumPlayersOfTeamInArea(EnemyTeam, TheHive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), false, nullptr, AVH_USER3_NONE);

		int MaxDefenders = imini(NumAttackers + 1, (int)floorf((float)AIMGR_GetNumPlayersOnTeam(BotTeam) * 0.5f));

		float ThisDist = vDist2D(pBot->Edict->v.origin, TheHive->FloorLocation);

		int NumExistingDefenders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, TheHive->FloorLocation, ThisDist - 10.0f, false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

		if (NumExistingDefenders < MaxDefenders)
		{
			AITASK_SetDefendTask(pBot, Task, TheHive->HiveEdict, true);
			return;
		}
	}

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

		if (TheHive->HealthPercent < 1.0f)
		{
			Task->TaskType = TASK_HEAL;
			Task->TaskTarget = TheHive->HiveEdict;
			Task->bTaskIsUrgent = true;
			return;
		}
	}

	if (pBot->BotRole == BOT_ROLE_SWEEPER && !IsPlayerGorge(pBot->Edict) && TheHive->HealthPercent < 0.8f)
	{
		if (pBot->ExperiencePointsAvailable >= GetGameRules()->GetCostForMessageID(ALIEN_LIFEFORM_TWO))
		{
			if (!IsPlayerGorge(pBot->Edict))
			{
				AITASK_SetEvolveTask(pBot, Task, TheHive->HiveEdict, ALIEN_LIFEFORM_TWO, true);
				return;
			}
		}
	}


	AITASK_ClearBotTask(pBot, Task);
}

void AIPlayerEndMatchThink(AvHAIPlayer* pBot)
{
	
	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

	if (pBot->CurrentEnemy > -1)
	{
		if (IsPlayerMarine(pBot->Player))
		{
			MarineCombatThink(pBot);
		}
		else
		{
			AlienCombatThink(pBot);
		}

		return;
	}

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	vector<AvHPlayer*> EnemyPlayers = AIMGR_GetAllPlayersOnTeam(EnemyTeam);

	AvHPlayer* EnemyToAttack = nullptr;
	float MinDist = 0.0f;

	for (auto it = EnemyPlayers.begin(); it != EnemyPlayers.end(); it++)
	{
		edict_t* PlayerEdict = (*it)->edict();
		if (IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(pBot->Player->pev->origin, PlayerEdict->v.origin);

			if (!EnemyToAttack || ThisDist < MinDist)
			{
				EnemyToAttack = (*it);
				MinDist = ThisDist;
			}
		}
	}

	if (EnemyToAttack)
	{
		MoveTo(pBot, EnemyToAttack->pev->origin, MOVESTYLE_NORMAL, UTIL_MetresToGoldSrcUnits(10.0f));
	}
	else
	{
		AIPlayerDMThink(pBot);
	}
	
}

void AIPlayerDMThink(AvHAIPlayer* pBot)
{
	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

	if (pBot->CurrentEnemy > -1)
	{
		if (IsPlayerMarine(pBot->Player))
		{
			MarineCombatThink(pBot);
		}
		else
		{
			AlienCombatThink(pBot);
		}

		return;
	}

	AITASK_BotUpdateAndClearTasks(pBot);

	if (pBot->CurrentTask->TaskType == TASK_NONE)
	{
		Vector RandomMovePoint = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_MetresToGoldSrcUnits(50.0f));

		if (!vIsZero(RandomMovePoint))
		{
			AITASK_SetMoveTask(pBot, pBot->CurrentTask, RandomMovePoint, false);
		}
	}

	BotProgressTask(pBot, pBot->CurrentTask);

}

void AIPlayerThink(AvHAIPlayer* pBot)
{

#ifdef BOTDEBUG
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i + 1);

		if (DebugBots[i] == pBot->Edict && !FNullEnt(PlayerEdict) && IsEdictPlayer(PlayerEdict) && IsPlayerHuman(PlayerEdict))
		{
			AvHAIPlayer* BotRef = AIMGR_GetBotRefFromEdict(DebugBots[i]);

			if (BotRef)
			{
				DEBUG_PrintBotDebugInfo(PlayerEdict, BotRef);
			}
		}
	}
#endif

	pBot->ThinkDelta = fminf(gpGlobals->time - pBot->LastThinkTime, 0.1f);
	pBot->LastThinkTime = gpGlobals->time;

	bool bShouldThink = ShouldBotThink(pBot);

	if (bShouldThink)
	{
		if (pBot->bIsInactive)
		{
			BotResumePlay(pBot);
		}
	}
	else
	{
		ClearBotInputs(pBot);
		pBot->bIsInactive = true;
		return;
	}

	StartNewBotFrame(pBot);

	if (avh_botdebugmode.value == 1)
	{
		DroneThink(pBot);
	}
	else if (avh_botdebugmode.value == 2)
	{
		TestNavThink(pBot);
	}
	else if (avh_botdebugmode.value == 3)
	{
		CustomThink(pBot);
	}
	else
	{
		switch (GetGameRules()->GetMapMode())
		{
		case MAP_MODE_NS:
		{
			if (AIMGR_IsMatchPracticallyOver())
			{
				AIPlayerEndMatchThink(pBot);
			}
			else
			{
				AIPlayerNSThink(pBot);
			}
		}			
		break;
		case MAP_MODE_CO:
			AIPlayerCOThink(pBot);
			break;
		default:
			AIPlayerDMThink(pBot);
			break;
		}
	}

	BotUpdateDesiredViewRotation(pBot);

	EndBotFrame(pBot);
}

void TestNavThink(AvHAIPlayer* pBot)
{
	AITASK_BotUpdateAndClearTasks(pBot);

	pBot->CurrentTask = &pBot->PrimaryBotTask;

	if (IsPlayerAlien(pBot->Edict) && IsPlayerSkulk(pBot->Edict))
	{
		if (AITAC_GetNumPlayersOnTeamOfClass(pBot->Player->GetTeam(), AVH_USER3_ALIEN_PLAYER2, pBot->Edict) == 0)
		{
			if (pBot->Player->GetResources() >= BALANCE_VAR(kGorgeCost))
			{
				BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_TWO);
				return;
			}
		}

		if (AITAC_GetNumPlayersOnTeamOfClass(pBot->Player->GetTeam(), AVH_USER3_ALIEN_PLAYER3, pBot->Edict) == 0)
		{
			if (pBot->Player->GetResources() >= BALANCE_VAR(kLerkCost))
			{
				BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_THREE);
				return;
			}
			else
			{
				pBot->Player->GiveResources(BALANCE_VAR(kLerkCost));
			}
		}

		if (AITAC_GetNumPlayersOnTeamOfClass(pBot->Player->GetTeam(), AVH_USER3_ALIEN_PLAYER4, pBot->Edict) == 0)
		{
			if (pBot->Player->GetResources() >= BALANCE_VAR(kFadeCost))
			{
				BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_FOUR);
				return;
			}
			else
			{
				pBot->Player->GiveResources(BALANCE_VAR(kFadeCost));
			}
		}

		if (AITAC_GetNumPlayersOnTeamOfClass(pBot->Player->GetTeam(), AVH_USER3_ALIEN_PLAYER5, pBot->Edict) == 0)
		{
			if (pBot->Player->GetResources() >= BALANCE_VAR(kOnosCost))
			{
				BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_FIVE);
				return;
			}
			else
			{
				pBot->Player->GiveResources(BALANCE_VAR(kOnosCost));
			}
		}
	}

	if (pBot->PrimaryBotTask.TaskType == TASK_MOVE)
	{
		if (vDist2DSq(pBot->Edict->v.origin, pBot->PrimaryBotTask.TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(1.0f)))
		{
			AITASK_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			return;
		}

		BotProgressTask(pBot, &pBot->PrimaryBotTask);
	}
	else
	{
		Vector RandomPoint = ZERO_VECTOR;

		if (GetGameRules()->GetMapMode() == MAP_MODE_NS)
		{
			AvHAIResourceNode* RandomNode = AITAC_GetRandomResourceNode((AvHTeamNumber)pBot->Edict->v.team, pBot->BotNavInfo.NavProfile.ReachabilityFlag);

			if (!RandomNode) { return; }

			RandomPoint = RandomNode->Location;
		}
		else
		{
			RandomPoint = UTIL_GetRandomPointOnNavmeshInRadius(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, UTIL_MetresToGoldSrcUnits(100.0f));
		}

		if (!vIsZero(RandomPoint) && UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, RandomPoint, max_player_use_reach))
		{
			AITASK_SetMoveTask(pBot, &pBot->PrimaryBotTask, RandomPoint, true);
		}
		else
		{
			if (!vIsZero(pBot->BotNavInfo.LastNavMeshPosition))
			{
				MoveToWithoutNav(pBot, pBot->BotNavInfo.LastNavMeshPosition);
			}
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
	return NavmeshLoaded() && GetGameRules()->GetGameStarted() && !AIMGR_HasMatchEnded() && (IsPlayerActiveInGame(pBot->Edict) || IsPlayerCommander(pBot->Edict)) && !IsPlayerGestating(pBot->Edict);
}

void BotResumePlay(AvHAIPlayer* pBot)
{
	ClearBotMovement(pBot);
	SetBaseNavProfile(pBot);

	pBot->bIsInactive = false;

	// Keep things nicely randomized in Combat mode
	if (GetGameRules()->GetMapMode() == MAP_MODE_CO)
	{
		AITASK_ClearBotTask(pBot, &pBot->PrimaryBotTask);
	}
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
	if (HiveRef && vDist2DSq(HiveRef->Location, Destination) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
	{
		if (HiveRef->Status == HIVE_STATUS_UNBUILT)
		{
			if (!AICOMM_IsHiveFullySecured(pBot, HiveRef, false))
			{
				AITASK_SetSecureHiveTask(pBot, &pBot->CommanderTask, HiveRef->HiveEdict, ActualMoveLocation, false);
				pBot->CommanderTask.bIssuedByCommander = true;
				return;
			}
		}
		else
		{
			if (UTIL_QuickTrace(pBot->Edict, Destination + Vector(0.0f, 0.0f, 32.0f), HiveRef->Location))
			{
				AITASK_SetAttackTask(pBot, &pBot->CommanderTask, HiveRef->HiveEdict, false);
				pBot->CommanderTask.bIssuedByCommander = true;
				return;
			}
		}

		
	}

	// Otherwise, treat as a normal move order. Go there and wait a bit to see what the commander wants to do next
	AITASK_SetMoveTask(pBot, &pBot->CommanderTask, ActualMoveLocation, true);
	pBot->CommanderTask.bIssuedByCommander = true;
	
}

void BotStopCommanderMode(AvHAIPlayer* pBot)
{
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

	int NumMissingChambers = 0;

	// Do we have any missing upgrade chambers (should have 3 of each if we can build them)
	AvHAIDeployableStructureType MissingStructure = AITAC_GetNextMissingUpgradeChamberForTeam(BotTeam, NumMissingChambers);
	bool bShouldBuildMissingStructure = false;

	if (MissingStructure != STRUCTURE_NONE)
	{
		int NumBuilders = AITASK_GetNumBotsWithBuildTask(BotTeam, MissingStructure, pBot->Edict);

		if (NumBuilders < NumMissingChambers)
		{
			bShouldBuildMissingStructure = true;
		}
	}

	// If we do have a missing upgrade chamber, built it at the nearest hive or resource node that we own, whichever is nearest
	if (bShouldBuildMissingStructure)
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

	float MinDist = 0.0f;

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

			vector<AvHAIBuildableStructure> AllReinforcingStructures = AITAC_FindAllDeployables(ThisHive->FloorLocation, &ExistingReinforcementFilter);

			int NumOCs = 0;
			int NumDCs = 0;
			int NumMCs = 0;
			int NumSCs = 0;

			for (auto it = AllReinforcingStructures.begin(); it != AllReinforcingStructures.end(); it++)
			{
				switch ((*it).StructureType)
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
				float ThisDist = vDist2DSq(pBot->Edict->v.origin, ThisHive->FloorLocation);

				if (!HiveToSecure || ThisDist < MinDist)
				{
					HiveToSecure = ThisHive;
					MinDist = ThisDist;
				}
			}
			
		}
	}

	if (HiveToSecure)
	{
		AITASK_SetReinforceStructureTask(pBot, Task, HiveToSecure->HiveEdict, false);
		return;
	}

	DeployableSearchFilter ResNodeFilter;
	ResNodeFilter.DeployableTeam = BotTeam;
	ResNodeFilter.DeployableTypes = STRUCTURE_ALIEN_RESTOWER;
	ResNodeFilter.ReachabilityTeam = BotTeam;
	ResNodeFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;

	vector<AvHAIBuildableStructure> AllMatchingTowers = AITAC_FindAllDeployables(pBot->Edict->v.origin, &ResNodeFilter);

	edict_t* TowerToReinforce = nullptr;
	MinDist = 0.0f;

	for (auto it = AllMatchingTowers.begin(); it != AllMatchingTowers.end(); it++)
	{
		AvHAIBuildableStructure ThisResTower = (*it);

		AvHAIResourceNode* NodeRef = AITAC_GetNearestResourceNodeToLocation(ThisResTower.Location);

		// Don't reinforce RTs in active hives, they can be defended by the aliens (or it's owned by enemy alien team, either way, don't build in there!)
		if (NodeRef && !FNullEnt(NodeRef->ParentHive))
		{
			const AvHAIHiveDefinition* NearestHive = AITAC_GetHiveNearestLocation(NodeRef->Location);

			if (NearestHive->Status == HIVE_STATUS_BUILT) { continue; }
		}

		// Don't reinforce RTs which have enemy stuff nearby. Would be suicide for the gorge.
		DeployableSearchFilter EnemyStructureFilter;
		EnemyStructureFilter.DeployableTeam = EnemyTeam;
		EnemyStructureFilter.DeployableTypes = (STRUCTURE_MARINE_PHASEGATE | STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_ALIEN_OFFENCECHAMBER);
		EnemyStructureFilter.ExcludeStatusFlags = (STRUCTURE_STATUS_RECYCLING);
		EnemyStructureFilter.IncludeStatusFlags = (STRUCTURE_STATUS_COMPLETED);
		EnemyStructureFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(7.5);

		if (AITAC_DeployableExistsAtLocation(ThisResTower.Location, &EnemyStructureFilter)) { continue; }

		DeployableSearchFilter ExistingReinforcementFilter;
		ExistingReinforcementFilter.DeployableTeam = BotTeam;
		ExistingReinforcementFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(5.0f);
		ExistingReinforcementFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;

		vector<AvHAIBuildableStructure> AllReinforcingStructures = AITAC_FindAllDeployables(ThisResTower.Location, &ExistingReinforcementFilter);

		int NumOCs = 0;
		int NumDCs = 0;
		int NumMCs = 0;
		int NumSCs = 0;

		for (auto it = AllReinforcingStructures.begin(); it != AllReinforcingStructures.end(); it++)
		{
			switch ((*it).StructureType)
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
			float ThisDist = vDist2DSq(AITAC_GetTeamStartingLocation(EnemyTeam), ThisResTower.Location);

			if (!TowerToReinforce || ThisDist < MinDist)
			{
				TowerToReinforce = ThisResTower.edict;
				MinDist = ThisDist;
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

	bool bCanAttackTowers = (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2 || PlayerHasWeapon(pBot->Player, WEAPON_GORGE_BILEBOMB));

	// If we're already capping a node, are at the node and there is an unfinished tower on there, then finish the job and don't move on yet
	if (Task->TaskType == TASK_CAP_RESNODE)
	{
		const AvHAIResourceNode* ResNodeIndex = AITAC_GetNearestResourceNodeToLocation(Task->TaskLocation);

		if (ResNodeIndex && ResNodeIndex->OwningTeam != BotTeam)
		{
			if (ResNodeIndex->OwningTeam != BotTeam)
			{
				if (!ResNodeIndex->bIsOccupied || bCanAttackTowers) { return; }
			}
			else
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
	}

	float ResourcesRequired = BALANCE_VAR(kResourceTowerCost);

	if (GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2)
	{
		ResourcesRequired += BALANCE_VAR(kGorgeCost);
	}

	bool bCanPlaceTower = pBot->Player->GetResources() >= (BALANCE_VAR(kResourceTowerCost) * 0.8f);

	if (IsPlayerLerk(pBot->Edict) || IsPlayerFade(pBot->Edict) || IsPlayerOnos(pBot->Edict))
	{
		bCanPlaceTower = pBot->Player->GetResources() >= 75 && AITAC_GetTeamResNodeOwnership(BotTeam, true) >= 0.5f;
	}

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
		AITASK_SetEvolveTask(pBot, Task, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_ONE, true);
		return;
	}

	if (Task->TaskType == TASK_EVOLVE) { return; }

	// CHECK TO SEE IF WE NEED TO EVOLVE INTO FADE/ONOS

	if (!IsPlayerFade(pBot->Edict) && !IsPlayerOnos(pBot->Edict))
	{

		if (CONFIG_IsOnosAllowed() && !IsPlayerOnos(pBot->Edict) && pBot->Player->GetResources() >= BALANCE_VAR(kOnosCost))
		{
			int NumOnos = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER5, pBot->Edict);

			if (NumOnos < 2)
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetNearestTeamHive(BotTeam, pBot->Edict->v.origin, true);

				if (NearestHive)
				{
					AITASK_SetEvolveTask(pBot, Task, NearestHive->HiveEdict, ALIEN_LIFEFORM_FIVE, true);
					return;
				}
			}
		}

		if (CONFIG_IsFadeAllowed() && !IsPlayerFade(pBot->Edict) && pBot->Player->GetResources() >= BALANCE_VAR(kFadeCost))
		{
			int NumFades = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER4, pBot->Edict);
			int NumOnos = AITAC_GetNumPlayersOnTeamOfClass(BotTeam, AVH_USER3_ALIEN_PLAYER5, pBot->Edict);

			if (NumFades < 2 || NumOnos >= 2)
			{
				const AvHAIHiveDefinition* NearestHive = AITAC_GetNearestTeamHive(BotTeam, pBot->Edict->v.origin, true);

				if (NearestHive)
				{
					AITASK_SetEvolveTask(pBot, Task, NearestHive->HiveEdict, ALIEN_LIFEFORM_FOUR, true);
					return;
				}

			}
		}

	}

	// CHECK TO SEE IF WE NEED TO HELP BREAK A SIEGE THAT IS ACTIVE (I.E. HAS SIEGE TURRETS)

	const AvHAIHiveDefinition* NearestSiegedHive = AITAC_GetNearestHiveUnderActiveSiege(EnemyTeam, pBot->Edict->v.origin);

	if (NearestSiegedHive)
	{
		// Check if we're already trying to break a siege attempt, so we don't get torn between multiple potentials
		if (Task->TaskType == TASK_ATTACK)
		{
			const AvHAIHiveDefinition* HiveNearestAttackTarget = AITAC_GetNearestTeamHive(BotTeam, Task->TaskTarget->v.origin, false);

			if (HiveNearestAttackTarget && vDist2DSq(HiveNearestAttackTarget->Location, Task->TaskTarget->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(25.0f))) { return; }
		}

		DeployableSearchFilter EnemyStuffFilter;
		EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
		EnemyStuffFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
		EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
		EnemyStuffFilter.DeployableTeam = EnemyTeam;
		EnemyStuffFilter.ReachabilityTeam = BotTeam;
		EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
		EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(25.0f);

		vector<AvHAIBuildableStructure> AllSiegingStructures = AITAC_FindAllDeployables(NearestSiegedHive->Location, &EnemyStuffFilter);

		AvHAIBuildableStructure StructureToTarget;

		float MinDist = 0.0f;

		for (auto it = AllSiegingStructures.begin(); it != AllSiegingStructures.end(); it++)
		{
			AvHAIBuildableStructure ThisStructure = (*it);

			// Always go for the phase gate first to prevent reinforcements
			if (ThisStructure.StructureType == STRUCTURE_MARINE_PHASEGATE)
			{
				StructureToTarget = ThisStructure;
				continue;
			}

			// Then go for any turret factories, especially advanced ones to cut off siege turrets
			if (ThisStructure.StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
			{
				StructureToTarget = ThisStructure;
				continue;
			}

			if (ThisStructure.StructureType == STRUCTURE_MARINE_TURRETFACTORY)
			{
				StructureToTarget = ThisStructure;
				continue;
			}

			// Pick up anything else
			float ThisDist = vDist2DSq(ThisStructure.Location, pBot->Edict->v.origin);

			if (FNullEnt(StructureToTarget.edict) || ThisDist < MinDist)
			{
				StructureToTarget = ThisStructure;
				MinDist = ThisDist;
			}
		}

		if (StructureToTarget.IsValid())
		{
			AITASK_SetAttackTask(pBot, Task, StructureToTarget.edict, true);
			return;
		}
	}

	// CHECK IF MARINES ARE TRYING TO BUILD A SIEGE BASE

	if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_MARINE)
	{
		// Check if we're already trying to break a siege attempt, so we don't get torn between multiple potentials
		if (Task->TaskType == TASK_ATTACK)
		{
			const AvHAIHiveDefinition* HiveNearestAttackTarget = AITAC_GetNearestTeamHive(BotTeam, Task->TaskTarget->v.origin, false);

			if (HiveNearestAttackTarget && vDist2DSq(HiveNearestAttackTarget->Location, Task->TaskTarget->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(25.0f))) { return; }
		}

		vector<AvHAIHiveDefinition*> AllTeamHives = AITAC_GetAllTeamHives(BotTeam, false);

		for (auto it = AllTeamHives.begin(); it != AllTeamHives.end(); it++)
		{
			AvHAIHiveDefinition* ThisHive = (*it);

			DeployableSearchFilter EnemyStuffFilter;
			EnemyStuffFilter.DeployableTypes = SEARCH_ALL_STRUCTURES;
			EnemyStuffFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
			EnemyStuffFilter.DeployableTeam = EnemyTeam;
			EnemyStuffFilter.ReachabilityTeam = BotTeam;
			EnemyStuffFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
			EnemyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(25.0f);

			vector<AvHAIBuildableStructure> AllSiegingStructures = AITAC_FindAllDeployables(ThisHive->Location, &EnemyStuffFilter);

			AvHAIBuildableStructure StructureToTarget;

			float MinDist = 0.0f;

			for (auto it = AllSiegingStructures.begin(); it != AllSiegingStructures.end(); it++)
			{
				AvHAIBuildableStructure ThisStructure = (*it);

				// Always go for the phase gate first to prevent reinforcements
				if (ThisStructure.StructureType == STRUCTURE_MARINE_PHASEGATE)
				{
					StructureToTarget = ThisStructure;
					continue;
				}

				// Then go for any turret factories, especially advanced ones to cut off siege turrets
				if (ThisStructure.StructureType == STRUCTURE_MARINE_ADVTURRETFACTORY)
				{
					StructureToTarget = ThisStructure;
					continue;
				}

				if (ThisStructure.StructureType == STRUCTURE_MARINE_TURRETFACTORY)
				{
					StructureToTarget = ThisStructure;
					continue;
				}

				// Pick up anything else
				float ThisDist = vDist2DSq(ThisStructure.Location, pBot->Edict->v.origin);

				if (FNullEnt(StructureToTarget.edict) || ThisDist < MinDist)
				{
					StructureToTarget = ThisStructure;
					MinDist = ThisDist;
				}
			}

			if (StructureToTarget.IsValid())
			{
				int NumAttackers = AITAC_GetNumPlayersOfTeamInArea(BotTeam, StructureToTarget.Location, UTIL_MetresToGoldSrcUnits(10.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

				if (NumAttackers < 2)
				{
					AITASK_SetAttackTask(pBot, Task, StructureToTarget.edict, true);
					return;
				}
			}
		}

	}

	// CHECK IF WE NEED TO HOLD AN EMPTY HIVE FOR US TO BUILD IN

	Vector EnemyBaseLocation = AITAC_GetTeamStartingLocation(EnemyTeam);

	AvHAIHiveDefinition* HiveToGuard = nullptr;

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
		if (!bShouldGuardEmptyHive) { continue; }

		AvHAIHiveDefinition* ThisHive = (*it);

		if (ThisHive->Status != HIVE_STATUS_UNBUILT) { continue; }

		bool bEnemyIsSecuring = AITAC_DeployableExistsAtLocation(ThisHive->FloorLocation, &EnemyStuffFilter);

		if (bEnemyIsSecuring) { continue; }			

		DeployableSearchFilter FriendlyStuffFilter;
		FriendlyStuffFilter.DeployableTeam = BotTeam;
		FriendlyStuffFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(15.0f);
		FriendlyStuffFilter.DeployableTypes = STRUCTURE_ALIEN_OFFENCECHAMBER;

		// Don't guard a hive if some defences are already present
		if (AITAC_GetNumDeployablesNearLocation(ThisHive->FloorLocation, &FriendlyStuffFilter) >= 2) { continue; }

		// Only guard empty hives if a gorge is in there
		if (AITAC_GetNumPlayersOfTeamAndClassInArea(BotTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(20.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2) == 0) { continue; }

		bool bNeedsExtraGuards = true;
		int NumGuards = 0;

		vector<AvHPlayer*> HumanPlayers = AIMGR_GetNonAIPlayersOnTeam(BotTeam);
		vector<AvHAIPlayer*> AITeamPlayers = AIMGR_GetAIPlayersOnTeam(BotTeam);

		for (auto AIIt = AITeamPlayers.begin(); AIIt != AITeamPlayers.end(); AIIt++)
		{
			if ((*AIIt) == pBot) { continue; }

			if ((*AIIt)->PrimaryBotTask.TaskType == TASK_GUARD && (*AIIt)->PrimaryBotTask.TaskTarget == ThisHive->HiveEdict)
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

	if (bShouldGuardEmptyHive && HiveToGuard)
	{
		Task->TaskType = TASK_GUARD;
		Task->TaskLocation = HiveToGuard->FloorLocation;
		Task->TaskTarget = HiveToGuard->HiveEdict;
		Task->TaskStartedTime = 0.0f;
		Task->TaskLength = 60.0f;
		return;
	}

	// FIND A HIVE TO RETAKE. PICK THE WEAKEST ONE

	int MaxHiveStrength = 0;
	AvHAIHiveDefinition* HiveToSecure = nullptr;

	for (auto it = AllHives.begin(); it != AllHives.end(); it++)
	{
		AvHAIHiveDefinition* ThisHive = (*it);

		if (ThisHive->OwningTeam == BotTeam) { continue; }

		vector<AvHAIBuildableStructure> EnemyStructures = AITAC_FindAllDeployables(ThisHive->FloorLocation, &EnemyStuffFilter);

		// Enemy hasn't built anything here, so doesn't need clearing
		if (ThisHive->OwningTeam != EnemyTeam && EnemyStructures.size() == 0) { continue; }

		int ThisStrength = 0;

		for (auto StructureIt = EnemyStructures.begin(); StructureIt != EnemyStructures.end(); StructureIt++)
		{
			switch (StructureIt->StructureType)
			{
				case STRUCTURE_MARINE_PHASEGATE:
					ThisStrength += 2;
					break;
				case STRUCTURE_MARINE_TURRETFACTORY:
					ThisStrength += (UTIL_IsStructureElectrified(StructureIt->edict)) ? 2 : 1;
					break;
				case STRUCTURE_MARINE_TURRET:
					ThisStrength += 1;
					break;
				case STRUCTURE_ALIEN_OFFENCECHAMBER:
					ThisStrength += 1;
					break;
				default:
					break;
			}
		}

		if (!HiveToSecure || ThisStrength < MaxHiveStrength)
		{
			HiveToSecure = ThisHive;
			MaxHiveStrength = ThisStrength;
		}
	}

	if (HiveToSecure)
	{
		AITASK_SetSecureHiveTask(pBot, Task, HiveToSecure->HiveEdict, HiveToSecure->FloorLocation, false);
		return;
	}

	// ATTACK THE ENEMY BASE

	DeployableSearchFilter EnemyInfPortalFilter;
	EnemyInfPortalFilter.DeployableTypes = STRUCTURE_MARINE_INFANTRYPORTAL;
	EnemyInfPortalFilter.DeployableTeam = EnemyTeam;
	EnemyInfPortalFilter.ReachabilityTeam = BotTeam;
	EnemyInfPortalFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyInfPortalFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
	EnemyInfPortalFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;

	AvHAIBuildableStructure EnemyInfPortal = AITAC_FindClosestDeployableToLocation(pBot->Edict->v.origin, &EnemyInfPortalFilter);

	if (EnemyInfPortal.IsValid())
	{
		AITASK_SetAttackTask(pBot, Task, EnemyInfPortal.edict, false);
		return;
	}

	// TODO: Attack enemy hive/base
	edict_t* EnemyChair = AITAC_GetCommChair(EnemyTeam);

	if (!FNullEnt(EnemyChair))
	{
		AITASK_SetAttackTask(pBot, Task, EnemyChair, false);
		return;
	}

	// FIND ANY LAST ENEMIES TO KILL AND END GAME

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
				AITASK_SetEvolveTask(pBot, Task, NearestHive->HiveEdict, ALIEN_LIFEFORM_THREE, true);
				return;
			}
			else
			{
				AITASK_SetEvolveTask(pBot, Task, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_THREE, true);
				return;
			}
		}

		if (IsPlayerGorge(pBot->Edict))
		{
			BotEvolveLifeform(pBot, pBot->CurrentFloorPosition, ALIEN_LIFEFORM_ONE);
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

	AvHAIBuildableStructure EnemyStructureToAttack;

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

	if (EnemyStructureToAttack.IsValid())
	{
		AITASK_SetAttackTask(pBot, Task, EnemyStructureToAttack.edict, false);
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

		AITASK_SetAttackTask(pBot, Task, EnemyHive->HiveEdict, false);
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

	if (pBot->DangerTurrets.size() > 0)
	{
		AvHAIBuildableStructure NearestDangerTurret;
		float MinDist = 0.0f;

		for (auto it = pBot->DangerTurrets.begin(); it != pBot->DangerTurrets.end(); it++)
		{
			float ThisDist = vDist2DSq(pBot->Edict->v.origin, (*it).Location);

			if (FNullEnt(NearestDangerTurret.edict) || ThisDist < MinDist)
			{
				NearestDangerTurret = (*it);
			}
		}

		if (NearestDangerTurret.IsValid())
		{
			if (AIMGR_GetTeamType(EnemyTeam) == AVH_CLASS_TYPE_ALIEN)
			{
				AITASK_SetAttackTask(pBot, Task, NearestDangerTurret.edict, true);
				return;
			}
			else
			{
				DeployableSearchFilter EnemyTFFilter;
				EnemyTFFilter.DeployableTeam = EnemyTeam;
				EnemyTFFilter.DeployableTypes = (STRUCTURE_MARINE_TURRETFACTORY | STRUCTURE_MARINE_ADVTURRETFACTORY);
				EnemyTFFilter.IncludeStatusFlags = STRUCTURE_STATUS_COMPLETED;
				EnemyTFFilter.ExcludeStatusFlags = STRUCTURE_STATUS_RECYCLING;
				if (pBot->Player->GetUser3() == AVH_USER3_ALIEN_PLAYER1 || pBot->Player->GetUser3() == AVH_USER3_ALIEN_PLAYER3)
				{
					EnemyTFFilter.ExcludeStatusFlags |= STRUCTURE_STATUS_ELECTRIFIED;
				}
				
				EnemyTFFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);
				EnemyTFFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
				EnemyTFFilter.ReachabilityTeam = BotTeam;

				AvHAIBuildableStructure EnemyTF = AITAC_FindClosestDeployableToLocation(NearestDangerTurret.Location, &EnemyTFFilter);

				if (EnemyTF.IsValid())
				{
					AITASK_SetAttackTask(pBot, Task, EnemyTF.edict, true);
					return;
				}
				else
				{
					AITASK_SetAttackTask(pBot, Task, NearestDangerTurret.edict, true);
					return;
				}
			}
		}
	}

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

		vector<AvHAIBuildableStructure> AllNearbyStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &DamagedStructuresFilter);

		edict_t* StructureToHeal = nullptr;

		MinDist = 0.0f;

		for (auto it = AllNearbyStructures.begin(); it != AllNearbyStructures.end(); it++)
		{
			AvHAIBuildableStructure ThisStructure = (*it);

			if (!FNullEnt(ThisStructure.edict) && ThisStructure.healthPercent < 0.99f)
			{
				float ThisDist = vDist2DSq(pBot->Edict->v.origin, ThisStructure.Location);
				if (FNullEnt(StructureToHeal) || ThisDist < MinDist)
				{
					StructureToHeal = ThisStructure.edict;
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

	if (pBot->PrimaryBotTask.TaskType == TASK_EVOLVE)
	{
		AITASK_ClearBotTask(pBot, &pBot->SecondaryBotTask);
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
			float DistToHive = vDist2DSq(ThisHive->FloorLocation, pBot->Edict->v.origin);

			int AttackerStrength = 0;
			int DefenderStrength = 0;

			vector<AvHPlayer*> AttackingPlayers = AITAC_GetAllPlayersOfTeamInArea(EnemyTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(20.0f), false, nullptr, AVH_USER3_NONE);
			
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

			vector<AvHPlayer*> DefendingPlayers = AITAC_GetAllPlayersOfTeamInArea(BotTeam, ThisHive->FloorLocation, UTIL_MetresToGoldSrcUnits(20.0f), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

			for (auto DefenderIt = DefendingPlayers.begin(); DefenderIt != DefendingPlayers.end(); DefenderIt++)
			{
				AvHPlayer* ThisPlayer = (*DefenderIt);
				edict_t* ThisPlayerEdict = ThisPlayer->edict();

				float ThisPlayerDist = vDist2DSq(ThisPlayerEdict->v.origin, ThisHive->FloorLocation);

				// This potential defender is further than us, don't count them in the strength measurements
				if (ThisPlayerDist > DistToHive) { continue; }
				
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

				if (ThisBot != pBot && IsPlayerActiveInGame(ThisBot->Edict) && GetPlayerActiveClass(pBot->Player) != AVH_USER3_ALIEN_PLAYER2 && vDist2DSq(ThisBot->Edict->v.origin, ThisHive->FloorLocation) > sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
				{
					if (ThisBot->SecondaryBotTask.TaskType == TASK_DEFEND && ThisBot->SecondaryBotTask.TaskTarget == ThisHive->HiveEdict)
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
				if (!HiveToDefend || DistToHive < MinDist)
				{
					HiveToDefend = ThisHive;
					MinDist = DistToHive;
				}
			}
		}
	}

	if (HiveToDefend)
	{
		AITASK_SetDefendTask(pBot, Task, HiveToDefend->HiveEdict, true);
		return;
	}

	// If we're engaging an enemy turret then finish the job
	if (Task->TaskType == TASK_ATTACK)
	{
		if (vDist2DSq(pBot->Edict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			return;
		}
	}

	if (pBot->PrimaryBotTask.TaskType == TASK_CAP_RESNODE)
	{
		float ResourceCost = BALANCE_VAR(kResourceTowerCost);
		if (!IsPlayerGorge(pBot->Edict))
		{
			ResourceCost += BALANCE_VAR(kGorgeCost);
		}

		if (pBot->Player->GetResources() >= ResourceCost)
		{
			AITASK_ClearBotTask(pBot, &pBot->SecondaryBotTask);
			return;
		}
	}

	DeployableSearchFilter AttackedStructuresFilter;
	AttackedStructuresFilter.DeployableTypes = (IsPlayerLerk(pBot->Edict)) ? SEARCH_ALL_STRUCTURES : STRUCTURE_ALIEN_RESTOWER;
	AttackedStructuresFilter.DeployableTeam = BotTeam;
	AttackedStructuresFilter.ReachabilityTeam = BotTeam;
	AttackedStructuresFilter.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	AttackedStructuresFilter.IncludeStatusFlags = STRUCTURE_STATUS_UNDERATTACK;
	AttackedStructuresFilter.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(30.0f);

	vector<AvHAIBuildableStructure> AllAttackedStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &AttackedStructuresFilter);

	AvHAIBuildableStructure StructureToDefend;
	MinDist = 0.0f;

	for (auto it = AllAttackedStructures.begin(); it != AllAttackedStructures.end(); it++)
	{
		AvHAIBuildableStructure ThisStructure = (*it);

		float ThisDist = vDist2D(pBot->Edict->v.origin, ThisStructure.edict->v.origin);

		int NumAttackers = AITAC_GetNumPlayersOnTeamWithLOS(EnemyTeam, ThisStructure.Location, UTIL_MetresToGoldSrcUnits(15.0f), nullptr);

		if (NumAttackers == 0) { continue; }

		int NumExistingDefenders = AITAC_GetNumPlayersOfTeamInArea(BotTeam, ThisStructure.Location, ThisDist - 10.0f, false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

		if (NumExistingDefenders < 2)
		{
			if (FNullEnt(StructureToDefend.edict) || ThisDist < MinDist)
			{
				StructureToDefend = ThisStructure;
				MinDist = ThisDist;
			}
		}
	}

	if (StructureToDefend.IsValid())
	{
		AITASK_SetDefendTask(pBot, Task, StructureToDefend.edict, true);
		return;
	}

	DeployableSearchFilter EnemyStructures;
	EnemyStructures.DeployableTypes = SEARCH_ALL_STRUCTURES;
	EnemyStructures.DeployableTeam = BotTeam;
	EnemyStructures.ReachabilityTeam = BotTeam;
	EnemyStructures.ReachabilityFlags = pBot->BotNavInfo.NavProfile.ReachabilityFlag;
	EnemyStructures.MaxSearchRadius = UTIL_MetresToGoldSrcUnits(10.0f);

	vector<AvHAIBuildableStructure> NearbyEnemyStructures = AITAC_FindAllDeployables(pBot->Edict->v.origin, &EnemyStructures);

	for (auto it = NearbyEnemyStructures.begin(); it != NearbyEnemyStructures.end(); it++)
	{
		if (UTIL_PlayerHasLOSToEntity(pBot->Edict, it->edict, UTIL_MetresToGoldSrcUnits(20.0f), false))
		{
			AITASK_SetAttackTask(pBot, Task, it->edict, false);
			return;
		}
	}

}

void AIPlayerSetWantsAndNeedsAlienTask(AvHAIPlayer* pBot, AvHAIPlayerTask* Task)
{
	float CurrentHealth = GetPlayerOverallHealthPercent(pBot->Edict);

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = AIMGR_GetEnemyTeam(BotTeam);

	if (PlayerHasWeapon(pBot->Player, WEAPON_LERK_PRIMALSCREAM) && !pBot->Player->GetIsScreaming())
	{
		vector<AvHPlayer*> NearbyAllies = AITAC_GetAllPlayersOfTeamInArea(pBot->Player->GetTeam(), pBot->Edict->v.origin, BALANCE_VAR(kPrimalScreamRange), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);
		int NumBuffTargets = 0;

		for (auto it = NearbyAllies.begin(); it != NearbyAllies.end(); it++)
		{
			AvHPlayer* ThisPlayer = (*it);
			edict_t* ThisPlayerEdict = ThisPlayer->edict();

			if (AITAC_AnyPlayerOnTeamWithLOS(EnemyTeam, ThisPlayerEdict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				NumBuffTargets++;
			}
		}

		if (NumBuffTargets > 0)
		{
			pBot->DesiredCombatWeapon = WEAPON_LERK_PRIMALSCREAM;

			if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_LERK_PRIMALSCREAM)
			{
				pBot->Button |= IN_ATTACK;
			}

			return;
		}
		
	}

	if (Task->TaskType == TASK_GET_HEALTH)
	{
		Task->bTaskIsUrgent = Task->bTaskIsUrgent || CurrentHealth < 0.4f;
		return;
	}

	if (gpGlobals->time - pBot->LastCombatTime > 10.0f)
	{
		if (Task->TaskType == TASK_EVOLVE) { return; }

		bool bInMiddleOfMove = false;

		if (pBot->BotNavInfo.CurrentPath.size() > 0 && pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size())
		{
			bot_path_node CurrentMove = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

			bInMiddleOfMove = CurrentMove.flag != SAMPLE_POLYFLAGS_WALK;
		}

		if (!bInMiddleOfMove)
		{
			if (!PlayerHasAlienUpgradeOfType(pBot->Edict, HIVE_TECH_DEFENCE) && AITAC_IsAlienUpgradeAvailableForTeam(pBot->Player->GetTeam(), HIVE_TECH_DEFENCE))
			{
				AITASK_SetEvolveTask(pBot, Task, pBot->CurrentFloorPosition, AlienGetDesiredUpgrade(pBot, HIVE_TECH_DEFENCE), true);
				return;
			}

			if (!PlayerHasAlienUpgradeOfType(pBot->Edict, HIVE_TECH_MOVEMENT) && AITAC_IsAlienUpgradeAvailableForTeam(pBot->Player->GetTeam(), HIVE_TECH_MOVEMENT))
			{
				AITASK_SetEvolveTask(pBot, Task, pBot->CurrentFloorPosition, AlienGetDesiredUpgrade(pBot, HIVE_TECH_MOVEMENT), true);
				return;
			}

			if (!PlayerHasAlienUpgradeOfType(pBot->Edict, HIVE_TECH_SENSORY) && AITAC_IsAlienUpgradeAvailableForTeam(pBot->Player->GetTeam(), HIVE_TECH_SENSORY))
			{
				AITASK_SetEvolveTask(pBot, Task, pBot->CurrentFloorPosition, AlienGetDesiredUpgrade(pBot, HIVE_TECH_SENSORY), true);
				return;
			}
		}
	}


	if (CurrentHealth >= 1.0f) { return; }

	bool bCanSelfHeal = (PlayerHasWeapon(pBot->Player, WEAPON_GORGE_HEALINGSPRAY) || PlayerHasWeapon(pBot->Player, WEAPON_FADE_METABOLIZE));

	if (CurrentHealth < 0.95f && bCanSelfHeal && gpGlobals->time - pBot->LastCombatTime > 5.0f)
	{
		pBot->DesiredCombatWeapon = (PlayerHasWeapon(pBot->Player, WEAPON_FADE_METABOLIZE)) ? WEAPON_FADE_METABOLIZE : WEAPON_GORGE_HEALINGSPRAY;

		if (GetPlayerCurrentWeapon(pBot->Player) == pBot->DesiredCombatWeapon)
		{
			pBot->Button |= IN_ATTACK;
		}
	}

	// Only look for gorges as a healing source if we're something with low health like a skulk or lerk, or we have over 50% health. Don't use gorges if we're near death as an Onos or it will take forever...
	edict_t* NearestHealingSource = AITAC_AlienFindNearestHealingSource(pBot->Player->GetTeam(), pBot->Edict->v.origin, pBot->Edict, (IsPlayerSkulk(pBot->Edict) || IsPlayerLerk(pBot->Edict) || CurrentHealth > 0.5f));

	if (FNullEnt(NearestHealingSource)) { return; }

	float GetHealthThreshold = 0.6f;

	if (IsPlayerOnos(pBot->Edict))
	{
		GetHealthThreshold = 0.33f;
	}

	if (IsPlayerFade(pBot->Edict))
	{
		GetHealthThreshold = 0.5f;
	}

	// If we're right by a healing source, then might as well heal up, set the "find health" threshold to 90% or less health
	if (vDist2DSq(pBot->Edict->v.origin, NearestHealingSource->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		GetHealthThreshold = 0.9f;
	}
	else
	{
		// If we can heal ourselves, then don't go running for the hive/DCs/Gorge unless we're very low
		if (bCanSelfHeal)
		{
			GetHealthThreshold = 0.4f;
		}
	}

	// If we're a skulk and we're attacking something, don't bother going to get health. DEATH OR GLORY.
	if (IsPlayerSkulk(pBot->Edict))
	{
		if (pBot->PrimaryBotTask.TaskType == TASK_ATTACK)
		{
			if (pBot->PrimaryBotTask.bTaskIsUrgent || vDist2DSq(pBot->Edict->v.origin, pBot->PrimaryBotTask.TaskTarget->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
			{
				AITASK_ClearBotTask(pBot, Task);
				return;
			}
		}

		if (pBot->SecondaryBotTask.TaskType == TASK_ATTACK)
		{
			if (pBot->SecondaryBotTask.bTaskIsUrgent || vDist2DSq(pBot->Edict->v.origin, pBot->SecondaryBotTask.TaskTarget->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
			{
				AITASK_ClearBotTask(pBot, Task);
				return;
			}
		}
	}	

	if (CurrentHealth < GetHealthThreshold)
	{
		AITASK_SetGetHealthTask(pBot, Task, NearestHealingSource, true);
	}
}

bool AlienCombatThink(AvHAIPlayer* pBot)
{
	if (pBot->CurrentEnemy > -1)
	{
		edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].PlayerEdict;

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

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];
	AvHPlayer* EnemyPlayer = TrackedEnemyRef->PlayerRef;
	edict_t* CurrentEnemy = TrackedEnemyRef->PlayerEdict;	

	AvHTeamNumber BotTeam = pBot->Player->GetTeam();
	AvHTeamNumber EnemyTeam = EnemyPlayer->GetTeam();

	float DistToEnemy = vDist2DSq(pBot->Edict->v.origin, TrackedEnemyRef->LastDetectedLocation);

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

			if (!UTIL_PlayerHasLOSToLocation(TrackedEnemyRef->PlayerEdict, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
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

		bool bIsCloaked = (UTIL_IsCloakedPlayerInvisible(CurrentEnemy, pBot->Player) || pBot->Player->GetOpacity() < 0.5f);

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

		if (!bIsCloaked && DesiredWeapon != WEAPON_SKULK_XENOCIDE)
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

		BotMoveStyle DesiredMoveStyle = (bIsCloaked) ? MOVESTYLE_AMBUSH : MOVESTYLE_NORMAL;
		pBot->BotNavInfo.bShouldWalk = bIsCloaked && !GetHasUpgrade(pBot->Edict->v.iuser4, MASK_SENSORY_NEARBY);

		MoveTo(pBot, MoveTarget, DesiredMoveStyle);

		if (!bIsCloaked && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			if (CanBotLeap(pBot))
			{
				BotLeap(pBot, CurrentEnemy->v.origin);
			}
			else
			{
				if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size()) { return true; }

				bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

				// EVASIVE MANOEUVRES! Only do this if we're running along the floor and aren't approaching a path point (so we don't stray off the path)
				if (CurrentPathNode.flag == SAMPLE_POLYFLAGS_WALK && vDist2DSq(pBot->Edict->v.origin, CurrentPathNode.Location) > sqrf(50.0f))
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
			BotLookAt(pBot, TrackedEnemyRef->LastDetectedLocation);

			if (!IsPlayerParasited(CurrentEnemy))
			{
				BotShootTarget(pBot, WEAPON_SKULK_PARASITE, CurrentEnemy);
			}

			return true;
		}
		else
		{
			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastDetectedLocation);
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

			BotLookAt(pBot, TrackedEnemyRef->LastDetectedLocation);

			BotShootTarget(pBot, WEAPON_SKULK_PARASITE, CurrentEnemy);

			return true;
		}
		else
		{
			if (GetPlayerEnergy(pBot->Edict) >= 0.9f)
			{
				MoveTo(pBot, TrackedEnemyRef->LastDetectedLocation, MOVESTYLE_NORMAL);
				return true;
			}
			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastDetectedLocation);
		}

		return true;
	}

	return false;
}

bool GorgeCombatThink(AvHAIPlayer* pBot)
{
	AvHTeamNumber BotTeam = pBot->Player->GetTeam();

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].PlayerEdict;

	if (FNullEnt(CurrentEnemy) || !IsPlayerActiveInGame(CurrentEnemy)) { return false; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	float CurrentHealthPercent = GetPlayerOverallHealthPercent(pBot->Edict);

	bool bPlayerCloaked = UTIL_IsCloakedPlayerInvisible(CurrentEnemy, pBot->Player);

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
				pBot->BotNavInfo.bShouldWalk = bPlayerCloaked && TrackedEnemyRef->bEnemyHasLOS;

				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

				if (!bPlayerCloaked)
				{
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

			// If we're invisible and healing, do nothing. Don't give our position away
			if (bPlayerCloaked)
			{
				if (TrackedEnemyRef->bHasLOS)
				{
					BotLookAt(pBot, TrackedEnemyRef->LastDetectedLocation);
				}
				else
				{
					BotLookAt(pBot, TrackedEnemyRef->LastLOSPosition);
				}

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

			if (!UTIL_PlayerHasLOSToLocation(TrackedEnemyRef->PlayerEdict, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
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
			MoveTo(pBot, TrackedEnemyRef->LastDetectedLocation, MOVESTYLE_NORMAL);
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

			BotLookAt(pBot, TrackedEnemyRef->LastDetectedLocation);

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
				MoveTo(pBot, TrackedEnemyRef->LastDetectedLocation, MOVESTYLE_NORMAL);
				return true;
			}

			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastDetectedLocation);

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
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];
	edict_t* CurrentEnemy = TrackedEnemyRef->PlayerEdict;

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

			Vector SporeLocation = (TrackedEnemyRef->bHasLOS) ? TrackedEnemyRef->LastDetectedLocation : TrackedEnemyRef->LastLOSPosition;

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

			if (!UTIL_PlayerHasLOSToLocation(TrackedEnemyRef->PlayerEdict, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				return true;
			}

		}

		return true;
	}

	if (pBot->CurrentCombatStrategy == COMBAT_STRATEGY_ATTACK)
	{
		if (PlayerHasWeapon(pBot->Player, WEAPON_LERK_PRIMALSCREAM) && !pBot->Player->GetIsScreaming() && GetPlayerEnergy(pBot->Edict) >= BALANCE_VAR(kPrimalScreamEnergyCost))
		{
			pBot->DesiredCombatWeapon = WEAPON_LERK_PRIMALSCREAM;

			if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_LERK_PRIMALSCREAM)
			{
				pBot->Button |= IN_ATTACK;
			}

			return true;
		}

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
		if (PlayerHasWeapon(pBot->Player, WEAPON_LERK_PRIMALSCREAM) && !pBot->Player->GetIsScreaming() && GetPlayerEnergy(pBot->Edict) >= BALANCE_VAR(kPrimalScreamEnergyCost))
		{
			int NumBuffTargets = AITAC_GetNumPlayersOfTeamInArea(pBot->Player->GetTeam(), pBot->Edict->v.origin, BALANCE_VAR(kPrimalScreamRange), false, pBot->Edict, AVH_USER3_ALIEN_PLAYER2);

			if (NumBuffTargets > 0)
			{
				pBot->DesiredCombatWeapon = WEAPON_LERK_PRIMALSCREAM;

				if (GetPlayerCurrentWeapon(pBot->Player) == WEAPON_LERK_PRIMALSCREAM)
				{
					pBot->Button |= IN_ATTACK;
				}

				return true;
			}
		}

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
				BotLookAt(pBot, TrackedEnemyRef->LastDetectedLocation);
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
			MoveTo(pBot, TrackedEnemyRef->LastDetectedLocation, MOVESTYLE_AMBUSH);
		}

		return true;
	}

	return true;
}

bool FadeCombatThink(AvHAIPlayer* pBot)
{
	edict_t* pEdict = pBot->Edict;

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];
	AvHPlayer* EnemyPlayer = TrackedEnemyRef->PlayerRef;
	edict_t* CurrentEnemy = TrackedEnemyRef->PlayerEdict;
	

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

			if (!bInHealingRange && GetPlayerOverallHealthPercent(pBot->Edict) < 0.5f)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

				// If we're still in danger while retreating, do extra leaping to get the hell out
				if (TrackedEnemyRef->bHasLOS)
				{
					if (pBot->BotNavInfo.CurrentPath.size() > 0 && pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size())
					{
						bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

						if (CurrentPathNode.flag != SAMPLE_POLYFLAGS_WALLCLIMB && CurrentPathNode.flag != SAMPLE_POLYFLAGS_LIFT)
						{							
							BotLeap(pBot, CurrentPathNode.Location);
						}
						
					}
				}
				else
				{
					if (PlayerHasWeapon(pBot->Player, WEAPON_FADE_METABOLIZE))
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

			// If we're not in immediate danger, and we're either healing up at the source, or we can metabolize, then wait a bit and catch our breath
			if (bOutOfEnemyLOS && (bInHealingRange || PlayerHasWeapon(pBot->Player, WEAPON_FADE_METABOLIZE)))
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

			if (!UTIL_PlayerHasLOSToLocation(TrackedEnemyRef->PlayerEdict, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

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

			// If the enemy can see the healing source, then we must go on the attack as we're cornered
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
			BotLookAt(pBot, TrackedEnemyRef->LastDetectedLocation);

			if (PlayerHasWeapon(pBot->Player, WEAPON_FADE_ACIDROCKET))
			{
				BotShootTarget(pBot, WEAPON_FADE_ACIDROCKET, CurrentEnemy);
			}

			return true;
		}
		else
		{
			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastDetectedLocation);

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

			BotLookAt(pBot, TrackedEnemyRef->LastDetectedLocation);

			BotShootTarget(pBot, WEAPON_FADE_ACIDROCKET, CurrentEnemy);

			return true;
		}
		else
		{
			if (GetPlayerEnergy(pBot->Edict) >= 0.9f && GetPlayerOverallHealthPercent(pBot->Edict) > 0.8f)
			{
				MoveTo(pBot, TrackedEnemyRef->LastDetectedLocation, MOVESTYLE_NORMAL);
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

			BotLookAt(pBot, (!vIsZero(TrackedEnemyRef->LastLOSPosition)) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastDetectedLocation);
		}

		return true;
	}

	return true;
}

bool OnosCombatThink(AvHAIPlayer* pBot)
{
	edict_t* pEdict = pBot->Edict;

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];
	AvHPlayer* EnemyPlayer = TrackedEnemyRef->PlayerRef;
	edict_t* CurrentEnemy = TrackedEnemyRef->PlayerEdict;
	

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

			if (!UTIL_PlayerHasLOSToLocation(CurrentEnemy, UTIL_GetEntityGroundLocation(NearestHealingSource) + Vector(0.0f, 0.0f, 16.0f), UTIL_MetresToGoldSrcUnits(30.0f)))
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

void DEBUG_PrintTaskInfo(edict_t* OutputPlayer, AvHAIPlayer* pBot)
{
	char buf[511];
	char interbuf[164];

	sprintf(buf, "Task info for %s:\n\n", STRING(pBot->Edict->v.netname));

	sprintf(interbuf, "Role: %s\n\n", UTIL_BotRoleToChar(pBot->BotRole));
	strcat(buf, interbuf);

	if (IsPlayerMarine(pBot->Edict))
	{
		const char* CommanderTask = UTIL_TaskTypeToChar(pBot->CommanderTask.TaskType);

		sprintf(interbuf, "Commander-Issued: %s\n", CommanderTask);
		strcat(buf, interbuf);
	}

	const char* PrimaryTask = UTIL_TaskTypeToChar(pBot->PrimaryBotTask.TaskType);

	sprintf(interbuf, "Primary: %s\n", PrimaryTask);
	strcat(buf, interbuf);

	const char* SecondaryTask = UTIL_TaskTypeToChar(pBot->SecondaryBotTask.TaskType);

	sprintf(interbuf, "Secondary: %s\n", SecondaryTask);
	strcat(buf, interbuf);

	const char* WantAndNeedTask = UTIL_TaskTypeToChar(pBot->WantsAndNeedsTask.TaskType);

	sprintf(interbuf, "Want/Need: %s\n\n", WantAndNeedTask);
	strcat(buf, interbuf);

	if (pBot->CurrentTask)
	{
		const char* CurrentTask = UTIL_TaskTypeToChar(pBot->CurrentTask->TaskType);

		sprintf(interbuf, "Current: %s\n\n", CurrentTask);
		strcat(buf, interbuf);
	}

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		sprintf(interbuf, "Red = Target, Yellow = Location\n");
		strcat(buf, interbuf);

		if (!FNullEnt(pBot->CurrentTask->TaskTarget))
		{
			UTIL_DrawLine(OutputPlayer, pBot->Edict->v.origin, pBot->CurrentTask->TaskTarget->v.origin, 255, 0, 0);
		}

		if (!vIsZero(pBot->CurrentTask->TaskLocation))
		{
			UTIL_DrawLine(OutputPlayer, pBot->Edict->v.origin, pBot->CurrentTask->TaskLocation, 255, 255, 0);
		}
	}

	UTIL_DrawHUDText(OutputPlayer, 0, 0.1, 0.1f, 255, 255, 255, buf);
}

void DEBUG_PrintCombatInfo(edict_t* OutputPlayer, AvHAIPlayer* pBot)
{
	char buf[511];
	char interbuf[164];

	sprintf(buf, "Combat info for %s:\n\n", STRING(pBot->Edict->v.netname));

	int TrackedEnemy = BotGetNextEnemyTarget(pBot);

	if (TrackedEnemy < 0)
	{
		sprintf(interbuf, "Main Threat: NONE\n\n");
	}
	else
	{
		sprintf(interbuf, "Main Threat: %s\n\n", STRING(pBot->TrackedEnemies[TrackedEnemy].PlayerEdict->v.netname));
	}
		
	strcat(buf, interbuf);

	if (TrackedEnemy < 0)
	{
		UTIL_DrawHUDText(OutputPlayer, 1, 0.6f, 0.1f, 255, 255, 255, buf);
		return;
	}

	enemy_status* TrackedInfo = &pBot->TrackedEnemies[TrackedEnemy];

	sprintf(interbuf, "Awareness: %.2f\n", TrackedInfo->AwarenessOfPlayer);
	strcat(buf, interbuf);

	float TimeSinceLastDetection = gpGlobals->time - TrackedInfo->LastDetectedTime;

	sprintf(interbuf, "Last detected: %.2f\n", TimeSinceLastDetection);
	strcat(buf, interbuf);

	float TimeSinceLastSeen = gpGlobals->time - TrackedInfo->LastVisibleTime;

	if (TrackedInfo->LastVisibleTime > 0.0f)
	{
		sprintf(interbuf, "Last seen: %.2f\n", TimeSinceLastSeen);
	}
	else
	{
		sprintf(interbuf, "Last seen: NEVER\n");
	}

	strcat(buf, interbuf);

	sprintf(interbuf, "Threat: %.2f\n\n", TrackedInfo->EnemyThreatLevel);
	strcat(buf, interbuf);

	AvHAICombatStrategy CurrentStrategy = GetBotCombatStrategyForTarget(pBot, TrackedInfo);

	switch (CurrentStrategy)
	{
		case COMBAT_STRATEGY_AMBUSH:
			sprintf(interbuf, "Strategy: Ambush\n");
			break;
		case COMBAT_STRATEGY_ATTACK:
			sprintf(interbuf, "Strategy: Attack\n");
			break;
		case COMBAT_STRATEGY_IGNORE:
			sprintf(interbuf, "Strategy: Ignore\n");
			break;
		case COMBAT_STRATEGY_RETREAT:
			sprintf(interbuf, "Strategy: Retreat\n");
			break;
		case COMBAT_STRATEGY_SKIRMISH:
			sprintf(interbuf, "Strategy: Skirmish\n");
			break;
		default:
			sprintf(interbuf, "Strategy: None\n");
			break;
	}

	strcat(buf, interbuf);

	UTIL_DrawHUDText(OutputPlayer, 1, 0.6f, 0.1f, 255, 255, 255, buf);

	if (!vIsZero(TrackedInfo->LastDetectedLocation))
	{
		UTIL_DrawLine(OutputPlayer, pBot->Edict->v.origin, TrackedInfo->LastDetectedLocation, 255, 0, 0);
	}

}

void DEBUG_PrintBotDebugInfo(edict_t* OutputPlayer, AvHAIPlayer* pBot)
{
	if (FNullEnt(OutputPlayer) || OutputPlayer->free) { return; }

	bool bBreak = true; // Add a break point here if you want to debug a specific bot

	AIDEBUG_DrawBotPath(OutputPlayer, pBot);

	DEBUG_PrintTaskInfo(OutputPlayer, pBot);
	DEBUG_PrintCombatInfo(OutputPlayer, pBot);
}