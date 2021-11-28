#include "src/game/logic/gameMachine.hxx"
#include "durak/gameData.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "test/util.hxx"
#include <catch2/catch.hpp>
#include <chrono>
#include <cstddef>
#include <durak/card.hxx>
#include <durak/game.hxx>
#include <iterator>
namespace test
{

std::vector<durak::Card>
testCardDeckMax36 (size_t cardCount = 36)
{
  auto cards = std::vector<durak::Card>{ { 7, durak::Type::clubs }, { 8, durak::Type::clubs }, { 3, durak::Type::hearts }, { 3, durak::Type::clubs }, { 2, durak::Type::diamonds }, { 3, durak::Type::diamonds }, { 2, durak::Type::clubs }, { 5, durak::Type::diamonds }, { 6, durak::Type::diamonds }, { 7, durak::Type::diamonds }, { 8, durak::Type::diamonds }, { 9, durak::Type::diamonds }, { 1, durak::Type::spades }, { 2, durak::Type::spades }, { 3, durak::Type::spades }, { 1, durak::Type::diamonds }, { 5, durak::Type::spades }, { 6, durak::Type::spades }, { 7, durak::Type::spades }, { 8, durak::Type::spades }, { 9, durak::Type::spades }, { 1, durak::Type::hearts }, { 2, durak::Type::hearts }, { 9, durak::Type::clubs }, { 1, durak::Type::clubs }, { 5, durak::Type::hearts }, { 6, durak::Type::clubs }, { 7, durak::Type::hearts }, { 8, durak::Type::hearts }, { 9, durak::Type::hearts }, { 4, durak::Type::hearts }, { 4, durak::Type::diamonds }, { 4, durak::Type::spades }, { 4, durak::Type::clubs }, { 5, durak::Type::clubs }, { 6, durak::Type::hearts } };

  return std::vector<durak::Card>{ cards.begin (), cards.begin () + static_cast<long int> (cardCount <= cards.size () ? cardCount : cards.size ()) };
}

TEST_CASE ("player starts attack cards on table", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  auto &player1MsgQueue = users.at (0)->msgQueue;
  auto &player2MsgQueue = users.at (1)->msgQueue;
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 () }, users, io_context, {}, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto attackingPlayer = gameMachine.getGame ().getAttackingPlayer ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer->id, .cards{ attackingPlayer->getCards ().at (0) } });
  REQUIRE (not player1MsgQueue.empty ());
  REQUIRE (not player2MsgQueue.empty ());
  REQUIRE (not gameMachine.getGame ().getTable ().empty ());
  REQUIRE (gameMachine.getGame ().getTable ().front ().first == durak::Card{ .value = 6, .type = durak::Type::hearts });
}

TEST_CASE ("player beats card and round ends", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  auto &player1MsgQueue = users.at (0)->msgQueue;
  auto &player2MsgQueue = users.at (1)->msgQueue;
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 () }, users, io_context, {}, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto &attackingPlayer = gameMachine.getGame ().getAttackingPlayer ().value ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  auto &table = gameMachine.getGame ().getTable ();
  REQUIRE (not table.empty ());
  auto &cardToBeat = table.front ().first;
  auto &slotToPutCardIn = table.front ().second;
  REQUIRE (cardToBeat == durak::Card{ .value = 6, .type = durak::Type::hearts });
  REQUIRE_FALSE (slotToPutCardIn.has_value ());
  auto &durak = gameMachine.durakStateMachine;
  auto &defendingPlayer = gameMachine.getGame ().getDefendingPlayer ().value ();
  durak.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = cardToBeat, .card = defendingPlayer.getCards ().at (0) });
  REQUIRE (not player1MsgQueue.empty ());
  REQUIRE (player1MsgQueue.at (0) != player2MsgQueue.at (1));
  REQUIRE (slotToPutCardIn.has_value ());
  REQUIRE (attackingPlayer.id == "Player1");
  REQUIRE (defendingPlayer.id == "Player2");
  durak.process_event (attackPass{ .playerName = "Player1" });
  REQUIRE (not player1MsgQueue.empty ());
  REQUIRE (not player2MsgQueue.empty ());
  durak.process_event (defendAnswerNo{ .playerName = defendingPlayer.id });
  REQUIRE (gameMachine.getGame ().getRound () == 2);
}

