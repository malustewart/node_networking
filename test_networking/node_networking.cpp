#include "node_networking.h"

#include <iostream>
#include <boost/bind.hpp>

#define RECEIVE_BUFFER_LEN 200

using boost::asio::ip::tcp;
using boost::asio::ip::address;


//todo: connection * should be smart pointers


typedef struct connection
{
	connection(boost::asio::ip::tcp::socket * _socket)
		: socket(_socket), send_pending(false) {
		receive_buffer.reserve(RECEIVE_BUFFER_LEN);
	}
	~connection() { 
		if (socket->is_open()) {
			// Gracefully end both send and receive operations
			socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both); //todo: throws exception when its called before a connection is established.
		}
		delete socket; // Calls close() method
	}

	boost::asio::ip::tcp::socket * socket;
	std::vector<unsigned char> send_buffer;
	std::vector<unsigned char> receive_buffer;
	bool send_pending;
} connection;

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
	for (auto map_entry : neighbourhood) {
		delete map_entry.second;
	}
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
	send_message_to(neighbourhood.find(endpoint_to_string(ip, port))->second, _msg);
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
	// Socket to manage communication with the next neighbour that tries to connect 
	// to the endpoint given to the acceptor
	auto socket = new tcp::socket(io_context);

	// Contains socket and other relevant elements of the connection
	connection * conn = new connection(socket);
	conn->socket = socket;

	acceptor.async_accept(
		*socket,
		boost::bind(&node_networking::handle_new_connection_by_accept,
					this,
					boost::asio::placeholders::error,
					conn)
	);
}

void node_networking::connect_to(boost::asio::ip::tcp::endpoint remote_endpoint)
{
	auto socket = new tcp::socket(io_context);
	socket->open(tcp::v4());
	socket->set_option(boost::asio::socket_base::reuse_address(true));	// If not set, bind fails because Acceptor is already binded to same endpoint.
	socket->bind(local_endpoint);	// Indicate which port the connection comes from. May fail if reuse_address option hasnt been set.
	
	connection * conn = new connection(socket);
	
	socket->async_connect(
		remote_endpoint,
		boost::bind(&node_networking::handle_new_connection,
			this,
			boost::asio::placeholders::error,
			conn)
	);
}

void node_networking::handle_new_connection_by_connect(const boost::system::error_code & error, 
														connection * conn)
{
	handle_new_connection(error, conn);
}

void node_networking::handle_new_connection_by_accept(const boost::system::error_code & error, 
														connection * conn)
{
	handle_new_connection(error, conn);

	// Wait for new connections
	this->accept_incoming_connections();
}

void node_networking::handle_new_connection(const boost::system::error_code & error,
											connection * conn)
{
	if (!error) {
		// If connection is established:
		auto remote_endpoint = conn->socket->remote_endpoint();	// Neighbours endpoint. Holds their IP address and port
		std::cout << " * "<< who_am_i << " connected with " << endpoint_to_string(remote_endpoint) << std::endl;

		// Store active connection
		neighbourhood.insert(map_entry_t(
			endpoint_to_string(remote_endpoint),
			conn));

		// Listen for incoming messages
		receive_message_from(conn);
		
	} else {
		// If connection cant be made, delete connection
		std::cerr << error.message() << std::endl;
		delete conn;
	}
}

void node_networking::send_message_to(connection * conn, const char * message)
{
	send_message_to(conn, (void *)message, strlen(message));
}

void node_networking::send_message_to(connection * conn, void * _buffer, int length)
{
	unsigned char * buffer = (unsigned char *)_buffer;
	conn->send_buffer.clear();
	for (int i = 0; i < length; i++)
	{
		conn->send_buffer.push_back(*buffer++);
	}
	// Important: the lifetime of the buffer that contains the 
	// message must last at least until the completion handler 
	// for async_read(...) is called, which is NEVER before the 
	// end of this function.

	conn->socket->async_send(
		boost::asio::buffer((const void *)conn->send_buffer.data(), length),
		boost::bind(&node_networking::handle_message_sent,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			conn)
	);
}

void node_networking::receive_message_from(connection * conn)
{
	// Listen for messages
	conn->socket->async_receive(
		boost::asio::buffer(conn->receive_buffer.data(), RECEIVE_BUFFER_LEN),
		boost::bind(&node_networking::handle_message_received,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			conn)
	);
}

void node_networking::handle_message_sent(const boost::system::error_code & error,
											std::size_t bytes_transferred,
											connection * conn)
{
	if (!error) {
		std::cout << " * " << who_am_i << " sent: \"";
		for (unsigned int i = 0; i < bytes_transferred; i++) {
			std::cout << ((unsigned char *)conn->send_buffer.data())[i];
		}
		std::cout << "\" (" << bytes_transferred << " bytes)"<< std::endl;
	} else {
		tcp::endpoint remote_endpoint = conn->socket->remote_endpoint();
		std::cerr << " * Error sending message from " << who_am_i << " to "  << \
			endpoint_to_string(remote_endpoint) << " : " << error.message() << std::endl;
	}
}

void node_networking::handle_message_received(const boost::system::error_code & error, 
												std::size_t bytes_transferred,
												connection * conn)
{
	switch (error.value())
	{
		case 0:	//success
		{
			std::cout << " * " << who_am_i << " received: \"";
			for (unsigned int i = 0; i < bytes_transferred; i++) {
				std::cout << ((unsigned char *)conn->receive_buffer.data())[i];
			}
			std::cout << "\" (" << bytes_transferred << " bytes)" << std::endl;
			receive_message_from(conn);			
			break;
		}
		case boost::asio::error::eof:
		{
			remove_connection(conn->socket->remote_endpoint());
			break;
		}
		case boost::asio::error::operation_aborted:
		{
			break;
		}
		default:
			// Depending on the error, the remote endpoint of the socket cant be accessed.
			std::cerr << " * Unexpected error receiving message " \
				<< " for " << who_am_i << " : " << error.message() << std::endl;

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
	auto it = neighbourhood.find(map_key);
	// If the connection to that endpoint doesnt exist, return.
	if (it == neighbourhood.end()) 
		return;
	
	auto conn = it->second;
	auto remote_endpoint = conn->socket->remote_endpoint();

	delete conn;
	
	neighbourhood.erase(map_key);

	std::cout << " * " << who_am_i << " disconnected from " \
		<< endpoint_to_string(remote_endpoint) << std::endl;
	
}

