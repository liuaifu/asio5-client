#include "stdafx.h"
#include "session.h"
#include <boost/bind.hpp>
#include <iostream>


session::session(boost::shared_ptr<boost::asio::io_service> io_svc, boost::shared_ptr<tcp::socket> sck)
	:client_socket(sck), server_socket(*io_svc)
{
	this->io_svc = io_svc;
}

session::~session(void)
{

}

void session::run(boost::asio::yield_context yield)
{
	BOOST_LOG_TRIVIAL(debug) << "session start";
	boost::system::error_code ec;
	server_socket.async_connect(
		*g_proxy_iterator,
		yield[ec]
	);

	if (ec) {
		BOOST_LOG_TRIVIAL(error) << "open " << g_proxy_iterator->host_name() << " fail, " << ec.message();
		stop();
		return;
	}

	BOOST_LOG_TRIVIAL(debug) << "open " << g_proxy_iterator->host_name();

	boost::asio::spawn(*io_svc, boost::bind(&session::read_server_loop, shared_from_this(), boost::placeholders::_1));
	read_client_loop(yield);
}

void session::stop()
{
	try {
		server_socket.close();
	}
	catch (const std::exception&) {

	}
	
	try {
		client_socket->close();
	}
	catch (const std::exception&) {

	}

	BOOST_LOG_TRIVIAL(debug) << "session stopped";
}

void session::read_server_loop(boost::asio::yield_context yield)
{
	char server_buf[SOCKET_RECV_BUF_LEN];
	boost::system::error_code ec;

	while (true) {
		size_t bytes_transferred = server_socket.async_read_some(
			boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
			yield[ec]
		);
		if (ec) {
			if (ec == boost::asio::error::operation_aborted)
				return;
			BOOST_LOG_TRIVIAL(error) << "read from server fail, " << ec.message();
			stop();
			return;
		}
		write_to_client(yield, server_buf, bytes_transferred, ec);
		if (ec) {
			if (ec == boost::asio::error::operation_aborted)
				return;
			BOOST_LOG_TRIVIAL(error) << "write to client fail, " << ec.message();
			stop();
			return;
		}
	}
}

void session::read_client_loop(boost::asio::yield_context& yield)
{
	char client_buf[SOCKET_RECV_BUF_LEN];
	boost::system::error_code ec;

	while (true) {
		size_t bytes_transferred = client_socket->async_read_some(
			boost::asio::buffer(client_buf, SOCKET_RECV_BUF_LEN),
			yield[ec]
		);
		if (ec) {
			if (ec == boost::asio::error::operation_aborted)
				return;
			BOOST_LOG_TRIVIAL(error) << "read from client fail, " << ec.message();
			stop();
			return;
		}
		write_to_server(yield, client_buf, bytes_transferred, ec);
		if (ec) {
			if (ec == boost::asio::error::operation_aborted)
				return;
			BOOST_LOG_TRIVIAL(error) << "write to server fail, " << ec.message();
			stop();
			return;
		}
	}
}

void session::write_to_client(boost::asio::yield_context& yield, char *data, size_t size, boost::system::error_code& ec)
{
	boost::shared_ptr<char> data_ptr = boost::shared_ptr<char>(new char[size]);
	memcpy(data_ptr.get(), data, size);
	for (size_t i = 0; i < size; i++)
		data_ptr.get()[i] ^= 'F';

	boost::asio::async_write(
		*client_socket,
		boost::asio::buffer(data_ptr.get(), size),
		yield[ec]
	);
}

void session::write_to_server(boost::asio::yield_context& yield, char *data, size_t size, boost::system::error_code& ec)
{
	auto data_ptr = boost::shared_ptr<char>(new char[size]);
	memcpy(data_ptr.get(), data, size);
	for (size_t i = 0; i < size; i++)
		data_ptr.get()[i] ^= 'F';

	boost::asio::async_write(
		server_socket,
		boost::asio::buffer(data_ptr.get(), size),
		yield[ec]
	);
}
