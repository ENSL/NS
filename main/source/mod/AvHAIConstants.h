#pragma once

#ifndef AVH_AI_CONSTANTS_H
#define AVH_AI_CONSTANTS_H

#include "DetourStatus.h"
#include "DetourNavMeshQuery.h"

#include "AvHHive.h"
#include "AvHEntities.h"
#include "AvHAIMath.h"

static const float commander_action_cooldown = 1.0f;
static const float min_request_spam_time = 10.0f;

constexpr auto MAX_AI_PATH_SIZE = 512; // Maximum number of points allowed in a path (this should be enough for any sized map)
static const int MAX_NAV_MESHES = 8; // Max number of nav meshes allowed. Currently 3 are used (one for building placement, one for the onos, and a regular one for everyone else)

// NS weapon types. Each number refers to the GoldSrc weapon index
typedef enum
{
	WEAPON_INVALID = 0,
	WEAPON_LERK_SPIKE = 4, // I think this is an early NS weapon, replaced by primal scream

	// Marine Weapons

	WEAPON_MARINE_KNIFE = 13,
	WEAPON_MARINE_PISTOL = 14,
	WEAPON_MARINE_MG = 15,
	WEAPON_MARINE_SHOTGUN = 16,
	WEAPON_MARINE_HMG = 17,
	WEAPON_MARINE_WELDER = 18,
	WEAPON_MARINE_MINES = 19,
	WEAPON_MARINE_GL = 20,
	WEAPON_MARINE_GRENADE = 28,

	// Alien Abilities

	WEAPON_SKULK_BITE = 5,
	WEAPON_SKULK_PARASITE = 10,
	WEAPON_SKULK_LEAP = 21,
	WEAPON_SKULK_XENOCIDE = 12,

	WEAPON_GORGE_SPIT = 2,
	WEAPON_GORGE_HEALINGSPRAY = 27,
	WEAPON_GORGE_BILEBOMB = 25,
	WEAPON_GORGE_WEB = 8,

	WEAPON_LERK_BITE = 6,
	WEAPON_LERK_SPORES = 3,
	WEAPON_LERK_UMBRA = 23,
	WEAPON_LERK_PRIMALSCREAM = 24,

	WEAPON_FADE_SWIPE = 7,
	WEAPON_FADE_BLINK = 11,
	WEAPON_FADE_METABOLIZE = 9,
	WEAPON_FADE_ACIDROCKET = 26,

	WEAPON_ONOS_GORE = 1,
	WEAPON_ONOS_DEVOUR = 30,
	WEAPON_ONOS_STOMP = 29,
	WEAPON_ONOS_CHARGE = 22,

	WEAPON_MAX = 31
}
AvHAIWeapon;

// Hives can either be unbuilt ("ghost" hive), in progress or fully built (active)
typedef enum
{
	HIVE_STATUS_UNBUILT = 0,
	HIVE_STATUS_BUILDING = 1,
	HIVE_STATUS_BUILT = 2
} HiveStatusType;

// All tech statuses that can be assigned to a hive
typedef enum
{
	HIVE_TECH_NONE = 0, // Hive doesn't have any tech assigned to it yet (no chambers built for it)
	HIVE_TECH_DEFENCE = 1,
	HIVE_TECH_SENSORY = 2,
	HIVE_TECH_MOVEMENT = 3
} HiveTechStatus;

typedef enum _AI_REACHABILITY_STATUS
{
	AI_REACHABILITY_NONE = 0,
	AI_REACHABILITY_MARINE = 1u << 0,
	AI_REACHABILITY_SKULK = 1u << 1,
	AI_REACHABILITY_GORGE = 1u << 2,
	AI_REACHABILITY_ONOS = 1u << 3,
	AI_REACHABILITY_WELDER = 1u << 4,
	AI_REACHABILITY_UNREACHABLE = 1u << 5,

	AI_REACHABILITY_ALL = -1
} AvHAIReachabilityStatus;

typedef enum
{
	STRUCTURE_STATUS_NONE = 0,					// No filters, all buildings will be returned
	STRUCTURE_STATUS_COMPLETED = 1,				// Structure is fully built
	STRUCTURE_STATUS_ELECTRIFIED = 1 << 1,
	STRUCTURE_STATUS_RECYCLING = 1 << 2,
	STRUCTURE_STATUS_PARASITED = 1 << 3,
	STRUCTURE_STATUS_UNDERATTACK = 1 << 4,
	STRUCTURE_STATUS_RESEARCHING = 1 << 5,
	STRUCTURE_STATUS_DAMAGED = 1 << 6,
	STRUCTURE_STATUS_DISABLED = 1 << 7,		// For marine turrets when there's no TF

	STRUCTURE_STATUS_ALL = -1
} AvHAIStructureStatus;

