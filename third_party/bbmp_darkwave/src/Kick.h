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

#ifndef KICK_H
#define KICK_H

#include "DarkPlug.h"
#include "AD.h"
#include "VCO.h"
#include "VCA.h"
#include "MultiFilter.h"

class CKickVoice 
{
public:
        CKickVoice(int sampleRate, int blockSamples);
       ~CKickVoice();
  void  Work(double *out);
  void  ProcessMessage(SControlMessage *message);
  void  GenerateBlock(int sample, int samples, double *out);
  void  WriteMessage(SControlMessage *message);
  void  ClearBuffer(int index, int samples);
  double FrequencyToVoltage(double frequency);
  void  SetBlockSamples(int blockSamples);
  void  SetSampleRate(int sampleRate);

private:
  CControlBuffer *ControlBuffer;
  CAD            *VCOAD, *AmpAD;
  CVCO           *VCO;
  CVCA           *VCA;
  CMultiFilter   *VCF;
  int             BlockSamples, SampleRate;
  double         **Buffer, Voltage;
};

class CKick : public CMachine
{
public:
                CKick(CHost *host);
  virtual      ~CKick();
  virtual void  DARKCALL Work();
  void          ProcessMessage(SControlMessage *message);
  virtual void  DARKCALL GetPropertyName(int index, char *text);
  virtual void  DARKCALL GetPropertyValueDisplay(int index, double value, char *text);
  virtual double DARKCALL GetProperty(int index);
  virtual void  DARKCALL SetProperty(int index, double value);
  virtual void  DARKCALL SetBlockSamples(int blockSamples);
  virtual void  DARKCALL SetSampleRate(int sampleRate);
  virtual bool  DARKCALL IsSynth();

private:
  int           SampleRate, BlockSamples;
  double        *Properties;
  CKickVoice   *Voice;
};

class CKickInfo : public CMachineInfo
{
public:
                     CKickInfo(CHost *host);
  CMachine* DARKCALL CreateMachine();

private:
  CHost *Host;
};

#endif