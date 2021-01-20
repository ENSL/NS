//======== (C) Copyright 2002 Charles G. Cleveland All rights reserved. =========
//
// The copyright to the contents herein is the property of Charles G. Cleveland.
// The contents may be used and/or copied only with the written permission of
// Charles G. Cleveland, or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose:
//
// $Workfile: AvHMarineWeapon.cpp $
// $Date: 2002/11/22 21:28:16 $
//
//-------------------------------------------------------------------------------
// $Log: AvHMarineWeapon.cpp,v $
// Revision 1.4  2002/11/22 21:28:16  Flayra
// - mp_consistency changes
//
// Revision 1.3  2002/10/03 18:58:15  Flayra
// - Added heavy view models
//
// Revision 1.2  2002/06/25 17:50:59  Flayra
// - Reworking for correct player animations and new enable/disable state, new view model artwork, alien weapon refactoring
//
// Revision 1.1  2002/05/23 02:33:42  Flayra
// - Post-crash checkin.  Restored @Backup from around 4/16.  Contains changes for last four weeks of development.
//
//===============================================================================
#include "AvHMarineWeapon.h"
#include "AvHMarineWeaponConstants.h"
#include "AvHSpecials.h"
#include "../util/Balance.h"
#include "AvHServerUtil.h"
#include "AvHSharedUtil.h"

bool AvHMarineWeapon::GetAllowedForUser3(AvHUser3 inUser3)
{
	bool theAllowed = false;

	// Alien weapons for aliens.  Don't take into account exact roles until needed (and until weapons have stabilized)
	switch(inUser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		theAllowed = true;
		break;
	}

	return theAllowed;
}

float AvHMarineWeapon::GetDeploySoundVolume() const
{
	return kDeployMarineWeaponVolume;
}

char* AvHMarineWeapon::GetHeavyViewModel() const
{
	return NULL;
}

float AvHMarineWeapon::ComputeAttackInterval() const
{
    float theROF = this->GetRateOfFire();

    int theUser4 = this->m_pPlayer->pev->iuser4;

    // Speed attack if in range of primal scream
    if(GetHasUpgrade(theUser4, MASK_BUFFED))
    {
        float theCatalystROFFactor = 1.0f + BALANCE_VAR(kCatalystROFFactor);
        theROF /= theCatalystROFFactor;
    }

    return theROF;

}

char* AvHMarineWeapon::GetActiveViewModel() const
{
	char* theViewModel = this->GetViewModel();

	// If we're a marine with heavy armor, use the heavy view model
	if(this->m_pPlayer && (this->m_pPlayer->pev->iuser3 == AVH_USER3_MARINE_PLAYER || this->m_pPlayer->pev->iuser3 == AVH_USER3_COMMANDER_PLAYER) && (GetHasUpgrade(this->m_pPlayer->pev->iuser4, MASK_UPGRADE_13)))
	{
		char* theHeavyViewModel = this->GetHeavyViewModel();
		if(theHeavyViewModel)
		{
			theViewModel = theHeavyViewModel;
		}
	}

	return theViewModel;
}

void AvHMarineWeapon::Precache()
{
	AvHBasePlayerWeapon::Precache();

	char* theHeavyViewModel = this->GetHeavyViewModel();
	if(theHeavyViewModel)
	{
		PRECACHE_UNMODIFIED_MODEL(theHeavyViewModel);
	}
}


// AvHReloadableMarineWeapon
const int kSpecialReloadNone = 0;
const int kSpecialReloadGotoReload = 1;
const int kSpecialReloadReloadShell = 2;
const int kSpecialReloadPump = 3;

void AvHReloadableMarineWeapon::DeductCostForShot(void)
{
    AvHMarineWeapon::DeductCostForShot();

    // Stop reload if we were in the middle of one
    if(this->m_fInSpecialReload != kSpecialReloadNone)
    {
        this->m_fInSpecialReload = kSpecialReloadNone;
    }
}

int	AvHReloadableMarineWeapon::DefaultReload( int iClipSize, int iAnim, float fDelay )
{
    // Needed to prevet super fast default reload
    return FALSE;
}

void AvHReloadableMarineWeapon::Holster( int skiplocal)
{
    AvHMarineWeapon::Holster(skiplocal);

    // Cancel any reload in progress.
    this->m_fInSpecialReload = kSpecialReloadNone;
}

void AvHReloadableMarineWeapon::Init()
{
    this->m_fInSpecialReload = kSpecialReloadNone;
    //this->mNextReload = 0;
}

