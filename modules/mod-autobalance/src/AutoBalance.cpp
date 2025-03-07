/*
* Copyright (C) 2018 AzerothCore <http://www.azerothcore.org>
* Copyright (C) 2012 CVMagic <http://www.trinitycore.org/f/topic/6551-vas-autobalance/>
* Copyright (C) 2008-2010 TrinityCore <http://www.trinitycore.org/>
* Copyright (C) 2006-2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
* Copyright (C) 1985-2010 KalCorp  <http://vasserver.dyndns.org/>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/*
* Script Name: AutoBalance
* Original Authors: KalCorp and Vaughner
* Maintainer(s): AzerothCore
* Original Script Name: AutoBalance
* Description: This script is intended to scale based on number of players,
* instance mobs & world bosses' level, health, mana, and damage.
*/

#include "Configuration/Config.h"
#include "Unit.h"
#include "Chat.h"
#include "Creature.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "MapMgr.h"
#include "World.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "Language.h"
#include <vector>
#include "AutoBalance.h"
#include "ScriptMgrMacros.h"
#include "Group.h"

#if AC_COMPILER == AC_COMPILER_GNU
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using namespace Acore::ChatCommands;

ABScriptMgr* ABScriptMgr::instance()
{
    static ABScriptMgr instance;
    return &instance;
}

bool ABScriptMgr::OnBeforeModifyAttributes(Creature *creature, uint32 & instancePlayerCount)
{
    auto ret = IsValidBoolScript<ABModuleScript>([&](ABModuleScript* script)
    {
        return !script->OnBeforeModifyAttributes(creature, instancePlayerCount);
    });

    if (ret && *ret)
    {
        return false;
    }

    return true;
}

bool ABScriptMgr::OnAfterDefaultMultiplier(Creature *creature, float& defaultMultiplier)
{
    auto ret = IsValidBoolScript<ABModuleScript>([&](ABModuleScript* script)
    {
        return !script->OnAfterDefaultMultiplier(creature, defaultMultiplier);
    });

    if (ret && *ret)
    {
        return false;
    }

    return true;
}

bool ABScriptMgr::OnBeforeUpdateStats(Creature* creature, uint32& scaledHealth, uint32& scaledMana, float& damageMultiplier, uint32& newBaseArmor)
{
    auto ret = IsValidBoolScript<ABModuleScript>([&](ABModuleScript* script)
    {
        return !script->OnBeforeUpdateStats(creature, scaledHealth, scaledMana, damageMultiplier, newBaseArmor);
    });

    if (ret && *ret)
    {
        return false;
    }

    return true;
}

ABModuleScript::ABModuleScript(const char* name)
    : ModuleScript(name)
{
    ScriptRegistry<ABModuleScript>::AddScript(this);
}


class AutoBalanceCreatureInfo : public DataMap::Base
{
public:
    AutoBalanceCreatureInfo() {}
    AutoBalanceCreatureInfo(uint32 count, float dmg, float hpRate, float manaRate, float armorRate, uint8 selLevel) :
    instancePlayerCount(count),selectedLevel(selLevel), DamageMultiplier(dmg),HealthMultiplier(hpRate),ManaMultiplier(manaRate),ArmorMultiplier(armorRate) {}
    uint32 instancePlayerCount = 0;
    uint8 selectedLevel = 0;
    // this is used to detect creatures that update their entry
    uint32 entry = 0;
    float DamageMultiplier = 1;
    float HealthMultiplier = 1;
    float ManaMultiplier = 1;
    float ArmorMultiplier = 1;
};

class AutoBalanceMapInfo : public DataMap::Base
{
public:
    AutoBalanceMapInfo() {}
    AutoBalanceMapInfo(uint32 count, uint8 selLevel) : playerCount(count),mapLevel(selLevel) {}
    uint32 playerCount = 0;
    uint8 mapLevel = 0;
};

// The map values correspond with the .AutoBalance.XX.Name entries in the configuration file.
static std::map<int, int> forcedCreatureIds;
static std::map<uint32, uint8> enabledDungeonIds;
static std::map<uint32, float> dungeonOverrides;
static std::map<uint32, float> bossOverrides;
// cheaphack for difficulty server-wide.
// Another value TODO in player class for the party leader's value to determine dungeon difficulty.
static int8 PlayerCountDifficultyOffset, LevelScaling, higherOffset, lowerOffset;
static uint32 rewardRaid, rewardDungeon, MinPlayerReward;
static bool enabled, LevelEndGameBoost, DungeonsOnly, PlayerChangeNotify, LevelUseDb, rewardEnabled, DungeonScaleDownXP, DungeonScaleDownMoney;
static float globalRate,healthMultiplier, manaMultiplier, armorMultiplier, damageMultiplier, healthMultiplierAK, manaMultiplierAK, armorMultiplierAK, damageMultiplierAK, healthMultiplierAZ, manaMultiplierAZ, armorMultiplierAZ, damageMultiplierAZ,
healthMultiplierDK, manaMultiplierDK, armorMultiplierDK, damageMultiplierDK, healthMultiplierFS, manaMultiplierFS, armorMultiplierFS, damageMultiplierFS, healthMultiplierHR, manaMultiplierHR, armorMultiplierHR, damageMultiplierHR,
healthMultiplierPS, manaMultiplierPS, armorMultiplierPS, damageMultiplierPS, healthMultiplierGK, manaMultiplierGK, armorMultiplierGK, damageMultiplierGK, healthMultiplierNS, manaMultiplierNS, armorMultiplierNS, damageMultiplierNS, 
healthMultiplierOS, manaMultiplierOS, armorMultiplierOS, damageMultiplierOS, healthMultiplierUK, manaMultiplierUK, armorMultiplierUK, damageMultiplierUK, healthMultiplierUP, manaMultiplierUP, armorMultiplierUP, damageMultiplierUP, 
healthMultiplierVH, manaMultiplierVH, armorMultiplierVH, damageMultiplierVH, healthMultiplierHL, manaMultiplierHL, armorMultiplierHL, damageMultiplierHL, healthMultiplierHS, manaMultiplierHS, armorMultiplierHS, damageMultiplierHS, MinHPModifier, MinManaModifier, MinDamageModifier,
InflectionPoint, InflectionPointRaid, InflectionPointRaid10M, InflectionPointRaid25M, InflectionPointHeroic, InflectionPointRaidHeroic, InflectionPointRaid10MHeroic, InflectionPointRaid25MHeroic, BossInflectionMult;

int GetValidDebugLevel()
{
    int debugLevel = sConfigMgr->GetOption<uint32>("AutoBalance.DebugLevel", 2);

    if ((debugLevel < 0) || (debugLevel > 3))
        {
        return 1;
        }
    return debugLevel;
}

void LoadEnabledDungeons(std::string dungeonIdString) // Used for reading the string from the configuration file for selecting dungeons to scale
{
    std::string delimitedValue;
    std::stringstream dungeonIdStream;

    dungeonIdStream.str(dungeonIdString);
    while (std::getline(dungeonIdStream, delimitedValue, ',')) // Process each dungeon ID in the string, delimited by the comma - "," and then space " "
    {
        std::string pairOne, pairTwo;
        std::stringstream dungeonPairStream(delimitedValue);
        dungeonPairStream>>pairOne>>pairTwo;
        auto dungeonMapId = atoi(pairOne.c_str());
        auto minPlayers = atoi(pairTwo.c_str());
        enabledDungeonIds[dungeonMapId] = minPlayers;
    }
}

void LoadDungeonOverrides(std::string dungeonIdString) // Used for reading the string from the configuration file for selecting dungeons to override
{
    std::string delimitedValue;
    std::stringstream dungeonIdStream;

    dungeonIdStream.str(dungeonIdString);
    while (std::getline(dungeonIdStream, delimitedValue, ',')) // Process each dungeon ID in the string, delimited by the comma - "," and then space " "
    {
        std::string pairOne, pairTwo;
        std::stringstream dungeonPairStream(delimitedValue);
        dungeonPairStream>>pairOne>>pairTwo;
        auto dungeonMapId = atoi(pairOne.c_str());
        auto dungeonInflection = atof(pairTwo.c_str());
        dungeonOverrides[dungeonMapId] = dungeonInflection;
    }
}

