#include "src/util.hxx"

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

void
sendGameDataToAccountsInGame (durak::Game const &game, std::vector<std::shared_ptr<User>> &users)
{
  auto gameData = game.getGameData ();
  std::ranges::for_each (gameData.players, [] (auto &player) { std::ranges::sort (player.cards, [] (auto const &card1, auto const &card2) { return card1.value () < card2.value (); }); });
  std::ranges::for_each (users, [&gameData] (auto &_user) { _user->msgQueue.push_back (objectToStringWithObjectName (filterGameDataByAccountName (gameData, _user->accountName.value ()))); });
}