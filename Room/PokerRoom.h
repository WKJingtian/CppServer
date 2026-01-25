#pragma once
#include "Room.h"
#include "Game/HoldemPokerGame.h"
#include <unordered_map>
#include <functional>

class PokerRoom : public Room
{
public:
	void OnPlayerExit(std::shared_ptr<Player> player) override;
	RpcError OnRecvPlayerNetPack(std::shared_ptr<Player> player, NetPack& pack) override;
	virtual void OnRoomCreated(int id);
	void OnTick() override;

private:
	HoldemPokerGame _game{};
	std::unordered_map<int, std::shared_ptr<Player>> _playerById{};

	std::shared_ptr<Player> GetPlayerById(int playerId);
	void RegisterPlayer(std::shared_ptr<Player> player);
	void UnregisterPlayer(int playerId);

	void SendTableInfoTo(std::shared_ptr<Player> player);
	void BroadcastTableInfo();
	void BroadcastHandResult(const HandResult& result);

	void HandleSitDown(std::shared_ptr<Player> player, int seatIdx);
	void HandleBuyIn(std::shared_ptr<Player> player, int amount);
	void HandleStandUp(std::shared_ptr<Player> player);
	void HandleSetBlinds(std::shared_ptr<Player> player, int smallBlind, int bigBlind);
	void HandlePlayerAction(std::shared_ptr<Player> player, uint8_t action, int amount);

	void ReturnChipsToPlayer(std::shared_ptr<Player> player);
};
