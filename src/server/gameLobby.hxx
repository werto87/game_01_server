#ifndef DBE82937_D6AB_4777_A3C8_A62B68300AA3
#define DBE82937_D6AB_4777_A3C8_A62B68300AA3

#include <optional>
#include <string>
struct GameLobby
{

  std::string name{};
  std::optional<std::string> password{};

  // vector auf server vector<GameLobby>{}
};

#endif /* DBE82937_D6AB_4777_A3C8_A62B68300AA3 */
