/*  ExperimentalScene DarkWave Studio
    Copyright (C) 2004-2012 Daniel Werner
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



#include "Wave.h"
#include <math.h>

CWave::CWave(int samples)
{
  Data = new double[samples];
  Samples = samples;
}

CWave::~CWave()
{
  delete Data;
}

void CWave::AdjustDC()
{
  double total, average;
  int   i;

  total = 0.0f;
  for(i = 0; i < Samples; i++)
    total += Data[i];
  average = total / (double)Samples;
  for(i = 0; i < Samples; i++)
    Data[i] -= average;
}

void CWave::Normalize()
{
  double value, max, scale;
  int   i;

  max = 0.0f;
  for(i = 0; i < Samples; i++)
  {
    value = fabs(Data[i]);
    if(value > max)
      max = value;
  }
  if(max > 0.0f)
  {
    scale = 1.0f / max;
    for(i = 0; i < Samples; i++)
      Data[i] *= scale;
  }
}
