#include "src/util.hxx"
#include <durak/gameData.hxx>
#include <game_01_shared_class/serialization.hxx>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/unique.hpp>
#include <range/v3/algorithm/unique_copy.hpp>
#include <range/v3/all.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
durak::GameData
filterGameDataByAccountName (durak::GameData const &gameData, std::string const &accountName)
{
  auto filteredGameData = gameData;
  for (auto &player : filteredGameData.players | ranges::views::filter ([&accountName] (auto const &player) { return player.name != accountName; }))
    {
      ranges::transform (player.cards, player.cards.begin (), [] (boost::optional<durak::Card> const &) { return boost::optional<durak::Card>{}; });
    }
  return filteredGameData;
}

shared_class::DurakAllowedMoves
allowedMoves (durak::Game const &game, durak::PlayerRole playerRole, std::optional<std::vector<durak::Move>> const &overrideCalculatedAllowedMoves, std::optional<std::vector<durak::Move>> const &addToAllowedMoves)
{
  auto allowedMoves = shared_class::DurakAllowedMoves{ overrideCalculatedAllowedMoves.value_or (game.getAllowedMoves (playerRole)) };
  if (addToAllowedMoves && not addToAllowedMoves->empty ())
    {
      allowedMoves.allowedMoves.insert (allowedMoves.allowedMoves.end (), addToAllowedMoves.value ().begin (), addToAllowedMoves.value ().end ());
      ranges::sort (allowedMoves.allowedMoves);
      auto result = shared_class::DurakAllowedMoves{};
      ranges::unique_copy (allowedMoves.allowedMoves, ranges::back_inserter (result.allowedMoves));
      return result;
    }
  else
    {
      return allowedMoves;
    }
}

void
sendAvailableMoves (durak::Game const &game, std::vector<GameUser> const &_gameUsers, AllowedMoves const &overrideCalculatedAllowedMoves, AllowedMoves const &addToAllowedMoves)
{
  if (auto attackingPlayer = game.getAttackingPlayer ())
    {
      if (auto attackingUser = ranges::find_if (_gameUsers, [attackingPlayerName = attackingPlayer->id] (GameUser const &gameUser) { return gameUser._user->accountName == attackingPlayerName; }); attackingUser != _gameUsers.end ())
        {
          attackingUser->_user->msgQueue.push_back (objectToStringWithObjectName (allowedMoves (game, durak::PlayerRole::attack, overrideCalculatedAllowedMoves.attack, addToAllowedMoves.attack)));
        }
    }
  if (auto assistingPlayer = game.getAssistingPlayer ())
    {
      if (auto assistingUser = ranges::find_if (_gameUsers, [assistingPlayerName = assistingPlayer->id] (GameUser const &gameUser) { return gameUser._user->accountName == assistingPlayerName; }); assistingUser != _gameUsers.end ())
        {
          assistingUser->_user->msgQueue.push_back (objectToStringWithObjectName (allowedMoves (game, durak::PlayerRole::assistAttacker, overrideCalculatedAllowedMoves.assist, addToAllowedMoves.assist)));
        }
    }
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      if (auto defendingUser = ranges::find_if (_gameUsers, [defendingPlayerName = defendingPlayer->id] (GameUser const &gameUser) { return gameUser._user->accountName == defendingPlayerName; }); defendingUser != _gameUsers.end ())
        {
          defendingUser->_user->msgQueue.push_back (objectToStringWithObjectName (allowedMoves (game, durak::PlayerRole::defend, overrideCalculatedAllowedMoves.defend, addToAllowedMoves.defend)));
        }
    }
}

void
sendGameDataToAccountsInGame (durak::Game const &game, std::vector<GameUser> const &_gameUsers, AllowedMoves const &overrideCalculatedAllowedMoves, AllowedMoves const &addToAllowedMoves)
{
  auto gameData = game.getGameData ();
  ranges::for_each (gameData.players, [] (auto &player) { ranges::sort (player.cards, [] (auto const &card1, auto const &card2) { return card1.value () < card2.value (); }); });
  ranges::for_each (_gameUsers, [&gameData] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (filterGameDataByAccountName (gameData, gameUser._user->accountName.value ()))); });
  sendAvailableMoves (game, _gameUsers, overrideCalculatedAllowedMoves, addToAllowedMoves);
}