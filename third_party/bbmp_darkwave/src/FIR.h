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

#ifndef FIR_H
#define FIR_H

class CStereoFIR
{
public:
         CStereoFIR(int samples, double *leftImpulseResponse, double *rightImpulseResponse, bool optimized);
         CStereoFIR(int samples, bool optimized);
        ~CStereoFIR();
  void   SetImpulseResponse(double *leftImpulseResponse, double *rightImpulseResponse, int fadeSamples);
  void   Process(double in, double *outLeft, double *outRight, double *delayOut, int delayOffset);
  void   Process(double inLeft, double inRight, double *outLeft, double *outRight, double *delayOut, int delayOffset);
  double ImpulseResponse(int sample);
  void   Ping(int os);

  double *DelayBufferL, *DelayBufferR;
  int    DelayOffset, Samples;

  int    LeftTapCount, *LeftTapPoints, RightTapCount, *RightTapPoints;
  double *LeftTaps, *RightTaps;
  
  int    LeftTapCountOld, *LeftTapPointsOld, RightTapCountOld, *RightTapPointsOld;
  double *LeftTapsOld, *RightTapsOld;

  int    FadeSamples;
  double  FadeBalance, FadeBalanceInc;

  bool   Optimized;
  double *LeftIR, *RightIR;
  double  Scale;

inline void Process(double inLeft, double inRight, double *outLeft, double *outRight)
{
  int   tap, ds, dsmtms;
  double outl, outr;

  outl = outr = 0.0;
  DelayBufferL[DelayOffset] = inLeft;
  DelayBufferR[DelayOffset] = inRight;
  ds = DelayOffset + Samples;

  for(tap = 0; tap < Samples; tap++)
  {
    dsmtms = (ds - tap) % Samples;
    outl += DelayBufferL[dsmtms] * LeftIR[tap];
    outr += DelayBufferR[dsmtms] * RightIR[tap];
  }

  DelayOffset = (DelayOffset + 1) % Samples;

  *outLeft  += outl * Scale;
  *outRight += outr * Scale;
};

inline void Process(double inLeft, double inRight)
{
  DelayBufferL[DelayOffset] = inLeft;
  DelayBufferR[DelayOffset] = inRight;
  DelayOffset = (DelayOffset + 1) % Samples;
};

};

#endif