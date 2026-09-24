#pragma once

#ifndef AVH_AI_MARINE_BUILD_ORDER_H
#define AVH_AI_MARINE_BUILD_ORDER_H

#include "AvHAIConstants.h"
#include <string>
#include <vector>

typedef enum
{
	BUILD_ORDER_NONE,
	BUILD_ORDER_STRUCTURE,
	BUILD_ORDER_UPGRADE,
} AvHAIBuildOrderType;

typedef enum
{
	CONDITION_NONE,
	STRUCTURE_EXISTS,
	UPGRADE_EXISTS,
	TIME_ELAPSED,
} AvHAIBuildCondition;

typedef enum
{
	OR,
	AND,
} AvHAILogicConnective;

typedef struct _BUILD_ORDER_ENTRY
{
	AvHAIBuildOrderType          BuildOrderType = BUILD_ORDER_NONE;
	AvHAIDeployableStructureType StructureToBuild = STRUCTURE_NONE;
	AvHTechID                    UpgradeToResearch = TECH_NULL;
	AvHAIBuildCondition          BuildConditionOne = CONDITION_NONE;
	AvHAIDeployableStructureType StructureRequiredOne = STRUCTURE_NONE;
	AvHTechID                    UpgradeRequiredOne = TECH_NULL;
	AvHAILogicConnective         Connective = AND;
	AvHAIBuildCondition          BuildConditionTwo = CONDITION_NONE;
	AvHAIDeployableStructureType StructureRequiredTwo = STRUCTURE_NONE;
	AvHTechID                    UpgradeRequiredTwo = TECH_NULL;
	int TimeLimitInSecondsOne = 0;
	int TimeLimitInSecondsTwo = 0;
} Build_order_entry;

typedef struct _MARINE_BUILD_ORDER {
	std::vector<Build_order_entry> BuildOrder = {};
    std::string BuildOrderName = "";
	float Weight = 1.0f;
	int InitialInfantryPortalCount = 1;
} Marine_build_order;

extern std::vector<Marine_build_order> AIBO_MarineBuildOrders;
extern Marine_build_order* AIBO_CurrentBuildOrder;
extern vector<std::string> BuildOrderMessages;
extern bool BuildMessageAnnounced;
extern int BuildMessageAnnouncementIndex;

void AIBO_LoadHardCodedMarineBuildOrder();
void AIBO_ParseMarineBuildOrder();
void AIBO_ResetMarineBuildOrder();
void AIBO_SelectBuildOrderRandomly();
std::string GetBuildOrderMessage();
bool AIBO_BuildOrderIsShotgunRush();
int AIBO_IntRandomRange(int MinValue, int MaxValue);
float AIBO_FloatRandomRange(float MinValue, float MaxValue);
AvHAIDeployableStructureType AIBO_MapStringToStructure(const std::string& StructureName);
AvHAIDeployableStructureType AIBO_MapTechToRequiredStructure(AvHTechID TechID);
AvHAIBuildCondition AIBO_MapStringToBuildCondition(const std::string& ConditionName);
AvHAIBuildOrderType AIBO_MapStringToBuildOrderType(const std::string& BuildOrderTypeName);
AvHMessageID AIBO_MapTechIDToMessageID(AvHTechID TechID);
int AIBO_GetResearchCost(AvHTechID TechID);
AvHTechID AIBO_MapStringToTech(const std::string& TechName);

#endif