TEST_CASE ("player beats card but takes them in the end and attacking player adds cards", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 (10) }, users, io_context, {}, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto &attackingPlayer = gameMachine.getGame ().getAttackingPlayer ().value ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (2) } });
  auto &table = gameMachine.getGame ().getTable ();
  auto &cardToBeat = table.front ().first;
  auto &durak = gameMachine.durakStateMachine;
  auto &defendingPlayer = gameMachine.getGame ().getDefendingPlayer ().value ();
  durak.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = cardToBeat, .card = defendingPlayer.getCards ().at (3) });
  durak.process_event (attackPass{ .playerName = "Player1" });
  REQUIRE (gameMachine.getGame ().getRound () == 1);
  durak.process_event (defendAnswerYes{});
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (2) } });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  REQUIRE (attackingPlayer.id == "Player1");
  REQUIRE (defendingPlayer.id == "Player2");
  REQUIRE (gameMachine.getGame ().getRound () == 2);
}

TEST_CASE ("attacking player does not add cards", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  auto &player1MsgQueue = users.at (0)->msgQueue;
  auto &player2MsgQueue = users.at (1)->msgQueue;
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 (10) }, users, io_context, {}, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto &attackingPlayer = gameMachine.getGame ().getAttackingPlayer ().value ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (2) } });
  auto &table = gameMachine.getGame ().getTable ();
  auto &cardToBeat = table.front ().first;
  auto &durak = gameMachine.durakStateMachine;
  auto &defendingPlayer = gameMachine.getGame ().getDefendingPlayer ().value ();
  durak.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = cardToBeat, .card = defendingPlayer.getCards ().at (3) });
  durak.process_event (attackPass{ .playerName = "Player1" });
  durak.process_event (defendAnswerYes{});
  REQUIRE (not player1MsgQueue.empty ());
  REQUIRE (not player2MsgQueue.empty ());
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  REQUIRE (attackingPlayer.id == "Player1");
  REQUIRE (defendingPlayer.id == "Player2");
  REQUIRE (gameMachine.getGame ().getRound () == 2);
}