typedef enum
{
	STRUCTURE_NONE = 0,
	STRUCTURE_MARINE_RESTOWER = 1u,
	STRUCTURE_MARINE_INFANTRYPORTAL = 1u << 1,
	STRUCTURE_MARINE_TURRETFACTORY = 1u << 2,
	STRUCTURE_MARINE_ADVTURRETFACTORY = 1u << 3,
	STRUCTURE_MARINE_ARMOURY = 1u << 4,
	STRUCTURE_MARINE_ADVARMOURY = 1u << 5,
	STRUCTURE_MARINE_ARMSLAB = 1u << 6,
	STRUCTURE_MARINE_PROTOTYPELAB = 1u << 7,
	STRUCTURE_MARINE_OBSERVATORY = 1u << 8,
	STRUCTURE_MARINE_PHASEGATE = 1u << 9,
	STRUCTURE_MARINE_TURRET = 1u << 10,
	STRUCTURE_MARINE_SIEGETURRET = 1u << 11,
	STRUCTURE_MARINE_COMMCHAIR = 1u << 12,
	STRUCTURE_MARINE_DEPLOYEDMINE = 1u << 13,

	STRUCTURE_ALIEN_HIVE = 1u << 14,
	STRUCTURE_ALIEN_RESTOWER = 1u << 15,
	STRUCTURE_ALIEN_DEFENCECHAMBER = 1u << 16,
	STRUCTURE_ALIEN_SENSORYCHAMBER = 1u << 17,
	STRUCTURE_ALIEN_MOVEMENTCHAMBER = 1u << 18,
	STRUCTURE_ALIEN_OFFENCECHAMBER = 1u << 19,

	SEARCH_ALL_MARINE_STRUCTURES = 0xFFF,
	SEARCH_ALL_ALIEN_STRUCTURES = 0xFC000,
	SEARCH_ANY_RES_TOWER = (STRUCTURE_MARINE_RESTOWER | STRUCTURE_ALIEN_RESTOWER),

	SEARCH_ALL_STRUCTURES = ((unsigned int)-1 & ~(STRUCTURE_MARINE_DEPLOYEDMINE))

} AvHAIDeployableStructureType;

typedef enum
{
	DEPLOYABLE_ITEM_NONE = 0,
	DEPLOYABLE_ITEM_RESUPPLY = 1u, // For combat mode
	DEPLOYABLE_ITEM_HEAVYARMOUR = 1u << 1,
	DEPLOYABLE_ITEM_JETPACK = 1u << 2,
	DEPLOYABLE_ITEM_CATALYSTS = 1u << 3,
	DEPLOYABLE_ITEM_SCAN = 1u << 4,
	DEPLOYABLE_ITEM_HEALTHPACK = 1u << 5,
	DEPLOYABLE_ITEM_AMMO = 1u << 6,
	DEPLOYABLE_ITEM_MINES = 1u << 7,
	DEPLOYABLE_ITEM_WELDER = 1u << 8,
	DEPLOYABLE_ITEM_SHOTGUN = 1u << 9,
	DEPLOYABLE_ITEM_HMG = 1u << 10,
	DEPLOYABLE_ITEM_GRENADELAUNCHER = 1u << 11,

	DEPLOYABLE_ITEM_WEAPONS = 0xF80,
	DEPLOYABLE_ITEM_EQUIPMENT = 0x6,

	DEPLOYABLE_ITEM_ALL = -1
} AvHAIDeployableItemType;

// Type of goal the commander wants to achieve
typedef enum _STRUCTUREPURPOSE
{
	STRUCTURE_PURPOSE_NONE = 0,
	STRUCTURE_PURPOSE_GENERAL = 1u,
	STRUCTURE_PURPOSE_SIEGE = 1u << 1,
	STRUCTURE_PURPOSE_FORTIFY = 1u << 2,
	STRUCTURE_PURPOSE_BASE = 1u << 3,
	STRUCTURE_PURPOSE_ANY = -1

} StructurePurpose;

typedef enum _AVHAICOMMANDERMODE
{
	COMMANDERMODE_DISABLED,		// AI Commander not allowed
	COMMANDERMODE_IFNOHUMAN,	// AI Commander only allowed if no humans are on the marine team
	COMMANDERMODE_ENABLED		// AI Commander allowed if no human takes charge (following grace period)
} AvHAICommanderMode;

// Bot's role on the team. For marines, this only governs what they do when left to their own devices.
// Marine bots will always listen to orders from the commander regardless of role.
typedef enum _AVHAIBOTROLE
{
	BOT_ROLE_NONE,			 // No defined role

	// General Roles

	BOT_ROLE_FIND_RESOURCES, // Will hunt for uncapped resource nodes and cap them. Will attack enemy resource towers
	BOT_ROLE_SWEEPER,		 // Defensive role to protect infrastructure and build at base. Will patrol to keep outposts secure
	BOT_ROLE_ASSAULT,		 // Will go to attack the enemy base. In combat mode, used for Fade-focus aliens

	// Marine-only Roles

	BOT_ROLE_COMMAND,		 // Will attempt to take command
	BOT_ROLE_BOMBARDIER,	 // Bot is armed with a GL and wants to wreck your shit. In combat mode, used for Onos-focus aliens

	// Alien-only roles

	BOT_ROLE_BUILDER,		 // Will focus on building chambers and hives. Stays gorge most of the time
	BOT_ROLE_HARASS		 // Focuses on taking down enemy resource nodes and hunting the enemy
} AvHAIBotRole;

typedef enum _AVHAICOMBATSTRATEGY
{
	COMBAT_STRATEGY_IGNORE = 0, // Don't engage this enemy
	COMBAT_STRATEGY_AMBUSH,		// Set up an ambush for this enemy
	COMBAT_STRATEGY_RETREAT,	// Retreat and find health
	COMBAT_STRATEGY_SKIRMISH,	// Maintain distance, whittle down their health from range and generally be a pain the arse
	COMBAT_STRATEGY_ATTACK		// Attack the enemy
} AvHAICombatStrategy;

