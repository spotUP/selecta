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



#include "MultiFilter.h"
#include "Util.h"
#include "PI.h"
#include <math.h>

static double MoogGainTable[199] = { 0.999969, 0.990082, 0.980347, 0.970764, 0.961304, 0.951996, 0.94281, 0.933777, 0.924866, 0.916077, 0.90741, 0.898865, 0.890442, 
0.882141 , 0.873962, 0.865906, 0.857941, 0.850067, 0.842346, 0.834686, 0.827148, 0.819733, 0.812378, 0.805145, 0.798004, 0.790955, 0.783997, 0.77713, 0.770355, 
0.763672, 0.75708 , 0.75058, 0.744141, 0.737793, 0.731537, 0.725342, 0.719238, 0.713196, 0.707245, 0.701355, 0.695557, 0.689819, 0.684174, 0.678558, 0.673035, 
0.667572, 0.66217, 0.65686, 0.651581, 0.646393, 0.641235, 0.636169, 0.631134, 0.62619, 0.621277, 0.616425, 0.611633, 0.606903, 0.602234, 0.597626, 0.593048, 0.588531, 
0.584045, 0.579651, 0.575287 , 0.570953, 0.566681, 0.562469, 0.558289, 0.554169, 0.550079, 0.546051, 0.542053, 0.538116, 0.53421, 0.530334,
0.52652, 0.522736, 0.518982, 0.515289, 0.511627, 0.507996 , 0.504425, 0.500885, 0.497375, 0.493896, 0.490448, 0.487061, 0.483704, 0.480377, 0.477081, 0.473816, 0.470581, 
0.467377, 0.464203, 0.46109, 0.457977, 0.454926, 0.451874, 0.448883, 0.445892, 0.442932, 0.440033, 0.437134, 0.434265, 0.431427, 0.428619, 0.425842, 0.423096, 0.42038, 0.417664, 
0.415009, 0.412354, 0.409729, 0.407135, 0.404572, 0.402008, 0.399506, 0.397003, 0.394501, 0.392059, 0.389618, 0.387207, 0.384827, 0.382477, 0.380127, 0.377808, 0.375488, 0.37323, 
0.370972, 0.368713, 0.366516, 0.364319, 0.362122, 0.359985, 0.357849, 0.355713, 0.353607, 0.351532, 
0.349457, 0.347412, 0.345398, 0.343384, 0.34137, 0.339417, 0.337463, 0.33551, 0.333588, 0.331665, 0.329773, 0.327911, 0.32605, 0.324188, 0.322357, 0.320557, 0.318756, 0.316986, 
0.315216, 0.313446, 0.311707, 0.309998, 0.308289, 0.30658, 0.304901, 0.303223, 0.301575, 0.299927, 0.298309, 0.296692, 0.295074, 0.293488, 0.291931, 0.290375, 0.288818, 0.287262, 
0.285736, 0.284241, 0.282715, 0.28125, 0.279755, 0.27829, 0.276825, 0.275391, 0.273956, 0.272552, 0.271118, 0.269745, 0.268341, 0.266968, 0.265594, 0.264252, 0.262909, 0.261566, 0.260223, 0.258911, 0.257599, 0.256317, 0.255035, 0.25375 };

CMultiFilter::CMultiFilter(int sampleRate, int mode, int poles, double voltage, double resonance)
{
  Scale = 1.0f;
  Algorithm = MULTIFILTER_ALGORITHM_BUTTERWORTH;
  SampleRate = sampleRate;
  Nyquist = (double)sampleRate / 2;
  Mode = mode;
  Poles = poles;
  Sqrt2 = sqrtf(2.0f);
  SetVoltage(voltage);
  SetResonancedB(resonance);
  Resolution = 4;
  CalcCounter = 0;
  Reset();
}

void CMultiFilter::Reset()
{
  int i;

  for(i = 0; i < 4; i++)
  {
    PastInput[i] = 0.0f;
    PastOutput[i] = 0.0f;
  }
}

void CMultiFilter::SetVoltage(double value)
{
  Voltage = value;
  CalcCoefficients = true;
}

void CMultiFilter::SetResonancedB(double value)
{
  double powerRatio;

  Resonance = value;
  powerRatio = dBToPowerRatio(-Resonance);
  Q = Sqrt2 * powerRatio;
  BandWidth = powerRatio;
  CalcCoefficients = true;
}

void CMultiFilter::SetMode(int value)
{
  int i;

  Mode = value;

  for(i = 0; i < 4; i++)
  {
    PastInput[i] = 0.0f;
    PastOutput[i] = 0.0f;
  }
}

void CMultiFilter::SetPoles(int value)
{
  if(Poles != value)
  {
    if(value == MULTIFILTER_POLES_4)
    {
      PastInput[2] = 0;
      PastInput[3] = 0;
      PastOutput[2] = 0;
      PastOutput[3] = 0;
    }
    Poles = value;
  }
}

