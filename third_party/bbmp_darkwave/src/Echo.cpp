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

#include <malloc.h>

#include "Echo.h"
#include "DSP.h"
#include "Util.h"
#include <stdio.h>
#include <math.h>

#define BUFFER_COUNT 2

#define MIN_DB    (-64.0f)
#define MAX_DB    (16.0f)
#define MIN_DELAY (0.01f)
#define MAX_DELAY (1.0f)



double PropertyDefaults[PROPERTY_COUNT] =
{
  0.10,// Delay Time 1
  0.5, // Feedback 1
  0.0, // Filter Cutoff 1
  0.5, // Filter Resonance 1
  0.0, // LFO Amount 1
  0.5, // LFO Rate 1
  1.0, // Wet Level 1
  0.0, // Dry Level 1
  0.20,// Delay Time 2
  0.5, // Feedback 2
  0.1, // Filter Cutoff 2
  0.5, // Filter Resonance 2
  0.0, // LFO Amount 2
  0.5, // LFO Rate 2
  0.0, // Wet Level 2
  0.0, // Dry Level 2
  0.30,// Delay Time 3
  0.5, // Feedback 3
  0.2, // Filter Cutoff 3
  0.5, // Filter Resonance 3
  0.0, // LFO Amount 3
  0.5, // LFO Rate 3
  0.0, // Wet Level 3
  0.0, // Dry Level 3
  0.40,// Delay Time 4
  0.5, // Feedback 4
  0.3, // Filter Cutoff 4
  0.5, // Filter Resonance 4
  0.0, // LFO Amount 4
  0.5, // LFO Rate 4
  0.0, // Wet Level 4
  0.0  // Dry Level 4
};

char *EchoPropertyNames[PROPERTY_COUNT] =
{
  "Delay Time 1",
  "Feedback 1",
  "Filter Frequency 1",
  "Filter Bandwidth 1",
  "LFO Amount 1",
  "LFO Rate 1",
  "Wet Level 1",
  "Dry Level 1",
  "Delay Time 2",
  "Feedback 2",
  "Filter Frequency 2",
  "Filter Bandwidth 2",
  "LFO Amount 2",
  "LFO Rate 2",
  "Wet Level 2",
  "Dry Level 2",
  "Delay Time 3",
  "Feedback 3",
  "Filter Frequency 3",
  "Filter Bandwidth 3",
  "LFO Amount 3",
  "LFO Rate 3",
  "Wet Level 3",
  "Dry Level 3",
  "Delay Time 4",
  "Feedback 4",
  "Filter Frequency 4",
  "Filter Bandwidth 4",
  "LFO Amount 4",
  "LFO Rate 4",
  "Wet Level 4",
  "Dry Level 4"
};

CEcho::CEcho(CHost *host)
: CMachine(host)
{
  int i;

  for(i = 0; i < PROPERTY_COUNT; i++)
    Properties[i] = -1.0f;
  PropertyCount = PROPERTY_COUNT;

  BlockSamples = host->GetBlockSamples();
  SampleRate   = host->GetSampleRate();

  Buffer = new double*[BUFFER_COUNT];
  for(i = 0; i < BUFFER_COUNT; i++)
    Buffer[i] = (double*)_mm_malloc(sizeof(double) * BlockSamples, AUDIO_BUFFER_ALIGN);

  PortCount = 2;
  Ports = new CPort*[2];
  Ports[0] = new CPort(host, this, PT_AUDIO | PT_INPUT  | PT_STEREO, "Line In");
  Ports[1] = new CPort(host, this, PT_AUDIO | PT_OUTPUT | PT_STEREO, "Line Out");

  for(i = 0; i < 4; i++)
  {
    CombLeft[i]  = new CCombFilter(SampleRate, MAX_DELAY, PropertyDefaults[PROPERTY_FEEDBACK1 + (8 * i)]);
    CombRight[i] = new CCombFilter(SampleRate, MAX_DELAY, PropertyDefaults[PROPERTY_FEEDBACK1 + (8 * i)]);
    LFO[i] = new CVCO(SampleRate, SinTable, 10.0f, 0, 0);
  }


  for(i = 0; i < PROPERTY_COUNT; i++)
    SetProperty(i, PropertyDefaults[i]);
}

