#include <iostream>

#include <boost/asio.hpp>

#include "AsyncDaytimeServer.h"
#include "node_networking.h"

#define LOCALHOST "127.0.0.1"

#define IP_1 LOCALHOST
#define PORT_1 12341

#define IP_2 LOCALHOST
#define PORT_2 12342

#define IP_3 LOCALHOST
#define PORT_3 12343

#define IP_4 LOCALHOST
#define PORT_4 12344

#define IP_5 LOCALHOST
#define PORT_5 12345

int main()
{
	try
	{
		boost::asio::io_context io_context;
		AsyncDaytimeServer s(io_context);
		s.start();
		node_networking n1(io_context, PORT_1, "n1");

		n1.connect_to(LOCALHOST, 13);
		for (;;) io_context.poll();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}