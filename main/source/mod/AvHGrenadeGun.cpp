//======== (C) Copyright 2002 Charles G. Cleveland All rights reserved. =========
//
// The copyright to the contents herein is the property of Charles G. Cleveland.
// The contents may be used and/or copied only with the written permission of
// Charles G. Cleveland, or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile: AvHGrenadeGun.cpp $
// $Date: 2002/11/22 21:28:16 $
//
//-------------------------------------------------------------------------------
// $Log: AvHGrenadeGun.cpp,v $
// Revision 1.17  2002/11/22 21:28:16  Flayra
// - mp_consistency changes
//
// Revision 1.16  2002/10/16 20:53:09  Flayra
// - Removed weapon upgrade sounds
//
// Revision 1.15  2002/10/03 18:46:26  Flayra
// - Added heavy view model
//
// Revision 1.14  2002/07/24 19:09:16  Flayra
// - Linux issues
//
// Revision 1.13  2002/07/24 18:45:41  Flayra
// - Linux and scripting changes
//
// Revision 1.12  2002/06/25 17:50:59  Flayra
// - Reworking for correct player animations and new enable/disable state, new view model artwork, alien weapon refactoring
//
// Revision 1.11  2002/06/03 16:37:56  Flayra
// - Added different deploy times (this should be refactored a bit more), refactored grenades
//
// Revision 1.10  2002/05/28 17:44:58  Flayra
// - Tweak weapon deploy volume, as Valve's sounds weren't normalized
//
// Revision 1.9  2002/05/23 02:33:42  Flayra
// - Post-crash checkin.  Restored @Backup from around 4/16.  Contains changes for last four weeks of development.
//
//===============================================================================
#include "AvHPlayer.h"

#ifdef AVH_CLIENT
#include "cl_dll/eventscripts.h"
#include "cl_dll/in_defs.h"
#include "cl_dll/wrect.h"
#include "cl_dll/cl_dll.h"
#endif

#include "../common/hldm.h"
#include "../common/event_api.h"
#include "../common/event_args.h"
#include "../common/vector_util.h"
#include "AvHMarineWeapons.h"
#include "AvHMarineWeaponConstants.h"
#include "AvHServerUtil.h"

LINK_ENTITY_TO_CLASS(kwGrenadeGun, AvHGrenadeGun);
void V_PunchAxis( int axis, float punch );

const int	kSpecialReloadNone = 0;
const int	kSpecialReloadGotoReload = 1;
const int	kSpecialReloadAddGren = 2;
const int	kSpecialReloadCloseGG = 3;
const float kEndReloadAnimationTime = 2.43f;

void AvHGrenadeGun::Init()
{
	this->mRange = kGGRange;
	this->mDamage = BALANCE_VAR(kGrenadeDamage);
	this->m_fInSpecialReload = kSpecialReloadNone;
}

int	AvHGrenadeGun::GetBarrelLength() const
{
	return kGGBarrelLength;
}

float AvHGrenadeGun::GetRateOfFire() const
{
	return BALANCE_VAR(kGGROF);
}

int	AvHGrenadeGun::GetDeployAnimation() const
{
	int theAnimation = -1;
	
	int theShotsInClip = this->GetShotsInClip();
	
	switch(theShotsInClip)
	{
	case 4:
	case 0:
		theAnimation = 13;
		break;
	case 3:
		theAnimation = 14;
		break;
	case 2:
		theAnimation = 15;
		break;
	case 1:
		theAnimation = 16;
		break;
	}
	
	return theAnimation;
}

char* AvHGrenadeGun::GetDeploySound() const
{
	return kGGDeploySound;
}

float AvHGrenadeGun::GetReloadTime(void) const
{
	int theShotsToLoad = BALANCE_VAR(kGGMaxClip) - this->GetShotsInClip();

	float theBaseReloadTime = BALANCE_VAR(kGrenadeLauncherBaseReloadTime);
	float theGrenadeReloadTime = BALANCE_VAR(kGrenadeLauncherGrenadeReloadTime);
	float theEndReloadTime = BALANCE_VAR(kGrenadeLauncherEndReloadTime);

	return theBaseReloadTime + theShotsToLoad*theGrenadeReloadTime + theEndReloadTime;
}

bool AvHGrenadeGun::GetHasMuzzleFlash() const
{
	return true;
}

void AvHGrenadeGun::GetEventOrigin(Vector& outOrigin) const
{
	Vector theGunPosition = this->m_pPlayer->GetGunPosition();
	VectorCopy(theGunPosition, outOrigin);
}

void AvHGrenadeGun::GetEventAngles(Vector& outAngles) const
{
	float theGrenadeForce = BALANCE_VAR(kGrenadeForce);

	Vector theAiming = this->m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	Vector theVelocity = theAiming*theGrenadeForce + this->m_pPlayer->pev->velocity;

	VectorCopy(theVelocity, outAngles);
}

