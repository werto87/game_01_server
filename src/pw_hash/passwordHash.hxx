#ifndef AD3B06C2_4AC7_438D_8907_4643053A4E7E
#define AD3B06C2_4AC7_438D_8907_4643053A4E7E
#include <string>

std::string pw_to_hash (std::string const &password);

bool check_hashed_pw (std::string const &hashedPassword, std::string const &password);

#endif /* AD3B06C2_4AC7_438D_8907_4643053A4E7E */