void CEcho::SetBlockSamples(int blockSamples)
{
  int i, j;

  for(i = 0; i < BUFFER_COUNT; i++)
  {
    _mm_free(Buffer[i]);
    Buffer[i] = (double*)_mm_malloc(sizeof(double) * blockSamples, AUDIO_BUFFER_ALIGN);
	for(j = 0; j < blockSamples; j++)
		Buffer[i][j] = 0.0;
  
  }
  BlockSamples = blockSamples;
}

void CEcho::SetSampleRate(int sampleRate)
{
  int i;

  if(sampleRate != SampleRate)
  {
	  SampleRate = sampleRate;

	  for(i = 0; i < 4; i++)
	  {
    	CombLeft[i]->SetSampleRate(sampleRate);
		CombRight[i]->SetSampleRate(sampleRate);
		LFO[i]->SetSampleRate(sampleRate);
	  }
  }
}

CEcho::~CEcho()
{
  int i;

  for(i = 0; i < 4; i++)
  {
    delete LFO[i];
    delete CombLeft[i];
    delete CombRight[i];
  }
  
  for(i = 0; i < BUFFER_COUNT; i++)
    _mm_free(Buffer[i]);

  delete Buffer;
  delete Ports[0];
  delete Ports[1];
  delete Ports;
}

void CEcho::Work()
{
  double *in, *out, *mod, wet, dry;
  int    i, j;
  
  out  = Ports[1]->AudioBuffer->LeftData;
  for(j = 0; j < BlockSamples; j++)
  {
    out[j] = 0.0;
  }

  out  = Ports[1]->AudioBuffer->RightData;
  for(j = 0; j < BlockSamples; j++)
  {
    out[j] = 0.0;
  }

  for(i = 0; i < 4; i++)
  {
    wet = WetLevel[i] * 0.0625f;
    dry = DryLevel[i] * 0.25f;

    //if(LFOAmp[i] != 0.0)
    //{
      mod = Buffer[1];
      LFO[i]->Generate(mod, BlockSamples);
      for(j = 0; j < BlockSamples; j++)
        mod[j] = 1.0 + (mod[j] * LFOAmp[i]);
    //}
    //else
    //  mod = 0;

    in  = Ports[0]->AudioBuffer->LeftData;
    out = Ports[1]->AudioBuffer->LeftData;

    CombLeft[i]->Process(in, Buffer[0], mod, BlockSamples);
    for(j = 0; j < BlockSamples; j++)
    {
      out[j] += (in[j] * dry) + (Buffer[0][j] * wet);
      Buffer[0][j] = 0.0f;
    }

    in  = Ports[0]->AudioBuffer->RightData;
    out = Ports[1]->AudioBuffer->RightData;

    CombRight[i]->Process(in, Buffer[0], mod, BlockSamples);
    for(j = 0; j < BlockSamples; j++)
    {
      out[j] += (in[j] * dry) + (Buffer[0][j] * wet);
      Buffer[0][j] = 0.0f;
    }
  }
}

double CEcho::GetProperty(int index)
{
  if(index >= 0 && index < PROPERTY_COUNT)
    return Properties[index];
  else
    return 0.0f;
}



