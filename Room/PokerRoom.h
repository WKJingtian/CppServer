#pragma once
#include "Room.h"
#include "AI/HoldemBot.h"
#include "Game/HoldemHandResult.h"
#include "Game/HoldemPokerGame.h"
#include <unordered_map>
#include <functional>
#include <memory>

class PokerRoom : public Room
{
public:
	PokerRoom(uint32_t timerTickMs, uint32_t timerSlotCount);
	void OnPlayerExit(std::shared_ptr<Player> player) override;
	RpcError OnRecvPlayerNetPack(std::shared_ptr<Player> player, NetPack& pack) override;
	virtual void OnRoomCreated(int id);
	void OnRoomDestroy() override;
	void OnTick() override;

private:
	HoldemPokerGame _game{};
	std::unordered_map<int, std::shared_ptr<Player>> _playerById{};
	std::unordered_map<int, std::unique_ptr<HoldemBot>> _bots{};
	std::unordered_map<int, TimerWheel::TimerHandle> _botActionTimers{};
	uint64_t _lastSnapshotHash = 0;
	bool _hasSnapshotHash = false;

	std::shared_ptr<Player> GetPlayerById(int playerId);
	void RegisterPlayer(std::shared_ptr<Player> player);
	void UnregisterPlayer(int playerId);
	HoldemBot* GetBotById(int botId);

	void SendTableInfoTo(std::shared_ptr<Player> player);
	void BroadcastTableInfo();
	void BroadcastHandResult(const HandResult& result);
	void ScheduleBotActionIfNeeded(int actingPlayerId);
	void ExecuteBotAction(int botId);
	void CancelBotActionsExcept(int botId);
	void ClearBotsIfNoHumans();
	void RemoveBrokeBots();
	void RemoveBotById(int botId);
	void HandleAddBot(std::shared_ptr<Player> player);
	void HandleKickBot(int seatIdx);

	void HandleSitDown(std::shared_ptr<Player> player);
	void HandleBuyIn(std::shared_ptr<Player> player, int amount);
	void HandleStandUp(std::shared_ptr<Player> player);
	void HandleSetBlinds(std::shared_ptr<Player> player, int smallBlind, int bigBlind);
	void HandlePlayerAction(std::shared_ptr<Player> player, uint8_t action, int amount);

	void ReturnChipsToPlayer(std::shared_ptr<Player> player);
};
