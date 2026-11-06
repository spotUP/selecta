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

#ifndef ECHO_H
#define ECHO_H

#include "DarkPlug.h"
#include "CombFilter.h"
#include "VCO.h"

#define PROPERTY_DELAYTIME1    0
#define PROPERTY_FEEDBACK1     1
#define PROPERTY_CUTOFF1       2
#define PROPERTY_RESONANCE1    3
#define PROPERTY_LFOAMOUNT1    4
#define PROPERTY_LFORATE1      5
#define PROPERTY_WETLEVEL1     6
#define PROPERTY_DRYLEVEL1     7

#define PROPERTY_DELAYTIME2    8
#define PROPERTY_FEEDBACK2     9
#define PROPERTY_CUTOFF2       10
#define PROPERTY_RESONANCE2    11
#define PROPERTY_LFOAMOUNT2    12
#define PROPERTY_LFORATE2      13
#define PROPERTY_WETLEVEL2     14
#define PROPERTY_DRYLEVEL2     15

#define PROPERTY_DELAYTIME3    16
#define PROPERTY_FEEDBACK3     17
#define PROPERTY_CUTOFF3       18
#define PROPERTY_RESONANCE3    19
#define PROPERTY_LFOAMOUNT3    20
#define PROPERTY_LFORATE3      21
#define PROPERTY_WETLEVEL3     22
#define PROPERTY_DRYLEVEL3     23

#define PROPERTY_DELAYTIME4    24
#define PROPERTY_FEEDBACK4     25
#define PROPERTY_CUTOFF4       26
#define PROPERTY_RESONANCE4    27
#define PROPERTY_LFOAMOUNT4    28
#define PROPERTY_LFORATE4      29
#define PROPERTY_WETLEVEL4     30
#define PROPERTY_DRYLEVEL4     31


#define PROPERTY_COUNT         32

class CEcho : public CMachine
{
public:
                CEcho(CHost *host);
               ~CEcho();
  virtual void  DARKCALL Work();
  virtual double DARKCALL GetProperty(int index);
  virtual void  DARKCALL GetPropertyValueDisplay(int index, double value, char *text);
  virtual void  DARKCALL GetPropertyName(int index, char *text);
  virtual void  DARKCALL SetProperty(int index, double value);
  virtual void  DARKCALL SetSampleRate(int sampleRate);
  virtual void  DARKCALL SetBlockSamples(int blockSamples);
  virtual bool  DARKCALL IsSynth();

private:
  double         PropertyToDelayTime(double value);
  double         PropertyToFraction(double value);
  double         PropertyToPowerRatio(double value);
  
  double        Properties[PROPERTY_COUNT], **Buffer, LFOAmp[4];
  CCombFilter *CombLeft[4], *CombRight[4];
  int          BlockSamples, SampleRate;
  CVCO        *LFO[4];
  double        WetLevel[4], DryLevel[4];
};

class CEchoInfo : public CMachineInfo
{
public:
                             CEchoInfo(CHost *host);
  virtual CMachine* DARKCALL CreateMachine();

private:
  CHost *Host;
};

#endif