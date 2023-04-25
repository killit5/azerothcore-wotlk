#ifndef REFORGING_H
#define REFORGING_H

#include "Player.h"
#include "Config.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "GameEventMgr.h"
#include "Define.h"
#include "Player.h"
#include "WorldSession.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Bag.h"
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <unordered_map>
#include <vector>
#include <string>

#define PRESETS // comment this line to disable preset feature totally

struct ReforgeData
{
    uint32 increase, decrease;
    int32 stat_value;
};

class Player;
class Item;
class Reforge
{
	public:
		static Reforge* instance();
		typedef std::unordered_map<uint32, ReforgeData> ReforgeMapType;
		ReforgeMapType reforgeMap; // reforgeMap[iGUID] = ReforgeData
		std::vector<Item*> GetItemList(const Player* player);
		void RemoveReforge(Player* player, uint32 itemguid, bool update);
		void SendReforgePackets(Player* player);
		void SendReforgePacket(Player* player, uint32 entry, uint32 lowguid, const ReforgeData* reforge);
};
#define sReforge Reforge::instance()
#endif