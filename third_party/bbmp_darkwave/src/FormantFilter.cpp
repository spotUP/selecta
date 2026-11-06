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

#include <malloc.h>

#include "FormantFilter.h"
#include "Util.h"


const double PeakTable[VOWEL_COUNT][FORMANT_COUNT] = 
{
 { 136, 266, 2293, 2937 }, //   Male IY 
 { 135, 392, 1993, 2568 }, //   Male IH 
 { 128, 526, 1854, 2481 }, //   Male EH 
 { 125, 664, 1727, 2420 }, //   Male AE
 { 129, 631, 1192, 2377 }, //   Male AH
 { 124, 718, 1090, 2442 }, //   Male AA
 { 127, 568,  836, 2403 }, //   Male AO
 { 136, 437, 1023, 2245 }, //   Male UH
 { 139, 307,  876, 2239 }, //   Male UW
 { 132, 489, 1360, 1709 }, //   Male ER

 { 231, 310, 2783, 3312 }, // Female IY
 { 232, 441, 2474, 3063 }, // Female IH  
 { 220, 608, 2334, 2999 }, // Female EH  
 { 208, 863, 2049, 2832 }, // Female AE  
 { 223, 758, 1409, 2768 }, // Female AH  
 { 212, 864, 1229, 2783 }, // Female AA  
 { 216, 587,  915, 2736 }, // Female AO  
 { 234, 469, 1162, 2685 }, // Female UH  
 { 235, 378,  961, 2666 }, // Female UW  
 { 219, 503, 1641, 1977 }, // Female ER  

 { 800, 1150, 2900, 3900 },  // Soprano 'A'
 { 350, 2000, 2800, 3600 }, // Soprano 'E'
 { 270, 2140, 2950, 3900 }, // Soprano 'I'
 { 450,  800, 2830, 3800 }, // Soprano 'O'
 { 325,  700, 2700, 3800 }, // Soprano 'U'

 { 800, 1150, 2800, 3500 }, // Alto 'A'
 { 400, 1600, 2700, 3300 }, // Alto 'E'
 { 350, 1700, 2700, 3700 }, // Alto 'I'
 { 450,  800, 2830, 2500 },  // Alto 'O'
 { 325,  700, 2530, 3500 }, // Alto 'U'

 { 660, 1120, 2750, 3000 }, // Counter 'A'
 { 440, 1800, 2700, 3000 }, // Counter 'E'
 { 270, 1850, 2900, 3350 }, // Counter 'I'
 { 430,  820, 2700, 3000 }, // Counter 'O'
 { 370,  630, 2750, 3000 }, // Counter 'U'

 { 650, 1080, 2650, 2900 }, // Tenor 'A'
 { 400, 1700, 2600, 3200 }, // Tenor 'E'
 { 290, 1870, 2800, 3250 }, // Tenor 'I'
 { 400,  800, 2600, 2800 }, // Tenor 'O'
 { 350,  600, 2700, 2900 }, // Tenor 'U'
 
 { 600, 1040, 2250, 2450 }, // Bass 'A'
 { 400, 1620, 2400, 2800 }, // Bass 'E'
 { 250, 1750, 2600, 3050 }, // Bass 'I' 
 { 400,  750, 2400, 2600 }, // Bass 'O'
 { 350,  600, 2400, 2675 }, // Bass 'U'
};

const char* VowelTable[VOWEL_COUNT] =
{
 "Male IY", 
 "Male IH", 
 "Male EH", 
 "Male AE",
 "Male AH",
 "Male AA",
 "Male AO",
 "Male UH",
 "Male UW",
 "Male ER",
 "Female IY",
 "Female IH",  
 "Female EH",  
 "Female AE",  
 "Female AH",  
 "Female AA",  
 "Female AO",  
 "Female UH",  
 "Female UW",  
 "Female ER",  
 "Soprano A",
 "Soprano E",
 "Soprano I",
 "Soprano O",
 "Soprano U",
 "Alto A",
 "Alto E",
 "Alto I",
 "Alto O",
 "Alto U",
 "Counter A",
 "Counter E",
 "Counter I",
 "Counter O",
 "Counter U",
 "Tenor A",
 "Tenor E",
 "Tenor I",
 "Tenor O",
 "Tenor U",
 "Bass A",
 "Bass E",
 "Bass I",
 "Bass O",
 "Bass U"
};

