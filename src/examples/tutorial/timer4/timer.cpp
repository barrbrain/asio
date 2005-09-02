#include <iostream>
#include "asio.hpp"
#include "boost/bind.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

class printer
{
public:
  printer(asio::demuxer& d)
    : timer_(d, boost::posix_time::seconds(1)),
      count_(0)
  {
    timer_.async_wait(boost::bind(&printer::print, this));
  }

  ~printer()
  {
    std::cout << "Final count is " << count_ << "\n";
  }

  void print()
  {
    if (count_ < 5)
    {
      std::cout << count_ << "\n";
      ++count_;

      timer_.expires_at(timer_.expires_at() + boost::posix_time::seconds(1));
      timer_.async_wait(boost::bind(&printer::print, this));
    }
  }

private:
  asio::deadline_timer timer_;
  int count_;
};

int main()
{
  asio::demuxer d;
  printer p(d);
  d.run();

  return 0;
}
