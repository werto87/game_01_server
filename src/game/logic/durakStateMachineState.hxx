#ifndef A30C538C_58C3_49A5_8853_24519FACCE71
#define A30C538C_58C3_49A5_8853_24519FACCE71
#include "durak/card.hxx"
#include <boost/optional.hpp>
#include <chrono>
#include <optional>
#include <queue>
#include <string>

struct TempUser
{
  boost::optional<std::string> accountName{};
  std::deque<std::string> msgQueue{};
};

struct Chill
{
};
struct chill
{
};

struct AskDef
{
};
struct AskAttackAndAssist
{
};

struct askAttackAndAssist
{
};
struct askDef
{
};

struct attack
{
  std::string playerName{};
  std::vector<durak::Card> cards{};
};

struct defend
{
  std::string playerName{};
  durak::Card cardToBeat{};
  durak::Card card{};
};

struct attackPass
{
  std::string playerName{};
};

struct assistPass
{
  std::string playerName{};
};
struct defendPass
{
  std::string playerName{};
};

struct rewokePassAttack
{
};

struct rewokePassAssist
{
};

struct PassAttackAndAssist
{
  bool attack{};
  bool assist{};
};

struct defendAnswerYes
{
  std::string playerName{};
};
struct defendAnswerNo
{
  std::string playerName{};
};

struct leaveGame
{
  std::string accountName{};
};
struct defendRelog
{
};
struct attackRelog
{
};
struct assistRelog
{
};
struct waitingRelog
{
};

struct initTimer
{
};
struct nextRoundTimer
{
};
struct pauseTimer
{
  std::vector<std::string> playersToPause{};
};

struct resumeTimer
{
  std::vector<std::string> playersToResume{};
};
enum struct TimerType
{
  resetTimeOnNewRound,
  addTimeOnNewRound,
  noTimer
};

struct TimerOption
{
  TimerType timerType{};
  std::chrono::seconds timeAtStart{};
  std::chrono::seconds timeForEachRound{};
};

#endif /* A30C538C_58C3_49A5_8853_24519FACCE71 */
