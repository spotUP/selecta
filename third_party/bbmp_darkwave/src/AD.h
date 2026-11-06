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

#ifndef AD_H
#define AD_H

class CAD
{
public:
      CAD(int sampleRate, double attack, double decay, double attackCurve, double decayCurve);
     ~CAD();
 void SetAttack(double value);
 void SetDecay(double value);
 void SetSustain(double value);
 void SetAttackCurve(double value);
 void SetDecayCurve(double value);
 void Trigger();
 void Generate(int samples, double *out);
 void SetPredelay(double value);
 void SetSampleRate(int sampleRate);
 bool IsTriggered();
 void SetRetrigger(int count);

private:
  double Attack, Decay, AttackCurve, DecayCurve, AttackRate, DecayRate, AttackScale, DecayScale, Time, Sustain, SustainRate, Predelay, PredelayRate;
  int   State, SampleRate, RetriggerCount, RetriggerStep;
};

#endif