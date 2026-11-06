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



#include "Slide.h"
#include "Core.h"
#include "Util.h"

CSlide::CSlide(int sampleRate, double glide, double portamento, double curve)
{
  Log440  = logf(440.0f); 
  InvLog2 = 1.0f / logf(2.0f);
  SampleRate = (double)sampleRate;
  SetGlide(glide);
  SetPortamento(portamento);
  SetCurve(curve);
  VoltageA = 0.0f;
  VoltageB = 0.0f;
  Voltage  = 0.0f;
  Active   = false;
  Time     = 0.0f;
  Sliding  = false;
}

CSlide::~CSlide()
{
}

void CSlide::SetGlide(double value)
{
  Glide = value;
}

void CSlide::SetPortamento(double value)
{
  Portamento = value;
}

void CSlide::SetCurve(double value)
{
  Curve = value;
  CurveScale = ::CurveScale(value);
}

void CSlide::Trigger(unsigned char note)
{
  double frequency;

  Core->Host->GetNoteFrequency(note, &frequency);
  Trigger(FrequencyToVoltage(frequency));
}

void CSlide::Trigger(double voltage)
{
  double time;

  if(!Active)
  {
    VoltageA = voltage;
    VoltageB = voltage;
    Voltage  = voltage;
    Active   = true;
  }
  else
  {
    VoltageA  = Voltage;
    VoltageB  = voltage;
    time      = (Portamento + (fabs(VoltageB - VoltageA) * Glide));
    if(time > 0.0f)
    {
      SlideRate = (1.0f / time) / SampleRate;
      Sliding   = true;
    }
    else
    {
      SlideRate = 1.0f;
      Sliding = false;
      Voltage = VoltageB;
    }
    Time      = 0.0f;
  }
}

void CSlide::Generate(int samples, double *out)
{
  Generate(samples, out, true);
}

void CSlide::Generate(int samples, double *out, bool offset)
{
  if(Sliding)
  {
    while(samples-- > 0)
    {
      Time += SlideRate;
      if(Time > 1.0)
        Time = 1.0;

      Voltage = CurvePoint(VoltageA, VoltageB, Time, Curve, CurveScale);
      *(out++) = offset ? Voltage - VoltageB : Voltage;

      if(Time == 1.0)
      {
        Sliding = false;
        break;
      }
    }
  }
  while(samples-- > 0)
    *(out++) = offset ? 0.0 : Voltage;
}

double CSlide::FrequencyToVoltage(double frequency)
{
  return ((log(frequency) - Log440) * InvLog2) + 5.0f;
}

void CSlide::SetSampleRate(int sampleRate)
{
  SampleRate = (double)sampleRate;
}