TEST_CASE ("3 player attack assist def takes cards", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player3";
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2", "Player3" }, testCardDeckMax36 () }, users, io_context, {}, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 3);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto &attackingPlayer = gameMachine.getGame ().getAttackingPlayer ().value ();
  auto &assistingPlayer = gameMachine.getGame ().getAssistingPlayer ().value ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (2) } });
  auto &defendingPlayer = gameMachine.getGame ().getDefendingPlayer ().value ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (4) } });
  auto &table = gameMachine.getGame ().getTable ();
  auto &cardToBeat = table.at (1).first;
  auto &durak = gameMachine.durakStateMachine;
  durak.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = cardToBeat, .card = defendingPlayer.getCards ().at (0) });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = assistingPlayer.id, .cards{ assistingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  durak.process_event (attackPass{ .playerName = "Player1" });
  durak.process_event (assistPass{ .playerName = "Player3" });
  REQUIRE (defendingPlayer.id == "Player1");
  REQUIRE (assistingPlayer.id == "Player2");
  REQUIRE (attackingPlayer.id == "Player3");
  REQUIRE (gameMachine.getGame ().getRound () == 2);
  auto &player1MsgQueue = users.at (0)->msgQueue;
  auto &player2MsgQueue = users.at (1)->msgQueue;
  auto &player3MsgQueue = users.at (2)->msgQueue;
  CHECK (player1MsgQueue[0] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":6,"type":"hearts"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[1] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  CHECK (player1MsgQueue[2] == R"foo(DurakAttackSuccess|{})foo");
  CHECK (player1MsgQueue[3] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":6,"type":"hearts"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[4] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  CHECK (player1MsgQueue[5] == R"foo(DurakAttackSuccess|{})foo");
  CHECK (player1MsgQueue[6] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null],[{"Card":{"value":4,"type":"hearts"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":6,"type":"hearts"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[7] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  CHECK (player1MsgQueue[8] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null],[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}]],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":6,"type":"hearts"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[9] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  CHECK (player1MsgQueue[10] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null],[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],[{"Card":{"value":9,"type":"clubs"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":6,"type":"hearts"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[11] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  CHECK (player1MsgQueue[12] == R"foo(DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards|{})foo");
  CHECK (player1MsgQueue[13] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AttackAssistDoneAddingCards"},{"Move":"AddCards"}]})foo");
  CHECK (player1MsgQueue[14] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player1MsgQueue[15] == R"foo(DurakDefendWantsToTakeCardsFromTableDoneAddingCardsSuccess|{})foo");
  CHECK (player1MsgQueue[16] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":1,"type":"diamonds"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":5,"type":"spades"}},{"Card":{"value":6,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":2,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":15})foo");
  CHECK (player1MsgQueue[17] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player2MsgQueue[0] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[1] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player2MsgQueue[2] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[3] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"Defend"},{"Move":"TakeCards"}]})foo");
  CHECK (player2MsgQueue[4] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null],[{"Card":{"value":4,"type":"hearts"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[5] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"Defend"},{"Move":"TakeCards"}]})foo");
  CHECK (player2MsgQueue[6] == R"foo(DurakDefendSuccess|{})foo");
  CHECK (player2MsgQueue[7] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null],[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[8] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"Defend"},{"Move":"TakeCards"}]})foo");
  CHECK (player2MsgQueue[9] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null],[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],[{"Card":{"value":9,"type":"clubs"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[10] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"Defend"},{"Move":"TakeCards"}]})foo");
  CHECK (player2MsgQueue[11] == R"foo(DurakDefendPassSuccess|{})foo");
  CHECK (player2MsgQueue[12] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player2MsgQueue[13] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}},{"Card":{"value":9,"type":"clubs"}}],"playerRole":"assistAttacker"}}],"round":2,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":15})foo");
  CHECK (player2MsgQueue[14] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player3MsgQueue[0] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[1] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player3MsgQueue[2] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[3] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player3MsgQueue[4] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null],[{"Card":{"value":4,"type":"hearts"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[5] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player3MsgQueue[6] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null],[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[7] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  CHECK (player3MsgQueue[8] == R"foo(DurakAttackSuccess|{})foo");
  CHECK (player3MsgQueue[9] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":4,"type":"clubs"}},null],[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],[{"Card":{"value":9,"type":"clubs"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[10] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  CHECK (player3MsgQueue[11] == R"foo(DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards|{})foo");
  CHECK (player3MsgQueue[12] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AttackAssistDoneAddingCards"},{"Move":"AddCards"}]})foo");
  CHECK (player3MsgQueue[13] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player3MsgQueue[14] == R"foo(DurakDefendWantsToTakeCardsFromTableDoneAddingCardsSuccess|{})foo");
  CHECK (player3MsgQueue[15] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":6,"type":"spades"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":2,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":15})foo");
  CHECK (player3MsgQueue[16] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");

  // std::cout << player1MsgQueue << std::endl;
  // std::cout << "MSG_QUEU2" << std::endl;
  // std::cout << player2MsgQueue << std::endl;
  // std::cout << "MSG_QUEU3" << std::endl;
  // std::cout << player3MsgQueue << std::endl;
}

TEST_CASE ("3 player attack assist def success", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player3";
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2", "Player3" }, testCardDeckMax36 () }, users, io_context, {}, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 3);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto &attackingPlayer = gameMachine.getGame ().getAttackingPlayer ().value ();
  auto &defendingPlayer = gameMachine.getGame ().getDefendingPlayer ().value ();
  auto &assistingPlayer = gameMachine.getGame ().getAssistingPlayer ().value ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  auto &table = gameMachine.getGame ().getTable ();
  REQUIRE (not table.empty ());
  auto &cardToBeat = table.front ().first;
  auto &slotToPutCardIn = table.front ().second;
  REQUIRE (cardToBeat == durak::Card{ .value = 6, .type = durak::Type::hearts });
  REQUIRE_FALSE (slotToPutCardIn.has_value ());
  auto &durak = gameMachine.durakStateMachine;
  durak.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = cardToBeat, .card = defendingPlayer.getCards ().at (0) });
  durak.process_event (attackPass{ .playerName = "Player1" });
  durak.process_event (assistPass{ .playerName = "Player3" });
  durak.process_event (defendAnswerNo{ .playerName = defendingPlayer.id });
  REQUIRE (gameMachine.getGame ().getRound () == 2);
  REQUIRE (attackingPlayer.id == "Player2");
  REQUIRE (defendingPlayer.id == "Player3");
  REQUIRE (assistingPlayer.id == "Player1");
  auto &player1MsgQueue = users.at (0)->msgQueue;
  auto &player2MsgQueue = users.at (1)->msgQueue;
  auto &player3MsgQueue = users.at (2)->msgQueue;
  CHECK (player1MsgQueue[0] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":6,"type":"hearts"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[1] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  CHECK (player1MsgQueue[2] == R"foo(DurakAttackSuccess|{})foo");
  CHECK (player1MsgQueue[3] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[4] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player1MsgQueue[5] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}]],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[6] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AttackAssistPass"}]})foo");
  CHECK (player1MsgQueue[7] == R"foo(DurakAttackPassSuccess|{})foo");
  CHECK (player1MsgQueue[8] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player1MsgQueue[9] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":5,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":2,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":16})foo");
  CHECK (player1MsgQueue[10] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");

  CHECK (player2MsgQueue[0] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[1] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player2MsgQueue[2] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[3] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"Defend"},{"Move":"TakeCards"}]})foo");
  CHECK (player2MsgQueue[4] == R"foo(DurakDefendSuccess|{})foo");
  CHECK (player2MsgQueue[5] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[6] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"TakeCards"}]})foo");
  CHECK (player2MsgQueue[7] == R"foo(DurakAskDefendWantToTakeCards|{})foo");
  CHECK (player2MsgQueue[8] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AnswerDefendWantsToTakeCardsYes"},{"Move":"AnswerDefendWantsToTakeCardsNo"}]})foo");
  CHECK (player2MsgQueue[9] == R"foo(DurakAskDefendWantToTakeCardsAnswerSuccess|{})foo");
  CHECK (player2MsgQueue[10] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":6,"type":"spades"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":2,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":16})foo");
  CHECK (player2MsgQueue[11] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");

  CHECK (player3MsgQueue[0] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[1] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player3MsgQueue[2] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[3] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player3MsgQueue[4] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[5] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"},{"Move":"AttackAssistPass"}]})foo");
  CHECK (player3MsgQueue[6] == R"foo(DurakAssistPassSuccess|{})foo");
  CHECK (player3MsgQueue[7] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  CHECK (player3MsgQueue[8] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":2,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":16})foo");

  // std::cout << player1MsgQueue << std::endl;
  // std::cout << "MSG_QUEU2" << std::endl;
  // std::cout << player2MsgQueue << std::endl;
  // std::cout << "MSG_QUEU3" << std::endl;
  // std::cout << player3MsgQueue << std::endl;
}
TEST_CASE ("3 player attack assist def success with timer", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player3";
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2", "Player3" }, testCardDeckMax36 () }, users, io_context, TimerOption{ .timerType = shared_class::TimerType::resetTimeOnNewRound, .timeAtStart = std::chrono::seconds{ 1 }, .timeForEachRound = std::chrono::seconds{ 1 } }, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 3);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto &attackingPlayer = gameMachine.getGame ().getAttackingPlayer ().value ();
  auto &defendingPlayer = gameMachine.getGame ().getDefendingPlayer ().value ();
  auto &assistingPlayer = gameMachine.getGame ().getAssistingPlayer ().value ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  auto &table = gameMachine.getGame ().getTable ();
  REQUIRE (not table.empty ());
  auto &cardToBeat = table.front ().first;
  auto &slotToPutCardIn = table.front ().second;
  REQUIRE (cardToBeat == durak::Card{ .value = 6, .type = durak::Type::hearts });
  REQUIRE_FALSE (slotToPutCardIn.has_value ());
  auto &durak = gameMachine.durakStateMachine;
  durak.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = cardToBeat, .card = defendingPlayer.getCards ().at (0) });
  durak.process_event (attackPass{ .playerName = "Player1" });
  durak.process_event (assistPass{ .playerName = "Player3" });
  durak.process_event (defendAnswerNo{ .playerName = defendingPlayer.id });
  REQUIRE (gameMachine.getGame ().getRound () == 2);
  REQUIRE (attackingPlayer.id == "Player2");
  REQUIRE (defendingPlayer.id == "Player3");
  REQUIRE (assistingPlayer.id == "Player1");
  auto &player1MsgQueue = users.at (0)->msgQueue;
  auto &player2MsgQueue = users.at (1)->msgQueue;
  auto &player3MsgQueue = users.at (2)->msgQueue;
  io_context.run ();
  // TODO here is the problem that the time is not consistent. so we can not compare the time left with exact time. maybe we need to add some matcher or just check if the timer event was send
  // NOTE the test still makes sense because if the timer is not send we get an error because the array size wont match
  CHECK (player1MsgQueue[0] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":6,"type":"hearts"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[1] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  // CHECK (player1MsgQueue[2] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player1",1638139861190]],"pausedTimeUserDurationMilliseconds":[["Player2",10000],["Player3",10000]]})foo");
  CHECK (player1MsgQueue[3] == R"foo(DurakAttackSuccess|{})foo");
  CHECK (player1MsgQueue[4] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[5] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  // CHECK (player1MsgQueue[6] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player2",1638139861193]],"pausedTimeUserDurationMilliseconds":[["Player1",9997],["Player3",10000]]})foo");
  CHECK (player1MsgQueue[7] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}]],"players":[{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player1MsgQueue[8] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AttackAssistPass"}]})foo");
  // CHECK (player1MsgQueue[9] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player1",1638139861191],["Player3",1638139861194]],"pausedTimeUserDurationMilliseconds":[["Player2",9998]]})foo");
  CHECK (player1MsgQueue[10] == R"foo(DurakAttackPassSuccess|{})foo");
  CHECK (player1MsgQueue[11] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  // CHECK (player1MsgQueue[12] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player3",1638139861194]],"pausedTimeUserDurationMilliseconds":[["Player1",9996],["Player2",9998]]})foo");
  // CHECK (player1MsgQueue[13] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[],"pausedTimeUserDurationMilliseconds":[["Player1",9996],["Player2",9998],["Player3",9999]]})foo");
  // CHECK (player1MsgQueue[14] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player2",1638139861193]],"pausedTimeUserDurationMilliseconds":[["Player1",9996],["Player3",9999]]})foo");
  CHECK (player1MsgQueue[15] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player1","cards":[{"Card":{"value":4,"type":"hearts"}},{"Card":{"value":4,"type":"clubs"}},{"Card":{"value":4,"type":"diamonds"}},{"Card":{"value":4,"type":"spades"}},{"Card":{"value":5,"type":"clubs"}},{"Card":{"value":5,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":2,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":16})foo");
  CHECK (player1MsgQueue[16] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  // CHECK (player1MsgQueue[17] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player2",1638139861197]],"pausedTimeUserDurationMilliseconds":[["Player1",10000],["Player3",10000]]})foo");
  CHECK (player2MsgQueue[0] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[1] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  // CHECK (player2MsgQueue[2] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player1",1638139861190]],"pausedTimeUserDurationMilliseconds":[["Player2",10000],["Player3",10000]]})foo");
  CHECK (player2MsgQueue[3] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[4] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"Defend"},{"Move":"TakeCards"}]})foo");
  // CHECK (player2MsgQueue[5] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player2",1638139861193]],"pausedTimeUserDurationMilliseconds":[["Player1",9997],["Player3",10000]]})foo");
  CHECK (player2MsgQueue[6] == R"foo(DurakDefendSuccess|{})foo");
  CHECK (player2MsgQueue[7] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player2MsgQueue[8] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"TakeCards"}]})foo");
  // CHECK (player2MsgQueue[9] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player1",1638139861191],["Player3",1638139861194]],"pausedTimeUserDurationMilliseconds":[["Player2",9998]]})foo");
  // CHECK (player2MsgQueue[10] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player3",1638139861194]],"pausedTimeUserDurationMilliseconds":[["Player1",9996],["Player2",9998]]})foo");
  // CHECK (player2MsgQueue[11] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[],"pausedTimeUserDurationMilliseconds":[["Player1",9996],["Player2",9998],["Player3",9999]]})foo");
  CHECK (player2MsgQueue[12] == R"foo(DurakAskDefendWantToTakeCards|{})foo");
  CHECK (player2MsgQueue[13] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AnswerDefendWantsToTakeCardsYes"},{"Move":"AnswerDefendWantsToTakeCardsNo"}]})foo");
  // CHECK (player2MsgQueue[14] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player2",1638139861193]],"pausedTimeUserDurationMilliseconds":[["Player1",9996],["Player3",9999]]})foo");
  CHECK (player2MsgQueue[15] == R"foo(DurakAskDefendWantToTakeCardsAnswerSuccess|{})foo");
  CHECK (player2MsgQueue[16] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player2","cards":[{"Card":{"value":1,"type":"clubs"}},{"Card":{"value":5,"type":"hearts"}},{"Card":{"value":6,"type":"clubs"}},{"Card":{"value":6,"type":"spades"}},{"Card":{"value":7,"type":"hearts"}},{"Card":{"value":8,"type":"hearts"}}],"playerRole":"attack"}},{"PlayerData":{"name":"Player3","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":2,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":16})foo");
  CHECK (player2MsgQueue[17] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"}]})foo");
  // CHECK (player2MsgQueue[18] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player2",1638139861197]],"pausedTimeUserDurationMilliseconds":[["Player1",10000],["Player3",10000]]})foo");
  CHECK (player3MsgQueue[0] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[1] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  // CHECK (player3MsgQueue[2] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player1",1638139861190]],"pausedTimeUserDurationMilliseconds":[["Player2",10000],["Player3",10000]]})foo");
  CHECK (player3MsgQueue[3] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},null]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[4] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  // CHECK (player3MsgQueue[5] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player2",1638139861193]],"pausedTimeUserDurationMilliseconds":[["Player1",9997],["Player3",10000]]})foo");
  CHECK (player3MsgQueue[6] == R"foo(GameData|{"trump":"clubs","table":[[{"Card":{"value":6,"type":"hearts"}},{"Card":{"value":9,"type":"hearts"}}]],"players":[{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null],"playerRole":"defend"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"assistAttacker"}}],"round":1,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":18})foo");
  CHECK (player3MsgQueue[7] == R"foo(DurakAllowedMoves|{"allowedMoves":[{"Move":"AddCards"},{"Move":"AttackAssistPass"}]})foo");
  // CHECK (player3MsgQueue[8] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player1",1638139861191],["Player3",1638139861194]],"pausedTimeUserDurationMilliseconds":[["Player2",9998]]})foo");
  // CHECK (player3MsgQueue[9] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player3",1638139861194]],"pausedTimeUserDurationMilliseconds":[["Player1",9996],["Player2",9998]]})foo");
  CHECK (player3MsgQueue[10] == R"foo(DurakAssistPassSuccess|{})foo");
  CHECK (player3MsgQueue[11] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  // CHECK (player3MsgQueue[12] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[],"pausedTimeUserDurationMilliseconds":[["Player1",9996],["Player2",9998],["Player3",9999]]})foo");
  // CHECK (player3MsgQueue[13] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player2",1638139861193]],"pausedTimeUserDurationMilliseconds":[["Player1",9996],["Player3",9999]]})foo");
  CHECK (player3MsgQueue[14] == R"foo(GameData|{"trump":"clubs","table":[],"players":[{"PlayerData":{"name":"Player2","cards":[null,null,null,null,null,null],"playerRole":"attack"}},{"PlayerData":{"name":"Player3","cards":[{"Card":{"value":1,"type":"hearts"}},{"Card":{"value":2,"type":"hearts"}},{"Card":{"value":7,"type":"spades"}},{"Card":{"value":8,"type":"spades"}},{"Card":{"value":9,"type":"clubs"}},{"Card":{"value":9,"type":"spades"}}],"playerRole":"defend"}},{"PlayerData":{"name":"Player1","cards":[null,null,null,null,null,null],"playerRole":"assistAttacker"}}],"round":2,"lastCardInDeck":{"value":7,"type":"clubs"},"cardsInDeck":16})foo");
  CHECK (player3MsgQueue[15] == R"foo(DurakAllowedMoves|{"allowedMoves":[]})foo");
  // CHECK (player3MsgQueue[16] == R"foo(DurakTimers|{"runningTimeUserTimePointMilliseconds":[["Player2",1638139861197]],"pausedTimeUserDurationMilliseconds":[["Player1",10000],["Player3",10000]]})foo");

  // std::cout << player1MsgQueue << std::endl;
  // std::cout << "MSG_QUEU2" << std::endl;
  // std::cout << player2MsgQueue << std::endl;
  // std::cout << "MSG_QUEU3" << std::endl;
  // std::cout << player3MsgQueue << std::endl;
}