void CMultiFilter::SetAlgorithm(int value)
{
  Algorithm = value;
}

void CMultiFilter::Process(int samples, double *data, double *modData, double *resData)
{
  if(Algorithm == MULTIFILTER_ALGORITHM_BUTTERWORTH)
    ProcessButterworth(samples, data, modData, resData);
  if(Algorithm == MULTIFILTER_ALGORITHM_MOOG)
    ProcessMoog(samples, data, modData, resData);
}

void CMultiFilter::ProcessButterworth(int samples, double *data, double *modData, double *resData)
{
  int   i, calcCounter;
  double voltage, baseVoltage, cutoff, q, resonance, scale, invSampleRate, dataVal, d, bw;
  double sqrt2, sqrt2m01, invSampleRatePI;
  double x0_0, x1_0, x2_0, y0_0, y1_0, y2_0;
  double x0_1, x1_1, x2_1, y0_1, y1_1, y2_1;
  double c, csq, a0, a1, a2, b1, b2;
  bool  modulate, calcCoefficients;

  if(modData != 0 || resData != 0)
    modulate = true;
  else
    modulate = false;

  calcCoefficients = CalcCoefficients;
  calcCounter      = CalcCounter;

  invSampleRate   = 1.0 / (double)SampleRate;
  invSampleRatePI = invSampleRate * PI;
  sqrt2           = Sqrt2;
  sqrt2m01        = (sqrt2 - 0.1f);

  baseVoltage = Voltage;
  q           = Q;
  bw          = BandWidth;
  resonance   = dBToPowerRatio(-Resonance);
  scale       = dBToPowerRatio(Resonance * -0.5f) * Scale;

  a0 = Coefficients[0];
  a1 = Coefficients[1];
  a2 = Coefficients[2];
  b1 = Coefficients[3];
  b2 = Coefficients[4];

  x0_0 = PastInput[0];
  x1_0 = PastInput[1];

  y0_0 = PastOutput[0];
  y1_0 = PastOutput[1];

  if(Poles == MULTIFILTER_POLES_4)
  {
    x0_1 = PastInput[2];
    x1_1 = PastInput[3];

    y0_1 = PastOutput[2];
    y1_1 = PastOutput[3];
  }

  for(i = 0; i < samples; i++)
  {
    if(modulate)
    {
      if(calcCounter == 0)
      {
        calcCoefficients = true;
        calcCounter      = Resolution - 1;
      }
      else
        calcCounter--;
    }

    if(calcCoefficients)
    {
      calcCoefficients = false;

      if(modData == 0)
        voltage = baseVoltage;
      else
        voltage = baseVoltage + modData[i];

      if(resData != 0)
      {
        double res;
        res         = (Resonance + resData[i]);
        resonance   = dBToPowerRatio(-res);
        scale       = dBToPowerRatio(res * -0.5f) * Scale;

        q  = Sqrt2 * resonance;
        bw = resonance;
      }

      cutoff = fastpow2(voltage - 5.0f) * 440.0f;
      if(cutoff < 1.0f)
        cutoff = 1.0f;
      else if(cutoff > (Nyquist * 0.5f))
        cutoff = (Nyquist * 0.5f);

      switch(Mode)
      {
        case MULTIFILTER_MODE_LP:
        {
          c = 1.0 / (tan(cutoff * invSampleRatePI));
          csq = c * c;

          a0 = (1.0f / (1.0f + (q * c) + (csq)));
          a1 = 2.0f * a0;
          a2 = a0;
          b1 = (2.0f * a0) * (1.0f - csq);
          b2 = a0 * (1.0f - (q * c) + csq);

          break;
        }
        case MULTIFILTER_MODE_HP:
        {
          c = tan(cutoff * invSampleRatePI);
          csq = c * c;

          a0 = 1.0f / (1.0f + (q * c) + (csq));
          a1 = -2.0f * a0;
          a2 = a0;
          b1 = (2.0f * a0) * (csq - 1.0f);
          b2 = a0 * (1.0f - (q * c) + csq);
          
          break;
        }
        case MULTIFILTER_MODE_BP:
        {
          c = 1.0 / tan(bw * cutoff * invSampleRatePI);
          d = 2.0 * cos(2.0f * cutoff * invSampleRatePI);

          a0 = 1.0f / (1.0f + c);
          a1 = 0.0f;
          a2 = -a0;
          b1 = -a0 * c * d;
          b2 = a0 * (c - 1.0f);

          break;
        }
        case MULTIFILTER_MODE_BR:
        {
          c = tan(bw * cutoff * invSampleRatePI);
          d = 2.0 * cos(2.0 * cutoff * invSampleRatePI);

          a0 = 1.0f / (1.0f + c);
          a1 = -a0 * d;
          a2 = a0;
          b1 = -a0 * d;
          b2 = a0 * (1.0f - c);

          break;
        }
      }
    }

    dataVal = data[i];

    x2_0 = x1_0;
    x1_0 = x0_0;
    x0_0 = dataVal;
    y2_0 = y1_0;
    y1_0 = y0_0;
    y0_0 = (a0 * x0_0) + (a1 * x1_0) + (a2 * x2_0) - (b1 * y1_0) - (b2 * y2_0);
    KillDenormal(y0_0);
    dataVal = y0_0;

    if(Poles == MULTIFILTER_POLES_4)
    {
      x2_1 = x1_1;
      x1_1 = x0_1;
      x0_1 = dataVal;
      y2_1 = y1_1;
      y1_1 = y0_1;
      y0_1 = (a0 * x0_1) + (a1 * x1_1) + (a2 * x2_1) - (b1 * y1_1) - (b2 * y2_1);
      KillDenormal(y0_1);
      dataVal = y0_1;
    }

    data[i] = dataVal * scale;
  }

  PastInput[0] = x0_0;
  PastInput[1] = x1_0;

  PastOutput[0] = y0_0;
  PastOutput[1] = y1_0;

  if(Poles == MULTIFILTER_POLES_4)
  {
    PastInput[2] = x0_1;
    PastInput[3] = x1_1;

    PastOutput[2] = y0_1;
    PastOutput[3] = y1_1;
  }

  Coefficients[0] = a0;
  Coefficients[1] = a1;
  Coefficients[2] = a2;
  Coefficients[3] = b1;
  Coefficients[4] = b2;

  CalcCoefficients = calcCoefficients;
  CalcCounter      = calcCounter;
}

