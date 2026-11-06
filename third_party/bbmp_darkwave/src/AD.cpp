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



#include "AD.h"
#include "Util.h"

#define STATE_OFF      0
#define STATE_PREDELAY 1
#define STATE_ATTACK   2
#define STATE_SUSTAIN  3
#define STATE_DECAY    4

CAD::CAD(int sampleRate, double attack, double decay, double attackCurve, double decayCurve)
{
  RetriggerCount = 0;
  RetriggerStep  = 0;
  SampleRate = sampleRate;
  SetAttack(attack);
  SetDecay(decay);
  SetSustain(0.0f);
  SetAttackCurve(attackCurve);
  SetDecayCurve(decayCurve);
  SetPredelay(0.0f);
  State = STATE_OFF;
  Time  = 0.0f;
}

CAD::~CAD()
{
}

void CAD::SetRetrigger(int count)
{
  RetriggerCount = count;
}

void CAD::SetAttack(double value)
{
  Attack     = value;
  if(value <= 0.0f)
    AttackRate = 1.0f;
  else
    AttackRate = (1.0f / value) / (double)SampleRate; 
}

void CAD::SetPredelay(double value)
{
  Predelay = value;
  if(value <= 0.0f)
    PredelayRate = 1.0f;
  else
    PredelayRate = (1.0f / value) / (double)SampleRate;
}

void CAD::SetDecay(double value)
{
  Decay     = value;
  if(value <= 0.0f)
    DecayRate = 1.0f;
  else
    DecayRate = (1.0f / value) / (double)SampleRate;
}

void CAD::SetSustain(double value)
{
  Sustain = value;
  if(value <= 0.0f)
    SustainRate = 1.0f;
  else
    SustainRate = (1.0f / value) / (double)SampleRate;
}

void CAD::SetAttackCurve(double value)
{
  double volume;

  if(State == STATE_ATTACK)
    volume = CurvePoint(0.0f, 1.0f, Time, AttackCurve, AttackScale);
  AttackCurve = value;
  AttackScale = CurveScale(value);
  if(State == STATE_ATTACK)
    Time = CurvePointInv(0.0f, 1.0f, volume, AttackCurve, AttackScale);
}

void CAD::SetDecayCurve(double value)
{
  double volume;

  if(State == STATE_DECAY)
    volume = CurvePoint(1.0f, 0.0f, Time, DecayCurve, DecayScale);
  DecayCurve = value;
  DecayScale = CurveScale(value);
  if(State == STATE_DECAY)
  {
    Time = CurvePointInv(1.0f, 0.0f, volume, DecayCurve, DecayScale);
  }
}

void CAD::Trigger()
{
  double volume;

  RetriggerStep = RetriggerCount;

  if(State == STATE_DECAY)
  {   
    volume = CurvePoint(1.0f, 0.0f, Time, DecayCurve, DecayScale);
    Time = CurvePointInv(0.0f, 1.0f, volume, AttackCurve, AttackScale);
  }

  if(State == STATE_SUSTAIN)
  {
    volume = 1.0f;
    Time = CurvePointInv(0.0f, 1.0f, volume, AttackCurve, AttackScale);
  }

  if(Predelay > 0.0f)
    State = STATE_PREDELAY;
  else
    State = STATE_ATTACK;
}

bool CAD::IsTriggered()
{
  return (State != STATE_OFF);
}

void CAD::Generate(int samples, double *out)
{
  while(samples > 0)
  {
    switch(State)
    {
      case STATE_OFF:
        while(samples-- > 0)
        {
          *(out++) = 0.0;
        }
        break;

      case STATE_PREDELAY:
        while(samples-- > 0)
        {
          Time += PredelayRate;
          if(Time > 1.0)
            Time = 1.0;

          *(out++) = 0.0;

          if(Time == 1.0)
          {
            Time = 0.0;
            State = STATE_ATTACK;
            break;
          }
        }
        break;

      case STATE_ATTACK:
        while(samples-- > 0)
        {
          Time += AttackRate;
          if(Time > 1.0)
            Time = 1.0;

          *(out++) = CurvePoint(0.0f, 1.0f, Time, AttackCurve, AttackScale);

          if(Time == 1.0)
          {
            Time  = 0.0;
            if(Sustain > 0.0f)
              State = STATE_SUSTAIN;
            else
              State = STATE_DECAY;
            break;
          }
        }
        break;

      case STATE_SUSTAIN:
        while(samples-- > 0)
        {
          Time += SustainRate;
          if(Time > 1.0)
            Time = 1.0;

          *(out++) = 1.0f;

          if(Time == 1.0f)
          {
            Time  = 0.0f;
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
          
          *(out++) = CurvePoint(1.0, 0.0, Time, DecayCurve, DecayScale);

          if(Time == 1.0f)
          {
            Time  = 0.0f;
            State = STATE_OFF;
            if(RetriggerStep)
            {
              RetriggerStep--;
              Time = 0.0f;
              State = STATE_ATTACK;
            }
            break;
          }
        }
        break;
    }
  }
}

void CAD::SetSampleRate(int sampleRate)
{
  SampleRate = sampleRate;
  SetPredelay(Predelay);
  SetAttack(Attack);
  SetSustain(Sustain);
  SetDecay(Decay);
}
