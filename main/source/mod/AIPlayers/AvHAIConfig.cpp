
#include "AvHAIConfig.h"
#include "AvHAIMath.h"

#include "../AvHServerUtil.h"

#include <unordered_map>

float fCommanderWaitTime = 10.0f;
float fLerkCooldown = 60.0f;
float MaxStuckTime = 30.0f;

bool bLerkAllowed = true;
bool bFadeAllowed = true;
bool bOnosAllowed = true;

std::unordered_map<std::string, TeamSizeDefinitions> TeamSizeMap;

std::unordered_map<std::string, bot_skill> BotSkillLevelsMap;
std::string CurrentSkillLevel;

std::string GlobalSkillLevel = "default";

AvHMessageID ChamberSequence[3] = { ALIEN_BUILD_DEFENSE_CHAMBER, ALIEN_BUILD_MOVEMENT_CHAMBER, ALIEN_BUILD_SENSORY_CHAMBER };

char BotPrefix[32] = "";


float CONFIG_GetCommanderWaitTime()
{
    return fCommanderWaitTime;
}

float CONFIG_GetLerkCooldown()
{
    return fLerkCooldown;
}

bool CONFIG_IsLerkAllowed()
{
    return bLerkAllowed;
}

bool CONFIG_IsFadeAllowed()
{
    return bFadeAllowed;
}

bool CONFIG_IsOnosAllowed()
{
    return bOnosAllowed;
}

float CONFIG_GetMaxStuckTime()
{
    return MaxStuckTime;
}

string CONFIG_GetBotPrefix()
{
    return string(BotPrefix);
}

int CONFIG_GetTeamASizeForMap(const char* MapName)
{
    std::string s = MapName;
    std::unordered_map<std::string, TeamSizeDefinitions>::const_iterator got = TeamSizeMap.find(s);

    if (got == TeamSizeMap.end())
    {
        return TeamSizeMap["default"].TeamASize;
    }
    else
    {
        return got->second.TeamASize;
    }
}

int CONFIG_GetTeamBSizeForMap(const char* MapName)
{
    std::string s = MapName;
    std::unordered_map<std::string, TeamSizeDefinitions>::const_iterator got = TeamSizeMap.find(s);

    if (got == TeamSizeMap.end())
    {
        return TeamSizeMap["default"].TeamBSize;
    }
    else
    {
        return got->second.TeamBSize;
    }
}

AvHMessageID CONFIG_GetHiveTechAtIndex(const int Index)
{
    if (Index < 0 || Index > 2) { return MESSAGE_NULL; }

    return ChamberSequence[Index];
}

bot_skill CONFIG_GetBotSkillLevel(const char* SkillName)
{
    std::string s = SkillName;
    std::unordered_map<std::string, bot_skill>::const_iterator got = BotSkillLevelsMap.find(s);

    if (got == BotSkillLevelsMap.end())
    {
        return BotSkillLevelsMap["default"];
    }
    else
    {
        return got->second;
    }
}

bool CONFIG_BotSkillLevelExists(const char* SkillName)
{
    std::string s = SkillName;
    std::unordered_map<std::string, bot_skill>::const_iterator got = BotSkillLevelsMap.find(s);

    return (got != BotSkillLevelsMap.end());
}

bot_skill CONFIG_GetGlobalBotSkillLevel()
{
    return BotSkillLevelsMap[GlobalSkillLevel.c_str()];
}

