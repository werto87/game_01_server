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
#include <boost/fusion/include/pair.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/support/pair.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/type_index.hpp>
#include <confu_boost/confuBoost.hxx>
#include <crypt.h>
#include <game_01_shared_class/serialization.hxx>
#include <iostream>
#include <iterator>
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
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
  using Ts::operator()...;
};
template <class... Ts> overloaded (Ts...) -> overloaded<Ts...>;

boost::asio::awaitable<std::vector<std::string> >
handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User> > &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto result = std::vector<std::string>{};
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () == 2)
    {
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      if (typeToSearch == "CreateAccount")
        {
          result.push_back (co_await createAccount (objectAsString, io_context, pool));
        }
      else if (typeToSearch == "LoginAccount")
        {
          result.push_back (co_await loginAccount (objectAsString, io_context, users, *user, pool));
        }
      else if (typeToSearch == "BroadCastMessage")
        {
          result.push_back (broadCastMessage (objectAsString, users, *user));
        }
      else if (user->accountName && typeToSearch == "JoinChannel")
        {
          // if user is logged in should be checked inside of joinChannel user->accountName so we can send error
          result.push_back (joinChannel (objectAsString, *user));
        }
      else if (user->accountName && typeToSearch == "LeaveChannel")
        {
          // if user is logged in should be checked inside of leaveChannel user->accountName so we can send error
          leaveChannel (objectAsString, *user);
        }
      else if (typeToSearch == "LogoutAccount")
        {
          result.push_back (logoutAccount (*user));
        }
      else if (typeToSearch == "CreateGameLobby")
        {
          result.push_back (createGameLobby (objectAsString, user, gameLobbys));
        }
      else if (typeToSearch == "JoinGameLobby")
        {
          if (auto joinGameLobbyError = joinGameLobby (objectAsString, user, gameLobbys))
            {
              result.push_back (joinGameLobbyError.value ());
            }
        }
      else
        {
          std::cout << "could not find a match for typeToSearch '" << typeToSearch << "'" << std::endl;
          result.push_back ("error|unhandled message: " + msg);
        }
    }
  co_return result;
}

boost::asio::awaitable<std::string>
createAccount (std::string objectAsString, boost::asio::io_context &io_context, boost::asio::thread_pool &pool)
{
  auto createAccountObject = confu_boost::toObject<shared_class::CreateAccount> (objectAsString);
  soci::session sql (soci::sqlite3, pathToTestDatabase);
  if (confu_soci::findStruct<database::Account> (sql, "accountName", createAccountObject.accountName))
    {
      co_return objectToStringWithObjectName (shared_class::CreateAccountError{ .accountName = createAccountObject.accountName, .error = "account already created" });
    }
  else
    {
      auto hashedPw = co_await async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable);
      if (database::createAccount (createAccountObject.accountName, hashedPw))
        {
          co_return objectToStringWithObjectName (shared_class::CreateAccountSuccess{ .accountName = createAccountObject.accountName });
        }
      else
        {
          co_return objectToStringWithObjectName (shared_class::CreateAccountError{ .accountName = createAccountObject.accountName, .error = "account already created" });
        }
    }
}

boost::asio::awaitable<std::string>
loginAccount (std::string objectAsString, boost::asio::io_context &io_context, std::list<std::shared_ptr<User> > &users, User &user, boost::asio::thread_pool &pool)
{
  auto loginAccountObject = confu_boost::toObject<shared_class::LoginAccount> (objectAsString);
  soci::session sql (soci::sqlite3, pathToTestDatabase);
  if (auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName))
    {
      if (std::find_if (users.begin (), users.end (), [accountName = account->accountName] (auto const &u) { return accountName == u->accountName; }) != users.end ())
        {
          co_return objectToStringWithObjectName (shared_class::LoginAccountError{ .accountName = loginAccountObject.accountName, .error = "Account already logged in" });
        }
      else
        {
          if (co_await async_check_hashed_pw (pool, io_context, account->password, loginAccountObject.password, boost::asio::use_awaitable))
            {
              user.accountName = account->accountName;
              co_return objectToStringWithObjectName (shared_class::LoginAccountSuccess{ .accountName = loginAccountObject.accountName });
            }
          else
            {
              co_return objectToStringWithObjectName (shared_class::LoginAccountError{ .accountName = loginAccountObject.accountName, .error = "Incorrect username or password" });
            }
        }
    }
  else
    {
      co_return objectToStringWithObjectName (shared_class::LoginAccountError{ .accountName = loginAccountObject.accountName, .error = "Incorrect username or password" });
    }
}