typedef enum _AVHAINAVMESHSTATUS
{
	NAVMESH_STATUS_PENDING = 0,	// Waiting to try loading the navmesh
	NAVMESH_STATUS_FAILED,		// Failed to load the navmesh
	NAVMESH_STATUS_SUCCESS		// Successfully loaded the navmesh
} AvHAINavMeshStatus;

typedef struct _OFF_MESH_CONN
{
	unsigned int ConnectionRefs[2];
	unsigned int ConnectionFlags = 0;
	unsigned int DefaultConnectionFlags = 0;
	Vector FromLocation = g_vecZero;
	Vector ToLocation = g_vecZero;
	edict_t* TargetObject = nullptr;
	bool bBiDirectional = false;
} AvHAIOffMeshConnection;

typedef struct _STRUCTURE_OBSTACLE
{
	unsigned int NavMeshIndex = 0;
	unsigned int ObstacleRef = 0;
} AvHAITempObstacle;

// Data structure used to track resource nodes in the map
typedef struct _RESOURCE_NODE
{
	AvHFuncResource* ResourceEntity = nullptr;						// The func_resource edict reference
	edict_t* ResourceEdict = nullptr;
	Vector Location = g_vecZero;									// origin of the func_resource edict (not the tower itself)
	bool bIsOccupied = false;										// True if there is any resource tower on it
	AvHTeamNumber OwningTeam = TEAM_IND;							// The team that has currently capped this node (TEAM_IND if none)
	edict_t* ActiveTowerEntity = nullptr;							// Reference to the resource tower edict (if capped)
	bool bIsBaseNode = false;										// Is this a node in the marine base or active alien hive?
	edict_t* ParentHive = nullptr;
	unsigned int TeamAReachabilityFlags = AI_REACHABILITY_NONE;		// Who on team A can reach this node?
	unsigned int TeamBReachabilityFlags = AI_REACHABILITY_NONE;		// Who on team B can reach this node?
	bool bReachabilityMarkedDirty = false;							// Reachability needs to be recalculated
	float NextReachabilityRefreshTime = 0.0f;
} AvHAIResourceNode;

// Data structure to hold information about each hive in the map
typedef struct _HIVE_DEFINITION_T
{
	AvHHive* HiveEntity = nullptr;					// Hive entity reference
	edict_t* HiveEdict = nullptr;					// Hive edict reference
	Vector Location = g_vecZero;					// Origin of the hive
	Vector FloorLocation = g_vecZero;				// Some hives are suspended in the air, this is the floor location directly beneath it
	HiveStatusType Status = HIVE_STATUS_UNBUILT;	// Can be unbuilt, in progress, or fully built
	AvHMessageID TechStatus = MESSAGE_NULL;			// What tech (if any) is assigned to this hive right now
	bool bIsUnderAttack = false;					// Is the hive currently under attack? Becomes false if not taken damage for more than 10 seconds
	float HealthPercent = 0.0f;						// If the hive is built and active, what its health currently is
	AvHAIResourceNode* HiveResNodeRef = nullptr;	// Which resource node (indexes into ResourceNodes array) belongs to this hive?
	unsigned int ObstacleRefs[MAX_NAV_MESHES];		// When in progress or built, will place an obstacle so bots don't try to walk through it
	float NextFloorLocationCheck = 0.0f;			// When should the closest navigable point to the hive be calculated? Used to delay the check after a hive is built
	AvHTeamNumber OwningTeam = TEAM_IND;			// Which team owns this hive currently (TEAM_IND if empty)
	unsigned int TeamAReachabilityFlags = AI_REACHABILITY_NONE;		// Who on team A can reach this node?
	unsigned int TeamBReachabilityFlags = AI_REACHABILITY_NONE;		// Who on team B can reach this node?
	char HiveName[64];
} AvHAIHiveDefinition;

// A nav profile combines a nav mesh reference (indexed into NavMeshes) and filters to determine how a bot should find paths
typedef struct _NAV_PROFILE
{
	int NavMeshIndex = -1;
	dtQueryFilter Filters;
	bool bFlyingProfile = false;
	AvHAIReachabilityStatus ReachabilityFlag = AI_REACHABILITY_NONE;
} nav_profile;

typedef struct _LOAD_NAV_HINT
{
	unsigned int id = 0;
	unsigned int hintType = 0;
	float position[3] = { 0.0f, 0.0f, 0.0f };
} LoadNavHint;

typedef struct _NAV_HINT
{
	unsigned int hintType = 0;
	Vector Position = g_vecZero;
	edict_t* OccupyingBuilding = nullptr;
} NavHint;

typedef struct _DEPLOYABLE_SEARCH_FILTER
{
	unsigned int DeployableTypes = SEARCH_ALL_STRUCTURES;
	unsigned int IncludeStatusFlags = STRUCTURE_STATUS_NONE;
	unsigned int ExcludeStatusFlags = STRUCTURE_STATUS_NONE;
	unsigned int ReachabilityFlags = AI_REACHABILITY_NONE;
	float MinSearchRadius = 0.0f;
	float MaxSearchRadius = 0.0f;
	bool bConsiderPhaseDistance = false;
	AvHTeamNumber DeployableTeam = TEAM_IND;
	AvHTeamNumber ReachabilityTeam = TEAM_IND;
	unsigned int PurposeFlags = STRUCTURE_PURPOSE_ANY;
} DeployableSearchFilter;

