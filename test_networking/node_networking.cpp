#include "node_networking.h"

#include <iostream>
#include <boost/bind.hpp>


using boost::asio::ip::tcp;
using boost::asio::ip::address;

//todo: handle disconnects

// todo: should actually use more than one buffer to avoid overwriting
unsigned char incoming_buffer[200];

node_networking::node_networking(boost::asio::io_context& io_context_, int port, const char * who_am_i_)
	: io_context(io_context_),
		
		// Holds info on which port and protocol is used 
		local_endpoint(boost::asio::ip::tcp::v4(), port),
		
		// Create acceptor, bind it to endpoint and start listening for incoming connections
		acceptor(io_context_, local_endpoint),
		who_am_i(who_am_i_)
{
	accept_incoming_connections();
}

node_networking::node_networking(boost::asio::io_context & io_context, int port)
	: node_networking(io_context, port, "Malu")
{
}

node_networking::~node_networking()
{
}

void node_networking::connect_to(const char * ip, int port)
{
	tcp::endpoint remote_endpoint(address::from_string(ip), port);
	connect_to(remote_endpoint);
}

bool node_networking::send_message_to(const char * ip, int port, const char * _msg)
{
	if (!is_neighbour(ip, port))
	{
		return false;
	}
	send_message_to((*neighbourhood.find(create_map_key(ip, port))).second, _msg);
	return true;
}

bool node_networking::is_neighbour(const char * ip, int port)
{
	return neighbourhood.find(create_map_key(ip, port)) != neighbourhood.end();
}

void node_networking::accept_incoming_connections()
{
	// Socket to manage connection with the next neighbour that tries to connect 
	// to the endpoint given to the acceptor
	auto socket = new tcp::socket(io_context);

	acceptor.async_accept(
		*socket,
		boost::bind(&node_networking::handle_new_connection_by_accept,
					this,
					boost::asio::placeholders::error,
					socket)
	);
}

void node_networking::connect_to(boost::asio::ip::tcp::endpoint remote_endpoint)
{
	auto socket = new tcp::socket(io_context);
	(*socket).open(tcp::v4());
	(*socket).set_option(boost::asio::socket_base::reuse_address(true));	// If not set, bind fails because Acceptor is already binded to same endpoint.
	(*socket).bind(local_endpoint);	// Indicate which port the connection comes from. May fail is reuse_address option hasnt been set.
	(*socket).async_connect(
		remote_endpoint,
		boost::bind(&node_networking::handle_new_connection,
			this,
			boost::asio::placeholders::error,
			socket)
	);
}

void node_networking::handle_new_connection_by_connect(const boost::system::error_code & error, boost::asio::ip::tcp::socket * socket)
{
	handle_new_connection(error, socket);
}

void node_networking::handle_new_connection_by_accept(const boost::system::error_code & error, boost::asio::ip::tcp::socket * socket)
{
	handle_new_connection(error, socket);

	// Wait for new connections
	this->accept_incoming_connections();
}

void node_networking::handle_new_connection(const boost::system::error_code & error,
												tcp::socket * socket)
{
	if (!error) {
		// If connection is established:
		auto remote_endpoint = (*socket).remote_endpoint();	// Neighbours endpoint. Holds their IP address and port
		std::cout << " * "<< who_am_i << " connected with " << remote_endpoint.address() << ":" << remote_endpoint.port() << std::endl;

		// Store active socket
		neighbourhood.insert(map_entry_t(
			create_map_key(remote_endpoint.address().to_string().c_str(), remote_endpoint.port()),
			socket));

		// Listen for incoming messages
		receive_message_from(socket);
		
	} else {
		// If connection cant be made, delete socket
		std::cerr << error.message() << std::endl;
		delete socket;
	}
}

void node_networking::send_message_to(boost::asio::ip::tcp::socket * socket,
	const char * message)
{
	msg = std::string(message);	// Important: the lifetime of the buffer that contains the 
								// message must last at least until the completion handler 
								// for async_read(...) is called, which is NEVER before the 
								// end of this function. Therefore, the message is passed 
								// onto the msg class variable, which should store it until 
								// the handler is called.

	socket->async_send(
		boost::asio::buffer((const void *)msg.data(), msg.length()),
		boost::bind(&node_networking::handle_message_sent,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			(void *)msg.data())
	);
}

void node_networking::receive_message_from(boost::asio::ip::tcp::socket * socket)
{
	// Listen for messages
	(*socket).async_receive(
		boost::asio::buffer(incoming_buffer, 200),
		boost::bind(&node_networking::handle_message_received,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			incoming_buffer,
			socket)
	);
}

void node_networking::handle_message_sent(const boost::system::error_code & error,
											std::size_t bytes_transferred,
											void * buffer)
{
	if (!error) {
		std::cout << " * " << who_am_i << " sent: \"";
		for (int i = 0; i < bytes_transferred; i++) {
			std::cout << ((unsigned char *)buffer)[i];
		}
		std::cout << "\" (" << bytes_transferred << " bytes)"<< std::endl;
	} else {
		std::cerr << " * Error sending message from " << who_am_i << ": " << error.message() << std::endl;
	}
}

void node_networking::handle_message_received(const boost::system::error_code & error, 
												std::size_t bytes_transferred,
												void * buffer,
												boost::asio::ip::tcp::socket * socket)
{
	if (!error) {
		std::cout << " * " << who_am_i << " received: \"";
		for (int i = 0; i < bytes_transferred; i++) {
			std::cout << ((unsigned char *)buffer)[i];
		}
		std::cout << "\" (" << bytes_transferred << " bytes)" << std::endl;

		receive_message_from(socket);
	}
	else {
		std::cerr << error.message() << std::endl;
	}
}


std::string node_networking::create_map_key(const char * ip, int port)
{
	return std::string(ip) + ":" + std::to_string(port);
}
