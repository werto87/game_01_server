#include "src/logic/logic.hxx"
#include "src/database/database.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/pw_hash/passwordHash.hxx"
#include "src/util.hxx"
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
#include <boost/serialization/vector.hpp>
#include <boost/type_index.hpp>
#include <confu_json/confu_json.hxx>
#include <crypt.h>
#include <durak/game.hxx>
#include <durak/gameData.hxx>
#include <durak/gameOption.hxx>
#include <fmt/core.h>
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

boost::asio::awaitable<std::vector<std::string>>
handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines)
{
  auto result = std::vector<std::string>{};
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () == 2)
    {
      // TODO rework this. we dont need result we can put in user in every function and add the messages there. this will help for cases when we have to send messages to multiple users. bz improving symetrie
      // we have 3 cases send something to user send somthing to many users dont send anything.
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      if (typeToSearch == "CreateAccount")
        {
          result.push_back (co_await createAccountAndLogin (objectAsString, io_context, user, pool));
          user->ignoreCreateAccount = false;
          user->ignoreLogin = false;
        }
      else if (typeToSearch == "LoginAccount")
        {
          result.push_back (co_await loginAccount (objectAsString, io_context, users, user, pool, gameLobbys, gameMachines));
          user->ignoreCreateAccount = false;
          user->ignoreLogin = false;
        }
      else if (typeToSearch == "BroadCastMessage")
        {
          if (auto broadCastMessageResult = broadCastMessage (objectAsString, users, *user))
            {
              result.push_back (broadCastMessageResult.value ());
            }
        }
      else if (typeToSearch == "JoinChannel")
        {
          result.push_back (joinChannel (objectAsString, *user));
        }
      else if (user->accountName && typeToSearch == "LeaveChannel")
        {
          result.push_back (leaveChannel (objectAsString, *user));
        }
      else if (typeToSearch == "LogoutAccount")
        {
          result.push_back (logoutAccount (*user));
        }
      else if (typeToSearch == "CreateGameLobby")
        {
          result = createGameLobby (objectAsString, user, gameLobbys);
        }
      else if (typeToSearch == "JoinGameLobby")
        {
          if (auto joinGameLobbyError = joinGameLobby (objectAsString, user, gameLobbys))
            {
              result.push_back (joinGameLobbyError.value ());
            }
        }
      else if (typeToSearch == "SetMaxUserSizeInCreateGameLobby")
        {
          if (auto setMaxUserSizeInCreateGameError = setMaxUserSizeInCreateGameLobby (objectAsString, user, gameLobbys))
            {
              result.push_back (setMaxUserSizeInCreateGameError.value ());
            }
        }
      else if (typeToSearch == "SetMaxCardValueInCreateGameLobby")
        {
          if (auto setMaxCardValueInCreateGameLobbyError = setMaxCardValueInCreateGameLobby (objectAsString, user, gameLobbys))
            {
              result.push_back (setMaxCardValueInCreateGameLobbyError.value ());
            }
        }
      else if (typeToSearch == "LeaveGameLobby")
        {
          if (auto leaveGameLobbyResult = leaveGameLobby (user, gameLobbys))
            {
              result.push_back (leaveGameLobbyResult.value ());
            }
        }
      else if (typeToSearch == "RelogTo")
        {
          if (auto relogToResult = relogTo (objectAsString, user, gameLobbys, gameMachines))
            {
              result.push_back (relogToResult.value ());
            }
        }
      else if (typeToSearch == "LoginAccountCancel")
        {
          loginAccountCancel (user);
        }
      else if (typeToSearch == "CreateAccountCancel")
        {
          createAccountCancel (user);
        }
      else if (typeToSearch == "CreateGame")
        {
          createGame (user, gameLobbys, gameMachines);
        }
      else if (typeToSearch == "DurakAttack")
        {
          durakAttack (objectAsString, user, gameMachines);
        }
      else if (typeToSearch == "DurakDefend")
        {
          durakDefend (objectAsString, user, gameMachines);
        }
      else if (typeToSearch == "DurakDefendPass")
        {
          durakDefendPass (user, gameMachines);
        }
      else if (typeToSearch == "DurakAttackPass")
        {
          durakAttackPass (user, gameMachines);
        }
      else if (typeToSearch == "DurakAssistPass")
        {
          durakAssistPass (user, gameMachines);
        }
      else if (typeToSearch == "DurakAskDefendWantToTakeCardsAnswer")
        {
          durakAskDefendWantToTakeCardsAnswer (objectAsString, user, gameMachines);
        }
      else if (typeToSearch == "DurakLeaveGame")
        {
          durakLeaveGame (user, gameMachines);
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
createAccountAndLogin (std::string objectAsString, boost::asio::io_context &io_context, std::shared_ptr<User> user, boost::asio::thread_pool &pool)
{
  auto createAccountObject = stringToObject<shared_class::CreateAccount> (objectAsString);
  soci::session sql (soci::sqlite3, pathToTestDatabase);
  if (confu_soci::findStruct<database::Account> (sql, "accountName", createAccountObject.accountName))
    {
      co_return objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "account already created" });
    }
  else
    {
      auto hashedPw = co_await async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable);
      if (user->ignoreCreateAccount)
        {
          co_return objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "Canceled by User Request" });
        }
      else
        {
          if (auto account = database::createAccount (createAccountObject.accountName, hashedPw))
            {
              user->accountName = account->accountName;
              co_return objectToStringWithObjectName (shared_class::LoginAccountSuccess{ createAccountObject.accountName });
            }
          else
            {
              co_return objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "account already created" });
            }
        }
    }
}

