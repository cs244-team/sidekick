#include "quack.hh"

void PowerSums::add( const ModInt n )
{
  ModInt tmp = n;
  for ( size_t i = 0; i < threshold_; i++ ) {
    sums_[i] += tmp;
    tmp *= n;
  }
}

void PowerSums::remove( const ModInt n )
{
  ModInt tmp = n;
  for ( size_t i = 0; i < threshold_; i++ ) {
    sums_[i] -= tmp;
    tmp *= n;
  }
}

PowerSums PowerSums::difference( const PowerSums& other )
{
  PowerSums diff( threshold_ );
  for ( size_t i = 0; i < threshold_; i++ ) {
    diff.sums_[i] = sums_[i] - other.sums_[i];
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
  coeffs_.resize( sums.size() + 1 );
  coeffs_[0] = 1; // x^n

  for ( size_t i = 1; i < coeffs_.size(); i++ ) {
    for ( size_t j = 1; j < i; j++ ) {
      coeffs_[i] -= coeffs_[i - j] * sums[j - 1];
    }

    coeffs_[i] -= sums[i - 1];
    coeffs_[i] /= i;
  }
}

// Evaluates a polynomial at `x` via Horner's method. `coeffs` should be ordered
// from highest degree to lowest.
ModInt Polynomial::eval( ModInt x )
{
  ModInt y = 0;
  for ( auto coeff : coeffs_ ) {
    y = y * x + coeff;
  }
  return y;
}