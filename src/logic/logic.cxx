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
#include <boost/numeric/conversion/cast.hpp>
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
#include <optional>
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

void
removeGameIfItIsOver (std::string const &accountName, std::list<GameMachine> &gameMachines)
{
  if (auto gameWithUser = ranges::find_if (gameMachines,
                                           [&accountName] (auto const &gameMachine) {
                                             auto accountNames = std::vector<std::string>{};
                                             ranges::transform (gameMachine.getGameUsers (), ranges::back_inserter (accountNames), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
                                             return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                           });
      gameWithUser != gameMachines.end ())
    {
      if (gameWithUser->getGame ().checkIfGameIsOver ())
        {
          gameMachines.erase (gameWithUser);
        }
    }
}

boost::asio::awaitable<void>
handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines)
{
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () == 2)
    {
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      if (typeToSearch == "CreateAccount")
        {
          co_await createAccountAndLogin (objectAsString, io_context, user, pool);
          user->ignoreCreateAccount = false;
          user->ignoreLogin = false;
        }
      else if (typeToSearch == "LoginAccount")
        {
          co_await loginAccount (objectAsString, io_context, users, user, pool, gameLobbys, gameMachines);
          user->ignoreCreateAccount = false;
          user->ignoreLogin = false;
        }
      else if (typeToSearch == "BroadCastMessage")
        {
          broadCastMessage (objectAsString, users, *user);
        }
      else if (typeToSearch == "JoinChannel")
        {
          joinChannel (objectAsString, *user);
        }
      else if (user->accountName && typeToSearch == "LeaveChannel")
        {
          leaveChannel (objectAsString, *user);
        }
      else if (typeToSearch == "LogoutAccount")
        {
          logoutAccount (*user);
        }
      else if (typeToSearch == "CreateGameLobby")
        {
          createGameLobby (objectAsString, user, gameLobbys);
        }
      else if (typeToSearch == "JoinGameLobby")
        {
          joinGameLobby (objectAsString, user, gameLobbys);
        }
      else if (typeToSearch == "SetMaxUserSizeInCreateGameLobby")
        {
          setMaxUserSizeInCreateGameLobby (objectAsString, user, gameLobbys);
        }
      else if (typeToSearch == "GameOption")
        {
          setGameOption (objectAsString, user, gameLobbys);
        }
      else if (typeToSearch == "LeaveGameLobby")
        {
          leaveGameLobby (user, gameLobbys);
        }
      else if (typeToSearch == "RelogTo")
        {
          relogTo (objectAsString, user, gameLobbys, gameMachines);
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
          createGame (user, gameLobbys, gameMachines, io_context);
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
      else if (typeToSearch == "SetTimerOption")
        {
          setTimerOption (objectAsString, user, gameLobbys);
        }
      else
        {
          std::cout << "UnhandledMessage|{\"message\": \"" << msg << "\"}" << std::endl;
          user->msgQueue.push_back ("UnhandledMessage|{\"message\": \"" + msg + "\"}");
        }
    }
  else
    {
      std::cout << "UnhandledMessage|{\"message\": \"" << msg << "\"}" << std::endl;
      user->msgQueue.push_back ("UnhandledMessage|{\"message\": \"" + msg + "\"}");
    }
  if (user->accountName)
    {
      //  TODO this is a workaround because calling gameOver in the statemachine leads to a crash in the destructor
      //  TODO the statemachine should call gameOver

      removeGameIfItIsOver (user->accountName.value (), gameMachines);
    }
  co_return;
}

boost::asio::awaitable<void>
createAccountAndLogin (std::string objectAsString, boost::asio::io_context &io_context, std::shared_ptr<User> user, boost::asio::thread_pool &pool)
{
  auto createAccountObject = stringToObject<shared_class::CreateAccount> (objectAsString);
  soci::session sql (soci::sqlite3, databaseName);
  if (confu_soci::findStruct<database::Account> (sql, "accountName", createAccountObject.accountName))
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "account already created" }));
      co_return;
    }
  else
    {
      auto hashedPw = co_await async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable);
      if (user->ignoreCreateAccount)
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "Canceled by User Request" }));
          co_return;
        }
      else
        {
          if (auto account = database::createAccount (createAccountObject.accountName, hashedPw))
            {
              user->accountName = account->accountName;
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountSuccess{ createAccountObject.accountName }));
              co_return;
            }
          else
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "account already created" }));
              co_return;
            }
        }
    }
}

