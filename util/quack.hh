#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <vector>

// 32-bit Modular Integer
class ModInt
{
  static constexpr uint64_t QUACK_MODULUS = 4294967291; // Largest prime less than 2^32 - 1

private:
  uint64_t value_;
  uint64_t prime_;

public:
  ModInt() : value_( 0 ), prime_( QUACK_MODULUS ) {}
  ModInt( uint64_t n ) : value_( n % QUACK_MODULUS ), prime_( QUACK_MODULUS ) {}
  ModInt( uint64_t n, uint64_t prime ) : value_( n % prime ), prime_( prime ) {} // For testing

  uint32_t value() const { return value_; };

  ModInt operator+( const ModInt& rhs ) { return ModInt( value_, prime_ ) += rhs; }
  ModInt& operator+=( const ModInt& rhs )
  {
    assert( prime_ == rhs.prime_ );
    value_ = ( value_ + rhs.value_ ) % prime_;
    return *this;
  }

  ModInt operator-( const ModInt& rhs ) { return ModInt( value_, prime_ ) -= rhs; }
  ModInt& operator-=( const ModInt& rhs )
  {
    assert( prime_ == rhs.prime_ );
    if ( value_ < rhs.value_ )
      value_ += prime_;
    value_ = value_ - rhs.value_;
    return *this;
  }

  ModInt operator*( const ModInt& rhs ) { return ModInt( value_, prime_ ) *= rhs; }
  ModInt& operator*=( const ModInt& rhs )
  {
    assert( prime_ == rhs.prime_ );
    value_ = ( value_ * rhs.value_ ) % prime_;
    return *this;
  }

  ModInt operator%( const ModInt& rhs ) { return ModInt( value_, prime_ ) %= rhs; }
  ModInt& operator%=( const ModInt& rhs )
  {
    assert( prime_ == rhs.prime_ );
    value_ = ( value_ % rhs.value_ ) % prime_;
    return *this;
  }

  ModInt operator/( const ModInt& rhs ) { return ModInt( value_, prime_ ) /= rhs; }
  ModInt& operator/=( const ModInt& rhs )
  {
    assert( prime_ == rhs.prime_ );
    *this *= rhs.inverse();
    return *this;
  }

  ModInt& operator++()
  {
    value_ = ( value_ + 1 ) % prime_;
    return *this;
  }

  ModInt& operator--()
  {
    value_ = ( value_ - 1 ) % prime_;
    return *this;
  }

  bool operator==( const ModInt& rhs )
  {
    assert( prime_ == rhs.prime_ );
    return value_ == rhs.value_;
  }

  bool operator!=( const ModInt& rhs )
  {
    assert( prime_ == rhs.prime_ );
    return value_ != rhs.value_;
  }

  // Fermat or Euclid will help us calculate the inverse of x.
  //
  // Euclid's theorem says that `ax + by = gcd(a, b) = 1` since `b` is prime.
  // So `x` will be the inverse of `a`.
  ModInt inverse() const
  {
    int64_t a = value_;
    int64_t b = prime_;
    int64_t x = 1;
    int64_t y = 0;
    int64_t quotient;
    int64_t temp;

    while ( a > 1 ) {
      quotient = a / b;

      temp = b;
      b = a % b;
      a = temp;

      temp = y;
      y = x - quotient * y;
      x = temp;
    }

    if ( x < 0 ) {
      x += prime_;
    }

    return ModInt( x, prime_ );
  }

  friend std::ostream& operator<<( std::ostream& stream, const ModInt& obj )
  {
    stream << obj.value_;
    return stream;
  }

  friend bool operator<( const ModInt& l, const ModInt& r ) { return l.value_ < r.value_; };
};

class PowerSums
{
private:
  std::vector<ModInt> sums_ {};
  std::set<ModInt> items_ {};
  size_t threshold_;

public:
  PowerSums( size_t threshold ) : threshold_( threshold ) { sums_.resize( threshold ); }
  // Directly construct the power sums
  PowerSums( std::vector<ModInt>& sums )
  {
    sums_ = std::move( sums );
    threshold_ = sums_.size();
  };

  size_t size() const { return threshold_; }
  void add( const ModInt n );
  void remove( const ModInt n );
  PowerSums difference( const PowerSums& other );

  const ModInt& operator[]( int idx ) const { return sums_[idx]; }
  friend std::ostream& operator<<( std::ostream& stream, const PowerSums& obj )
  {
    for ( auto sum : obj.sums_ ) {
      stream << sum << ' ';
    }
    return stream;
  }
};

class Polynomial
{
private:
  std::vector<ModInt> coeffs_ {};

public:
  Polynomial( const PowerSums& sums );
  ModInt eval( ModInt x );

  friend std::ostream& operator<<( std::ostream& stream, const Polynomial& obj )
  {
    for ( auto sum : obj.coeffs_ ) {
      stream << sum << ' ';
    }
    return stream;
  }
};