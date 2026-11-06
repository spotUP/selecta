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

#ifndef VCO_H
#define VCO_H

#include "Wave.h"
#include "MinBLEP.h"

#define VCO_SHAPE_SINE     0
#define VCO_SHAPE_TRIANGLE 1
#define VCO_SHAPE_SAW      2
#define VCO_SHAPE_SQUARE   3
#define VCO_SHAPE_IMPULSE  4

class CVCO
{
public:
       CVCO(int sampleRate, CWave *wave, double frequency, CMinBLEP *minBLEP, CMinBLEP *impulse);
      ~CVCO();
  void Generate(double *modIn, double *sigOut, int samples);
  void Generate(double *modIn, double *sigOut, int samples, CWave *wave2, double *mixIn, double mixLevel);
  void Generate(double *sigOut, int samples);
  void GenerateSH(double *sigOut, int samples);
  void GenerateSG(double *sigOut, int samples);
  void Generate(double *pulseWidthIn, double *syncIn, double *fmIn, double *fmInLinear, double fmLinearScale, double *sigOut, double *syncOut, int samples);
  void GenerateBLEP(double *pwIn, double *syncIn, double *fmIn, double *sigOut, double *syncOut, int samples);
  void SetFrequency(double value);
  void SetFrequency(double value, double semitone);
  void SetFrequency(double value, double semitone, double detune);
  void SetWave(CWave *wave);
  void SetVoltage(double value);
  void SetShape(int shape);
  void AddBLEP(double offset, double amp, bool impulse);
  void SetPhase(double value);
  void SetSampleRate(int sampleRate);

private:
  int       SampleRate, Shape, Resolution;
  int       BufferInit, BufferOffset, BufferCount;
  double    *Buffer;
  double     Phase, Value, Frequency, Voltage, Log440, InvLog2, Sample1, Sample2, Integrator1, Integrator2, Leaky;
  CWave    *Wave, *SinTable;
  CMinBLEP *MinBLEP, *Impulse;
};

#endif