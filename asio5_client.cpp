// asio5_client.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "session.h"

boost::asio::io_service g_io_service;
tcp::resolver::iterator g_proxy_iterator;

using boost::asio::ip::tcp;

void init_log(boost::log::trivial::severity_level log_level)
{
	boost::log::add_console_log(std::cout, boost::log::keywords::format = (
		boost::log::expressions::stream
		<< boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%H:%M:%S.%f")
		<< " <" << boost::log::trivial::severity
		<< "> " << boost::log::expressions::smessage)
	);
	boost::log::add_file_log
	(
		boost::log::keywords::file_name = "log/asio5_client_%Y%m%d_%N.log",
		boost::log::keywords::rotation_size = 50 * 1024 * 1024,
		boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
		boost::log::keywords::auto_flush = true,
		boost::log::keywords::open_mode = std::ios::app,
		boost::log::keywords::format = (
			boost::log::expressions::stream
			<< boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
			<< "|" << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID")
			<< "|" << boost::log::trivial::severity
			<< "|" << boost::log::expressions::smessage
			)
	);
	boost::log::core::get()->set_filter
	(
		boost::log::trivial::severity >= log_level
	);
	boost::log::add_common_attributes();
}

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
			BOOST_LOG_TRIVIAL(debug) << client_socket.remote_endpoint().address().to_string() + " connected";
			session_ptr->start();
			session_ptr.reset(new session(g_io_service));
		}
		else
		{
			session_ptr->stop();
			BOOST_LOG_TRIVIAL(error) << "accept fail, " << error;
		}

		start_accept();
	}

	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	boost::shared_ptr<session> session_ptr;

};

int main(int argc, char **argv)
{
	using namespace boost::program_options;
	//声明需要的选项
	options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("log-level", value<int>()->default_value(1), "0:trace,1:debug,2:info,3:warning,4:error,5:fatal")
		("server-ip", value<std::string>()->default_value("52.77.223.223"), "server ip")
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

	int log_level = vm["log-level"].as<int>();
	if (log_level < 0 || log_level > 5) {
		std::cerr << "log-level invalid" << std::endl;
		std::exit(-1);
	}

	init_log((boost::log::trivial::severity_level)log_level);

	std::string proxy_host = vm["server-ip"].as<std::string>();
	if (proxy_host == "") {
		BOOST_LOG_TRIVIAL(fatal) << "--server-ip not set";
		return -1;
	}
	std::string proxy_port = vm["server-port"].as<std::string>();

	short port = vm["port"].as<short>();
	boost::shared_ptr<server> server_ptr(new server(port));
	
	BOOST_LOG_TRIVIAL(info) << "--------------------------------------------------";
	BOOST_LOG_TRIVIAL(info) << "asio5 client v0.5";
	BOOST_LOG_TRIVIAL(info) << "laf163@gmail.com";
	BOOST_LOG_TRIVIAL(info) << "compiled at " << __DATE__ << " " << __TIME__;
	BOOST_LOG_TRIVIAL(info) << "listening on 0.0.0.0:" << port;

	tcp::resolver resolver(g_io_service);
	tcp::resolver::query query(proxy_host, proxy_port);
	g_proxy_iterator = resolver.resolve(query);

	g_io_service.run();

    return 0;
}

