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

#ifndef COMBFILTER_H
#define COMBFILTER_H

#define COMBFILTER_MODE_DEFAULT  0
#define COMBFILTER_MODE_LOWPASS  1
#define COMBFILTER_MODE_HIGHPASS 2
#define COMBFILTER_MODE_BANDPASS 3

class CCombFilter
{
public:
        CCombFilter(int sampleRate, double delayTime, double feedback);
       ~CCombFilter();
  void  SetDelayTime(double value);
  void  SetFeedback(double value);
  double GetDelayTime();
  double GetFeedback();
  void  Process(double *input, double *output, double *mod, int samples);
  void  SetFraction(double value);
  void  SetMode(int value);
  void  CalcCoefficients(double mod);
  void  SetCutoffVoltage(double value);
  void  SetBandwidth(double value);
  void  SetSampleRate(int sampleRate);

private:
  double Bandwidth;
  int   Mode, SampleRate, DelaySamples, CurrentSample;
  double DelayTime, Feedback, *DelayBuffer, ReadOffset, ReadOffsetFollow, CutoffVoltage;
  double PastInput[2], PastOutput[2], Coefficients[5];
};

#endif