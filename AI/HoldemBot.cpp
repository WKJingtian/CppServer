#include "pch.h"
#include "HoldemBot.h"

HoldemBot::HoldemBot(std::unique_ptr<IHoldemBotStrategy> strategy)
	: HoldemBot(std::move(strategy),
		HoldemBotConfig::NextId(),
		HoldemBotConfig::GetDefaultName(),
		HoldemBotConfig::GetStartingChips())
{
}

HoldemBot::HoldemBot(std::unique_ptr<IHoldemBotStrategy> strategy, int id, std::string name, int startingChips)
	: _strategy(std::move(strategy))
{
	_info.SetID(id);
	_info.SetName(std::move(name));
	_info.SetLanguage(Language::English);
	_info.SetChipsMemoryOnly(startingChips);
}

HoldemDecision HoldemBot::Decide(const HoldemTableSnapshot& snapshot,
	const HoldemDecisionSeat& seat,
	const HoldemDecisionOptions& options)
{
	if (!_strategy)
		return HoldemDecision{};
	return _strategy->Decide(snapshot, seat, options);
}
