#pragma once

#include <boost\asio.hpp>
#include <string>

typedef std::pair<std::string, boost::asio::ip::tcp::socket * > map_entry_t;

class node_networking
{
public:
	node_networking(boost::asio::io_context& io_context, int port, const char * who_am_i);
	node_networking(boost::asio::io_context& io_context, int port);

	~node_networking();

	void connect_to(const char * ip,  int port);
	
	//todo: bool can_i_send_msg();

	bool send_message_to(const char * ip, int port, const char * buffer_sent);	//ascii, 0 terminated string.

	//todo: bool send_message_to(const char * ip, int port, void * buffer, int length);	


	void remove_neighbour(const char * ip, int port);	//Como manejar el cierre de los sockets con operaciones pendientes?
	
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
											boost::asio::ip::tcp::socket * socket);

	// Handler for when a new connection is established after a call to async_connect(...).
	// Is equivalent to calling handle_new_connection(...) follwed by async_accept(...).
	void handle_new_connection_by_accept(const boost::system::error_code & error,
											boost::asio::ip::tcp::socket * socket);

	// Stores the socket and starts listening for incoming messages
	void handle_new_connection(const boost::system::error_code & error, 
								boost::asio::ip::tcp::socket * socket);


	/***** SENDING / RECEIVING MESSAGES FUNCTIONS *****/

	// Starts sending message through the connection saved in the socket. 
	void send_message_to(boost::asio::ip::tcp::socket * socket, const char * message);

	// Starts waiting for messages through the connection saved in the socket.
	void receive_message_from(boost::asio::ip::tcp::socket * socket);

	// Handler for when a message is sent after a call to async_send(...).
	void handle_message_sent(const boost::system::error_code& error, 
								std::size_t bytes_transferred,
								void * buffer, 
								boost::asio::ip::tcp::socket * socket);

	// Handler for when a message is received after a call to async_receive(...). Calls
	// receive_message_from(...) with the same socket after its done.
	void handle_message_received(const boost::system::error_code& error,
									std::size_t bytes_transferred,
									void * buffer,
									boost::asio::ip::tcp::socket * socket);
	

	/****** STORING ACTIVE CONNECTIONS *****/

	std::string create_map_key(const char * ip, int port);

	std::string create_map_key(boost::asio::ip::tcp::endpoint endpoint);

	std::map<std::string, boost::asio::ip::tcp::socket * > neighbourhood;	// tcp::socket cant be copied, 
																			// therefore socket * is used 
																			// and sockets are created in 
																			// heap

	void remove_connection(const char * ip, unsigned int port);

	void remove_connection(boost::asio::ip::tcp::endpoint endpoint);

	void remove_connection(std::string map_key);

	/***********/

	//todo: bool sending_msg;	// true if async_send() was called but the handler hasnt been called, false otherwise
						// For simplicity, this implementations only allows one message to be sent at a time, 
						// ie. after an async_send(), it has to wait until its handler is called to be able 
						// to call async_send() again without errors.

	std::vector<unsigned char> buffer_sent;	// Contains the last message sent. It is crucial that the buffer that 
									// contains the message sent by async_send is not modified or destroyed 
									// until after the handler is called. In this implementation, this 
									// variable acts as the buffer.

	const char * who_am_i;

	boost::asio::io_context& io_context;			// Needed to create new sockets
	boost::asio::ip::tcp::endpoint local_endpoint;	// Contains IP and port from which data will be sent
	boost::asio::ip::tcp::acceptor acceptor;		// Used to accept incoming connections
};

//todo: start and stop listening to incoming connections