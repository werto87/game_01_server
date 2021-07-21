#ifndef DBE82937_D6AB_4777_A3C8_A62B68300AA3
#define DBE82937_D6AB_4777_A3C8_A62B68300AA3

#include "durak/gameOption.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/server/user.hxx"
#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
struct GameLobby
{
  GameLobby (std::string name, std::string password) : _name{ std::move (name) }, _password (std::move (password)) {}

  std::string const &gameLobbyName () const;
  std::string const &gameLobbyPassword () const;
  bool hasPassword () const;
  size_t maxUserCount () const;
  std::optional<std::string> setMaxUserCount (size_t userMaxCount);

  std::vector<std::string> accountNames () const;
  std::string gameLobbyAdminAccountName () const;
  bool tryToAddUser (std::shared_ptr<User> const &user);
  bool tryToRemoveUser (std::string const &userWhoTriesToRemove, std::string const &userToRemoveName);
  bool removeUser (std::shared_ptr<User> const &user);
  bool tryToRemoveAdminAndSetNewAdmin ();
  void sendToAllAccountsInGameLobby (std::string const &message);
  size_t accountCount ();
  void relogUser (std::shared_ptr<User> &user);

  std::vector<std::shared_ptr<User>> _users{};
  durak::GameOption gameOption{};
  TimerOption timerOption{};

private:
  std::string _name{};
  std::string _password{};
  size_t _maxUserCount{ 1 };
};

#endif /* DBE82937_D6AB_4777_A3C8_A62B68300AA3 */
