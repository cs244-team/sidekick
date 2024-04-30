#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

// Thread-safe queue
template<typename T>
class conqueue
{
private:
  std::queue<T> _inner {};
  std::mutex _lock {};
  std::condition_variable _cv {};

public:
  void push( T& item )
  {
    std::unique_lock lk( _lock );
    _inner.push( item );
    _cv.notify_one();
  }

  T pop()
  {
    std::unique_lock lk( _lock );
    while ( _inner.empty() ) {
      _cv.wait( lk );
    }

    T item = _inner.front();
    _inner.pop();
    return item;
  }

  size_t size() const
  {
    std::unique_lock lk( _lock );
    return _inner.size();
  }
};