void CEcho::GetPropertyValueDisplay(int index, double value, char *text)
{
  if(index < 0 || index >= PROPERTY_COUNT)
    *text = 0;
  else
  {
    switch(index)
    {
      case PROPERTY_DELAYTIME1:
      case PROPERTY_DELAYTIME2:
      case PROPERTY_DELAYTIME3:
      case PROPERTY_DELAYTIME4:
        sprintf(text, "%ims", (int)(PropertyToDelayTime(value) * 1000.0f));
        break;

      case PROPERTY_FEEDBACK1:
      case PROPERTY_CUTOFF1:
      case PROPERTY_RESONANCE1:
      case PROPERTY_LFOAMOUNT1:
      case PROPERTY_LFORATE1:

      case PROPERTY_FEEDBACK2:
      case PROPERTY_CUTOFF2:
      case PROPERTY_RESONANCE2:
      case PROPERTY_LFOAMOUNT2:
      case PROPERTY_LFORATE2:

      case PROPERTY_FEEDBACK3:
      case PROPERTY_CUTOFF3:
      case PROPERTY_RESONANCE3:
      case PROPERTY_LFOAMOUNT3:
      case PROPERTY_LFORATE3:

      case PROPERTY_FEEDBACK4:
      case PROPERTY_CUTOFF4:
      case PROPERTY_RESONANCE4:
      case PROPERTY_LFOAMOUNT4:
      case PROPERTY_LFORATE4:

        sprintf(text, "%i%%", (int)(value * 100.0f));
        break;

      case PROPERTY_WETLEVEL1:
      case PROPERTY_DRYLEVEL1:

      case PROPERTY_WETLEVEL2:
      case PROPERTY_DRYLEVEL2:

      case PROPERTY_WETLEVEL3:
      case PROPERTY_DRYLEVEL3:

      case PROPERTY_WETLEVEL4:
      case PROPERTY_DRYLEVEL4:

        value = PropertyToPowerRatio(value);
        if(value == 0.0f)
          strcpy(text, "-oo dB");
        else
          sprintf(text, "%.2fdB", PowerRatioTodB(value));
        break;

      default:
        *text = 0;
        break;
    }
  }
}

void CEcho::GetPropertyName(int index, char *text)
{
  if(index >= 0 && index < PROPERTY_COUNT)
    strcpy(text, EchoPropertyNames[index]);
  else
    *text = 0;
}

