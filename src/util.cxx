#include "src/util.hxx"
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
sendGameDataToAccountsInGame (durak::Game const &game, std::vector<GameUser> const&_gameUsers)
{
  auto gameData = game.getGameData ();
  ranges::for_each (gameData.players, [] (auto &player) { ranges::sort (player.cards, [] (auto const &card1, auto const &card2) { return card1.value () < card2.value (); }); });
  ranges::for_each (_gameUsers, [&gameData] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (filterGameDataByAccountName (gameData, gameUser._user->accountName.value ()))); });
}