std::string
logoutAccount (User &user)
{
  user.accountName = {};
  user.msgQueue.clear ();
  user.communicationChannels.clear ();
  return objectToStringWithObjectName (shared_class::LogoutAccountSuccess{});
}

std::string
broadCastMessage (std::string const &objectAsString, std::list<std::shared_ptr<User> > &users, User const &sendingUser)
{
  auto broadCastMessageObject = confu_boost::toObject<shared_class::BroadCastMessage> (objectAsString);
  if (sendingUser.accountName)
    {
      for (auto &user : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountName = sendingUser.accountName] (auto const &user) {
                          //
                          return user->communicationChannels.find (channel) != user->communicationChannels.end ();
                        }))
        {
          soci::session sql (soci::sqlite3, pathToTestDatabase);
          auto message = shared_class::Message{ .fromAccount = sendingUser.accountName.value (), .channel = broadCastMessageObject.channel, .message = broadCastMessageObject.message };
          user->msgQueue.push_back (objectToStringWithObjectName (std::move (message)));
        }
      return objectToStringWithObjectName (shared_class::BroadCastMessageSuccess{ .channel = broadCastMessageObject.channel, .message = broadCastMessageObject.message });
    }
  else
    {
      return objectToStringWithObjectName (shared_class::BroadCastMessageError{ .channel = broadCastMessageObject.channel, .error = "account not logged in" });
    }
}

std::string
joinChannel (std::string const &objectAsString, User &user)
{
  auto joinChannelObject = confu_boost::toObject<shared_class::JoinChannel> (objectAsString);
  user.communicationChannels.insert (joinChannelObject.channel);
  return objectToStringWithObjectName (shared_class::JoinChannelSuccess{ .channel = joinChannelObject.channel });
}

void
leaveChannel (std::string const &objectAsString, User &user)
{
  auto leaveChannelObject = confu_boost::toObject<shared_class::LeaveChannel> (objectAsString);
  user.communicationChannels.erase (leaveChannelObject.channel);
}

std::string
createGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto createGameLobbyObject = confu_boost::toObject<shared_class::CreateGameLobby> (objectAsString);
  // TODO userId accountId userName account name this is hell!!!! replace everything with accountName
  if (auto gameLobbyWithUser = std::ranges::find_if (gameLobbys,
                                                     [accountName = user->accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames ();
                                                       return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
      gameLobbyWithUser != gameLobbys.end ())
    {
      return objectToStringWithObjectName (shared_class::CreateGameLobbyError{ .error = { "account has already a game lobby with the name: " + gameLobbyWithUser->gameLobbyName () } });
    }
  else
    {
      auto newGameLobby = gameLobbys.emplace_back (GameLobby{ ._name = createGameLobbyObject.name, ._password = createGameLobbyObject.password });
      if (newGameLobby.tryToAddUser (user))
        {
          return objectToStringWithObjectName (shared_class::CreateGameLobbySuccess{ .name = createGameLobbyObject.name });
        }
      else
        {
          throw std::logic_error{ "user can not join lobby which he created" };
        }
    }
}

std::optional<std::string>
joinGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto joinGameLobbyObject = confu_boost::toObject<shared_class::JoinGameLobby> (objectAsString);
  if (auto gameLobby = std::ranges::find_if (gameLobbys, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.name] (auto const &_gameLobby) { return _gameLobby.gameLobbyName () == gameLobbyName && _gameLobby.gameLobbyPassword () == gameLobbyName; }); gameLobby != gameLobbys.end ())
    {
      if (gameLobby->tryToAddUser (user))
        {
          // send every player in the game lobby the actual players in the game lobby
          return {};
        }
    }
  return objectToStringWithObjectName (shared_class::JoinGameLobbyError{});
}
