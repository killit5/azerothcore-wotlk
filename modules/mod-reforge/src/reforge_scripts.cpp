#include "Reforging.h"
#define sR  sReforge

enum Menus
{
    MAIN_MENU = 200, // stat_type_max
    SELECT_ITEM,
    SELECT_STAT_REDUCE,
    SELECT_STAT_INCREASE,
    SELECT_RESTORE,
    RESTORE,
    REFORGE,
};
static const ItemModType statTypes[] = { ITEM_MOD_SPIRIT, ITEM_MOD_DODGE_RATING, ITEM_MOD_PARRY_RATING, ITEM_MOD_HIT_RATING, ITEM_MOD_CRIT_RATING, ITEM_MOD_HASTE_RATING, ITEM_MOD_EXPERTISE_RATING };
static const uint8 stat_type_max = sizeof(statTypes) / sizeof(*statTypes);

static const char* GetStatName(uint32 ItemStatType, Player* player)
{
    switch (ItemStatType)
    {
        case ITEM_MOD_SPIRIT: return player->GetSession()->GetAcoreString(40000); break;
        case ITEM_MOD_DODGE_RATING: return player->GetSession()->GetAcoreString(40001); break;
        case ITEM_MOD_PARRY_RATING: return player->GetSession()->GetAcoreString(40002); break;
        case ITEM_MOD_HIT_RATING: return player->GetSession()->GetAcoreString(40003); break;
        case ITEM_MOD_CRIT_RATING: return player->GetSession()->GetAcoreString(40004); break;
        case ITEM_MOD_HASTE_RATING: return player->GetSession()->GetAcoreString(40005); break;
        case ITEM_MOD_EXPERTISE_RATING: return player->GetSession()->GetAcoreString(40006); break;
        default: return NULL;
    }
}

static const char* GetSlotName(uint8 slot, WorldSession* session)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: return session->GetAcoreString(40007);
        case EQUIPMENT_SLOT_NECK: return session->GetAcoreString(40008);
        case EQUIPMENT_SLOT_SHOULDERS: return session->GetAcoreString(40009);
        case EQUIPMENT_SLOT_BODY: return session->GetAcoreString(40010);
        case EQUIPMENT_SLOT_CHEST: return session->GetAcoreString(40011);
        case EQUIPMENT_SLOT_WAIST: return session->GetAcoreString(40012);
        case EQUIPMENT_SLOT_LEGS: return session->GetAcoreString(40013);
        case EQUIPMENT_SLOT_FEET: return session->GetAcoreString(40014);
        case EQUIPMENT_SLOT_WRISTS: return session->GetAcoreString(40015);
        case EQUIPMENT_SLOT_HANDS: return session->GetAcoreString(40016);
        case EQUIPMENT_SLOT_FINGER1: return session->GetAcoreString(40017);
        case EQUIPMENT_SLOT_FINGER2: return session->GetAcoreString(40018);
        case EQUIPMENT_SLOT_TRINKET1: return session->GetAcoreString(40019);
        case EQUIPMENT_SLOT_TRINKET2: return session->GetAcoreString(40020);
        case EQUIPMENT_SLOT_BACK: return session->GetAcoreString(40021);
        case EQUIPMENT_SLOT_MAINHAND: return session->GetAcoreString(40022);
        case EQUIPMENT_SLOT_OFFHAND: return session->GetAcoreString(40023);
        case EQUIPMENT_SLOT_TABARD: return session->GetAcoreString(40024);
        case EQUIPMENT_SLOT_RANGED: return session->GetAcoreString(40025);
        default: return NULL;
    }
}

static uint32 Melt(uint8 i, uint8 j)
{
    return (i << 8) + j;
}

static void Unmelt(uint32 melt, uint8& i, uint8& j)
{
    i = melt >> 8;
    j = melt & 0xFF;
}

static Item* GetEquippedItem(Player* player, uint32 guidlow)
{
    for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetGUID().GetCounter() == guidlow)
                return pItem;
    return NULL;
}

