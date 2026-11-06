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


#ifdef COREMACHINES
#include "Resource.h"
#include "Core.h"
#else
#ifdef ANTIALIAS
#include "Resource.h"
#include "Core.h"
#else
#ifdef DGENR8
#include "Resource.h"
#include "Core.h"
#else
#ifdef SPATIALVERB
#include "Resource.h"
#include "Core.h"
#define IDR_MINBLEP 0
#else
#ifdef DARKPLUG_HOST
#include "Resource.h"
#endif
#endif
#endif
#endif
#endif

#include "DSP.h"
#include "BLIT.h"
#include "PI.h"
#include "DFT.h"
#include "FFT.h"

#include <math.h>

#define MINBLEP_ZEROCROSSINGS 32
#define MINBLEP_OVERSAMPLING  256
#define WAVE_SAMPLES          8192

CWave *SinTable, *CosTable;

CDSP::CDSP(bool sinCosOnly)
{
  InitWaves(sinCosOnly);
}

CDSP::~CDSP()
{
  FreeWaves();
}

void CDSP::InitWaves(bool sinCosOnly)
{
  int     sample, n;
  double   phase, phaseStep;
  
  n = 20;

  SinTable       = new CWave(WAVE_SAMPLES);
  CosTable       = new CWave(WAVE_SAMPLES);

  if(sinCosOnly)
  {
    TriangleWave = 0;
    SawWave = 0;
    SquareWave = 0;
  }
  else
  {
    TriangleWave   = new CWave(WAVE_SAMPLES);
    SawWave        = new CWave(WAVE_SAMPLES);
    SquareWave     = new CWave(WAVE_SAMPLES);
  }

  phase = 0.0f;
  phaseStep = 1.0f / (double)(WAVE_SAMPLES);

  for(sample = 0; sample < WAVE_SAMPLES; sample++)
  {
    SinTable->Data[sample] = sin(phase * PI * 2.0);
    CosTable->Data[sample] = cos(phase * PI * 2.0);
    
    
    if(!sinCosOnly)
    {
      TriangleWave->Data[sample] = phase < 0.5f ?  (1.0f - (fabs(phase - 0.25f) * 4.0f)) :
                                                  -(1.0f - (fabs(phase - 0.75f) * 4.0f));
      SawWave->Data[sample] = (1.0f - (phase * 2.0f));
      SquareWave->Data[sample] = phase < 0.5f ? 1.0f : -1.0f;

      phase += phaseStep;
    }
  }

  if(sinCosOnly)
  {
    MinBLEP = 0;
    Impulse = 0;
    MinBLAR = 0;
  }
  else
  {
    //MinBLEP = new CMinBLEP(MINBLEP_ZEROCROSSINGS, MINBLEP_OVERSAMPLING, false);
    //MinBLEP->Save("\\MinBLEP[32ZC,256xOS,64].dat");

    MinBLEP = new CMinBLEP(IDR_MINBLEP);
    //Impulse = new CMinBLEP(MINBLEP_ZEROCROSSINGS, MINBLEP_OVERSAMPLING, true);
    Impulse = new CMinBLEP(8, 16, true);
    MinBLAR = new CMinBLEP(MinBLEP);
  }
}

void CDSP::FreeWaves()
{
  delete SinTable;
  delete CosTable;
  if(TriangleWave)
    delete TriangleWave;
  if(SawWave)
    delete SawWave;
  if(SquareWave)
    delete SquareWave;
  if(MinBLEP)
    delete MinBLEP;
  if(Impulse)
    delete Impulse;
  if(MinBLAR)
    delete MinBLAR;
}