void LoadBossOverrides(std::string dungeonIdString) // Used for reading the string from the configuration file for selecting dungeons to override boss inflection
{
    std::string delimitedValue;
    std::stringstream dungeonIdStream;

    dungeonIdStream.str(dungeonIdString);
    while (std::getline(dungeonIdStream, delimitedValue, ',')) // Process each dungeon ID in the string, delimited by the comma - "," and then space " "
    {
        std::string pairOne, pairTwo;
        std::stringstream dungeonPairStream(delimitedValue);
        dungeonPairStream>>pairOne>>pairTwo;
        auto dungeonMapId = atoi(pairOne.c_str());
        auto bossInflection = atof(pairTwo.c_str());
        bossOverrides[dungeonMapId] = bossInflection;
    }
}


bool isEnabledDungeon(uint32 dungeonId)
{
    return (enabledDungeonIds.find(dungeonId) != enabledDungeonIds.end());
}

bool perDungeonScalingEnabled()
{
    return (!enabledDungeonIds.empty());
}

bool hasDungeonOverride(uint32 dungeonId)
{
    return (dungeonOverrides.find(dungeonId) != dungeonOverrides.end());
}

bool hasBossOverride(uint32 dungeonId)
{
    return (bossOverrides.find(dungeonId) != bossOverrides.end());
}

void LoadForcedCreatureIdsFromString(std::string creatureIds, int forcedPlayerCount) // Used for reading the string from the configuration file to for those creatures who need to be scaled for XX number of players.
{
    std::string delimitedValue;
    std::stringstream creatureIdsStream;

    creatureIdsStream.str(creatureIds);
    while (std::getline(creatureIdsStream, delimitedValue, ',')) // Process each Creature ID in the string, delimited by the comma - ","
    {
        int creatureId = atoi(delimitedValue.c_str());
        if (creatureId >= 0)
        {
            forcedCreatureIds[creatureId] = forcedPlayerCount;
        }
    }
}

int GetForcedNumPlayers(int creatureId)
{
    if (forcedCreatureIds.find(creatureId) == forcedCreatureIds.end()) // Don't want the forcedCreatureIds map to blowup to a massive empty array
    {
        return -1;
    }
    return forcedCreatureIds[creatureId];
}


void getAreaLevel(Map *map, uint8 areaid, uint8 &min, uint8 &max) {
    LFGDungeonEntry const* dungeon = GetLFGDungeon(map->GetId(), map->GetDifficulty());
    if (dungeon && (map->IsDungeon() || map->IsRaid())) {
        min  = dungeon->minlevel;
        max  = dungeon->reclevel ? dungeon->reclevel : dungeon->maxlevel;
    }

    if (!min && !max)
    {
        AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(areaid);
        if (areaEntry && areaEntry->area_level > 0) {
            min = areaEntry->area_level;
            max = areaEntry->area_level;
        }
    }
}

