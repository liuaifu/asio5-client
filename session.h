#pragma once
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>


using boost::asio::ip::tcp;

class session : public boost::enable_shared_from_this<session>
{
public:
	session(boost::shared_ptr<boost::asio::io_service> io_svc, boost::shared_ptr<tcp::socket> sck);
	~session(void);

	void run(boost::asio::yield_context yield);
	void read_server_loop(boost::asio::yield_context yield);
	void read_client_loop(boost::asio::yield_context& yield);
	void write_to_client(boost::asio::yield_context& yield, char *data, size_t size, boost::system::error_code& ec);
	void write_to_server(boost::asio::yield_context& yield, char *data, size_t size, boost::system::error_code& ec);

private:
	void stop();
	boost::shared_ptr<tcp::socket> client_socket;

private:
	enum { SOCKET_RECV_BUF_LEN = 1024 };
	tcp::socket server_socket;
	boost::shared_ptr<boost::asio::io_service> io_svc;
};
