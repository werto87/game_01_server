#ifndef E18680A5_3B06_4019_A849_6CDB82D14796
#define E18680A5_3B06_4019_A849_6CDB82D14796
#include <boost/optional.hpp>
#include <string>
#include <vector>

std::vector<std::string> handleMessage (std::string const &msg);

boost::optional<std::string> createAccount (std::string const &msg);

boost::optional<std::string> createCharacter (std::string const &msg);

#endif /* E18680A5_3B06_4019_A849_6CDB82D14796 */
