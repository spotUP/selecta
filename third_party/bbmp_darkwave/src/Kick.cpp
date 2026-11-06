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



#include "Kick.h"
#include "Core.h"
#include "DSP.h"
#include "Util.h"
#include "Resource.h"
#include "Error.h"
#include <stdio.h>
#include <math.h>

#define PI 3.14159265358979f

#define PROPERTY_FORMAT_PERCENT    0
#define PROPERTY_FORMAT_COUNT      1

#define PROPERTY_VCO_ATTACK            0
#define PROPERTY_VCO_DECAY             1
#define PROPERTY_VCO_CURVE             2
#define PROPERTY_AMP_ATTACK            3
#define PROPERTY_AMP_DECAY             4
#define PROPERTY_AMP_CURVE             5
#define PROPERTY_VCF_CUTOFF            6

#define PROPERTY_COUNT 7

#define BUFFER_VCOENV 0
#define BUFFER_VCOSIG 1
#define BUFFER_AMPENV 0
#define BUFFER_AMPSIG 1

#define BUFFER_COUNT  2

const char* KickPropertyNames[PROPERTY_COUNT] = 
{
  "VCO Attack",
  "VCO Decay",
  "VCO Curve",
  "Amp Attack",
  "Amp Decay",
  "Amp Curve",
  "VCF Cutoff"
};
 
const double PropertyDefaults[PROPERTY_COUNT] = 
{
  0.02f, // VCO Attack
  0.50f, // VCO Decay
  0.28f, // VCO Curve
  0.05f, // Amp Attack
  0.50f, // Amp Decay
  1.00f, // Amp Curve
  0.37f  // VCF Cutoff
};

const int PropertyFormat[PROPERTY_COUNT] = 
{
  PROPERTY_FORMAT_PERCENT, // VCO Attack
  PROPERTY_FORMAT_PERCENT, // VCO Decay
  PROPERTY_FORMAT_PERCENT, // VCO Curve
  PROPERTY_FORMAT_PERCENT, // Amp Attack
  PROPERTY_FORMAT_PERCENT, // Amp Decay
  PROPERTY_FORMAT_PERCENT, // Amp Curve
  PROPERTY_FORMAT_PERCENT  // VCF Cutoff                 
};

CKickVoice::CKickVoice(int sampleRate, int blockSamples)
{
  int i;

  SampleRate   = sampleRate;
  BlockSamples = blockSamples;
 
  Buffer = new double*[BUFFER_COUNT];
  for(i = 0; i < BUFFER_COUNT; i++)
    Buffer[i] = new double[blockSamples];

  ControlBuffer = new CControlBuffer();
  VCO = new CVCO(SampleRate, SinTable, 1.0f, 0, 0);
  VCO->SetVoltage(0.0f);
  VCF = new CMultiFilter(SampleRate, MULTIFILTER_MODE_LP, MULTIFILTER_POLES_4, 6.0f, 0.0f);
  VCA = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);
  AmpAD = new CAD(SampleRate, 0.01f, 1.0f, 0.0f, 0.0f);
  VCOAD = new CAD(SampleRate, 0.01f, 1.0f, 0.0f, 0.0f);
}

CKickVoice::~CKickVoice()
{
  int i;

  delete ControlBuffer;

  for(i = 0; i < BUFFER_COUNT; i++)
    delete Buffer[i];
  delete Buffer;

  delete VCO;
  delete VCA;
  delete VCF;
  delete AmpAD;
  delete VCOAD;
}

void CKickVoice::SetBlockSamples(int blockSamples)
{
  int i;

  for(i = 0; i < BUFFER_COUNT; i++)
  {
    delete Buffer[i];
    Buffer[i] = new double[blockSamples];
  }
  BlockSamples = blockSamples;
}

void CKickVoice::SetSampleRate(int sampleRate)
{
  VCO->SetSampleRate(sampleRate);
  VCF->SetSampleRate(sampleRate);
  AmpAD->SetSampleRate(sampleRate);
  VCOAD->SetSampleRate(sampleRate);
}

void CKickVoice::Work(double *out)
{
  SControlMessage message;
  bool            gotMessage;
  int             firstSample, lastSample;

  gotMessage = ControlBuffer->Read(&message);
  if(!gotMessage) // no messages, just generate signal
    GenerateBlock(0, BlockSamples, out);
  else // got one or more messages
  {
    if(message.BlockSample > 0) // process data before first block
      GenerateBlock(0, message.BlockSample, out);

    while(gotMessage) // process message and data until no more messages
    {
      ProcessMessage(&message);
      firstSample = message.BlockSample;
      gotMessage = ControlBuffer->Read(&message);
      if(gotMessage)
        lastSample = message.BlockSample - 1;
      else
        lastSample = BlockSamples - 1;
      GenerateBlock(firstSample, lastSample - firstSample + 1, out);
    }
  }
}

double CKickVoice::FrequencyToVoltage(double frequency)
{
  return ((log(frequency) - log(440.0f)) * (1.0 / log(2.0))) + 5.0;
}