static bool IsReforgable(Item* invItem, Player* player)
{
    //if (!invItem->IsEquipped())
    //    return false;
    if (invItem->GetOwnerGUID() != player->GetGUID())
        return false;
    const ItemTemplate* pProto = invItem->GetTemplate();
    //if (pProto->ItemLevel < 200)
    //    return false;
    //if (pProto->Quality == ITEM_QUALITY_HEIRLOOM) // block heirlooms necessary?
    //    return false;
    if (!pProto->StatsCount || pProto->StatsCount >= MAX_ITEM_PROTO_STATS) // Mandatory! Do NOT remove or edit
        return false;
    if (!sR->reforgeMap.empty() && sR->reforgeMap.find(invItem->GetGUID().GetCounter()) != sR->reforgeMap.end()) // Mandatory! Do NOT remove or edit
        return false;
    for (uint32 i = 0; i < pProto->StatsCount; ++i)
    {
        if (!GetStatName(pProto->ItemStat[i].ItemStatType, player))
            continue;
        if (((int32)floorf((float)pProto->ItemStat[i].ItemStatValue * 0.4f)) > 1)
            return true;
    }

    return false;
}

static void UpdatePlayerReforgeStats(Item* invItem, Player* player, uint32 decrease, uint32 increase) // stat types
{
    const ItemTemplate* pProto = invItem->GetTemplate();

    int32 stat_diff = 0;
    for (uint32 i = 0; i < pProto->StatsCount; ++i)
    {
        if (pProto->ItemStat[i].ItemStatType == increase)
            return; // Should not have the increased stat already
        if (pProto->ItemStat[i].ItemStatType == decrease)
            stat_diff = (int32)floorf((float)pProto->ItemStat[i].ItemStatValue * 0.4f);
    }
    if (stat_diff <= 0)
        return; // Should have some kind of diff

    // Update player stats
    if (invItem->IsEquipped())
        player->_ApplyItemMods(invItem, invItem->GetSlot(), false);
    uint32 guidlow = invItem->GetGUID().GetCounter();
    ReforgeData& data = sR->reforgeMap[guidlow];
    data.increase = increase;
    data.decrease = decrease;
    data.stat_value = stat_diff;
    if (invItem->IsEquipped())
        player->_ApplyItemMods(invItem, invItem->GetSlot(), true);
    // CharacterDatabase.PExecute("REPLACE INTO `custom_reforging` (`GUID`, `increase`, `decrease`, `stat_value`) VALUES (%u, %u, %u, %i)", guidlow, increase, decrease, stat_diff);
    player->ModifyMoney(pProto->SellPrice < (10 * GOLD) ? (-10 * GOLD) : -(int32)pProto->SellPrice);
    sR->SendReforgePacket(player, invItem->GetEntry(), 0, &data);
    // player->SaveToDB();
}

class npc_reforger : public CreatureScript
{
public:
    npc_reforger() : CreatureScript("npc_reforger") { }
	
    struct Timed : public BasicEvent
    {
        // This timed event tries to fix modify money breaking gossip
        // This event closes the gossip menu and on the second player update tries to open the next menu
        Timed(Player* player, Creature* creature) : guid(creature->GetGUID()), player(player), triggered(false)
        {
            CloseGossipMenuFor(player);
            player->m_Events.AddEvent(this, player->m_Events.CalculateTime(1));
        }

        bool Execute(uint64, uint32) override
        {
            if (!triggered)
            {
                triggered = true;
                player->m_Events.AddEvent(this, player->m_Events.CalculateTime(1));
                return false;
            }
            //if (Creature* creature = player->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_GOSSIP))
               // OnGossipHello(player, creature);
            return true;
        }

        ObjectGuid guid;
        Player* player;
        bool triggered;
    };

