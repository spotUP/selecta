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

#ifdef DGENR8
#include "Resource.h"
#include "Core.h"
#else
#ifdef DARKPLUG_HOST
#include "Resource.h"
#include "Core.h"
#else
#include "Resource.h"
#include "Core.h"
#endif
#endif


#include "DrumKit.h"
#include "DSP.h"
#include "Util.h"
#include "PI.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define VOICE_BASS_DRUM    0
#define VOICE_SNARE_DRUM   1
#define VOICE_LOW_TOM      2
#define VOICE_MID_TOM      3
#define VOICE_HIGH_TOM     4
#define VOICE_RIM_SHOT     5
#define VOICE_CLAP         6
#define VOICE_HIHAT_CLOSED 7
#define VOICE_HIHAT_OPEN   8
#define VOICE_CRASH        9
#define VOICE_RIDE         10

#define VOICE_COUNT        11

#define PROPERTY_FORMAT_PERCENT    0

#define PROPERTY_FORMAT_COUNT      1

#define PROPERTY_BASSDRUM_TUNE          0
#define PROPERTY_BASSDRUM_LEVEL         1
#define PROPERTY_BASSDRUM_BALANCE       2
#define PROPERTY_BASSDRUM_ATTACK        3
#define PROPERTY_BASSDRUM_DECAY         4
#define PROPERTY_SNAREDRUM_TUNE         5
#define PROPERTY_SNAREDRUM_LEVEL        6
#define PROPERTY_SNAREDRUM_BALANCE      7
#define PROPERTY_SNAREDRUM_TONE         8
#define PROPERTY_SNAREDRUM_SNAPPY       9
#define PROPERTY_LOWTOM_TUNE            10
#define PROPERTY_LOWTOM_LEVEL           11
#define PROPERTY_LOWTOM_BALANCE         12
#define PROPERTY_LOWTOM_DECAY           13
#define PROPERTY_MIDTOM_TUNE            14
#define PROPERTY_MIDTOM_LEVEL           15
#define PROPERTY_MIDTOM_BALANCE         16
#define PROPERTY_MIDTOM_DECAY           17
#define PROPERTY_HIGHTOM_TUNE           18
#define PROPERTY_HIGHTOM_LEVEL          19
#define PROPERTY_HIGHTOM_BALANCE        20
#define PROPERTY_HIGHTOM_DECAY          21
#define PROPERTY_RIMSHOT_LEVEL          22
#define PROPERTY_RIMSHOT_BALANCE        23
#define PROPERTY_CLAP_LEVEL             24
#define PROPERTY_CLAP_BALANCE           25
#define PROPERTY_HIHAT_CLOSED_LEVEL     26
#define PROPERTY_HIHAT_CLOSED_BALANCE   27
#define PROPERTY_HIHAT_CLOSED_DECAY     28
#define PROPERTY_HIHAT_OPEN_LEVEL       29
#define PROPERTY_HIHAT_OPEN_BALANCE     30
#define PROPERTY_HIHAT_OPEN_DECAY       31
#define PROPERTY_CRASH_LEVEL            32
#define PROPERTY_CRASH_BALANCE          33
#define PROPERTY_CRASH_DECAY            34
#define PROPERTY_RIDE_LEVEL             35
#define PROPERTY_RIDE_BALANCE           36
#define PROPERTY_RIDE_DECAY             37

#define PROPERTY_COUNT                  38

#define BASS_BUFFER_VCOENV  0
#define BASS_BUFFER_VCOSIG  1
#define BASS_BUFFER_AMPENV  0
#define BASS_BUFFER_AMPSIG  1
#define BASS_BUFFER_PULSE   2
#define BASS_BUFFER_NOISE   3
#define BASS_BUFFER_AMPSIG2 3

#define BASS_BUFFER_COUNT   4

#define SNARE_BUFFER_VCO1SIG   0
#define SNARE_BUFFER_VCO1ENV   1
#define SNARE_BUFFER_VCO2SIG   2
#define SNARE_BUFFER_VCO2ENV   3
#define SNARE_BUFFER_VCOENV    4
#define SNARE_BUFFER_LOWNOISE  4
#define SNARE_BUFFER_HIGHNOISE 5 
#define SNARE_BUFFER_LPAD      1
#define SNARE_BUFFER_HPAD      3
#define SNARE_BUFFER_SNAPPY    4

#define SNARE_BUFFER_COUNT     6

#define TOM_BUFFER_VCO1    0
#define TOM_BUFFER_VCO2    0
#define TOM_BUFFER_VCO3    0
#define TOM_BUFFER_VCOMIX  1
#define TOM_BUFFER_AMPAD   3
#define TOM_BUFFER_VCOMOD  2
#define TOM_BUFFER_NOISE   0
#define TOM_BUFFER_NOISEAD 2

#define TOM_BUFFER_COUNT   4

#define RIMSHOT_BUFFER_NOISE 0
#define RIMSHOT_BUFFER_AD    1
#define RIMSHOT_BUFFER_FMOD  2

#define RIMSHOT_BUFFER_COUNT 3

#define CLAP_BUFFER_NOISE 0
#define CLAP_BUFFER_AD    1
#define CLAP_BUFFER_FMOD  2

#define CLAP_BUFFER_COUNT 3

#define HIHAT_CLOSED_BUFFER_OUT   0
#define HIHAT_CLOSED_BUFFER_AD    1

#define HIHAT_CLOSED_BUFFER_COUNT 2

#define HIHAT_OPEN_BUFFER_OUT   0
#define HIHAT_OPEN_BUFFER_AD    1

#define HIHAT_OPEN_BUFFER_COUNT 2

#define CRASH_BUFFER_OSC1  0
#define CRASH_BUFFER_OSC2  1
#define CRASH_BUFFER_OSC3  2
#define CRASH_BUFFER_MIX   3
#define CRASH_BUFFER_VCF1  0
#define CRASH_BUFFER_VCF2  1
#define CRASH_BUFFER_AD1   2
#define CRASH_BUFFER_AD2   2
#define CRASH_BUFFER_AD3   2
#define CRASH_BUFFER_OUT   3

#define CRASH_BUFFER_COUNT 4

#define RIDE_BUFFER_OSC1  0
#define RIDE_BUFFER_OSC2  1
#define RIDE_BUFFER_OSC3  2
#define RIDE_BUFFER_MIX   3
#define RIDE_BUFFER_VCF1  0
#define RIDE_BUFFER_VCF2  1
#define RIDE_BUFFER_AD1   2
#define RIDE_BUFFER_AD2   2
#define RIDE_BUFFER_AD3   2
#define RIDE_BUFFER_OUT   3

#define RIDE_BUFFER_COUNT 4

const int DrumKitVoiceLevelProperties[VOICE_COUNT] =
{
  PROPERTY_BASSDRUM_LEVEL,
  PROPERTY_SNAREDRUM_LEVEL,
  PROPERTY_LOWTOM_LEVEL,
  PROPERTY_MIDTOM_LEVEL,
  PROPERTY_HIGHTOM_LEVEL,
  PROPERTY_RIMSHOT_LEVEL,
  PROPERTY_CLAP_LEVEL,
  PROPERTY_HIHAT_CLOSED_LEVEL,
  PROPERTY_HIHAT_OPEN_LEVEL,
  PROPERTY_CRASH_LEVEL,
  PROPERTY_RIDE_LEVEL
};

const char* DrumKitPropertyNames[PROPERTY_COUNT] = 
{
  "BD Tune",
  "BD Level",
  "BD Balance",
  "BD Attack",
  "BD Decay",
  "SD Tune",
  "SD Level",
  "SD Balance",
  "SD Tone",
  "SD Snappy",
  "LT Tune",
  "LT Level",
  "LT Balance",
  "LT Decay",
  "MT Tune",
  "MT Level",
  "MT Balance",
  "MT Decay",
  "HT Tune",
  "HT Level",
  "HT Balance",
  "HT Decay",
  "RS Level",
  "RS Balance",
  "CP Level",
  "CP Balance",
  "CH Level",
  "CH Balance",
  "CH Decay",
  "OH Level",
  "OH Balance",
  "OH Decay",
  "CC Level",
  "CC Balance",
  "CC Decay",
  "RC Level",
  "RC Balance",
  "RC Decay",
};
 
const char* DrumKitVoiceNames[VOICE_COUNT] = 
{
  "Bass Drum",
  "Snare Drum",
  "Low Tom",
  "Mid Tom",
  "High Tom",
  "Rim Shot",
  "Clap",
  "Closed Hi Hat",
  "Open Hi Hat",
  "Crash Cymbal",
  "Ride Cymbal"
};

