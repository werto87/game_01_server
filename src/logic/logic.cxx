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
#include <pipes/filter.hpp>
#include <pipes/pipes.hpp>
#include <pipes/push_back.hpp>
#include <pipes/transform.hpp>
#include <range/v3/all.hpp>
#include <range/v3/range.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/view/filter.hpp>
#include <sodium.h>
#include <sstream>
#include <string>
#include <utility>

boost::asio::awaitable<std::vector<std::string> >
handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::map<size_t, User> &users, User &user)
{
  auto result = std::vector<std::string>{};
  if (boost::algorithm::contains (msg, "create account|"))
    {
      if (auto accountAsString = co_await createAccount (msg, io_context, pool))
        {
          result.push_back (accountAsString.value ());
        }
    }
  else if (boost::algorithm::contains (msg, "login account|"))
    {
      if (auto loginResult = co_await loginAccount (msg, io_context, pool, user))
        {
          result.push_back (loginResult.value ());
        }
    }
  // broadcast message|channel,msg
  else if (boost::algorithm::contains (msg, "broadcast message|"))
    {
      broadcastMessage (msg, users, user);
    }
  // join channel|channel
  else if (boost::algorithm::contains (msg, "join channel|"))
    {
      joinChannel (msg, user);
    }
  // leave channel|channel
  else if (boost::algorithm::contains (msg, "leave channel|"))
    {
      leaveChannel (msg, user);
    }
  else
    {
      result.push_back ("error|unhandled message: " + msg);
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
loginAccount (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, User &user)
{
  auto result = std::string{ "login result|false,Password and Account does not match" };
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
              user.accountId = account->id;
              result = "login result|true,ok";
            }
        }
    }
  co_return result;
}

void
broadcastMessage (std::string const &msg, std::map<size_t, User> &users, User const &sendingUser)
{
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () >= 2)
    {
      boost::algorithm::split (splitMesssage, splitMesssage.at (1), boost::is_any_of (","));
      if (splitMesssage.size () >= 2)
        {
          for (auto &[key, user] : users | ranges::views::filter ([channel = splitMesssage.at (0), accountId = sendingUser.accountId] (auto const &keyUser) {
                                     auto &user = std::get<1> (keyUser);
                                     return user.accountId != accountId && user.communicationChannels.find (channel) != user.communicationChannels.end ();
                                   }))
            {
              user.msgQueue.push_back (splitMesssage.at (1));
            }
        }
    }
}

void
joinChannel (std::string const &msg, User &user)
{
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () >= 2)
    {
      boost::algorithm::split (splitMesssage, splitMesssage.at (1), boost::is_any_of (","));
      if (splitMesssage.size () >= 1)
        {
          user.communicationChannels.insert (splitMesssage.at (0));
        }
    }
}

void
leaveChannel (std::string const &msg, User &user)
{
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () >= 2)
    {
      boost::algorithm::split (splitMesssage, splitMesssage.at (1), boost::is_any_of (","));
      if (splitMesssage.size () >= 1)
        {
          user.communicationChannels.erase (splitMesssage.at (0));
        }
    }
}
