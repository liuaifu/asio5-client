// asio5_client.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "session.h"

boost::asio::io_service g_io_service;
tcp::resolver::iterator g_proxy_iterator;

using boost::asio::ip::tcp;

class server
{
public:
	server(short port)
		: io_service_(g_io_service), acceptor_(g_io_service, tcp::endpoint(tcp::v4(), port))
	{
		session_ptr.reset(new session(g_io_service));
		start_accept();
	}

private:
	void start_accept()
	{
		static int id = 0;
		id++;

		acceptor_.async_accept(session_ptr->client_socket,
			boost::bind(&server::handle_accept, this, boost::ref(session_ptr->client_socket), _1));
	}

	void handle_accept(tcp::socket &client_socket, const boost::system::error_code& error)
	{

		if (!error &&client_socket.remote_endpoint().address().to_string() != "")
		{
			std::cout << client_socket.remote_endpoint().address().to_string() + " connected" << std::endl;
			session_ptr->start();
			session_ptr.reset(new session(g_io_service));
		}
		else
		{
			session_ptr->close_client();
			session_ptr->close_server();
			std::cout << __FUNCTION__ << ": error, " << error << std::endl;
		}

		start_accept();
	}

	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	boost::shared_ptr<session> session_ptr;

};

int main(int argc, char **argv)
{
	std::cout << "asio5 client v0.4" << std::endl;
	std::cout << "laf163@gmail.com" << std::endl;

	using namespace boost::program_options;
	//声明需要的选项
	options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("server-ip", value<std::string>()->default_value(""), "server ip")
		("server-port", value<std::string>()->default_value("7070"), "server port")
		("port", value<short>()->default_value(7070), "listen port")
		;

	variables_map vm;
	store(parse_command_line(argc, argv, desc), vm);
	notify(vm);

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return 0;
	}

	std::string proxy_host = vm["server-ip"].as<std::string>();
	if (proxy_host == "") {
		std::cout << "--server-ip not set" << std::endl;
		return -1;
	}
	std::string proxy_port = vm["server-port"].as<std::string>();

	short port = vm["port"].as<short>();
	boost::shared_ptr<server> server_ptr(new server(port));
	std::cout << "listening on 0.0.0.0:" << port << std::endl;
	
	tcp::resolver resolver(g_io_service);
	tcp::resolver::query query(proxy_host, proxy_port);
	g_proxy_iterator = resolver.resolve(query);

	g_io_service.run();

    return 0;
}

