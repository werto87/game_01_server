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
struct AttackAndAssistAnswer
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

#endif /* A30C538C_58C3_49A5_8853_24519FACCE71 */