static inline double MoogSaturate(double input)
{
  #define _limit 0.95f
  double x1 = fabs( input + _limit );
  double x2 = fabs( input - _limit );
  return 0.5 * (x1 - x2);
}

static inline double MoogCrossfade(double amount, double a, double b) 
{
  return (1-amount)*a + amount*b;
}

void CMultiFilter::ProcessMoog(int samples, double *data, double *modData, double *resData)
{
  int   i, ixint, poles, pole;
  double voltage, baseVoltage, cutoff, invSampleRate, fc, x2, x3, p, ix, ixfrac, q;
  double input, output, temp, resonance;

  baseVoltage = Voltage;
  invSampleRate = 1.0f / (double)SampleRate;
  output = PastInput[0];
  resonance = Resonance;

  for(i = 0; i < samples; i++)
  {
    voltage = baseVoltage + modData[i];
    cutoff = fastpow2(voltage - 5.0f) * 440.0f;
    if(cutoff < 1.0f)
      cutoff = 1.0f;
    else if(cutoff > (Nyquist * 0.5f))
      cutoff = (Nyquist * 0.5f);

    fc = 2 * cutoff * invSampleRate;
    x2 = fc * fc;
    x3 = fc * x2;
    p  = -0.69346f * x3 - 0.59515f * x2 + 3.2937f * fc - 1.0072f; 

    ix = p * 99;
    ixint = (int)floor(ix);
    ixfrac = ix - ixint;
    if(resData != 0)
    {
      resonance = Resonance + resData[i];
      if(resonance < 0.0f)
        resonance = 0.0f;
      else if(resonance > 1.0f)
        resonance = 1.0f;
    }
      q = (resonance * 4.0f) * MoogCrossfade(ixfrac, (double)MoogGainTable[ ixint + 99 ], (double)MoogGainTable[ ixint + 100 ] );

    input = data[i];
    output = 0.25f * (input - output); //negative feedback
          
    if(Poles == MULTIFILTER_POLES_2)
      poles = 2;
    else
      poles = 4;

    for(pole = 0; pole < poles; pole++) 
    {
      temp = PastOutput[pole];
      output = MoogSaturate(output + p * (output - temp));
      PastOutput[pole] = output;
      output = MoogSaturate( output + temp );
    }

    switch(Mode)
    {
      case MULTIFILTER_MODE_HP:
        data[i] = input - output;
        break;
      case MULTIFILTER_MODE_BP:
        data[i] = 3.0f * PastOutput[poles / 2] - output;
        break;
      case MULTIFILTER_MODE_BR:
        data[i] = input - (3.0f * PastOutput[poles / 2] - output);
        break;
      default:
        data[i] = output;
        break;
    }
    output *= q;  //scale the feedback
  }

  PastInput[0] = output;
}

void CMultiFilter::SetSampleRate(int sampleRate)
{
  SampleRate = sampleRate;
  Nyquist = (double)sampleRate / 2;
  CalcCoefficients = true;
}

void CMultiFilter::SetScale(double scale)
{
  Scale = scale;
}