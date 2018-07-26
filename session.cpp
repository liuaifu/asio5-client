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
	cout << "start read client" << endl;
	
	server_socket.async_connect(*g_proxy_iterator,
		strand_.wrap(boost::bind(&session::handle_connect_server, shared_from_this(),
			boost::asio::placeholders::error, g_proxy_iterator)
		)
	);
}

void session::close_server()
{
	server_socket.close();
	cout << "server closed" << endl;
}

void session::close_client()
{
	client_socket.close();
	cout << "client closed" << endl;
}

void session::write_to_client(char *data, size_t size)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	//BOOST_LOG_TRIVIAL(debug) <<id_<<": "<<"向client"<<"发送"<<size<<"字节";
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
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if (error) {
		if (error == boost::asio::error::operation_aborted) {
			//BOOST_LOG_TRIVIAL(debug) << id_ << ": abort " << __FUNCTION__;
			return;
		}
		if (error == boost::asio::error::eof) {
			close_client();
			close_server();
			return;
		}
		//BOOST_LOG_TRIVIAL(error) <<id_<<": "<<"client"<<"读取数据出错：错误码="<<error.value()<<", "<<error.message();
		//service_ptr_->stop();
		close_client();
		close_server();
		return;
	}

	//BOOST_LOG_TRIVIAL(debug)<<id_<<": "<<"从client"<<"读取到"<<bytes_transferred<<"字节";

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
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if (error)
	{
		if (error == boost::asio::error::operation_aborted)
			return;

		//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"client"<<"发送数据出错：错误码="<<error.value()<<", "<<error.message();
		//service_ptr_->stop();
		close_client();
		close_server();
		return;
	}
}

void session::handle_connect_server(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if (error)
	{
		std::cout << "connect " << endpoint_iterator->host_name() << " fail" << std::endl;
		close_client();
		close_server();
		return;
	}

	//BOOST_LOG_TRIVIAL(debug) << id_ << ": 已连接到" << endpoint_iterator->host_name();

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
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	//BOOST_LOG_TRIVIAL(debug)<<id_<<": "<<"向service"<<"发送"<<size<<"字节";
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
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;
		if (error == boost::asio::error::eof) {
			stopping = true;
			return;
		}
		//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"service"<<"读取数据出错：错误码="<<error.value()<<", "<<error.message();
		close_client();
		close_server();
		return;
	}

	//BOOST_LOG_TRIVIAL(debug)<<id_<<": "<<"从service"<<"读取到"<<bytes_transferred<<"字节";

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
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if (error)
	{
		if (error == boost::asio::error::operation_aborted)
			return;

		//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"service"<<"向服务器发送数据出错：错误码="<<error.value()<<", "<<error.message();
		close_client();
		close_server();
		return;
	}
}
