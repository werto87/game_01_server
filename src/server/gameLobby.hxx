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

  GameLobby () = default;
  GameLobby (std::string name, std::string password) : name{ std::move (name) }, password (std::move (password)) {}

  enum struct LobbyAdminType
  {
    FirstUserInLobbyUsers,
    MatchmakingSystem
  };

  std::string const &gameLobbyName () const;
  std::string const &gameLobbyPassword () const;
  size_t maxUserCount () const;
  std::optional<std::string> setMaxUserCount (size_t userMaxCount);

  std::vector<std::string> accountNames () const;
  bool isGameLobbyAdmin (std::string const &accountName) const;
  std::optional<std::string> tryToAddUser (std::shared_ptr<User> const &user);
  bool tryToRemoveUser (std::string const &userWhoTriesToRemove, std::string const &userToRemoveName);
  bool removeUser (std::shared_ptr<User> const &user);
  bool tryToRemoveAdminAndSetNewAdmin ();
  void sendToAllAccountsInGameLobby (std::string const &message);
  size_t accountCount ();
  void relogUser (std::shared_ptr<User> &user);
  void startTimerToAcceptTheInvite (boost::asio::io_context &io_context, std::function<void ()> gameOverCallback);
  void cancelTimer();

  std::vector<std::shared_ptr<User>> _users{};
  durak::GameOption gameOption{};
  TimerOption timerOption{};
  std::vector<std::shared_ptr<User>> readyUsers{};
  LobbyAdminType lobbyAdminType = LobbyAdminType::FirstUserInLobbyUsers;
  std::optional<std::string> name{};
  std::string password{};

private:
  std::shared_ptr<boost::asio::system_timer> _timer;
  size_t _maxUserCount{ 2 };
};

#endif /* DBE82937_D6AB_4777_A3C8_A62B68300AA3 */