boost::asio::awaitable<std::string>
loginAccount (std::string objectAsString, boost::asio::io_context &io_context, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, boost::asio::thread_pool &pool, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines)
{
  auto loginAccountObject = stringToObject<shared_class::LoginAccount> (objectAsString);
  soci::session sql (soci::sqlite3, pathToTestDatabase);
  if (auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName))
    {
      if (std::find_if (users.begin (), users.end (), [accountName = account->accountName] (auto const &u) { return accountName == u->accountName; }) != users.end ())
        {
          co_return objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Account already logged in" });
        }
      else
        {
          if (co_await async_check_hashed_pw (pool, io_context, account->password, loginAccountObject.password, boost::asio::use_awaitable))
            {
              if (user->ignoreLogin)
                {
                  co_return objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Canceled by User Request" });
                }
              else
                {
                  user->accountName = account->accountName;
                  if (auto gameLobbyWithUser = ranges::find_if (gameLobbys,
                                                                [accountName = user->accountName] (auto const &gameLobby) {
                                                                  auto const &accountNames = gameLobby.accountNames ();
                                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                                });
                      gameLobbyWithUser != gameLobbys.end ())
                    {
                      co_return objectToStringWithObjectName (shared_class::WantToRelog{ loginAccountObject.accountName, "Create Game Lobby" });
                    }
                  else if (auto gameWithUser = ranges::find_if (gameMachines,
                                                                [accountName = user->accountName] (auto const &gameMachine) {
                                                                  auto accountNames = std::vector<std::string>{};
                                                                  ranges::transform (gameMachine.getUsers (), ranges::back_inserter (accountNames), [] (auto const &user) { return user->accountName.value (); });
                                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                                });
                           gameWithUser != gameMachines.end ())
                    {
                      co_return objectToStringWithObjectName (shared_class::WantToRelog{ loginAccountObject.accountName, "Back To Game" });
                    }
                  else
                    {
                      co_return objectToStringWithObjectName (shared_class::LoginAccountSuccess{ loginAccountObject.accountName });
                    }
                }
            }
          else
            {
              co_return objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Incorrect Username or Password" });
            }
        }
    }
  else
    {
      co_return objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Incorrect username or password" });
    }
}

std::string
logoutAccount (User &user)
{
  user.accountName = {};
  user.msgQueue.clear ();
  user.communicationChannels.clear ();
  user.ignoreLogin = false;
  user.ignoreCreateAccount = false;
  return objectToStringWithObjectName (shared_class::LogoutAccountSuccess{});
}

std::optional<std::string>
broadCastMessage (std::string const &objectAsString, std::list<std::shared_ptr<User>> &users, User const &sendingUser)
{
  auto broadCastMessageObject = stringToObject<shared_class::BroadCastMessage> (objectAsString);
  if (sendingUser.accountName)
    {
      for (auto &user : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountName = sendingUser.accountName] (auto const &user) { return user->communicationChannels.find (channel) != user->communicationChannels.end (); }))
        {
          soci::session sql (soci::sqlite3, pathToTestDatabase);
          auto message = shared_class::Message{ sendingUser.accountName.value (), broadCastMessageObject.channel, broadCastMessageObject.message };
          user->msgQueue.push_back (objectToStringWithObjectName (std::move (message)));
        }
      return {};
    }
  else
    {
      return objectToStringWithObjectName (shared_class::BroadCastMessageError{ broadCastMessageObject.channel, "account not logged in" });
    }
}

