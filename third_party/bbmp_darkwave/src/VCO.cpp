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



#include "VCO.h"
#include "Core.h"
#include "PI.h"
#include "Util.h"
#include <math.h>
#include <stdlib.h>

CVCO::CVCO(int sampleRate, CWave *wave, double frequency, CMinBLEP *minBlep, CMinBLEP *impulse)
{
  Resolution = 4;
  SampleRate = sampleRate;
  SetWave(wave);
  Log440       = log(440.0); 
  InvLog2      = 1.0f / logf(2.0f);
  SetFrequency(frequency);
  Phase        = 0.0f;
  Value        = 0.0f;
  Sample1      = Sample2 = 0.0f;
  MinBLEP      = minBlep;
  Impulse      = impulse;
  Buffer       = 0;
  BufferOffset = 0;
  BufferInit   = 0;
  BufferCount  = 0;
  if(MinBLEP != 0)
  {
    BufferCount = (MinBLEP->ZeroCrossings * 2) + 1;
    Buffer = new double[BufferCount];
  }
}

CVCO::~CVCO()
{
  if(Buffer != 0)
    delete Buffer;
}

void CVCO::Generate(double *sigOut, int samples)
{
  double phaseStep, phase;
  
  phase = Phase;
  phaseStep = Frequency / ((double)SampleRate);
  while(samples--)
  {
    *(sigOut++) = Wave->GetSample(phase);
    phase += phaseStep;
    if(phase >= 1.0f)
      phase -= 1.0f;
  }
  Phase = phase;
}

void CVCO::GenerateSH(double *sigOut, int samples)
{
  double phaseStep, phase;
  
  phase = Phase;
  phaseStep = Frequency / ((double)SampleRate);
  while(samples--)
  {
    *(sigOut++) = Sample1;
    phase += phaseStep;
    if(phase >= 1.0f)
    {
      Sample1 = (((double)rand() / (double)RAND_MAX) - 0.5f) * 2.0f;
      phase -= 1.0f;
    }
  }
  Phase = phase;
}

void CVCO::GenerateSG(double *sigOut, int samples)
{
  double phaseStep, phase;
  
  phase = Phase;
  phaseStep = Frequency / ((double)SampleRate);
  while(samples--)
  {
    *(sigOut++) = Sample1 + ((Sample2 - Sample1) * phase);
    phase += phaseStep;
    if(phase >= 1.0f)
    {
      phase -= 1.0f;
      Sample1 = Sample2;
      Sample2 = (((double)rand() / (double)RAND_MAX) - 0.5f) * 2.0f;
    }
  }
  Phase = phase;
}

inline double Blit(double phase, double aotn, double tnp1)
{
  double denom, num, blit;

  denom = SinTable->GetSample(phase);
  //  denom = sinf(phase * PI);
  if(denom > 0.0002f || denom < -0.0002f)
  {
    num  = SinTable->GetSample(tnp1 * phase);
    // num  = sinf(tnp1 * phase * PI);
    blit = aotn * ((num / denom) - 1.0f);
  }
  else
    blit = 1.0f;
  return blit;
}

// BandLimited Waveform Generation Using BandLimited Impulse Trains