class AutoBalance_WorldScript : public WorldScript
{
    public:
    AutoBalance_WorldScript()
        : WorldScript("AutoBalance_WorldScript")
    {
    }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        SetInitialWorldSettings();
    }
    void OnStartup() override
    {
    }

    void SetInitialWorldSettings()
    {
        forcedCreatureIds.clear();
        enabledDungeonIds.clear();
        dungeonOverrides.clear();
        bossOverrides.clear();
        LoadForcedCreatureIdsFromString(sConfigMgr->GetOption<std::string>("AutoBalance.ForcedID40", ""), 40);
        LoadForcedCreatureIdsFromString(sConfigMgr->GetOption<std::string>("AutoBalance.ForcedID25", ""), 25);
        LoadForcedCreatureIdsFromString(sConfigMgr->GetOption<std::string>("AutoBalance.ForcedID10", ""), 10);
        LoadForcedCreatureIdsFromString(sConfigMgr->GetOption<std::string>("AutoBalance.ForcedID5", ""), 5);
        LoadForcedCreatureIdsFromString(sConfigMgr->GetOption<std::string>("AutoBalance.ForcedID2", ""), 2);
        LoadForcedCreatureIdsFromString(sConfigMgr->GetOption<std::string>("AutoBalance.DisabledID", ""), 0);
        LoadEnabledDungeons(sConfigMgr->GetOption<std::string>("AutoBalance.PerDungeonPlayerCounts", ""));
        LoadDungeonOverrides(sConfigMgr->GetOption<std::string>("AutoBalance.PerDungeonScaling", ""));
        LoadBossOverrides(sConfigMgr->GetOption<std::string>("AutoBalance.PerDungeonBossScaling", ""));

        enabled = sConfigMgr->GetOption<bool>("AutoBalance.enable", 1);
        LevelEndGameBoost = sConfigMgr->GetOption<bool>("AutoBalance.LevelEndGameBoost", 1);
        DungeonsOnly = sConfigMgr->GetOption<bool>("AutoBalance.DungeonsOnly", 1);
        PlayerChangeNotify = sConfigMgr->GetOption<bool>("AutoBalance.PlayerChangeNotify", 1);
        LevelUseDb = sConfigMgr->GetOption<bool>("AutoBalance.levelUseDbValuesWhenExists", 1);
        rewardEnabled = sConfigMgr->GetOption<bool>("AutoBalance.reward.enable", 1);
        DungeonScaleDownXP = sConfigMgr->GetOption<bool>("AutoBalance.DungeonScaleDownXP", 0);
        DungeonScaleDownMoney = sConfigMgr->GetOption<bool>("AutoBalance.DungeonScaleDownMoney", 0);

        LevelScaling = sConfigMgr->GetOption<uint32>("AutoBalance.levelScaling", 1);
        PlayerCountDifficultyOffset = sConfigMgr->GetOption<uint32>("AutoBalance.playerCountDifficultyOffset", 0);
        higherOffset = sConfigMgr->GetOption<uint32>("AutoBalance.levelHigherOffset", 3);
        lowerOffset = sConfigMgr->GetOption<uint32>("AutoBalance.levelLowerOffset", 0);
        rewardRaid = sConfigMgr->GetOption<uint32>("AutoBalance.reward.raidToken", 49426);
        rewardDungeon = sConfigMgr->GetOption<uint32>("AutoBalance.reward.dungeonToken", 47241);
        MinPlayerReward = sConfigMgr->GetOption<float>("AutoBalance.reward.MinPlayerReward", 1);

        InflectionPoint = sConfigMgr->GetOption<float>("AutoBalance.InflectionPoint", 0.5f);
        InflectionPointRaid = sConfigMgr->GetOption<float>("AutoBalance.InflectionPointRaid", InflectionPoint);
        InflectionPointRaid25M = sConfigMgr->GetOption<float>("AutoBalance.InflectionPointRaid25M", InflectionPointRaid);
        InflectionPointRaid10M = sConfigMgr->GetOption<float>("AutoBalance.InflectionPointRaid10M", InflectionPointRaid);
        InflectionPointHeroic = sConfigMgr->GetOption<float>("AutoBalance.InflectionPointHeroic", InflectionPoint);
        InflectionPointRaidHeroic = sConfigMgr->GetOption<float>("AutoBalance.InflectionPointRaidHeroic", InflectionPointRaid);
        InflectionPointRaid25MHeroic = sConfigMgr->GetOption<float>("AutoBalance.InflectionPointRaid25MHeroic", InflectionPointRaid25M);
        InflectionPointRaid10MHeroic = sConfigMgr->GetOption<float>("AutoBalance.InflectionPointRaid10MHeroic", InflectionPointRaid10M);
        BossInflectionMult = sConfigMgr->GetOption<float>("AutoBalance.BossInflectionMult", 1.0f);
        globalRate = sConfigMgr->GetOption<float>("AutoBalance.rate.global", 1.0f);
        healthMultiplier = sConfigMgr->GetOption<float>("AutoBalance.rate.health", 1.0f);
        manaMultiplier = sConfigMgr->GetOption<float>("AutoBalance.rate.mana", 1.0f);
        armorMultiplier = sConfigMgr->GetOption<float>("AutoBalance.rate.armor", 1.0f);
        damageMultiplier = sConfigMgr->GetOption<float>("AutoBalance.rate.damage", 1.0f);
		MinHPModifier = sConfigMgr->GetOption<float>("AutoBalance.MinHPModifier", 0.1f);
        MinManaModifier = sConfigMgr->GetOption<float>("AutoBalance.MinManaModifier", 0.1f);
        MinDamageModifier = sConfigMgr->GetOption<float>("AutoBalance.MinDamageModifier", 0.1f);
		//Ahnkahet
        healthMultiplierAK = sConfigMgr->GetOption<float>("AutoBalance.rate.healthAK", 1.0f);
        manaMultiplierAK = sConfigMgr->GetOption<float>("AutoBalance.rate.manaAK", 1.0f);
        armorMultiplierAK = sConfigMgr->GetOption<float>("AutoBalance.rate.armorAK", 1.0f);
        damageMultiplierAK = sConfigMgr->GetOption<float>("AutoBalance.rate.damageAK", 1.0f);
		//AzjolNerub
        healthMultiplierAZ = sConfigMgr->GetOption<float>("AutoBalance.rate.healthAN", 1.0f);
        manaMultiplierAZ = sConfigMgr->GetOption<float>("AutoBalance.rate.manaAN", 1.0f);
        armorMultiplierAZ = sConfigMgr->GetOption<float>("AutoBalance.rate.armorAN", 1.0f);
        damageMultiplierAZ = sConfigMgr->GetOption<float>("AutoBalance.rate.damageAN", 1.0f);
		//DraktharonKeep		
        healthMultiplierDK = sConfigMgr->GetOption<float>("AutoBalance.rate.healthDK", 1.0f);
        manaMultiplierDK = sConfigMgr->GetOption<float>("AutoBalance.rate.manaDK", 1.0f);
        armorMultiplierDK = sConfigMgr->GetOption<float>("AutoBalance.rate.armorDK", 1.0f);
        damageMultiplierDK = sConfigMgr->GetOption<float>("AutoBalance.rate.damageDK", 1.0f);	
		//ForgeOfSouls
        healthMultiplierFS = sConfigMgr->GetOption<float>("AutoBalance.rate.healthFS", 1.0f);
        manaMultiplierFS = sConfigMgr->GetOption<float>("AutoBalance.rate.manaFS", 1.0f);
        armorMultiplierFS = sConfigMgr->GetOption<float>("AutoBalance.rate.armorFS", 1.0f);
        damageMultiplierFS = sConfigMgr->GetOption<float>("AutoBalance.rate.damageFS", 1.0f);
		//HallsOfReflection
        healthMultiplierHR = sConfigMgr->GetOption<float>("AutoBalance.rate.healthHR", 1.0f);
        manaMultiplierHR = sConfigMgr->GetOption<float>("AutoBalance.rate.manaHR", 1.0f);
        armorMultiplierHR = sConfigMgr->GetOption<float>("AutoBalance.rate.armorHR", 1.0f);
        damageMultiplierHR = sConfigMgr->GetOption<float>("AutoBalance.rate.damageHR", 1.0f);
		//PitOfSaron
        healthMultiplierPS = sConfigMgr->GetOption<float>("AutoBalance.rate.healthPS", 1.0f);
        manaMultiplierPS = sConfigMgr->GetOption<float>("AutoBalance.rate.manaPS", 1.0f);
        armorMultiplierPS = sConfigMgr->GetOption<float>("AutoBalance.rate.armorPS", 1.0f);
        damageMultiplierPS = sConfigMgr->GetOption<float>("AutoBalance.rate.damagePS", 1.0f);
		//Gundrak
        healthMultiplierGK = sConfigMgr->GetOption<float>("AutoBalance.rate.healthGK", 1.0f);
        manaMultiplierGK = sConfigMgr->GetOption<float>("AutoBalance.rate.manaGK", 1.0f);
        armorMultiplierGK = sConfigMgr->GetOption<float>("AutoBalance.rate.armorGK", 1.0f);
        damageMultiplierGK = sConfigMgr->GetOption<float>("AutoBalance.rate.damageGK", 1.0f);
		//Nexus
        healthMultiplierNS = sConfigMgr->GetOption<float>("AutoBalance.rate.healthNS", 1.0f);
        manaMultiplierNS = sConfigMgr->GetOption<float>("AutoBalance.rate.manaNS", 1.0f);
        armorMultiplierNS = sConfigMgr->GetOption<float>("AutoBalance.rate.armorNS", 1.0f);
        damageMultiplierNS = sConfigMgr->GetOption<float>("AutoBalance.rate.damageNS", 1.0f);
		//Oculus
        healthMultiplierOS = sConfigMgr->GetOption<float>("AutoBalance.rate.healthOS", 1.0f);
        manaMultiplierOS = sConfigMgr->GetOption<float>("AutoBalance.rate.manaOS", 1.0f);
        armorMultiplierOS = sConfigMgr->GetOption<float>("AutoBalance.rate.armorOS", 1.0f);
        damageMultiplierOS = sConfigMgr->GetOption<float>("AutoBalance.rate.damageOS", 1.0f);
		//UtgardeKeep
        healthMultiplierUK = sConfigMgr->GetOption<float>("AutoBalance.rate.healthUK", 1.0f);
        manaMultiplierUK = sConfigMgr->GetOption<float>("AutoBalance.rate.manaUK", 1.0f);
        armorMultiplierUK = sConfigMgr->GetOption<float>("AutoBalance.rate.armorUK", 1.0f);
        damageMultiplierUK = sConfigMgr->GetOption<float>("AutoBalance.rate.damageUK", 1.0f);
		//UtgardePinnacle
        healthMultiplierUP = sConfigMgr->GetOption<float>("AutoBalance.rate.healthUP", 1.0f);
        manaMultiplierUP = sConfigMgr->GetOption<float>("AutoBalance.rate.manaUP", 1.0f);
        armorMultiplierUP = sConfigMgr->GetOption<float>("AutoBalance.rate.armorUP", 1.0f);
        damageMultiplierUP = sConfigMgr->GetOption<float>("AutoBalance.rate.damageUP", 1.0f);
		//VioletHold
        healthMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.healthVH", 1.0f);
        manaMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.manaVH", 1.0f);
        armorMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.armorVH", 1.0f);
        damageMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.damageVH", 1.0f);	
		//HallsOfLightning
        healthMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.healthHL", 1.0f);
        manaMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.manaHL", 1.0f);
        armorMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.armorHL", 1.0f);
        damageMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.damageHL", 1.0f);	
		//HallsOfStone
        healthMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.healthHS", 1.0f);
        manaMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.manaHS", 1.0f);
        armorMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.armorHS", 1.0f);
        damageMultiplierVH = sConfigMgr->GetOption<float>("AutoBalance.rate.damageHS", 1.0f);			
    }
};

class AutoBalance_PlayerScript : public PlayerScript
{
    public:
        AutoBalance_PlayerScript()
            : PlayerScript("AutoBalance_PlayerScript")
        {
        }

        void OnLogin(Player *Player) override
        {
            if ((sConfigMgr->GetOption<bool>("AutoBalanceAnnounce.enable", true)) && (sConfigMgr->GetOption<bool>("AutoBalance.enable", true))) {
                ChatHandler(Player->GetSession()).SendSysMessage("This server is running the |cff4CFF00AutoBalance |rmodule.");
            }
        }

