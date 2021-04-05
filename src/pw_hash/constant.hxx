#ifndef EB95EBA8_1297_44AA_9C1C_A1AB22A88B30
#define EB95EBA8_1297_44AA_9C1C_A1AB22A88B30

#include <sodium.h>

#ifdef DEBUG
auto const hash_memory = crypto_pwhash_argon2id_MEMLIMIT_MIN;
auto const hash_opt = crypto_pwhash_argon2id_OPSLIMIT_MIN;
#else
auto const hash_memory = crypto_pwhash_argon2id_MEMLIMIT_MODERATE;
auto const hash_opt = crypto_pwhash_argon2id_OPSLIMIT_MODERATE;
#endif

#endif /* EB95EBA8_1297_44AA_9C1C_A1AB22A88B30 */
