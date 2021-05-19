#ifndef EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#define EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#include "durak/game.hxx"
#include "durak/gameData.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/server/user.hxx"
#include "src/util.hxx"
#include <bits/ranges_algo.h>
#include <boost/sml.hpp>
#include <fmt/format.h>
#include <game_01_shared_class/serialization.hxx>
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

// care const& is not &. AttackAndAssistAnswer &attackAndAssistAnswer is another object than AttackAndAssistAnswer const&attackAndAssistAnswer

auto const allCardsBeaten = [] (durak::Game &game) {
  //
  return game.countOfNotBeatenCardsOnTable () == 0 && game.getAttackStarted ();
};

auto const isDefendingPlayer = [] (defend const &defendEv, durak::Game &game) { return game.getRoleForName (defendEv.playerName) == durak::PlayerRole::defend; };

auto const setAttackAnswer = [] (attackPass const &attackPassEv, AttackAndAssistAnswer &attackAndAssistAnswer, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto userItr = std::ranges::find_if (users, [accountName = attackPassEv.playerName] (std::shared_ptr<User> &user) { return user->accountName.value () == accountName; }); userItr != users.end ())
    {
      auto &sendingUserMsgQueue = userItr->get ()->msgQueue;
      if (not attackAndAssistAnswer.attack)
        {
          if (game.getRoleForName (attackPassEv.playerName) == durak::PlayerRole::attack)
            {
              attackAndAssistAnswer.attack = true;
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsSuccess{}));
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ .error = "role is not attack" }));
            }
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ .error = "pass already set" }));
        }
    }
};
auto const setAssistAnswer = [] (assistPass const &assistPassEv, AttackAndAssistAnswer &attackAndAssistAnswer, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto userItr = std::ranges::find_if (users, [accountName = assistPassEv.playerName] (std::shared_ptr<User> &user) { return user->accountName.value () == accountName; }); userItr != users.end ())
    {
      auto &sendingUserMsgQueue = userItr->get ()->msgQueue;
      if (not attackAndAssistAnswer.assist)
        {
          if (game.getRoleForName (assistPassEv.playerName) == durak::PlayerRole::assistAttacker)
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsSuccess{}));
              attackAndAssistAnswer.assist = true;
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ .error = "role is not assist" }));
            }
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ .error = "pass already set" }));
        }
    }
};

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

auto const checkAttackAndAssistAnswer = [] (AttackAndAssistAnswer &attackAndAssistAnswer, durak::Game &game, std::vector<std::shared_ptr<User>> &users, boost::sml::back::process<chill> process_event) {
  if (auto attackingPlayer = game.getAttackingPlayer (); not attackingPlayer || attackingPlayer->getCards ().empty ())
    {
      attackAndAssistAnswer.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); not assistingPlayer || assistingPlayer->getCards ().empty ())
    {
      attackAndAssistAnswer.assist = true;
    }
  if (attackAndAssistAnswer.attack && attackAndAssistAnswer.assist)
    {
      game.nextRound (true);
      sendGameDataToAccountsInGame (game, users);
      process_event (chill{});
    }
};

auto const startAskDef = [] (durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      if (auto player = std::ranges::find_if (users, [&defendingPlayer] (std::shared_ptr<User> const &user) { return user->accountName.value () == defendingPlayer->id; }); player != users.end ())
        {
          player->get ()->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCards{}));
        }
    }
};

auto const startAskAttackAndAssist = [] (AttackAndAssistAnswer &attackAndAssistAnswer, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto attackingPlayer = game.getAttackingPlayer (); attackingPlayer && not attackingPlayer->getCards ().empty ())
    {
      if (auto player = std::ranges::find_if (users, [&attackingPlayer] (std::shared_ptr<User> const &user) { return user->accountName.value () == attackingPlayer->id; }); player != users.end ())
        {
          player->get ()->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
        }
    }
  else
    {
      attackAndAssistAnswer.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); assistingPlayer && not assistingPlayer->getCards ().empty ())
    {
      if (auto player = std::ranges::find_if (users, [&assistingPlayer] (std::shared_ptr<User> const &user) { return user->accountName.value () == assistingPlayer->id; }); player != users.end ())
        {
          player->get ()->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
        }
    }
  else
    {
      attackAndAssistAnswer.assist = true;
    }
};

auto const setAttackPass = [] (attackPass const &attackPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto userItr = std::ranges::find_if (users, [accountName = attackPassEv.playerName] (std::shared_ptr<User> &user) { return user->accountName.value () == accountName; }); userItr != users.end ())
    {
      auto &sendingUserMsgQueue = userItr->get ()->msgQueue;
      auto playerRole = game.getRoleForName (attackPassEv.playerName);
      if (game.countOfNotBeatenCardsOnTable () == 0)
        {
          if (playerRole == durak::PlayerRole::attack)
            {
              passAttackAndAssist.attack = true;
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassSuccess{}));
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassError{ .error = "account role is not attack: " + attackPassEv.playerName }));
            }
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassError{ .error = "there are not beaten cards on the table" }));
        }
    }
};

