#include "AvHAIMarineBuildOrderManager.h"
#include <string>
#include <algorithm>

std::vector<Marine_build_order> AIBO_MarineBuildOrders;
Marine_build_order* AIBO_CurrentBuildOrder = nullptr;
bool BuildMessageAnnounced = false;

bool bo_rng_initialized = false;
int BuildMessageAnnouncementIndex = 0;

const std::vector<std::string> BuildOrderMessageTemplates = {
    "Guys today we're doing a nice {} build!",
    "Today we're going to do a {} build, guys!",
    "I'm feeling like trying a {} build today, my friends!",
    "Let's go for a {} build today, team!",
    "Why not do the famous {} build today, everyone?",
    "I'm not sure about you, but I'm excited to try the {} build this time!",
};

int AIBO_IntRandomRange(int MinValue, int MaxValue)
{
    if (MinValue == MaxValue) { return MinValue; }
    return MinValue + (rand() % (MaxValue - MinValue + 1));
}

float AIBO_FloatRandomRange(float MinValue, float MaxValue)
{
    if (MinValue == MaxValue) { return MinValue; }
    return ((float(rand()) / float(RAND_MAX)) * (MaxValue - MinValue)) + MinValue;
}

AvHAIDeployableStructureType AIBO_MapStringToStructure(const std::string& StructureName)
{
    if (!stricmp(StructureName.c_str(), "CommChair"))
        return STRUCTURE_MARINE_COMMCHAIR;
    if (!stricmp(StructureName.c_str(), "InfantryPortal"))
        return STRUCTURE_MARINE_INFANTRYPORTAL;
    if (!stricmp(StructureName.c_str(), "Armoury") || !stricmp(StructureName.c_str(), "Armory"))
        return STRUCTURE_MARINE_ARMOURY;
    if (!stricmp(StructureName.c_str(), "TurretFactory"))
        return STRUCTURE_MARINE_TURRETFACTORY;
    if (!stricmp(StructureName.c_str(), "Turrets"))
        return STRUCTURE_MARINE_TURRET;
    if (!stricmp(StructureName.c_str(), "Resourcetower") || !stricmp(StructureName.c_str(), "Restower") || !stricmp(StructureName.c_str(), "Resourcetowers") || !stricmp(StructureName.c_str(), "Restowers"))
        return STRUCTURE_MARINE_RESTOWER;
    if (!stricmp(StructureName.c_str(), "Armslab"))
        return STRUCTURE_MARINE_ARMSLAB;
    if (!stricmp(StructureName.c_str(), "Observatory"))
        return STRUCTURE_MARINE_OBSERVATORY;
    if (!stricmp(StructureName.c_str(), "Phasegate"))
        return STRUCTURE_MARINE_PHASEGATE;
    if (!stricmp(StructureName.c_str(), "AdvancedArmoury") || !stricmp(StructureName.c_str(), "AdvancedArmory"))
        return STRUCTURE_MARINE_ADVARMOURY;
    if (!stricmp(StructureName.c_str(), "ProtoLab"))
        return STRUCTURE_MARINE_PROTOTYPELAB;

    return STRUCTURE_NONE;
}

AvHAIDeployableStructureType AIBO_MapTechToRequiredStructure(AvHTechID TechID)
{
    switch (TechID)
    {
        case TECH_RESEARCH_ELECTRICAL: return STRUCTURE_MARINE_TURRETFACTORY;
        case TECH_RESEARCH_ARMOR_ONE: return STRUCTURE_MARINE_ARMSLAB;
        case TECH_RESEARCH_ARMOR_TWO: return STRUCTURE_MARINE_ARMSLAB;
        case TECH_RESEARCH_ARMOR_THREE: return STRUCTURE_MARINE_ARMSLAB;
        case TECH_RESEARCH_WEAPONS_ONE: return STRUCTURE_MARINE_ARMSLAB;
        case TECH_RESEARCH_WEAPONS_TWO: return STRUCTURE_MARINE_ARMSLAB;
        case TECH_RESEARCH_WEAPONS_THREE: return STRUCTURE_MARINE_ARMSLAB;
        case TECH_ADVANCED_TURRET_FACTORY: return STRUCTURE_MARINE_TURRETFACTORY;
        case TECH_RESEARCH_JETPACKS: return STRUCTURE_MARINE_PROTOTYPELAB;
        case TECH_RESEARCH_HEAVYARMOR: return STRUCTURE_MARINE_PROTOTYPELAB;
        case TECH_RESEARCH_DISTRESSBEACON: return STRUCTURE_MARINE_OBSERVATORY;
        case TECH_RESEARCH_MOTIONTRACK: return STRUCTURE_MARINE_OBSERVATORY;
        case TECH_RESEARCH_PHASETECH: return STRUCTURE_MARINE_OBSERVATORY;
        case TECH_RESEARCH_CATALYSTS: return STRUCTURE_MARINE_ARMSLAB;
        case TECH_RESEARCH_GRENADES: return STRUCTURE_MARINE_ARMOURY;
        default: return STRUCTURE_NONE; // No structure required for this tech
    }
}

