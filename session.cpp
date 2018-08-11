#include "stdafx.h"
#include "session.h"
#include <boost/bind.hpp>
#include <iostream>

extern boost::asio::io_service g_io_service;
session::session(boost::asio::io_service& _io_service)
	:client_socket(_io_service), server_socket(_io_service), strand_(_io_service)
{
	client_recv_count = 0;
	stopping = false;
}

session::~session(void)
{

}

void session::start()
{
	BOOST_LOG_TRIVIAL(debug) << "session start";
	server_socket.async_connect(*g_proxy_iterator,
		strand_.wrap(boost::bind(&session::handle_connect_server, shared_from_this(),
			boost::asio::placeholders::error, g_proxy_iterator)
		)
	);
}

void session::stop()
{
	try {
		server_socket.close();
	}
	catch (const std::exception&) {

	}
	
	try {
		client_socket.close();
	}
	catch (const std::exception&) {

	}

	BOOST_LOG_TRIVIAL(debug) << "session stopped";
}

void session::write_to_client(char *data, size_t size)
{
	boost::shared_ptr<char> data_ptr = boost::shared_ptr<char>(new char[size]);
	memcpy(data_ptr.get(), data, size);
	for (size_t i = 0; i < size; i++)
		data_ptr.get()[i] ^= 'F';

	boost::asio::async_write(client_socket,
		boost::asio::buffer(data_ptr.get(), size),
		strand_.wrap(boost::bind(&session::handle_client_write, shared_from_this(),
			data_ptr,
			boost::asio::placeholders::error)
		)
	);
}

void session::handle_resolve(const boost::system::error_code& error, boost::asio::ip::tcp::resolver::results_type results)
{

}

void session::handle_client_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(error) << error;
		stop();
		return;
	}

	write_to_server(client_buf, bytes_transferred);

	client_socket.async_read_some(boost::asio::buffer(client_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::handle_client_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::handle_client_write(boost::shared_ptr<char> data, const boost::system::error_code& error)
{
	if (error)
	{
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(error) << error;
		stop();
		return;
	}
}

void session::handle_connect_server(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator)
{
	if (error)
	{
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(error) << "open " << endpoint_iterator->host_name() << " fail, " << error;
		stop();
		return;
	}

	BOOST_LOG_TRIVIAL(debug) << "open " << endpoint_iterator->host_name();

	server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::handle_server_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);

	client_socket.async_read_some(boost::asio::buffer(client_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::handle_client_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::write_to_server(char *data, size_t size)
{
	auto data_ptr = boost::shared_ptr<char>(new char[size]);
	memcpy(data_ptr.get(), data, size);
	for (size_t i = 0; i < size; i++)
		data_ptr.get()[i] ^= 'F';

	boost::asio::async_write(server_socket,
		boost::asio::buffer(data_ptr.get(), size),
		strand_.wrap(boost::bind(&session::handle_server_write, shared_from_this(),
			data_ptr, boost::asio::placeholders::error)
		)
	);
}

void session::handle_server_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;
		if (error == boost::asio::error::eof) {
			stopping = true;
			return;
		}
		BOOST_LOG_TRIVIAL(error) << error;
		stop();
		return;
	}

	write_to_client(server_buf, bytes_transferred);
	server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::handle_server_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::handle_server_write(boost::shared_ptr<char> data_ptr, const boost::system::error_code& error)
{
	if (error)
	{
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(error) << error;
		stop();
		return;
	}
}
