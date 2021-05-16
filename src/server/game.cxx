#include "src/server/game.hxx"
#include <boost/optional/optional.hpp>
#include <durak/card.hxx>
#include <durak/gameData.hxx>
#include <ranges>
#include <vector>

Game::Game (GameLobby &&gameLobby) : durakGame{ gameLobby.accountNames () } { _users.swap (gameLobby._users); }

durak::GameData
filterGameDataByAccountName (durak::GameData const &gameData, std::string const &accountName)
{
  auto filteredGameData = gameData;
  for (auto &player : filteredGameData.players | std::ranges::views::filter ([&accountName] (auto const &player) { return player.name != accountName; }))
    {
      std::ranges::transform (player.cards, player.cards.begin (), [] (boost::optional<durak::Card> const &) { return boost::optional<durak::Card>{}; });
    }
  return filteredGameData;
}
