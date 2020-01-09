#include "node_networking.h"

#include <iostream>
#include <boost/bind.hpp>


using boost::asio::ip::tcp;
using boost::asio::ip::address;


//todo: socket * should be smart pointers

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
	send_message_to((*neighbourhood.find(endpoint_to_string(ip, port))).second, _msg);
	return true;
}

void node_networking::remove_neighbour(const char * ip, int port)
{
	remove_connection(ip, port);
}

bool node_networking::is_neighbour(const char * ip, int port)
{
	return neighbourhood.find(endpoint_to_string(ip, port)) != neighbourhood.end();
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
		std::cout << " * "<< who_am_i << " connected with " << endpoint_to_string(remote_endpoint) << std::endl;

		// Store active socket
		neighbourhood.insert(map_entry_t(
			endpoint_to_string(remote_endpoint),
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
	send_message_to(socket, (void *)message, strlen(message));
}

void node_networking::send_message_to(boost::asio::ip::tcp::socket * socket, void * _buffer, int length)
{
	unsigned char * buffer = (unsigned char *)_buffer;
	buffer_sent.clear();
	for (int i = 0; i < length; i++)
	{
		buffer_sent.push_back(*buffer++);
	}
	// Important: the lifetime of the buffer that contains the 
	// message must last at least until the completion handler 
	// for async_read(...) is called, which is NEVER before the 
	// end of this function.

	socket->async_send(
		boost::asio::buffer((const void *)buffer_sent.data(), length),
		boost::bind(&node_networking::handle_message_sent,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			(void *)buffer_sent.data(),
			socket)
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
											void * buffer,
											boost::asio::ip::tcp::socket * socket)
{
	if (!error) {
		std::cout << " * " << who_am_i << " sent: \"";
		for (unsigned int i = 0; i < bytes_transferred; i++) {
			std::cout << ((unsigned char *)buffer)[i];
		}
		std::cout << "\" (" << bytes_transferred << " bytes)"<< std::endl;
	} else {
		tcp::endpoint remote_endpoint = (*socket).remote_endpoint();
		std::cerr << " * Error sending message from " << who_am_i << " to "  << \
			endpoint_to_string(remote_endpoint) << " : " << error.message() << std::endl;
	}
}

void node_networking::handle_message_received(const boost::system::error_code & error, 
												std::size_t bytes_transferred,
												void * buffer,
												boost::asio::ip::tcp::socket * socket)
{
	switch (error.value())
	{
		case 0:	//success
		{
			std::cout << " * " << who_am_i << " received: \"";
			for (unsigned int i = 0; i < bytes_transferred; i++) {
				std::cout << ((unsigned char *)buffer)[i];
			}
			std::cout << "\" (" << bytes_transferred << " bytes)" << std::endl;
			receive_message_from(socket);			
			break;
		}
		case boost::asio::error::eof:
		{
			//tcp::endpoint remote_endpoint = (*socket).remote_endpoint();
			//std::cout << " * " << endpoint_to_string(remote_endpoint) << " disconnected from "\
			//	<< who_am_i << std::endl;
			////todo: delete socket;
			remove_connection((*socket).remote_endpoint());
			break;
		}
		case boost::asio::error::operation_aborted:
		{
			break;
		}
		default:
			tcp::endpoint remote_endpoint = (*socket).remote_endpoint();
			std::cerr << " * who_ Unexpeted error receiving message from " << endpoint_to_string(remote_endpoint)\
				<< " to " << who_am_i << " : " << error.message() << std::endl;

	}
}


std::string node_networking::endpoint_to_string(const char * ip, int port)
{
	return std::string(ip) + ":" + std::to_string(port);
}

std::string node_networking::endpoint_to_string(boost::asio::ip::tcp::endpoint endpoint)
{
	return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}

void node_networking::remove_connection(const char * ip, unsigned int port)
{
	remove_connection(endpoint_to_string(ip, port));
}

void node_networking::remove_connection(boost::asio::ip::tcp::endpoint endpoint)
{
	remove_connection(endpoint_to_string(endpoint));
}

void node_networking::remove_connection(std::string map_key)
{
	auto connection = neighbourhood.find(map_key);
	// If the connection to that endpoint doesnt exist, return.
	if (connection == neighbourhood.end()) 
		return;
	
	auto socket = connection->second;
	auto remote_endpoint = socket->remote_endpoint();

	// Gracefully end both send and receive operations
	socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
	delete socket;	// Calls close() method
	
	neighbourhood.erase(map_key);

	std::cout << " * " << who_am_i << " disconnected from " \
		<< endpoint_to_string(remote_endpoint) << std::endl;
	
}

