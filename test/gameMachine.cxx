#include "src/game/logic/gameMachine.hxx"
#include "durak/gameData.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include <bits/ranges_algo.h>
#include <catch2/catch.hpp>
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
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 () }, users };
  REQUIRE (gameMachine.getGame ().getPlayers ().size () == 2);
  REQUIRE (gameMachine.getGame ().getPlayers ().at (0).id == "Player1");
  auto attackingPlayer = gameMachine.getGame ().getAttackingPlayer ();
  gameMachine.durakStateMachine.process_event (attack{ .playerName = attackingPlayer->id, .cards{ attackingPlayer->getCards ().at (0) } });
  REQUIRE (not player1MsgQueue.empty ());
  REQUIRE (player1MsgQueue.at (0) == "DurakAttackSuccess|22 serialization::archive 18 0 0");
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
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 () }, users };
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
  player1MsgQueue.clear ();
  player2MsgQueue.clear ();
  durak.process_event (defend{ .playerName = defendingPlayer.id, .cardToBeat = cardToBeat, .card = defendingPlayer.getCards ().at (0) });
  REQUIRE (not player1MsgQueue.empty ());
  REQUIRE (player1MsgQueue.at (0) != player2MsgQueue.at (1));
  REQUIRE (slotToPutCardIn.has_value ());
  REQUIRE (attackingPlayer.id == "Player1");
  REQUIRE (defendingPlayer.id == "Player2");
  player1MsgQueue.clear ();
  player2MsgQueue.clear ();
  durak.process_event (attackPass{ .playerName = "Player1" });
  REQUIRE (not player1MsgQueue.empty ());
  REQUIRE (not player2MsgQueue.empty ());
  REQUIRE (player1MsgQueue.front () == "DurakAttackPassSuccess|22 serialization::archive 18 0 0");
  REQUIRE (player2MsgQueue.front () == "DurakAskDefendWantToTakeCards|22 serialization::archive 18 0 0");
  player1MsgQueue.clear ();
  player2MsgQueue.clear ();
  durak.process_event (defendAnswerNo{ .playerName = defendingPlayer.id });
  REQUIRE (not player1MsgQueue.empty ());
  REQUIRE (not player2MsgQueue.empty ());
  REQUIRE (attackingPlayer.id == "Player2");
  REQUIRE (defendingPlayer.id == "Player1");
  REQUIRE (gameMachine.getGame ().getRound () == 2);
}

TEST_CASE ("player beats card but takes them in the end and attacking player adds cards", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  auto &player1MsgQueue = users.at (0)->msgQueue;
  auto &player2MsgQueue = users.at (1)->msgQueue;
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 (10) }, users };
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
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 (10) }, users };
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
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2", "Player3" }, testCardDeckMax36 () }, users };
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
}

TEST_CASE ("3 player attack assist def success", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player3";
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2", "Player3" }, testCardDeckMax36 () }, users };
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
  REQUIRE (users.at (1)->msgQueue.at (4) == "DurakAskDefendWantToTakeCardsAnswerSuccess|22 serialization::archive 18 0 0");
  REQUIRE (gameMachine.getGame ().getRound () == 2);
  REQUIRE (attackingPlayer.id == "Player2");
  REQUIRE (defendingPlayer.id == "Player3");
  REQUIRE (assistingPlayer.id == "Player1");
}

TEST_CASE ("defend takes cards", "[game]")
{
  std::vector<std::shared_ptr<User>> users{};
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player1";
  users.emplace_back (std::make_shared<User> (User{}))->accountName = "Player2";
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 () }, users };
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
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 (7) }, users };
  auto &player1MsgQueue = users.at (0)->msgQueue;
  auto &player2MsgQueue = users.at (1)->msgQueue;
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
  auto gameMachine = GameMachine{ durak::Game{ { "Player1", "Player2" }, testCardDeckMax36 (8) }, users };
  auto &player1MsgQueue = users.at (0)->msgQueue;
  auto &player2MsgQueue = users.at (1)->msgQueue;
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
  gameMachine.getGame ().checkIfGameIsOver ();
  REQUIRE (not gameMachine.getGame ().durak ().has_value ());
}

}