    bool OnGossipHello(Player* player, Creature* creature)
    {
		AddGossipItemFor(player, GOSSIP_ICON_BATTLE, player->GetSession()->GetAcoreString(40036), 0, Melt(MAIN_MENU, 0));
		for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
		{
			if (Item* invItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
				if (IsReforgable(invItem, player))
					if (const char* slotname = GetSlotName(slot, player->GetSession()))
						AddGossipItemFor(player, GOSSIP_ICON_TRAINER, slotname, 0, Melt(SELECT_STAT_REDUCE, slot));
		}
		AddGossipItemFor(player, GOSSIP_ICON_TRAINER, player->GetSession()->GetAcoreString(40035), 0, Melt(SELECT_RESTORE, 0));
		AddGossipItemFor(player, GOSSIP_ICON_CHAT, player->GetSession()->GetAcoreString(40030), 0, Melt(MAIN_MENU, 0));
		SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
		return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 melt)
    {
		ClearGossipMenuFor(player);

		uint8 menu, action;
		Unmelt(melt, menu, action);

		switch (menu)
		{
			case MAIN_MENU: OnGossipHello(player, creature); break;
			case SELECT_STAT_REDUCE:
				// action = slot
				if (Item* invItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, action))
				{
					if (IsReforgable(invItem, player))
					{
						uint32 guidlow = invItem->GetGUID().GetCounter();
						const ItemTemplate* pProto = invItem->GetTemplate();
						AddGossipItemFor(player, GOSSIP_ICON_BATTLE, player->GetSession()->GetAcoreString(40034), sender, melt);
						for (uint32 i = 0; i < pProto->StatsCount; ++i)
						{
							int32 stat_diff = ((int32)floorf((float)pProto->ItemStat[i].ItemStatValue * 0.4f));
							if (stat_diff > 1)
								if (const char* stat_name = GetStatName(pProto->ItemStat[i].ItemStatType, player))
								{
									std::ostringstream oss;
									oss << stat_name << " (" << pProto->ItemStat[i].ItemStatValue << " |cFFDB2222-" << stat_diff << "|r)";
									AddGossipItemFor(player, GOSSIP_ICON_TRAINER, oss.str(), guidlow, Melt(SELECT_STAT_INCREASE, i));
								}
						}
						AddGossipItemFor(player, GOSSIP_ICON_TALK, player->GetSession()->GetAcoreString(40028), 0, Melt(MAIN_MENU, 0));
						SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
					}
					else
					{
						player->GetSession()->SendNotification(player->GetSession()->GetAcoreString(40026));
						OnGossipHello(player, creature);
					}
				}
				else
				{
					player->GetSession()->SendNotification(player->GetSession()->GetAcoreString(40026));
					OnGossipHello(player, creature);
				}
				break;
			case SELECT_STAT_INCREASE:
				// sender = item guidlow
				// action = StatsCount id
				{
					Item* invItem = GetEquippedItem(player, sender);
					if (invItem)
					{
						const ItemTemplate* pProto = invItem->GetTemplate();
						int32 stat_diff = ((int32)floorf((float)pProto->ItemStat[action].ItemStatValue * 0.4f));

						AddGossipItemFor(player, GOSSIP_ICON_BATTLE, player->GetSession()->GetAcoreString(40033), sender, melt);
						for (uint8 i = 0; i < stat_type_max; ++i)
						{
							bool cont = false;
							for (uint32 j = 0; j < pProto->StatsCount; ++j)
							{
								if (statTypes[i] == pProto->ItemStat[j].ItemStatType) // skip existing stats on item
								{
									cont = true;
									break;
								}
							}
							if (cont)
								continue;
							if (const char* stat_name = GetStatName(statTypes[i], player))
							{
								std::ostringstream oss;
								oss << stat_name << " |cFF3ECB3C+" << stat_diff << "|r";
								AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, oss.str(), sender, Melt(i, (uint8)pProto->ItemStat[action].ItemStatType), player->GetSession()->GetAcoreString(40032) + pProto->Name1, (pProto->SellPrice < (10 * GOLD) ? (10 * GOLD) : pProto->SellPrice), false);
							}
						}
						AddGossipItemFor(player, GOSSIP_ICON_TALK, player->GetSession()->GetAcoreString(40028), 0, Melt(SELECT_STAT_REDUCE, invItem->GetSlot()));
						SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
					}
					else
					{
						player->GetSession()->SendNotification(player->GetSession()->GetAcoreString(40026));
						OnGossipHello(player, creature);
					}
				}
				break;
			case SELECT_RESTORE:
				{
					AddGossipItemFor(player, GOSSIP_ICON_BATTLE, player->GetSession()->GetAcoreString(40027), sender, melt);
					if (!sR->reforgeMap.empty())
					{
						for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
						{
							if (Item* invItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
								if (sR->reforgeMap.find(invItem->GetGUID().GetCounter()) != sR->reforgeMap.end())
									if (const char* slotname = GetSlotName(slot, player->GetSession()))
										AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, slotname, invItem->GetGUID().GetCounter(), Melt(RESTORE, 0), player->GetSession()->GetAcoreString(40029) + invItem->GetTemplate()->Name1, 0, false);
						}
					}
					AddGossipItemFor(player, GOSSIP_ICON_CHAT, player->GetSession()->GetAcoreString(40030), sender, melt);
					AddGossipItemFor(player, GOSSIP_ICON_TALK, player->GetSession()->GetAcoreString(40028), 0, Melt(MAIN_MENU, 0));
					SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
				}
				break;
			case RESTORE:
				// sender = item guidlow
				{
					if (player->GetItemByGuid(ObjectGuid(HighGuid::Item, 0, sender)))
					{
						if (!sR->reforgeMap.empty() && sR->reforgeMap.find(sender) != sR->reforgeMap.end())
							sR->RemoveReforge(player, sender, true);
					}
					OnGossipHello(player, creature);
				}
				break;
			default: // Reforge
				// sender = item guidlow
				// menu = stat type to increase index to statTypes[]
				// action = stat type to decrease
				{
					if (menu < stat_type_max)
					{
						Item* invItem = GetEquippedItem(player, sender);
						if (invItem && IsReforgable(invItem, player))
						{
							if (player->HasEnoughMoney(invItem->GetTemplate()->SellPrice < (10 * GOLD) ? (10 * GOLD) : invItem->GetTemplate()->SellPrice))
							{
								// int32 stat_diff = ((int32)floorf((float)invItem->GetTemplate()->ItemStat[action].ItemStatValue * 0.4f));
								UpdatePlayerReforgeStats(invItem, player, action, statTypes[menu]); // rewrite this function
							}
							else
							{
								player->GetSession()->SendNotification(player->GetSession()->GetAcoreString(40031));
							}
						}
						else
						{
							player->GetSession()->SendNotification(player->GetSession()->GetAcoreString(40026));
						}
					}
					 OnGossipHello(player, creature);
					new Timed(player, creature);
				}
		}
		return true;
    }
};

