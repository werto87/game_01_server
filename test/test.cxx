#include "src/database/database.hxx"
#include "src/logic/logic.hxx"
#include <boost/archive/basic_text_iarchive.hpp>
#include <boost/archive/basic_text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <catch2/catch.hpp>
#include <confu_soci/convenienceFunctionForSoci.hxx>
namespace test
{

SCENARIO ("create an account with createAccount", "[createAccount]")
{
  GIVEN ("empty database")
  {
    database::createEmptyDatabase ();
    database::createTables ();
    WHEN ("the account gets created")
    {
      auto accountAsString = createAccount ("|joe,doe").value ();
      std::vector<std::string> splitMesssage{};
      boost::algorithm::split (splitMesssage, accountAsString, boost::is_any_of ("|"));

      THEN ("account is in table and result from create account can be serialized into account object")
      {
        auto accountStringStream = std::stringstream{};
        accountStringStream << accountAsString;
        boost::archive::text_iarchive ia (accountStringStream);
        auto account = database::Account{};
        ia >> account;
        REQUIRE (account.accountName == "joe");
        soci::session sql (soci::sqlite3, pathToTestDatabase);
        REQUIRE (confu_soci::findStruct<database::Account> (sql, "accountName", "joe").has_value ());
      }
    }
  }
}

SCENARIO ("create an character with createCharacter", "[createCharacter]")
{
  GIVEN ("empty database")
  {
    database::createEmptyDatabase ();
    database::createTables ();
    soci::session sql (soci::sqlite3, pathToTestDatabase);
    confu_soci::insertStruct (sql, database::Account{ .id = "accountId", .accountName = "firstName", .password = "lastName" });
    WHEN ("the account gets created")
    {
      auto accountAsString = createCharacter ("|accountId").value ();
      THEN ("Character is in table and result from create Character can be serialized into Character object")
      {
        auto characterStringStream = std::stringstream{};
        characterStringStream << accountAsString;
        boost::archive::text_iarchive ia (characterStringStream);
        auto character = database::Character{};
        ia >> character;
        REQUIRE (character.accountId == "accountId");
        REQUIRE (confu_soci::findStruct<database::Character> (sql, "accountId", "accountId").has_value ());
      }
    }
  }
}

SCENARIO ("create an account with handleMessage", "[handleMessage]")
{
  GIVEN ("empty database")
  {
    database::createEmptyDatabase ();
    database::createTables ();
    WHEN ("the account gets created")
    {
      auto accountAsString = handleMessage ("create account|joe,doe").at (0);
      THEN ("account is in table and result from create account can be serialized into account object")
      {
        auto accountStringStream = std::stringstream{};
        accountStringStream << accountAsString;
        boost::archive::text_iarchive ia (accountStringStream);
        auto account = database::Account{};
        ia >> account;
        REQUIRE (account.accountName == "joe");
        soci::session sql (soci::sqlite3, pathToTestDatabase);
        REQUIRE (confu_soci::findStruct<database::Account> (sql, "accountName", "joe").has_value ());
      }
    }
  }
}
}