void CEcho::SetProperty(int index, double value)
{
  if(index >= 0 && index < PROPERTY_COUNT)
  {
    if(Properties[index] != value)
    {
      Properties[index] = value;
    
      switch(index)
      {
        case PROPERTY_DELAYTIME1:
          CombLeft[0]->SetFraction(PropertyToFraction(value));
          CombRight[0]->SetFraction(PropertyToFraction(value));
          break;

        case PROPERTY_DELAYTIME2:
          CombLeft[1]->SetFraction(PropertyToFraction(value));
          CombRight[1]->SetFraction(PropertyToFraction(value));
          break;

        case PROPERTY_DELAYTIME3:
          CombLeft[2]->SetFraction(PropertyToFraction(value));
          CombRight[2]->SetFraction(PropertyToFraction(value));
          break;

        case PROPERTY_DELAYTIME4:
          CombLeft[3]->SetFraction(PropertyToFraction(value));
          CombRight[3]->SetFraction(PropertyToFraction(value));
          break;

        case PROPERTY_FEEDBACK1:
          CombLeft[0]->SetFeedback(value);
          CombRight[0]->SetFeedback(value);
          break;

        case PROPERTY_FEEDBACK2:
          CombLeft[1]->SetFeedback(value);
          CombRight[1]->SetFeedback(value);
          break;

        case PROPERTY_FEEDBACK3:
          CombLeft[2]->SetFeedback(value);
          CombRight[2]->SetFeedback(value);
          break;

        case PROPERTY_FEEDBACK4:
          CombLeft[3]->SetFeedback(value);
          CombRight[3]->SetFeedback(value);
          break;

        case PROPERTY_RESONANCE1:
          CombLeft[0]->SetBandwidth(10.0f + (value * 10000.0f));
          CombRight[0]->SetBandwidth(10.0f + (value * 10000.0f));
          break;
        case PROPERTY_RESONANCE2:
          CombLeft[1]->SetBandwidth(10.0f + (value * 10000.0f));
          CombRight[1]->SetBandwidth(10.0f + (value * 10000.0f));
          break;
        case PROPERTY_RESONANCE3:
          CombLeft[2]->SetBandwidth(10.0f + (value * 10000.0f));
          CombRight[2]->SetBandwidth(10.0f + (value * 10000.0f));
          break;
        case PROPERTY_RESONANCE4:
          CombLeft[3]->SetBandwidth(10.0f + (value * 10000.0f));
          CombRight[3]->SetBandwidth(10.0f + (value * 10000.0f));
          break;

        case PROPERTY_LFOAMOUNT1:
          LFOAmp[0] = value;  
          break;

        case PROPERTY_LFOAMOUNT2:
          LFOAmp[1] = value;  
          break;

        case PROPERTY_LFOAMOUNT3:
          LFOAmp[2] = value;  
          break;

        case PROPERTY_LFOAMOUNT4:
          LFOAmp[3] = value;  
          break;

        case PROPERTY_LFORATE1:
          LFO[0]->SetVoltage(-10.0f + (value * 6.0f));
          break;

        case PROPERTY_LFORATE2:
          LFO[1]->SetVoltage(-10.0f + (value * 6.0f));
          break;

        case PROPERTY_LFORATE3:
          LFO[2]->SetVoltage(-10.0f + (value * 6.0f));
          break;

        case PROPERTY_LFORATE4:
          LFO[3]->SetVoltage(-10.0f + (value * 6.0f));
          break;

        case PROPERTY_WETLEVEL1:
          WetLevel[0] = value;
          break;

        case PROPERTY_WETLEVEL2:
          WetLevel[1] = value;
          break;
 
        case PROPERTY_WETLEVEL3:
          WetLevel[2] = value;
          break;
 
        case PROPERTY_WETLEVEL4:
          WetLevel[3] = value;
          break;
 
        case PROPERTY_DRYLEVEL1:
          DryLevel[0] = value;
          break;
          
        case PROPERTY_DRYLEVEL2:
          DryLevel[1] = value;
          break;
          
        case PROPERTY_DRYLEVEL3:
          DryLevel[2] = value;
          break;
          
        case PROPERTY_DRYLEVEL4:
          DryLevel[3] = value;
          break;
          
        case PROPERTY_CUTOFF1:
          CombLeft[0]->SetCutoffVoltage(value * 12.0f);
          CombRight[0]->SetCutoffVoltage(value * 12.0f);
          break;

        case PROPERTY_CUTOFF2:
          CombLeft[1]->SetCutoffVoltage(value * 12.0f);
          CombRight[1]->SetCutoffVoltage(value * 12.0f);
          break;

        case PROPERTY_CUTOFF3:
          CombLeft[2]->SetCutoffVoltage(value * 12.0f);
          CombRight[2]->SetCutoffVoltage(value * 12.0f);
          break;

        case PROPERTY_CUTOFF4:
          CombLeft[3]->SetCutoffVoltage(value * 12.0f);
          CombRight[3]->SetCutoffVoltage(value * 12.0f);
          break;
      }
    }
  }
}

double CEcho::PropertyToPowerRatio(double value)
{
  if(value == 0.0f)
    return 0.0f;
  else
    return dBToPowerRatio(MIN_DB + ((MAX_DB - MIN_DB) * value));
}

double CEcho::PropertyToDelayTime(double value)
{
  return PropertyToFraction(value) * MAX_DELAY;
}

double CEcho::PropertyToFraction(double value)
{
  value = fastpow2(value) - 1.0f;
  return MIN_DELAY + (value * (1.0f - MIN_DELAY));
}

CEchoInfo::CEchoInfo(CHost *host)
{
  Host = host;
  Name = "ES QuadDelay";
  UniqueID = "DarkPlug::ES QuadDelay";
}

bool CEcho::IsSynth()
{
  return false;
}

CMachine *CEchoInfo::CreateMachine()
{
  return new CEcho(Host);
}