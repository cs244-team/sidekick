#include "quack.hh"

void PowerSums::add( const ModInt n )
{
  ModInt tmp = n;
  for ( size_t i = 0; i < _threshold; i++ ) {
    _sums[i] += tmp;
    tmp *= n;
  }
}

void PowerSums::remove( const ModInt n )
{
  ModInt tmp = n;
  for ( size_t i = 0; i < _threshold; i++ ) {
    _sums[i] -= tmp;
    tmp *= n;
  }
}

PowerSums PowerSums::difference( const PowerSums& other )
{
  PowerSums diff( _threshold );
  for ( size_t i = 0; i < _threshold; i++ ) {
    diff._sums[i] = _sums[i] - other._sums[i];
  }
  return diff;
}

// Newton's identities
//
// Coefficients of the polynomial with roots x1,...,xn can
// be represented in terms of the power sums (p1,...,pn) as:
//  e0 = 1
// -e1 =                           - p1
//  e2 = (1/2)*(              e1p1 - p2)
// -e3 = (1/3)*(     - e2p1 + e1p2 - p3)
// -e4 = (1/4)*(e3p1 - e2p2 + e1p3 - p4)
// ... and so on
//
// The resulting polynomial will look like:
// e_0*x^n - e_1*x^(n-1) + e_2*x^(n-2) + ... + e_n*x^0
Polynomial::Polynomial( const PowerSums& sums )
{
  _coeffs.resize( sums.size() + 1 );
  _coeffs[0] = 1; // x^n

  for ( size_t i = 1; i < _coeffs.size(); i++ ) {
    for ( size_t j = 1; j < i; j++ ) {
      _coeffs[i] -= _coeffs[i - j] * sums[j - 1];
    }

    _coeffs[i] -= sums[i - 1];
    _coeffs[i] /= i;
  }
}

// Evaluates a polynomial at `x` via Horner's method. `coeffs` should be ordered
// from highest degree to lowest.
ModInt Polynomial::eval( ModInt x )
{
  ModInt y = 0;
  for ( auto coeff : _coeffs ) {
    y = y * x + coeff;
  }
  return y;
}