#include "pch.h"
#include "PokerRoom.h"
#include "AI/HoldemAlwaysCallBotStrategy.h"
#include "AI/HoldemBotConfig.h"
#include "AI/HoldemDecisionBuilder.h"
#include "Game/HoldemTableSnapshot.h"
#include "RoomMgr.h"
#include "Net/NetPack.h"
#include "Player/PlayerUtils.h"
#include <algorithm>

namespace
{
	constexpr uint64_t kSnapshotHashOffset = 14695981039346656037ull;
	constexpr uint64_t kSnapshotHashPrime = 1099511628211ull;

	uint64_t HashCombine(uint64_t hash, uint64_t value)
	{
		return (hash ^ value) * kSnapshotHashPrime;
	}

	uint64_t HashSeat(uint64_t hash, const Seat& seat)
	{
		hash = HashCombine(hash, static_cast<uint64_t>(seat.seatIndex));
		hash = HashCombine(hash, static_cast<uint64_t>(seat.playerId));
		hash = HashCombine(hash, static_cast<uint64_t>(seat.chips));
		hash = HashCombine(hash, static_cast<uint64_t>(seat.currentBet));
		hash = HashCombine(hash, static_cast<uint64_t>(seat.totalBetThisHand));
		hash = HashCombine(hash, seat.inHand ? 1u : 0u);
		hash = HashCombine(hash, seat.folded ? 1u : 0u);
		hash = HashCombine(hash, seat.allIn ? 1u : 0u);
		hash = HashCombine(hash, seat.pendingLeave ? 1u : 0u);
		hash = HashCombine(hash, seat.sittingOut ? 1u : 0u);
		hash = HashCombine(hash, seat.autoMode ? 1u : 0u);
		return hash;
	}

	uint64_t HashSnapshot(const HoldemTableSnapshot& snapshot)
	{
		uint64_t hash = kSnapshotHashOffset;
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.stage));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.totalPot));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.actingPlayerId));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.lastBet));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.lastRaise));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.smallBlind));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.bigBlind));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.dealerSeatIndex));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.smallBlindSeatIndex));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.bigBlindSeatIndex));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.lastActionPlayerId));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.lastAction));
		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.lastActionAmount));

		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.sidePots.size()));
		for (const auto& pot : snapshot.sidePots)
		{
			hash = HashCombine(hash, static_cast<uint64_t>(pot.amount));
			hash = HashCombine(hash, static_cast<uint64_t>(pot.eligiblePlayerIds.size()));
			for (int pid : pot.eligiblePlayerIds)
				hash = HashCombine(hash, static_cast<uint64_t>(pid));
		}

		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.community.size()));
		for (const auto& card : snapshot.community)
		{
			hash = HashCombine(hash, static_cast<uint64_t>(card.Rank()));
			hash = HashCombine(hash, static_cast<uint64_t>(card.Suit()));
		}

		hash = HashCombine(hash, static_cast<uint64_t>(snapshot.seats.size()));
		for (const auto& seatSnapshot : snapshot.seats)
			hash = HashSeat(hash, seatSnapshot.seat);

		return hash;
	}
}

PokerRoom::PokerRoom(uint32_t timerTickMs, uint32_t timerSlotCount)
	: Room(timerTickMs, timerSlotCount)
{
}

void PokerRoom::OnPlayerExit(std::shared_ptr<Player> player)
{
	if (player)
	{
		ReturnChipsToPlayer(player);

		auto wLock = _lock.OnWrite();
		int playerId = player->GetID();
		_game.MarkPendingLeave(playerId);
		UnregisterPlayer(playerId);
	}
	Room::OnPlayerExit(player);

}