AvHAIBuildCondition AIBO_MapStringToBuildCondition(const std::string& ConditionName)
{
    if (!stricmp(ConditionName.c_str(), "StructureExists"))
        return STRUCTURE_EXISTS;
    if (!stricmp(ConditionName.c_str(), "UpgradeExists"))
        return UPGRADE_EXISTS;
    if (!stricmp(ConditionName.c_str(), "TimeElapsed"))
        return TIME_ELAPSED;
	return CONDITION_NONE;
}

AvHAIBuildOrderType AIBO_MapStringToBuildOrderType(const std::string& BuildOrderTypeName)
{
    if (!stricmp(BuildOrderTypeName.c_str(), "Structure"))
        return BUILD_ORDER_STRUCTURE;
    if (!stricmp(BuildOrderTypeName.c_str(), "Upgrade"))
        return BUILD_ORDER_UPGRADE;
    return BUILD_ORDER_NONE;
}

AvHTechID AIBO_MapStringToTech(const std::string& TechName)
{
    if (!stricmp(TechName.c_str(), "Electrification"))
		return TECH_RESEARCH_ELECTRICAL;
    if (!stricmp(TechName.c_str(), "ArmorOne"))
        return TECH_RESEARCH_ARMOR_ONE;
    if (!stricmp(TechName.c_str(), "ArmorTwo"))
        return TECH_RESEARCH_ARMOR_TWO;
    if (!stricmp(TechName.c_str(), "ArmorThree"))
        return TECH_RESEARCH_ARMOR_THREE;
    if (!stricmp(TechName.c_str(), "WeaponsOne"))
        return TECH_RESEARCH_WEAPONS_ONE;
    if (!stricmp(TechName.c_str(), "WeaponsTwo"))
        return TECH_RESEARCH_WEAPONS_TWO;
    if (!stricmp(TechName.c_str(), "WeaponsThree"))
        return TECH_RESEARCH_WEAPONS_THREE;
	if (!stricmp(TechName.c_str(), "AdvancedTurretFactory"))
		return TECH_ADVANCED_TURRET_FACTORY;
	if (!stricmp(TechName.c_str(), "Jetpacks"))
		return TECH_RESEARCH_JETPACKS;
    if (!stricmp(TechName.c_str(), "HeavyArmor"))
		return TECH_RESEARCH_HEAVYARMOR;
    if (!stricmp(TechName.c_str(), "DistressBeacon"))
		return TECH_RESEARCH_DISTRESSBEACON;
	if (!stricmp(TechName.c_str(), "HealthTech"))
		return TECH_RESEARCH_HEALTH;
	if (!stricmp(TechName.c_str(), "MotionTracking"))
		return TECH_RESEARCH_MOTIONTRACK;
	if (!stricmp(TechName.c_str(), "PhaseTech"))
		return TECH_RESEARCH_PHASETECH;
    if (!stricmp(TechName.c_str(), "Catalysts"))
		return TECH_RESEARCH_CATALYSTS;
	if (!stricmp(TechName.c_str(), "Grenades"))
		return TECH_RESEARCH_GRENADES;
    return TECH_NULL;
}

