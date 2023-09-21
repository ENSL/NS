#ifndef AVH_AI_PLAYER_H
#define AVH_AI_PLAYER_H

#include "../AvHPlayer.h"

typedef struct AVH_AI_PLAYER
{
	AvHPlayer*		Player = nullptr;
	edict_t*		Edict = nullptr;
	AvHTeamNumber	Team = TEAM_IND;
	float			ForwardMove = 0.0f;
	float			SideMove = 0.0f;
	float			UpMove = 0.0f;
	int				Button = 0.0f;
	int				Impulse = 0.0f;
	byte			AdjustedMsec = 0;

	float f_previous_command_time = 0.0f;

} AvHAIPlayer;

string BotNames[MAX_PLAYERS] = {	"MrRobot",
									"Wall-E",
									"BeepBoop",
									"Robotnik",
									"JonnyAutomaton",
									"Burninator",
									"SteelDeath",
									"Meatbag",
									"Undertaker",
									"Botini",
									"Robottle",
									"Rusty",
									"HeavyMetal",
									"Combot",
									"BagelLover",
									"Screwdriver",
									"LoveBug",
									"iSmash",
									"Chippy",
									"Baymax",
									"BoomerBot",
									"Jarvis",
									"Marvin",
									"Data",
									"Scrappy",
									"Mortis",
									"TerrorHertz",
									"Omicron",
									"Herbie",
									"Robogeddon",
									"Velociripper",
									"TerminalFerocity"
								};

#endif