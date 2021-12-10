#include "src/game/logic/durakStateMachine.hxx"
#include "durak/gameData.hxx"
#include "src/database/database.hxx"
#include "src/game/gameUser.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/game/logic/gameMachine.hxx"
#include "src/server/gameLobby.hxx"
#include "src/server/user.hxx"
#include "test/util.hxx"
#include <catch2/catch.hpp>
#include <chrono>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <cstddef>
#include <durak/card.hxx>
#include <durak/game.hxx>
#include <iterator>

TEST_CASE ("updateRatingWinLose", "[rating]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  auto user1 = std::make_shared<User> ();
  user1->accountName = "user1";
  auto user2 = std::make_shared<User> ();
  user2->accountName = "user2";
  auto user3 = std::make_shared<User> ();
  user3->accountName = "user3";
  auto user4 = std::make_shared<User> ();
  user4->accountName = "user4";
  auto gameUsers = std::vector<GameUser>{ { user1, {} }, { user2, {} }, { user3, {} }, { user4, {} } };
  SECTION ("updateRatingWinLose 2 player", "[rating]")
  {
    auto losers = std::vector<database::Account>{ { "player1", "", 1500 } };
    auto winners = std::vector<database::Account>{ { "player2", "", 2000 } };
    updateRatingWinLose (gameUsers, losers, winners);
    auto sql = soci::session (soci::sqlite3, databaseName);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").value ().rating == 1499);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").value ().rating == 2001);
  }
  SECTION ("updateRatingWinLose 3 player different", "[rating]")
  {
    auto losers = std::vector<database::Account>{ { "player1", "", 2000 } };
    auto winners = std::vector<database::Account>{ { "player2", "", 1000 }, { "player3", "", 1000 }, { "player4", "", 1000 } };
    updateRatingWinLose (gameUsers, losers, winners);
    auto sql = soci::session (soci::sqlite3, databaseName);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").value ().rating == 1980);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").value ().rating == 1007);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").value ().rating == 1007);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player4").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player4").value ().rating == 1007);
  }
  database::createEmptyDatabase ();
  database::createTables ();
}

TEST_CASE ("updateRatingDraw", "[rating]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  auto user1 = std::make_shared<User> ();
  user1->accountName = "user1";
  auto user2 = std::make_shared<User> ();
  user2->accountName = "user2";
  auto user3 = std::make_shared<User> ();
  user3->accountName = "user3";
  auto user4 = std::make_shared<User> ();
  user4->accountName = "user4";
  auto gameUsers = std::vector<GameUser>{ { user1, {} }, { user2, {} }, { user3, {} }, { user4, {} } };
  SECTION ("updateRatingDraw", "[rating]")
  {
    auto accounts = std::vector<database::Account>{ { "player1", "", 1500 }, { "player2", "", 2000 } };
    updateRatingDraw (gameUsers, accounts);
    auto sql = soci::session (soci::sqlite3, databaseName);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").value ().rating == 1509);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").value ().rating == 1991);
  }
  SECTION ("updateRatingDraw 3 players", "[rating]")
  {
    auto accounts = std::vector<database::Account>{ { "player1", "", 1500 }, { "player2", "", 2000 }, { "player3", "", 2000 } };
    updateRatingDraw (gameUsers, accounts);
    auto sql = soci::session (soci::sqlite3, databaseName);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").value ().rating == 1512);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").value ().rating == 1994);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").value ().rating == 1994);
  }
  SECTION ("updateRatingDraw 4 players 3 low 1 high", "[rating]")
  {
    auto accounts = std::vector<database::Account>{ { "player1", "", 1000 }, { "player2", "", 2000 }, { "player3", "", 1000 }, { "player4", "", 1000 } };
    updateRatingDraw (gameUsers, accounts);
    auto sql = soci::session (soci::sqlite3, databaseName);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").value ().rating == 1003);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").value ().rating == 1990);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").value ().rating == 1003);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player4").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player4").value ().rating == 1003);
  }

  SECTION ("updateRatingDraw 4 players 1 low 3 high", "[rating]")
  {
    auto accounts = std::vector<database::Account>{ { "player1", "", 2000 }, { "player2", "", 2000 }, { "player3", "", 2000 }, { "player4", "", 1000 } };
    updateRatingDraw (gameUsers, accounts);
    auto sql = soci::session (soci::sqlite3, databaseName);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").value ().rating == 1993);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").value ().rating == 1993);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").value ().rating == 1993);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player4").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player4").value ().rating == 1021);
  }
  SECTION ("updateRatingDraw 4 players 2 low 2 high", "[rating]")
  {
    auto accounts = std::vector<database::Account>{ { "player1", "", 2000 }, { "player2", "", 2000 }, { "player3", "", 1 }, { "player4", "", 1 } };
    updateRatingDraw (gameUsers, accounts);
    auto sql = soci::session (soci::sqlite3, databaseName);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player1").value ().rating == 1990);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player2").value ().rating == 1990);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player3").value ().rating == 11);
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player4").has_value ());
    CHECK (confu_soci::findStruct<database::Account> (sql, "accountName", "player4").value ().rating == 11);
  }
  database::createEmptyDatabase ();
  database::createTables ();
}