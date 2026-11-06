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

#ifndef WAVE_H
#define WAVE_H

#include "Util.h"

class CWave
{
public:
               CWave(int samples);
              ~CWave();
  inline double GetSample(double phase);
  void         AdjustDC();
  void         Normalize();

  double *Data;
  int    Samples;
};

inline double CWave::GetSample(double phase)
{
  double fsamples, fs1, r2;
  int   s1, s2;

  if(phase >= 1.0f)
    phase -= (double)(floor_int(phase));

  fsamples = (double)Samples;
  fs1      = fsamples * phase;
  s1       = floor_int(fs1);
  s2       = s1 >= (Samples - 1) ? 0 : s1 + 1;
  r2       = fs1 - (double)s1;
  return Data[s1] + ((Data[s2] - Data[s1]) * r2);
}

#endif