void AvHReloadableMarineWeapon::Reload(void)
{
    //int theReloadAnimation = this->GetReloadAnimation();
    //float theReloadTime = this->GetReloadTime();
    int theClipSize = this->GetClipSize();

    if((this->m_pPlayer->m_rgAmmo[this->m_iPrimaryAmmoType] > 0) && (m_iClip < theClipSize))
    {
		if (this->m_fInSpecialReload == kSpecialReloadPump)
		{
			//pump the shotgun to end the reload if attack is pressed during a reload
			//ALERT(at_console, "reloadpump3\n");
			this->SendWeaponAnim(this->GetEndReloadAnimation());

			//float theEndReloadAnimationTime = this->GetEndReloadAnimationTime();
			//this->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + theEndReloadAnimationTime;
			//this->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + theEndReloadAnimationTime;
			//this->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + theEndReloadAnimationTime;

			//+1 second is the average of the gotoreload and shellreload times previously used to limit primary attack and matches the marine putting his hand on the gun.
			//this->m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0f;
			this->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0f;
			this->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.0f;
			this->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0f;

			this->m_pPlayer->SetAnimation(PLAYER_RELOAD_END);
			this->m_fInSpecialReload = kSpecialReloadNone;
		}
        // don't reload until recoil is done
        else if(this->m_flNextPrimaryAttack <= UTIL_WeaponTimeBase())
        {
            if(this->m_fInSpecialReload == kSpecialReloadNone)
            {
                // Start reload
				this->m_fInSpecialReload = kSpecialReloadGotoReload;
				//ALERT(at_console, "reload0\n");
                this->SendWeaponAnim(this->GetGotoReloadAnimation());

                float theGotoReloadAnimationTime = this->GetGotoReloadAnimationTime();
				
				bool oldreload = (CVAR_GET_FLOAT("sv_nsversion") < 323.0f);
				if (oldreload)
				{ 
					this->m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.1f;
					this->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.1f;
					this->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.1f;
					this->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.1f;
				}
				else
				{
					this->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + theGotoReloadAnimationTime;
					this->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + theGotoReloadAnimationTime;
					this->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + theGotoReloadAnimationTime;
				}

				this->m_pPlayer->SetAnimation(PLAYER_RELOAD_START);
			
            }
            else if(this->m_fInSpecialReload == kSpecialReloadGotoReload)
            {
                if (m_flTimeWeaponIdle <= UTIL_WeaponTimeBase())
                {
                    // was waiting for gun to move to side
					this->m_fInSpecialReload = kSpecialReloadReloadShell;
					//ALERT(at_console, "reload1\n");
                    this->SendWeaponAnim(this->GetShellReloadAnimation());

                    float theShellReloadTime = this->GetShellReloadAnimationTime();

                    //this->mNextReload = UTIL_WeaponTimeBase() + theShellReloadTime;
                    this->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + theShellReloadTime;

					this->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + theShellReloadTime;
					this->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + theShellReloadTime;
					this->m_pPlayer->SetAnimation(PLAYER_RELOAD_INSERT);

                }
            }
            else if(this->m_fInSpecialReload == kSpecialReloadReloadShell)
            {
                //DefaultReload(theClipSize, theReloadAnimation, theReloadTime);
				
                // Don't idle for a bit
                //this->SetNextIdle();

				//ALERT(at_console, "reload2clipadd %g\n", gpGlobals->time);
			
                // Add them to the clip
                this->m_iClip += 1;
                this->m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 1;
				this->m_fInSpecialReload = kSpecialReloadGotoReload;
				this->m_pPlayer->SetAnimation(PLAYER_RELOAD_END);
            }

        }

    }
}


void AvHReloadableMarineWeapon::WeaponIdle(void)
{
	// : 0000484 - ensures that all idle weapons can fire the empty sound
	ResetEmptySound();
	if(this->m_flTimeWeaponIdle < UTIL_WeaponTimeBase())
    {
        if((this->m_iClip == 0) && (this->m_fInSpecialReload == kSpecialReloadNone) && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
        {
            this->Reload();
        }
        else if(this->m_fInSpecialReload != kSpecialReloadNone)
        {
            if((m_iClip != this->GetClipSize()) && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
            {
                this->Reload();
            }
            else
            {
                // reload debounce has timed out
                this->m_fInSpecialReload = kSpecialReloadNone;
				//ALERT(at_console, "specreset time:%g idle:%g primary:%g specrel:%d\n", gpGlobals->time, this->m_flTimeWeaponIdle, this->m_flNextPrimaryAttack, m_fInSpecialReload);
                this->SendWeaponAnim(this->GetEndReloadAnimation());

                float theEndReloadAnimationTime = this->GetEndReloadAnimationTime();

                m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + theEndReloadAnimationTime;
            }
        }
        else
        {
            // Hack to prevent idle animation from playing mid-reload.  Not sure how to fix this right, but all this special reloading is happening server-side, client doesn't know about it
            if(m_iClip == this->GetClipSize())
            {
                AvHMarineWeapon::WeaponIdle();
            }
        }
    }
}
