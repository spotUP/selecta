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



#include "CombFilter.h"
#include "Util.h"
#include "PI.h"
#include <math.h>

CCombFilter::CCombFilter(int sampleRate, double delayTime, double feedback)
{
  SampleRate = sampleRate;
  DelaySamples = 0;
  DelayBuffer = 0;
  SetDelayTime(delayTime);
  Feedback = feedback;
  ReadOffset = 0.0f;
  ReadOffsetFollow = 0.0f;
  Mode = COMBFILTER_MODE_LOWPASS;
  PastInput[0] = PastInput[1] = 0.0f;
  PastOutput[0] = PastOutput[1] = 0.0f;
}

CCombFilter::~CCombFilter()
{
  delete DelayBuffer;
}

void CCombFilter::CalcCoefficients(double mod)
{
  double cutoff, c, csq, a0, a1, a2, b1, b2;
  double q, d, bw;

  if(Mode != COMBFILTER_MODE_DEFAULT)
  {
      q = sqrtf(2.0f);
      cutoff = fastpow2((CutoffVoltage - 5.0f)) * 440.0f * (mod * 12.0f);
      if(cutoff < 1.0f)
        cutoff = 1.0f;
      else if(cutoff > (SampleRate * 0.25f))
        cutoff = (SampleRate * 0.25f);
      bw = Bandwidth;
  }

  if(Mode == COMBFILTER_MODE_LOWPASS)
  {
    c = 1.0 / (tan(cutoff * (1.0 / SampleRate) * PI));
    csq = c * c;

    a0 = 1.0 / (1.0 + (q * c) + (csq));
    a1 = 2.0 * a0;
    a2 = a0;
    b1 = (2.0 * a0) * (1.0 - csq);
    b2 = a0 * (1.0 - (q * c) + csq);
  }

  if(Mode == COMBFILTER_MODE_HIGHPASS)
  {
    c = tan(cutoff * (1.0 / SampleRate) * PI);
    csq = c * c;

    a0 = 1.0 / (1.0 + (q * c) + (csq));
    a1 = -2.0 * a0;
    a2 = a0;
    b1 = (2.0 * a0) * (csq - 1.0);
    b2 = a0 * (1.0 - (q * c) + csq);
  }

  if(Mode == COMBFILTER_MODE_BANDPASS)
  {
    c = 1.0 / tan(bw * (1.0f / SampleRate) * PI);
    d = 2.0 * cos(2.0 * PI * cutoff * (1.0 / SampleRate));

    a0 = 1.0 / (1.0 + c);
    a1 = 0.0f;
    a2 = -a0;
    b1 = -a0 * c * d;
    b2 = a0 * (c - 1.0);
  }

  if(Mode != COMBFILTER_MODE_DEFAULT)
  {
    Coefficients[0] = a0;
    Coefficients[1] = a1;
    Coefficients[2] = a2;
    Coefficients[3] = b1;
    Coefficients[4] = b2;
  }
}

void CCombFilter::SetBandwidth(double value)
{
  Bandwidth = value;
  CalcCoefficients(0.0f);
}

void CCombFilter::SetCutoffVoltage(double value)
{
  CutoffVoltage = value;
  CalcCoefficients(0.0f);
}

void CCombFilter::SetMode(int value)
{
  int i;

  if(Mode != value)
  {
    if(Mode == COMBFILTER_MODE_DEFAULT)
      for(i = 0; i < 2; i++)
      {
        PastInput[i]  = 0.0f;
        PastOutput[i] = 0.0f;
      }

    Mode = value;

    CalcCoefficients(0.0f);
  }
}

void CCombFilter::SetDelayTime(double value)
{
  int delaySamples, i;
    
  delaySamples = (int)(value * (double)SampleRate);
  if(delaySamples != DelaySamples)
  {
	  DelayTime = value;
	  if(delaySamples < 1)
		delaySamples = 1;
	  if(DelayBuffer != 0)
		delete DelayBuffer;
	  DelaySamples = delaySamples;
	  DelayBuffer = new double[delaySamples];
	  CurrentSample = 0;
	  for(i = 0; i < delaySamples; i++)
		DelayBuffer[i] = 0.0f;
  }
}

void CCombFilter::SetSampleRate(int sampleRate)
{
	if(SampleRate != sampleRate)
	{
		SampleRate = sampleRate;
		SetDelayTime(DelayTime);
	}
}

void CCombFilter::SetFeedback(double value)
{
  Feedback = value;
}

double CCombFilter::GetDelayTime()
{
  return DelayTime;
}

double CCombFilter::GetFeedback()
{
  return Feedback;
}

void CCombFilter::Process(double *input, double *output, double *mod, int samples)
{
  int   currentSample, delaySamples, s1, s2;
  double value, fs1, r1, r2;
  double offset;
  double x0, x1, x2, y0, y1, y2;
  double a0, a1, a2, b1, b2;

  currentSample = CurrentSample;
  delaySamples = DelaySamples;
  offset = ReadOffset;


  x0 = PastInput[0];
  x1 = PastInput[1];
  
  y0 = PastOutput[0];
  y1 = PastOutput[1];

  while(samples--)
  {
    if(mod)
      CalcCoefficients((*(mod++)));

    a0 = Coefficients[0];
    a1 = Coefficients[1];
    a2 = Coefficients[2];
    b1 = Coefficients[3];
    b2 = Coefficients[4];

    value = *(input++);

    //if(mod)
    //{
    //  offset = ReadOffset * *(mod++);
    //  if(offset < 0.0f)
    //    offset = 0.0f;
    //  else if(offset >= DelaySamples)
    //    offset = (double)(DelaySamples - 1);
    //}

    ReadOffsetFollow = AdjustVolume(offset, ReadOffsetFollow);
    fs1 = fmod((currentSample + ReadOffsetFollow), DelaySamples);
    s1 = floor_int(fs1);
    s2 = (s1 >= (DelaySamples - 1)) ? 0 : s1 + 1;
    r2 = fs1 - s1;
    r1 = 1.0f - r2;
    value += ((DelayBuffer[s1] * r1) + (DelayBuffer[s2] * r2)) * Feedback;
    KillDenormal(value);
    
    if(Mode != COMBFILTER_MODE_DEFAULT)
    {
      x2 = x1;
      x1 = x0;
      x0 = value;
      y2 = y1;
      y1 = y0;
      y0 = (a0 * x0) + (a1 * x1) + (a2 * x2) - (b1 * y1) - (b2 * y2);
      KillDenormal(y0);
      value = y0;
    }

    DelayBuffer[currentSample] = value;
	currentSample++;
    if(currentSample >= DelaySamples)
      currentSample = 0;
    *(output++) = value;
  }

  PastInput[0]  = x0;
  PastInput[1]  = x1;
  PastOutput[0] = y0;
  PastOutput[1] = y1;

  CurrentSample = currentSample;
}

void CCombFilter::SetFraction(double value)
{
  if(value < 0.0f)
    value = 0.0f;
  else if(value > 1.0f)
    value = 1.0f;
  ReadOffset = (1.0f - value) * (DelaySamples - 1);
}