boost::asio::awaitable<void>
loginAccount (std::string objectAsString, boost::asio::io_context &io_context, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, boost::asio::thread_pool &pool, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines)
{
  auto loginAccountObject = stringToObject<shared_class::LoginAccount> (objectAsString);
  soci::session sql (soci::sqlite3, databaseName);
  if (auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName))
    {
      if (std::find_if (users.begin (), users.end (), [accountName = account->accountName] (auto const &u) { return accountName == u->accountName; }) != users.end ())
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Account already logged in" }));
          co_return;
        }
      else
        {
          if (co_await async_check_hashed_pw (pool, io_context, account->password, loginAccountObject.password, boost::asio::use_awaitable))
            {
              if (user->ignoreLogin)
                {
                  user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Canceled by User Request" }));
                  co_return;
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
                      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::WantToRelog{ loginAccountObject.accountName, "Create Game Lobby" }));
                      co_return;
                    }
                  else if (auto gameWithUser = ranges::find_if (gameMachines,
                                                                [accountName = user->accountName] (auto const &gameMachine) {
                                                                  auto accountNames = std::vector<std::string>{};
                                                                  ranges::transform (gameMachine.getGameUsers (), ranges::back_inserter (accountNames), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
                                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                                });
                           gameWithUser != gameMachines.end ())
                    {
                      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::WantToRelog{ loginAccountObject.accountName, "Back To Game" }));
                      co_return;
                    }
                  else
                    {
                      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountSuccess{ loginAccountObject.accountName }));
                      co_return;
                    }
                }
            }
          else
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Incorrect Username or Password" }));
              co_return;
            }
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Incorrect username or password" }));
      co_return;
    }
}

void
logoutAccount (User &user)
{
  user.accountName = {};
  user.msgQueue.clear ();
  user.communicationChannels.clear ();
  user.ignoreLogin = false;
  user.ignoreCreateAccount = false;
  user.msgQueue.push_back (objectToStringWithObjectName (shared_class::LogoutAccountSuccess{}));
}

void
broadCastMessage (std::string const &objectAsString, std::list<std::shared_ptr<User>> &users, User &sendingUser)
{
  auto broadCastMessageObject = stringToObject<shared_class::BroadCastMessage> (objectAsString);
  if (sendingUser.accountName)
    {
      for (auto &user : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountName = sendingUser.accountName] (auto const &user) { return user->communicationChannels.find (channel) != user->communicationChannels.end (); }))
        {
          soci::session sql (soci::sqlite3, databaseName);
          auto message = shared_class::Message{ sendingUser.accountName.value (), broadCastMessageObject.channel, broadCastMessageObject.message };
          user->msgQueue.push_back (objectToStringWithObjectName (std::move (message)));
        }
      return;
    }
  else
    {
      sendingUser.msgQueue.push_back (objectToStringWithObjectName (shared_class::BroadCastMessageError{ broadCastMessageObject.channel, "account not logged in" }));
      return;
    }
}

void
joinChannel (std::string const &objectAsString, User &user)
{
  auto joinChannelObject = stringToObject<shared_class::JoinChannel> (objectAsString);
  if (user.accountName)
    {
      user.communicationChannels.insert (joinChannelObject.channel);
      user.msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinChannelSuccess{ joinChannelObject.channel }));
      return;
    }
  else
    {
      user.msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinChannelError{ joinChannelObject.channel, { "user not logged in" } }));
      return;
    }
}

void
leaveChannel (std::string const &objectAsString, User &user)
{
  auto leaveChannelObject = stringToObject<shared_class::LeaveChannel> (objectAsString);
  if (user.accountName)
    {
      if (user.communicationChannels.erase (leaveChannelObject.channel))
        {
          user.msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveChannelSuccess{ leaveChannelObject.channel }));
          return;
        }
      else
        {
          user.msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveChannelError{ leaveChannelObject.channel, { "channel not found" } }));
          return;
        }
    }
  else
    {
      user.msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveChannelError{ leaveChannelObject.channel, { "user not logged in" } }));
      return;
    }
}

std::optional<shared_class::GameOptionError>
errorInGameOption (durak::GameOption const &gameOption)
{
  if (gameOption.customCardDeck)
    {
      if (gameOption.customCardDeck->empty ())
        {
          return shared_class::GameOptionError{ "Empty Custom Deck is not allowed" };
        }
      else
        {
          return std::nullopt;
        }
    }
  else
    {
      if (gameOption.maxCardValue < 1)
        {
          return shared_class::GameOptionError{ "Max Card Value must be greater than 0" };
        }
      else
        {
          return std::nullopt;
        }
    }
}

