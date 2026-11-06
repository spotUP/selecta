/*  ExperimentalScene DarkWave Studio
    Copyright (C) 2004-2011 Daniel Werner
    http://www.experimentalscene.com/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SINC_H
#define SINC_H

#include "PI.h"
#include <math.h>

inline double SINC(double x)
{
  double pix;

  if(x == 0.0f)
    return 1.0f;
  else
  {
    pix = PI * x;
    return sin(pix) / pix;
  }
}

inline void BlackmanWindow(int n, double *w)
{
  int   m = n - 1;
  int   i;
  double f1, f2, fm;

  fm = (double)m;
  for(i = 0; i <= m; i++)
  {
    f1 = 2.0f * PI * (double)i / fm;
    f2 = 2.0f * f1;
    w[i] = 0.42 - (0.5 * cos(f1)) + (0.08 * cos(f2));
  }
}

#endif