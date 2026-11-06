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



// DFT Code Converted From Scientist And Engineer's Guide To DSP

#include "PI.h"
#include "FFT.h"
#include "DSP.h"
#include <math.h>

void DFT(int n, double *realTime, double *imagTime, double *realFreq, double *imagFreq)
{
  int   k,  i;
  double sr, si, p;

  for(k = 0; k < n; k++)
  {
    realFreq[k] = 0.0;
    imagFreq[k] = 0.0;
  }

  for(k = 0; k < n; k++)
    for(i = 0; i < n; i++)
    {
      p  = (2.0 * PI * (double)(k * i)) / (double)n;
      sr =  cos(p);
      si = -sin(p);
      realFreq[k] += (realTime[i] * sr) - (imagTime[i] * si);
      imagFreq[k] += (realTime[i] * si) + (imagTime[i] * sr);
    }
}

void InverseDFT(int n, double *realTime, double *imagTime, double *realFreq, double *imagFreq)
{
  int   k,  i;
  double sr, si, p;

  for(k = 0; k < n; k++)
  {
    realTime[k] = 0.0;
    imagTime[k] = 0.0;
  }

  for(k = 0; k < n; k++)
  {
    for(i = 0; i < n; i++)
    {
      p  = (2.0 * PI * (double)(k * i)) / (double)n;
      sr =  cos(p);
      si = -sin(p);
      realTime[k] += (realFreq[i] * sr) + (imagFreq[i] * si);
      imagTime[k] += (realFreq[i] * si) - (imagFreq[i] * sr);
    }
    realTime[k] /= n;
    imagTime[k] /= n;
  }
}

void ChooseDFT(int n, double *realTime, double *imagTime, double *realFreq, double *imagFreq)
{
  int i, p, it2;
  
  p = 2;
  while(p <= n)
  {
    if(p == n)
    {
      double *complex;
      
      complex = new double[n * 2];
      for(i = 0; i < n; i++)
      {
        it2 = i << 1;
        complex[it2]     = realTime[i];
        complex[it2 + 1] = imagTime[i]; 
      }

      FFT(complex, n);

      for(i = 0; i < n; i++)
      {
        it2 = i << 1;
        realFreq[i] = complex[it2];
        imagFreq[i] = complex[it2 + 1]; 
      }
      
      delete complex;
      return;
    }
    p = p << 1;
  }

  DFT(n, realTime, imagTime, realFreq, imagFreq);
}

void ChooseInverseDFT(int n, double *realTime, double *imagTime, double *realFreq, double *imagFreq)
{
  int i, p, it2;
  
  p = 2;
  while(p <= n)
  {
    if(p == n)
    {
      double *complex;
      
      complex = new double[n * 2];
      for(i = 0; i < n; i++)
      {
        it2 = i << 1;
        complex[it2]     = realFreq[i];
        complex[it2 + 1] = imagFreq[i]; 
      }

      InverseFFT(complex, n);

      for(i = 0; i < n; i++)
      {
        it2 = i << 1;
        realTime[i] = complex[it2];
        imagTime[i] = complex[it2 + 1]; 
      }

      delete complex;
      return;
    }
    p = p << 1;
  }

  InverseDFT(n, realTime, imagTime, realFreq, imagFreq);
}