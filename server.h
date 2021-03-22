#define BOOST_ASIO_ENABLE_HANDLER_TRACKING

#include <iostream>
#include <memory>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

class thread_safe_cout: public std::ostringstream
{
public:
    thread_safe_cout() = default;

    ~thread_safe_cout()
    {
        std::lock_guard<std::mutex> guard(_mutexPrint);
        std::cout << this->str();
    }

private:
    static std::mutex _mutexPrint;
};

std::mutex thread_safe_cout::_mutexPrint{};

using boost::asio::ip::tcp;

class some_session
{
public:
    some_session(tcp::socket socket)
            : socket_(std::move(socket))
            , data_strand_(data_service_)
            , data_work_(data_service_)
            , handler_counter_(0)
            , received_handler_counter_(0)
    {
    }

    ~some_session()
    {
        socket_.close();
        io_thread_.interrupt();
        io_thread_.join();
    }

    void start()
    {
        do_read();
        io_thread_ = boost::thread{ [&](){ data_service_.run();}};
    }


private:
    void do_read()
    {
        socket_.async_receive(boost::asio::buffer(recv_buffer_),
                              [&](boost::system::error_code ec, std::size_t)
                              {
                                  for( int i = 0; i < 10; ++i)
                                  {
                                      data_strand_.post([&]() { do_write(); });
                                  }
                              });
    }

    void do_write()
    {
        thread_safe_cout{} << "handler created:  " << handler_counter_++ << ::std::endl;
        socket_.async_send(
                boost::asio::buffer(recv_buffer_, 40),
                boost::bind(&some_session::receive_handler, this,
                            boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
        );
    }

    void receive_handler(boost::system::error_code ec, std::size_t)
    {
        thread_safe_cout{} << "handler completed:  " << received_handler_counter_++ << ::std::endl;
    }

    tcp::socket socket_;
    boost::asio::io_context data_service_;
    boost::asio::io_context::strand data_strand_;
    boost::asio::io_service::work data_work_;
    uint8_t	recv_buffer_[256];
    boost::thread io_thread_;
    uint16_t handler_counter_;
    uint16_t received_handler_counter_;
};

class some_server
{
public:
    some_server(boost::asio::io_context& io_service, const tcp::endpoint& endpoint)
            : acceptor_(io_service, endpoint)
            , socket_(io_service)
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(socket_, [this](boost::system::error_code ec)
        {
            if (!ec)
            {
                current_session_ = std::make_shared<some_session>(std::move(socket_));
                current_session_->start();
            }

            do_accept();
        });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::shared_ptr<some_session> current_session_;
};


int main(int argc, char* argv[])
{
    try
    {
        boost::asio::io_context io_service;

        some_server server{ io_service, tcp::endpoint (tcp::v4(), 6666)};
        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}