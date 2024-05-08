#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

// Thread-safe queue
template<typename T>
class conqueue
{
private:
  std::queue<T> inner_ {};
  std::mutex lock_ {};
  std::condition_variable non_empty_cv_ {};

public:
  void push( T& item )
  {
    std::unique_lock lk( lock_ );
    inner_.push( item );
    non_empty_cv_.notify_one();
  }

  T pop()
  {
    std::unique_lock lk( lock_ );
    while ( inner_.empty() ) {
      non_empty_cv_.wait( lk );
    }

    T item = inner_.front();
    inner_.pop();
    return item;
  }

  size_t size() const
  {
    std::unique_lock lk( lock_ );
    return inner_.size();
  }
};