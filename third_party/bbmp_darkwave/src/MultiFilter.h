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

#ifndef MULTIFILTER_H
#define MULTIFILTER_H

#define MULTIFILTER_MODE_LP 0
#define MULTIFILTER_MODE_HP 1
#define MULTIFILTER_MODE_BP 2
#define MULTIFILTER_MODE_BR 3

#define MULTIFILTER_POLES_2 0
#define MULTIFILTER_POLES_4 1

#define MULTIFILTER_ALGORITHM_BUTTERWORTH 0
#define MULTIFILTER_ALGORITHM_MOOG        1

class CMultiFilter
{
public:
       CMultiFilter(int sampleRate, int mode, int poles, double voltage, double resonance);
  void SetVoltage(double value);
  void SetResonancedB(double value);
  void SetMode(int value);
  void SetPoles(int value);
  void SetAlgorithm(int value);
  void Process(int samples, double *data, double *modData, double *resData);
  void SetSampleRate(int sampleRate);
  void SetScale(double scale);
  void Reset();

private:
  void ProcessButterworth(int samples, double *data, double *modData, double *resData);
  void ProcessMoog(int samples, double *data, double *modData, double *resData);

  double PastInput[4], PastOutput[4], Coefficients[5];
  double Nyquist, Voltage, Resonance, Q, BandWidth, Sqrt2, Scale;
  int   SampleRate, Mode, Poles, Algorithm, Resolution, CalcCounter;
  bool  CalcCoefficients;
};

#endif