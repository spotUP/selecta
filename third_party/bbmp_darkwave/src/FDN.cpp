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



#include "FDN.h"
#include "Util.h"
#include "PI.h"

CFDN::CFDN(int sampleRate, int *delayLengths, double feedback, double gain, double cutoff)
{
  int i, j, n;

  SampleRate = sampleRate;

  for(i = 0; i < 4; i++)
  {
    n = delayLengths[i];
    DelayLengths[i] = n;
    DelayBuffers[i] = new double[n];
    for(j = 0; j < n; j++)
      DelayBuffers[i][j] = 0.0;
    DelayOffsets[i] = 0;

    PastInput[i][0]  = 0.0;
    PastInput[i][1]  = 0.0;
    PastOutput[i][0] = 0.0;
    PastOutput[i][1] = 0.0;
  }

  Feedback       = feedback;
  FeedbackFollow = feedback;
  Gain           = gain;
  GainFollow     = gain;
  SetCutoff(cutoff);
}

CFDN::~CFDN()
{
  int i;

  for(i = 0; i < 4; i++)
    delete DelayBuffers[i];
}

void CFDN::SetFeedback(double value)
{
  Feedback = value;
}

void CFDN::SetGain(double value)
{
  Gain = value;
}

void CFDN::SetCutoff(double value)
{
  double nd2, voltage, cutoff, q, c, csq, a0, a1, a2, b1, b2;

  Cutoff  = value;
  nd2     = 0.25 * (double)(SampleRate);
  voltage = value * FrequencyToVoltage(nd2);
  cutoff  = VoltageToFrequency(voltage);
  q       = sqrt(2.0);

  if(cutoff < 1.0)
    cutoff = 1.0;
  else if(cutoff > nd2)
    cutoff = nd2;

  c   = 1.0 / tan(cutoff * ((1.0 / (double)SampleRate) * PI));
  csq = c * c;

  a0  = 1.0 / (1.0 + (q * c) + (csq));
  a1  = 2.0 * a0;
  a2  = a0;
  b1  = (2.0 * a0) * (1.0 - csq);
  b2  = a0 * (1.0 - (q * c) + csq);

  Coefficients[0] = a0;
  Coefficients[1] = a1;
  Coefficients[2] = a2;
  Coefficients[3] = b1;
  Coefficients[4] = b2;
}

CFDN *CFDN::Clone()
{
  return new CFDN(SampleRate, DelayLengths, Feedback, Gain, Cutoff);
}

void CFDN::Process(double inLeft, double inRight, double *outLeft, double *outRight)
{
  double delayInput[4], delayOutput[4], gain, value, feed, feedback;
  int   i, delayOffset;
  double a0, a1, a2, b1, b2, x0, x1, x2, y0, y1, y2;

  GainFollow     = AdjustVolume(Gain, GainFollow);
  gain           = 0.5f * GainFollow;
  FeedbackFollow = AdjustVolume(Feedback, FeedbackFollow);
  feedback       = FeedbackFollow;

  a0 = Coefficients[0];
  a1 = Coefficients[1];
  a2 = Coefficients[2];
  b1 = Coefficients[3];
  b2 = Coefficients[4];

  for(i = 0; i < 4; i++)
  {
    value = DelayBuffers[i][DelayOffsets[i]];
    
    x0 = PastInput[i][0];
    x1 = PastInput[i][1];
  
    y0 = PastOutput[i][0];
    y1 = PastOutput[i][1];

    x2 = x1;
    x1 = x0;
    x0 = value;
    y2 = y1;
    y1 = y0;
    y0 = (a0 * x0) + (a1 * x1) + (a2 * x2) - (b1 * y1) - (b2 * y2);

    KillDenormal(y0);

    PastInput[i][0]  = x0;
    PastInput[i][1]  = x1;
    
    PastOutput[i][0] = y0;
    PastOutput[i][1] = y1;
    
    delayOutput[i]   = y0;
  }
  
  *outLeft  = (delayOutput[0] + delayOutput[2]) * gain;
  *outRight = (delayOutput[1] + delayOutput[3]) * gain;
  
  feed = 0.0;
  for(i = 0; i < 4; i++)
  {
    value = delayOutput[i];
    delayInput[i] = value * feedback;
    feed += value;
  }

  feed *= -0.5 * feedback;

  delayInput[0] += inLeft;
  delayInput[2] += inLeft;
  delayInput[1] += inRight;
  delayInput[3] += inRight;

  for(i = 0; i < 4; i++)
  {
    value = delayInput[i] + feed;
    KillDenormal(value);
    delayOffset = DelayOffsets[i];
    DelayBuffers[i][delayOffset++] = value;
    if(delayOffset >= DelayLengths[i])
      delayOffset = 0;
    DelayOffsets[i] = delayOffset;
  }
}