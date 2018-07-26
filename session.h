﻿#pragma once
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/asio.hpp>
#include <map>

using namespace std;
using boost::asio::ip::tcp;

class session : public boost::enable_shared_from_this<session>
{
public:
	session(boost::asio::io_service& _io_service);
	~session(void);

	void start();
	void close_server();
	void close_client();
	//void stop(boost::shared_ptr<client> client_ptr_);
	void write_to_client(char *data, size_t size);
	void handle_client_read(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_client_write(boost::shared_ptr<char> data, const boost::system::error_code& error);
	void handle_resolve(const boost::system::error_code& error, boost::asio::ip::tcp::resolver::results_type results);
	void handle_connect_server(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator);

	void write_to_server(char *data, size_t size);
	void handle_server_read(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_server_write(boost::shared_ptr<char> data_ptr, const boost::system::error_code& error);

public:
	boost::mutex mutex_;
	boost::asio::io_service::strand strand_;
	tcp::socket client_socket;

private:
	enum { SOCKET_RECV_BUF_LEN = 1024 };

	tcp::socket server_socket;

	char client_buf[SOCKET_RECV_BUF_LEN];
	char server_buf[SOCKET_RECV_BUF_LEN];

	int client_recv_count;
	bool stopping;
};