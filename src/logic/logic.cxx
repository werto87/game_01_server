#include "src/logic/logic.hxx"
#include "src/database/database.hxx"
#include "src/pw_hash/passwordHash.hxx"
#include <algorithm>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/type_index.hpp>
#include <crypt.h>
#include <sodium.h>
#include <sstream>
#include <string>

boost::asio::awaitable<std::vector<std::string> >
handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool)
{
  auto result = std::vector<std::string>{};
  if (boost::algorithm::starts_with (msg, "create account|"))
    {
      if (auto accountAsString = co_await createAccount (msg, io_context, pool))
        {
          result.push_back (accountAsString.value ());
        }
    }
  else if (boost::algorithm::contains (msg, "create character|"))
    {
      if (auto characterAsString = createCharacter (msg))
        {
          result.push_back (characterAsString.value ());
        }
    }
  else if (boost::algorithm::contains (msg, "login account|"))
    {
      if (auto loginResult = co_await loginAccount (msg, io_context, pool))
        {
          result.push_back (loginResult.value ());
        }
    }
  co_return result;
}

boost::asio::awaitable<boost::optional<std::string> >
createAccount (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool)
{
  auto accountStringStream = std::stringstream{};
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () >= 2)
    {
      boost::algorithm::split (splitMesssage, splitMesssage.at (1), boost::is_any_of (","));
      if (splitMesssage.size () >= 2)
        {
          auto hashedPw = co_await async_hash (pool, io_context, splitMesssage.at (1), boost::asio::use_awaitable);
          if (auto account = database::createAccount (splitMesssage.at (0), hashedPw))
            {
              accountStringStream << "account|";
              boost::archive::text_oarchive accountArchive{ accountStringStream };
              accountArchive << account.value ();
            }
        }
    }
  co_return accountStringStream.str ();
}

boost::asio::awaitable<boost::optional<std::string> >
loginAccount (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool)
{
  auto result = std::string{ "login result|false,Password and Account does not match " };
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () >= 2)
    {
      boost::algorithm::split (splitMesssage, splitMesssage.at (1), boost::is_any_of (","));
      if (splitMesssage.size () >= 2)
        {
          soci::session sql (soci::sqlite3, pathToTestDatabase);
          auto account = confu_soci::findStruct<database::Account> (sql, "accountName", splitMesssage.at (0));
          if (account && co_await async_check_hashed_pw (pool, io_context, account->password, splitMesssage.at (1), boost::asio::use_awaitable))
            {
              result = "login result|true,ok";
            }
        }
    }
  co_return result;
}

boost::optional<std::string>
createCharacter (std::string const &msg)
{
  auto characterStringStream = std::stringstream{};
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () >= 2)
    {
      if (auto character = database::createCharacter (splitMesssage.at (1)))
        {
          boost::archive::text_oarchive characterArchive{ characterStringStream };
          characterArchive << character.value ();
        }
    }
  return characterStringStream.str ();
}