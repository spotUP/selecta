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



#include "FIR.h"
#include "Error.h"
#include <math.h>

CStereoFIR::CStereoFIR(int samples, double *leftImpulseResponse, double *rightImpulseResponse, bool optimized)
{
  int i;
  
  Optimized        = optimized;
  LeftIR           = 0;
  RightIR          = 0;
  Samples          = samples;
  LeftTapCount     = 0;
  LeftTaps         = 0;
  LeftTapPoints    = 0;
  RightTapCount    = 0;
  RightTaps        = 0;
  RightTapPoints   = 0;
  DelayOffset      = 0;
  DelayBufferL     = new double[samples];
  DelayBufferR     = new double[samples];
  FadeSamples      = 0;

  SetImpulseResponse(leftImpulseResponse, rightImpulseResponse, 0);

  for(i = 0; i < samples; i++)
    DelayBufferL[i] = DelayBufferR[i] = 0.0f;
}

CStereoFIR::CStereoFIR(int samples, bool optimized)
{
  int i;

  Scale            = 1.0;
  Samples          = samples;
  LeftTapCount     = 0;
  LeftTaps         = 0;
  LeftTapPoints    = 0;
  RightTapCount    = 0;
  RightTaps        = 0;
  RightTapPoints   = 0;
  DelayOffset      = 0;
  DelayBufferL     = new double[samples];
  DelayBufferR     = new double[samples];
  FadeSamples      = 0;
  Optimized        = optimized;
  LeftIR           = 0;
  RightIR          = 0;

  for(i = 0; i < samples; i++)
    DelayBufferL[i] = DelayBufferR[i] = 0.0f;
}

CStereoFIR::~CStereoFIR()
{
  if(FadeSamples != 0)
  {
    if(LeftTapsOld != 0)
      delete LeftTapsOld;
    if(LeftTapPointsOld != 0)
      delete LeftTapPointsOld;
    if(RightTapsOld != 0)
      delete RightTapsOld;
    if(RightTapPointsOld != 0)
      delete RightTapPointsOld;
  }

  delete DelayBufferL;
  delete DelayBufferR;
  if(LeftTaps != 0)
    delete LeftTaps;
  if(LeftTapPoints != 0)
    delete LeftTapPoints;
  if(RightTaps != 0)
    delete RightTaps;
  if(RightTapPoints != 0)
    delete RightTapPoints;
  if(LeftIR != 0)
    delete LeftIR;
  if(RightIR != 0)
    delete RightIR;
}

void CStereoFIR::SetImpulseResponse(double *leftImpulseResponse, double *rightImpulseResponse, int fadeSamples)
{
  int i, leftTap, rightTap;

  if(Optimized)
  {

    if(fadeSamples > 0 && FadeSamples == 0)
    {
      LeftTapsOld       = LeftTaps;
      LeftTapPointsOld  = LeftTapPoints;
      RightTapsOld      = RightTaps;
      RightTapPointsOld = RightTapPoints;
      LeftTapCountOld   = LeftTapCount;
      RightTapCountOld  = RightTapCount;

      FadeSamples    = fadeSamples;
      FadeBalance    = 0.0;
      FadeBalanceInc = 1.0 / (double)fadeSamples;
    }
    else
    {
      if(LeftTaps != 0)
        delete LeftTaps;
      if(LeftTapPoints != 0)
        delete LeftTapPoints;
      if(RightTaps != 0)
        delete RightTaps;
      if(RightTapPoints != 0)
        delete RightTapPoints;
    }

    LeftTapCount = 0;
    RightTapCount = 0;
    
    for(i = 0; i < Samples; i++)
    {
      if(leftImpulseResponse[i] != 0.0)
        LeftTapCount++;
      if(rightImpulseResponse[i] != 0.0)
        RightTapCount++;
    }

    LeftTaps = new double[LeftTapCount];
    LeftTapPoints = new int[LeftTapCount];
    RightTaps = new double[RightTapCount];
    RightTapPoints = new int[RightTapCount];

    leftTap = rightTap = 0;
    for(i = 0; i < Samples; i++)
    {
      if(leftImpulseResponse[i] != 0.0)
      {
        LeftTaps[leftTap] = leftImpulseResponse[i];
        LeftTapPoints[leftTap++] = i; 
      }
      if(rightImpulseResponse[i] != 0.0)
      {
        RightTaps[rightTap] = rightImpulseResponse[i];
        RightTapPoints[rightTap++] = i; 
      }
    }
  }
  else
  {
    if(LeftIR)
      delete LeftIR;
    if(RightIR)
      delete RightIR;

    LeftIR  = new double[Samples];
    RightIR = new double[Samples];

    for(i = 0; i < Samples; i++)
    {
      LeftIR[i]  = leftImpulseResponse[i];
      RightIR[i] = rightImpulseResponse[i];
    }
  }
}

