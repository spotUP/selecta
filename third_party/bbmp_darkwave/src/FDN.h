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

// N=4 Householder Feedback Delay Network

#ifndef FDN_H
#define FDN_H

class CFDN
{
public:
        CFDN(int sampleRate, int *delayLengths, double feedback, double gain, double cutoff);
       ~CFDN();
  void  SetFeedback(double value);
  void  SetGain(double value);
  void  SetCutoff(double value);
  CFDN *Clone();
  void  Process(double inLeft, double inRight, double *outLeft, double *outRight);

private:
  double  Feedback, FeedbackFollow, Gain, GainFollow, Cutoff;
  double *DelayBuffers[4], Coefficients[5], PastInput[4][2], PastOutput[4][2];
  int    DelayLengths[4], DelayOffsets[4], SampleRate;
};

#endif