void CONFIG_ParseConfigFile()
{
    TeamSizeMap.clear();
    TeamSizeMap["default"].TeamASize = 6;
    TeamSizeMap["default"].TeamBSize = 6;

    BotSkillLevelsMap.clear();

    BotSkillLevelsMap["default"].marine_bot_aim_skill = 0.3f;
    BotSkillLevelsMap["default"].marine_bot_motion_tracking_skill = 0.3f;
    BotSkillLevelsMap["default"].marine_bot_reaction_time = 0.3f;
    BotSkillLevelsMap["default"].marine_bot_view_speed = 1.0f;
    BotSkillLevelsMap["default"].alien_bot_aim_skill = 0.5f;
    BotSkillLevelsMap["default"].alien_bot_motion_tracking_skill = 0.5f;
    BotSkillLevelsMap["default"].alien_bot_reaction_time = 0.3f;
    BotSkillLevelsMap["default"].alien_bot_view_speed = 1.5f;

    CurrentSkillLevel = "default";


    string BotConfigFile = string(getModDirectory()) + "/bots.cfg";

    const char* filename = BotConfigFile.c_str();

    std::ifstream cFile(filename);
    if (cFile.is_open())
    {
        std::string line;
        while (getline(cFile, line))
        {
            line.erase(std::remove_if(line.begin(), line.end(), isspace),
                line.end());
            if (line[0] == '#' || line.empty())
                continue;
            auto delimiterPos = line.find("=");
            auto key = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);

            if (key.compare("TeamSize") == 0)
            {
                auto mapDelimiterPos = value.find(":");

                if (mapDelimiterPos == std::string::npos)
                {
                    continue;
                }

                auto mapName = value.substr(0, mapDelimiterPos);
                auto teamSizes = value.substr(mapDelimiterPos + 1);
                auto sizeDelimiterPos = teamSizes.find("/");
                if (sizeDelimiterPos == std::string::npos)
                {
                    continue;
                }
                auto marineSize = teamSizes.substr(0, sizeDelimiterPos);
                auto alienSize = teamSizes.substr(sizeDelimiterPos + 1);

                int iMarineSize = atoi(marineSize.c_str());
                int iAlienSize = atoi(alienSize.c_str());

                if (iMarineSize >= 0 && iMarineSize <= 32 && iAlienSize >= 0 && iAlienSize <= 32)
                {
                    TeamSizeMap[mapName].TeamASize = atoi(marineSize.c_str());
                    TeamSizeMap[mapName].TeamBSize = atoi(alienSize.c_str());
                }

                continue;
            }

            if (key.compare("prefix") == 0)
            {
                sprintf(BotPrefix, value.c_str());

                continue;
            }

            if (key.compare("CommanderWaitTime") == 0)
            {
                fCommanderWaitTime = (float)atoi(value.c_str());
                fCommanderWaitTime = fmaxf(0.0f, fCommanderWaitTime);
                continue;
            }

            if (key.compare("LerkCooldown") == 0)
            {
                fLerkCooldown = (float)atoi(value.c_str());
                fLerkCooldown = fmaxf(0.0f, fLerkCooldown);
                continue;
            }

            if (key.compare("AllowLerk") == 0)
            {
                bLerkAllowed = atoi(value.c_str()) > 0;
                continue;
            }

            if (key.compare("AllowFade") == 0)
            {
                bFadeAllowed = atoi(value.c_str()) > 0;
                continue;
            }

            if (key.compare("AllowOnos") == 0)
            {
                bOnosAllowed = atoi(value.c_str()) > 0;
                continue;
            }

            if (key.compare("MaxStuckTime") == 0)
            {
                MaxStuckTime = (float)atoi(value.c_str());
                MaxStuckTime = fmaxf(0.0f, MaxStuckTime);
                continue;
            }

            if (key.compare("BotSkillName") == 0)
            {
                BotSkillLevelsMap[value.c_str()].marine_bot_aim_skill = 0.5f;
                BotSkillLevelsMap[value.c_str()].marine_bot_motion_tracking_skill = 0.5f;
                BotSkillLevelsMap[value.c_str()].marine_bot_reaction_time = 0.2f;
                BotSkillLevelsMap[value.c_str()].marine_bot_view_speed = 1.0f;
                BotSkillLevelsMap[value.c_str()].alien_bot_aim_skill = 0.5f;
                BotSkillLevelsMap[value.c_str()].alien_bot_motion_tracking_skill = 0.5f;
                BotSkillLevelsMap[value.c_str()].alien_bot_reaction_time = 0.2f;
                BotSkillLevelsMap[value.c_str()].alien_bot_view_speed = 1.0f;

                CurrentSkillLevel = value;
                continue;
            }

            if (key.compare("MarineReactionTime") == 0)
            {

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].marine_bot_reaction_time = clampf(NewValue, 0.0f, 1.0f);

                continue;
            }

            if (key.compare("AlienReactionTime") == 0)
            {

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].alien_bot_reaction_time = clampf(NewValue, 0.0f, 1.0f);

                continue;
            }

            if (key.compare("MarineAimSkill") == 0)
            {

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].marine_bot_aim_skill = clampf(NewValue, 0.0f, 1.0f);

                continue;
            }

            if (key.compare("AlienAimSkill") == 0)
            {

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].alien_bot_aim_skill = clampf(NewValue, 0.0f, 1.0f);

                continue;
            }

            if (key.compare("MarineMovementTracking") == 0)
            {

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].marine_bot_motion_tracking_skill = clampf(NewValue, 0.0f, 1.0f);

                continue;
            }

            if (key.compare("AlienMovementTracking") == 0)
            {

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].alien_bot_motion_tracking_skill = clampf(NewValue, 0.0f, 1.0f);

                continue;
            }

            if (key.compare("MarineViewSpeed") == 0)
            {

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].marine_bot_view_speed = clampf(NewValue, 0.0f, 5.0f);

                continue;
            }

            if (key.compare("AlienViewSpeed") == 0)
            {

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].alien_bot_view_speed = clampf(NewValue, 0.0f, 5.0f);

                continue;
            }

            if (key.compare("DefaultSkillLevel") == 0)
            {
                GlobalSkillLevel = value;

                continue;
            }

            if (key.compare("ChamberSequence") == 0)
            {
                AvHMessageID HiveOneTech = MESSAGE_NULL;
                AvHMessageID HiveTwoTech = MESSAGE_NULL;
                AvHMessageID HiveThreeTech = MESSAGE_NULL;

                std::vector<AvHMessageID> AvailableTechs = { ALIEN_BUILD_DEFENSE_CHAMBER, ALIEN_BUILD_MOVEMENT_CHAMBER, ALIEN_BUILD_SENSORY_CHAMBER };

                auto firstTechDelimiter = value.find("/");

                if (firstTechDelimiter == std::string::npos)
                {
                    continue;
                }

                auto FirstTech = value.substr(0, firstTechDelimiter);
                auto NextTechs = value.substr(firstTechDelimiter + 1);

                auto SecondTechDelimiter = NextTechs.find("/");

                if (SecondTechDelimiter == std::string::npos)
                {
                    continue;
                }

                auto SecondTech = NextTechs.substr(0, SecondTechDelimiter);
                auto ThirdTech = NextTechs.substr(SecondTechDelimiter + 1);

                if (FirstTech.compare("movement") == 0)
                {
                    HiveOneTech = ALIEN_BUILD_MOVEMENT_CHAMBER;

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_MOVEMENT_CHAMBER), AvailableTechs.end());

                }
                else if (FirstTech.compare("defense") == 0)
                {
                    HiveOneTech = ALIEN_BUILD_DEFENSE_CHAMBER;

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_DEFENSE_CHAMBER), AvailableTechs.end());
                }
                else if (FirstTech.compare("sensory") == 0)
                {
                    HiveOneTech = ALIEN_BUILD_SENSORY_CHAMBER;

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_SENSORY_CHAMBER), AvailableTechs.end());
                }

                if (SecondTech.compare("movement") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_MOVEMENT_CHAMBER) != AvailableTechs.end())
                    {
                        HiveTwoTech = ALIEN_BUILD_MOVEMENT_CHAMBER;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_MOVEMENT_CHAMBER), AvailableTechs.end());
                    }
                }
                else if (SecondTech.compare("defense") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_DEFENSE_CHAMBER) != AvailableTechs.end())
                    {
                        HiveTwoTech = ALIEN_BUILD_DEFENSE_CHAMBER;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_DEFENSE_CHAMBER), AvailableTechs.end());
                    }
                }
                else if (SecondTech.compare("sensory") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_SENSORY_CHAMBER) != AvailableTechs.end())
                    {
                        HiveTwoTech = ALIEN_BUILD_SENSORY_CHAMBER;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_SENSORY_CHAMBER), AvailableTechs.end());
                    }
                }

                if (ThirdTech.compare("movement") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_MOVEMENT_CHAMBER) != AvailableTechs.end())
                    {
                        HiveThreeTech = ALIEN_BUILD_MOVEMENT_CHAMBER;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_MOVEMENT_CHAMBER), AvailableTechs.end());
                    }
                }
                else if (ThirdTech.compare("defense") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_DEFENSE_CHAMBER) != AvailableTechs.end())
                    {
                        HiveThreeTech = ALIEN_BUILD_DEFENSE_CHAMBER;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_DEFENSE_CHAMBER), AvailableTechs.end());
                    }
                }
                else if (ThirdTech.compare("sensory") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_SENSORY_CHAMBER) != AvailableTechs.end())
                    {
                        HiveThreeTech = ALIEN_BUILD_SENSORY_CHAMBER;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), ALIEN_BUILD_SENSORY_CHAMBER), AvailableTechs.end());
                    }
                }

                if (HiveOneTech == MESSAGE_NULL)
                {
                    int random = rand() % AvailableTechs.size();
                    HiveOneTech = AvailableTechs[random];

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HiveOneTech), AvailableTechs.end());
                }

                if (HiveTwoTech == MESSAGE_NULL)
                {
                    int random = rand() % AvailableTechs.size();
                    HiveTwoTech = AvailableTechs[random];

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HiveTwoTech), AvailableTechs.end());
                }

                if (HiveThreeTech == MESSAGE_NULL)
                {
                    int random = rand() % AvailableTechs.size();
                    HiveThreeTech = AvailableTechs[random];

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HiveTwoTech), AvailableTechs.end());
                }

                ChamberSequence[0] = HiveOneTech;
                ChamberSequence[1] = HiveTwoTech;
                ChamberSequence[2] = HiveThreeTech;

                continue;
            }
        }
    }

    if (!CONFIG_BotSkillLevelExists(GlobalSkillLevel.c_str()))
    {
        GlobalSkillLevel = "default";
    }
}