AvHMessageID AIBO_MapTechIDToMessageID(AvHTechID TechID)
{
    switch (TechID)
    {
        case TECH_RESEARCH_ELECTRICAL: return RESEARCH_ELECTRICAL;
        case TECH_RESEARCH_ARMOR_ONE: return RESEARCH_ARMOR_ONE;
        case TECH_RESEARCH_ARMOR_TWO: return RESEARCH_ARMOR_TWO;
        case TECH_RESEARCH_ARMOR_THREE: return RESEARCH_ARMOR_THREE;
        case TECH_RESEARCH_WEAPONS_ONE: return RESEARCH_WEAPONS_ONE;
        case TECH_RESEARCH_WEAPONS_TWO: return RESEARCH_WEAPONS_TWO;
        case TECH_RESEARCH_WEAPONS_THREE: return RESEARCH_WEAPONS_THREE;
        case TECH_ADVANCED_TURRET_FACTORY: return TURRET_FACTORY_UPGRADE;
        case TECH_RESEARCH_JETPACKS: return RESEARCH_JETPACKS;
        case TECH_RESEARCH_HEAVYARMOR: return RESEARCH_HEAVYARMOR;
        case TECH_RESEARCH_DISTRESSBEACON: return RESEARCH_DISTRESSBEACON;
        case TECH_RESEARCH_HEALTH: return RESEARCH_HEALTH;
        case TECH_RESEARCH_MOTIONTRACK: return RESEARCH_MOTIONTRACK;
        case TECH_RESEARCH_PHASETECH: return RESEARCH_PHASETECH;
        case TECH_RESEARCH_CATALYSTS: return RESEARCH_CATALYSTS;
        case TECH_RESEARCH_GRENADES: return RESEARCH_GRENADES;
        default: return MESSAGE_NULL;
    }
}

int AIBO_GetResearchCost(AvHTechID TechID)
{
    switch (TechID)
    {
    case TECH_RESEARCH_ELECTRICAL: return (BALANCE_VAR(kElectricalUpgradeResearchCost));
        case TECH_RESEARCH_ARMOR_ONE: return (BALANCE_VAR(kArmorOneResearchCost));
        case TECH_RESEARCH_ARMOR_TWO: return (BALANCE_VAR(kArmorTwoResearchCost));
        case TECH_RESEARCH_ARMOR_THREE: return (BALANCE_VAR(kArmorThreeResearchCost));
        case TECH_RESEARCH_WEAPONS_ONE: return (BALANCE_VAR(kWeaponsOneResearchCost));
        case TECH_RESEARCH_WEAPONS_TWO: return (BALANCE_VAR(kWeaponsTwoResearchCost));
        case TECH_RESEARCH_WEAPONS_THREE: return (BALANCE_VAR(kWeaponsThreeResearchCost));
        case TECH_ADVANCED_TURRET_FACTORY: return (BALANCE_VAR(kTurretFactoryUpgradeCost));
        case TECH_RESEARCH_JETPACKS: return (BALANCE_VAR(kJetpacksResearchCost));
        case TECH_RESEARCH_HEAVYARMOR: return (BALANCE_VAR(kHeavyArmorResearchCost));
        case TECH_RESEARCH_MOTIONTRACK: return (BALANCE_VAR(kMotionTrackingResearchCost));
        case TECH_RESEARCH_PHASETECH: return (BALANCE_VAR(kPhaseTechResearchCost));
        case TECH_RESEARCH_CATALYSTS: return (BALANCE_VAR(kCatalystResearchCost));
        case TECH_RESEARCH_GRENADES: return (BALANCE_VAR(kGrenadesResearchCost));
        default: return -1; // No cost for this tech
    }
}