void
createGame (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines, boost::asio::io_context &io_context)
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
          if (gameLobbyWithUser->accountNames ().size () >= 2)
            {
              if (auto gameOptionError = errorInGameOption (gameLobbyWithUser->gameOption))
                {
                  user->msgQueue.push_back (objectToStringWithObjectName (gameOptionError.value ()));
                }
              else
                {
                  ranges::for_each (gameLobbyWithUser->_users, [] (auto &_user) { _user->msgQueue.push_back (objectToStringWithObjectName (shared_class::StartGame{})); });
                  auto names = std::vector<std::string>{};
                  ranges::transform (gameLobbyWithUser->_users, ranges::back_inserter (names), [] (auto const &tempUser) { return tempUser->accountName.value (); });
                  auto game = durak::Game{ std::move (names), gameLobbyWithUser->gameOption };
                  auto &gameMachine = gameMachines.emplace_back (game, gameLobbyWithUser->_users, io_context, gameLobbyWithUser->timerOption, [accountName = user->accountName, &gameMachines] () {
                    if (auto gameWithUser = ranges::find_if (gameMachines,
                                                             [accountName] (auto const &gameMachine) {
                                                               auto accountNames = std::vector<std::string>{};
                                                               ranges::transform (gameMachine.getGameUsers (), ranges::back_inserter (accountNames), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
                                                               return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                             });
                        gameWithUser != gameMachines.end ())
                      {
                        gameMachines.erase (gameWithUser);
                      }
                  });
                  sendGameDataToAccountsInGame (gameMachine.getGame (), gameMachine.getGameUsers ());
                  gameLobbys.erase (gameLobbyWithUser);
                }
            }
          else
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ "You need atleast two user to create a game" }));
            }
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ "you need to be admin in a game lobby to start a game" }));
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ "Could not find a game lobby for the user" }));
    }
}

void
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
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameLobbyError{ { "account has already a game lobby with the name: " + gameLobbyWithUser->gameLobbyName () } }));
          return;
        }
      else
        {
          auto &newGameLobby = gameLobbys.emplace_back (GameLobby{ createGameLobbyObject.name, createGameLobbyObject.password });
          if (newGameLobby.tryToAddUser (user))
            {
              throw std::logic_error{ "user can not join lobby which he created" };
            }
          else
            {
              auto result = std::vector<std::string>{};
              auto usersInGameLobby = shared_class::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = newGameLobby.maxUserCount ();
              usersInGameLobby.name = newGameLobby.gameLobbyName ();
              usersInGameLobby.durakGameOption = newGameLobby.gameOption;
              ranges::transform (newGameLobby.accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
              user->msgQueue.push_back (objectToStringWithObjectName (usersInGameLobby));
              return;
            }
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameLobbyError{ { "lobby already exists with name: " + createGameLobbyObject.name } }));
      return;
    }
}

void
joinGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto joinGameLobbyObject = stringToObject<shared_class::JoinGameLobby> (objectAsString);
  if (auto gameLobby = ranges::find_if (gameLobbys, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.gameLobbyName () == gameLobbyName && _gameLobby.gameLobbyPassword () == lobbyPassword; }); gameLobby != gameLobbys.end ())
    {
      if (auto error = gameLobby->tryToAddUser (user))
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ joinGameLobbyObject.name, error.value () }));
          return;
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
          auto usersInGameLobby = shared_class::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobby->maxUserCount ();
          usersInGameLobby.name = gameLobby->gameLobbyName ();
          usersInGameLobby.durakGameOption = gameLobby->gameOption;
          ranges::transform (gameLobby->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
          gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
          gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::SetTimerOption{ gameLobby->timerOption.timerType, boost::numeric_cast<int> (gameLobby->timerOption.timeAtStart.count ()), boost::numeric_cast<int> (gameLobby->timerOption.timeForEachRound.count ()) }));
          return;
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ joinGameLobbyObject.name, "wrong password name combination or lobby does not exists" }));
      return;
    }
}

void
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
      if (gameLobbyWithAccount->gameLobbyAdminAccountName () == user->accountName)
        {
          if (auto errorMessage = gameLobbyWithAccount->setMaxUserCount (setMaxUserSizeInCreateGameLobbyObject.maxUserSize))
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ errorMessage.value () }));
              return;
            }
          else
            {
              gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::MaxUserSizeInCreateGameLobby{ setMaxUserSizeInCreateGameLobbyObject.maxUserSize }));
              return;
            }
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ "you need to be admin in a game lobby to change the user size" }));
          return;
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ "could not find a game lobby for account" }));
      return;
    }
}