TEST_CASE ("defend takes cards", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 () }, users, io_context, {}, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto &attackingPlayer = gameMachine.getGame ().getAttackingPlayer ().value ();
  auto &defendingPlayer = gameMachine.getGame ().getDefendingPlayer ().value ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  auto &table = gameMachine.getGame ().getTable ();
  REQUIRE (not table.empty ());
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  REQUIRE (gameMachine.getGame ().getRound () == 1);
  REQUIRE (attackingPlayer.getCards ().size () == 5);
}

TEST_CASE ("game ends", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 (7) }, users, io_context, {}, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto &attackingPlayer = gameMachine.getGame ().getAttackingPlayer ().value ();
  auto &defendingPlayer = gameMachine.getGame ().getDefendingPlayer ().value ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  REQUIRE (gameMachine.getGame ().checkIfGameIsOver ());
  REQUIRE (gameMachine.getGame ().durak ().has_value ());
}

TEST_CASE ("game ends draw", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  boost::asio::io_context io_context{};
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 (8) }, users, io_context, {}, [] () {} };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto &attackingPlayer = gameMachine.getGame ().getAttackingPlayer ().value ();
  auto &defendingPlayer = gameMachine.getGame ().getDefendingPlayer ().value ();
  auto &table = gameMachine.getGame ().getTable ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = table.at (0).first, .card = defendingPlayer.getCards ().at (0) });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  gameMachine.durakStateMachine.process_event (defendAnswerNo{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (1) } });
  gameMachine.durakStateMachine.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = table.at (0).first, .card = defendingPlayer.getCards ().at (2) });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  gameMachine.durakStateMachine.process_event (defendAnswerNo{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defendPass{ .playerName = defendingPlayer.id });
  gameMachine.durakStateMachine.process_event (attackPass{ .playerName = attackingPlayer.id });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  gameMachine.durakStateMachine.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = table.at (0).first, .card = defendingPlayer.getCards ().at (1) });
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer.id, .cards{ attackingPlayer.getCards ().at (0) } });
  REQUIRE (not gameMachine.getGame ().checkIfGameIsOver ());
  gameMachine.durakStateMachine.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = table.at (1).first, .card = defendingPlayer.getCards ().at (0) });
  REQUIRE (gameMachine.getGame ().checkIfGameIsOver ());
  REQUIRE (not gameMachine.getGame ().durak ().has_value ());
}

}