void AIBO_LoadHardCodedMarineBuildOrder()
{
	Marine_build_order defaultBuildOrder;
    std::vector<Build_order_entry> entries;
    // Add each entry using push_back instead of initializer list
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_ARMOURY, TECH_NULL, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL});
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_TURRETFACTORY, TECH_NULL, TIME_ELAPSED, STRUCTURE_NONE, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL, 180, 0});
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_TURRET, TECH_NULL, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL});
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_RESTOWER, TECH_NULL, STRUCTURE_EXISTS, STRUCTURE_MARINE_ARMOURY, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL});
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_INFANTRYPORTAL, TECH_NULL, TIME_ELAPSED, STRUCTURE_NONE, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL, 180, 0 });
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_ARMSLAB, TECH_NULL, STRUCTURE_EXISTS, STRUCTURE_MARINE_TURRETFACTORY, TECH_NULL, OR, TIME_ELAPSED, STRUCTURE_NONE, TECH_NULL, 200, 0});
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_OBSERVATORY, TECH_NULL, STRUCTURE_EXISTS, STRUCTURE_MARINE_ARMSLAB});
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_PHASEGATE, TECH_NULL, STRUCTURE_EXISTS, STRUCTURE_MARINE_OBSERVATORY});
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_ADVARMOURY, TECH_NULL, STRUCTURE_EXISTS, STRUCTURE_MARINE_PHASEGATE});
    entries.push_back(Build_order_entry{BUILD_ORDER_STRUCTURE, STRUCTURE_MARINE_PROTOTYPELAB, TECH_NULL, STRUCTURE_EXISTS ,STRUCTURE_MARINE_ADVARMOURY });
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE ,STRUCTURE_NONE ,TECH_RESEARCH_GRENADES ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL ,AND ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL });
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE ,STRUCTURE_NONE ,TECH_RESEARCH_ARMOR_ONE ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL ,AND ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL });
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE ,STRUCTURE_NONE ,TECH_RESEARCH_WEAPONS_ONE ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL ,AND ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL });
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE ,STRUCTURE_NONE ,TECH_RESEARCH_PHASETECH ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL ,AND ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL });
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE ,STRUCTURE_NONE ,TECH_RESEARCH_MOTIONTRACK ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL ,AND ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL });
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE ,STRUCTURE_NONE ,TECH_RESEARCH_ARMOR_TWO ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL ,AND ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL });
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE ,STRUCTURE_NONE ,TECH_RESEARCH_WEAPONS_TWO ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL ,AND ,CONDITION_NONE ,STRUCTURE_NONE ,TECH_NULL });
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE, STRUCTURE_NONE, TECH_RESEARCH_CATALYSTS, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL});
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE, STRUCTURE_NONE, TECH_RESEARCH_HEAVYARMOR, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL});
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE, STRUCTURE_NONE, TECH_RESEARCH_JETPACKS, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL});
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE, STRUCTURE_NONE, TECH_RESEARCH_ARMOR_THREE, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL});
    entries.push_back(Build_order_entry{BUILD_ORDER_UPGRADE, STRUCTURE_NONE, TECH_RESEARCH_WEAPONS_THREE, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL, AND, CONDITION_NONE, STRUCTURE_NONE, TECH_NULL});

    defaultBuildOrder.BuildOrder = entries;
    defaultBuildOrder.BuildOrderName = "Default Build-Order";
    defaultBuildOrder.Weight = 1.0f;
    AIBO_MarineBuildOrders.push_back(defaultBuildOrder);
    AIBO_CurrentBuildOrder = &AIBO_MarineBuildOrders[0]; // Set the current build order to the default one
}

void AIBO_ParseMarineBuildOrder()
{
    BuildMessageAnnounced = false;
    AIBO_MarineBuildOrders.clear();

    std::string BuildOrderFileString = std::string(getModDirectory()) + "/marine_build_orders.txt";
    std::ifstream BuildOrderFile(BuildOrderFileString.c_str());

    if (!BuildOrderFile.is_open())
    {
		g_engfuncs.pfnServerPrint("Failed to open marine build order file. Using default.\n");
		AIBO_LoadHardCodedMarineBuildOrder();
        return;
    }

    std::string Line;
    Marine_build_order currentOrder;
    bool inBuildOrder = false;

    while (std::getline(BuildOrderFile, Line))
    {
        // Remove whitespace from both ends
        Line.erase(Line.begin(), std::find_if(Line.begin(), Line.end(), [](int ch) { return !std::isspace(ch); }));
        Line.erase(std::find_if(Line.rbegin(), Line.rend(), [](int ch) { return !std::isspace(ch); }).base(), Line.end());

        if (Line.empty() || Line[0] == '#')
            continue;

        if (Line == "BuildOrderStart")
        {
            inBuildOrder = true;
            currentOrder = Marine_build_order(); // reset
            continue;
        }
        if (Line == "BuildOrderEnd")
        {
            if (inBuildOrder)
            {
                AIBO_MarineBuildOrders.push_back(currentOrder);
                inBuildOrder = false;
            }
            continue;
        }
        if (!inBuildOrder)
            continue;

        if (Line.find("name=") == 0)
        {
            currentOrder.BuildOrderName = Line.substr(5);
        }
        else if (Line.find("weight=") == 0)
        {
            currentOrder.Weight = std::stof(Line.substr(7));
        }
        else if (Line.find("initialIPs=") == 0)
        {
			currentOrder.InitialInfantryPortalCount = std::stoi(Line.substr(11));
        }
        else if (Line.find("entry=") == 0)
        {
            std::string entryLine = Line.substr(6);

			vector<std::string> entryParts;
            entryParts.reserve(7);
			std::string entryName;
			size_t pos = 0;
			int entryCount = 0;
			while ((pos = entryLine.find(',')) != std::string::npos) {
				entryName = entryLine.substr(0, pos);
				entryLine.erase(0, pos + 1);
				entryParts.push_back(entryName);
				entryCount++;
			}
			// Add the last part after the last comma
			if (!entryLine.empty()) {
				entryParts.push_back(entryLine);
				entryCount++;
			}
            if (entryCount != 7)
            {
				continue; // Invalid entry, skip
            }
            
            Build_order_entry entry;
			entry.BuildOrderType = AIBO_MapStringToBuildOrderType(entryParts[0]);
			entry.StructureToBuild = (entry.BuildOrderType == BUILD_ORDER_STRUCTURE) ? AIBO_MapStringToStructure(entryParts[1]) : STRUCTURE_NONE;
			entry.UpgradeToResearch = (entry.BuildOrderType == BUILD_ORDER_UPGRADE) ? AIBO_MapStringToTech(entryParts[1]) : TECH_NULL;
			entry.BuildConditionOne = AIBO_MapStringToBuildCondition(entryParts[2]);
			entry.StructureRequiredOne = (entry.BuildConditionOne == STRUCTURE_EXISTS) ? AIBO_MapStringToStructure(entryParts[3]) : STRUCTURE_NONE;
			entry.UpgradeRequiredOne = (entry.BuildConditionOne == UPGRADE_EXISTS) ? AIBO_MapStringToTech(entryParts[3]) : TECH_NULL;
            entry.Connective = (!stricmp(entryParts[4].c_str(), "or")) ? OR : AND;
			entry.BuildConditionTwo = AIBO_MapStringToBuildCondition(entryParts[5]);
			entry.StructureRequiredTwo = (entry.BuildConditionTwo == STRUCTURE_EXISTS) ? AIBO_MapStringToStructure(entryParts[6]) : STRUCTURE_NONE;
			entry.UpgradeRequiredTwo = (entry.BuildConditionTwo == UPGRADE_EXISTS) ? AIBO_MapStringToTech(entryParts[6]) : TECH_NULL;
            if (entry.BuildConditionOne == TIME_ELAPSED)
            {
                entry.TimeLimitInSecondsOne = std::stoi(entryParts[3]);
                std::string message = "TimeLimitOne tried parsing " + entryParts[3] + ". Got " + std::to_string(std::stoi(entryParts[3])) + "\n";
                g_engfuncs.pfnServerPrint(message.c_str());
            }
            if (entry.BuildConditionTwo == TIME_ELAPSED)
            {
                entry.TimeLimitInSecondsTwo = std::stoi(entryParts[6]);
                std::string message = "TimeLimitOne tried parsing " + entryParts[6] + ". Got " + std::to_string(std::stoi(entryParts[6])) + "\n";
                g_engfuncs.pfnServerPrint(message.c_str());
            }
            currentOrder.BuildOrder.push_back(entry);
        }
    }

	BuildOrderFile.close();

}


