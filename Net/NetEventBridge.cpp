#include "pch.h"
#include "NetEventBridge.h"
#include "Net/INetEngine.h"
#include "Net/NetPack.h"
#include "Net/NetPackHandler.h"
#include "Player/PlayerMgr.h"

void NetEventBridge::Dispatch(const std::vector<NetEvent>& events, INetEngine* engine)
{
	for (const auto& ev : events)
	{
		if (ev.type == NetEventType::Connected)
		{
			if (!engine)
				continue;
			if (PlayerMgr::FindByConnId(ev.connId))
				continue;
			PlayerMgr::OnPlayerConnected(ev.connId, engine);
		}
		else if (ev.type == NetEventType::Disconnected)
		{
			PlayerMgr::OnPlayerDisconnected(ev.connId);
		}
		else if (ev.type == NetEventType::Frame)
		{
			auto player = PlayerMgr::FindByConnId(ev.connId);
			if (!player)
				continue;
			if (ev.data.size() < 4)
			{
				player->Delete();
				continue;
			}
			NetPack pack = NetPack(ev.data.data());
			if (pack.MsgType() == RpcEnum::INVALID || pack.Length() < 4)
			{
				player->Delete();
				continue;
			}
			NetPackHandler::AddTask(player, pack);
		}
	}
}