std::string
joinChannel (std::string const &objectAsString, User &user)
{
  auto joinChannelObject = stringToObject<shared_class::JoinChannel> (objectAsString);
  if (user.accountName)
    {
      user.communicationChannels.insert (joinChannelObject.channel);
      return objectToStringWithObjectName (shared_class::JoinChannelSuccess{ joinChannelObject.channel });
    }
  else
    {
      return objectToStringWithObjectName (shared_class::JoinChannelError{ joinChannelObject.channel, { "user not logged in" } });
    }
}

std::string
leaveChannel (std::string const &objectAsString, User &user)
{
  auto leaveChannelObject = stringToObject<shared_class::LeaveChannel> (objectAsString);
  if (user.accountName)
    {
      if (user.communicationChannels.erase (leaveChannelObject.channel))
        {
          return objectToStringWithObjectName (shared_class::LeaveChannelSuccess{ leaveChannelObject.channel });
        }
      else
        {
          return objectToStringWithObjectName (shared_class::LeaveChannelError{ leaveChannelObject.channel, { "channel not found" } });
        }
    }
  else
    {
      return objectToStringWithObjectName (shared_class::LeaveChannelError{ leaveChannelObject.channel, { "user not logged in" } });
    }
}

void
createGame (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines)
{
  if (auto gameLobbyWithUser = ranges::find_if (gameLobbys,
                                                [accountName = user->accountName] (auto const &gameLobby) {
                                                  auto const &accountNames = gameLobby.accountNames ();
                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                });
      gameLobbyWithUser != gameLobbys.end ())
    {
      if (gameLobbyWithUser->gameLobbyAdminAccountName () == user->accountName)
        {
          ranges::for_each (gameLobbyWithUser->_users, [] (auto &_user) { _user->msgQueue.push_back (objectToStringWithObjectName (shared_class::StartGame{})); });
          auto names = std::vector<std::string>{};
          ranges::transform (gameLobbyWithUser->_users, ranges::back_inserter (names), [] (auto const &tempUser) { return tempUser->accountName.value (); });
          auto game = durak::Game{ std::move (names), gameLobbyWithUser->gameOption };
          auto &gameMachine = gameMachines.emplace_back (game, gameLobbyWithUser->_users);
          sendGameDataToAccountsInGame (gameMachine.getGame (), gameLobbyWithUser->_users);
          gameLobbys.erase (gameLobbyWithUser);
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ "you need to be admin in a game lobby to start a game" }));
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ "you need to be admin in a game lobby to start a game" }));
    }
}

std::vector<std::string>
createGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto createGameLobbyObject = stringToObject<shared_class::CreateGameLobby> (objectAsString);
  if (ranges::find_if (gameLobbys, [gameLobbyName = createGameLobbyObject.name, lobbyPassword = createGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.gameLobbyName () == gameLobbyName && _gameLobby.gameLobbyPassword () == lobbyPassword; }) == gameLobbys.end ())
    {
      if (auto gameLobbyWithUser = ranges::find_if (gameLobbys,
                                                    [accountName = user->accountName] (auto const &gameLobby) {
                                                      auto const &accountNames = gameLobby.accountNames ();
                                                      return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                    });
          gameLobbyWithUser != gameLobbys.end ())
        {
          return { objectToStringWithObjectName (shared_class::CreateGameLobbyError{ { "account has already a game lobby with the name: " + gameLobbyWithUser->gameLobbyName () } }) };
        }
      else
        {
          auto &newGameLobby = gameLobbys.emplace_back (GameLobby{ createGameLobbyObject.name, createGameLobbyObject.password });
          if (newGameLobby.tryToAddUser (user))
            {
              auto result = std::vector<std::string>{};
              auto usersInGameLobby = shared_class::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = newGameLobby.maxUserCount ();
              usersInGameLobby.name = newGameLobby.gameLobbyName ();
              usersInGameLobby.durakGameOption.maxCardValue = newGameLobby.gameOption.maxCardValue;
              ranges::transform (newGameLobby.accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
              result.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
              result.push_back (objectToStringWithObjectName (usersInGameLobby));
              return result;
            }
          else
            {
              throw std::logic_error{ "user can not join lobby which he created" };
            }
        }
    }
  else
    {
      return { objectToStringWithObjectName (shared_class::CreateGameLobbyError{ { "lobby already exists with name: " + createGameLobbyObject.name } }) };
    }
}