const int DrumKitPropertyVoices[PROPERTY_COUNT] =
{
  VOICE_BASS_DRUM,    // BD Tune
  -1,                 // BD Level
  -1,                 // BD Balance
  VOICE_BASS_DRUM,    // BD Attack
  VOICE_BASS_DRUM,    // BD Decay
  VOICE_SNARE_DRUM,   // SD Tune
  -1,                 // SD Level
  -1,                 // SD Balance
  VOICE_SNARE_DRUM,   // SD Tone
  VOICE_SNARE_DRUM,   // SD Snappy
  VOICE_LOW_TOM,      // LT Tune
  -1,                 // LT Level
  -1,                 // LT Balance
  VOICE_LOW_TOM,      // LT Decay
  VOICE_MID_TOM,      // MT Tune
  -1,                 // MT Level
  -1,                 // MT Balance
  VOICE_MID_TOM,      // MT Decay
  VOICE_HIGH_TOM,     // HT Tune
  -1,                 // HT Level
  -1,                 // HT Balance
  VOICE_HIGH_TOM,     // HT Decay
  -1,                 // RS Level
  -1,                 // RS Balance
  -1,                 // CP Level
  -1,                 // CP Balance
  -1,                 // CH Level
  -1,                 // CH Balance
  VOICE_HIHAT_CLOSED, // CH Decay
  -1,                 // OH Level
  -1,                 // OH Balance
  VOICE_HIHAT_OPEN,   // OH Decay
  -1,                 // CC Level
  -1,                 // CC Balance
  VOICE_CRASH,        // CC Tune
  -1,                 // RC Level
  -1,                 // RC Balance
  VOICE_RIDE          // RC Tune
};

const double DrumKitPropertyDefaults[PROPERTY_COUNT] = 
{
  1.0f, // BD Tune
  1.0f, // BD Level
  0.5f, // BD Balance
  0.5f, // BD Attack
  1.0f, // BD Decay
  0.5f, // SD Tune
  1.0f, // SD Level
  0.5f, // SD Balance
  0.4f, // SD Tone
  0.8f, // SD Snappy
  1.0f, // LT Tune
  1.0f, // LT Level
  0.5f, // LT Balance
  0.5f, // LT Decay
  1.0f, // MT Tune
  1.0f, // MT Level
  0.5f, // MT Balance
  0.5f, // MT Decay
  1.0f, // HT Tune
  1.0f, // HT Level
  0.5f, // HT Balance
  0.5f, // HT Decay
  1.0f, // RS Level
  0.5f, // RS Balance
  1.0f, // CP Level
  0.5f, // CP Balance
  1.0f, // CH Level
  0.5f, // CH Balance
  0.3f, // CH Decay
  1.0f, // OH Level
  0.5f, // OH Balance
  0.3f, // OH Decay
  1.0f, // CC Level
  0.5f, // CC Balance
  0.5f, // CC Tune
  1.0f, // RC Level
  0.5f, // RC Balance
  0.5f, // RC Tune
};

const int PropertyFormat[PROPERTY_COUNT] = 
{
  PROPERTY_FORMAT_PERCENT, // BD Tune
  PROPERTY_FORMAT_PERCENT, // BD Level
  PROPERTY_FORMAT_PERCENT, // BD Balance
  PROPERTY_FORMAT_PERCENT, // BD Attack
  PROPERTY_FORMAT_PERCENT, // BD Decay
  PROPERTY_FORMAT_PERCENT, // SD Tune
  PROPERTY_FORMAT_PERCENT, // SD Level
  PROPERTY_FORMAT_PERCENT, // SD Balance
  PROPERTY_FORMAT_PERCENT, // SD Tone
  PROPERTY_FORMAT_PERCENT, // SD Snappy
  PROPERTY_FORMAT_PERCENT, // LT Tune
  PROPERTY_FORMAT_PERCENT, // LT Level
  PROPERTY_FORMAT_PERCENT, // LT Balance
  PROPERTY_FORMAT_PERCENT, // LT Decay
  PROPERTY_FORMAT_PERCENT, // MT Tune
  PROPERTY_FORMAT_PERCENT, // MT Level
  PROPERTY_FORMAT_PERCENT, // MT Balance
  PROPERTY_FORMAT_PERCENT, // MT Decay
  PROPERTY_FORMAT_PERCENT, // HT Tune
  PROPERTY_FORMAT_PERCENT, // HT Level
  PROPERTY_FORMAT_PERCENT, // HT Balance
  PROPERTY_FORMAT_PERCENT, // HT Decay
  PROPERTY_FORMAT_PERCENT, // RS Level
  PROPERTY_FORMAT_PERCENT, // RS Balance
  PROPERTY_FORMAT_PERCENT, // CP Level
  PROPERTY_FORMAT_PERCENT, // CP Balance
  PROPERTY_FORMAT_PERCENT, // CH Level
  PROPERTY_FORMAT_PERCENT, // CH Balance
  PROPERTY_FORMAT_PERCENT, // CH Decay
  PROPERTY_FORMAT_PERCENT, // OH Level
  PROPERTY_FORMAT_PERCENT, // OH Balance
  PROPERTY_FORMAT_PERCENT, // OH Decay
  PROPERTY_FORMAT_PERCENT, // CC Level
  PROPERTY_FORMAT_PERCENT, // CC Balance
  PROPERTY_FORMAT_PERCENT, // CC Tune
  PROPERTY_FORMAT_PERCENT, // RC Level
  PROPERTY_FORMAT_PERCENT, // RC Balance
  PROPERTY_FORMAT_PERCENT  // RC Tune
};

// Module Level Functions

double DrumKitPropertyToPowerRatio(double value)
{
  if(value <= 0.0f)
    return 0.0f;
  else
    return dBToPowerRatio(-((1.0f - value) * 32.0f));
}

// Drum Kit Voice

CDrumKitVoice::CDrumKitVoice(int sampleRate, int blockSamples, int bufferCount)
{
  int i;

  Idle         = true;
  SampleRate   = sampleRate;
  BlockSamples = blockSamples;
  BufferCount  = bufferCount;
  InitLevels   = true;

  if(BufferCount > 0)
  {
    Buffer = new double*[BufferCount];
    for(i = 0; i < BufferCount; i++)
      Buffer[i] = (double*)_mm_malloc(sizeof(double) * blockSamples, AUDIO_BUFFER_ALIGN);
  }
  else
    Buffer = 0;

  ControlBuffer = new CControlBuffer();
}

CDrumKitVoice::~CDrumKitVoice()
{
  int i;

  delete ControlBuffer;

  if(BufferCount > 0)
  {
    for(i = 0; i < BufferCount; i++)
      _mm_free(Buffer[i]);
    delete Buffer;
  }
}

void CDrumKitVoice::SetIdle(bool value)
{
  Idle = value;
}

bool CDrumKitVoice::IsIdle()
{
  return Idle;
}

void CDrumKitVoice::SetBlockSamples(int blockSamples)
{
  int i;

  for(i = 0; i < BufferCount; i++)
  {
    _mm_free(Buffer[i]);
    Buffer[i] = (double*)_mm_malloc(sizeof(double) * blockSamples, AUDIO_BUFFER_ALIGN);
  }
  BlockSamples = blockSamples;
}

void CDrumKitVoice::DiodeClip(int buffer, int samples, double level)
{
  int   i;
  double nlevel, v;

  nlevel = -level;

  for(i = 0; i < samples; i++)
  {
    v = Buffer[buffer][i];
    if(v < nlevel)
      v = nlevel;
    else if(v > level)
      v = level;
    Buffer[buffer][i] = v;
  }
}

void CDrumKitVoice::Work(double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel)
{
  SControlMessage message;
  bool            gotMessage;
  int             firstSample, lastSample;

  gotMessage = ControlBuffer->Read(&message);
  if(!gotMessage) // no messages, just generate signal
  {
    if(!IsIdle())
      GenerateBlock(0, BlockSamples, outLeft, outRight, commonNoise, leftLevel, rightLevel);
  }
  else // got one or more messages
  {
    if(message.BlockSample > 0) // process data before first block
      if(!IsIdle())
        GenerateBlock(0, message.BlockSample, outLeft, outRight, commonNoise, leftLevel, rightLevel);

    while(gotMessage) // process message and data until no more messages
    {
      ProcessMessage(&message);
      firstSample = message.BlockSample;
      gotMessage = ControlBuffer->Read(&message);
      if(gotMessage)
        lastSample = message.BlockSample - 1;
      else
        lastSample = BlockSamples - 1;
      if(!IsIdle())
        GenerateBlock(firstSample, lastSample - firstSample + 1, outLeft, outRight, commonNoise, leftLevel, rightLevel);
    }
  }
}

void CDrumKitVoice::ClearBuffer(int index, int samples)
{
  double *buffer;

  buffer = Buffer[index];
  while(samples-- > 0)
    *(buffer++) = 0.0;
}

void CDrumKitVoice::WriteMessage(SControlMessage *message)
{
  ControlBuffer->Write(message);
}

// Bass Drum

