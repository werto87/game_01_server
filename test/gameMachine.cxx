#include "src/game/logic/gameMachine.hxx"
#include "durak/gameData.hxx"
#include <catch2/catch.hpp>

namespace test
{

std::vector<durak::Card>
testCardDeck ()
{
  return std::vector<durak::Card>{ { 7, durak::Type::clubs }, { 8, durak::Type::clubs }, { 3, durak::Type::hearts }, { 3, durak::Type::clubs }, { 2, durak::Type::diamonds }, { 3, durak::Type::diamonds }, { 2, durak::Type::clubs }, { 5, durak::Type::diamonds }, { 6, durak::Type::diamonds }, { 7, durak::Type::diamonds }, { 8, durak::Type::diamonds }, { 9, durak::Type::diamonds }, { 1, durak::Type::spades }, { 2, durak::Type::spades }, { 3, durak::Type::spades }, { 1, durak::Type::diamonds }, { 5, durak::Type::spades }, { 6, durak::Type::spades }, { 7, durak::Type::spades }, { 8, durak::Type::spades }, { 9, durak::Type::spades }, { 1, durak::Type::hearts }, { 2, durak::Type::hearts }, { 9, durak::Type::clubs }, { 1, durak::Type::clubs }, { 5, durak::Type::hearts }, { 6, durak::Type::clubs }, { 7, durak::Type::hearts }, { 8, durak::Type::hearts }, { 9, durak::Type::hearts }, { 4, durak::Type::hearts }, { 4, durak::Type::diamonds }, { 4, durak::Type::spades }, { 4, durak::Type::clubs }, { 5, durak::Type::clubs }, { 6, durak::Type::hearts } };
}

TEST_CASE ("player starts attack cards on table", "[game]")
{
  // std::vector<std::shared_ptr<User>> users{};

  // users.push_back (std::make_shared<User> (User{}));
  // users.push_back (std::make_shared<User> (User{}));
  // auto gameMachine = GameMachine{ users };
  // REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  // REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  // auto attackingPlayer = gameMachine.getGame ().getAttackingPlayer ();
  // gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer->id, .cards{ attackingPlayer->getCards ().at (0) } });
  // REQUIRE (not gameMachine.getGame ().getTable ().empty ());
}

TEST_CASE ("playground", "[game]")
{
  // std::vector<std::shared_ptr<User>> users{};
  // users.push_back (std::make_shared<User> (User{ .accountName = { "Player1" }, .msgQueue = {} }));
  // users.push_back (std::make_shared<User> (User{ .accountName = { "Player2" }, .msgQueue = {} }));
  // auto names = std::vector<std::string>{};
  // std::ranges::transform (users, std::back_inserter (names), [] (auto const &tempUser) { return tempUser->accountName.value (); });
  // auto game = durak::Game{ std::move (names), testCardDeck () };
  // auto gameMachine = GameMachine{ game, users };
  // REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  // REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  // auto attackingPlayer = gameMachine.getGame ().getAttackingPlayer ();
  // gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer->id, .cards{ attackingPlayer->getCards ().at (0) } });
  // REQUIRE (not gameMachine.getGame ().getTable ().empty ());
}
}