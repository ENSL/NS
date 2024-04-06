#pragma once

#ifndef AVH_AI_CONFIG_H
#define AVH_AI_CONFIG_H

#include "AvHAIConstants.h"

// Bot fill mode determines how bots should be automatically added/removed from teams
typedef enum _BOTFILLMODE
{
	BOTFILL_MANUAL = 0,  // Manual, no automatic adding or removal of bots
	BOTFILL_BALANCEONLY, // Bots are automatically added/removed to ensure teams remain balanced
	BOTFILL_FILLTEAMS    // Bots are automatically added/removed to ensure teams maintain a certain number of players (see TeamSizeDefinitions)

} BotFillMode;

// Each map can have a desired marine and alien team size
typedef struct _TEAMSIZEDEFINITIONS
{
	int TeamASize = 6;
	int TeamBSize = 6;
} TeamSizeDefinitions;

typedef enum _BOTFILLTIMING
{
	FILLTIMING_MAPLOAD = 0,		// Bots will start filling teams after map load (after grace period)
	FILLTIMING_ALLHUMANS,	// Bots will only start filling teams once all humans in the ready room have joined a team
	FILLTIMING_ROUNDSTART	// Bots will only start filling teams after round start
} BotFillTiming;

// Reads evobot.cfg in addons/evobot and populates all the settings from it
void CONFIG_ParseConfigFile();

string CONFIG_GetBotPrefix();

// Returns the current commander wait time is COMMANDERMODE_ALWAYS (see CONFIG_GetCommanderMode())
float CONFIG_GetCommanderWaitTime();

// Returns the current lerk cooldown (how long aliens wait before evolving another lerk after the last one died)
float CONFIG_GetLerkCooldown();

bool CONFIG_IsLerkAllowed();
bool CONFIG_IsFadeAllowed();
bool CONFIG_IsOnosAllowed();

// Returns the max time a bot is allowed to be stuck before suiciding (0 means forever)
float CONFIG_GetMaxStuckTime();

float CONFIG_GetMaxAIMatchTimeMinutes();

// Returns the desired marine team size for the given map, indexes into TeamSizeMap
int CONFIG_GetTeamASizeForMap(const char* MapName);
// Returns the desired alien team size for the given map, indexes into TeamSizeMap
int CONFIG_GetTeamBSizeForMap(const char* MapName);

// Returns the configured hive tech at that index (chamber build sequence)
AvHMessageID CONFIG_GetHiveTechAtIndex(const int Index);

bot_skill CONFIG_GetBotSkillLevel();

BotFillTiming CONFIG_GetBotFillTiming();

void CONFIG_RegenerateIniFile();

void CONFIG_PopulateBotNames();

string CONFIG_GetNextBotName();

#endif