CBassDrum::CBassDrum(int sampleRate, int blockSamples)
: CDrumKitVoice(sampleRate, blockSamples, BASS_BUFFER_COUNT)
{
  VCO = new CVCO(SampleRate, SinTable, 1.0f, 0, 0);
  VCO->SetVoltage(0.0f);
  VCA1 = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);
  VCOAD = new CAD(SampleRate, 0.00f, 0.50f, 0.0f,  -6.0f);
  AmpAD1 = new CAD(SampleRate, 0.05f, 0.50f, 0.0f, -4.0f);

  LPF1   = new CMultiFilter(SampleRate, MULTIFILTER_MODE_LP, MULTIFILTER_POLES_2, FrequencyToVoltage(1200.0f), 0.0f);
  BPF1   = new CMultiFilter(SampleRate, MULTIFILTER_MODE_BP, MULTIFILTER_POLES_2, FrequencyToVoltage(800.0f),  3.0f);
  LPF2   = new CMultiFilter(SampleRate, MULTIFILTER_MODE_LP, MULTIFILTER_POLES_2, FrequencyToVoltage(300.0f),  0.0f);
  AmpAD2 = new CAD(SampleRate, 0.005f, 0.1f, 1.0f, 1.0f);
  VCA2   = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);

  Trigger = false;
}

CBassDrum::~CBassDrum()
{
  delete VCO;
  delete VCA1;
  delete AmpAD1;
  delete VCOAD;
  delete LPF1;
  delete BPF1;
  delete LPF2;
  delete AmpAD2;
  delete VCA2;
}

void CBassDrum::SetSampleRate(int sampleRate)
{
  VCO->SetSampleRate(sampleRate);
  AmpAD1->SetSampleRate(sampleRate);
  VCOAD->SetSampleRate(sampleRate);
  LPF1->SetSampleRate(sampleRate);
  BPF1->SetSampleRate(sampleRate);
  LPF2->SetSampleRate(sampleRate);
  AmpAD2->SetSampleRate(sampleRate);
  SampleRate = sampleRate;
}

void CBassDrum::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_NOTE_ON:
    case CONTROL_MESSAGE_VOICE_ON:
    {
      AmpAD1->Trigger();
      AmpAD2->Trigger();
      VCOAD->Trigger();
      Trigger = true;
      SetIdle(false);
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
        case PROPERTY_BASSDRUM_TUNE:
        {
          // Min Tone - 142 Hz - 50 Hz
          // Max Tone - 250 Hz - 50 Hz
          BaseVoltage  = FrequencyToVoltage(50.0f);
          SweepVoltage = FrequencyToVoltage(142.0f + ((250.0f - 142.0f) * value)) - BaseVoltage;
          AmpAD1->SetSustain(value * 0.05f);
          break;
        }
        case PROPERTY_BASSDRUM_ATTACK:
        {
          // 0.001 - 0.02
          AmpAD1->SetAttack(0.001f + (value * 0.019f));
          break;
        }
        case PROPERTY_BASSDRUM_DECAY:
        {
          // 0.1 - 0.5
          AmpAD1->SetDecay(0.1f + (value * 0.4f));
          VCOAD->SetDecay(0.1f + (value * 0.4f));
          AmpAD2->SetDecay(value * 0.1f);
          break;
        }
      }
    }
  }
}

void CBassDrum::GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel)
{
  int   i;
  double out;

  // Frequency Sweeping VCO

  VCOAD->Generate(samples, Buffer[BASS_BUFFER_VCOENV]);
  for(i = 0; i < samples; i++)
    Buffer[BASS_BUFFER_VCOENV][i] = BaseVoltage + (Buffer[BASS_BUFFER_VCOENV][i] * SweepVoltage);
  VCO->Generate(Buffer[BASS_BUFFER_VCOENV], Buffer[BASS_BUFFER_VCOSIG], samples, Core->DSP->TriangleWave, Buffer[BASS_BUFFER_VCOENV], 0.15f);

  AmpAD1->Generate(samples, Buffer[BASS_BUFFER_AMPENV]);
  VCA1->Process(samples, Buffer[BASS_BUFFER_VCOSIG], Buffer[BASS_BUFFER_AMPENV]);
  
  DiodeClip(BASS_BUFFER_VCOSIG, samples, 0.9f);

  // Pulse Generator

  for(i = 0; i < samples; i++)
  {
    if(Trigger)
    {
      Buffer[BASS_BUFFER_PULSE][i] = 1.0f;
      Trigger = false;
    }
    else
    {
      Buffer[BASS_BUFFER_PULSE][i] = 0.0f;
    }
  }

  LPF1->Process(samples, Buffer[BASS_BUFFER_PULSE], 0, 0);
  BPF1->Process(samples, Buffer[BASS_BUFFER_PULSE], 0, 0);

  // Low Pass Filtered Noise

  for(i = 0; i < samples; i++)
    Buffer[BASS_BUFFER_NOISE][i] = commonNoise[sample + i];
  LPF2->Process(samples, Buffer[BASS_BUFFER_NOISE], 0, 0);


  // Mix Pulse And Noise
  for(i = 0; i < samples; i++)
   Buffer[BASS_BUFFER_PULSE][i] = (Buffer[BASS_BUFFER_PULSE][i] * 500.0f) + (0.4f * Buffer[BASS_BUFFER_NOISE][i]);
  AmpAD2->Generate(samples, Buffer[BASS_BUFFER_AMPSIG2]);
  VCA2->Process(samples, Buffer[BASS_BUFFER_PULSE], Buffer[BASS_BUFFER_AMPSIG2]);

  for(i = 0; i < samples; i++)
  {
    out = Buffer[BASS_BUFFER_AMPSIG][i] + Buffer[BASS_BUFFER_PULSE][i];
    if(InitLevels)
    {
      LeftLevelFollow = leftLevel;
      RightLevelFollow = rightLevel;
      InitLevels = false;
    }
    else
    {
      LeftLevelFollow  = AdjustVolume(leftLevel, LeftLevelFollow);
      RightLevelFollow = AdjustVolume(rightLevel, RightLevelFollow);
    }
    outLeft[sample + i]  += out * LeftLevelFollow; 
    outRight[sample + i] += out * RightLevelFollow; 
  }

  if(!AmpAD1->IsTriggered())
    SetIdle(true);
}

// Snare Drum

CSnareDrum::CSnareDrum(int sampleRate, int blockSamples)
: CDrumKitVoice(sampleRate, blockSamples, SNARE_BUFFER_COUNT)
{
  double f1, f2;

  Snappy = 0.0f;

  AmpAD1 = new CAD(sampleRate, 0.001f, 0.17f,  0.0f, -4.0f);
  AmpAD2 = new CAD(sampleRate, 0.001f, 0.17f,  0.0f, -8.0f);
  VCOAD  = new CAD(sampleRate, 0.0f,   0.17f,  0.0f, -2.0f);
  LPAD   = new CAD(sampleRate, 0.0f,   0.0f,   0.0f,  0.0f);
  HPAD   = new CAD(sampleRate, 0.03f,  0.09f,  0.0f,  0.0f);
  LPAD->SetSustain(0.002f);
  
  AmpAD1->SetPredelay(0.002f);
  AmpAD2->SetPredelay(0.002f);
  VCOAD->SetPredelay(0.002f);

  f1 = 140.0f;
  f2 = (f1 / 180.0f) * 330.0f;

  VCO1 = new CVCO(sampleRate, SinTable, f1, 0, 0);
  VCO2 = new CVCO(sampleRate, SinTable, f2, 0, 0);

  VCA  = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);

  LPF = new CMultiFilter(sampleRate, MULTIFILTER_MODE_LP, MULTIFILTER_POLES_2, FrequencyToVoltage(5000.0f), 0.0f);
  HPF = new CMultiFilter(sampleRate, MULTIFILTER_MODE_HP, MULTIFILTER_POLES_2, FrequencyToVoltage(5000.0f), 0.0f);
}

CSnareDrum::~CSnareDrum()
{
  delete LPF;
  delete HPF;
  delete LPAD;
  delete HPAD;
  delete AmpAD1;
  delete AmpAD2;
  delete VCOAD;
  delete VCO1;
  delete VCO2;
  delete VCA;
}

void CSnareDrum::SetSampleRate(int sampleRate)
{
  LPF->SetSampleRate(sampleRate);
  HPF->SetSampleRate(sampleRate);
  LPAD->SetSampleRate(sampleRate);
  HPAD->SetSampleRate(sampleRate);
  AmpAD1->SetSampleRate(sampleRate);
  AmpAD2->SetSampleRate(sampleRate);
  VCOAD->SetSampleRate(sampleRate);
  VCO1->SetSampleRate(sampleRate);
  VCO2->SetSampleRate(sampleRate);
}

void CSnareDrum::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_NOTE_ON:
    case CONTROL_MESSAGE_VOICE_ON:
    {
      AmpAD1->Trigger();
      AmpAD2->Trigger();
      VCOAD->Trigger();
      LPAD->Trigger();
      HPAD->Trigger();
      SetIdle(false);
      break;
    }

    case CONTROL_MESSAGE_CONTROL_CHANGE:
    {
      double value;

      value = message->ControlChange.Value;
      switch(message->ControlChange.Controller)
      {
        case PROPERTY_SNAREDRUM_TUNE:
        {
          double f1, f2;

          f1 = 140.0f + (80.0f * value);
          f2 = (f1 / 180.0f) * 330.0f;

          VCO1->SetFrequency(f1);
          VCO2->SetFrequency(f2);

          break;
        }

        case PROPERTY_SNAREDRUM_TONE:
        {
          HPAD->SetDecay(0.09f + (0.18f * value));
          break;
        }

        case PROPERTY_SNAREDRUM_SNAPPY:
        {
          Snappy = DrumKitPropertyToPowerRatio(value);
          break;
        }
      }
    }
  }
}

