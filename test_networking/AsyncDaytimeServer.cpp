#include "AsyncDaytimeServer.h"

#include <iostream>
#include <string>
#include <boost\bind.hpp>

using boost::asio::ip::tcp;

std::string make_daytime_string();


AsyncDaytimeServer::AsyncDaytimeServer(boost::asio::io_context& io_context)
	: context_(io_context),
	acceptor_(io_context, tcp::endpoint(tcp::v4(), 13)),
	socket_(io_context)
{
}

AsyncDaytimeServer::~AsyncDaytimeServer()
{
}

void AsyncDaytimeServer::start()
{
	if (socket_.is_open())
	{
		socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
		socket_.close();
	}
	wait_connection();
}

void AsyncDaytimeServer::wait_connection()
{
	std::cout << "wait_connection()" << std::endl;
	if (socket_.is_open())
	{
		std::cout << "Error: Can't accept new connection from an open socket" << std::endl;
		return;
	}
	acceptor_.async_accept(
		socket_,
		boost::bind(
			&AsyncDaytimeServer::connection_received_cb,
			this,
			boost::asio::placeholders::error
		)
	);
}

void AsyncDaytimeServer::answer()
{
	std::cout << "answer()" << std::endl;
	msg = make_daytime_string();
	boost::asio::async_write(
		socket_,
		boost::asio::buffer(msg),
		boost::bind(
			&AsyncDaytimeServer::response_sent_cb,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
	socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
	socket_.close();
}


void AsyncDaytimeServer::connection_received_cb(const boost::system::error_code& error)
{
	std::cout << "connection_received_cb()" << std::endl;
	if (!error) {
		answer();
		wait_connection();
	}
	else {
		std::cout << error.message() << std::endl;
	}
}

void AsyncDaytimeServer::response_sent_cb(const boost::system::error_code& error,
	size_t bytes_sent)
{
	std::cout << "response_sent_cb()" << std::endl;
	if (!error) {
		std::cout << "Response sent. " << bytes_sent << " bytes." << std::endl;
	}

}


std::string make_daytime_string()
{
#pragma warning(disable : 4996)
	using namespace std; // For time_t, time and ctime;
	time_t now = time(0);
	return ctime(&now);
}