// Pending message a bot wants to say. Allows for a delay in sending a message to simulate typing, or prevent too many messages on the same frame
typedef struct _BOT_MSG
{
	char msg[64]; // Message to send
	float SendTime = 0.0f; // When the bot should send this message
	bool bIsPending = false; // Represents a valid pending message
	bool bIsTeamSay = false; // Is this a team-only message?
} bot_msg;

typedef struct _AVH_AI_GUARD_INFO
{
	Vector GuardLocation = g_vecZero; // What position are we guarding?
	Vector GuardStandPosition = g_vecZero; // Where the bot should stand to guard position (moves around a bit)
	std::vector<Vector> GuardPoints; // All potential areas to watch that an enemy could approach from
	int NumGuardPoints = 0; // How many watch areas there are for the current location
	Vector GuardLookLocation = g_vecZero; // Which area are we currently watching?
	float GuardStartLookTime = 0.0f; // When did we start watching the current area?
	float ThisGuardLookTime = 0.0f; // How long should we watch this area for?
	float ThisGuardStandTime = 0.0f; // How long should we watch this area for?
	float GuardStartStandTime = 0.0f; // How long should we watch this area for?

} AvHAIGuardInfo;

// Data structure to hold information on any kind of buildable structure (hive, resource tower, chamber, marine building etc)
typedef struct _AVH_AI_BUILDABLE_STRUCTURE
{
	AvHBaseBuildable* EntityRef = nullptr;
	edict_t* edict = nullptr; // Reference to structure edict
	Vector Location = g_vecZero; // origin of the structure edict
	float healthPercent = 0.0f; // Current health of the building
	float lastDamagedTime = 0.0f; // When it was last damaged by something. Used by bots to determine if still needs defending
	AvHAIDeployableStructureType StructureType = STRUCTURE_NONE; // Type of structure it is (e.g. hive, comm chair, infantry portal, defence chamber etc.)
	unsigned int StructureStatusFlags = STRUCTURE_STATUS_NONE;
	unsigned int TeamAReachabilityFlags = AI_REACHABILITY_NONE;
	unsigned int TeamBReachabilityFlags = AI_REACHABILITY_NONE;
	int LastSeen = 0; // Which refresh cycle was this last seen on? Used to determine if the building has been removed from play
	vector< AvHAITempObstacle> Obstacles;
	vector<AvHAIOffMeshConnection> OffMeshConnections; // References to any off-mesh connections this structure is associated with
	Vector LastSuccessfulCommanderLocation = g_vecZero; // Tracks the last commander view location where it successfully placed or selected the building
	Vector LastSuccessfulCommanderAngle = g_vecZero; // Tracks the last commander input angle ("click" location) used to successfully place or select building
	StructurePurpose Purpose = STRUCTURE_PURPOSE_NONE;
	bool bReachabilityMarkedDirty = false; // If true, reachability flags will be recalculated for this structure

	bool IsValid() { return !FNullEnt(edict) && !edict->free && !(edict->v.flags & EF_NODRAW) && edict->v.deadflag == DEAD_NO; }
	bool IsCompleted() { return (StructureStatusFlags & STRUCTURE_STATUS_COMPLETED); }

} AvHAIBuildableStructure;

// Any kind of pickup that has been dropped either by the commander or by a player
typedef struct _DROPPED_MARINE_ITEM
{
	edict_t* edict = nullptr; // Reference to the item edict
	Vector Location = g_vecZero; // Origin of the entity
	AvHAIDeployableItemType ItemType = DEPLOYABLE_ITEM_NONE; // Is it a weapon, health pack, ammo pack etc?
	unsigned int TeamAReachabilityFlags = AI_REACHABILITY_NONE;
	unsigned int TeamBReachabilityFlags = AI_REACHABILITY_NONE;
	bool bReachabilityMarkedDirty = false; // Reachability needs to be recalculated
	int LastSeen = 0; // Which refresh cycle was this last seen on? Used to determine if the item has been removed from play
} AvHAIDroppedItem;

// How far a bot can be from a useable object when trying to interact with it. Used also for melee attacks. We make it slightly less than actual to avoid edge cases
static const float max_ai_use_reach = 55.0f;

// Minimum time a bot can wait between attempts to use something in seconds (when not holding the use key down)
static const float min_ai_use_interval = 0.5f;

// Minimum time a bot can wait between attempts to use something in seconds (when not holding the use key down)
static const float max_ai_jump_height = 62.0f;

// Affects the bot's pathfinding choices
enum BotMoveStyle
{
	MOVESTYLE_NORMAL, // Most direct route to target
	MOVESTYLE_AMBUSH, // Prefer wall climbing and vents
	MOVESTYLE_HIDE // Prefer crouched areas like vents
};

// The list of potential task types for the bot_task structure
typedef enum
{
	TASK_NONE,
	TASK_GET_HEALTH,
	TASK_GET_AMMO,
	TASK_GET_WEAPON,
	TASK_GET_EQUIPMENT,
	TASK_BUILD,
	TASK_ATTACK,
	TASK_MOVE,
	TASK_CAP_RESNODE,
	TASK_DEFEND,
	TASK_GUARD,
	TASK_HEAL,
	TASK_WELD,
	TASK_RESUPPLY,
	TASK_EVOLVE,
	TASK_COMMAND,
	TASK_USE,
	TASK_TOUCH,
	TASK_REINFORCE_STRUCTURE,
	TASK_SECURE_HIVE,
	TASK_PLACE_MINE,
	TASK_ATTACK_BASE
}
BotTaskType;