auto const setAssistPass = [] (assistPass const &assistPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto userItr = std::ranges::find_if (users, [accountName = assistPassEv.playerName] (std::shared_ptr<User> &user) { return user->accountName.value () == accountName; }); userItr != users.end ())
    {
      auto &sendingUserMsgQueue = userItr->get ()->msgQueue;
      auto playerRole = game.getRoleForName (assistPassEv.playerName);
      if (game.countOfNotBeatenCardsOnTable () == 0)
        {
          if (playerRole == durak::PlayerRole::assistAttacker)
            {
              passAttackAndAssist.assist = true;
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassSuccess{}));
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassError{ .error = "account role is not assist: " + assistPassEv.playerName }));
            }
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassError{ .error = "there are not beaten cards on the table" }));
        }
    }
};

auto const handleDefendSuccess = [] (durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  game.nextRound (false);
  sendGameDataToAccountsInGame (game, users);
};

auto const resetPassStateMachineData = [] (PassAttackAndAssist &passAttackAndAssist, AttackAndAssistAnswer &attackAndAssistAnswer) {
  passAttackAndAssist = PassAttackAndAssist{};
  attackAndAssistAnswer = AttackAndAssistAnswer{};
};

auto const resetPassAttackAndAssist = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist = PassAttackAndAssist{}; };

auto const setRewokePassAssist = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist.assist = false; };
auto const setRewokePassAttack = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist.attack = false; };

auto const doAttack = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto userItr = std::ranges::find_if (users, [accountName = attackEv.playerName] (std::shared_ptr<User> &user) { return user->accountName.value () == accountName; }); userItr != users.end ())
    {
      auto &sendingUserMsgQueue = userItr->get ()->msgQueue;
      auto playerRole = game.getRoleForName (attackEv.playerName);
      if (playerRole == durak::PlayerRole::attack)
        {
          if (game.getAttackStarted ())
            {
              if (game.playerAssists (playerRole, attackEv.cards))
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackSuccess{}));
                  sendGameDataToAccountsInGame (game, users);
                  passAttackAndAssist = PassAttackAndAssist{};
                }
              else
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackError{ .error = "not allowed to play cards" }));
                }
            }
          else
            {
              if (game.playerStartsAttack (attackEv.cards))
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackSuccess{}));
                  sendGameDataToAccountsInGame (game, users);
                  passAttackAndAssist = PassAttackAndAssist{};
                }
              else
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackError{ .error = "not allowed to play cards" }));
                }
            }
        }
      else if (playerRole == durak::PlayerRole::assistAttacker)
        {
          if (game.getAttackStarted ())
            {
              if (game.playerAssists (playerRole, attackEv.cards))
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackSuccess{}));
                  sendGameDataToAccountsInGame (game, users);
                  passAttackAndAssist = PassAttackAndAssist{};
                }
              else
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackError{ .error = "not allowed to play cards" }));
                }
            }
        }
    }
};

auto const doDefend = [] (defend const &defendEv, durak::Game &game, std::vector<std::shared_ptr<User>> &users) {
  if (auto userItr = std::ranges::find_if (users, [accountName = defendEv.playerName] (std::shared_ptr<User> &user) { return user->accountName.value () == accountName; }); userItr != users.end ())
    {
      auto &sendingUserMsgQueue = userItr->get ()->msgQueue;
      auto playerRole = game.getRoleForName (defendEv.playerName);
      if (playerRole == durak::PlayerRole::defend)
        {
          if (game.playerDefends (defendEv.cardToBeat, defendEv.card))
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendSuccess{}));
              sendGameDataToAccountsInGame (game, users);
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendError{ .error = "Error while defending " + fmt::format ("CardToBeat: {},{} vs. Card: {},{}", defendEv.cardToBeat.value, magic_enum::enum_name (defendEv.cardToBeat.type), defendEv.card.value, magic_enum::enum_name (defendEv.card.type)) }));
            }
        }
    }
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
, state<Chill>                  + event<attackPass>                                       /(setAttackPass,checkData)
, state<Chill>                  + event<assistPass>                                       /(setAssistPass,checkData)
, state<Chill>                  + event<defendPass>                                                                                   = state<AskAttackAndAssist>
, state<Chill>                  + event<attack>                                           / doAttack 
, state<Chill>                  + event<defend>                 [isDefendingPlayer]       / doDefend 
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskDef>                 + on_entry<_>                                             / startAskDef
, state<AskDef>                 + event<defendAnswerYes>                                                                                = state<AskAttackAndAssist>
, state<AskDef>                 + event<defendAnswerNo>                                   / handleDefendSuccess                         = state<Chill>
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskAttackAndAssist>     + on_entry<_>                                             / startAskAttackAndAssist
, state<AskAttackAndAssist>     + event<attackPass>                                     /(setAttackAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<assistPass>                                     /(setAssistAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<attack>                                           / doAttack 
, state<AskAttackAndAssist>     + event<chill>                                                                                          =state<Chill>
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
// clang-format on   
    );
  }
};

typedef boost::sml::sm<PassMachine, boost::sml::logger<my_logger>,boost::sml::process_queue<std::queue>> DurakStateMachine;



#endif /* EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4 */