void CSnareDrum::GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel)
{
  int   i;
  double out;

  // VCO Part

  VCOAD->Generate(samples, Buffer[SNARE_BUFFER_VCOENV]);
  for(i = 0; i < samples; i++)
    Buffer[SNARE_BUFFER_VCOENV][i] = 0.12f * (-1.0f + Buffer[SNARE_BUFFER_VCOENV][i]);

  AmpAD1->Generate(samples, Buffer[SNARE_BUFFER_VCO1ENV]);
  VCO1->Generate(Buffer[SNARE_BUFFER_VCOENV], Buffer[SNARE_BUFFER_VCO1SIG], samples, Core->DSP->TriangleWave, Buffer[SNARE_BUFFER_VCO1ENV], 0.3f);
  VCA->Process(samples, Buffer[SNARE_BUFFER_VCO1SIG], Buffer[SNARE_BUFFER_VCO1ENV]);

  AmpAD2->Generate(samples, Buffer[SNARE_BUFFER_VCO2ENV]);
  VCO2->Generate(Buffer[SNARE_BUFFER_VCOENV], Buffer[SNARE_BUFFER_VCO2SIG], samples, Core->DSP->TriangleWave, Buffer[SNARE_BUFFER_VCO2ENV], 0.3f);
  VCA->Process(samples, Buffer[SNARE_BUFFER_VCO2SIG], Buffer[SNARE_BUFFER_VCO2ENV]);

  // Diode Clipper

  DiodeClip(SNARE_BUFFER_VCO1SIG, samples, 0.8f);
  DiodeClip(SNARE_BUFFER_VCO2SIG, samples, 0.8f);

  // Snappy

  for(i = 0; i < samples; i++)
    Buffer[SNARE_BUFFER_LOWNOISE][i] = commonNoise[sample + i];

  LPF->Process(samples, Buffer[SNARE_BUFFER_LOWNOISE], 0, 0);

  for(i = 0; i < samples; i++)
    Buffer[SNARE_BUFFER_HIGHNOISE][i] = Buffer[SNARE_BUFFER_LOWNOISE][i];

  HPF->Process(samples, Buffer[SNARE_BUFFER_LOWNOISE], 0, 0);

  LPAD->Generate(samples, Buffer[SNARE_BUFFER_LPAD]);
  HPAD->Generate(samples, Buffer[SNARE_BUFFER_HPAD]);

  VCA->Process(samples, Buffer[SNARE_BUFFER_LOWNOISE], Buffer[SNARE_BUFFER_LPAD]);
  VCA->Process(samples, Buffer[SNARE_BUFFER_HIGHNOISE], Buffer[SNARE_BUFFER_HPAD]);

  for(i = 0; i < samples; i++)
  {
    if(InitLevels)
      SnappyFollow = Snappy;
    else
      SnappyFollow = AdjustVolume(Snappy, SnappyFollow);
    Buffer[SNARE_BUFFER_SNAPPY][i] = ((0.1f + SnappyFollow) * Buffer[SNARE_BUFFER_LOWNOISE][i]) + (1.2f * SnappyFollow * Buffer[SNARE_BUFFER_HIGHNOISE][i]);
  }
  for(i = 0; i < samples; i++)
  {
    out = Buffer[SNARE_BUFFER_VCO1SIG][i] + (0.4f * Buffer[SNARE_BUFFER_VCO2SIG][i]) +
          Buffer[SNARE_BUFFER_SNAPPY][i];
    if(InitLevels)
    {
      LeftLevelFollow = leftLevel;
      RightLevelFollow = rightLevel;
      InitLevels = false;
    }
    else
    {
      LeftLevelFollow  = AdjustVolume(leftLevel, LeftLevelFollow);
      RightLevelFollow = AdjustVolume(rightLevel, RightLevelFollow);
    }
    outLeft[sample + i]  += out * LeftLevelFollow; 
    outRight[sample + i] += out * RightLevelFollow; 
  }

  if(!((LPAD->IsTriggered()) || (HPAD->IsTriggered())))
    SetIdle(true);
}

// Tom Drum

CTomDrum::CTomDrum(int sampleRate, int blockSamples, int tomType)
: CDrumKitVoice(sampleRate, blockSamples, TOM_BUFFER_COUNT)
{
  TomType = tomType;

  AmpAD1 = new CAD(sampleRate, 0.01f, 0.31f,  0.0f, -2.0f);  
  AmpAD1->SetSustain(0.04f);

  NoiseAD = new CAD(sampleRate, 0.0001f, 0.001f, 0.0f, 0.0f);
  NoiseAD->SetSustain(0.00f);

  VCO1 = new CVCO(sampleRate, SinTable, 1.0, 0, 0);
  VCO2 = new CVCO(sampleRate, SinTable, 1.0, 0, 0);
  VCO3 = new CVCO(sampleRate, SinTable, 1.0, 0, 0);

  SetTune(0.0f);
  
  NoiseFilter = new CMultiFilter(sampleRate, MULTIFILTER_MODE_LP, MULTIFILTER_POLES_2, FrequencyToVoltage(1100.0f), 4.0f);

  VCA  = new CVCA(VCA_MODE_EXPONENTIAL, 1.0);
}

void CTomDrum::SetDecay(double value)
{
  AmpAD1->SetDecay(0.23f + (0.17f * value));
}

void CTomDrum::SetTune(double value)
{
  double tune = 1.0 + value;

  switch(TomType)
  {
    case 0: // Low Tom
    {
      VCO1->SetFrequency(45.45 * tune);
      VCO2->SetFrequency(30.3  * tune);
      VCO3->SetFrequency(83.33 * tune);
      break;
    }
    case 1: // Mid Tom
    {
      VCO1->SetFrequency(55.56 * tune);
      VCO2->SetFrequency(37.03 * tune);
      VCO3->SetFrequency(100.0 * tune);
      break;
    }
    case 2: // High Tom
    {
      VCO1->SetFrequency(66.67  * tune);
      VCO2->SetFrequency(45.45  * tune);
      VCO3->SetFrequency(121.95 * tune);
      break;
    }    
  }
}

CTomDrum::~CTomDrum()
{
  delete AmpAD1;
  delete NoiseAD;
  delete VCO1;
  delete VCO2;
  delete VCO3;
  delete NoiseFilter;
  delete VCA;
}

void CTomDrum::SetSampleRate(int sampleRate)
{
  AmpAD1->SetSampleRate(sampleRate);
  NoiseAD->SetSampleRate(sampleRate);
  VCO1->SetSampleRate(sampleRate);
  VCO2->SetSampleRate(sampleRate);
  VCO3->SetSampleRate(sampleRate);
  NoiseFilter->SetSampleRate(sampleRate);
}

void CTomDrum::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_VOICE_ON:
    case CONTROL_MESSAGE_NOTE_ON:
    {
      AmpAD1->Trigger();
      NoiseAD->Trigger();
      SetIdle(false);
      break;
    };
    case CONTROL_MESSAGE_CONTROL_CHANGE:
    {
      double value;

      value = message->ControlChange.Value;

      switch(message->ControlChange.Controller)
      {
        case PROPERTY_LOWTOM_TUNE:
        case PROPERTY_MIDTOM_TUNE:
        case PROPERTY_HIGHTOM_TUNE:
        {
          SetTune(value);
          break;
        }
        case PROPERTY_LOWTOM_DECAY:
        case PROPERTY_MIDTOM_DECAY:
        case PROPERTY_HIGHTOM_DECAY:
        {
          SetDecay(value);
          break;
        }
      }
    };
  }
}

