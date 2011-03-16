/* Copyright 2010 Matthew Arsenault, Travis Desell, Boleslaw
Szymanski, Heidi Newberg, Carlos Varela, Malik Magdon-Ismail and
Rensselaer Polytechnic Institute.

This file is part of Milkway@Home.

Milkyway@Home is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Milkyway@Home is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Milkyway@Home.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "nbody_priv.h"
#include "milkyway_util.h"

mwvector cartesianToLbr_rad(mwvector r, real sunGCDist)
{
    mwvector lbR;

    const real xp = X(r) + sunGCDist;

    L(lbR) = mw_atan2(Y(r), xp);
    B(lbR) = mw_atan2( Z(r), mw_sqrt( sqr(xp) + sqr(Y(r)) ) );
    R(lbR) = mw_sqrt(sqr(xp) + sqr(Y(r)) + sqr(Z(r)));
    W(lbR) = 0.0;

    if (L(lbR) < 0.0)
        L(lbR) += M_2PI;

    return lbR;
}

mwvector cartesianToLbr(mwvector r, real sunGCDist)
{
    mwvector lbR;
    lbR = cartesianToLbr_rad(r, sunGCDist);
    L(lbR) = r2d(L(lbR));
    B(lbR) = r2d(B(lbR));

    return lbR;
}

static inline mwvector _lbrToCartesian(const real l, const real b, const real r, const real sun)
{
    mwvector cart;

    X(cart) = r * mw_cos(l) * mw_cos(b) - sun;
    Y(cart) = r * mw_sin(l) * mw_cos(b);
    Z(cart) = r * mw_sin(b);
    W(cart) = 0.0;

    return cart;
}

mwvector lbrToCartesian_rad(mwvector lbr, real sunGCDist)
{
    return _lbrToCartesian(L(lbr), B(lbr), R(lbr), sunGCDist);
}

mwvector lbrToCartesian(mwvector lbr, real sunGCDist)
{
    return _lbrToCartesian(d2r(L(lbr)), d2r(B(lbr)), R(lbr), sunGCDist);
}