#ifdef OLD_TABLE

const short FormantTable[VOICE_COUNT][VOWEL_COUNT][FORMANT_COUNT][PARAMETER_COUNT] =
{
  { // Soprano
    { // A
      {800,   0,   80}, // f1
      {1150, -6,   90}  // f2
    },
    { // E
      {350,    0,  60}, // f1
      {2000, -20, 100}  // f2
    },
    { // I
      {270,    0,  60}, // f1
      {2140, -12,  90}  // f2
    },
    { // O
      {450,    0,  70}, // f1
      {800,  -11,  80}  // f2
    },
    { // U
      {325,    0,  50}, // f1
      {700,  -16,  60}  // f2
    }
  },
  { // Alto
    { // A
      {800,   0,   80}, // f1
      {1150, -4,   90}  // f2
    },
    { // E
      {400,    0,  60}, // f1
      {1600, -24,  80}  // f2
    },
    { // I
      {350,    0,  50}, // f1
      {1700, -20, 100}  // f2
    },
    { // O
      {450,    0,  70}, // f1
      {800,   -9,  80}  // f2
    },
    { // U
      {325,    0,  50}, // f1
      {700,  -12,  60}  // f2
    }
  }
};

#endif

CFormantFilter::CFormantFilter(int sampleRate, int blockSamples, int vowel)
{
  int i;
  Buffer = 0;
  SlideBuffer = 0;
  BlockSamples = blockSamples;
  SampleRate = sampleRate;

  for(i = 0; i < FORMANT_COUNT; i++)
  {
    Filters[i] = new CMultiFilter(sampleRate, MULTIFILTER_MODE_BP, MULTIFILTER_POLES_2, 0.0f, 10.0f);
    Filters[i]->SetScale(16.0f);
    Slides[i] = new CSlide(sampleRate, 0.5f, 0.0f, 0.0f);
  }
  SetBlockSamples(blockSamples);
  SampleRate = sampleRate;
  SetVowel(vowel);
}

CFormantFilter::~CFormantFilter()
{
  int i;

  SetBlockSamples(0);
  for(i = 0; i < FORMANT_COUNT; i++)
  {
    delete Filters[i];
    delete Slides[i];
  }
}

void CFormantFilter::SetBlockSamples(int blockSamples)
{
  if(Buffer != 0)
  {
     _mm_free(Buffer);
     Buffer = 0;
  }
  if(SlideBuffer != 0)
  {
    _mm_free(SlideBuffer);
    SlideBuffer = 0;
  }
  if(blockSamples != 0)
  {
    Buffer = (double*)_mm_malloc(sizeof(double) * blockSamples, 16);
    SlideBuffer = (double*)_mm_malloc(sizeof(double) * blockSamples, 16);
  }
  BlockSamples = blockSamples;
}

void CFormantFilter::SetSampleRate(int sampleRate)
{
  int i;

  if(sampleRate != SampleRate)
  {
    for(i = 0; i < FORMANT_COUNT; i++)
    {
      Filters[i]->SetSampleRate(sampleRate);
      Slides[i]->SetSampleRate(sampleRate);
    }
    SampleRate = sampleRate;
  }
}

void CFormantFilter::Process(int samples, double *in, double *out)
{
  int i, j;

  for(i = 0; i < samples; i++)
    out[i] = 0.0f;

  for(i = 0; i < FORMANT_COUNT; i++)
  {
    for(j = 0; j < samples; j++)
      Buffer[j] = in[j];
    Slides[i]->Generate(samples, SlideBuffer, false);
    Filters[i]->Process(samples, Buffer, SlideBuffer, 0);
    for(j = 0; j < samples; j++)
      out[j] += Buffer[j];
  }
}

void CFormantFilter::SetVowel(int vowel)
{
  int   i;
  double frequency;

  if(Vowel != vowel)
  {
    Vowel = vowel;

    for(i = 0; i < FORMANT_COUNT; i++)
    {
      frequency = (double)PeakTable[vowel][i];
      Slides[i]->Trigger(FrequencyToVoltage(frequency));
    }
  }
}