RpcError PokerRoom::OnRecvPlayerNetPack(std::shared_ptr<Player> player, NetPack& pack)
{
	switch (pack.MsgType())
	{
	case RpcEnum::rpc_server_get_poker_table_info:
		SendTableInfoTo(player);
		return RpcError::SUCCESS;
	case RpcEnum::rpc_server_sit_down:
	{
		HandleSitDown(player);
		return RpcError::SUCCESS;
	}
	case RpcEnum::rpc_server_poker_buyin:
	{
		int amount = pack.ReadInt32();
		HandleBuyIn(player, amount);
		return RpcError::SUCCESS;
	}
	case RpcEnum::rpc_server_poker_standup:
	{
		HandleStandUp(player);
		return RpcError::SUCCESS;
	}
	case RpcEnum::rpc_server_poker_set_blinds:
	{
		int smallBlind = pack.ReadInt32();
		int bigBlind = pack.ReadInt32();
		HandleSetBlinds(player, smallBlind, bigBlind);
		return RpcError::SUCCESS;
	}
	case RpcEnum::rpc_server_poker_action:
	{
		auto action = pack.ReadUInt8();
		int amount = pack.ReadInt32();
		HandlePlayerAction(player, action, amount);
		return RpcError::SUCCESS;
	}
	case RpcEnum::rpc_server_poker_add_bot:
	{
		HandleAddBot(player);
		return RpcError::SUCCESS;
	}
	case RpcEnum::rpc_server_poker_kick_bot:
	{
		int seatIdx = pack.ReadInt32();
		HandleKickBot(seatIdx);
		return RpcError::SUCCESS;
	}
	default:
		return Room::OnRecvPlayerNetPack(player, pack);
	}
}

void PokerRoom::OnRoomCreated(int id)
{
	Room::OnRoomCreated(id);
	_type = RoomType::POKER_ROOM;
}

void PokerRoom::OnRoomDestroy()
{
	std::unordered_map<int, int> refunds{};
	{
		auto wLock = _lock.OnWrite();
		const auto& seats = _game.GetSeats();
		for (const Seat& seat : seats)
		{
			if (seat.playerId < 0)
				continue;
			int refund = seat.chips + seat.totalBetThisHand;
			if (refund <= 0)
				continue;
			refunds[seat.playerId] += refund;
		}
		CancelBotActionsExcept(-1);
		_botActionTimers.clear();
		_bots.clear();
	}

	for (const auto& entry : refunds)
	{
		int playerId = entry.first;
		int refund = entry.second;
		PlayerUtils::AddChipsToDatabase(playerId, refund, [playerId, refund](bool success)
			{
				if (success)
				{
					std::cout << "[PokerRoom] Returned " << refund << " chips to player " << playerId
						<< " on room destroy" << std::endl;
				}
				else
				{
					std::cerr << "[PokerRoom] CRITICAL: Failed to return " << refund << " chips to player "
						<< playerId << " on room destroy" << std::endl;
				}
			});
	}

	Room::OnRoomDestroy();
}

void PokerRoom::OnTick()
{
	bool shouldBroadcastHandResult = false;
	HandResult handResult{};
	int actingPlayerId = -1;
	HoldemTableSnapshot snapshot{};
	uint64_t snapshotHash = 0;
	
	{
		auto wLock = _lock.OnWrite();
		_game.RemovePendingLeavers();
		ClearBotsIfNoHumans();
		if (_game.GetStage() == HoldemPokerGame::Stage::Waiting)
			RemoveBrokeBots();
		if (_game.CanStart())
			_game.StartHand();
		_game.ProcessAutoModePlayer();
		_game.ResolveIfNeeded();
		
		// Check for pending hand result
		if (_game.HasPendingHandResult())
		{
			shouldBroadcastHandResult = true;
			handResult = _game.GetLastHandResult();
			_game.ClearPendingHandResult();
		}

		actingPlayerId = _game.ActingPlayerId();
		snapshot = HoldemTableSnapshot::Build(_game, -1);
		snapshotHash = HashSnapshot(snapshot);
	}
	
	// Broadcast hand result if available
	if (shouldBroadcastHandResult)
		BroadcastHandResult(handResult);

	ScheduleBotActionIfNeeded(actingPlayerId);

	if (!_hasSnapshotHash || snapshotHash != _lastSnapshotHash)
	{
		_lastSnapshotHash = snapshotHash;
		_hasSnapshotHash = true;
		BroadcastTableInfo();
	}
}

std::shared_ptr<Player> PokerRoom::GetPlayerById(int playerId)
{
	auto it = _playerById.find(playerId);
	if (it != _playerById.end())
		return it->second;
	return nullptr;
}

void PokerRoom::RegisterPlayer(std::shared_ptr<Player> player)
{
	if (player)
		_playerById[player->GetID()] = player;
}

void PokerRoom::UnregisterPlayer(int playerId)
{
	_playerById.erase(playerId);
}