void CStereoFIR::Process(double inLeft, double inRight, double *outLeft, double *outRight, double *delayOut, int delayOffset)
{
  int    tap, ds, dsmtms;
  double outl, outr;

  outl = outr = 0.0f;

  if(delayOut != 0)
  {
    *delayOut = DelayBufferL[(DelayOffset + Samples + delayOffset) % Samples];
    DelayBufferL[DelayOffset] = inLeft;
    DelayBufferR[DelayOffset] = inRight;
    outl = outr = 0.0f;
  
	  ds = DelayOffset + Samples;

    for(tap = 0; tap < Samples; tap++)
    {
	    dsmtms = (ds - tap) % Samples;
	    outl += DelayBufferL[dsmtms] * LeftIR[tap];
      outr += DelayBufferR[dsmtms] * RightIR[tap];
    }

    DelayOffset = (DelayOffset + 1) % Samples;
  }
  else
  {
    for(tap = 0; tap < Samples; tap++)
    {
      outl += inLeft * LeftIR[tap];
      outr += inRight * RightIR[tap];
    }
  }

 *outLeft  += outl * Scale;
 *outRight += outr * Scale;
}

void CStereoFIR::Process(double in, double *outLeft, double *outRight, double *delayOut, int delayOffset)
{
  int   tap;
  double outl, outr, fade2;

  if(!Optimized)
  {
    outl = outr = 0.0f;

    for(tap = 0; tap < Samples; tap++)
    {
      outl += in * LeftIR[tap];
      outr += in * RightIR[tap];
     
    }

   *outLeft  += outl;
   *outRight += outr;

    return;
  }

  if(delayOut != 0)
  {
    *delayOut = DelayBufferL[(DelayOffset + Samples + delayOffset) % Samples];
    DelayBufferL[DelayOffset] = in;
    DelayBufferR[DelayOffset] = in;
    outl = outr = 0.0;
  
    for(tap = 0; tap < LeftTapCount; tap++)
      outl += DelayBufferL[(DelayOffset + Samples - LeftTapPoints[tap]) % Samples] * LeftTaps[tap];
    for(tap = 0; tap < RightTapCount; tap++)
      outr += DelayBufferR[(DelayOffset + Samples - RightTapPoints[tap]) % Samples] * RightTaps[tap];
  }
  else
  {
    outl = outr = 0.0;
    for(tap = 0; tap < LeftTapCount; tap++)
      outl += in * LeftTaps[tap];
    for(tap = 0; tap < RightTapCount; tap++)
      outr += in * RightTaps[tap];
  }

  if(FadeSamples > 0)
  {
    outl *= FadeBalance;
    outr *= FadeBalance;  
    fade2 = (1.0f - FadeBalance);

    for(tap = 0; tap < LeftTapCountOld; tap++)
      outl += fade2 * DelayBufferL[(DelayOffset + Samples - LeftTapPointsOld[tap]) % Samples] * LeftTapsOld[tap];
    for(tap = 0; tap < RightTapCountOld; tap++)
      outr += fade2 * DelayBufferR[(DelayOffset + Samples - RightTapPointsOld[tap]) % Samples] * RightTapsOld[tap];

    if(--FadeSamples == 0)
    {
      if(LeftTapsOld != 0)
        delete LeftTapsOld;
      if(LeftTapPointsOld != 0)
        delete LeftTapPointsOld;
      if(RightTapsOld != 0)
        delete RightTapsOld;
      if(RightTapPointsOld != 0)
        delete RightTapPointsOld;
    }
    else
    {
      FadeBalance += FadeBalanceInc;
    }
  }

  DelayOffset = (DelayOffset + 1) % Samples;
  *outLeft += outl;
  *outRight += outr;
}
double CStereoFIR::ImpulseResponse(int sample)
{
  int    tap, pos;
  double response;

  response = 0.0;
  for(tap = 0; tap < LeftTapCount; tap++)
  {
    pos = LeftTapPoints[tap];
    if(pos == sample)
    {
      response += LeftTaps[tap];
      break;
    }
    else if(pos > sample)
      break;
  }
  for(tap = 0; tap < RightTapCount; tap++)
  {
    pos = RightTapPoints[tap];
    if(pos == sample)
    {
      response += RightTaps[tap];
      break;
    }
    else if(pos > sample)
      break;
  }
  return response;
}

void CStereoFIR::Ping(int os)
{
  double f;
  int    i;

  f = 0.0;

  for(i = 0; i < Samples; i += os)
    f += LeftIR[i];
  if(f != 0.0)
    Scale = 1.0 / fabs(f);
  else
    Scale = 1.0;
}