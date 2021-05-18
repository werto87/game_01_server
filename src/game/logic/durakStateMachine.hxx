#ifndef EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#define EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#include "durak/game.hxx"
#include "durak/gameData.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/server/user.hxx"
#include <bits/ranges_algo.h>
#include <boost/sml.hpp>
#include <iostream>
#include <optional>
#include <queue>
#include <ranges>
#include <vector>
struct my_logger
{
  template <class SM, class TEvent>
  void
  log_process_event (const TEvent &)
  {
    // if(std::string{sml::aux::get_type_name<TEvent> ()}!=std::string{"draw"}){
    //   printf ("[%s][process_event] %s\n", sml::aux::get_type_name<SM> (), sml::aux::get_type_name<TEvent> ());
    // }
  }

  template <class SM, class TGuard, class TEvent>
  void
  log_guard (const TGuard &, const TEvent &, bool /*result*/)
  {
    // printf ("[%s][guard] %s %s %s\n", sml::aux::get_type_name<SM> (), sml::aux::get_type_name<TGuard> (), sml::aux::get_type_name<TEvent> (), (result ? "[OK]" : "[Reject]"));
  }

  template <class SM, class TAction, class TEvent>
  void
  log_action (const TAction &, const TEvent &)
  {
    // printf ("[%s][action] %s %s\n", sml::aux::get_type_name<SM> (), sml::aux::get_type_name<TAction> (), sml::aux::get_type_name<TEvent> ());
  }

  template <class SM, class TSrcState, class TDstState>
  void
  log_state_change (const TSrcState &src, const TDstState &dst)
  {
    printf ("[%s] %s -> %s\n", boost::sml::aux::get_type_name<SM> (), src.c_str (), dst.c_str ());
  }
};

// TODO think about 2 player solution maybe allways pass for assist??

// care const& is not &. AttackAndAssistAnswer &attackAndAssistAnswer is another object than AttackAndAssistAnswer const&attackAndAssistAnswer
auto const hasNoAnswerFromAttack = [] (AttackAndAssistAnswer &attackAndAssistAnswer) { return not attackAndAssistAnswer.attack.has_value (); };
auto const hasNoAnswerFromAssist = [] (AttackAndAssistAnswer &attackAndAssistAnswer) { return not attackAndAssistAnswer.assist.has_value (); };

auto const allCardsBeaten = [] (durak::Game &game) { return game.countOfNotBeatenCardsOnTable () == 0 && game.getAttackStarted (); };

auto const isDefendingPlayer = [] (defend const &defendEv, durak::Game &game) { return game.getRoleForName (defendEv.playerName) == durak::PlayerRole::defend; };

auto const setAttackAnswer = [] (attackAnswer const &attackAnswerEv, AttackAndAssistAnswer &attackAndAssistAnswer) { attackAndAssistAnswer.attack = attackAnswerEv.accept; };
auto const setAssistAnswer = [] (assistAnswer const &assistAnswerEv, AttackAndAssistAnswer &attackAndAssistAnswer) { attackAndAssistAnswer.assist = assistAnswerEv.accept; };

auto const checkData = [] (PassAttackAndAssist &passAttackAndAssist, durak::Game &game, boost::sml::back::process<askDef> process_event) {
  if (auto attackingPlayer = game.getAttackingPlayer (); not attackingPlayer || attackingPlayer->getCards ().empty ())
    {
      passAttackAndAssist.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); not assistingPlayer || assistingPlayer->getCards ().empty ())
    {
      passAttackAndAssist.assist = true;
    }
  if (passAttackAndAssist.assist && passAttackAndAssist.attack && game.countOfNotBeatenCardsOnTable () == 0)
    {
      process_event (askDef{});
    }
};

auto const checkAttackAndAssistAnswer = [] (AttackAndAssistAnswer &attackAndAssistAnswer, durak::Game &game, boost::sml::back::process<chill> process_event) {
  if (auto attackingPlayer = game.getAttackingPlayer (); not attackingPlayer || attackingPlayer->getCards ().empty ())
    {
      attackAndAssistAnswer.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); not assistingPlayer || assistingPlayer->getCards ().empty ())
    {
      attackAndAssistAnswer.assist = true;
    }
  if (attackAndAssistAnswer.attack && attackAndAssistAnswer.assist && attackAndAssistAnswer.attack.value () && attackAndAssistAnswer.assist.value ())
    {
      game.nextRound (true);
      // TODO send info to all player
      process_event (chill{});
    }
};

auto const startAskDef = [] (durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      if (auto player = std::ranges::find_if (users, [&defendingPlayer] (std::shared_ptr<User> const &user) { return user->accountName.value () == defendingPlayer->id; }); player != users.end ())
        {
          // TODO push ask def msg
          // player->get()->msgQueue.push_back()
        }
    }
};