std::optional<std::string>
joinGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto joinGameLobbyObject = stringToObject<shared_class::JoinGameLobby> (objectAsString);
  if (auto gameLobby = ranges::find_if (gameLobbys, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.gameLobbyName () == gameLobbyName && _gameLobby.gameLobbyPassword () == lobbyPassword; }); gameLobby != gameLobbys.end ())
    {
      if (gameLobby->tryToAddUser (user))
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
          auto usersInGameLobby = shared_class::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobby->maxUserCount ();
          usersInGameLobby.name = gameLobby->gameLobbyName ();
          usersInGameLobby.durakGameOption.maxCardValue = gameLobby->gameOption.maxCardValue;
          ranges::transform (gameLobby->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
          gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
          return {};
        }
      else
        {
          return objectToStringWithObjectName (shared_class::JoinGameLobbyError{ joinGameLobbyObject.name, "lobby full" });
        }
    }
  else
    {
      return objectToStringWithObjectName (shared_class::JoinGameLobbyError{ joinGameLobbyObject.name, "wrong password name combination or lobby does not exists" });
    }
}

std::optional<std::string>
setMaxUserSizeInCreateGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto setMaxUserSizeInCreateGameLobbyObject = stringToObject<shared_class::SetMaxUserSizeInCreateGameLobby> (objectAsString);
  auto accountNameToSearch = user->accountName.value ();
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbys,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (auto errorMessage = gameLobbyWithAccount->setMaxUserCount (setMaxUserSizeInCreateGameLobbyObject.maxUserSize))
        {
          return objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ errorMessage.value () });
        }
      else
        {
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::MaxUserSizeInCreateGameLobby{ setMaxUserSizeInCreateGameLobbyObject.maxUserSize }));
          return {};
        }
    }
  else
    {
      return objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ "could not find a game lobby for account" });
    }
}

std::optional<std::string>
setMaxCardValueInCreateGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto setMaxCardValueInCreateGameLobbyObject = stringToObject<shared_class::SetMaxCardValueInCreateGameLobby> (objectAsString);
  auto accountNameToSearch = user->accountName.value ();
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbys,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (setMaxCardValueInCreateGameLobbyObject.maxCardValue < 1)
        {
          return objectToStringWithObjectName (shared_class::SetMaxCardValueInCreateGameLobbyError{ "maxCardValue < 1" });
        }
      else
        {
          gameLobbyWithAccount->gameOption.maxCardValue = setMaxCardValueInCreateGameLobbyObject.maxCardValue;
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::MaxCardValueInCreateGameLobby{ setMaxCardValueInCreateGameLobbyObject.maxCardValue }));
          return {};
        }
    }
  else
    {
      return objectToStringWithObjectName (shared_class::SetMaxCardValueInCreateGameLobbyError{ "could not find a game lobby for account" });
    }
}

std::optional<std::string>
leaveGameLobby (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbys,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (gameLobbyWithAccount->removeUser (user))
        {
          gameLobbys.erase (gameLobbyWithAccount);
        }
      else
        {
          auto usersInGameLobby = shared_class::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
          usersInGameLobby.name = gameLobbyWithAccount->gameLobbyName ();
          usersInGameLobby.durakGameOption.maxCardValue = gameLobbyWithAccount->gameOption.maxCardValue;
          ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
        }
      return objectToStringWithObjectName (shared_class::LeaveGameLobbySuccess{});
    }
  else
    {
      return objectToStringWithObjectName (shared_class::LeaveGameLobbyError{ "could not remove user from lobby user not found in lobby" });
    }
}