class PS_Reforging : public PlayerScript
{
public:
    PS_Reforging() : PlayerScript("PS_Reforging") 
	{
		CharacterDatabase.Execute("DELETE FROM `custom_reforging` WHERE NOT EXISTS (SELECT 1 FROM `item_instance` WHERE `item_instance`.`guid` = `custom_reforging`.`GUID`)");		
	}

    class SendRefPackLogin : public BasicEvent
    {
    public:
        SendRefPackLogin(Player* _player) : player(_player)
        {
            _player->m_Events.AddEvent(this, _player->m_Events.CalculateTime(1000));
        }

        bool Execute(uint64, uint32) override
        {
            sR->SendReforgePackets(player);
            return true;
        }
        Player* player;
    };

	void OnAfterApplyItemBonuses(Player* player, ItemTemplate const* proto, uint8 slot, bool apply, bool only_level_scale /*= false*/)
	{
		if (slot >= INVENTORY_SLOT_BAG_END || !proto)
			return;
	    uint32 statcount = proto->StatsCount;
	    ReforgeData* reforgeData = NULL;
	    bool decreased = false;
	    if (statcount < MAX_ITEM_PROTO_STATS)
	    {
		    if (Item* invItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
		    {
			    if (sR->reforgeMap.find(invItem->GetGUID().GetCounter()) != sR->reforgeMap.end())
			    {
				    reforgeData = &sR->reforgeMap[invItem->GetGUID().GetCounter()];
				    ++statcount;
			    }
		    }
	    }	
		for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
		{
			uint32 statType = 0;
			int32  val = 0;
			int32  val_bis = 0;
			if (i >= statcount)
				continue;
			
			statType = proto->ItemStat[i].ItemStatType;
			val = proto->ItemStat[i].ItemStatValue;
			if (reforgeData)
			{
				if(i == statcount-1)
				{
					statType = reforgeData->increase;
					val = reforgeData->stat_value;
				}
				else if (!decreased && reforgeData->decrease == statType)
				{
					val_bis = val;
					val -= reforgeData->stat_value + val_bis;
					decreased = true;
				}
			}
			if (val == 0)
				continue;
			if ((reforgeData) && ((statType == reforgeData->increase) || (statType == reforgeData->decrease)))
			{
				switch (statType)
				{
					case ITEM_MOD_MANA:
						player->HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
						break;
					case ITEM_MOD_HEALTH:                           // modify HP
						player->HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
						break;
					case ITEM_MOD_AGILITY:                          // modify agility
						player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
						player->ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
						break;
					case ITEM_MOD_STRENGTH:                         //modify strength
						player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
						player->ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
						break;
					case ITEM_MOD_INTELLECT:                        //modify intellect
						player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
						player->ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
						break;
					case ITEM_MOD_SPIRIT:                           //modify spirit
						player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
						player->ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
						break;
					case ITEM_MOD_STAMINA:                          //modify stamina
						player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
						player->ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
						break;
					case ITEM_MOD_DEFENSE_SKILL_RATING:
						player->ApplyRatingMod(CR_DEFENSE_SKILL, int32(val), apply);
						break;
					case ITEM_MOD_DODGE_RATING:
						player->ApplyRatingMod(CR_DODGE, int32(val), apply);
						break;
					case ITEM_MOD_PARRY_RATING:
						player->ApplyRatingMod(CR_PARRY, int32(val), apply);
						break;
					case ITEM_MOD_BLOCK_RATING:
						player->ApplyRatingMod(CR_BLOCK, int32(val), apply);
						break;
					case ITEM_MOD_HIT_MELEE_RATING:
						player->ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
						break;
					case ITEM_MOD_HIT_RANGED_RATING:
						player->ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
						break;
					case ITEM_MOD_HIT_SPELL_RATING:
						player->ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_CRIT_MELEE_RATING:
						player->ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
						break;
					case ITEM_MOD_CRIT_RANGED_RATING:
						player->ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
						break;
					case ITEM_MOD_CRIT_SPELL_RATING:
						player->ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
						player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
						break;
					case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
						player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
						break;
					case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
						player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
						player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
						break;
					case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
						player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
						break;
					case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
						player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_HASTE_MELEE_RATING:
						player->ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
						break;
					case ITEM_MOD_HASTE_RANGED_RATING:
						player->ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
						break;
					case ITEM_MOD_HASTE_SPELL_RATING:
						player->ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_HIT_RATING:
						player->ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
						player->ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
						player->ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_CRIT_RATING:
						player->ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
						player->ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
						player->ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_HIT_TAKEN_RATING:
						player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
						player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
						player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_CRIT_TAKEN_RATING:
					case ITEM_MOD_RESILIENCE_RATING:
						player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
						player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
						player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_HASTE_RATING:
						player->ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
						player->ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
						player->ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
						break;
					case ITEM_MOD_EXPERTISE_RATING:
						player->ApplyRatingMod(CR_EXPERTISE, int32(val), apply);
						break;
					case ITEM_MOD_ATTACK_POWER:
						player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(val), apply);
						player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
						break;
					case ITEM_MOD_RANGED_ATTACK_POWER:
						player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
						break;
					//            case ITEM_MOD_FERAL_ATTACK_POWER:
					//                ApplyFeralAPBonus(int32(val), apply);
					//                break;
					case ITEM_MOD_MANA_REGENERATION:
						player->ApplyManaRegenBonus(int32(val), apply);
						break;
					case ITEM_MOD_ARMOR_PENETRATION_RATING:
						player->ApplyRatingMod(CR_ARMOR_PENETRATION, int32(val), apply);
						break;
					case ITEM_MOD_SPELL_POWER:
						player->ApplySpellPowerBonus(int32(val), apply);
						break;
					case ITEM_MOD_HEALTH_REGEN:
						player->ApplyHealthRegenBonus(int32(val), apply);
						break;
					case ITEM_MOD_SPELL_PENETRATION:
						player->ApplySpellPenetrationBonus(val, apply);
						break;
					case ITEM_MOD_BLOCK_VALUE:
						player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(val), apply);
						break;
					// deprecated item mods
					case ITEM_MOD_SPELL_HEALING_DONE:
					case ITEM_MOD_SPELL_DAMAGE_DONE:
						break;
				}
			}
		}
	}

    void OnLogin(Player* player)
    {
        uint32 playerGUID = player->GetGUID().GetCounter();
		
        QueryResult result = CharacterDatabase.Query("SELECT `GUID`, `increase`, `decrease`, `stat_value` FROM `custom_reforging` WHERE `Owner` = {}", playerGUID);
        if (result)
        {
            do
            {
                uint32 lowGUID = (*result)[0].Get<uint32>();
                Item* invItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, 0, lowGUID));
                if (invItem && invItem->IsEquipped())
                    player->_ApplyItemMods(invItem, invItem->GetSlot(), false);
                ReforgeData& data = sR->reforgeMap[lowGUID];
                data.increase = (*result)[1].Get<uint32>();
                data.decrease = (*result)[2].Get<uint32>();
                data.stat_value = (*result)[3].Get<int32>();
                if (invItem && invItem->IsEquipped())
                    player->_ApplyItemMods(invItem, invItem->GetSlot(), true);
				
                // SendReforgePacket(player, entry, lowGUID);
            } while (result->NextRow());

            // SendReforgePackets(player);
            new SendRefPackLogin(player);
        }
    }

    //void OnLogout(Player* player) override
    //{
    //    if (reforgeMap.empty())
    //        return;
    //    for (ReforgeMapType::const_iterator it = reforgeMap.begin(); it != reforgeMap.end();)
    //    {
    //        ReforgeMapType::const_iterator old_it = it++;
    //        RemoveReforge(player, old_it->first, false);
    //    }
    //}

    void OnSave(Player* player) 
    {
        uint32 lowguid = player->GetGUID().GetCounter();
        auto trans = CharacterDatabase.BeginTransaction();
        trans->Append("DELETE FROM `custom_reforging` WHERE `Owner` = {}", lowguid);

        if (!sR->reforgeMap.empty())
        {
            // Only save items that are in inventory / bank / etc
            std::vector<Item*> items = sR->GetItemList(player);
            for (std::vector<Item*>::const_iterator it = items.begin(); it != items.end(); ++it)
            {
                Reforge::ReforgeMapType::const_iterator it2 = sR->reforgeMap.find((*it)->GetGUID().GetCounter());
                if (it2 == sR->reforgeMap.end())
                    continue;

                const ReforgeData& data = it2->second;
				trans->Append("REPLACE INTO `custom_reforging` (`GUID`, `increase`, `decrease`, `stat_value`, `Owner`) VALUES ({}, {}, {}, {}, {})", it2->first, data.increase, data.decrease, data.stat_value, lowguid);               
			}
        }

        if (trans->GetSize()) // basically never false
            CharacterDatabase.CommitTransaction(trans);
    }
	
	void OnEquip(Player* player, Item* it, uint8 bag, uint8 slot, bool update)
	{    
			sR->SendReforgePackets(player);
	}
	
};

void AddReforgeScripts()
{
    new npc_reforger();
    new PS_Reforging();
}

