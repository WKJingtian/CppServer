#include "pch.h"
#include "PokerRoom.h"
#include "RoomMgr.h"
#include "Net/NetPack.h"
#include "Player/PlayerUtils.h"

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

	bool doRemoveRoom = false;
	{
		auto wLock = _lock.OnWrite();
		if (!_roomExpired && GetPlayerCnt() == 0)
		{
			_roomExpired = true;
			doRemoveRoom = true;
		}
	}
	if (doRemoveRoom)
		RoomMgr::RemoveRoom(_roomId);
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
		int seatIdx = pack.ReadInt32();
		HandleSitDown(player, seatIdx);
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
	default:
		return Room::OnRecvPlayerNetPack(player, pack);
	}
}

void PokerRoom::OnRoomCreated(int id)
{
	Room::OnRoomCreated(id);
	_type = RoomType::POKER_ROOM;
}

void PokerRoom::OnTick()
{
	bool shouldBroadcastHandResult = false;
	HandResult handResult;
	
	{
		auto wLock = _lock.OnWrite();
		_game.RemovePendingLeavers();
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
	}
	
	// Broadcast hand result if available
	if (shouldBroadcastHandResult)
		BroadcastHandResult(handResult);
	
	BroadcastTableInfo();
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

void PokerRoom::HandleSitDown(std::shared_ptr<Player> player, int seatIdx)
{
	if (!player) return;

	auto wLock = _lock.OnWrite();
	int playerId = player->GetID();
	int actualSeatIdx = -1;

	if (!_game.SitDown(playerId, seatIdx, actualSeatIdx))
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

			// ????
			auto wLock = _lock.OnWrite();
			auto result = _game.BuyIn(playerId, amount);

			NetPack send{ RpcEnum::rpc_client_poker_buyin };
			send.WriteUInt8(static_cast<uint8_t>(result));
			if (result == HoldemPokerGame::BuyInResult::Success)
			{
				const Seat* seat = _game.GetSeatByPlayerId(playerId);
				send.WriteInt32(seat ? seat->chips : 0);  // ????
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
	_game.HandleAction(playerId, actionEnum, amount);
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
