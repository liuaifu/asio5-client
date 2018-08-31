#pragma once
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class session : public boost::enable_shared_from_this<session>
{
public:
	session(boost::shared_ptr<boost::asio::io_service> io_svc, boost::shared_ptr<tcp::socket> sck);
	~session(void);

	void run();
	void read_server_loop();
	void read_client_loop();
	void write_to_client(char *data, size_t size, boost::system::error_code& ec);
	void write_to_server(char *data, size_t size, boost::system::error_code& ec);

private:
	void stop();
	boost::shared_ptr<tcp::socket> client_socket;

private:
	enum { SOCKET_RECV_BUF_LEN = 1024 };
	tcp::socket server_socket;
};