// 
typedef enum
{
	ATTACK_SUCCESS,
	ATTACK_BLOCKED,
	ATTACK_OUTOFRANGE,
	ATTACK_INVALIDTARGET,
	ATTACK_NOWEAPON
}
BotAttackResult;

typedef enum
{
	BUILD_ATTEMPT_NONE = 0,
	BUILD_ATTEMPT_PENDING,
	BUILD_ATTEMPT_SUCCESS,
	BUILD_ATTEMPT_FAILED
} BotBuildAttemptStatus;

typedef enum
{
	MOVE_TASK_NONE = 0,
	MOVE_TASK_MOVE,
	MOVE_TASK_USE,
	MOVE_TASK_BREAK,
	MOVE_TASK_TOUCH,
	MOVE_TASK_PICKUP,
	MOVE_TASK_WELD
} BotMovementTaskType;

// Door type. Not currently used, future feature so bots know how to open a door
enum DoorActivationType
{
	DOOR_NONE,   // No type, cannot be activated (permanently open/shut)
	DOOR_USE,    // Door activated by using it directly
	DOOR_TRIGGER,// Door activated by touching a trigger_once or trigger_multiple
	DOOR_BUTTON, // Door activated by pressing a button
	DOOR_WELD,   // Door activated by welding something
	DOOR_SHOOT,  // Door activated by being shot
	DOOR_BREAK	 // Door activated by breaking something
};

// Door type. Not currently used, future feature so bots know how to open a door
enum NavDoorType
{
	DOORTYPE_DOOR,   // No type, cannot be activated (permanently open/shut)
	DOORTYPE_PLAT,    // Door activated by using it directly
	DOORTYPE_TRAIN	// Door activated by touching a trigger_once or trigger_multiple
};

// Bot path node. A path will be several of these strung together to lead the bot to its destination
typedef struct _BOT_PATH_NODE
{
	Vector FromLocation = g_vecZero; // Location to move from
	Vector Location = g_vecZero; // Location to move to
	float requiredZ = 0.0f; // If climbing a up ladder or wall, how high should they aim to get before dismounting.
	unsigned int flag = 0; // Is this a ladder movement, wall climb, walk etc
	unsigned char area = 0; // Is this a crouch area, normal walking area etc
	unsigned int poly = 0; // The nav mesh poly this point resides on
} bot_path_node;

// Represents a bot's current understanding of an enemy player's status
typedef struct _ENEMY_STATUS
{
	AvHPlayer* PlayerRef = nullptr; // Reference to the enemy AvHPlayer
	edict_t* PlayerEdict = nullptr; // Reference to the enemy player edict

	Vector LastDetectedLocation = g_vecZero; // Where the bot last detected the enemy, either through sight, motion tracking or sound
	Vector LastVisibleLocation = g_vecZero;  // Last point the bot had visible confirmation of the enemy
	Vector LastKnownVelocity = g_vecZero;
	Vector VisiblePointOnPlayer = g_vecZero;
	float AwarenessOfPlayer = 0.0f;			 // How aware of this enemy the bot is
	float LastDetectedTime = 0.0f;			 // When the bot last saw the enemy or they pinged on motion tracking
	float InitialAwarenessTime = 0.0f;			 // When the bot first became aware of the enemy
	float LastVisibleTime = 0.0f;			// Last time the bot actually saw the enemy
	float EnemyThreatLevel = 0.0f;			// Generally, >=3.0 means actively fighting them, >=2.0 means visible and close, >=1.0 means not visible but close and <1.0 means they can be heard but not close

	bool bHasLOS = false;					 // Does the bot has LOS of the enemy?
	bool bEnemyHasLOS = false;

	Vector LastLOSPosition = g_vecZero;
	Vector LastCoverPosition = g_vecZero;

} enemy_status;

// Tracks what orders have been given to which players
typedef struct _BOT_SKILL
{
	float marine_bot_reaction_time = 0.2f; // How quickly the bot will react to seeing an enemy
	float marine_bot_aim_skill = 0.5f; // How quickly the bot can lock on to an enemy
	float marine_bot_motion_tracking_skill = 0.5f; // How well the bot can follow an enemy target's motion
	float marine_bot_view_speed = 1.0f; // How fast a bot can spin its view to aim in a given direction
	float alien_bot_reaction_time = 0.2f; // How quickly the bot will react to seeing an enemy
	float alien_bot_aim_skill = 0.5f; // How quickly the bot can lock on to an enemy
	float alien_bot_motion_tracking_skill = 0.5f; // How well the bot can follow an enemy target's motion
	float alien_bot_view_speed = 0.5f; // How fast a bot can spin its view to aim in a given direction

} bot_skill;

typedef struct _AVH_AI_BUILD_ATTEMPT
{
	AvHAIDeployableStructureType AttemptedStructureType = STRUCTURE_NONE;
	Vector AttemptedLocation = g_vecZero;
	int NumAttempts = 0;
	BotBuildAttemptStatus BuildStatus = BUILD_ATTEMPT_NONE;
	float BuildAttemptTime = 0.0f;
	AvHAIBuildableStructure* LinkedStructure = nullptr;
} AvHAIBuildAttempt;

