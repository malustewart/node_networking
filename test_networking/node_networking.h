#pragma once

#include <boost\asio.hpp>
#include <string>

// forward declaration that allows less verbose syntax ("connection" instead of "struct connection")
typedef struct connection connection;

typedef boost::shared_ptr<connection> connection_ptr;

typedef std::pair<std::string, connection_ptr > map_entry_t;

class node_networking
{
public:
	node_networking(boost::asio::io_context& io_context, int port, const char * who_am_i);
	node_networking(boost::asio::io_context& io_context, int port);

	~node_networking();

	void connect_to(const char * ip,  int port);
	
	//todo: bool can_i_send_msg();

	bool send_message_to(const char * ip, int port, const char * buffer_sent);	//ascii, 0 terminated string.

	bool send_message_to(const char * ip, int port, void * buffer, int length);	

	void remove_neighbour(const char * ip, int port);
	
	bool is_neighbour(const char * ip, int port);

private:
	/***** ESTABLISHING CONNECTIONS FUNCTIONS *****/
	
	// Starts listening for any incoming connection at the local endpoint. If any is found,
	// a connection is established and handle_new_connection_by_accept(...) is 
	// called.
	void accept_incoming_connections();
	
	// Tries to establish a connection with the remote endpoint given. If it succeeds, 
	// handle_new_connection_by_connect(...) is called.
	void connect_to(boost::asio::ip::tcp::endpoint remote_endpoint);
	
	// Handler for when a new connection is established after a call to async_connect(...).
	// Is equivalent to handle_new_connection(...), it only exists for symmetry with
	// handle_new_connection_by_accept(...).
	void handle_new_connection_by_connect(const boost::system::error_code & error,
											connection_ptr conn);

	// Handler for when a new connection is established after a call to async_connect(...).
	// Is equivalent to calling handle_new_connection(...) follwed by async_accept(...).
	void handle_new_connection_by_accept(const boost::system::error_code & error,
											connection_ptr conn);

	// Stores the connection and starts listening for incoming messages
	void handle_new_connection(const boost::system::error_code & error, 
								connection_ptr conn);


	/***** SENDING / RECEIVING MESSAGES FUNCTIONS *****/

	// Starts sending message through the specified connection.
	// message: C-string, '\0' terminated
	void send_message_to(connection_ptr conn, const char * message);

	// Starts sending message through the specified connection.
	// buffer: byte buffer, NOT '\0' terminated
	void send_message_to(connection_ptr conn, void * buffer, int length);

	// Starts waiting for messages through the specified connection..
	void receive_message_from(connection_ptr conn);

	// Handler for when a message is sent after a call to async_send(...).
	void handle_message_sent(const boost::system::error_code& error, 
								std::size_t bytes_transferred,
								connection_ptr conn);

	// Handler for when a message is received after a call to async_receive(...). Calls
	// receive_message_from(...) with the same socket after its done.
	void handle_message_received(const boost::system::error_code& error,
									std::size_t bytes_transferred,
									connection_ptr conn);
	

	/****** STORING ACTIVE CONNECTIONS *****/

	// Stores all active connections.
	// Key format: std::string containing ip + ":" + port. Example: std::string("127.0.0.1:12345")
	std::map<std::string, connection_ptr> neighbourhood;

	// Gracefully waits for all async operations to end and closes the connection.
	void remove_connection(const char * ip, unsigned int port);

	// Gracefully waits for all async operations to end and closes the connection.
	void remove_connection(boost::asio::ip::tcp::endpoint endpoint);
	
	// Gracefully waits for all async operations to end and closes the connection.
	void remove_connection(std::string map_key);

	/***********/

	// Resulting std::string works as key for the std::map neighbourhood
	std::string endpoint_to_string(const char * ip, int port);

	// Resulting std::string works as key for the std::map neighbourhood
	std::string endpoint_to_string(boost::asio::ip::tcp::endpoint endpoint);

	const char * who_am_i;

	boost::asio::io_context& io_context;			// Needed to create new sockets
	boost::asio::ip::tcp::endpoint local_endpoint;	// Contains IP and port from which data will be sent
	boost::asio::ip::tcp::acceptor acceptor;		// Used to accept incoming connections
};

//todo: start and stop listening to incoming connections