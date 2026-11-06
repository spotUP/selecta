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



#include "ADSR.h"
#include "Util.h"

#define STATE_OFF     0
#define STATE_ATTACK  1
#define STATE_DECAY   2
#define STATE_SUSTAIN 3
#define STATE_RELEASE 4

CADSR::CADSR(int sampleRate, double attack, double decay, double sustain, double release)
{
  SampleRate = sampleRate;
  SetAttack(attack);
  SetDecay(decay);
  SetSustain(sustain);
  SetRelease(release);
  SetAttackCurve(0.0f);
  SetDecayCurve(0.0f);
  SetReleaseCurve(0.0f);
  State = STATE_OFF;
  Time = 0.0f;
}

void CADSR::SetAttack(double value)
{
  Attack = value;
  AttackRate = (1.0f / Attack) / ((double)SampleRate); 
}

bool CADSR::IsTriggered()
{
  return (State != STATE_OFF);
}

void CADSR::SetDecay(double value)
{
  Decay = value;
  DecayRate = ((1.0f / Decay) / ((double)SampleRate));
}

void CADSR::SetSustain(double value)
{
  Sustain = value;
}

void CADSR::SetRelease(double value)
{
  Release = value;
  ReleaseRate = ((1.0f / Release) / ((double)SampleRate));
}

void CADSR::SetAttackCurve(double value)
{
  double volume;

  if(State == STATE_ATTACK)
    volume = CurvePoint(0.0f, 1.0f, Time, AttackCurve, AttackScale);
  AttackCurve = value;
  AttackScale = CurveScale(value);
  if(State == STATE_ATTACK)
    Time = CurvePointInv(0.0f, 1.0f, volume, AttackCurve, AttackScale);
}

void CADSR::SetDecayCurve(double value)
{
  double volume;

  if(State == STATE_DECAY)
    volume = CurvePoint(1.0f, Sustain, Time, DecayCurve, DecayScale);
  DecayCurve = value;
  DecayScale = CurveScale(value);
  if(State == STATE_DECAY)
    Time = CurvePointInv(1.0f, Sustain, volume, DecayCurve, DecayScale);
}

void CADSR::SetReleaseCurve(double value)
{
  double volume;

  if(State == STATE_RELEASE)
    volume = CurvePoint(Sustain, 0.0f, Time, ReleaseCurve, ReleaseScale);
  ReleaseCurve = value;
  ReleaseScale = CurveScale(value);
  if(State == STATE_RELEASE)
    Time = CurvePointInv(Sustain, 0.0f, volume, ReleaseCurve, ReleaseScale);
}

void CADSR::TriggerAttack()
{
  double volume;

  if(State == STATE_OFF)
    volume = 0.0f;

  if(State == STATE_ATTACK)
    return;

  if(State == STATE_DECAY)
    volume = CurvePoint(1.0f, Sustain, Time, DecayCurve, DecayScale);

  if(State == STATE_SUSTAIN)
    volume = Sustain;

  if(State == STATE_RELEASE)
    volume = CurvePoint(1.0f, 0.0f, Time, ReleaseCurve, ReleaseScale);

  Time = CurvePointInv(0.0f, 1.0f, volume, AttackCurve, AttackScale);
  State = STATE_ATTACK;
}

void CADSR::TriggerRelease()
{
  double volume;

  if(State == STATE_OFF)
    return;

  if(State == STATE_ATTACK)
    volume = CurvePoint(0.0f, 1.0f, Time, AttackCurve, AttackScale);

  if(State == STATE_DECAY)
    volume = CurvePoint(1.0f, Sustain, Time, DecayCurve, DecayScale);

  if(State == STATE_SUSTAIN)
    volume = Sustain;

  if(State == STATE_RELEASE)
    return;

  Time = CurvePointInv(1.0f, 0.0f, volume, ReleaseCurve, ReleaseScale);
  State = STATE_RELEASE;
}

void CADSR::Generate(int samples, double *envOut)
{
  while(samples > 0)
  {
    switch(State)
    {
      case STATE_OFF:
        while(samples-- > 0)
          *(envOut++) = 0.0f;
        break;

      case STATE_ATTACK:
        while(samples-- > 0)
        {
          Time += AttackRate;
          if(Time > 1.0f)
            Time = 1.0f;

          *(envOut++) = CurvePoint(0.0f, 1.0f, Time, AttackCurve, AttackScale);
          
          if(Time == 1.0f)
          {
            Time = 0.0f;
            State = STATE_DECAY;
            break;
          }
        }
        break;

      case STATE_DECAY:
        while(samples-- > 0)
        {
          Time += DecayRate;
          if(Time > 1.0f)
            Time = 1.0f;

          *(envOut++) = CurvePoint(1.0f, Sustain, Time, DecayCurve, DecayScale);

          if(Time == 1.0f)
          {
            Time = 0.0f;
            State = STATE_SUSTAIN;
            break;
          }
        }
        break;

      case STATE_SUSTAIN:
        while(samples-- > 0)
        {
          *(envOut++) = Sustain;
        }
        break;

      case STATE_RELEASE:
        while(samples-- > 0)
        {
          Time += ReleaseRate;
          if(Time > 1.0f)
            Time = 1.0f;

          *(envOut++) = CurvePoint(1.0f, 0.0f, Time, ReleaseCurve, ReleaseScale);

          if(Time == 1.0f)
          {
            Time = 0.0f;
            State = STATE_OFF;
            break;
          }
        }
        break;
    }
  }
}

void CADSR::SetSampleRate(int sampleRate)
{
  SampleRate = sampleRate;
  SetAttack(Attack);
  SetDecay(Decay);
  SetSustain(Sustain);
  SetRelease(Release);
}