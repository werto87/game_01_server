#ifndef A30C538C_58C3_49A5_8853_24519FACCE71
#define A30C538C_58C3_49A5_8853_24519FACCE71
#include "durak/card.hxx"
#include <boost/optional.hpp>
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

struct askDef
{
};
struct askAttackAndAssist
{
};

struct attackPass
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

struct assistPass
{
};
struct defendPass
{
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
struct AttackAndAssistAnswer
{
  std::optional<bool> attack{};
  std::optional<bool> assist{};
};
struct attackAnswer
{
  bool accept{};
};
struct assistAnswer
{
  bool accept{};
};
struct defendAnswerYes
{
};
struct defendAnswerNo
{
};

#endif /* A30C538C_58C3_49A5_8853_24519FACCE71 */
