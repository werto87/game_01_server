#include "src/server/game.hxx"
#include <boost/optional/optional.hpp>
#include <durak/card.hxx>
#include <durak/gameData.hxx>
#include <range/v3/all.hpp>
#include <vector>

Game::Game (GameLobby &&gameLobby) : durakGame{ gameLobby.accountNames () } { _users.swap (gameLobby._users); }