char* AvHGrenadeGun::GetHeavyViewModel() const
{
	return kGGHVVModel;
}

int	AvHGrenadeGun::GetIdleAnimation() const
{
	int theAnimation = -1;

	int theShotsInClip = this->GetShotsInClip();

	switch(theShotsInClip)
	{
	case 0:
	case 4:
		theAnimation = 0;
		break;

	case 1:
		theAnimation = 3;
		break;

	case 2:
		theAnimation = 2;
		break;

	case 3:
		theAnimation = 1;
		break;
	}

	return theAnimation;
}

char* AvHGrenadeGun::GetPlayerModel() const
{
	return kGGPModel;
}

int	AvHGrenadeGun::GetReloadAnimation() const
{
	int theAnimation = -1;

	int ShotsEmpty = this->GetClipSize() - this->GetShotsInClip();

	int Shotsleft = this->m_pPlayer->m_rgAmmo[this->m_iPrimaryAmmoType];

	int ShotsToReload = min(ShotsEmpty, Shotsleft);
	
	switch(ShotsToReload)
	{
	case 4:
		theAnimation = 7;
		break;
	
	case 3:
		theAnimation = 6;
		break;
	
	case 2:
		theAnimation = 5;
		break;
	
	case 1:
		theAnimation = 4;
		break;
	}

	return theAnimation;
}

int	AvHGrenadeGun::GetEmptyShootAnimation() const
{
	return 12;
}

int	AvHGrenadeGun::GetShootAnimation() const
{
	int theAnimation = -1;
	
	int theShotsInClip = this->GetShotsInClip();
	
	switch(theShotsInClip)
	{
	case 4:
		theAnimation = 8;
		break;
	case 3:
		theAnimation = 9;
		break;
	case 2:
		theAnimation = 10;
		break;
	case 1:
		theAnimation = 11;
		break;
	case 0:
		theAnimation = 12;
		break;
	}
	
	return theAnimation;
}

int	AvHGrenadeGun::GetEndAnimation() const
{
	int theAnimation = -1;

	int theShotsInClip = this->GetShotsInClip();

	switch (theShotsInClip)
	{
	case 1:
		theAnimation = 17;
		break;

	case 2:
	case 3:
		theAnimation = 18;
		break;
	}

	return theAnimation;
}

char* AvHGrenadeGun::GetViewModel() const
{
	return kGGVModel;
}

char* AvHGrenadeGun::GetWorldModel() const
{
	return kGGWModel;
}

void AvHGrenadeGun::FireProjectiles(void)
{
	#ifdef AVH_SERVER

	Vector theOrigin;
	this->GetEventOrigin(theOrigin);

	// Grenade gun uses velocity here instead of angles, assumes angles are the player angles (for both the server grenade and the client temp entity)
	Vector theVelocity;
	this->GetEventAngles(theVelocity);

	// How to handle this?  Only generate entity on server, but we should do SOMETHING on the client, no?
	CGrenade* theGrenade = AvHSUShootServerGrenade(this->m_pPlayer->pev, theOrigin, theVelocity, BALANCE_VAR(kGrenDetonateTime), false);
    ASSERT(theGrenade);
    theGrenade->pev->dmg = this->mDamage;

	#endif
}

void AvHGrenadeGun::Precache()
{
	AvHMarineWeapon::Precache();

	PRECACHE_UNMODIFIED_MODEL(kGGEjectModel);
	PRECACHE_UNMODIFIED_MODEL(kGGAmmoModel);

	PRECACHE_UNMODIFIED_SOUND(kGrenadeBounceSound1);
	PRECACHE_UNMODIFIED_SOUND(kGrenadeBounceSound2);
	PRECACHE_UNMODIFIED_SOUND(kGrenadeBounceSound3);

	PRECACHE_UNMODIFIED_SOUND(kGGFireSound1);
	
	PRECACHE_UNMODIFIED_SOUND(kGGReloadSound);
	
	this->mEvent = PRECACHE_EVENT(1, kGGEventName);
}

void AvHGrenadeGun::DeductCostForShot(void)
{
	AvHMarineWeapon::DeductCostForShot();

	// Stop reload if we were in the middle of one
	if (this->m_fInSpecialReload != kSpecialReloadNone)
	{
		this->m_fInSpecialReload = kSpecialReloadNone;
	}
}

int	AvHGrenadeGun::DefaultReload(int iClipSize, int iAnim, float fDelay)
{
	// Needed to prevet super fast default reload
	return FALSE;
}

void AvHGrenadeGun::Holster(int skiplocal)
{
	AvHMarineWeapon::Holster(skiplocal);

	// Cancel any reload in progress.
	this->m_fInSpecialReload = kSpecialReloadNone;
}

