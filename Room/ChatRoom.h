#pragma once
# include "Room.h"
class ChatRoom : public Room
{
	RoomType _type = CHAT_ROOM;

	std::unordered_map<int, std::shared_ptr<Player>> _inRoomPlayerCache{};
	std::unordered_map<int, std::string> _pendingPlayerMessageCache{};

	void BroadcastText(std::shared_ptr<Player> sender, const std::string& msg, bool includeSpeakerName);

public:
	void OnPlayerExit(std::shared_ptr<Player> player) override;
	RpcError OnRecvPlayerNetPack(std::shared_ptr<Player> player, NetPack& pack) override;

	void OnTick() override;
};