void AIBO_ResetMarineBuildOrder() {
    BuildMessageAnnounced = false;
}

void AIBO_SelectBuildOrderRandomly() {
    if (!bo_rng_initialized)
    {
		unsigned int seed = static_cast<unsigned int>(time(nullptr));
        srand(seed);
        bo_rng_initialized = true;
    }

    if (AIBO_MarineBuildOrders.empty()) {
        AIBO_LoadHardCodedMarineBuildOrder();
    }

    std::vector<float> cumulativeWeights;
    cumulativeWeights.reserve(AIBO_MarineBuildOrders.size());
    float sumWeights = 0.0f;

    for (auto& order : AIBO_MarineBuildOrders)
    {
        sumWeights += order.Weight;
        cumulativeWeights.push_back(sumWeights);
    }

    float randomValue = AIBO_FloatRandomRange(0.0f, sumWeights);
    auto it = std::lower_bound(cumulativeWeights.begin(), cumulativeWeights.end(), randomValue);

    if (it != cumulativeWeights.end()) {
        size_t index = std::distance(cumulativeWeights.begin(), it);
        AIBO_CurrentBuildOrder = &AIBO_MarineBuildOrders[index];
    }
    else {
        AIBO_CurrentBuildOrder = &AIBO_MarineBuildOrders.back();
    }

	BuildMessageAnnouncementIndex = rand() % BuildOrderMessageTemplates.size();
    BuildMessageAnnounced = false;
}

std::string GetBuildOrderMessage() {
    if (!AIBO_CurrentBuildOrder) return "No build order selected!";
    const std::string& tmpl = BuildOrderMessageTemplates[BuildMessageAnnouncementIndex];
    size_t pos = tmpl.find("{}");
    if (pos != std::string::npos) {
        std::string msg = tmpl;
        msg.replace(pos, 2, AIBO_CurrentBuildOrder->BuildOrderName);
        return msg;
    }
    return tmpl;
}

bool AIBO_BuildOrderIsShotgunRush() {
    if (!AIBO_CurrentBuildOrder) return false;
    return !stricmp(AIBO_CurrentBuildOrder->BuildOrderName.c_str(), "Shotgun Rush");
}
