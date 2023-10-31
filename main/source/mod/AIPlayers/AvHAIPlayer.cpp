
#include "AvHAIPlayer.h"
#include "AvHAIPlayerUtil.h"
#include "AvHAIHelper.h"
#include "AvHAIMath.h"
#include "AvHAIHelper.h"
#include "AvHAINavigation.h"
#include "AvHAIWeaponHelper.h"
#include "AvHAITactical.h"
#include "AvHAITask.h"

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

	AvHAIWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

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

AvHAIWeapon GetBotCurrentWeapon(const AvHAIPlayer* pBot)
{
	AvHBasePlayerWeapon* theBasePlayerWeapon = dynamic_cast<AvHBasePlayerWeapon*>(pBot->Player->m_pActiveItem);

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

	if (GetBotCurrentWeapon(pBot) != LeapWeapon)
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


void LinkDeployedObjectToCommanderAction(AvHAIPlayer* Commander, AvHAIBuildableStructure* NewStructure)
{
	if (!Commander || !NewStructure || FNullEnt(Commander->Edict)) { return; }

	commander_action* Action = nullptr;

	if (Commander->BuildAction.bIsAwaitingBuildLink && Commander->BuildAction.StructureToBuild == NewStructure->StructureType)
	{
		if (vDist2DSq(Commander->BuildAction.BuildLocation, NewStructure->Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			Action = &Commander->BuildAction;
		}
	}

	if (!Action)
	{
		if (Commander->SupportAction.bIsAwaitingBuildLink && Commander->SupportAction.StructureToBuild == NewStructure->StructureType)
		{
			if (vDist2DSq(Commander->SupportAction.BuildLocation, NewStructure->Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
			{
				Action = &Commander->SupportAction;
			}
		}
	}

	if (!Action) { return; }

	NewStructure->LastSuccessfulCommanderLocation = Action->LastAttemptedCommanderLocation;
	NewStructure->LastSuccessfulCommanderAngle = Action->LastAttemptedCommanderAngle;
	NewStructure->Purpose = Action->ActionPurpose;

	float CoolDown = (Action->NumDesiredInstances > 1) ? 0.33f : commander_action_cooldown;

	Commander->next_commander_action_time = gpGlobals->time + CoolDown;

	Action->NumInstances++;

	if (Action->NumDesiredInstances > 1)
	{
		Action->BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BaseNavProfiles[MARINE_BASE_NAV_PROFILE], Action->BuildLocation, UTIL_MetresToGoldSrcUnits(1.0f));
	}

	Action->bIsAwaitingBuildLink = false;

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
		AvHAIWeapon PrimaryWeapon = UTIL_GetBotPrimaryWeapon(pBot);
		AvHAIWeapon SecondaryWeapon = GetBotMarineSecondaryWeapon(pBot);
		AvHAIWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

		if (WeaponCanBeReloaded(PrimaryWeapon) && BotGetPrimaryWeaponClipAmmo(pBot) < BotGetPrimaryWeaponMaxClipSize(pBot) && BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
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

void BotAttackTarget(AvHAIPlayer* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	AvHAIWeapon Weapon = WEAPON_INVALID;

	if (IsPlayerMarine(pBot->Edict))
	{
		Weapon = BotMarineChooseBestWeaponForStructure(pBot, Target);
	}
	else
	{
		Weapon = BotAlienChooseBestWeaponForStructure(pBot, Target);
	}

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

		if (IsPlayerLerk(pBot->Edict))
		{
			if (AITAC_ShouldBotBeCautious(pBot))
			{
				MoveTo(pBot, Target->v.origin, MOVESTYLE_HIDE, 100.0f);
			}
			else
			{
				MoveTo(pBot, Target->v.origin, MOVESTYLE_NORMAL, 100.0f);
			}

			return;
		}

		Vector AttackPoint = Target->v.origin;

		if (StructureType == STRUCTURE_ALIEN_HIVE)
		{
			const AvHAIHiveDefinition* HiveDefinition = AITAC_GetHiveFromEdict(Target);

			if (HiveDefinition)
			{
				AttackPoint = HiveDefinition->FloorLocation;
			}
		}

		MoveTo(pBot, AttackPoint, MOVESTYLE_NORMAL, WeaponRange);

		if (IsPlayerMarine(pBot->Edict))
		{
			if (gpGlobals->time - pBot->LastCombatTime > 5.0f)
			{
				BotReloadWeapons(pBot);
			}
		}

		return;
	}

	if (AttackResult == ATTACK_BLOCKED)
	{
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

void BotShootTarget(AvHAIPlayer* pBot, AvHAIWeapon AttackWeapon, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	AvHAIWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

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
		bool bShouldReload = (BotGetCurrentWeaponReserveAmmo(pBot) > 0);

		if (CurrentWeapon == WEAPON_MARINE_SHOTGUN && IsEdictStructure(Target))
		{
			bShouldReload = bShouldReload && ((float)BotGetCurrentWeaponClipAmmo(pBot) / (float)BotGetCurrentWeaponMaxClipAmmo(pBot) < 0.5f);
		}
		else
		{
			bShouldReload = bShouldReload && BotGetCurrentWeaponClipAmmo(pBot) == 0;
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

	AvHAIWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

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
		bool bShouldReload = (BotGetCurrentWeaponReserveAmmo(pBot) > 0);

		if (CurrentWeapon == WEAPON_MARINE_SHOTGUN)
		{
			bShouldReload = bShouldReload && ((float)BotGetCurrentWeaponClipAmmo(pBot) / (float)BotGetCurrentWeaponMaxClipAmmo(pBot) < 0.5f);
		}
		else
		{
			bShouldReload = bShouldReload && BotGetCurrentWeaponClipAmmo(pBot) == 0;
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

void BotEvolveLifeform(AvHAIPlayer* pBot, AvHMessageID TargetLifeform)
{
	pBot->Impulse = TargetLifeform;
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
	return IsPlayerActiveInGame(pBot->Edict) && !IsPlayerGestating(pBot->Edict);
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
					AITASK_SetMoveTask(pBot, &pBot->CommanderTask, OrderLocation, true);
					break;
				case ORDERTYPET_BUILD:
					AITASK_SetBuildTask(pBot, &pBot->CommanderTask, INDEXENT(it->GetTargetIndex()), true);
					break;
				default:
					break;
			}
		}
	}
}