std::optional<std::string>
relogTo (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines)
{
  auto relogToObject = stringToObject<shared_class::RelogTo> (objectAsString);
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbys,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (relogToObject.wantsToRelog)
        {
          gameLobbyWithAccount->relogUser (user);
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::RelogToCreateGameLobbySuccess{}));
          auto usersInGameLobby = shared_class::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
          usersInGameLobby.name = gameLobbyWithAccount->gameLobbyName ();
          usersInGameLobby.durakGameOption.maxCardValue = gameLobbyWithAccount->gameOption.maxCardValue;
          ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
          return objectToStringWithObjectName (usersInGameLobby);
        }
      else
        {
          gameLobbyWithAccount->removeUser (user);
          if (gameLobbyWithAccount->accountCount () == 0)
            {
              gameLobbys.erase (gameLobbyWithAccount);
            }
          else
            {
              auto usersInGameLobby = shared_class::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
              usersInGameLobby.name = gameLobbyWithAccount->gameLobbyName ();
              usersInGameLobby.durakGameOption.maxCardValue = gameLobbyWithAccount->gameOption.maxCardValue;
              ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
              gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
              return {};
            }
        }
    }
  else if (auto gameWithUser = ranges::find_if (gameMachines,
                                                [accountName = user->accountName] (auto const &gameMachine) {
                                                  auto accountNames = std::vector<std::string>{};
                                                  ranges::transform (gameMachine.getUsers (), ranges::back_inserter (accountNames), [] (auto const &user) { return user->accountName.value (); });
                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                });
           gameWithUser != gameMachines.end ())
    {
      if (relogToObject.wantsToRelog)
        {
          // TODO send things to client so client shows game
          // gameLobbyWithAc count->relogUser (user);
          // gameWithUser
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::RelogToGameSuccess{}));
          // auto usersInGameLobby = shared_class::UsersInGameLobby{};
          // usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
          // usersInGameLobby.name = gameLobbyWithAccount->gameLobbyName ();
          // usersInGameLobby.durakGameOption.maxCardValue = gameLobbyWithAccount->gameOption.maxCardValue;
          // ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
        }
      else
        {
          // TODO remove user from game
          // gameLobbyWithAccount->removeUser (user);
          // if (gameLobbyWithAccount->accountCount () == 0)
          //   {
          //     gameLobbys.erase (gameLobbyWithAccount);
          //   }
          // else
          //   {
          //     auto usersInGameLobby = shared_class::UsersInGameLobby{};
          //     usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
          //     usersInGameLobby.name = gameLobbyWithAccount->gameLobbyName ();
          //     usersInGameLobby.durakGameOption.maxCardValue = gameLobbyWithAccount->gameOption.maxCardValue;
          //     ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
          //     gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
          //     return {};
          //   }
        }
    }
  else if (relogToObject.wantsToRelog)
    {
      return objectToStringWithObjectName (shared_class::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" });
    }
  return {};
}

void
durakAttack (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakAttackObject = stringToObject<shared_class::DurakAttack> (objectAsString);
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (attack{ .playerName = user->accountName.value (), .cards{ std::move (durakAttackObject.cards) } });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakDefend (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakDefendObject = stringToObject<shared_class::DurakDefend> (objectAsString);
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (defend{ .playerName = user->accountName.value (), .cardToBeat{ durakDefendObject.cardToBeat }, .card{ durakDefendObject.card } });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakAttackPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (attackPass{ .playerName = user->accountName.value () });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakAssistPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (assistPass{ .playerName = user->accountName.value () });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakAskDefendWantToTakeCardsAnswer (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakAskDefendWantToTakeCardsAnswerObject = stringToObject<shared_class::DurakAskDefendWantToTakeCardsAnswer> (objectAsString);
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      if (durakAskDefendWantToTakeCardsAnswerObject.answer)
        {
          gameMachine->durakStateMachine.process_event (defendAnswerYes{ .playerName = user->accountName.value () });
        }
      else
        {
          gameMachine->durakStateMachine.process_event (defendAnswerNo{ .playerName = user->accountName.value () });
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCardsAnswerError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
loginAccountCancel (std::shared_ptr<User> user)
{
  if (not user->accountName)
    {
      user->ignoreLogin = true;
    }
}

void
createAccountCancel (std::shared_ptr<User> user)
{
  if (not user->accountName)
    {
      user->ignoreCreateAccount = true;
    }
}
void
durakDefendPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (defendPass{ .playerName = user->accountName.value () });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendPassError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakLeaveGame (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (leaveGame{ user->accountName.value () });
      if (gameMachine->getUsers ().empty ())
        {
          gameMachines.erase (gameMachine);
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakLeaveGameError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
  std::cout << "gameMachines.size (): " << gameMachines.size () << std::endl;
}
