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



#include "Blit.h"
#include <math.h>

#define PI 3.14159265358979f

void BLIT(int samples, double cycles, int harmonics, double *out)
{
  int   i, k;
  double amp, time, interval, sum, piT2;
  
  time     = 0.0f;
  interval = cycles / (double)samples;
  amp      = 1.0f / (double)harmonics;
  piT2     = PI * 2.0f;

  for(i = 0; i < samples; i++)
  {
    sum = 0.0f;
    for(k = 1; k <= harmonics; k++)
      sum += cos(piT2 * (double)k * time);
    out[i] = amp * sum;
    time += interval;
  }
}

void BPBLIT(int samples, double cycles, int harmonics, double pulseWidth, double *in, double *out)
{
  int   i, k;
  double amp, time, interval, sum, piT2;
  
  time     = 0.0f;
  interval = cycles / (double)samples;
  amp      = 1.0f / (double)harmonics;
  piT2     = PI * 2.0f;

  for(i = 0; i < samples; i++)
  {
    sum = 0.0f;
    for(k = 1; k <= harmonics; k++)
    {
      sum += cos(piT2 * (double)k * (time + pulseWidth));
    }
    out[i] = in[i] - (amp * sum);
    time  += interval;
  }
}