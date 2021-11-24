#ifndef FA9CDEF0_BEE5_4919_910F_EC780C1C3C4C
#define FA9CDEF0_BEE5_4919_910F_EC780C1C3C4C

#include <durak/game.hxx>
#include <game_01_shared_class/serialization.hxx>

struct AllowedMoves
{
  std::optional<std::vector<shared_class::Move>> defend{};
  std::optional<std::vector<shared_class::Move>> attack{};
  std::optional<std::vector<shared_class::Move>> assist{};
};

#endif /* FA9CDEF0_BEE5_4919_910F_EC780C1C3C4C */
