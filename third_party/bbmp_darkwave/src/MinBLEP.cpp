/*  ExperimentalScene DarkWave Studio
    Copyright (C) 2004-2012 Daniel Werner
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


#ifdef DGENR8
#include "Resource.h"
#else
#include "Resource.h"
#endif
#include "MinBLEP.h"
#include "Sinc.h"
#include "MinimumPhase.h"
#include "Core.h"
#include "Error.h"
#include <math.h>
#include <stdio.h>

struct MinBLEPHeader
{
  int ZeroCrossings;
  int OverSampling;
  int Samples;
};

CMinBLEP::CMinBLEP(int zeroCrossings, int overSampling, bool impulseOnly)
{
  int    i, n;
  double  r, a, b;
  double *buffer1, *buffer2;

  ZeroCrossings = zeroCrossings;
  OverSampling  = overSampling;
  n = (ZeroCrossings * 2 * OverSampling) + 1;

  buffer1 = new double[n];
  buffer2 = new double[n];
  
  // Generate Sinc

  a = (double)-ZeroCrossings;
  b = (double)ZeroCrossings;
  for(i = 0; i < n; i++)
  {
    r = ((double)i) / ((double)(n - 1));
    buffer1[i] = SINC(a + (r * (b - a)));
  }

  // Window Sinc

  BlackmanWindow(n, buffer2);
  for(i = 0; i < n; i++)
    buffer1[i] *= buffer2[i];

  // Minimum Phase Reconstruction

  RealCepstrum(n, buffer1, buffer2);
  MinimumPhase(n, buffer2, buffer1);

  if(impulseOnly)
  {
    // Copy Minimum Phase Impulse Into Wave

    Wave = new CWave(n);
    for(i = 0; i < n; i++)
      Wave->Data[i] = buffer1[i];
  }
  else
  {
    // Integrate Into MinBLEP

    Wave = new CWave(n);
    a = 0.0f;
    for(i = 0; i < n; i++)
    {
      a += buffer1[i];
      Wave->Data[i] = a;
    }

    // Normalize
    a = Wave->Data[n - 1];
    a = 1.0f / a;
    for(i = 0; i < n; i++)
      Wave->Data[i] *= a;
  }

  delete buffer1;
  delete buffer2;
}

CMinBLEP::CMinBLEP(int resourceID)
{  
#ifndef DW_HEADLESS
  HGLOBAL        resource;
  MinBLEPHeader *header;
  int            i, samples;
  double         *data;

  resource = LoadResource(AppInstance, FindResourceA(AppInstance, MAKEINTRESOURCEA(resourceID), "BINARY"));
  header = (MinBLEPHeader*)(LockResource(resource));

  ZeroCrossings = header->ZeroCrossings;
  OverSampling  = header->OverSampling;
  samples       = header->Samples;


  Wave = new CWave(samples);
  data = (double*)(header+1);
  for(i = 0; i < samples; i++)
    Wave->Data[i] = data[i];

  FreeResource(resource);
#else
  extern const char DarkWaveMinBLEP[];
  const MinBLEPHeader *header = (const MinBLEPHeader*)DarkWaveMinBLEP;
  ZeroCrossings = header->ZeroCrossings;
  OverSampling  = header->OverSampling;
  Wave = new CWave(header->Samples);
  memcpy(Wave->Data, header + 1, sizeof(double) * header->Samples);
#endif
}

CMinBLEP::CMinBLEP(CMinBLEP *minBLEP)
{
  int   samples, i;
  double a;

  ZeroCrossings = minBLEP->ZeroCrossings;
  OverSampling  = minBLEP->OverSampling;
  samples  = (ZeroCrossings * 2 * OverSampling) + 1;

  // Integrate

  Wave = new CWave(samples);
  a = 0.0f;
  for(i = 0; i < samples; i++)
  {
    a += minBLEP->Wave->Data[i];
    Wave->Data[i] = a;
  }

  // Normalize
  a = Wave->Data[samples - 1];
  a = 1.0f / a;
  for(i = 0; i < samples; i++)
    Wave->Data[i] *= a;
}

CMinBLEP::~CMinBLEP()
{
  delete Wave;
}

void CMinBLEP::Save(char *path)
{
  MinBLEPHeader h;
  FILE         *f;
  size_t        n;

  f = fopen(path, "wb");
  if(f == 0)
    ThrowString("Can't Open File <%s>", path);

  h.OverSampling  = OverSampling;
  h.ZeroCrossings = ZeroCrossings;
  h.Samples       = Wave->Samples;

  n = fwrite(&h, sizeof(MinBLEPHeader), 1, f);
  if(n != 1)
    ThrowString("Can't Write To File <%s>", path);

  n = fwrite(Wave->Data, Wave->Samples * sizeof(double), 1, f);
  if(n != 1)
    ThrowString("Can't Write To File <%s>", path);

  fclose(f);
}

