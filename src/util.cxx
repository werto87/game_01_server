#include "src/util.hxx"
#include <durak/gameData.hxx>
#include <game_01_shared_class/serialization.hxx>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/all.hpp>
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

void
sendAvailableMoves (durak::Game const &game, std::vector<GameUser> const &_gameUsers, AllowedMoves const &allowedMoves)
{
  if (auto attackingPlayer = game.getAttackingPlayer ())
    {
      if (auto attackingUser = ranges::find_if (_gameUsers, [attackingPlayerName = attackingPlayer->id] (GameUser const &gameUser) { return gameUser._user->accountName == attackingPlayerName; }); attackingUser != _gameUsers.end ())
        {
          attackingUser->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ (allowedMoves.attack) ? allowedMoves.attack.value () : game.getAllowedMoves (durak::PlayerRole::attack) }));
        }
    }
  if (auto assistingPlayer = game.getAssistingPlayer ())
    {
      if (auto assistingUser = ranges::find_if (_gameUsers, [assistingPlayerName = assistingPlayer->id] (GameUser const &gameUser) { return gameUser._user->accountName == assistingPlayerName; }); assistingUser != _gameUsers.end ())
        {
          assistingUser->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ (allowedMoves.assist) ? allowedMoves.assist.value () : game.getAllowedMoves (durak::PlayerRole::assistAttacker) }));
        }
    }
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      if (auto defendingUser = ranges::find_if (_gameUsers, [defendingPlayerName = defendingPlayer->id] (GameUser const &gameUser) { return gameUser._user->accountName == defendingPlayerName; }); defendingUser != _gameUsers.end ())
        {
          defendingUser->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ (allowedMoves.defend) ? allowedMoves.defend.value () : game.getAllowedMoves (durak::PlayerRole::defend) }));
        }
    }
}

void
sendGameDataToAccountsInGame (durak::Game const &game, std::vector<GameUser> const &_gameUsers, AllowedMoves const &allowedMoves)
{
  auto gameData = game.getGameData ();
  ranges::for_each (gameData.players, [] (auto &player) { ranges::sort (player.cards, [] (auto const &card1, auto const &card2) { return card1.value () < card2.value (); }); });
  ranges::for_each (_gameUsers, [&gameData] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (filterGameDataByAccountName (gameData, gameUser._user->accountName.value ()))); });
  sendAvailableMoves (game, _gameUsers, allowedMoves);
}