void CTomDrum::GenerateBlock(int sample, int samples, double *outLeft, double *outRight,
                             double *commonNoise, double leftLevel, double rightLevel)
{
  int i;
  double v;
  const double mix = 0.2f;

  AmpAD1->Generate(samples, Buffer[TOM_BUFFER_AMPAD]);
  for(i = 0; i < samples; i++)
    Buffer[TOM_BUFFER_VCOMOD][i] = (Buffer[TOM_BUFFER_AMPAD][i] - 0.5) * 0.2;

  VCO1->Generate(Buffer[TOM_BUFFER_VCOMOD], Buffer[TOM_BUFFER_VCO1], samples, Core->DSP->TriangleWave, 0, mix);
  MixBlocks(Buffer[TOM_BUFFER_VCOMIX], Buffer[TOM_BUFFER_VCO1], samples, 0.2, true, false, Core->CPUFeatures);

  VCO2->Generate(Buffer[TOM_BUFFER_VCOMOD], Buffer[TOM_BUFFER_VCO2], samples, Core->DSP->TriangleWave, 0, mix);
  MixBlocks(Buffer[TOM_BUFFER_VCOMIX], Buffer[TOM_BUFFER_VCO2], samples, 1.0, false, false, Core->CPUFeatures);

  VCO3->Generate(Buffer[TOM_BUFFER_VCOMOD], Buffer[TOM_BUFFER_VCO3], samples, Core->DSP->TriangleWave, 0, mix);
  MixBlocks(Buffer[TOM_BUFFER_VCOMIX], Buffer[TOM_BUFFER_VCO3], samples, 0.2, false, false, Core->CPUFeatures);


  NoiseAD->Generate(samples, Buffer[TOM_BUFFER_NOISEAD]);
  for(i = 0; i < samples; i++)
    Buffer[TOM_BUFFER_NOISE][i] = commonNoise[i + sample];
  NoiseFilter->Process(samples, Buffer[TOM_BUFFER_NOISE], Buffer[TOM_BUFFER_NOISEAD], 0);
  VCA->Process(samples, Buffer[TOM_BUFFER_NOISE], Buffer[TOM_BUFFER_NOISEAD]);
  MixBlocks(Buffer[TOM_BUFFER_VCOMIX], Buffer[TOM_BUFFER_NOISE], samples, 2.0f, false, false, Core->CPUFeatures);


  VCA->Process(samples, Buffer[TOM_BUFFER_VCOMIX], Buffer[TOM_BUFFER_AMPAD]);
  for(i = 0; i < samples; i++)
  {
    v = Buffer[TOM_BUFFER_VCOMIX][i];
    outLeft[sample + i]  += v * leftLevel;
    outRight[sample + i] += v * rightLevel;
  }

  if(!AmpAD1->IsTriggered())
    SetIdle(true);
}

// Rim Shot

CRimShot::CRimShot(int sampleRate, int blockSamples)
: CDrumKitVoice(sampleRate, blockSamples, RIMSHOT_BUFFER_COUNT)
{
  AD1 = new CAD(sampleRate, 0.005f, 0.054f, 0.0f, 0.0f);
  AD2 = new CAD(sampleRate, 0.0f, 0.06f, 0.0f, 0.0f);
  PinkFilter = new CMultiFilter(sampleRate, MULTIFILTER_MODE_LP, MULTIFILTER_POLES_4, FrequencyToVoltage(4000.0f), 3.0f);
  LPFilter = new CMultiFilter(sampleRate, MULTIFILTER_MODE_BP, MULTIFILTER_POLES_2, 4.0f, 0.0f);
  VCA = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);
}

CRimShot::~CRimShot()
{
  delete AD1;
  delete AD2;
  delete PinkFilter;
  delete LPFilter;
  delete VCA;
}

void CRimShot::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_VOICE_ON:
    case CONTROL_MESSAGE_NOTE_ON:
    {
      AD1->Trigger();
      AD2->Trigger();
      SetIdle(false);
      break;
    };
  }
}

void CRimShot::SetSampleRate(int sampleRate)
{
  AD1->SetSampleRate(sampleRate);
  AD2->SetSampleRate(sampleRate);
  PinkFilter->SetSampleRate(sampleRate);
  LPFilter->SetSampleRate(sampleRate);
}

void CRimShot::GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel)
{
  double v;
  int   i;

  MixBlocks(Buffer[RIMSHOT_BUFFER_NOISE], commonNoise, samples, 4.0f, true, false, Core->CPUFeatures); 
  PinkFilter->Process(samples, Buffer[RIMSHOT_BUFFER_NOISE], 0, 0);
  AD1->Generate(samples, Buffer[RIMSHOT_BUFFER_AD]);
  AD2->Generate(samples, Buffer[RIMSHOT_BUFFER_FMOD]);
  

  for(i = 0; i < samples; i++)
    Buffer[RIMSHOT_BUFFER_FMOD][i] *= 2.0f;

  LPFilter->Process(samples, Buffer[RIMSHOT_BUFFER_NOISE], Buffer[RIMSHOT_BUFFER_FMOD], 0); 

  VCA->Process(samples, Buffer[RIMSHOT_BUFFER_NOISE], Buffer[RIMSHOT_BUFFER_AD]);
  for(i = 0; i < samples; i++)
  {
    v = Buffer[RIMSHOT_BUFFER_NOISE][i];
    outLeft[sample + i]  += v * leftLevel;
    outRight[sample + i] += v * rightLevel;
  }

  if(!(AD1->IsTriggered() || AD2->IsTriggered()))
   SetIdle(true);
}


// Clap

CClap::CClap(int sampleRate, int blockSamples)
: CDrumKitVoice(sampleRate, blockSamples, CLAP_BUFFER_COUNT)
{
  AD1 = new CAD(sampleRate, 0.02f, 0.14f, 0.0f, 2.0f);
  AD2 = new CAD(sampleRate, 0.01f, 0.03f, 0.0f, 2.0f);
  
  AD2->SetRetrigger(3);

  PinkFilter = new CMultiFilter(sampleRate, MULTIFILTER_MODE_LP, MULTIFILTER_POLES_2, FrequencyToVoltage(1000.0f), 0.0f);
  LPFilter = new CMultiFilter(sampleRate, MULTIFILTER_MODE_BP, MULTIFILTER_POLES_2, 7.0f, 0.0f);
  VCA = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);
}

CClap::~CClap()
{
  delete AD1;
  delete AD2;
  delete PinkFilter;
  delete LPFilter;
  delete VCA;
}

void CClap::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_VOICE_ON:
    case CONTROL_MESSAGE_NOTE_ON:
    {
      AD1->Trigger();
      AD2->Trigger();
      SetIdle(false);
      break;
    };
  }
}

void CClap::SetSampleRate(int sampleRate)
{
  AD1->SetSampleRate(sampleRate);
  AD2->SetSampleRate(sampleRate);
  PinkFilter->SetSampleRate(sampleRate);
  LPFilter->SetSampleRate(sampleRate);
}

void CClap::GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel)
{
  double v;
  int   i;

  MixBlocks(Buffer[CLAP_BUFFER_NOISE], commonNoise, samples, 16.0f, true, false, Core->CPUFeatures); 
  PinkFilter->Process(samples, Buffer[CLAP_BUFFER_NOISE], 0, 0);
  AD1->Generate(samples, Buffer[CLAP_BUFFER_AD]);
  AD2->Generate(samples, Buffer[CLAP_BUFFER_FMOD]);
  

  for(i = 0; i < samples; i++)
    Buffer[CLAP_BUFFER_FMOD][i] *= 5.0f;

  LPFilter->Process(samples, Buffer[CLAP_BUFFER_NOISE], Buffer[CLAP_BUFFER_FMOD], 0); 

  VCA->Process(samples, Buffer[CLAP_BUFFER_NOISE], Buffer[CLAP_BUFFER_AD]);
  for(i = 0; i < samples; i++)
  {
    v = Buffer[CLAP_BUFFER_NOISE][i];
    outLeft[sample + i]  += v * leftLevel;
    outRight[sample + i] += v * rightLevel;
  }

  if(!(AD1->IsTriggered() || AD2->IsTriggered()))
   SetIdle(true);
}

// Hi Hat Closed

CHiHatClosed::CHiHatClosed(int sampleRate, int blockSamples)
: CDrumKitVoice(sampleRate, blockSamples, HIHAT_CLOSED_BUFFER_COUNT)
{
  AD = new CAD(sampleRate, 0.0001f, 0.13f, 0.0f, 0.0f);
  BPFilter = new CMultiFilter(sampleRate, MULTIFILTER_MODE_BP, MULTIFILTER_POLES_2, FrequencyToVoltage(8000.0f), 3.0f);
  VCA = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);
}

CHiHatClosed::~CHiHatClosed()
{
  delete AD;
  delete BPFilter;
  delete VCA;
}

void CHiHatClosed::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_VOICE_ON:
    case CONTROL_MESSAGE_NOTE_ON:
    {
      AD->Trigger();
      SetIdle(false);
      break;
    };
    case CONTROL_MESSAGE_CONTROL_CHANGE:
    {
      double value;

      value = message->ControlChange.Value;

      switch(message->ControlChange.Controller)
      {
        case PROPERTY_HIHAT_CLOSED_DECAY:
        {
          AD->SetDecay(0.045f + value);
          break;
        };
      }

      break;
    };
  }
}

void CHiHatClosed::SetSampleRate(int sampleRate)
{
  AD->SetSampleRate(sampleRate);
  BPFilter->SetSampleRate(sampleRate);
}

void CHiHatClosed::GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel)
{
  double v = 0.0f;
  int i;

  MixBlocks(Buffer[HIHAT_CLOSED_BUFFER_OUT], commonNoise, samples, 1.0f, true, false, Core->CPUFeatures);
  BPFilter->Process(samples, Buffer[HIHAT_CLOSED_BUFFER_OUT], 0, 0);
  AD->Generate(samples, Buffer[HIHAT_CLOSED_BUFFER_AD]);
  VCA->Process(samples, Buffer[HIHAT_CLOSED_BUFFER_OUT], Buffer[HIHAT_CLOSED_BUFFER_AD]);

  for(i = 0; i < samples; i++)
  {
    v = Buffer[HIHAT_CLOSED_BUFFER_OUT][i];
    outLeft[sample + i]  += v * leftLevel;
    outRight[sample + i] += v * rightLevel;
  }

  if(!AD->IsTriggered())
   SetIdle(true);
}