void
setGameOption (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto gameOption = stringToObject<durak::GameOption> (objectAsString);
  auto accountNameToSearch = user->accountName.value ();
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbys,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (gameLobbyWithAccount->gameLobbyAdminAccountName () == user->accountName)
        {
          gameLobbyWithAccount->gameOption = gameOption;
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (gameOption));
          return;
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::GameOptionError{ "you need to be admin in the create game lobby to change game option" }));
          return;
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::GameOptionError{ "could not find a game lobby for account" }));
      return;
    }
}

void
setTimerOption (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto setTimerOptionObject = stringToObject<shared_class::SetTimerOption> (objectAsString);
  auto accountNameToSearch = user->accountName.value ();
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbys,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (gameLobbyWithAccount->gameLobbyAdminAccountName () == user->accountName)
        {
          using namespace std::chrono;
          gameLobbyWithAccount->timerOption = TimerOption{ setTimerOptionObject.timerType, seconds (setTimerOptionObject.timeAtStartInSeconds), seconds (setTimerOptionObject.timeForEachRoundInSeconds) };
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (setTimerOptionObject));
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetTimerOptionError{ "you need to be admin in a game lobby to change the timer option" }));
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetTimerOptionError{ "could not find a game lobby for account" }));
    }
}

void
leaveGameLobby (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbys,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbys.end ())
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
          usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
          ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
        }
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveGameLobbySuccess{}));
      return;
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveGameLobbyError{ "could not remove user from lobby user not found in lobby" }));
      return;
    }
}

void
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
          usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
          ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetTimerOption{ gameLobbyWithAccount->timerOption.timerType, boost::numeric_cast<int> (gameLobbyWithAccount->timerOption.timeAtStart.count ()), boost::numeric_cast<int> (gameLobbyWithAccount->timerOption.timeForEachRound.count ()) }));
          user->msgQueue.push_back (objectToStringWithObjectName (usersInGameLobby));
          return;
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
              usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
              ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
              gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
              return;
            }
        }
    }
  else if (auto gameWithUser = ranges::find_if (gameMachines,
                                                [accountName = user->accountName] (auto const &gameMachine) {
                                                  auto accountNames = std::vector<std::string>{};
                                                  ranges::transform (gameMachine.getGameUsers (), ranges::back_inserter (accountNames), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                });
           gameWithUser != gameMachines.end ())
    {
      if (relogToObject.wantsToRelog)
        {
          gameWithUser->relogUser (user);
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::RelogToGameSuccess{}));
          auto gameData = gameWithUser->getGame ().getGameData ();
          if (auto playerRelog = ranges::find_if (gameData.players, [accountName = user->accountName.value ()] (auto const &player) { return player.name == accountName; }); playerRelog != gameData.players.end ())
            {
              ranges::sort (playerRelog->cards, [] (auto const &card1, auto const &card2) { return card1.value () < card2.value (); });
              user->msgQueue.push_back (objectToStringWithObjectName (filterGameDataByAccountName (gameData, user->accountName.value ())));
            }
          auto playerRole = gameWithUser->getGame ().getRoleForName (user->accountName.value ());
          gameWithUser->durakStateMachine.process_event (userRelogged{});
          switch (playerRole)
            {
            case durak::PlayerRole::attack:
              {
                gameWithUser->durakStateMachine.process_event (attackRelog{});
                break;
              }
            case durak::PlayerRole::defend:
              {
                gameWithUser->durakStateMachine.process_event (defendRelog{});
                break;
              }
            case durak::PlayerRole::assistAttacker:
              {
                gameWithUser->durakStateMachine.process_event (assistRelog{});
                break;
              }
            case durak::PlayerRole::waiting:
              {
                gameWithUser->durakStateMachine.process_event (waitingRelog{});
                break;
              }
            }
        }
      else
        {
          gameWithUser->durakStateMachine.process_event (leaveGame{ user->accountName.value () });
          gameMachines.erase (gameWithUser);
        }
    }
  else if (relogToObject.wantsToRelog)
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
      return;
    }
  return;
}

void
durakAttack (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakAttackObject = stringToObject<shared_class::DurakAttack> (objectAsString);
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
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
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
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
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
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
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
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
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
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
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
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
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (leaveGame{ user->accountName.value () });
      gameMachines.erase (gameMachine);
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakLeaveGameError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}
