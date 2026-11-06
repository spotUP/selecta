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

#ifndef FORMANT_FILTER_H
#define FORMANT_FILTER_H

#include "MultiFilter.h"
#include "Slide.h"

#define FORMANT_COUNT 4
#define VOWEL_COUNT   45

extern const double PeakTable[VOWEL_COUNT][FORMANT_COUNT];
extern const char* VowelTable[VOWEL_COUNT];

class CFormantFilter
{
public:
      CFormantFilter(int sampleRate, int blockSamples, int vowel);
     ~CFormantFilter();
 void SetBlockSamples(int blockSamples);
 void SetSampleRate(int sampleRate);
 void Process(int samples, double *in, double *out);
 void SetVowel(int vowel);

private:
  double        *Buffer, *SlideBuffer;
  CMultiFilter *Filters[FORMANT_COUNT];
  CSlide       *Slides[FORMANT_COUNT];
  int           BlockSamples, SampleRate;
  int           Voice, Vowel;
};


#endif