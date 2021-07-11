#include "src/database/database.hxx"
#include "src/server/server.hxx"
#include <boost/bind/bind.hpp>
#include <boost/json/src.hpp>
#include <exception>
#include <iostream>
#include <sodium.h>
#include <stdexcept>

auto const DEFAULT_PORT = u_int16_t{ 55555 };

int
main (int argc, char *argv[])
{
#ifdef DEBUG
  std::cout << "DEBUG" << std::endl;
#else
  std::cout << "NO DEBUG" << std::endl;
#endif
  auto port = u_int16_t{ 0 };
  if (argc < 2)
    {
      port = u_int16_t{ DEFAULT_PORT };
      std::cout << "no port specified in commandline using default port: " << port << std::endl;
    }
  else
    {
      port = boost::lexical_cast<unsigned short> (argv[1]);
    }
  try
    {
      if (sodium_init () < 0)
        {
          std::cout << "sodium_init => 0" << std::endl;
          std::terminate ();
          /* panic! the library couldn't be initialized, it is not safe to use */
        }
      database::createEmptyDatabase ();
      database::createTables ();
      using namespace boost::asio;
      io_context io_context (1);
      signal_set signals (io_context, SIGINT, SIGTERM);
      signals.async_wait ([&] (auto, auto) { io_context.stop (); });
      thread_pool pool{ 2 };
      auto server = Server{ io_context, pool, { ip::tcp::v4 (), port } };
      co_spawn (
          io_context, [&server] { return server.listener (); }, detached);
      io_context.run ();
    }
  catch (std::exception &e)
    {
      std::printf ("Exception: %s\n", e.what ());
    }
  return 0;
}