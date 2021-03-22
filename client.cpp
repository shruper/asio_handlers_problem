#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class some_client
{
public:
    some_client(boost::asio::io_context& io_context,
                const tcp::resolver::results_type& endpoints)
            : io_context_(io_context),
              socket_(io_context)
    {
        do_connect(endpoints);
    }

    void write()
    {
        boost::asio::post(io_context_, [this]() { do_write(); });
    }

    void close()
    {
        boost::asio::post(io_context_, [this]() { socket_.close(); });
    }

private:
    void do_connect(const tcp::resolver::results_type& endpoints)
    {
        boost::asio::async_connect(socket_, endpoints, [this](boost::system::error_code ec, tcp::endpoint) {  });
    }

    void do_write()
    {
        boost::asio::async_write(socket_, boost::asio::buffer(buffer_),
                                 [this](boost::system::error_code ec, std::size_t )
                                 {
                                     if (!ec)
                                     {
                                         std::cout << "written";
                                     }
                                     close();
                                 });
    }

private:
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    uint8_t buffer_[40];
};

int main(int argc, char* argv[])
{
    try
    {

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "6666");
        some_client c(io_context, endpoints);

        std::thread t([&io_context](){ io_context.run(); });

        c.write();
        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