void CVCO::Generate(double *pwIn, double *syncIn, double *fmIn, double *fmInLinear, double fmLinearScale, double *sigOut, double *syncOut, int samples)
{
  double phase, out;
  double baseVoltage, invSampleRate, invSampleRate2;
  double pw, freq, blit, bpblit, aotn, tnp1, saw, square, triangle;
  int   n;
  bool  retrig;

  baseVoltage    = Voltage;
  invSampleRate  = (1.0f / ((double)SampleRate));
  invSampleRate2 = invSampleRate * 0.5f;
  phase          = Phase;
  freq           = Frequency;
  pw             = 0.5f;
  retrig         = true;

  while(samples--)
  {
    if(fmIn != 0)
    {
      if(retrig)
      {
        freq = fastpow2((baseVoltage + *(fmIn++)) - 5.0f) * 440.0f;
        if(fmInLinear != 0 && fmLinearScale != 0.0f)
          freq *= 1.0f + ((*(fmInLinear++)) * fmLinearScale);
        if(freq < 1.0f)
          freq = 1.0f;
        if(freq > 0.25f * (double)SampleRate)
          freq = 0.25f * (double)SampleRate;
        retrig = false;
      }
      else
      {
        fmIn++;
        if(fmInLinear != 0)
          fmInLinear++;
      }
    }

    if(pwIn != 0)
    {
      pw = (1.0f + *(pwIn++)) * 0.5f;
      if(pw < 0.0f)
        pw = 0.0f;
      else if(pw > 1.0f)
        pw = 1.0f;
    }

    n    = 20; //(int)((double)SampleRate / 2.0f / freq);
    tnp1 = ((double)n * 2.0f) + 1.0f;
    aotn = 1.0f / (2.0f * (double)n);

    switch(Shape)
    {
      // Triangle Wave
      case VCO_SHAPE_TRIANGLE:
        blit     = Blit(phase, aotn, tnp1);
        bpblit   = blit - Blit(phase + (pw * 0.5f), aotn, tnp1); 
        square   = bpblit + (Leaky * Integrator1);
        triangle = square + (Leaky * Integrator2);
        Integrator1 = square;
        Integrator2 = triangle;
        out =  triangle / 600.0f / (0.1f + pw) *	0.24680113792419434f	* (double)sqrt(freq);
        
        break;

      // Square Wave
      case VCO_SHAPE_SQUARE:

        blit   = Blit(phase, aotn, tnp1);
        bpblit = blit - Blit(phase + (pw * 0.5f), aotn, tnp1); 
        square = bpblit + (Leaky * Integrator1);
        Integrator1 = square;
        out = square * 0.5f;
        
        break;

      // Saw Wave
      case VCO_SHAPE_SAW:
        blit = Blit(phase, aotn, tnp1);
        saw = blit + (Leaky * Integrator1);
        Integrator1 = saw;
        out = saw * 0.687f;
        
        break;

      // Sine Wave
      default:
        out = Wave->GetSample(phase);
        break;
    }
    
    *(sigOut++) = out;
        
    if(Shape == VCO_SHAPE_SINE)
      phase += freq * invSampleRate;
    else
      phase += freq * invSampleRate2;

    if(phase > 1.0f)
    {
      phase -= 1.0f;
      retrig = true;
      if(syncOut != 0)
        *(syncOut++) = 0.0f; 
    }
    else
      if(syncOut != 0)
        *(syncOut++) = phase;

    if(syncIn != 0)
      if(*(syncIn++) == 0.0f)
        phase = 0.0f;
  }
  Phase = phase;
}

// BandLimited Waveform Generation Using MinBLEPs

void CVCO::GenerateBLEP(double *pwIn, double *syncIn, double *fmIn, double *sigOut, double *syncOut, int samples)
{
  double fs, phase, value, out;
  double baseVoltage, invSampleRate;
  double pw, freq, resetPhase;
  bool  reset;
  int   calcCounter;

  baseVoltage    = Voltage;
  invSampleRate  = (1.0f / ((double)SampleRate));
  phase          = Phase;
  value          = Value;
  freq           = Frequency;
  pw             = 0.5f;
  fs             = freq * invSampleRate;
  calcCounter    = 0;

  while(samples--)
  {
    if(fmIn != 0)
    {
      if(calcCounter == 0)
      {
        freq = fastpow2((baseVoltage + *(fmIn++)) - 5.0f) * 440.0f;
        
        if(freq < 1.0f)
          freq = 1.0f;
        if(freq > 0.25f * (double)SampleRate)
          freq = 0.25f * (double)SampleRate;
        fs = freq * invSampleRate;
        calcCounter = Resolution - 1;
      }
      else
        calcCounter--;
    }

    if(pwIn != 0)
    {
      pw = (1.0f + *(pwIn++)) * 0.5f;
      if(pw < 0.0f)
        pw = 0.0f;
      else if(pw > 1.0f)
        pw = 1.0f;
    }
            
    phase += fs;

    if(syncIn != 0)
      if(*(syncIn++) == 0.0f)
      {
        reset      = true;
        resetPhase = 0.0f;
      }
      else
        reset = false;
    else
      reset = false;

    if(!reset)
      if(phase >= 1.0f)
      {
        reset = true;
        resetPhase = phase - 1.0f;
      }

    if(reset)
    {
      if(syncOut != 0)
        *(syncOut++) = 0.0f;
      AddBLEP(resetPhase / fs, Shape == VCO_SHAPE_SAW ? phase : 1.0f, Shape == VCO_SHAPE_IMPULSE);
      phase = resetPhase;
      value = 0.0f;
    }
    else
      if(syncOut != 0)
        *(syncOut++) = 1.0f;

    if(Shape == VCO_SHAPE_IMPULSE)
    {
      if((value == 0.0f) && (phase > pw))
      {
        value = 1.0f;
        AddBLEP((phase - pw) / fs, -1.0f, true);
      }
      out = 0.0f;
    }
    else
    {
      if(Shape == VCO_SHAPE_SQUARE || Shape == VCO_SHAPE_TRIANGLE)
      {
        if((value == 0.0f) && (phase > pw))
        {
          value = 1.0f;
          AddBLEP((phase - pw) / fs, -1.0f, false);
        }
        out = value;
      }
      else
        out = phase;
    }

    if(BufferInit != 0)
    {
      out += Buffer[BufferOffset];
      BufferInit--;
      if(++BufferOffset >= BufferCount)
        BufferOffset = 0;
    }

    if(Shape == VCO_SHAPE_IMPULSE)
      *(sigOut++) = out;
    else
      *(sigOut++) = (2.0f * out) - 1.0f;
  }
  Phase = phase;
  Value = value;
}