HoldemBot* PokerRoom::GetBotById(int botId)
{
	auto it = _bots.find(botId);
	if (it != _bots.end())
		return it->second.get();
	return nullptr;
}

void PokerRoom::SendTableInfoTo(std::shared_ptr<Player> player)
{
	if (!player || player->Expired()) return;

	NetPack send{ RpcEnum::rpc_client_get_poker_table_info };
	{
		auto rLock = _lock.OnRead();
		send.WriteInt32(_roomId);
		_game.WriteTable(send, player->GetID());
	}
	player->Send(send);
}

void PokerRoom::BroadcastTableInfo()
{
	std::vector<std::shared_ptr<Player>> members{};
	{
		auto rLock = _lock.OnRead();
		for (const auto& m : _members)
		{
			if (m && !m->Expired())
				members.push_back(m);
		}
	}
	for (auto& p : members)
		SendTableInfoTo(p);
}

void PokerRoom::BroadcastHandResult(const HandResult& result)
{
	std::vector<std::shared_ptr<Player>> members{};
	{
		auto rLock = _lock.OnRead();
		for (const auto& m : _members)
		{
			if (m && !m->Expired())
				members.push_back(m);
		}
	}
	
	for (auto& p : members)
	{
		NetPack send{ RpcEnum::rpc_client_poker_hand_result };
		send.WriteInt32(_roomId);
		result.Write(send);
		p->Send(send);
	}
}

void PokerRoom::ScheduleBotActionIfNeeded(int actingPlayerId)
{
	if (actingPlayerId < 0)
	{
		CancelBotActionsExcept(-1);
		return;
	}

	if (!_bots.contains(actingPlayerId))
	{
		CancelBotActionsExcept(-1);
		return;
	}

	CancelBotActionsExcept(actingPlayerId);
	if (_botActionTimers.contains(actingPlayerId))
		return;

	int delayMs = HoldemBotConfig::GetThinkTimeMs();
	auto handle = GetTimerWheel().ScheduleOnce(static_cast<uint32_t>(delayMs),
		[this, actingPlayerId]()
		{
			ExecuteBotAction(actingPlayerId);
		});

	if (handle.IsValid())
		_botActionTimers[actingPlayerId] = handle;
}

void PokerRoom::ExecuteBotAction(int botId)
{
	_botActionTimers.erase(botId);

	HoldemBot* bot = GetBotById(botId);
	if (!bot)
		return;

	HoldemDecision decision{};
	{
		auto wLock = _lock.OnWrite();
		if (_game.ActingPlayerId() != botId)
			return;

		HoldemTableSnapshot snapshot = HoldemTableSnapshot::Build(_game, botId);
		HoldemDecisionSeat seat{};
		HoldemDecisionOptions options{};
		if (!HoldemDecisionBuilder::Build(snapshot, seat, options))
			return;

		decision = bot->Decide(snapshot, seat, options);
		if (decision.action == HoldemDecisionAction::BetRaise && !options.canRaise)
			decision.action = HoldemDecisionAction::CheckCall;

		HoldemPokerGame::Action actionEnum = HoldemPokerGame::Action::CheckCall;
		int amount = 0;
		switch (decision.action)
		{
		case HoldemDecisionAction::CheckCall:
			actionEnum = HoldemPokerGame::Action::CheckCall;
			break;
		case HoldemDecisionAction::BetRaise:
			if (snapshot.lastBet <= 0)
			{
				actionEnum = HoldemPokerGame::Action::Bet;
				amount = decision.raiseTo;
			}
			else
			{
				actionEnum = HoldemPokerGame::Action::Raise;
				amount = std::max(0, decision.raiseTo - snapshot.lastBet);
			}
			break;
		case HoldemDecisionAction::Fold:
		default:
			actionEnum = HoldemPokerGame::Action::Fold;
			break;
		}

		_game.HandleAction(botId, actionEnum, amount);
	}
}

void PokerRoom::CancelBotActionsExcept(int botId)
{
	for (auto it = _botActionTimers.begin(); it != _botActionTimers.end();)
	{
		if (it->first == botId)
		{
			++it;
			continue;
		}
		GetTimerWheel().Cancel(it->second);
		it = _botActionTimers.erase(it);
	}
}

