#include "src/server/game.hxx"
#include <boost/optional/optional.hpp>
#include <durak/card.hxx>
#include <durak/gameData.hxx>
#include <ranges>
#include <vector>

Game::Game (GameLobby &&gameLobby) : durakGame{ gameLobby.accountNames () } { _users.swap (gameLobby._users); }
