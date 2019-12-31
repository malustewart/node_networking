
# Asynchronous TCP Daytime Server
Based on [this
asynchronous daytime server example](https://www.boost.org/doc/libs/1_72_0/doc/html/boost_asio/tutorial/tutdaytime3/src.html) from the Boost-Asio documentation.

Example use:
```C++
    try
	{
		boost::asio::io_context io_context;
		AsyncDaytimeServer s(io_context);
		s.start();
		io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
```
Example tested with [this daytime client example](https://www.boost.org/doc/libs/1_72_0/doc/html/boost_asio/tutorial/tutdaytime1/src.html) from the Boost-Asio documentation, using `localhost` and `private IP`.