void PokerRoom::ClearBotsIfNoHumans()
{
	if (GetPlayerCnt() != 0)
		return;
	if (_game.GetStage() != HoldemPokerGame::Stage::Waiting)
		return;
	if (_bots.empty())
		return;

	for (const auto& entry : _bots)
		_game.MarkPendingLeave(entry.first);
	CancelBotActionsExcept(-1);
	_bots.clear();
	_botActionTimers.clear();
	_game.RemovePendingLeavers();
}

void PokerRoom::RemoveBrokeBots()
{
	if (_bots.empty())
		return;

	std::vector<int> toRemove{};
	for (const auto& entry : _bots)
	{
		int botId = entry.first;
		const Seat* seat = _game.GetSeatByPlayerId(botId);
		if (!seat || seat->chips <= 0)
			toRemove.push_back(botId);
	}

	if (toRemove.empty())
		return;

	for (int botId : toRemove)
		RemoveBotById(botId);

	_game.RemovePendingLeavers();
}

void PokerRoom::RemoveBotById(int botId)
{
	auto it = _bots.find(botId);
	if (it == _bots.end())
		return;

	_game.MarkPendingLeave(botId);
	auto timerIt = _botActionTimers.find(botId);
	if (timerIt != _botActionTimers.end())
	{
		GetTimerWheel().Cancel(timerIt->second);
		_botActionTimers.erase(timerIt);
	}
	_bots.erase(it);
}

void PokerRoom::HandleSitDown(std::shared_ptr<Player> player)
{
	if (!player) return;

	auto wLock = _lock.OnWrite();
	int playerId = player->GetID();
	int actualSeatIdx = -1;

	if (!_game.AreBlindsSet())
	{
		player->SendError(RpcError::POKER_BLINDS_NOT_SET);
		return;
	}

	if (!_game.HasAvailableSeat())
	{
		player->SendError(RpcError::POKER_TABLE_FULL);
		return;
	}

	if (!_game.SitDown(playerId, -1, actualSeatIdx))
		return;

	RegisterPlayer(player);

	NetPack send{ RpcEnum::rpc_client_sit_down };
	send.WriteInt32(actualSeatIdx);
	send.WriteInt32(0);
	send.WriteInt32(_game.GetMinBuyin());
	send.WriteInt32(_game.GetBigBlind());
	send.WriteInt32(player->GetInfo().GetChip());
	player->Send(send);
}

void PokerRoom::HandleBuyIn(std::shared_ptr<Player> player, int amount)
{
	if (!player) return;

	int playerId = player->GetID();
	int walletChips = player->GetInfo().GetChip();

	if (!_game.AreBlindsSet())
	{
		player->SendError(RpcError::POKER_BLINDS_NOT_SET);
		return;
	}

	if (_game.GetSeatByPlayerId(playerId) == nullptr)
	{
		player->SendError(RpcError::POKER_PLAYER_NOT_SEATED);
		return;
	}

	if (walletChips < amount)
	{
		player->SendError(RpcError::POKER_INSUFFICIENT_CHIPS);
		return;
	}

	if (amount < _game.GetMinBuyin())
	{
		NetPack send{ RpcEnum::rpc_client_poker_buyin };
		send.WriteUInt8(static_cast<uint8_t>(HoldemPokerGame::BuyInResult::BelowMinimum));
		send.WriteInt32(0);
		send.WriteInt32(walletChips);
		player->Send(send);
		return;
	}

	PlayerUtils::AddChipsToDatabase(playerId, -amount, [this, player, playerId, amount](bool dbSuccess)
		{
			if (!dbSuccess)
			{
				player->SendError(RpcError::POKER_BUYIN_FAILED);
				return;
			}

			player->GetInfo().AddChipsMemoryOnly(-amount);

			auto wLock = _lock.OnWrite();
			auto result = _game.BuyIn(playerId, amount);

			NetPack send{ RpcEnum::rpc_client_poker_buyin };
			send.WriteUInt8(static_cast<uint8_t>(result));
			if (result == HoldemPokerGame::BuyInResult::Success)
			{
				const Seat* seat = _game.GetSeatByPlayerId(playerId);
				send.WriteInt32(seat ? seat->chips : 0);
			}
			else
			{
				PlayerUtils::AddChipsToDatabase(playerId, amount, [player, amount](bool refundSuccess)
					{
						if (refundSuccess)
							player->GetInfo().AddChipsMemoryOnly(amount);
					});
				send.WriteInt32(0);
			}
			send.WriteInt32(player->GetInfo().GetChip());
			player->Send(send);
		});
}