void CVCO::Generate(double *modIn, double *sigOut, int samples)
{
  double phase, invSampleRate440, baseVoltage;
  
  baseVoltage = Voltage;
  phase = Phase;
  invSampleRate440 = (440.0f / ((double)SampleRate));
  while(samples--)
  {
    *(sigOut++) = Wave->GetSample(phase);
    phase += fastpow2((*(modIn++) + baseVoltage) - 5.0f) * invSampleRate440;
    if(phase >= 1.0f)
      phase -= 1.0f;
  }
  Phase = phase;
}

void CVCO::Generate(double *modIn, double *sigOut, int samples, CWave *wave2, double *mixIn, double mixLevel)
{
  double phase, invSampleRate440, baseVoltage, mix, fs;
  
  baseVoltage = Voltage;
  phase = Phase;
  invSampleRate440 = (440.0 / ((double)SampleRate));
  fs = fastpow2(baseVoltage - 5.0) * invSampleRate440;
  mix = mixLevel;

  while(samples--)
  {
    if(mixIn)
      mix = *(mixIn++) * mixLevel;
    *(sigOut++) = ((1.0 - mix) * Wave->GetSample(phase)) + (mix * wave2->GetSample(phase));
    if(modIn != 0)
      fs = fastpow2((*(modIn++) + baseVoltage) - 5.0) * invSampleRate440;
    phase += fs;
    if(phase >= 1.0)
      phase -= 1.0;
  }
  Phase = phase;
}

void CVCO::SetWave(CWave *wave)
{
  Wave = wave;
}

void CVCO::SetFrequency(double value)
{
  Frequency = value;
  Voltage = ((log(value) - Log440) * InvLog2) + 5.0f;
}

void CVCO::SetVoltage(double value)
{
  Voltage = value;
  Frequency = fastpow2(value - 5.0f) * 440.0f;
}

void CVCO::SetFrequency(double frequency, double semitone)
{
  int   semitones;
  double ratio;

  semitones = (int)((semitone - 0.5f) * (8.0f * 12.0f));
  ratio = fastpow2(fabs((double)semitones) / 12.0f);

  if(semitones < 0)
    SetFrequency(frequency / ratio);
  else
    SetFrequency(frequency * ratio);
}

void CVCO::SetFrequency(double frequency, double semitone, double detune)
{
  int   semitones;
  double realSemitones;
  double ratio;

  semitones = (int)((semitone - 0.5f) * (8.0f * 12.0f));
  realSemitones = ((double)semitones) + detune;
  ratio = fastpow2(fabs(realSemitones) / 12.0);

  if(realSemitones < 0.0f)
    SetFrequency(frequency / ratio);
  else
    SetFrequency(frequency * ratio);
}

void CVCO::SetShape(int shape)
{
  if(Shape != shape)
  {
    if(shape == VCO_SHAPE_TRIANGLE)
      Leaky = 0.995f; 
    else
      Leaky = 0.999f;

    if(shape == VCO_SHAPE_SAW)
      Integrator1 = -0.5f;
    else
      Integrator1 = 0.0f;

    Integrator2 = 0.0f;
    Phase       = 0.0f;
    Shape       = shape;
  }
}

void CVCO::AddBLEP(double offset, double amp, bool impulse)
{
  int       i, os;
  double    *lpOut, *lpIn, frac, *lpOutEnd, *lpInEnd, f;
  CMinBLEP *minBLEP;

  if(impulse)
    minBLEP = Impulse;
  else
  {
    if(Shape == VCO_SHAPE_TRIANGLE)
      minBLEP = Core->DSP->MinBLAR;
    else
      minBLEP = MinBLEP;
  }

  os       = minBLEP->OverSampling;
  lpOut    = Buffer + BufferOffset;
  lpIn     = minBLEP->Wave->Data + (int)(os * offset);
  frac     = fmod(os * offset, 1.0); 
  lpOutEnd = Buffer + BufferCount;
  lpInEnd  = minBLEP->Wave->Data + (minBLEP->Wave->Samples - 1);

	// add
	for (i = 0; i < BufferInit && lpIn < lpInEnd; i++, lpIn += os, lpOut++)
	{
		if(lpOut >= lpOutEnd)
      lpOut = Buffer;
		f = lpIn[0] + ((lpIn[1] - lpIn[0]) * frac);
		*lpOut += amp * (1.0f - f);
	}

	// copy
	for (; lpIn < lpInEnd; lpIn += os, lpOut++)
	{
		if(lpOut >= lpOutEnd) 
      lpOut = Buffer;
		f = lpIn[0] + ((lpIn[1] - lpIn[0]) * frac);
		*lpOut = amp * (1.0f - f);
    BufferInit++;
  }
}

void CVCO::SetPhase(double value)
{
  Phase = value;
}

void CVCO::SetSampleRate(int sampleRate)
{
  SampleRate = sampleRate;
}