// A bot task is a goal the bot wants to perform, such as attacking a structure, placing a structure etc. NOT USED BY COMMANDER
typedef struct _AVH_AI_PLAYER_TASK
{
	BotTaskType TaskType = TASK_NONE; // Task Type (e.g. build, attack, defend, heal etc)
	Vector TaskLocation = g_vecZero; // Task location, if task needs one (e.g. where to place structure for TASK_BUILD)
	edict_t* TaskTarget = nullptr; // Reference to a target, if task needs one (e.g. TASK_ATTACK)
	edict_t* TaskSecondaryTarget = nullptr; // Secondary target, if task needs one (e.g. TASK_REINFORCE)
	AvHAIDeployableStructureType StructureType = STRUCTURE_NONE; // For Gorges, what structure to build (if TASK_BUILD)
	float TaskStartedTime = 0.0f; // When the bot started this task. Helps time-out if the bot gets stuck trying to complete it
	bool bIssuedByCommander = false; // Was this task issued by the commander? Top priority if so
	bool bTargetIsPlayer = false; // Is the TaskTarget a player?
	bool bTaskIsUrgent = false; // Determines whether this task is prioritised over others if bot has multiple
	bool bIsWaitingForBuildLink = false; // If true, Gorge has sent the build impulse and is waiting to see if the building materialised
	float LastBuildAttemptTime = 0.0f; // When did the Gorge last try to place a structure?
	int BuildAttempts = 0; // How many attempts the Gorge has tried to place it, so it doesn't keep trying forever
	AvHMessageID Evolution = MESSAGE_NULL; // Used by TASK_EVOLVE to determine what to evolve into
	float TaskLength = 0.0f; // If a task has gone on longer than this time, it will be considered completed
	AvHAIBuildAttempt ActiveBuildInfo; // If gorge, the current status of any recent attempt to place a structure
} AvHAIPlayerTask;

typedef struct _DOOR_TRIGGER
{
	CBaseEntity* Entity = nullptr;
	CBaseToggle* ToggleEnt = nullptr;
	edict_t* Edict = nullptr;
	DoorActivationType TriggerType = DOOR_NONE;
	bool bIsActivated = false;
	CBaseEntity* TriggerChangeTargetRef = nullptr;
	float ActivationDelay = 0.0f;
	float LastActivatedTime = 0.0f;
	TOGGLE_STATE LastToggleState = TS_AT_BOTTOM;
	float LastNextThink = 0.0f;
	float NextActivationTime = 0.0f;
} DoorTrigger;

typedef struct _AVH_AI_PLAYER_MOVE_TASK
{
	BotMovementTaskType TaskType = MOVE_TASK_NONE;
	Vector TaskLocation = g_vecZero;
	edict_t* TaskTarget = nullptr;
	DoorTrigger* TriggerToActivate = nullptr;
	bool bPathGenerated = false;
} AvHAIPlayerMoveTask;

typedef struct _AVH_AI_STUCK_TRACKER
{
	Vector LastBotPosition = g_vecZero;
	Vector MoveDestination = g_vecZero;
	float TotalStuckTime = 0.0f; // Total time the bot has spent stuck
	bool bPathFollowFailed = false;

} AvHAIPlayerStuckTracker;

// Contains the bot's current navigation info, such as current path
typedef struct _NAV_STATUS
{
	vector<bot_path_node> CurrentPath; // Bot's path nodes
	unsigned int CurrentPathPoint = 0;

	Vector TargetDestination = g_vecZero; // Desired destination
	Vector ActualMoveDestination = g_vecZero; // Actual destination on nav mesh
	Vector PathDestination = g_vecZero; // Where the path is currently headed to

	Vector LastNavMeshCheckPosition = g_vecZero;
	Vector LastNavMeshPosition = g_vecZero; // Tracks the last place the bot was on the nav mesh. Useful if accidentally straying off it
	Vector LastOpenLocation = g_vecZero; // Tracks the last place the bot had enough room to move around people. Useful if in a vent and need to back up somewhere to let another player past.

	int CurrentMoveType = MOVETYPE_NONE; // Tracks the edict's current movement type

	unsigned int CurrentPoly = 0; // Which nav mesh poly the bot is currently on

	float LastStuckCheckTime = 0.0f; // Last time the bot checked if it had successfully moved
	float TotalStuckTime = 0.0f; // Total time the bot has spent stuck
	float LastDistanceFromDestination = 0.0f; // How far from its destination was it last stuck check

	Vector StuckCheckMoveLocation = g_vecZero; // Where is the bot trying to go that we're checking if they're stuck?
	Vector UnstuckMoveLocation = g_vecZero; // If the bot is unable to find a path, blindly move here to try and fix the problem

	float LandedTime = 0.0f; // When the bot last landed after a fall/jump.
	float LeapAttemptedTime = 0.0f; // When the bot last attempted to leap/blink. Avoid spam that sends it flying around too fast
	bool bIsJumping = false; // Is the bot in the air from a jump? Will duck so it can duck-jump
	bool IsOnGround = true; // Is the bot currently on the ground, or on a ladder?
	bool bHasAttemptedJump = false; // Last frame, the bot tried a jump. If the bot is still on the ground, it probably tried to jump in a vent or something
	float LastFlapTime = 0.0f; // When the bot last flapped its wings (if Lerk). Prevents per-frame spam draining adrenaline

	bool bShouldWalk = false; // Should the bot walk at this point?

	BotMoveStyle PreviousMoveStyle = MOVESTYLE_NORMAL; // Previous desired move style (e.g. normal, ambush, hide). Will trigger new path calculations if this changes
	BotMoveStyle MoveStyle = MOVESTYLE_NORMAL; // Current desired move style (e.g. normal, ambush, hide). Will trigger new path calculations if this changes
	float LastPathCalcTime = 0.0f; // When the bot last calculated a path, to limit how frequently it can recalculate

	float NextForceRecalc = 0.0f; // If set, then the bot will force-recalc its current path

	bool bZig; // Is the bot zigging, or zagging?
	float NextZigTime; // Controls how frequently they zig or zag

	nav_profile NavProfile;
	bool bNavProfileChanged = false;

	AvHAIPlayerStuckTracker StuckInfo;

	unsigned int SpecialMovementFlags = 0; // Any special movement flags required for this path (e.g. needs a welder, needs a jetpack etc.)

	AvHAIPlayerMoveTask MovementTask;
} nav_status;