void CKickVoice::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_NOTE_ON:
    {
      Voltage = FrequencyToVoltage(message->NoteOn.Frequency);
      AmpAD->Trigger();
      VCOAD->Trigger();
      break;
    }

    case CONTROL_MESSAGE_NOTE_OFF:
    {
      break;
    }

    case CONTROL_MESSAGE_CONTROL_CHANGE:
    {
      double value;

      value = message->ControlChange.Value;
      switch(message->ControlChange.Controller)
      { 
        case PROPERTY_AMP_ATTACK:
        {
          AmpAD->SetAttack(value * 0.2f);
          break;
        }
        case PROPERTY_VCO_ATTACK:
        {
          VCOAD->SetAttack(value * 0.2f);
          break;
        }
        case PROPERTY_AMP_DECAY:
        {
          AmpAD->SetDecay(value * 1.0f);
          break;
        }
        case PROPERTY_VCO_DECAY:
        {
          VCOAD->SetDecay(value * 1.0f);
          break;
        }
        case PROPERTY_VCO_CURVE:
        {
          VCOAD->SetDecayCurve((value - 0.5f) * 16.0f);
          break;
        }
        case PROPERTY_AMP_CURVE:
        {
          AmpAD->SetDecayCurve((value - 0.5f) * 16.0f);
          break;
        }
        case PROPERTY_VCF_CUTOFF:
        {
          VCF->SetVoltage(value * 12.0f);
          break;
        }
     }
    }
  }
}

void CKickVoice::GenerateBlock(int sample, int samples, double *out)
{
  int i;

  VCOAD->Generate(samples, Buffer[BUFFER_VCOENV]);
  for(i = 0; i < samples; i++)
    Buffer[BUFFER_VCOENV][i] *= Voltage;
  VCO->Generate(Buffer[BUFFER_VCOENV], Buffer[BUFFER_VCOSIG], samples);

  VCF->Process(samples, Buffer[BUFFER_VCOSIG], 0, 0); 

  AmpAD->Generate(samples, Buffer[BUFFER_AMPENV]);
  VCA->Process(samples, Buffer[BUFFER_VCOSIG], Buffer[BUFFER_AMPENV]);

  for(i = 0; i < samples; i++)
    out[sample + i] = Buffer[BUFFER_AMPSIG][i];
}

void CKickVoice::ClearBuffer(int index, int samples)
{
  double *buffer;

  buffer = Buffer[index];
  while(samples-- > 0)
    *(buffer++) = 0.0f;
}

void CKickVoice::WriteMessage(SControlMessage *message)
{
  ControlBuffer->Write(message);
}

CKick::CKick(CHost *host)
: CMachine(host)
{
  int i;

  SampleRate   = host->GetSampleRate();
  BlockSamples = host->GetBlockSamples();  

  Ports = new CPort*[2];
  Ports[0] = new CPort(host, this, PT_INPUT | PT_CONTROL, "Control In");
  Ports[1] = new CPort(host, this, PT_OUTPUT | PT_AUDIO,  "Line Out");
  PortCount = 2;

  Voice = new CKickVoice(SampleRate, BlockSamples);

  PropertyCount = PROPERTY_COUNT;
  
  Properties = new double[PropertyCount];
  for(i = 0; i < PropertyCount; i++)
    SetProperty(i, PropertyDefaults[i]);
}

CKick::~CKick()
{
  delete Ports[0];
  delete Ports[1];
  delete Ports;
  delete Voice;
}

bool CKick::IsSynth()
{
  return true;
}

void CKick::Work()
{
  bool            messageRead;
  SControlMessage message;

  messageRead = Ports[0]->ControlBuffer->Read(&message);
  while(messageRead)
  {
    ProcessMessage(&message);
    messageRead = Ports[0]->ControlBuffer->Read(&message);
  }

  Voice->Work(Ports[1]->AudioBuffer->MonoData);
}

void CKick::SetBlockSamples(int blockSamples)
{
  Voice->SetBlockSamples(blockSamples);
  BlockSamples = blockSamples;
}

void CKick::SetSampleRate(int sampleRate)
{
  Voice->SetSampleRate(sampleRate);
  SampleRate = sampleRate;
}

void CKick::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_NOTE_ON:
    {
      Voice->WriteMessage(message);
      break;
    }
    case CONTROL_MESSAGE_NOTE_OFF:
    {
      Voice->WriteMessage(message);
      break;
    }
    case CONTROL_MESSAGE_CONTROL_CHANGE:
    {
      SetPropertyAutomated(message->ControlChange.Controller,
        message->ControlChange.Value);
      break;
    }
  }
}

void CKick::GetPropertyName(int index, char *text)
{
  if((index >= 0) && (index < PROPERTY_COUNT))
    strcpy(text, KickPropertyNames[index]);
  else
    *text = 0;
}

void CKick::GetPropertyValueDisplay(int index, double value, char *text)
{
  if((index >= 0) && (index < PROPERTY_COUNT))
  {
    switch(PropertyFormat[index])
    {
      case PROPERTY_FORMAT_PERCENT:
      {
        sprintf(text, "%.0f%%", value * 100.0f);
        break;
      }
      default:
      {
        *text = 0;
      }
    }
  }
  else
    *text = 0;
}

double CKick::GetProperty(int index)
{
  if((index >= 0) && (index < PROPERTY_COUNT))
    return Properties[index];
  else
    return 0.0f;
}

void CKick::SetProperty(int index, double value)
{
  SControlMessage message;

  if((index >= 0) && (index < PROPERTY_COUNT))
  {
    Properties[index] = value;
    memset(&message, 0, sizeof(SControlMessage));
    message.Type = CONTROL_MESSAGE_CONTROL_CHANGE;
    message.ControlChange.Controller = index;
    message.ControlChange.Value = value;
    Voice->WriteMessage(&message);
  }
}

CKickInfo::CKickInfo(CHost *host)
{
  Host = host;
  Name = "ES Stomp";
  UniqueID = "DarkPlug::ES Stomp";
}

CMachine *CKickInfo::CreateMachine()
{
  return new CKick(Host);
}

