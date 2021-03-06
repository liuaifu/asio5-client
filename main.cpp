﻿// asio5_client.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "session.h"


using boost::asio::ip::tcp;

tcp::resolver::iterator g_proxy_iterator;

void init_log(boost::log::trivial::severity_level log_level, std::string log_dir)
{
	/*boost::log::add_console_log(std::cout, boost::log::keywords::format = (
		boost::log::expressions::stream
		<< boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%H:%M:%S.%f")
		<< " <" << boost::log::trivial::severity
		<< "> " << boost::log::expressions::smessage)
	);*/

	if(log_dir != "") {
		boost::filesystem::path path = boost::filesystem::path(log_dir) / boost::filesystem::path("asio5_client");
		boost::filesystem::create_directories(path);
		boost::filesystem::path filename = path / boost::filesystem::path("asio5_client_%Y%m%d_%N.log");
		boost::log::add_file_log
		(
			boost::log::keywords::file_name = filename.string(),
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
	}
	boost::log::core::get()->set_filter
	(
		boost::log::trivial::severity >= log_level
	);
	boost::log::add_common_attributes();
}

class server
{
public:
	server(boost::shared_ptr< boost::asio::io_service > io_svc, short port)
		: io_svc_(io_svc), acceptor_(*io_svc, tcp::endpoint(tcp::v4(), port))
	{
	}

	void run(boost::asio::yield_context yield)
	{
		int id = 0;
		while (true) {
			id++;

			boost::system::error_code ec;
			boost::shared_ptr<tcp::socket> sck = boost::make_shared<tcp::socket>(*io_svc_);
			acceptor_.async_accept(*sck, yield[ec]);
			if (ec) {
				boost::system::error_code ec_sck;
				sck->close(ec_sck);
				BOOST_LOG_TRIVIAL(error) << "accept fail, " << ec.message();
				return;
			}

			std::string remote_addr = sck->remote_endpoint().address().to_string();
			if (remote_addr != "")
				BOOST_LOG_TRIVIAL(debug) << remote_addr + " connected";
			else {
				// 刚连接过来就断开了
				boost::system::error_code ec_sck;
				sck->close(ec_sck);
				continue;
			}

			tcp::resolver::iterator end;
			if (g_proxy_iterator == end) {
				//服务器的域名还没有解析
				boost::system::error_code ec_sck;
				sck->close(ec_sck);
				BOOST_LOG_TRIVIAL(error) << "server domain name has not been resolved.";
				continue;
			}

			boost::shared_ptr<session> session_ptr = boost::make_shared<session>(io_svc_, sck);
			boost::asio::spawn(*io_svc_, boost::bind(&session::run, session_ptr, boost::placeholders::_1));
		}
	}

private:
	boost::shared_ptr< boost::asio::io_service > io_svc_;
	tcp::acceptor acceptor_;
};

int main(int argc, char **argv)
{
	using namespace boost::program_options;
	variables_map vm;
	//声明需要的选项
	options_description desc("Allowed options");

	try {
		
		desc.add_options()
			("help,h", "produce help message")
			("log-level", value<int>()->default_value(1), "0:trace,1:debug,2:info,3:warning,4:error,5:fatal")
			("log-dir", value<std::string>(), "log directory, if not specified, no log output")
			("server-host", value<std::string>()->required(), "server host")
			("server-port", value<std::string>()->default_value("7070"), "server port")
			("port", value<short>()->default_value(7070), "listen port")
			;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);
	}
	catch (std::exception& e)
	{
		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return 0;
		}

		std::cerr << "Error: " << e.what() << "\n";
		return -1;
	}
	catch (...)
	{
		std::cerr << "Unknown error!" << "\n";
		return -1;
	}

	int log_level = vm["log-level"].as<int>();
	if (log_level < 0 || log_level > 5) {
		std::cerr << "log-level invalid" << std::endl;
		std::exit(-1);
	}
	std::string log_dir;
	if (vm.count("log-dir"))
		log_dir = vm["log-dir"].as<std::string>();
	init_log((boost::log::trivial::severity_level)log_level, log_dir);

	std::string proxy_host = vm["server-host"].as<std::string>();
	if (proxy_host == "") {
		BOOST_LOG_TRIVIAL(fatal) << "--server-host not set";
		return -1;
	}
	std::string proxy_port = vm["server-port"].as<std::string>();

	short port = vm["port"].as<short>();
	boost::shared_ptr< boost::asio::io_service > io_svc = boost::make_shared< boost::asio::io_service >();
	boost::shared_ptr<server> server_ptr(new server(io_svc, port));
	boost::asio::spawn(*io_svc, boost::bind(&server::run, server_ptr, boost::placeholders::_1));
	
	BOOST_LOG_TRIVIAL(info) << "--------------------------------------------------";
	BOOST_LOG_TRIVIAL(info) << "asio5 client v0.7";
	BOOST_LOG_TRIVIAL(info) << "laf163@gmail.com";
	BOOST_LOG_TRIVIAL(info) << "compiled at " << __DATE__ << " " << __TIME__;
	BOOST_LOG_TRIVIAL(info) << "listening on 0.0.0.0:" << port;

	tcp::resolver resolver(*io_svc);
	tcp::resolver::query query(proxy_host, proxy_port);
	
	auto fn_resolver = [&](boost::asio::yield_context yield) {
		boost::system::error_code ec;
		while(true) {
			g_proxy_iterator = resolver.async_resolve(query, yield[ec]);
			if (!ec)
				break;

			boost::asio::system_timer timer(*io_svc);
			timer.expires_from_now(std::chrono::seconds(3));
			timer.async_wait(yield);
		}
	};
	boost::asio::spawn(*io_svc, fn_resolver);
	io_svc->run();

    return 0;
}