// Type of goal the commander wants to achieve
typedef enum _COMMANDERACTIONTYPE
{
	ACTION_NONE = 0,
	ACTION_UPGRADE,
	ACTION_RESEARCH,
	ACTION_RECYCLE,
	ACTION_GIVEORDER,
	ACTION_DEPLOY // Deploy a structure or item into the map

} CommanderActionType;

// Some commander actions are multi-step (e.g. click to select building, release to complete selection, input recycle command etc). Tracks where the commander is in the process
typedef enum _COMMANDERACTIONSTEP
{
	ACTION_STEP_NONE = 0,
	ACTION_STEP_BEGIN_SELECT, // Click mouse button down to start select
	ACTION_STEP_END_SELECT, // Release mouse button to complete select

} CommanderActionStep;


// Used by the AI commander instead of bot_task. Has data specifically to handle commander-specific stuff
typedef struct _COMMANDER_ACTION
{
	bool bIsActive = false;
	CommanderActionType ActionType = ACTION_NONE; // What action to perform (e.g. build, recycle, drop item etc)
	CommanderActionStep ActionStep = ACTION_STEP_NONE; // Used for multi-stage processes such as selecting a building, issuing recycle command etc.
	AvHAIDeployableStructureType StructureToBuild = STRUCTURE_NONE; // What structure to build if build action
	AvHAIDeployableItemType ItemToPlace = DEPLOYABLE_ITEM_NONE;
	int NumInstances = 0;
	int NumDesiredInstances = 0;
	StructurePurpose ActionPurpose = STRUCTURE_PURPOSE_NONE;
	Vector BuildLocation = g_vecZero; // Where to build the structure
	Vector DesiredCommanderLocation = g_vecZero; // To perform this action, where does the commander's view need to be? For building, usually directly above location, but could be off to side if obstructed by geometry
	Vector LastAttemptedCommanderLocation = g_vecZero; // The position of the commander's view at the last action attempt
	Vector LastAttemptedCommanderAngle = g_vecZero; // The click angle of the last action attempt
	int AssignedPlayer = 0; // Which player index is assigned to perform the action (e.g. build structure)? Will send orders to that player (move here, build this structure etc.)
	edict_t* StructureOrItem = nullptr; // Reference the structure edict. If a structure has been successfully placed but not yet fully built, it will be referenced here
	edict_t* ActionTarget = nullptr; // Mostly used for dropping health packs and ammo for players where the drop location might be moving around
	bool bHasAttemptedAction = false; // Has the commander tried placing a structure or item at the build location? If so, and it didn't appear, will try to adjust view around until it works
	float StructureBuildAttemptTime = 0.0f; // When the commander tried placing a structure. Commander will wait a short while to confirm if the building appeared or if it should try again
	int NumActionAttempts = 0; // Commander will give up after a certain number of attempts to place structure/item
	AvHMessageID ResearchId = MESSAGE_NULL; // What research to perform if research action
	bool bIsAwaitingBuildLink = false; // The AI has tried placing a structure or item and is waiting to confirm it worked or not
	bool bIsActionUrgent = false;

} commander_action;

typedef enum
{
	ORDERPURPOSE_NONE,
	ORDERPURPOSE_SECURE_HIVE,
	ORDERPURPOSE_SIEGE_HIVE,
	ORDERPURPOSE_SECURE_RESNODE
} AvHAIOrderPurpose;

typedef struct _AI_COMMANDER_ORDER
{
	edict_t* Assignee = nullptr;
	AvHAIOrderPurpose OrderPurpose = ORDERPURPOSE_NONE;
	edict_t* OrderTarget = nullptr;
	Vector OrderLocation = g_vecZero;
	float LastReminderTime = 0.0f;
	float LastPlayerDistance = 0.0f;
} ai_commander_order;

typedef struct _AI_COMMANDER_REQUEST
{
	bool bNewRequest = false; // Is this a new request just come in?
	edict_t* Requestor = nullptr; // Who sent the request?
	AvHMessageID RequestType = MESSAGE_NULL; // What did they request?
	bool bAcknowledged = false; // If we can't satisfy the request right now, have we at least acknowledged it?
	bool bResponded = false; // Have we already responded to this request?
	float RequestTime = 0.0f; // When the request came in
	int ResponseAttempts = 0; // How many times have we tried to respond to this request?
	Vector RequestLocation = g_vecZero; // Where was the request raised? Ideal drop location for stuff
} ai_commander_request;