void PokerRoom::HandleStandUp(std::shared_ptr<Player> player)
{
	if (!player) return;

	auto wLock = _lock.OnWrite();
	int playerId = player->GetID();
	bool success = _game.StandUp(playerId);

	NetPack send{ RpcEnum::rpc_client_poker_standup };
	send.WriteUInt8(success ? 1 : 0);
	player->Send(send);
}

void PokerRoom::HandleSetBlinds(std::shared_ptr<Player> player, int smallBlind, int bigBlind)
{
	if (!player) return;

	if (smallBlind <= 0 ||
		bigBlind < 2 * smallBlind)
	{
		player->SendError(RpcError::POKER_INVALID_BLIND);
		return;
	}

	auto wLock = _lock.OnWrite();
	auto result = _game.SetBlinds(smallBlind, bigBlind);

	NetPack send{ RpcEnum::rpc_client_poker_set_blinds };
	send.WriteUInt8(static_cast<uint8_t>(result));
	send.WriteInt32(_game.GetSmallBlind());
	send.WriteInt32(_game.GetBigBlind());
	send.WriteInt32(_game.GetMinBuyin());
	player->Send(send);
}

void PokerRoom::HandlePlayerAction(std::shared_ptr<Player> player, uint8_t action, int amount)
{
	if (!player) return;

	auto wLock = _lock.OnWrite();
	int playerId = player->GetID();

	if (_game.CanStart())
		_game.StartHand();

	auto actionEnum = static_cast<HoldemPokerGame::Action>(action);
	HoldemPokerGame::ActionResult result = _game.HandleAction(playerId, actionEnum, amount);

	if (result == HoldemPokerGame::ActionResult::Invalid)
		player->SendError(RpcError::POKER_INVALID_ACTION);
}

void PokerRoom::HandleAddBot(std::shared_ptr<Player> player)
{
	if (player)
	{
		bool blindsSet = false;
		{
			auto rLock = _lock.OnRead();
			blindsSet = _game.AreBlindsSet();
		}
		if (!blindsSet)
		{
			player->SendError(RpcError::POKER_BLINDS_NOT_SET);
			return;
		}
	}

	auto bot = std::make_unique<HoldemBot>(std::make_unique<HoldemAlwaysCallBotStrategy>());
	int botId = bot->GetID();
	int actualSeatIdx = -1;

	{
		auto wLock = _lock.OnWrite();
		if (!_game.SitDown(botId, -1, actualSeatIdx))
			return;

		int buyin = HoldemBotConfig::GetStartingChips();
		int minBuyin = _game.GetMinBuyin();
		if (buyin < minBuyin)
			buyin = minBuyin;

		auto result = _game.BuyIn(botId, buyin);
		if (result != HoldemPokerGame::BuyInResult::Success)
		{
			_game.MarkPendingLeave(botId);
			return;
		}
		bot->GetInfo().SetChipsMemoryOnly(buyin);
		_bots[botId] = std::move(bot);
	}
}

void PokerRoom::HandleKickBot(int seatIdx)
{
	if (seatIdx < 0)
		return;

	int botId = -1;
	{
		auto wLock = _lock.OnWrite();
		const Seat* seat = _game.GetSeatByIndex(seatIdx);
		if (!seat)
			return;
		if (!_bots.contains(seat->playerId))
			return;
		botId = seat->playerId;
		RemoveBotById(botId);
	}
}

void PokerRoom::ReturnChipsToPlayer(std::shared_ptr<Player> player)
{
	if (!player) return;

	int playerId = player->GetID();
	int tableChips = 0;

	{
		auto wLock = _lock.OnWrite();
		tableChips = _game.CashOut(playerId);
	}

	if (tableChips <= 0) return;

	PlayerUtils::AddChipsToDatabase(playerId, tableChips, [player, tableChips](bool success)
		{
			if (success)
			{
				player->GetInfo().AddChipsMemoryOnly(tableChips);
				std::cout << "[PokerRoom] Returned " << tableChips << " chips to player " << player->GetID() << std::endl;
			}
			else
			{
				std::cerr << "[PokerRoom] CRITICAL: Failed to return " << tableChips << " chips to player " << player->GetID() << std::endl;
			}
		});
}
