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

#ifndef UTIL_H
#define UTIL_H

#include <math.h>
#include <windows.h>

#ifdef WIN64
	#define SSE_ALIGNED(ptr)         ((((__int64)ptr) & 0x0F) == 0)
#else
	#define SSE_ALIGNED(ptr)         ((((__int32)ptr) & 0x0F) == 0)
#endif

#define SSE_ALIGNED2(ptr1, ptr2) (SSE_ALIGNED(ptr1) && SSE_ALIGNED(ptr2))

double      dBToPowerRatio(double dB);
double      PowerRatioTodB(double powerRatio);
char       *CopyString(char *string);
char       *CopyString(char *string, int *length);
void        ShowWebPage(char *url);
void        AddValueToBlock(double *block, double value, int samples, int cpuFeatures);
void        MixBlocks(double *block1, double *block2, int samples, double amount, bool replace, bool multiply, int cpuFeatures);
void        CopyFixedString(int length, char *src, char *dst);

inline double fastpow2(double x) { return pow(2.0, x); }

inline bool TestDenormal(double val)
{
  const long long unsigned int x = *reinterpret_cast<const long long int *>(&val);
  const long long unsigned int fraction       = x & (0x000FFFFFFFFFFFFF);
  const long long unsigned int biasedExponent = x & (0x7FF0000000000000);
  
  return ((biasedExponent == 0) && (fraction != 0));
}

inline void KillDenormal(double &val)
{
  if(TestDenormal(val))
    val = 0.0;
}


inline COLORREF MixColors(COLORREF color1, COLORREF color2, double mix)
{
  BYTE  r, g, b;
  double mix2;

  if(mix < 0.0f)
    mix = 0.0f;
  else if(mix > 1.0f)
    mix = 1.0f;
  mix2 = 1.0f - mix;

  r = (BYTE)((GetRValue(color1) * mix2) + (GetRValue(color2) * mix));
  g = (BYTE)((GetGValue(color1) * mix2) + (GetGValue(color2) * mix));
  b = (BYTE)((GetBValue(color1) * mix2) + (GetBValue(color2) * mix));
  return RGB(r, g, b);
}

inline int PropertyToInt(double value, int count)
{
  int i;

  i = (int)(value * count);
  if(i < 0)
    i = 0;
  if(i >= count)
    i = count - 1;
  return i;
}

inline double FrequencyToVoltage(double frequency)
{
  return ((log(frequency) - log(440.0)) * (1.0 / log(2.0))) + 5.0;
}

inline double VoltageToFrequency(double voltage)
{
  return fastpow2(voltage - 5.0) * 440.0;
}

#define VOLUME_VELOCITY        0.0004

inline double AdjustVolume(double powerRatio, double powerRatioFollow)
{
    if(powerRatio != powerRatioFollow)
    {
      if(powerRatioFollow < powerRatio)
      {
        powerRatioFollow += (0.1 + (powerRatio - powerRatioFollow)) * VOLUME_VELOCITY;
        if(powerRatioFollow > powerRatio)
          powerRatioFollow = powerRatio;
      }
      else 
      {
        powerRatioFollow -= (0.1 + (powerRatioFollow - powerRatio)) * VOLUME_VELOCITY;
        if(powerRatioFollow < powerRatio)
          powerRatioFollow = powerRatio;
      }
    }
    return powerRatioFollow;
}

inline double CurvePoint(double a, double b, double t, double curve, double scale)
{
  if(curve == 0.0f)
    return a + ((b - a) * t);
  else
    return a + ((b - a) * (1.0 - exp(t * curve)) * scale);
}

inline double CurvePointInv(double a, double b, double y, double curve, double scale)
{
  if(curve == 0.0f)
    return (y - a) / (b - a);
  else
   return log(0.0f - (((y - a) / (b - a) / scale) - 1.0)) / curve;
}

inline double CurveScale(double curve)
{
  if(curve == 0.0f)
    return 1.0f;
  else
    return 1.0f / (1.0f - exp(curve));
}

inline int round_int(double x)
{
  return (int)(x + 0.5f);
}

#define floor_int(x) (int)floor(x)
#define ceil_int(x) (int)ceild(x);

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) (((a) < (b)) ? (b) : (a))
#endif

#endif