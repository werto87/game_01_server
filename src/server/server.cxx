#include "src/server/server.hxx"
#include "src/logic/logic.hxx"
#include <boost/certify/https_verification.hpp>
#include <experimental/coroutine>

using namespace boost::beast;
using namespace boost::asio;
namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
Server::Server (boost::asio::io_context &io_context, boost::asio::thread_pool &pool, boost::asio::ip::tcp::endpoint const &endpoint) : _io_context{ io_context }, _pool{ pool }, _endpoint{ endpoint } {}

awaitable<std::string>
Server::my_read (SSLWebsocket &ws_)
{
  std::cout << "read" << std::endl;
  flat_buffer buffer;
  co_await ws_.async_read (buffer, use_awaitable);
  auto msg = buffers_to_string (buffer.data ());
  std::cout << "number of letters '" << msg.size () << "' msg: '" << msg << "'" << std::endl;
  co_return msg;
}

awaitable<void>
Server::readFromClient (std::list<std::shared_ptr<User>>::iterator user, SSLWebsocket &connection)
{
  try
    {
      for (;;)
        {
          // use this to simulate lag
          // auto timer = steady_timer (co_await this_coro::executor);
          // using namespace std::chrono_literals;
          // timer.expires_after (10s);
          // co_await timer.async_wait (use_awaitable);
          auto readResult = co_await my_read (connection);
          co_await handleMessage (readResult, _io_context, _pool, users, *user, gameLobbies, games);
        }
    }
  catch (std::exception &e)
    {
      removeUser (user);
      std::cout << "read Exception: " << e.what () << std::endl;
    }
}

bool
isRegistered (std::string const &accountName)
{
  soci::session sql (soci::sqlite3, databaseName);
  return confu_soci::findStruct<database::Account> (sql, "accountName", accountName).has_value ();
}

void
Server::removeUser (std::list<std::shared_ptr<User>>::iterator user)
{
  if (user->get ()->accountName && not isRegistered (user->get ()->accountName.value ()))
    {
      removeUserFromLobbyAndGame (*user, gameLobbies, games);
    }
  user->get ()->communicationChannels.clear ();
  user->get ()->ignoreLogin = false;
  user->get ()->ignoreCreateAccount = false;
  user->get ()->msgQueue.clear ();
  users.erase (user);
}

awaitable<void>
Server::writeToClient (std::shared_ptr<User> user, std::weak_ptr<SSLWebsocket> &connection)
{
  try
    {
      while (not connection.expired ())
        {
          // TODO this is polling because we check every 100 milli seconds.
          auto timer = steady_timer (co_await this_coro::executor);
          auto const waitForNewMessagesToSend = std::chrono::milliseconds{ 100 };
          timer.expires_after (waitForNewMessagesToSend);
          co_await timer.async_wait (use_awaitable);
          while (not user->msgQueue.empty () && not connection.expired ())
            {
              auto tmpMsg = std::move (user->msgQueue.front ());
              std::cout << "send msg to: " << user->accountName.value_or ("not logged in user") << " msg: " << tmpMsg << std::endl;
              user->msgQueue.pop_front ();
              co_await connection.lock ()->async_write (buffer (tmpMsg), use_awaitable);
            }
        }
    }
  catch (std::exception &e)
    {
      std::cout << "write Exception: " << e.what () << std::endl;
    }
}

awaitable<void>
Server::listener ()
{
  auto executor = co_await this_coro::executor;
  ip::tcp::acceptor acceptor (executor, _endpoint);
  net::ssl::context ctx (net::ssl::context::tls_server);
  ctx.set_verify_mode (ssl::context::verify_peer);
  ctx.set_default_verify_paths ();

#ifdef DEBUG
  auto const pathToChainFile = std::string{ "/home/walde/certificate/otherTestCert/fullchain.pem" };
  auto const pathToPrivateFile = std::string{ "/home/walde/certificate/otherTestCert/privkey.pem" };
  auto const pathToTmpDhFile = std::string{ "/home/walde/certificate/otherTestCert/dh2048.pem" };
#else
  auto const pathToChainFile = std::string{ "/secrets/fullchain.pem" };
  auto const pathToPrivateFile = std::string{ "/secrets/privkey.pem" };
  auto const pathToTmpDhFile = std::string{ "/secrets/dh2048.pem" };
#endif
  try
    {
      ctx.use_certificate_chain_file (pathToChainFile);
    }
  catch (std::exception &e)
    {
      std::cout << "load fullchain: " << pathToChainFile << " exception : " << e.what () << std::endl;
    }
  try
    {
      ctx.use_private_key_file (pathToPrivateFile, boost::asio::ssl::context::pem);
    }
  catch (std::exception &e)
    {
      std::cout << "load privkey: " << pathToPrivateFile << " exception : " << e.what () << std::endl;
    }
  try
    {
      ctx.use_tmp_dh_file (pathToTmpDhFile);
    }
  catch (std::exception &e)
    {
      std::cout << "load dh2048: " << pathToTmpDhFile << " exception : " << e.what () << std::endl;
    }

  boost::certify::enable_native_https_server_verification (ctx);
  ctx.set_options (SSL_SESS_CACHE_OFF | SSL_OP_NO_TICKET); //  disable ssl cache. It has a bad support in boost asio/beast and I do not know if it helps in performance in our usecase
  for (;;)
    {
      try
        {
          auto socket = co_await acceptor.async_accept (use_awaitable);
          auto connection = std::make_shared<SSLWebsocket> (std::move (socket), ctx);
          users.emplace_back (std::make_shared<User> (User{ {}, {}, {} }));
          std::list<std::shared_ptr<User>>::iterator user = std::next (users.end (), -1);
          connection->set_option (websocket::stream_base::timeout::suggested (role_type::server));
          connection->set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
          co_await connection->next_layer ().async_handshake (ssl::stream_base::server, use_awaitable);
          co_await connection->async_accept (use_awaitable);
          co_spawn (
              executor, [connection, this, &user] () mutable { return readFromClient (user, *connection); }, detached);
          co_spawn (
              executor, [connectionWeakPointer = std::weak_ptr<SSLWebsocket>{ connection }, this, &user] () mutable { return writeToClient (*user, connectionWeakPointer); }, detached);
        }
      catch (std::exception &e)
        {
          std::cout << "Server::listener () connect  Exception : " << e.what () << std::endl;
        }
    }
}
