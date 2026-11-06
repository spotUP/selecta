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



#include "VCA.h"
#include "Util.h"
#include <math.h>

CVCA::CVCA(int mode, double scale)
{
  Mode = mode;
  Scale = scale;
}

void CVCA::Process(int samples, double *data, double *modData)
{
  double scale;

  scale = 1.0f / Scale;

  if(Mode == VCA_MODE_EXPONENTIAL)
  {
    // TODO: fastpow2 could be generalized for this case (only values between 0..1)
    while(samples-- > 0)
      *(data++) *= fastpow2((*(modData++) * scale)) - 1.0f;
  }
  else
    while(samples-- > 0)
      *(data++) *= *(modData++) * scale;
}