auto const startAskAttackAndAssist = [] (AttackAndAssistAnswer &attackAndAssistAnswer, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto attackingPlayer = game.getAttackingPlayer (); not attackingPlayer->getCards ().empty ())
    {
      if (auto player = std::ranges::find_if (users, [&attackingPlayer] (std::shared_ptr<User> const &user) { return user->accountName.value () == attackingPlayer->id; }); player != users.end ())
        {
          // TODO push ask attack msg
          // player->get()->msgQueue.push_back()
        }
    }
  else
    {
      attackAndAssistAnswer.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); not assistingPlayer->getCards ().empty ())
    {
      if (auto player = std::ranges::find_if (users, [&assistingPlayer] (std::shared_ptr<User> const &user) { return user->accountName.value () == assistingPlayer->id; }); player != users.end ())
        {
          // TODO push ask assist msg
          // player->get()->msgQueue.push_back()
        }
    }
  else
    {
      attackAndAssistAnswer.assist = true;
    }
};

auto const setAttackPass = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist.assist = true; };

auto const setAssistPass = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist.attack = true; };

auto const handleDefendSuccess = [] (durak::Game &game) {
  game.nextRound (false);
  // TODO send info to all player
};

auto const resetPassStateMachineData = [] (PassAttackAndAssist &passAttackAndAssist, AttackAndAssistAnswer &attackAndAssistAnswer) {
  passAttackAndAssist = PassAttackAndAssist{};
  attackAndAssistAnswer = AttackAndAssistAnswer{};
};

auto const resetPassAttackAndAssist = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist = PassAttackAndAssist{}; };

auto const setRewokePassAssist = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist.assist = false; };
auto const setRewokePassAttack = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist.attack = false; };

auto const doAttack = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  // TODO send results to users msg queue
  // TODO if cards get player rewoke pass from everyone
  auto playerRole = game.getRoleForName (attackEv.playerName);
  if (playerRole == durak::PlayerRole::attack)
    {
      if (game.getAttackStarted ())
        {
          if (game.playerAssists (playerRole, attackEv.cards))
            {
              // TODO send message attack success
              passAttackAndAssist = PassAttackAndAssist{};
            }
          else
            {
              // TODO send message attack error
            }
        }
      else
        {
          if (game.playerStartsAttack (attackEv.cards))
            {
              // TODO send message attack success
              passAttackAndAssist = PassAttackAndAssist{};
            }
          else
            {
              // TODO send message attack error
            }
        }
    }
  else if (playerRole == durak::PlayerRole::assistAttacker)
    {
      if (game.getAttackStarted ())
        {
          if (game.playerAssists (playerRole, attackEv.cards))
            {
              // TODO send message attack success
              passAttackAndAssist = PassAttackAndAssist{};
            }
          else
            {
              // TODO send message attack error
            }
        }
    }
};

auto const doDefend = [] (defend const &defendEv, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  // TODO send results to users msg queue
  game.playerDefends (defendEv.cardToBeat, defendEv.card);
};

struct PassMachine
{
  auto
  operator() () const
  {
    // TODO rewoke pass when user plays card
    using namespace boost::sml;
    return make_transition_table (
        // clang-format off
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
* state<Chill>                  + on_entry<_>                                             / resetPassStateMachineData           
, state<Chill>                  + event<askDef>                                                                                       = state<AskDef>
, state<Chill>                  + event<askAttackAndAssist>                                                                           = state<AskAttackAndAssist>
, state<Chill>                  + event<attackPass>             [ allCardsBeaten ]        /(setAttackPass,checkData)
, state<Chill>                  + event<assistPass>             [ allCardsBeaten ]        /(setAssistPass,checkData)
, state<Chill>                  + event<defendPass>                                                                                   = state<AskAttackAndAssist>
, state<Chill>                  + event<attack>                                           / doAttack 
, state<Chill>                  + event<defend>                 [isDefendingPlayer]       / doDefend 
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskDef>                 + on_entry<_>                                             / startAskDef
, state<AskDef>                 + event<defendAnswerYes>                                                                                = state<AskAttackAndAssist>
, state<AskDef>                 + event<defendAnswerNo>                                   / handleDefendSuccess                         = state<Chill>
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskAttackAndAssist>     + on_entry<_>                                             / startAskAttackAndAssist
, state<AskAttackAndAssist>     + event<attackAnswer>           [ hasNoAnswerFromAttack ] /(setAttackAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<assistAnswer>           [ hasNoAnswerFromAssist ] /(setAssistAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<chill>                                                                                          =state<Chill>
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
// clang-format on   
    );
  }
};

typedef boost::sml::sm<PassMachine, boost::sml::logger<my_logger>,boost::sml::process_queue<std::queue>> DurakStateMachine;



#endif /* EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4 */
