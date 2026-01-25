#include "pch.h"
#include "ChatRoom.h"
#include <algorithm>

void ChatRoom::OnPlayerExit(std::shared_ptr<Player> player)
{
	Room::OnPlayerExit(player);
	bool doRemoveRoom = false;
	{
		auto wLock = _lock.OnWrite();
		if (_roomExpired == false && GetPlayerCnt() == 0)
		{
			_roomExpired = true;
			doRemoveRoom = true;
		}
	}
	if (doRemoveRoom)
		RoomMgr::RemoveRoom(_roomId);
}
RpcError ChatRoom::OnRecvPlayerNetPack(std::shared_ptr<Player> player, NetPack& pack)
{
	switch (pack.MsgType())
	{
	case RpcEnum::rpc_server_send_text:
	{
		auto msg = pack.ReadString();
		std::erase(msg, '\0');
		int pid = player->GetID();
		bool inCache = false;
		{
			auto rLock = _lock.OnRead();
            inCache = _inRoomPlayerCache.contains(pid);
		}
		if (!inCache)
		{
			auto wLock = _lock.OnWrite();
			_pendingPlayerMessageCache[pid] = msg;
			return RpcError::SUCCESS;
		}
		BroadcastText(player, msg, true);
		return RpcError::SUCCESS;
	}
	default:
		return Room::OnRecvPlayerNetPack(player, pack);
	}
}

void ChatRoom::OnRoomCreated(int id)
{
	Room::OnRoomCreated(id);
    _type = RoomType::CHAT_ROOM;
}

void ChatRoom::OnTick()
{
    std::vector<std::shared_ptr<Player>> toAdd;
    std::vector<std::shared_ptr<Player>> toRemove;
    {
        auto rLock = _lock.OnRead();
        std::unordered_map<int, std::shared_ptr<Player>> currentMembers;
        for (const auto& p : _members)
        {
            if (p && !p->Expired())
                currentMembers[p->GetID()] = p;
        }
        for (auto member : currentMembers)
            if (!_inRoomPlayerCache.contains(member.first))
                toAdd.push_back(member.second);
        for (auto member : _inRoomPlayerCache)
            if (!currentMembers.contains(member.first))
                toRemove.push_back(member.second);
    }

    if (!toAdd.empty() || !toRemove.empty())
    {
        auto wLock = _lock.OnWrite();
        for (auto member : toRemove)
        {
            _inRoomPlayerCache.erase(member->GetID());
            _pendingPlayerMessageCache.erase(member->GetID());
        }
        for (auto member : toAdd)
            _inRoomPlayerCache[member->GetID()] = member;
    }
    
    for (auto p : toAdd)
    {
        if (!p)
            continue;
        BroadcastText(p, p->GetName() + " joined the room", false);
        std::string pendingMsg;
        {
            auto wLock = _lock.OnWrite();
            auto it = _pendingPlayerMessageCache.find(p->GetID());
            if (it != _pendingPlayerMessageCache.end())
            {
                pendingMsg = it->second;
                _pendingPlayerMessageCache.erase(it);
            }
        }
        if (!pendingMsg.empty())
            BroadcastText(p, pendingMsg, true);
    }
    
    for (auto p : toRemove)
    {
        if (!p)
            continue;
        BroadcastText(p, p->GetName() + " left the room", false);
    }
}

void ChatRoom::BroadcastText(std::shared_ptr<Player> sender, const std::string& msg, bool includeSpeakerName)
{
    if (!sender)
        return;
	//std::cout << sender->GetName() << " says: " << msg << std::endl;
    NetPack send{ RpcEnum::rpc_client_send_text };
    sender->GetInfo().WriteInfo(send);
    send.WriteString(msg);
    send.WriteInt8(includeSpeakerName ? 1 : 0);
    std::unordered_set<std::shared_ptr<Player>> membersCopy;
    {
        auto rLock = _lock.OnRead();
        membersCopy = _members;
    }
    for (const auto& p : membersCopy)
    {
        if (p && !p->Expired())
            p->Send(send);
    }
}