// Hi Hat Open

CHiHatOpen::CHiHatOpen(int sampleRate, int blockSamples)
: CDrumKitVoice(sampleRate, blockSamples, HIHAT_OPEN_BUFFER_COUNT)
{
  AD = new CAD(sampleRate, 0.0001f, 0.13f, 0.0f, 0.0f);
  BPFilter = new CMultiFilter(sampleRate, MULTIFILTER_MODE_BP, MULTIFILTER_POLES_2, FrequencyToVoltage(8000.0f), 0.0f);
  HPFilter = new CMultiFilter(sampleRate, MULTIFILTER_MODE_HP, MULTIFILTER_POLES_2, FrequencyToVoltage(4000.0f), 0.0f);
  VCA = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);
}

CHiHatOpen::~CHiHatOpen()
{
  delete AD;
  delete BPFilter;
  delete HPFilter;
  delete VCA;
}

void CHiHatOpen::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_VOICE_ON:
    case CONTROL_MESSAGE_NOTE_ON:
    {
      AD->Trigger();
      SetIdle(false);
      break;
    };
    case CONTROL_MESSAGE_CONTROL_CHANGE:
    {
      double value;

      value = message->ControlChange.Value;

      switch(message->ControlChange.Controller)
      {
        case PROPERTY_HIHAT_OPEN_DECAY:
        {
          AD->SetDecay(0.25f + (value * 0.5f));
          break;
        };
      }

      break;
    };
  }
}

void CHiHatOpen::SetSampleRate(int sampleRate)
{
  AD->SetSampleRate(sampleRate);
  BPFilter->SetSampleRate(sampleRate);
  HPFilter->SetSampleRate(sampleRate);
}

void CHiHatOpen::GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel)
{
  double v = 0.0f;
  int i;

  MixBlocks(Buffer[HIHAT_OPEN_BUFFER_OUT], commonNoise, samples, 1.0f, true, false, Core->CPUFeatures);
  BPFilter->Process(samples, Buffer[HIHAT_OPEN_BUFFER_OUT], 0, 0);
  HPFilter->Process(samples, Buffer[HIHAT_OPEN_BUFFER_OUT], 0, 0);
  
  AD->Generate(samples, Buffer[HIHAT_OPEN_BUFFER_AD]);
  VCA->Process(samples, Buffer[HIHAT_OPEN_BUFFER_OUT], Buffer[HIHAT_OPEN_BUFFER_AD]);

  for(i = 0; i < samples; i++)
  {
    v = Buffer[HIHAT_OPEN_BUFFER_OUT][i];
    outLeft[sample + i]  += v * leftLevel;
    outRight[sample + i] += v * rightLevel;
  }

  if(!AD->IsTriggered())
   SetIdle(true);
}

// Crash Cymbal