        virtual void OnLevelChanged(Player* player, uint8 /*oldlevel*/) override
        {
            if (!enabled || !player)
                return;

            if (LevelScaling == 0)
                return;

            AutoBalanceMapInfo *mapABInfo=player->GetMap()->CustomData.GetDefault<AutoBalanceMapInfo>("AutoBalanceMapInfo");

            if (mapABInfo->mapLevel < player->getLevel())
                mapABInfo->mapLevel = player->getLevel();
        }

        void OnGiveXP(Player* player, uint32& amount, Unit* victim) override
        {
            if (victim && DungeonScaleDownXP)
            {
                Map* map = player->GetMap();

                if (map->IsDungeon())
                {
                    // Ensure that the players always get the same XP, even when entering the dungeon alone
                    auto maxPlayerCount = ((InstanceMap*)sMapMgr->FindMap(map->GetId(), map->GetInstanceId()))->GetMaxPlayers();
                    auto currentPlayerCount = map->GetPlayersCountExceptGMs();
                    amount *= (float)currentPlayerCount / maxPlayerCount;
                }
            }
        }

        void OnBeforeLootMoney(Player* player, Loot* loot) override
        {
            if (DungeonScaleDownMoney)
            {
                Map* map = player->GetMap();

                if (map->IsDungeon())
                {
                    // Ensure that the players always get the same money, even when entering the dungeon alone
                    auto maxPlayerCount = ((InstanceMap*)sMapMgr->FindMap(map->GetId(), map->GetInstanceId()))->GetMaxPlayers();
                    auto currentPlayerCount = map->GetPlayersCountExceptGMs();
                    loot->gold = uint32(loot->gold * ((float)currentPlayerCount / (float)maxPlayerCount));
                }
            }

        }
};

class AutoBalance_UnitScript : public UnitScript
{
    public:
    AutoBalance_UnitScript()
        : UnitScript("AutoBalance_UnitScript", true)
    {
    }

    uint32 DealDamage(Unit* AttackerUnit, Unit *playerVictim, uint32 damage, DamageEffectType /*damagetype*/) override
    {
        return _Modifer_DealDamage(playerVictim, AttackerUnit, damage);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* /*spellInfo*/) override
    {
        damage = _Modifer_DealDamage(target, attacker, damage);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        damage = _Modifer_DealDamage(target, attacker, damage);
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        damage = _Modifer_DealDamage(target, attacker, damage);
    }

    void ModifyHealReceived(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* /*spellInfo*/) override {
        damage = _Modifer_DealDamage(target, attacker, damage);
    }


    uint32 _Modifer_DealDamage(Unit* target, Unit* attacker, uint32 damage)
    {
        if (!enabled)
            return damage;

        if (!attacker || attacker->GetTypeId() == TYPEID_PLAYER || !attacker->IsInWorld())
            return damage;

        float damageMultiplier = attacker->CustomData.GetDefault<AutoBalanceCreatureInfo>("AutoBalanceCreatureInfo")->DamageMultiplier;

        if (damageMultiplier == 1)
            return damage;

        if (!(!DungeonsOnly
                || (target->GetMap()->IsDungeon() && attacker->GetMap()->IsDungeon()) || (attacker->GetMap()->IsBattleground()
                     && target->GetMap()->IsBattleground())))
            return damage;


        if ((attacker->IsHunterPet() || attacker->IsPet() || attacker->IsSummon()) && attacker->IsControlledByPlayer())
            return damage;

        return damage * damageMultiplier;
    }
};


class AutoBalance_AllMapScript : public AllMapScript
{
    public:
    AutoBalance_AllMapScript()
        : AllMapScript("AutoBalance_AllMapScript")
        {
        }

        void OnPlayerEnterAll(Map* map, Player* player)
        {
            if (!enabled)
                return;

            if (player->IsGameMaster())
                return;

            AutoBalanceMapInfo *mapABInfo=map->CustomData.GetDefault<AutoBalanceMapInfo>("AutoBalanceMapInfo");

            // always check level, even if not conf enabled
            // because we can enable at runtime and we need this information
            if (player) {
                if (player->getLevel() > mapABInfo->mapLevel)
                    mapABInfo->mapLevel = player->getLevel();
            } else {
                Map::PlayerList const &playerList = map->GetPlayers();
                if (!playerList.IsEmpty())
                {
                    for (Map::PlayerList::const_iterator playerIteration = playerList.begin(); playerIteration != playerList.end(); ++playerIteration)
                    {
                        if (Player* playerHandle = playerIteration->GetSource())
                        {
                            if (!playerHandle->IsGameMaster() && playerHandle->getLevel() > mapABInfo->mapLevel)
                                mapABInfo->mapLevel = playerHandle->getLevel();
                        }
                    }
                }
            }

            //mapABInfo->playerCount++; //(maybe we've to found a safe solution to avoid player recount each time)
            mapABInfo->playerCount = map->GetPlayersCountExceptGMs();

            if (PlayerChangeNotify)
            {
                if (map->GetEntry()->IsDungeon() && player)
                {
                    Map::PlayerList const &playerList = map->GetPlayers();
                    if (!playerList.IsEmpty())
                    {
                        for (Map::PlayerList::const_iterator playerIteration = playerList.begin(); playerIteration != playerList.end(); ++playerIteration)
                        {
                            if (Player* playerHandle = playerIteration->GetSource())
                            {
                                ChatHandler chatHandle = ChatHandler(playerHandle->GetSession());
                                chatHandle.PSendSysMessage("|cffFF0000 [AutoBalance]|r|cffFF8000 %s entered the Instance %s. Auto setting player count to %u (Player Difficulty Offset = %u) |r", player->GetName().c_str(), map->GetMapName(), mapABInfo->playerCount + PlayerCountDifficultyOffset, PlayerCountDifficultyOffset);
                            }
                        }
                    }
                }
            }
        }

        void OnPlayerLeaveAll(Map* map, Player* player)
        {
            if (!enabled)
                return;

            if (player->IsGameMaster())
                return;

            AutoBalanceMapInfo *mapABInfo=map->CustomData.GetDefault<AutoBalanceMapInfo>("AutoBalanceMapInfo");

            //mapABInfo->playerCount--;// (maybe we've to found a safe solution to avoid player recount each time)
            // mapABInfo->playerCount = map->GetPlayersCountExceptGMs();
            //pklloveyou天鹿:
            if (map->GetEntry() && map->GetEntry()->IsDungeon())
            {
                bool AutoBalanceCheck = false;
                Map::PlayerList const& pl = map->GetPlayers();
                for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
                {
                    if (Player* plr = itr->GetSource())
                    {
                        if (plr->IsInCombat())
                            AutoBalanceCheck = true;
                    }
                }
                //pklloveyou天鹿:
                if (AutoBalanceCheck)
                {
                    for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
                    {
                        if (Player* plr = itr->GetSource())
                        {
                            plr->GetSession()->SendNotification("|cff4cff00%s|rAccess can be unlocked during non-combat", map->GetMapName());
                            plr->GetSession()->SendNotification("|cffffffff[%s]|rThe player left during the battle|cff4cff00%s|rInstance elastic lock", player->GetName().c_str(), map->GetMapName());
                        }
                    }
                }
                else
                {
                    //mapABInfo->playerCount--;
                    mapABInfo->playerCount = map->GetPlayersCountExceptGMs() - 1;
                }
            }

            // always check level, even if not conf enabled
            // because we can enable at runtime and we need this information
            if (!mapABInfo->playerCount) {
                mapABInfo->mapLevel = 0;
                return;
            }

            if (PlayerChangeNotify)
            {
                if (map->GetEntry()->IsDungeon() && player)
                {
                    Map::PlayerList const &playerList = map->GetPlayers();
                    if (!playerList.IsEmpty())
                    {
                        for (Map::PlayerList::const_iterator playerIteration = playerList.begin(); playerIteration != playerList.end(); ++playerIteration)
                        {
                            if (Player* playerHandle = playerIteration->GetSource())
                            {
                                ChatHandler chatHandle = ChatHandler(playerHandle->GetSession());
                                chatHandle.PSendSysMessage("|cffFF0000 [-AutoBalance]|r|cffFF8000 %s left the Instance %s. Auto setting player count to %u (Player Difficulty Offset = %u) |r", player->GetName().c_str(), map->GetMapName(), mapABInfo->playerCount, PlayerCountDifficultyOffset);
                            }
                        }
                    }
                }
            }
        }
};

