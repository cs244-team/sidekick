#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

// 32-bit Modular Integer
class ModInt
{
  static constexpr uint64_t QUACK_MODULUS = 4294967291; // Largest prime less than 2^32 - 1

private:
  uint64_t _value;
  uint64_t _prime;

public:
  ModInt() : _value( 0 ), _prime( QUACK_MODULUS ) {}
  ModInt( uint64_t n ) : _value( n % QUACK_MODULUS ), _prime( QUACK_MODULUS ) {}
  ModInt( uint64_t n, uint64_t prime ) : _value( n % prime ), _prime( prime ) {} // For testing

  ModInt operator+( const ModInt& rhs ) { return ModInt( _value, _prime ) += rhs; }
  ModInt& operator+=( const ModInt& rhs )
  {
    assert( _prime == rhs._prime );
    _value = ( _value + rhs._value ) % _prime;
    return *this;
  }

  ModInt operator-( const ModInt& rhs ) { return ModInt( _value, _prime ) -= rhs; }
  ModInt& operator-=( const ModInt& rhs )
  {
    assert( _prime == rhs._prime );
    if ( _value < rhs._value )
      _value += _prime;
    _value = _value - rhs._value;
    return *this;
  }

  ModInt operator*( const ModInt& rhs ) { return ModInt( _value, _prime ) *= rhs; }
  ModInt& operator*=( const ModInt& rhs )
  {
    assert( _prime == rhs._prime );
    _value = ( _value * rhs._value ) % _prime;
    return *this;
  }

  ModInt operator%( const ModInt& rhs ) { return ModInt( _value, _prime ) %= rhs; }
  ModInt& operator%=( const ModInt& rhs )
  {
    assert( _prime == rhs._prime );
    _value = ( _value % rhs._value ) % _prime;
    return *this;
  }

  ModInt operator/( const ModInt& rhs ) { return ModInt( _value, _prime ) /= rhs; }
  ModInt& operator/=( const ModInt& rhs )
  {
    assert( _prime == rhs._prime );
    *this *= rhs.inverse();
    return *this;
  }

  ModInt& operator++()
  {
    _value = ( _value + 1 ) % _prime;
    return *this;
  }

  ModInt& operator--()
  {
    _value = ( _value - 1 ) % _prime;
    return *this;
  }

  bool operator==( const ModInt& rhs )
  {
    assert( _prime == rhs._prime );
    return _value == rhs._value;
  }

  bool operator!=( const ModInt& rhs )
  {
    assert( _prime == rhs._prime );
    return _value != rhs._value;
  }

  // Fermat or Euclid will help us calculate the inverse of x.
  //
  // Euclid's theorem says that `ax + by = gcd(a, b) = 1` since `b` is prime.
  // So `x` will be the inverse of `a`.
  ModInt inverse() const
  {
    int64_t a = _value;
    int64_t b = _prime;
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
      x += _prime;
    }

    return ModInt( x, _prime );
  }

  friend std::ostream& operator<<( std::ostream& stream, const ModInt& obj )
  {
    stream << obj._value;
    return stream;
  }
};

class PowerSums
{
private:
  std::vector<ModInt> _sums {};
  size_t _threshold;

public:
  PowerSums( size_t threshold ) : _threshold( threshold ) { _sums.resize( threshold ); }

  size_t size() const { return _threshold; }
  void add( const ModInt n );
  void remove( const ModInt n );
  PowerSums difference( const PowerSums& other );

  const ModInt& operator[]( int idx ) const { return _sums[idx]; }
  friend std::ostream& operator<<( std::ostream& stream, const PowerSums& obj )
  {
    for ( auto sum : obj._sums ) {
      stream << sum << ' ';
    }
    return stream;
  }
};

class Polynomial
{
private:
  std::vector<ModInt> _coeffs {};

public:
  Polynomial( const PowerSums& sums );
  ModInt eval( ModInt x );

  friend std::ostream& operator<<( std::ostream& stream, const Polynomial& obj )
  {
    for ( auto sum : obj._coeffs ) {
      stream << sum << ' ';
    }
    return stream;
  }
};