CCrashCymbal::CCrashCymbal(int sampleRate, int blockSamples)
: CDrumKitVoice(sampleRate, blockSamples, CRASH_BUFFER_COUNT)
{
  AD1 = new CAD(sampleRate, 0.005f, 0.075f, 0.0f, 0.0f);
  AD2 = new CAD(sampleRate, 0.026f, 3.700f, 0.0f, 0.0f);
  AD3 = new CAD(sampleRate, 0.005f, 1.400f, 0.0f, -4.0f);

  
  Filter1 = new CMultiFilter(sampleRate, MULTIFILTER_MODE_BP, MULTIFILTER_POLES_2, FrequencyToVoltage(1050.0f), 0.0f);
  Filter2 = new CMultiFilter(sampleRate, MULTIFILTER_MODE_HP, MULTIFILTER_POLES_2, FrequencyToVoltage(2049.0f), 0.0f);
  VCA = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);
  
  VCO1 = new CVCO(sampleRate, SinTable, 1047.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  VCO2 = new CVCO(sampleRate, SinTable, 1109.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  VCO3 = new CVCO(sampleRate, SinTable, 1175.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  
  VCO4 = new CVCO(sampleRate, SinTable, 1481.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  VCO5 = new CVCO(sampleRate, SinTable, 1049.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  VCO6 = new CVCO(sampleRate, SinTable, 1480.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 

  VCO1->SetShape(VCO_SHAPE_SQUARE);
  VCO2->SetShape(VCO_SHAPE_SQUARE);
  VCO3->SetShape(VCO_SHAPE_SQUARE);
  VCO4->SetShape(VCO_SHAPE_SQUARE);
  VCO5->SetShape(VCO_SHAPE_SQUARE);
  VCO6->SetShape(VCO_SHAPE_SQUARE);
}

CCrashCymbal::~CCrashCymbal()
{
  delete AD1;
  delete AD2;
  delete AD3;
  delete Filter1;
  delete Filter2;
  delete VCA;
  delete VCO1;
  delete VCO2;
  delete VCO3;
  delete VCO4;
  delete VCO5;
  delete VCO6;
}

void CCrashCymbal::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_VOICE_ON:
    case CONTROL_MESSAGE_NOTE_ON:
    {
      AD1->Trigger();
      AD2->Trigger();
      AD3->Trigger();
      SetIdle(false);
      break;
    };
    case CONTROL_MESSAGE_CONTROL_CHANGE:
    {
      double value;

      value = message->ControlChange.Value;

      switch(message->ControlChange.Controller)
      {
        case PROPERTY_CRASH_DECAY:
        {
          AD3->SetDecay(0.4f + value);
          break;
        };
      }

      break;
    };
  }
}

void CCrashCymbal::SetSampleRate(int sampleRate)
{
  AD1->SetSampleRate(sampleRate);
  AD2->SetSampleRate(sampleRate);
  AD3->SetSampleRate(sampleRate);
  Filter1->SetSampleRate(sampleRate);
  Filter2->SetSampleRate(sampleRate);
  VCO1->SetSampleRate(sampleRate);
  VCO2->SetSampleRate(sampleRate);
  VCO3->SetSampleRate(sampleRate);
  VCO4->SetSampleRate(sampleRate);
  VCO5->SetSampleRate(sampleRate);
  VCO6->SetSampleRate(sampleRate);
}

void CCrashCymbal::GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel)
{
  double v = 0.0f;
  int i;


  VCO1->GenerateBLEP(0, 0, 0, Buffer[CRASH_BUFFER_OSC1], 0, samples);
  VCO2->GenerateBLEP(0, 0, 0, Buffer[CRASH_BUFFER_OSC2], 0, samples);
  VCO3->GenerateBLEP(0, 0, 0, Buffer[CRASH_BUFFER_OSC3], 0, samples);

  VCO1->GenerateBLEP(0, 0, Buffer[CRASH_BUFFER_OSC1], Buffer[CRASH_BUFFER_OSC1], 0, samples);
  VCO2->GenerateBLEP(0, 0, Buffer[CRASH_BUFFER_OSC2], Buffer[CRASH_BUFFER_OSC2], 0, samples);
  VCO3->GenerateBLEP(0, 0, Buffer[CRASH_BUFFER_OSC3], Buffer[CRASH_BUFFER_OSC3], 0, samples);

  MixBlocks(Buffer[CRASH_BUFFER_MIX], Buffer[CRASH_BUFFER_OSC1], samples, 1.0f, true,  false, Core->CPUFeatures);
  MixBlocks(Buffer[CRASH_BUFFER_MIX], Buffer[CRASH_BUFFER_OSC2], samples, 1.0f, false, false, Core->CPUFeatures);
  MixBlocks(Buffer[CRASH_BUFFER_MIX], Buffer[CRASH_BUFFER_OSC3], samples, 1.0f, false, false, Core->CPUFeatures);

  for(i = 0; i < 1; i++)
  {
    Buffer[CRASH_BUFFER_VCF1][i] = Buffer[CRASH_BUFFER_MIX][i];
    Buffer[CRASH_BUFFER_VCF2][i] = Buffer[CRASH_BUFFER_MIX][i];
  }

  AD1->Generate(samples, Buffer[CRASH_BUFFER_AD1]);
  VCA->Process(samples, Buffer[CRASH_BUFFER_VCF1], Buffer[CRASH_BUFFER_AD1]);
  Filter1->Process(samples, Buffer[CRASH_BUFFER_VCF1], Buffer[CRASH_BUFFER_AD1], 0); 

  AD2->Generate(samples, Buffer[CRASH_BUFFER_AD2]);
  Filter2->Process(samples, Buffer[CRASH_BUFFER_VCF2], Buffer[CRASH_BUFFER_AD2], 0); 

  MixBlocks(Buffer[CRASH_BUFFER_OUT], Buffer[CRASH_BUFFER_VCF1], samples, 0.5f, true, false, Core->CPUFeatures);
  MixBlocks(Buffer[CRASH_BUFFER_OUT], Buffer[CRASH_BUFFER_VCF2], samples, 1.0f, false, false, Core->CPUFeatures);

  AD3->Generate(samples, Buffer[CRASH_BUFFER_AD3]);
  VCA->Process(samples, Buffer[CRASH_BUFFER_OUT], Buffer[CRASH_BUFFER_AD3]);

  for(i = 0; i < samples; i++)
  {
    v = Buffer[CRASH_BUFFER_OUT][i] * 0.8f;
    outLeft[sample + i]  += v * leftLevel;
    outRight[sample + i] += v * rightLevel;
  }

  if(!AD3->IsTriggered())
   SetIdle(true);
}

// Ride Cymbal

CRideCymbal::CRideCymbal(int sampleRate, int blockSamples)
: CDrumKitVoice(sampleRate, blockSamples, RIDE_BUFFER_COUNT)
{
  AD1 = new CAD(sampleRate, 0.005f, 0.075f, 0.0f, 0.0f);
  AD2 = new CAD(sampleRate, 0.016f, 1.600f, 0.0f, 0.0f);
  AD3 = new CAD(sampleRate, 0.005f, 1.400f, 0.0f, -4.0f);

  
  Filter1 = new CMultiFilter(sampleRate, MULTIFILTER_MODE_BP, MULTIFILTER_POLES_2, FrequencyToVoltage(1050.0f), 0.0f);
  Filter2 = new CMultiFilter(sampleRate, MULTIFILTER_MODE_HP, MULTIFILTER_POLES_2, FrequencyToVoltage(2049.0f), 0.0f);
  VCA = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);
  
  VCO1 = new CVCO(sampleRate, SinTable, 1047.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  VCO2 = new CVCO(sampleRate, SinTable, 1109.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  VCO3 = new CVCO(sampleRate, SinTable, 1175.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  
  VCO4 = new CVCO(sampleRate, SinTable, 1481.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  VCO5 = new CVCO(sampleRate, SinTable, 1049.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 
  VCO6 = new CVCO(sampleRate, SinTable, 1480.0f, Core->DSP->MinBLEP, Core->DSP->Impulse); 

  VCO1->SetShape(VCO_SHAPE_SQUARE);
  VCO2->SetShape(VCO_SHAPE_SQUARE);
  VCO3->SetShape(VCO_SHAPE_SQUARE);
  VCO4->SetShape(VCO_SHAPE_SQUARE);
  VCO5->SetShape(VCO_SHAPE_SQUARE);
  VCO6->SetShape(VCO_SHAPE_SQUARE);
}

CRideCymbal::~CRideCymbal()
{
  delete AD1;
  delete AD2;
  delete AD3;
  delete Filter1;
  delete Filter2;
  delete VCA;
  delete VCO1;
  delete VCO2;
  delete VCO3;
  delete VCO4;
  delete VCO5;
  delete VCO6;
}

void CRideCymbal::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_VOICE_ON:
    case CONTROL_MESSAGE_NOTE_ON:
    {
      AD1->Trigger();
      AD2->Trigger();
      AD3->Trigger();
      SetIdle(false);
      break;
    };
    case CONTROL_MESSAGE_CONTROL_CHANGE:
    {
      double value;

      value = message->ControlChange.Value;

      switch(message->ControlChange.Controller)
      {
        case PROPERTY_RIDE_DECAY:
        {
          AD3->SetDecay(0.2f + (value * 0.5f));
          break;
        };
      }

      break;
    };
  }
}

void CRideCymbal::SetSampleRate(int sampleRate)
{
  AD1->SetSampleRate(sampleRate);
  AD2->SetSampleRate(sampleRate);
  AD3->SetSampleRate(sampleRate);
  Filter1->SetSampleRate(sampleRate);
  Filter2->SetSampleRate(sampleRate);
  VCO1->SetSampleRate(sampleRate);
  VCO2->SetSampleRate(sampleRate);
  VCO3->SetSampleRate(sampleRate);
  VCO4->SetSampleRate(sampleRate);
  VCO5->SetSampleRate(sampleRate);
  VCO6->SetSampleRate(sampleRate);
}

void CRideCymbal::GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel)
{
  double v = 0.0f;
  int i;


  VCO1->GenerateBLEP(0, 0, 0, Buffer[RIDE_BUFFER_OSC1], 0, samples);
  VCO2->GenerateBLEP(0, 0, 0, Buffer[RIDE_BUFFER_OSC2], 0, samples);
  VCO3->GenerateBLEP(0, 0, 0, Buffer[RIDE_BUFFER_OSC3], 0, samples);

  VCO1->GenerateBLEP(0, 0, Buffer[RIDE_BUFFER_OSC1], Buffer[RIDE_BUFFER_OSC1], 0, samples);
  VCO2->GenerateBLEP(0, 0, Buffer[RIDE_BUFFER_OSC2], Buffer[RIDE_BUFFER_OSC2], 0, samples);
  VCO3->GenerateBLEP(0, 0, Buffer[RIDE_BUFFER_OSC3], Buffer[RIDE_BUFFER_OSC3], 0, samples);

  MixBlocks(Buffer[RIDE_BUFFER_MIX], Buffer[RIDE_BUFFER_OSC1], samples, 1.0f, true,  false, Core->CPUFeatures);
  MixBlocks(Buffer[RIDE_BUFFER_MIX], Buffer[RIDE_BUFFER_OSC2], samples, 1.0f, false, false, Core->CPUFeatures);
  MixBlocks(Buffer[RIDE_BUFFER_MIX], Buffer[RIDE_BUFFER_OSC3], samples, 1.0f, false, false, Core->CPUFeatures);

  for(i = 0; i < 1; i++)
  {
    Buffer[RIDE_BUFFER_VCF1][i] = Buffer[RIDE_BUFFER_MIX][i];
    Buffer[RIDE_BUFFER_VCF2][i] = Buffer[RIDE_BUFFER_MIX][i];
  }

  AD1->Generate(samples, Buffer[RIDE_BUFFER_AD1]);
  VCA->Process(samples, Buffer[RIDE_BUFFER_VCF1], Buffer[RIDE_BUFFER_AD1]);
  Filter1->Process(samples, Buffer[RIDE_BUFFER_VCF1], Buffer[RIDE_BUFFER_AD1], 0); 

  AD2->Generate(samples, Buffer[RIDE_BUFFER_AD2]);
  Filter2->Process(samples, Buffer[RIDE_BUFFER_VCF2], Buffer[RIDE_BUFFER_AD2], 0); 

  MixBlocks(Buffer[RIDE_BUFFER_OUT], Buffer[RIDE_BUFFER_VCF1], samples, 0.5f, true, false, Core->CPUFeatures);
  MixBlocks(Buffer[RIDE_BUFFER_OUT], Buffer[RIDE_BUFFER_VCF2], samples, 1.0f, false, false, Core->CPUFeatures);

  AD3->Generate(samples, Buffer[RIDE_BUFFER_AD3]);
  VCA->Process(samples, Buffer[RIDE_BUFFER_OUT], Buffer[RIDE_BUFFER_AD3]);

  for(i = 0; i < samples; i++)
  {
    v = Buffer[RIDE_BUFFER_OUT][i] * 0.9f;
    outLeft[sample + i]  += v * leftLevel;
    outRight[sample + i] += v * rightLevel;
  }

  if(!AD3->IsTriggered())
   SetIdle(true);}

// Drum Kit

bool CDrumKit::GetGUIRect(short *x1, short *y1, short *x2, short *y2)
{
#ifndef DW_HEADLESS
  *x1 = 0;
  *x2 = Info->Background->GetWidth();
  *y1 = 0;
  *y2 = Info->Background->GetHeight();
  return true;
#else
  return false;
#endif
}

bool CDrumKit::IsSynth()
{
  return true;
}

CDrumKit::CDrumKit(CHost *host, CDrumKitInfo *info)
: CMachine(host)
{
  int i;
  GUI = 0;

  SampleRate   = host->GetSampleRate();
  BlockSamples = host->GetBlockSamples();
  NoiseBuffer  = (double*)_mm_malloc(sizeof(double) * BlockSamples, AUDIO_BUFFER_ALIGN);
  
  Info = info;
  Ports = new CPort*[2];
  Ports[0] = new CPort(host, this, PT_INPUT | PT_CONTROL, "Control In");
  Ports[1] = new CPort(host, this, PT_OUTPUT | PT_AUDIO | PT_STEREO,  "Line Out");
  PortCount = 2;

  VoiceCount = VOICE_COUNT;
  Voices     = new CDrumKitVoice*[VoiceCount];
  Voices[0]  = new CBassDrum(SampleRate, BlockSamples);
  Voices[1]  = new CSnareDrum(SampleRate, BlockSamples);
  Voices[2]  = new CTomDrum(SampleRate, BlockSamples, 0);
  Voices[3]  = new CTomDrum(SampleRate, BlockSamples, 1);
  Voices[4]  = new CTomDrum(SampleRate, BlockSamples, 2);
  Voices[5]  = new CRimShot(SampleRate, BlockSamples);
  Voices[6]  = new CClap(SampleRate, BlockSamples);
  Voices[7]  = new CHiHatClosed(SampleRate, BlockSamples);
  Voices[8]  = new CHiHatOpen(SampleRate, BlockSamples);
  Voices[9]  = new CCrashCymbal(SampleRate, BlockSamples);
  Voices[10] = new CRideCymbal(SampleRate, BlockSamples);

  PropertyCount = PROPERTY_COUNT;
  
  Properties = new double[PropertyCount];
  for(i = 0; i < PropertyCount; i++)
    SetProperty(i, DrumKitPropertyDefaults[i]);
}

CDrumKit::~CDrumKit()
{
  int i;

  for(i = 0; i < VoiceCount; i++)
    delete Voices[i];
  delete Voices;

  delete Ports[0];
  delete Ports[1];
  delete Ports;
  _mm_free(NoiseBuffer);
#ifndef DW_HEADLESS
  if(GUI != 0)
    delete GUI;
#endif
}

void CDrumKit::SetBlockSamples(int blockSamples)
{
  int i;

  for(i = 0; i < VoiceCount; i++)
    Voices[i]->SetBlockSamples(blockSamples);
  _mm_free(NoiseBuffer);
  NoiseBuffer = (double*)_mm_malloc(sizeof(double) * blockSamples, AUDIO_BUFFER_ALIGN);
  BlockSamples = blockSamples;
}

void CDrumKit::SetSampleRate(int sampleRate)
{
  int i;

  for(i = 0; i < VoiceCount; i++)
    Voices[i]->SetSampleRate(sampleRate);
  SampleRate = sampleRate;
}

void CDrumKit::Work()
{
  bool            messageRead;
  SControlMessage message;
  int             i;
  double          *leftOut, *rightOut, leftMix, rightMix, level;

  messageRead = Ports[0]->ControlBuffer->Read(&message);
  while(messageRead)
  {
    ProcessMessage(&message);
    messageRead = Ports[0]->ControlBuffer->Read(&message);
  }

  leftOut  = Ports[1]->AudioBuffer->LeftData;
  rightOut = Ports[1]->AudioBuffer->RightData;

  for(i = 0; i < BlockSamples; i++)
  {
    leftOut[i]  = 0.0;
    rightOut[i] = 0.0;
    NoiseBuffer[i] = (((double)rand() / (double)RAND_MAX) - 0.5) * 2.0;
  }

  for(i = 0; i < VoiceCount; i++)
  {
      level     = 2.0 * DrumKitPropertyToPowerRatio(Properties[DrumKitVoiceLevelProperties[i]]);
      rightMix  = Properties[DrumKitVoiceLevelProperties[i] + 1];
      leftMix   = 1.0 - rightMix;
      Voices[i]->Work(leftOut, rightOut, NoiseBuffer, leftMix * level, rightMix * level);
  }
}

int CDrumKit::NoteToVoice(int note)
{
  return note % VOICE_COUNT;
}

void CDrumKit::ProcessMessage(SControlMessage *message)
{
  switch(message->Type)
  {
    case CONTROL_MESSAGE_NOTE_ON:
    {
      Voices[NoteToVoice(message->NoteOn.Note)]->WriteMessage(message);
      break;
    }
    case CONTROL_MESSAGE_VOICE_ON:
    {
      int voice;

      voice = message->VoiceOn.Voice;
      if(voice >= 0 && voice < VoiceCount)
        Voices[voice]->WriteMessage(message);
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

void CDrumKit::GetPropertyName(int index, char *text)
{
  if((index >= 0) && (index < PROPERTY_COUNT))
    strcpy(text, DrumKitPropertyNames[index]);
  else
    *text = 0;
}

void CDrumKit::GetVoiceName(int index, char *text)
{
  if((index >= 0) && (index < VOICE_COUNT))
    strcpy(text, DrumKitVoiceNames[index]);
  else
    *text = 0;
}

void CDrumKit::GetPropertyValueDisplay(int index, double value, char *text)
{
  if((index >= 0) && (index < PROPERTY_COUNT))
    switch(PropertyFormat[index])
    {
      case PROPERTY_FORMAT_PERCENT:
        sprintf(text, "%.0f%%", value * 100.0f);
        break;

      default:
        *text = 0;
        break;
    }
  else
    *text = 0;
}

double CDrumKit::GetProperty(int index)
{
  if((index >= 0) && (index < PROPERTY_COUNT))
    return Properties[index];
  else
    return 0.0f;
}

void CDrumKit::SetProperty(int index, double value)
{
  SControlMessage message;
  int             voice;

  if((index >= 0) && (index < PROPERTY_COUNT))
  {
    Properties[index] = value;
    memset(&message, 0, sizeof(SControlMessage));
    message.Type = CONTROL_MESSAGE_CONTROL_CHANGE;
    message.ControlChange.Controller = index;
    message.ControlChange.Value = value;
    voice = DrumKitPropertyVoices[index];
    if(voice != -1)
      Voices[voice]->WriteMessage(&message);
#ifndef DW_HEADLESS
    if(GUI)
      GUI->SetProperty(index, value);
#endif
  }
}

HWND CDrumKit::CreateGUI(HWND parent)
{
#ifndef DW_HEADLESS
  if(GUI != 0)
  {
    delete GUI;
    GUI = 0;
  }

  if(GUI == 0)
    GUI = new CDrumKitGUI(parent, this, Info);
  
  return GUI->Handle;
#else
  return 0;
#endif
}

CDrumKitInfo::CDrumKitInfo(CHost *host)
{
  Host = host;
  Name = "ES DGenR8";
  UniqueID = "DarkPlug::ES DGenR8";
  Background = new CFrameBuffer(IDR_DGENR8_BACKGROUND);
  Knob = new CFrameBuffer(IDR_DGENR8_KNOB);
}

CDrumKitInfo::~CDrumKitInfo()
{
  delete Background;
  delete Knob;
}

CMachine *CDrumKitInfo::CreateMachine()
{
  return new CDrumKit(Host, this);
}

#ifndef DW_HEADLESS
#define X_S 56

#define X_1 28
#define X_2 (X_1 + X_S)
#define X_3 (X_2 + X_S)
#define X_4 (X_3 + X_S)

#define X_5 276
#define X_6 (X_5 + X_S)
#define X_7 (X_6 + X_S)
#define X_8 (X_7 + X_S)

#define Y_S 88

#define Y_1 64
#define Y_2 (Y_1 + Y_S)
#define Y_3 (Y_2 + Y_S)
#define Y_4 (Y_3 + Y_S)

int KnobXY[PROPERTY_COUNT][2] =
{
  {X_1, Y_1}, // BD Tune
  {X_2, Y_1}, // BD Level
  {0,   0},   // BD Balance
  {X_3, Y_1}, // BD Attack
  {X_4, Y_1}, // BD Decay

  {X_1, Y_2}, // SD Tune
  {X_2, Y_2}, // SD Level
  {0,   0},   // SD Balance
  {X_3, Y_2}, // SD Tone
  {X_4, Y_2}, // SD Snappy

  {X_5, Y_1}, // LT Tune
  {X_6, Y_1}, // LT Level
  {0,   0},   // LT Balance
  {X_7, Y_1}, // LT Decay

  {X_5, Y_2}, // MT Tune
  {X_6, Y_2}, // MT Level
  {0,   0},   // MT Balance
  {X_7, Y_2}, // MT Decay

  {X_5, Y_3}, // HT Tune
  {X_6, Y_3}, // HT Level
  {0,   0},   // HT Balance
  {X_7, Y_3}, // HT Decay

  {X_1, Y_3}, // RS Level
  {0,     0}, // RS Balance
  {X_2, Y_3}, // CP Level
  {0,   0},   // CP Balance
  
  {X_1, Y_4}, // CH Level
  {0,   0},   // CH Balance
  {X_2, Y_4}, // CH Decay
  {X_3, Y_4}, // OH Level
  {0,   0},   // OH Balance
  {X_4, Y_4}, // OH Decay

  {X_5, Y_4}, // CC Level
  {0,   0},   // CC Balance
  {X_6, Y_4}, // CC Tune
  {X_7, Y_4}, // RC Level
  {0,   0},   // RC Balance
  {X_8, Y_4}  // RC Tune
};

CDrumKitGUI::CDrumKitGUI(HWND parent, CDrumKit *drumKit, CDrumKitInfo *info)
: CLiteContainer(parent, info->Background->GetWidth(), info->Background->GetHeight(), info->Background)
{
  int x, y, i;

  Knob = 0;

  DrumKit = drumKit;
  Info = info;

  KnobCount = 0;
  Knob = new CLiteKnob*[PROPERTY_COUNT];

  for(i = 0; i < PROPERTY_COUNT; i++)
  {
    x = KnobXY[i][0];
    y = KnobXY[i][1];
    if(x != 0)
    {
      Knob[i] = new CLiteKnob(this, DrumKit, i, Info->Knob, x, y, 38);
      KnobCount++;
      Knob[i]->Show();
    }
    else
      Knob[i] = 0;
  }

  Render();
  Show();
}

CDrumKitGUI::~CDrumKitGUI()
{
  int i;

  for(i = 0; i < KnobCount; i++)
    if(Knob[i])
      delete Knob[i];
}

void CDrumKitGUI::SetProperty(int index, double value)
{
  if(Knob == 0)
    return;

  if(Knob[index] == 0)
    return;

  Knob[index]->SetValue(value);
}
#endif
