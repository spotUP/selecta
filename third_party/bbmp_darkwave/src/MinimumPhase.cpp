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



#include "MinimumPhase.h"
#include "DFT.h"
#include "Complex.h"
#include <math.h>

void RealCepstrum(int n, double *signal, double *realCepstrum)
{
  double *realTime, *imagTime, *realFreq, *imagFreq;
  int    i;

  realTime = new double[n];
  imagTime = new double[n];
  realFreq = new double[n];
  imagFreq = new double[n];

  // Compose Complex FFT Input

  for(i = 0; i < n; i++)
  {
    realTime[i] = signal[i];
    imagTime[i] = 0.0f;
  }
  
  // Perform DFT

  ChooseDFT(n, realTime, imagTime, realFreq, imagFreq);

  // Calculate Log Of Absolute Value

  for(i = 0; i < n; i++)
  {
    realFreq[i] = log(cabs(realFreq[i], imagFreq[i]));
    imagFreq[i] = 0.0;
  }

  // Perform Inverse FFT

  ChooseInverseDFT(n, realTime, imagTime, realFreq, imagFreq);

  // Output Real Part Of FFT
  for(i = 0; i < n; i++)
    realCepstrum[i] = realTime[i];

  delete realTime;
  delete imagTime;
  delete realFreq;
  delete imagFreq;
}

void MinimumPhase(int n, double *realCepstrum, double *minimumPhase)
{
  int i, nd2;
  double *realTime, *imagTime, *realFreq, *imagFreq;

  nd2 = n / 2;
  realTime = new double[n];
  imagTime = new double[n];
  realFreq = new double[n];
  imagFreq = new double[n];

  if((n % 2) == 1)
  {
    realTime[0] = realCepstrum[0];
    for(i = 1; i < nd2; i++)
      realTime[i] = 2.0f * realCepstrum[i];
    for(i = nd2; i < n; i++)
      realTime[i] = 0.0f;
  }
  else
  {
    realTime[0] = realCepstrum[0];
    for(i = 1; i < nd2; i++)
      realTime[i] = 2.0f * realCepstrum[i];
    realTime[nd2] = realCepstrum[nd2];
    for(i = nd2 + 1; i < n; i++)
      realTime[i] = 0.0f;
  }

  for(i = 0; i < n; i++)
    imagTime[i] = 0.0f;

  ChooseDFT(n, realTime, imagTime, realFreq, imagFreq);
  
  for(i = 0; i < n; i++)
    cexp(realFreq[i], imagFreq[i], &realFreq[i], &imagFreq[i]);

  ChooseInverseDFT(n, realTime, imagTime, realFreq, imagFreq);

  for(i = 0; i < n; i++)
    minimumPhase[i] = realTime[i];

  delete realTime;
  delete imagTime;
  delete realFreq;
  delete imagFreq;
}