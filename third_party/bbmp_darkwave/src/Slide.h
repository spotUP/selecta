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

#ifndef SLIDE_H
#define SLIDE_H

class CSlide
{
public:
         CSlide(int sampleRate, double glide, double portamento, double curve);
        ~CSlide();
  void   SetGlide(double value);
  void   SetPortamento(double value);
  void   SetCurve(double value);
  void   Trigger(unsigned char note);
  void   Trigger(double voltage);
  void   Generate(int samples, double *out);
  void   Generate(int samples, double *out, bool offset);

  double FrequencyToVoltage(double frequency);
  void   SetSampleRate(int sampleRate);

private:
  double Log440, InvLog2;
  double SampleRate, Glide, Portamento, Curve, CurveScale;
  double Voltage, VoltageA, VoltageB, Time, SlideRate;
  bool  Active, Sliding;
};

#endif