class AutoBalance_AllCreatureScript : public AllCreatureScript
{
public:
    AutoBalance_AllCreatureScript()
        : AllCreatureScript("AutoBalance_AllCreatureScript")
    {
    }


    void Creature_SelectLevel(const CreatureTemplate* /*creatureTemplate*/, Creature* creature) override
    {
        if (!enabled)
            return;

        ModifyCreatureAttributes(creature, true);
    }

    void OnAllCreatureUpdate(Creature* creature, uint32 /*diff*/) override
    {
        if (!enabled)
            return;

        ModifyCreatureAttributes(creature);
    }

    bool checkLevelOffset(uint8 selectedLevel, uint8 targetLevel) {
        return selectedLevel && ((targetLevel >= selectedLevel && targetLevel <= (selectedLevel + higherOffset) ) || (targetLevel <= selectedLevel && targetLevel >= (selectedLevel - lowerOffset)));
    }

    void ModifyCreatureAttributes(Creature* creature, bool resetSelLevel = false)
    {
        if (!creature || !creature->GetMap())
            return;

        if (!creature->GetMap()->IsDungeon() && !creature->GetMap()->IsBattleground() && DungeonsOnly)
            return;

        if (((creature->IsHunterPet() || creature->IsPet() || creature->IsSummon()) && creature->IsControlledByPlayer()))
        {
            return;
        }

        AutoBalanceMapInfo *mapABInfo=creature->GetMap()->CustomData.GetDefault<AutoBalanceMapInfo>("AutoBalanceMapInfo");
        if (!mapABInfo->mapLevel)
            return;

        CreatureTemplate const *creatureTemplate = creature->GetCreatureTemplate();

        InstanceMap* instanceMap = ((InstanceMap*)sMapMgr->FindMap(creature->GetMapId(), creature->GetInstanceId()));
        uint32 mapId = instanceMap->GetEntry()->MapID;
        if (perDungeonScalingEnabled() && !isEnabledDungeon(mapId))
        {
            return;
        }
        uint32 maxNumberOfPlayers = instanceMap->GetMaxPlayers();
        int forcedNumPlayers = GetForcedNumPlayers(creatureTemplate->Entry);

        if (forcedNumPlayers > 0)
            maxNumberOfPlayers = forcedNumPlayers; // Force maxNumberOfPlayers to be changed to match the Configuration entries ForcedID2, ForcedID5, ForcedID10, ForcedID20, ForcedID25, ForcedID40
        else if (forcedNumPlayers == 0)
            return; // forcedNumPlayers 0 means that the creature is contained in DisabledID -> no scaling

        AutoBalanceCreatureInfo *creatureABInfo=creature->CustomData.GetDefault<AutoBalanceCreatureInfo>("AutoBalanceCreatureInfo");

        // force resetting selected level.
        // this is also a "workaround" to fix bug of not recalculated
        // attributes when UpdateEntry has been used.
        // TODO: It's better and faster to implement a core hook
        // in that position and force a recalculation then
        if ((creatureABInfo->entry != 0 && creatureABInfo->entry != creature->GetEntry()) || resetSelLevel) {
            creatureABInfo->selectedLevel = 0; // force a recalculation
        }

        if (!creature->IsAlive())
            return;

        uint32 curCount=mapABInfo->playerCount + PlayerCountDifficultyOffset;
        if (perDungeonScalingEnabled())
        {
            curCount = adjustCurCount(curCount, mapId);
        }

        uint8 bonusLevel = creatureTemplate->rank == CREATURE_ELITE_WORLDBOSS ? 3 : 0;
        // already scaled
        if (creatureABInfo->selectedLevel > 0) {
            if (LevelScaling) {
                if (checkLevelOffset(mapABInfo->mapLevel + bonusLevel, creature->getLevel()) &&
                    checkLevelOffset(creatureABInfo->selectedLevel, creature->getLevel()) &&
                    creatureABInfo->instancePlayerCount == curCount) {
                    return;
                }
            } else if (creatureABInfo->instancePlayerCount == curCount) {
                    return;
            }
        }

        creatureABInfo->instancePlayerCount = curCount;

        if (!creatureABInfo->instancePlayerCount) // no players in map, do not modify attributes
            return;

        if (!sABScriptMgr->OnBeforeModifyAttributes(creature, creatureABInfo->instancePlayerCount))
            return;

        uint8 originalLevel = creatureTemplate->maxlevel;

        uint8 level = mapABInfo->mapLevel;

        uint8 areaMinLvl, areaMaxLvl;
        getAreaLevel(creature->GetMap(), creature->GetAreaId(), areaMinLvl, areaMaxLvl);

        // avoid level changing for critters and special creatures (spell summons etc.) in instances
        bool skipLevel=false;
        if (originalLevel <= 1 && areaMinLvl >= 5)
            skipLevel = true;

        if (LevelScaling && creature->GetMap()->IsDungeon() && !skipLevel && !checkLevelOffset(level, originalLevel)) {  // change level only whithin the offsets and when in dungeon/raid
            if (level != creatureABInfo->selectedLevel || creatureABInfo->selectedLevel != creature->getLevel()) {
                // keep bosses +3 level
                creatureABInfo->selectedLevel = level + bonusLevel;
                creature->SetLevel(creatureABInfo->selectedLevel);
            }
        } else {
            creatureABInfo->selectedLevel = creature->getLevel();
        }

        creatureABInfo->entry = creature->GetEntry();

        bool useDefStats = false;
        if (LevelUseDb && creature->getLevel() >= creatureTemplate->minlevel && creature->getLevel() <= creatureTemplate->maxlevel)
            useDefStats = true;

        CreatureBaseStats const* origCreatureStats = sObjectMgr->GetCreatureBaseStats(originalLevel, creatureTemplate->unit_class);
        CreatureBaseStats const* creatureStats = sObjectMgr->GetCreatureBaseStats(creatureABInfo->selectedLevel, creatureTemplate->unit_class);

        uint32 baseHealth = origCreatureStats->GenerateHealth(creatureTemplate);
        uint32 baseMana = origCreatureStats->GenerateMana(creatureTemplate);
        uint32 scaledHealth = 0;
        uint32 scaledMana = 0;

        // Note: InflectionPoint handle the number of players required to get 50% health.
        //       you'd adjust this to raise or lower the hp modifier for per additional player in a non-whole group.
        //
        //       diff modify the rate of percentage increase between
        //       number of players. Generally the closer to the value of 1 you have this
        //       the less gradual the rate will be. For example in a 5 man it would take 3
        //       total players to face a mob at full health.
        //
        //       The +1 and /2 values raise the TanH function to a positive range and make
        //       sure the modifier never goes above the value or 1.0 or below 0.
        //
        float defaultMultiplier = 1.0f;
        if (creatureABInfo->instancePlayerCount < maxNumberOfPlayers)
        {
            float inflectionValue  = (float)maxNumberOfPlayers;
            if (hasDungeonOverride(mapId))
            {
                inflectionValue *= dungeonOverrides[mapId];
            }
            else
            {
                if (instanceMap->IsHeroic())
                {
                    if (instanceMap->IsRaid())
                    {
                        switch (instanceMap->GetMaxPlayers())
                        {
                            case 10:
                                inflectionValue *= InflectionPointRaid10MHeroic;
                                break;
                            case 25:
                                inflectionValue *= InflectionPointRaid25MHeroic;
                                break;
                            default:
                                inflectionValue *= InflectionPointRaidHeroic;
                        }
                    }
                    else
                        inflectionValue *= InflectionPointHeroic;
                }
                else
                {
                    if (instanceMap->IsRaid())
                    {
                        switch (instanceMap->GetMaxPlayers())
                        {
                            case 10:
                                inflectionValue *= InflectionPointRaid10M;
                                break;
                            case 25:
                                inflectionValue *= InflectionPointRaid25M;
                                break;
                            default:
                                inflectionValue *= InflectionPointRaid;
                        }
                    }
                    else
                        inflectionValue *= InflectionPoint;
                }
            }
            if (creature->IsDungeonBoss()) {
                if (hasBossOverride(mapId))
                {
                    inflectionValue *= bossOverrides[mapId];
                }
                else
                {
                    inflectionValue *= BossInflectionMult;
                }
            }

            float diff = ((float)maxNumberOfPlayers/5)*1.5f;
            defaultMultiplier = (tanh(((float)creatureABInfo->instancePlayerCount - inflectionValue) / diff) + 1.0f) / 2.0f;
        }

        if (!sABScriptMgr->OnAfterDefaultMultiplier(creature, defaultMultiplier))
            return;
		if (instanceMap->GetEntry()->MapID == 604 || instanceMap->GetEntry()->MapID == 601 || instanceMap->GetEntry()->MapID == 576
		|| instanceMap->GetEntry()->MapID == 574 || instanceMap->GetEntry()->MapID == 619 || instanceMap->GetEntry()->MapID == 600
		|| instanceMap->GetEntry()->MapID == 575 || instanceMap->GetEntry()->MapID == 608 || instanceMap->GetEntry()->MapID == 599
		|| instanceMap->GetEntry()->MapID == 602 || instanceMap->GetEntry()->MapID == 632 || instanceMap->GetEntry()->MapID == 658
		|| instanceMap->GetEntry()->MapID == 668 || instanceMap->GetEntry()->MapID == 578)
		{
			if (instanceMap->GetEntry()->MapID == 619)//Ahn'kahet
				creatureABInfo->HealthMultiplier =   healthMultiplierAK * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 601)//Azjol Nerub
				creatureABInfo->HealthMultiplier =   healthMultiplierAZ * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 600)//DraktharonKeep
				creatureABInfo->HealthMultiplier =   healthMultiplierDK * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 632)//ForgeOfSouls
				creatureABInfo->HealthMultiplier =   healthMultiplierFS * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 668)//HallsOfReflection
				creatureABInfo->HealthMultiplier =   healthMultiplierHR * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 658)//PitOfSaron
				creatureABInfo->HealthMultiplier =   healthMultiplierPS * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 604)//Gundrak
				creatureABInfo->HealthMultiplier =   healthMultiplierGK * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 576)//Nexus
				creatureABInfo->HealthMultiplier =   healthMultiplierNS * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 578)//Oculus
				creatureABInfo->HealthMultiplier =   healthMultiplierOS * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 574)//UtgardeKeep
				creatureABInfo->HealthMultiplier =   healthMultiplierUK * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 575)//UtgardePinnacle
				creatureABInfo->HealthMultiplier =   healthMultiplierUP * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 608)//VioletHold
				creatureABInfo->HealthMultiplier =   healthMultiplierVH * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 602)//HallsOfLightning
				creatureABInfo->HealthMultiplier =   healthMultiplierHL * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 576)//HallsOfStone
				creatureABInfo->HealthMultiplier =   healthMultiplierHS * defaultMultiplier * globalRate;
		}
		else
			creatureABInfo->HealthMultiplier =   healthMultiplier * defaultMultiplier * globalRate;
		
		if (creature->GetEntry() == 36838 || creature->GetEntry() == 36839)
			creatureABInfo->HealthMultiplier =   5 * defaultMultiplier * globalRate;
		
        if (creatureABInfo->HealthMultiplier <= MinHPModifier)
        {
            creatureABInfo->HealthMultiplier = MinHPModifier;
        }

        float hpStatsRate  = 1.0f;
        if (!useDefStats && LevelScaling && !skipLevel) {
            float newBaseHealth = 0;
            if (level <= 60)
                newBaseHealth=creatureStats->BaseHealth[0];
            else if(level <= 70)
                newBaseHealth=creatureStats->BaseHealth[1];
            else {
                newBaseHealth=creatureStats->BaseHealth[2];
                // special increasing for end-game contents
                if (LevelEndGameBoost)
                    newBaseHealth *= creatureABInfo->selectedLevel >= 75 && originalLevel < 75 ? float(creatureABInfo->selectedLevel-70) * 0.3f : 1;
            }

            float newHealth =  newBaseHealth * creatureTemplate->ModHealth;

            // allows health to be different with creatures that originally
            // differentiate their health by different level instead of multiplier field.
            // expecially in dungeons. The health reduction decrease if original level is similar to the area max level
            if (originalLevel >= areaMinLvl && originalLevel < areaMaxLvl) {
                float reduction = newHealth / float(areaMaxLvl-areaMinLvl) * (float(areaMaxLvl-originalLevel)*0.3f); // never more than 30%
                if (reduction > 0 && reduction < newHealth)
                    newHealth -= reduction;
            }

            hpStatsRate = newHealth / float(baseHealth);
        }

        creatureABInfo->HealthMultiplier *= hpStatsRate;

        scaledHealth = round(((float) baseHealth * creatureABInfo->HealthMultiplier) + 1.0f);

        //Getting the list of Classes in this group - this will be used later on to determine what additional scaling will be required based on the ratio of tank/dps/healer
        //GetPlayerClassList(creature, playerClassList); // Update playerClassList with the list of all the participating Classes

        float manaStatsRate  = 1.0f;
        if (!useDefStats && LevelScaling && !skipLevel) {
            float newMana =  creatureStats->GenerateMana(creatureTemplate);
            manaStatsRate = newMana/float(baseMana);
        }

		if (instanceMap->GetEntry()->MapID == 604 || instanceMap->GetEntry()->MapID == 601 || instanceMap->GetEntry()->MapID == 576
		|| instanceMap->GetEntry()->MapID == 574 || instanceMap->GetEntry()->MapID == 619 || instanceMap->GetEntry()->MapID == 600
		|| instanceMap->GetEntry()->MapID == 575 || instanceMap->GetEntry()->MapID == 608 || instanceMap->GetEntry()->MapID == 599
		|| instanceMap->GetEntry()->MapID == 602 || instanceMap->GetEntry()->MapID == 632 || instanceMap->GetEntry()->MapID == 658
		|| instanceMap->GetEntry()->MapID == 668 || instanceMap->GetEntry()->MapID == 578)
		{
			if (instanceMap->GetEntry()->MapID == 619)//Ahn'kahet
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierAK * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 601)//Azjol Nerub
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierAZ * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 600)//DraktharonKeep
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierDK * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 632)//ForgeOfSouls
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierFS * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 668)//HallsOfReflection
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierHR * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 658)//PitOfSaron
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierPS * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 604)//Gundrak
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierGK * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 576)//Nexus
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierNS * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 578)//Oculus
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierOS * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 574)//UtgardeKeep
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierUK * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 575)//UtgardePinnacle
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierUP * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 608)//VioletHold
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierVH * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 602)//HallsOfLightning
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierHL * defaultMultiplier * globalRate;
			if (instanceMap->GetEntry()->MapID == 576)//HallsOfStone
				creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplierHS * defaultMultiplier * globalRate;
		}
		else
			creatureABInfo->ManaMultiplier =  manaStatsRate * manaMultiplier * defaultMultiplier * globalRate;

        if (creatureABInfo->ManaMultiplier <= MinManaModifier)
        {
            creatureABInfo->ManaMultiplier = MinManaModifier;
        }

        scaledMana = round(baseMana * creatureABInfo->ManaMultiplier);

		float damageMul;
		
		if (instanceMap->GetEntry()->MapID == 604 || instanceMap->GetEntry()->MapID == 601 || instanceMap->GetEntry()->MapID == 576
		|| instanceMap->GetEntry()->MapID == 574 || instanceMap->GetEntry()->MapID == 619 || instanceMap->GetEntry()->MapID == 600
		|| instanceMap->GetEntry()->MapID == 575 || instanceMap->GetEntry()->MapID == 608 || instanceMap->GetEntry()->MapID == 599
		|| instanceMap->GetEntry()->MapID == 602 || instanceMap->GetEntry()->MapID == 632 || instanceMap->GetEntry()->MapID == 658
		|| instanceMap->GetEntry()->MapID == 668 || instanceMap->GetEntry()->MapID == 578)
		{
			if (instanceMap->GetEntry()->MapID == 619)//Ahn'kahet
				damageMul = defaultMultiplier * globalRate * damageMultiplierAK;
			if (instanceMap->GetEntry()->MapID == 601)//Azjol Nerub
				damageMul = defaultMultiplier * globalRate * damageMultiplierAZ;
			if (instanceMap->GetEntry()->MapID == 600)//DraktharonKeep
				damageMul = defaultMultiplier * globalRate * damageMultiplierDK;
			if (instanceMap->GetEntry()->MapID == 632)//ForgeOfSouls
				damageMul = defaultMultiplier * globalRate * damageMultiplierFS;
			if (instanceMap->GetEntry()->MapID == 668)//HallsOfReflection
				damageMul = defaultMultiplier * globalRate * damageMultiplierHR;
			if (instanceMap->GetEntry()->MapID == 658)//PitOfSaron
				 damageMul = defaultMultiplier * globalRate * damageMultiplierPS;
			if (instanceMap->GetEntry()->MapID == 604)//Gundrak
				damageMul = defaultMultiplier * globalRate * damageMultiplierGK;
			if (instanceMap->GetEntry()->MapID == 576)//Nexus
				damageMul = defaultMultiplier * globalRate * damageMultiplierNS;
			if (instanceMap->GetEntry()->MapID == 578)//Oculus
				damageMul = defaultMultiplier * globalRate * damageMultiplierOS;
			if (instanceMap->GetEntry()->MapID == 574)//UtgardeKeep
				damageMul = defaultMultiplier * globalRate * damageMultiplierUK;
			if (instanceMap->GetEntry()->MapID == 575)//UtgardePinnacle
				damageMul = defaultMultiplier * globalRate * damageMultiplierUP;
			if (instanceMap->GetEntry()->MapID == 608)//VioletHold
				damageMul = defaultMultiplier * globalRate * damageMultiplierVH;
			if (instanceMap->GetEntry()->MapID == 602)//HallsOfLightning
				damageMul = defaultMultiplier * globalRate * damageMultiplierHL;
			if (instanceMap->GetEntry()->MapID == 576)//HallsOfStone
				damageMul = defaultMultiplier * globalRate * damageMultiplierHS;
		}
		else
			damageMul = defaultMultiplier * globalRate * damageMultiplier;

		if (creature->GetEntry() == 36838 || creature->GetEntry() == 36839)
			damageMul = defaultMultiplier * globalRate * 5;
		
        // Can not be less then Min_D_Mod
        if (damageMul <= MinDamageModifier)
        {
            damageMul = MinDamageModifier;
        }

        if (!useDefStats && LevelScaling && !skipLevel) {
            float origDmgBase = origCreatureStats->GenerateBaseDamage(creatureTemplate);
            float newDmgBase = 0;
            if (level <= 60)
                newDmgBase=creatureStats->BaseDamage[0];
            else if(level <= 70)
                newDmgBase=creatureStats->BaseDamage[1];
            else {
                newDmgBase=creatureStats->BaseDamage[2];
                // special increasing for end-game contents
                if (LevelEndGameBoost && !creature->GetMap()->IsRaid()) {
                    newDmgBase *= creatureABInfo->selectedLevel >= 75 && originalLevel < 75 ? float(creatureABInfo->selectedLevel-70) * 0.3f : 1;
                }
            }

            damageMul *= newDmgBase/origDmgBase;
        }

		if (instanceMap->GetEntry()->MapID == 604 || instanceMap->GetEntry()->MapID == 601 || instanceMap->GetEntry()->MapID == 576
		|| instanceMap->GetEntry()->MapID == 574 || instanceMap->GetEntry()->MapID == 619 || instanceMap->GetEntry()->MapID == 600
		|| instanceMap->GetEntry()->MapID == 575 || instanceMap->GetEntry()->MapID == 608 || instanceMap->GetEntry()->MapID == 599
		|| instanceMap->GetEntry()->MapID == 602 || instanceMap->GetEntry()->MapID == 632 || instanceMap->GetEntry()->MapID == 658
		|| instanceMap->GetEntry()->MapID == 668 || instanceMap->GetEntry()->MapID == 578)
		{
			if (instanceMap->GetEntry()->MapID == 619)//Ahn'kahet
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierAK;
			if (instanceMap->GetEntry()->MapID == 601)//Azjol Nerub
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierAZ;
			if (instanceMap->GetEntry()->MapID == 600)//DraktharonKeep
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierDK;
			if (instanceMap->GetEntry()->MapID == 632)//ForgeOfSouls
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierFS;
			if (instanceMap->GetEntry()->MapID == 668)//HallsOfReflection
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierHR;
			if (instanceMap->GetEntry()->MapID == 658)//PitOfSaron
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierPS;
			if (instanceMap->GetEntry()->MapID == 604)//Gundrak
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierGK;
			if (instanceMap->GetEntry()->MapID == 576)//Nexus
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierNS;
			if (instanceMap->GetEntry()->MapID == 578)//Oculus
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierOS;
			if (instanceMap->GetEntry()->MapID == 574)//UtgardeKeep
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierUK;
			if (instanceMap->GetEntry()->MapID == 575)//UtgardePinnacle
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierUP;
			if (instanceMap->GetEntry()->MapID == 608)//VioletHold
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierVH;
			if (instanceMap->GetEntry()->MapID == 602)//HallsOfLightning
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierHL;
			if (instanceMap->GetEntry()->MapID == 576)//HallsOfStone
				creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplierHS;
		}
		else
			 creatureABInfo->ArmorMultiplier = defaultMultiplier * globalRate * armorMultiplier;

        
        uint32 newBaseArmor= round(creatureABInfo->ArmorMultiplier * (useDefStats || !LevelScaling || skipLevel ? origCreatureStats->GenerateArmor(creatureTemplate) : creatureStats->GenerateArmor(creatureTemplate)));

        if (!sABScriptMgr->OnBeforeUpdateStats(creature, scaledHealth, scaledMana, damageMul, newBaseArmor))
            return;

        uint32 prevMaxHealth = creature->GetMaxHealth();
        uint32 prevMaxPower = creature->GetMaxPower(POWER_MANA);
        uint32 prevHealth = creature->GetHealth();
        uint32 prevPower = creature->GetPower(POWER_MANA);

        Powers pType= creature->getPowerType();

        creature->SetArmor(newBaseArmor);
        creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, (float)newBaseArmor);
        creature->SetCreateHealth(scaledHealth);
        creature->SetMaxHealth(scaledHealth);
        creature->ResetPlayerDamageReq();
        creature->SetCreateMana(scaledMana);
        creature->SetMaxPower(POWER_MANA, scaledMana);
        creature->SetModifierValue(UNIT_MOD_ENERGY, BASE_VALUE, (float)100.0f);
        creature->SetModifierValue(UNIT_MOD_RAGE, BASE_VALUE, (float)100.0f);
        creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, (float)scaledHealth);
        creature->SetModifierValue(UNIT_MOD_MANA, BASE_VALUE, (float)scaledMana);
        creatureABInfo->DamageMultiplier = damageMul;

        uint32 scaledCurHealth=prevHealth && prevMaxHealth ? float(scaledHealth)/float(prevMaxHealth)*float(prevHealth) : 0;
        uint32 scaledCurPower=prevPower && prevMaxPower  ? float(scaledMana)/float(prevMaxPower)*float(prevPower) : 0;

        creature->SetHealth(scaledCurHealth);
        if (pType == POWER_MANA)
            creature->SetPower(POWER_MANA, scaledCurPower);
        else
            creature->setPowerType(pType); // fix creatures with different power types

        creature->UpdateAllStats();
    }

