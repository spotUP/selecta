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

#include <math.h>
#include <memory.h>
#include "PI.h"
#include "EQ.h"
#include "Util.h"

// static double vsa = (1.0 / 4294967295.0);   // Very small amount (Denormal Fix)

CEQ::CEQ(double lowFreq, double highFreq, double sampleRate)
{
  lg = lgFollow = 1.0;
  mg = mgFollow = 1.0;
  hg = hgFollow = 1.0;
  aux1Follow = aux2Follow = 0.0;
  gainFollow = 1.0;
  SetSampleRate(lowFreq, highFreq, sampleRate);
}

void CEQ::SetSampleRate(double lowFreq, double highFreq, double sampleRate)
{
  lf = 2 * sin(PI * ((double)lowFreq / (double)sampleRate));
  hf = 2 * sin(PI * ((double)highFreq / (double)sampleRate));

  f1p0 = f1p1 = f1p2 = f1p3 = 0.0;
  f2p0 = f2p1 = f2p2 = f2p3 = 0.0;
  
  sdm1 = sdm2 = sdm3 = 0.0;
}

void CEQ::SetGain(double low, double mid, double high)
{
  lg = low;
  mg = mid;
  hg = high;
}

void CEQ::Work(double *in, double *out, double *outAux1, double *outAux2, int samples, double gain, double aux1, double aux2, double *max)
{
  double   l,m,h;   // Low / Mid / High - Sample Values
  int      i;
  double   sample;
  double   lmh;

  *max = 0.0;
  
  for(i = 0; i < samples; i++)
  {
    sample = in[i];
    
    gainFollow = AdjustVolume(gain, gainFollow);

    // Filter 1 - Low Pass

    f1p0  += (lf * (sample   - f1p0));// + vsa;
    KillDenormal(f1p0);
    f1p1  += (lf * (f1p0 - f1p1));
    KillDenormal(f1p1);
    f1p2  += (lf * (f1p1 - f1p2));
    KillDenormal(f1p2);
    f1p3  += (lf * (f1p2 - f1p3));
    KillDenormal(f1p3);

    l          = f1p3;

    // Filter #2 (highpass)
    
    f2p0  += (hf * (sample   - f2p0));// + vsa;
    KillDenormal(f2p0);
    f2p1  += (hf * (f2p0 - f2p1));
    KillDenormal(f2p1);
    f2p2  += (hf * (f2p1 - f2p2));
    KillDenormal(f2p2);
    f2p3  += (hf * (f2p2 - f2p3));
    KillDenormal(f2p3);

    h          = sdm3 - f2p3;

    // Calculate midrange (signal - (low + high))

    m          = sdm3 - (h + l);

    // Scale, Combine and store

    lgFollow = AdjustVolume((double)lg, (double)lgFollow);
    mgFollow = AdjustVolume((double)mg, (double)mgFollow);
    hgFollow = AdjustVolume((double)hg, (double)hgFollow);

    l         *= lgFollow;
    m         *= mgFollow;
    h         *= hgFollow;

    KillDenormal(l);
    KillDenormal(m);
    KillDenormal(h);
    
    // Shuffle history buffer

    sdm3   = sdm2;
    sdm2   = sdm1;
    sdm1   = sample;                

    KillDenormal(sdm1);
    
    // Return Result

    lmh = (double)(l + m + h);
      
    aux1Follow = AdjustVolume(aux1, aux1Follow);
    aux2Follow = AdjustVolume(aux2, aux2Follow);

    outAux1[i] += (aux1Follow * lmh);
    outAux2[i] += (aux2Follow * lmh);
    
    lmh *= gainFollow;
    KillDenormal(lmh);
    if(*max < fabs(lmh))
      *max = fabs(lmh);
    out[i] += lmh;
  }
}

void CEQ::Work(double *in, double *out, int samples, double gain, double *max)
{
  double  l,m,h;      // Low / Mid / High - Sample Values
  int     i;
  double   sample;
  double   lmh;

  *max = 0.0;

  for(i = 0; i < samples; i++)
  {
    sample = in[i];

    gainFollow = AdjustVolume(gain, gainFollow);

    // Filter 1 - Low Pass

    f1p0  += (lf * (sample   - f1p0));
    KillDenormal(f1p0);
    f1p1  += (lf * (f1p0 - f1p1));
    KillDenormal(f1p1);
    f1p2  += (lf * (f1p1 - f1p2));
    KillDenormal(f1p2);
    f1p3  += (lf * (f1p2 - f1p3));
    KillDenormal(f1p3);

    l          = f1p3;

    // Filter 2 - High Pass
    
    f2p0  += (hf * (sample   - f2p0));
    KillDenormal(f2p0);
    f2p1  += (hf * (f2p0 - f2p1));
    KillDenormal(f2p1);
    f2p2  += (hf * (f2p1 - f2p2));
    KillDenormal(f2p2);
    f2p3  += (hf * (f2p2 - f2p3));
    KillDenormal(f2p3);

    h          = sdm3 - f2p3;

    // Calculate Mid Range (Signal - (Low + High))

    m          = sdm3 - (h + l);

    // Scale, Combine and store

    lgFollow = AdjustVolume((double)lg, (double)lgFollow);
    mgFollow = AdjustVolume((double)mg, (double)mgFollow);
    hgFollow = AdjustVolume((double)hg, (double)hgFollow);

    l         *= lgFollow;
    m         *= mgFollow;
    h         *= hgFollow;

    KillDenormal(l);
    KillDenormal(m);
    KillDenormal(h);
    
    // Shuffle History Buffer

    sdm3   = sdm2;
    sdm2   = sdm1;
    sdm1   = sample;                

    KillDenormal(sdm1);

    // Return Result

    lmh = (double)(l + m + h) * gainFollow;
    KillDenormal(lmh);
    if(*max < fabs(lmh))
      *max = fabs(lmh);
    out[i] += lmh;
  }
}