void AvHGrenadeGun::Reload(void)
{
	int theClipSize = this->GetClipSize();

	if ((this->m_pPlayer->m_rgAmmo[this->m_iPrimaryAmmoType] > 0) && (m_iClip < theClipSize))
	{
		if (this->m_fInSpecialReload == kSpecialReloadCloseGG)
		{
			this->SendWeaponAnim(this->GetEndAnimation());
			//Timings here made to match animations as well as reload length of the previous reload version. Numbers are seconds*2 because they get decremented twice in NS.
			this->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + kEndReloadAnimationTime;
			this->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + kEndReloadAnimationTime;
			this->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + kEndReloadAnimationTime;
			//this->m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + kEndReloadAnimationTime;

			this->m_pPlayer->SetAnimation(PLAYER_RELOAD_END);
			this->m_fInSpecialReload = kSpecialReloadNone;
		}
		else if (this->m_fInSpecialReload == kSpecialReloadAddGren)
		{
			// Add to grenade count at specified time in the middle of reload to sync with animation and sound.
			if (m_flTimeWeaponIdle <= UTIL_WeaponTimeBase())
			{
				// Add them to the clip
				this->m_iClip += 1;
				this->m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 1;

				if (this->m_iClip < theClipSize && (this->m_pPlayer->m_rgAmmo[this->m_iPrimaryAmmoType] != 0))
				{
					this->m_fInSpecialReload = kSpecialReloadGotoReload;
				}
				else
				{
					this->m_fInSpecialReload = kSpecialReloadNone;

					this->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + kEndReloadAnimationTime;
					this->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + kEndReloadAnimationTime;
					this->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + kEndReloadAnimationTime;

					this->m_pPlayer->SetAnimation(PLAYER_RELOAD_END);
				}
			}
		}
		// don't reload until recoil is done
		else if (this->m_flNextPrimaryAttack <= UTIL_WeaponTimeBase())
		{
			if (this->m_fInSpecialReload == kSpecialReloadNone)
			{
				// Start reload
				this->m_fInSpecialReload = kSpecialReloadGotoReload;

				this->SendWeaponAnim(this->GetReloadAnimation());

				const float theGotoReloadAnimationTime = 1.1f;

				this->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + theGotoReloadAnimationTime;
				this->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + theGotoReloadAnimationTime;
				this->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + theGotoReloadAnimationTime;

				//this->m_pPlayer->SetAnimation(PLAYER_RELOAD);
				this->m_pPlayer->SetAnimation(PLAYER_RELOAD_START);
			}
			else if (this->m_fInSpecialReload == kSpecialReloadGotoReload)
			{
				if (m_flTimeWeaponIdle <= UTIL_WeaponTimeBase())
				{
					this->m_fInSpecialReload = kSpecialReloadAddGren;

					const float theGrenReloadTime = 2.4f;

					this->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + theGrenReloadTime;
					this->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + theGrenReloadTime;
					this->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + theGrenReloadTime;

					this->m_pPlayer->SetAnimation(PLAYER_RELOAD_INSERT);

				}
			}
		}
	}
}

void AvHGrenadeGun::WeaponIdle(void)
{
	// : 0000484 - ensures that all idle weapons can fire the empty sound
	ResetEmptySound();
	if (this->m_flTimeWeaponIdle < UTIL_WeaponTimeBase())
	{
		if ((this->m_iClip == 0) && (this->m_fInSpecialReload == kSpecialReloadNone) && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			this->Reload();
		}
		else if (this->m_fInSpecialReload != kSpecialReloadNone)
		{
			if ((m_iClip != this->GetClipSize()) && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
			{
				this->Reload();
			}
			else
			{
				// reload debounce has timed out
				this->m_fInSpecialReload = kSpecialReloadNone;
				//ALERT(at_console, "specreset time:%g idle:%g primary:%g specrel:%d\n", gpGlobals->time, this->m_flTimeWeaponIdle, this->m_flNextPrimaryAttack, m_fInSpecialReload);
				this->SendWeaponAnim(this->GetEndAnimation());
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + kEndReloadAnimationTime;
				this->m_pPlayer->SetAnimation(PLAYER_RELOAD_END);
			}
		}
		//else
		//{
			// Hack to prevent idle animation from playing mid-reload.  Not sure how to fix this right, but all this special reloading is happening server-side, client doesn't know about it
			//if (m_iClip == this->GetClipSize())
			//{
			//	AvHMarineWeapon::WeaponIdle();
			//}
		//}
	}
}

void AvHGrenadeGun::Spawn() 
{ 
    AvHMarineWeapon::Spawn(); 

	Precache();

	this->m_iId = AVH_WEAPON_GRENADE_GUN;
	m_iDefaultAmmo = BALANCE_VAR(kGGMaxClip);

    // Set our class name
	this->pev->classname = MAKE_STRING(kwsGrenadeGun);

	SET_MODEL(ENT(this->pev), kGGWModel);

	FallInit();// get ready to fall down.
} 