typedef struct AVH_AI_PLAYER
{
	AvHPlayer* Player = nullptr;
	edict_t* Edict = nullptr;
	AvHTeamNumber	Team = TEAM_IND;
	float			ForwardMove = 0.0f;
	float			SideMove = 0.0f;
	float			UpMove = 0.0f;
	int				Button = 0.0f;
	int				Impulse = 0.0f;
	byte			AdjustedMsec = 0;

	bool bIsPendingKill = false;
	bool bIsInactive = false;

	float LastUseTime = 0.0f;

	float f_previous_command_time = 0.0f;

	Vector desiredMovementDir = g_vecZero;
	Vector CurrentLadderNormal = g_vecZero;
	Vector CurrentEyePosition = g_vecZero;
	Vector CurrentFloorPosition = g_vecZero;
	Vector LastPosition = g_vecZero;

	Vector CollisionHullBottomLocation = g_vecZero;
	Vector CollisionHullTopLocation = g_vecZero;

	float TimeSinceLastMovement = 0.0f;

	AvHAIWeapon DesiredMoveWeapon = WEAPON_INVALID;
	AvHAIWeapon DesiredCombatWeapon = WEAPON_INVALID;

	frustum_plane_t viewFrustum[6]; // Bot's view frustum. Essentially, their "screen" for determining visibility of stuff
	enemy_status TrackedEnemies[32];
	int CurrentEnemy = -1;
	AvHAICombatStrategy CurrentCombatStrategy = COMBAT_STRATEGY_ATTACK;
	edict_t* CurrentEnemyRef = nullptr;

	vector<AvHAIBuildableStructure> DangerTurrets;

	AvHAIPlayerTask PrimaryBotTask;
	AvHAIPlayerTask SecondaryBotTask;
	AvHAIPlayerTask WantsAndNeedsTask;
	AvHAIPlayerTask CommanderTask; // Task assigned by the commander
	AvHAIPlayerTask* CurrentTask = &PrimaryBotTask; // Bot's current task they're performing

	float BotNextTaskEvaluationTime = 0.0f;

	bot_skill BotSkillSettings;

	char PathStatus[128]; // Debug used to help figure out what's going on with a bot's path finding
	char MoveStatus[128]; // Debug used to help figure out what's going on with a bot's steering

	nav_status BotNavInfo; // Bot's movement information, their current path, where in the path they are etc.

	vector<ai_commander_request> ActiveRequests;
	vector<ai_commander_order> ActiveOrders;
	
	float next_commander_action_time = 0.0f;

	bot_msg ChatMessages[5]; // Bot can have up to 5 chat messages pending

	float LastCombatTime = 0.0f;

	AvHAIGuardInfo GuardInfo;

	float LastRequestTime = 0.0f; // When bot last used a voice line to request something. Prevents spam

	Vector DesiredLookDirection = g_vecZero; // What view angle is the bot currently turning towards
	Vector InterpolatedLookDirection = g_vecZero; // Used to smoothly interpolate the bot's view rather than snap instantly like an aimbot
	edict_t* LookTarget = nullptr; // Used to work out what view angle is needed to look at the desired entity
	Vector LookTargetLocation = g_vecZero; // This is the bot's current desired look target. Could be an enemy (see LookTarget), or point of interest
	Vector MoveLookLocation = g_vecZero; // If the bot has to look somewhere specific for movement (e.g. up for a ladder or wall-climb), this will override LookTargetLocation so the bot doesn't get distracted and mess the move up
	float LastTargetTrackUpdate = 0.0f; // Add a delay to how frequently a bot can track a target's movements
	float ViewInterpolationSpeed = 0.0f; // How fast should the bot turn its view? Depends on distance to turn
	float ViewInterpStartedTime = 0.0f; // Used for interpolation

	float ViewUpdateRate = 0.2f; // How frequently the bot can react to new sightings of enemies etc.
	float LastViewUpdateTime = 0.0f; // Used to throttle view updates based on ViewUpdateRate

	Vector ViewForwardVector = g_vecZero; // Bot's current forward unit vector
	Vector LastSafeLocation = g_vecZero;

	AvHAIBotRole BotRole = BOT_ROLE_NONE;

	int ExperiencePointsAvailable = 0; // How much experience the bot has to spend
	AvHMessageID NextCombatModeUpgrade = MESSAGE_NULL;

	float ThinkDelta = 0.0f; // How long since this bot last ran AIPlayerThink
	float LastThinkTime = 0.0f; // When the bot last ran AIPlayerThink

	float ServerUpdateDelta = 0.0f; // How long since we last called RunPlayerMove
	float LastServerUpdateTime = 0.0f; // When we last called RunPlayerMove

	float HearingThreshold = 0.0f; // How loud does a sound need to be before the bot detects it? This is set when hearing a sound so that louder sounds drown out quieter ones, and decrements quickly

	int DebugValue = 0; // Used for debugging the bot

} AvHAIPlayer;

typedef struct _AVH_AI_SQUAD
{
	AvHTeamNumber SquadTeam = TEAM_IND; // Which team this squad is for
	vector<AvHAIPlayer*> SquadMembers; // Which bots are assigned to this
	Vector SquadGatherLocation = g_vecZero; // Where should the squad gather before attempting the objective?
	edict_t* SquadTarget = nullptr; // The target of the objective
	BotTaskType SquadObjective = TASK_NONE; // What to do with the objective
	bool bExecuteObjective = false; // Are we at the gather or execute phase?

	bool IsValid()
	{
		return (SquadMembers.size() > 0 && !FNullEnt(SquadTarget));
	}
} AvHAISquad;


#endif