#include "src/pw_hash/passwordHash.hxx"
#include <array>
#include <iostream>
#include <sodium.h>

std::string
pw_to_hash (std::string const &password)
{
  auto hashed_password = std::array<char, crypto_pwhash_STRBYTES>{};
  if (crypto_pwhash_str (hashed_password.begin (), password.data (), password.size (), 3, crypto_pwhash_MEMLIMIT_SENSITIVE / 32) != 0)
    {
      std::cout << "out of memory" << std::endl;
      std::terminate ();
    }
  auto result = std::string{};
  std::copy_if (hashed_password.begin (), hashed_password.end (), std::back_inserter (result), [] (auto value) { return value != 0; });
  return result;
}

bool
check_hashed_pw (std::string const &hashedPassword, std::string const &password)
{
  return crypto_pwhash_str_verify (hashedPassword.data (), password.data (), password.size ()) == 0;
}