private:
    uint32 adjustCurCount(uint32 inputCount, uint32 dungeonId)
    {
        uint8 minPlayers = enabledDungeonIds[dungeonId];
        return inputCount < minPlayers ? minPlayers : inputCount;
    }
};
class AutoBalance_CommandScript : public CommandScript
{
public:
    AutoBalance_CommandScript() : CommandScript("AutoBalance_CommandScript") { }

    std::vector<ChatCommand> GetCommands() const
    {
        static std::vector<ChatCommand> ABCommandTable =
        {
            { "setoffset",        SEC_GAMEMASTER,                        true, &HandleABSetOffsetCommand,                 "Sets the global Player Difficulty Offset for instances. Example: (You + offset(1) = 2 player difficulty)." },
            { "getoffset",        SEC_GAMEMASTER,                        true, &HandleABGetOffsetCommand,                 "Shows current global player offset value" },
            { "checkmap",         SEC_GAMEMASTER,                        true, &HandleABCheckMapCommand,                  "Run a check for current map/instance, it can help in case you're testing autobalance with GM." },
            { "mapstat",          SEC_GAMEMASTER,                        true, &HandleABMapStatsCommand,                  "Shows current autobalance information for this map-" },
            { "creaturestat",     SEC_GAMEMASTER,                        true, &HandleABCreatureStatsCommand,             "Shows current autobalance information for selected creature." },
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "autobalance",     SEC_GAMEMASTER,                            false, NULL,                      "", ABCommandTable },
        };
        return commandTable;
    }

    static bool HandleABSetOffsetCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage(".autobalance setoffset #");
            handler->PSendSysMessage("Sets the Player Difficulty Offset for instances. Example: (You + offset(1) = 2 player difficulty).");
            return false;
        }
        char* offset = strtok((char*)args, " ");
        int32 offseti = -1;

        if (offset)
        {
            offseti = (uint32)atoi(offset);
            handler->PSendSysMessage("Changing Player Difficulty Offset to %i.", offseti);
            PlayerCountDifficultyOffset = offseti;
            return true;
        }
        else
            handler->PSendSysMessage("Error changing Player Difficulty Offset! Please try again.");
        return false;
    }

    static bool HandleABGetOffsetCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->PSendSysMessage("Current Player Difficulty Offset = %i", PlayerCountDifficultyOffset);
        return true;
    }

    static bool HandleABCheckMapCommand(ChatHandler* handler, const char* args)
    {
        Player *pl = handler->getSelectedPlayer();

        if (!pl)
        {
            handler->SendSysMessage(LANG_SELECT_PLAYER_OR_PET);
            handler->SetSentErrorMessage(true);
            return false;
        }

        AutoBalanceMapInfo *mapABInfo=pl->GetMap()->CustomData.GetDefault<AutoBalanceMapInfo>("AutoBalanceMapInfo");

        mapABInfo->playerCount = pl->GetMap()->GetPlayersCountExceptGMs();

        Map::PlayerList const &playerList = pl->GetMap()->GetPlayers();
        uint8 level = 0;
        if (!playerList.IsEmpty())
        {
            for (Map::PlayerList::const_iterator playerIteration = playerList.begin(); playerIteration != playerList.end(); ++playerIteration)
            {
                if (Player* playerHandle = playerIteration->GetSource())
                {
                    if (playerHandle->getLevel() > level)
                        mapABInfo->mapLevel = level = playerHandle->getLevel();
                }
            }
        }

        HandleABMapStatsCommand(handler, args);

        return true;
    }

    static bool HandleABMapStatsCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player *pl = handler->getSelectedPlayer();

        if (!pl)
        {
            handler->SendSysMessage(LANG_SELECT_PLAYER_OR_PET);
            handler->SetSentErrorMessage(true);
            return false;
        }

        AutoBalanceMapInfo *mapABInfo=pl->GetMap()->CustomData.GetDefault<AutoBalanceMapInfo>("AutoBalanceMapInfo");

        handler->PSendSysMessage("Players on map: %u", mapABInfo->playerCount);
        handler->PSendSysMessage("Max level of players in this map: %u", mapABInfo->mapLevel);

        return true;
    }

    static bool HandleABCreatureStatsCommand(ChatHandler* handler, const char* /*args*/)
    {
        Creature* target = handler->getSelectedCreature();

        if (!target)
        {
            handler->SendSysMessage(LANG_SELECT_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        AutoBalanceCreatureInfo *creatureABInfo=target->CustomData.GetDefault<AutoBalanceCreatureInfo>("AutoBalanceCreatureInfo");

        handler->PSendSysMessage("Instance player Count: %u", creatureABInfo->instancePlayerCount);
        handler->PSendSysMessage("Selected level: %u", creatureABInfo->selectedLevel);
        handler->PSendSysMessage("Damage multiplier: %.6f", creatureABInfo->DamageMultiplier);
        handler->PSendSysMessage("Health multiplier: %.6f", creatureABInfo->HealthMultiplier);
        handler->PSendSysMessage("Mana multiplier: %.6f", creatureABInfo->ManaMultiplier);
        handler->PSendSysMessage("Armor multiplier: %.6f", creatureABInfo->ArmorMultiplier);

        return true;

    }
};

class AutoBalance_GlobalScript : public GlobalScript {
public:
    AutoBalance_GlobalScript() : GlobalScript("AutoBalance_GlobalScript") { }

    void OnAfterUpdateEncounterState(Map* map, EncounterCreditType type,  uint32 /*creditEntry*/, Unit* source, Difficulty /*difficulty_fixed*/, DungeonEncounterList const* /*encounters*/, uint32 /*dungeonCompleted*/, bool updated) override {
        //if (!dungeonCompleted)
        //    return;

        if (!rewardEnabled || !updated)
            return;

        if (map->GetPlayersCountExceptGMs() < MinPlayerReward)
            return;

        AutoBalanceMapInfo *mapABInfo=map->CustomData.GetDefault<AutoBalanceMapInfo>("AutoBalanceMapInfo");

        uint8 areaMinLvl, areaMaxLvl;
        getAreaLevel(map, source->GetAreaId(), areaMinLvl, areaMaxLvl);

        // skip if it's not a pre-wotlk dungeon/raid and if it's not scaled
        if (!LevelScaling || lowerOffset >= 10 || mapABInfo->mapLevel <= 70 || areaMinLvl > 70
            // skip when not in dungeon or not kill credit
            || type != ENCOUNTER_CREDIT_KILL_CREATURE || !map->IsDungeon())
            return;

        Map::PlayerList const &playerList = map->GetPlayers();

        if (playerList.IsEmpty())
            return;

        uint32 reward = map->IsRaid() ? rewardRaid : rewardDungeon;
        if (!reward)
            return;

        //instanceStart=0, endTime;
        uint8 difficulty = map->GetDifficulty();

        for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            if (!itr->GetSource() || itr->GetSource()->IsGameMaster() || itr->GetSource()->getLevel() < DEFAULT_MAX_LEVEL)
                continue;

            itr->GetSource()->AddItem(reward, 1 + difficulty); // difficulty boost
        }
    }
};



void AddAutoBalanceScripts()
{
    new AutoBalance_WorldScript();
    new AutoBalance_PlayerScript();
    new AutoBalance_UnitScript();
    new AutoBalance_AllCreatureScript();
    new AutoBalance_AllMapScript();
    new AutoBalance_CommandScript();
    new AutoBalance_GlobalScript();
}
