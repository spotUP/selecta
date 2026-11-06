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

#include "BassHead.h"
#include "Core.h"
#include "Util.h"
#include "Resource.h"

#include <stdio.h>
#include <math.h>


#define BRAIN_VOICE_COUNT 16 /* For Polyphonic Version */


#define VCO_WAVE_COUNT 3

char *VCOWaveNames[VCO_WAVE_COUNT] = 
{
  "Saw",
  "Square",
  "TriSquare"
};

#define GLIDE_SCALE          0.4
#define VOLUME_SCALE         48.0
#define OVERDRIVE_SCALE      48.0
#define LFO_VCO_PITCH_SCALE  1.0
#define LFO_VCF_PITCH_SCALE  4.0
#define LFO_PULSEWIDTH_SCALE 1.0
#define LFO_RESONANCE_SCALE  24.0
#define LFO_AMP_SCALE        2.0
#define CUTOFF_SCALE         12.0
#define ENVMOD_SCALE         8.0
#define RESONANCE_SCALE      24.0

#define MIN_LFO_VOLTAGE (-12.0f)
#define MAX_LFO_VOLTAGE (1.0f)

#define BUFFER_LFO1        0
#define BUFFER_LFO2        1
#define BUFFER_LFO3        2
#define BUFFER_FM          3
#define BUFFER_SYNC        4
#define BUFFER_VCO1        5
#define BUFFER_VCO2        6
#define BUFFER_FILTERADSR  7
#define BUFFER_PW          8
#define BUFFER_SLIDE       9

#define BUFFER_FILTER      3
#define BUFFER_FILTER_2    4
#define BUFFER_RESONANCE   8
#define BUFFER_FILTERMOD   5
#define BUFFER_FORMANT     10

#define BUFFER_AMPADSR     8

#define BUFFER_COUNT       11

#define PROPERTY_FORMAT_MS             0
#define PROPERTY_FORMAT_PERCENT        1
#define PROPERTY_FORMAT_SHAPE          2
#define PROPERTY_FORMAT_SEMITONES      3
#define PROPERTY_FORMAT_SEMITONES2     4
#define PROPERTY_FORMAT_BOOLEAN        5
#define PROPERTY_FORMAT_FILTER_MODE    6
#define PROPERTY_FORMAT_VOLUME         7
#define PROPERTY_FORMAT_SATURATION     8 
#define PROPERTY_FORMAT_LFORATE        9 
#define PROPERTY_FORMAT_LFOSHAPE       10 
#define PROPERTY_FORMAT_FILTER_ROUTING 11
#define PROPERTY_FORMAT_OVERDRIVE      12
#define PROPERTY_FORMAT_BIPOLAR        13
#define PROPERTY_FORMAT_FADEIN         14
#define PROPERTY_FORMAT_FORMANT_VOWEL  15
#define PROPERTY_FORMAT_SEND           16

#define PROPERTY_FORMAT_COUNT          17

#define PROPERTY_AMP_ATTACK               0
#define PROPERTY_AMP_DECAY                1 
#define PROPERTY_AMP_SUSTAIN              2
#define PROPERTY_AMP_RELEASE              3
#define PROPERTY_AMP_ATTACK_CURVE         4
#define PROPERTY_AMP_DECAY_CURVE          5
#define PROPERTY_AMP_RELEASE_CURVE        6

#define PROPERTY_FILTER_ATTACK            7
#define PROPERTY_FILTER_DECAY             8
#define PROPERTY_FILTER_SUSTAIN           9 
#define PROPERTY_FILTER_RELEASE           10
#define PROPERTY_FILTER_ATTACK_CURVE      11
#define PROPERTY_FILTER_DECAY_CURVE       12
#define PROPERTY_FILTER_RELEASE_CURVE     13

#define PROPERTY_VCO1_SHAPE               14
#define PROPERTY_VCO1_WAVE_PW             15
#define PROPERTY_VCO1_SEMITONE            16
#define PROPERTY_VCO1_ENVMOD              17

#define PROPERTY_VCO2_SHAPE               18
#define PROPERTY_VCO2_WAVE_PW             19
#define PROPERTY_VCO2_SEMITONE            20
#define PROPERTY_VCO2_DETUNE              21
#define PROPERTY_VCO2_ENVMOD              22
#define PROPERTY_SYNC                     23

#define PROPERTY_VCF1_MODE                24
#define PROPERTY_VCF1_CUTOFF              25
#define PROPERTY_VCF1_RESONANCE           26
#define PROPERTY_VCF1_KEYFOLLOW           27
#define PROPERTY_VCF1_ENVMOD              28

#define PROPERTY_VCF2_MODE                29
#define PROPERTY_VCF2_CUTOFF              30
#define PROPERTY_VCF2_RESONANCE           31
#define PROPERTY_VCF2_KEYFOLLOW           32
#define PROPERTY_VCF2_ENVMOD              33

#define PROPERTY_FILTER_ROUTING           34
#define PROPERTY_FORMANT_MIX              35
#define PROPERTY_FORMANT_VOWEL            36
#define PROPERTY_OSC_BAL                  37
#define PROPERTY_OSC_VOL                  38
#define PROPERTY_NOISE                    39
#define PROPERTY_SATURATION               40
#define PROPERTY_OVERDRIVE                41
#define PROPERTY_RING_MOD                 42
#define PROPERTY_PORTAMENTO               43
#define PROPERTY_GLIDE                    44
#define PROPERTY_SLIDE_CURVE              45

#define PROPERTY_LFO1_RATE                46
#define PROPERTY_LFO1_SHAPE               47
#define PROPERTY_LFO1_FADE_IN             48 
#define PROPERTY_LFO1_SEND1               49  
#define PROPERTY_LFO1_AMP1                50
#define PROPERTY_LFO1_SEND2               51
#define PROPERTY_LFO1_AMP2                52
#define PROPERTY_LFO1_SEND3               53         
#define PROPERTY_LFO1_AMP3                54

#define PROPERTY_LFO2_RATE                55
#define PROPERTY_LFO2_SHAPE               56
#define PROPERTY_LFO2_FADE_IN             57
#define PROPERTY_LFO2_SEND1               58
#define PROPERTY_LFO2_AMP1                59
#define PROPERTY_LFO2_SEND2               60
#define PROPERTY_LFO2_AMP2                61
#define PROPERTY_LFO2_SEND3               62           
#define PROPERTY_LFO2_AMP3                63

#define PROPERTY_LFO3_RATE                64
#define PROPERTY_LFO3_SHAPE               65
#define PROPERTY_LFO3_FADE_IN             66
#define PROPERTY_LFO3_SEND1               67
#define PROPERTY_LFO3_AMP1                68
#define PROPERTY_LFO3_SEND2               69
#define PROPERTY_LFO3_AMP2                70
#define PROPERTY_LFO3_SEND3               71         
#define PROPERTY_LFO3_AMP3                72

#define PROPERTY_COUNT                    73

#define PROPERTY_SEND_VCO1_AMP            1
#define PROPERTY_SEND_VCO2_AMP            2
#define PROPERTY_SEND_VCO1_PITCH          3 
#define PROPERTY_SEND_VCO2_PITCH          4
#define PROPERTY_SEND_VCO1_PULSEWIDTH     5
#define PROPERTY_SEND_VCO2_PULSEWIDTH     6
#define PROPERTY_SEND_VCF1_PITCH          7
#define PROPERTY_SEND_VCF2_PITCH          8
#define PROPERTY_SEND_VCF1_RESONANCE      9
#define PROPERTY_SEND_VCF2_RESONANCE      10

#define PROPERTY_SEND_COUNT               11

const char* PropertySendNames[PROPERTY_SEND_COUNT] =
{
  "None",
  "VCO1 Amp",
  "VCO2 Amp",
  "VCO1 Pitch",
  "VCO2 Pitch",
  "VCO1 Pulse Width",
  "VCO2 Pulse Width",
  "VCF1 Pitch",
  "VCF2 Pitch",
  "VCF1 Resonance",
  "VCF2 Resonance"
};

inline int PropertyToSend(double value)
{
  return (int)(value * (PROPERTY_SEND_COUNT - 1));
}

const char* BassHeadPropertyNames[PROPERTY_COUNT] = {
  "Amp Attack",
  "Amp Decay",
  "Amp Sustain",
  "Amp Release",
  "Amp Attack Curve",
  "Amp Decay Curve",
  "Amp Release Curve",
  "Filter Attack",
  "Filter Decay",
  "Filter Sustain",
  "Filter Release",
  "Filter Attack Curve",
  "Filter Decay Curve",
  "Filter Release Curve",
  "VCO1 Shape",
  "VCO1 Pulse Width",
  "VCO1 Semitone",
  "VCO1 EnvMod",
  "VCO2 Shape",
  "VCO2 Pulse Width",
  "VCO2 Semitone",
  "VCO2 Detune",
  "VCO2 EnvMod",
  "Sync",
  "VCF1 Mode",
  "VCF1 Cutoff",
  "VCF1 Resonance",
  "VCF1 Key Follow",
  "VCF1 Env Mod",
  "VCF2 Mode",
  "VCF2 Cutoff",
  "VCF2 Resonance",
  "VCF2 Key Follow",
  "VCF2 Env Mod",
  "Filter Routing",
  "Formant Mix",
  "Formant Vowel",
  "Osc Balance",
  "Osc Volume",
  "Noise",
  "Saturation",
  "Overdrive",
  "Ring Mod",
  "Portamento",
  "Glide",
  "Slide Curve",
  "LFO1 Rate",
  "LFO1 Shape",
  "LFO1 Fade In",
  "LFO1 Send 1",
  "LFO1 Amp 1",
  "LFO1 Send 2",
  "LFO1 Amp 2",
  "LFO1 Send 3",
  "LFO1 Amp 3",
  "LFO2 Rate",
  "LFO2 Shape",
  "LFO2 Fade In",
  "LFO2 Send 1",
  "LFO2 Amp 1",
  "LFO2 Send 2",
  "LFO2 Amp 2",
  "LFO2 Send 3",
  "LFO2 Amp 3",
  "LFO3 Rate",
  "LFO3 Shape",
  "LFO3 Fade In",
  "LFO3 Send 1",
  "LFO3 Amp 1",
  "LFO3 Send 2",
  "LFO3 Amp 2",
  "LFO3 Send 3",
  "LFO3 Amp 3",
};
 
const double PropertyDefaults[PROPERTY_COUNT] = {
  0.2, // Amp Attack 
  0.5, // Amp Decay
  0.5, // Amp Sustain
  0.5, // Amp Release
  0.5, // Amp Attack Curve
  0.5, // Amp Decay Curve
  0.5, // Amp Release Curve
  0.2, // Filter Attack
  0.5, // Filter Decay
  1.0, // Filter Sustain
  0.5, // Filter Release
  0.5, // Filter Attack Curve
  0.5, // Filter Decay Curve
  0.5, // Filter Release Curve
  0.4, // VCO1 Shape
  0.5, // VCO1 Wave/PW
  0.5, // VCO1 Semitone
  0.0, // VCO1 EnvMod
  0.4, // VCO2 Shape
  0.5, // VCO2 Wave/PW
  0.5, // VCO2 Semitone
  0.1, // VCO2 Detune
  0.0, // VCO2 EnvMod
  0.0, // Sync
  0.0, // VCF1 Mode
  0.15,// VCF1 Cutoff
  0.5, // VCF1 Resonance
  1.0, // VCF1 Key Follow
  0.5, // VCF1 Env Mod
  0.0, // VCF2 Mode
  0.1, // VCF2 Cutoff
  0.2, // VCF2 Resonance
  1.0, // VCF2 Key Follow
  0.5, // VCF2 Env Mod
  0.0, // Filter Routing
  0.0, // Formant Mix
  0.0, // Formant Vowel
  0.5, // Osc Balance
  1.0, // Osc Volume
  0.0, // Noise
  0.0, // Saturation
  0.0, // Overdrive
  0.0, // Ring Mod
  0.0, // Portamento
  0.0, // Glide
  0.5, // Slide Curve
  0.5, // LFO1 Rate
  0.0, // LFO1 Shape
  0.0, // LFO1 Fade In
  0.0, // LFO1 Send1
  1.0, // LFO1 Amp 1
  0.0, // LFO1 Send2
  1.0, // LFO1 Amp 2
  0.0, // LFO1 Send3
  1.0, // LFO1 Amp 3
  0.5, // LFO2 Rate
  0.0, // LFO2 Shape
  0.0, // LFO2 Fade In
  0.0, // LFO2 Send1
  1.0, // LFO2 Amp 1
  0.0, // LFO2 Send2
  1.0, // LFO2 Amp 2
  0.0, // LFO2 Send3
  1.0, // LFO2 Amp 3
  0.5, // LFO3 Rate
  0.0, // LFO3 Shape
  0.0, // LFO3 Fade In
  0.0, // LFO3 Send1
  1.0, // LFO3 Amp 1
  0.0, // LFO3 Send2
  1.0, // LFO3 Amp 2
  0.0, // LFO3 Send3
  1.0  // LFO3 Amp 3
};

const int PropertyFormat[PROPERTY_COUNT] = {
  PROPERTY_FORMAT_MS,
  PROPERTY_FORMAT_MS,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_MS,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_MS,
  PROPERTY_FORMAT_MS,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_MS,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SHAPE,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SEMITONES,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SHAPE,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SEMITONES,
  PROPERTY_FORMAT_SEMITONES2,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_BOOLEAN,
  PROPERTY_FORMAT_FILTER_MODE,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_FILTER_MODE,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_FILTER_ROUTING,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_FORMANT_VOWEL,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_VOLUME,
  PROPERTY_FORMAT_VOLUME,
  PROPERTY_FORMAT_SATURATION,
  PROPERTY_FORMAT_OVERDRIVE,
  PROPERTY_FORMAT_VOLUME,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_LFORATE,
  PROPERTY_FORMAT_LFOSHAPE,
  PROPERTY_FORMAT_FADEIN,
  PROPERTY_FORMAT_SEND,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SEND,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SEND,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_LFORATE,
  PROPERTY_FORMAT_LFOSHAPE,
  PROPERTY_FORMAT_FADEIN,
  PROPERTY_FORMAT_SEND,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SEND,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SEND,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_LFORATE,
  PROPERTY_FORMAT_LFOSHAPE,
  PROPERTY_FORMAT_FADEIN,
  PROPERTY_FORMAT_SEND,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SEND,
  PROPERTY_FORMAT_PERCENT,
  PROPERTY_FORMAT_SEND
};

#define FILTER_MODE_COUNT 2

char *FilterModeText[FILTER_MODE_COUNT] = 
{
  "Low Pass",
  "High Pass"
};

#define FILTER_ROUTING_COUNT 3

char *FilterRoutingText[FILTER_ROUTING_COUNT] =
{
  "Serial",
  "Parallel",
  "Split"
};

#define FILTER_ROUTING_SERIAL   0
#define FILTER_ROUTING_PARALELL 1
#define FILTER_ROUTING_SPLIT    2

#define SATURATION_MODE_NONE     0
#define SATURATION_MODE_SOFTCLIP 1
#define SATURATION_MODE_DIGITAL  2
#define SATURATION_MODE_DISTORT  3

#define SATURATION_MODE_COUNT    4

char *SaturationModeText[SATURATION_MODE_COUNT] =
{
  "None",
  "Soft Clip",
  "Digital",
  "Distort"
};

#define LFO_SHAPE_SINE     0
#define LFO_SHAPE_TRIANGLE 1
#define LFO_SHAPE_SAW      2
#define LFO_SHAPE_PULSE    3
#define LFO_SHAPE_SH       4
#define LFO_SHAPE_SG       5

#define LFO_SHAPE_COUNT    6

char *LFOShapeText[LFO_SHAPE_COUNT] =
{
  "Sine",
  "Triangle",
  "Saw",
  "Pulse",
  "S&H",
  "S&G"
};

double PropertyToTime(double value)
{
  value = (value * value * value) * 8.0f;
  if(value < 0.01f)
    value = 0.01f;
  return value;
}

double PropertyToFade(double value)
{
  value = (value * value * value) * 8.0f;
  return value;
}

double PropertyToBipolar(double value, double scale)
{
  double bipolar;

  bipolar = (value - 0.5f);
  bipolar = (bipolar * bipolar * scale * 2.0f);
  if(value < 0.5f)
    bipolar = -bipolar;
  return bipolar;
}

CBassHeadVoice::CBassHeadVoice(int sampleRate, int blockSamples)
{
  int i;

  SampleRate = sampleRate;
  BlockSamples = blockSamples;
  Log440  = log(440.0); 
  InvLog2 = 1.0 / log(2.0);

  Buffer = new double*[BUFFER_COUNT];
  for(i = 0; i < BUFFER_COUNT; i++)
    Buffer[i] = (double*)_mm_malloc(sizeof(double) * blockSamples, AUDIO_BUFFER_ALIGN);

  ControlBuffer = new CControlBuffer();

  Slide = new CSlide(SampleRate, 0.0f, 0.0f, 0.0f);

  VCA = new CVCA(VCA_MODE_EXPONENTIAL, 1.0f);

  VCO1 = new CVCO(sampleRate, SinTable, 100.0f, Core->DSP->MinBLEP, Core->DSP->Impulse);
  VCO2 = new CVCO(sampleRate, SinTable, 100.0f, Core->DSP->MinBLEP, Core->DSP->Impulse);

  VCO1->SetVoltage(0.0f);
  VCO2->SetVoltage(0.0f);

  AmpADSR = new CADSR(sampleRate, 0.1f, 0.1f, 0.1f, 0.1f);

  VCF1 = new CMultiFilter(sampleRate, MULTIFILTER_MODE_LP, MULTIFILTER_POLES_2, 0.0f, 0.0f);
  VCF2 = new CMultiFilter(sampleRate, MULTIFILTER_MODE_LP, MULTIFILTER_POLES_2, 0.0f, 0.0f);

  FormantFilter = new CFormantFilter(sampleRate, blockSamples, 0);

  FilterADSR = new CADSR(sampleRate, 0.1f, 0.1f, 0.1f, 0.1f);

  Note = -1;

  for(i = 0; i < 3; i++)
    LFO[i] = new CVCO(sampleRate, SinTable, 10.0f, 0, 0);
  
  VCO1Shape = 0.0f;
  VCO1WavePW = 0.0f;
  VCO1Semitone = 0.5f;

  VCO2Shape = 0.0f;
  VCO2WavePW = 0.0f;
  VCO2Semitone = 0.5f;
  VCO2Detune = 0.0f;

  VCF1Cutoff = 0.0f;
  VCF1KeyFollow = 1.0f;
  VCF1EnvMod = 0.5f;

  VCF2Cutoff = 0.0f;
  VCF2KeyFollow = 1.0f;
  VCF2EnvMod = 0.5f;

  Sync = 0.0f;
  RingMod = 0.0f;
  
  VCO1EnvMod = 0.0f;
  VCO2EnvMod = 0.0f;

  Frequency = 440.0f;

  FilterRouting = 0;
  OscVol = 1.0f;
  OscBalance = 0.5f;
  SaturationMode = SATURATION_MODE_NONE;
  Overdrive = 1.0f;

  Velocity = 0.0f;

  for(i = 0; i < 3; i++)
  LFOVCO1Amp[i] = LFOVCO1Pitch[i] = LFOVCO1PulseWidth[i] = 
    LFOVCO2Amp[i] = LFOVCO2Pitch[i] = LFOVCO2PulseWidth[i] =
    LFOVCF1Pitch[i] = LFOVCF1Resonance[i] =
    LFOVCF2Pitch[i] = LFOVCF2Resonance[i] = 0.0f;

  SControlMessage message;

  for(i = 0; i < PROPERTY_COUNT; i++)
  {
	  memset(&message, 0, sizeof(SControlMessage));
	  message.Size = sizeof(SControlMessage);
	  message.Type = CONTROL_MESSAGE_CONTROL_CHANGE;
	  message.ControlChange.Controller = i;
	  message.ControlChange.Value = PropertyDefaults[i];
	  ProcessMessage(&message);
  }
}

CBassHeadVoice::~CBassHeadVoice()
{
  int i;

  delete Slide;
  delete ControlBuffer;
  delete VCA;
  delete VCO1;
  delete VCO2;
  delete AmpADSR;
  delete VCF1;
  delete VCF2;
  delete FormantFilter;
  delete FilterADSR;
  
  for(i = 0; i < 3; i++)
    delete LFO[i];

  for(i = 0; i < BUFFER_COUNT; i++)
    _mm_free(Buffer[i]);
  delete Buffer;
}

void CBassHeadVoice::SetBlockSamples(int blockSamples)
{
  int i;

  for(i = 0; i < BUFFER_COUNT; i++)
  {
    _mm_free(Buffer[i]);
    Buffer[i] = (double*)_mm_malloc(sizeof(double) * blockSamples, AUDIO_BUFFER_ALIGN);
  }
  FormantFilter->SetBlockSamples(blockSamples);
  BlockSamples = blockSamples;
}

void CBassHeadVoice::SetSampleRate(int sampleRate)
{
  Slide->SetSampleRate(sampleRate);
  VCO1->SetSampleRate(sampleRate);
  VCO2->SetSampleRate(sampleRate);
  LFO[0]->SetSampleRate(sampleRate);
  LFO[1]->SetSampleRate(sampleRate);
  LFO[2]->SetSampleRate(sampleRate);
  VCF1->SetSampleRate(sampleRate);
  VCF2->SetSampleRate(sampleRate);
  AmpADSR->SetSampleRate(sampleRate);
  FilterADSR->SetSampleRate(sampleRate);
  FormantFilter->SetSampleRate(sampleRate);
  SampleRate = sampleRate;
}

void CBassHeadVoice::Work(double *leftOut, double *rightOut, double mix)
{
  SControlMessage message;
  bool            gotMessage;
  int             firstSample, lastSample;

  gotMessage = ControlBuffer->Read(&message);
  if(!gotMessage) // no messages, just generate signal
    GenerateBlock(0, BlockSamples, leftOut, rightOut, mix);
  else // got one or more messages
  {
    if(message.BlockSample > 0) // process data before first block
      GenerateBlock(0, message.BlockSample, leftOut, rightOut, mix);

    while(gotMessage) // process message and data until no more messages
    {
      ProcessMessage(&message);
      firstSample = message.BlockSample;
      gotMessage = ControlBuffer->Read(&message);
      if(gotMessage)
        lastSample = message.BlockSample - 1;
      else
        lastSample = BlockSamples - 1;
      if(lastSample >= firstSample)
        GenerateBlock(firstSample, lastSample - firstSample + 1, leftOut, rightOut, mix);
    }
  }
}

void CBassHeadVoice::SetVCOFrequencies()
{
  VCO1->SetVoltage(0.0f);
  VCO2->SetVoltage(0.0f);
  VCO1->SetFrequency(Frequency, VCO1Semitone);
  VCO2->SetFrequency(Frequency, VCO2Semitone, VCO2Detune);
}

void CBassHeadVoice::SetVCFFrequencies()
{
  VCF1->SetVoltage(VCFVoltage(VCF1Cutoff, VCF1KeyFollow));
  VCF2->SetVoltage(VCFVoltage(VCF2Cutoff, VCF2KeyFollow));
}

double CBassHeadVoice::VCFVoltage(double cutoff, double keyFollow)
{
  return (CUTOFF_SCALE * cutoff); // + (FrequencyToVoltage(Frequency) * (2.0f * (keyFollow - 0.5f)));
}

void CBassHeadVoice::ProcessMessage(SControlMessage *message)
{
  int i;

  switch(message->Type)
  {
    case CONTROL_MESSAGE_NOTE_ON:
    {
      Note      = message->NoteOn.Note;
      Frequency = message->NoteOn.Frequency;
      Velocity  = message->NoteOn.Velocity;
      SetVCOFrequencies();
      SetVCFFrequencies();
      Slide->Trigger(message->NoteOn.Note);
      AmpADSR->TriggerAttack();
      FilterADSR->TriggerAttack();
      for(i = 0; i < 3; i++)
      {
        if(LFOFadeIn[i] > 0.0f)
          LFOFade[i] = 0.0f;
        else
          LFOFade[i] = 1.0f;
      }
      break;
    }

    case CONTROL_MESSAGE_NOTE_OFF:
    {
      AmpADSR->TriggerRelease();
      FilterADSR->TriggerRelease();
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
          AmpADSR->SetAttack(PropertyToTime(value));
          break;
        }
        case PROPERTY_AMP_DECAY:
        {
          AmpADSR->SetDecay(PropertyToTime(value));
          break;
        }
        case PROPERTY_AMP_SUSTAIN:
        {
          AmpADSR->SetSustain(value);
          break;
        }
        case PROPERTY_AMP_RELEASE:
        {
          AmpADSR->SetRelease(PropertyToTime(value));
          break;
        }
        case PROPERTY_FILTER_ATTACK:
        {
          FilterADSR->SetAttack(PropertyToTime(value));
          break;
        }
        case PROPERTY_FILTER_DECAY:
        {
          FilterADSR->SetDecay(PropertyToTime(value));
          break;
        }
        case PROPERTY_FILTER_SUSTAIN:
        {
          FilterADSR->SetSustain(value);
          break;
        }
        case PROPERTY_FILTER_RELEASE:
        {
          FilterADSR->SetRelease(PropertyToTime(value));
          break;
        }
        case PROPERTY_AMP_ATTACK_CURVE:
        {
          AmpADSR->SetAttackCurve((value - 0.5f) * 16.0f);
          break;
        }
        case PROPERTY_AMP_DECAY_CURVE:
        {
          AmpADSR->SetDecayCurve((value - 0.5f) * 16.0f);
          break;
        }

        case PROPERTY_AMP_RELEASE_CURVE:
        {
          AmpADSR->SetReleaseCurve((value - 0.5f) * 16.0f);
          break;
        }
        case PROPERTY_FILTER_ATTACK_CURVE:
        {
          FilterADSR->SetAttackCurve((value - 0.5f) * 16.0f);
          break;
        }
        case PROPERTY_FILTER_DECAY_CURVE:
        {
          FilterADSR->SetDecayCurve((value - 0.5f) * 16.0f);
          break;
        }
        case PROPERTY_FILTER_RELEASE_CURVE:
        {
          FilterADSR->SetReleaseCurve((value - 0.5f) * 16.0f);
          break;
        }
        case PROPERTY_VCO1_SHAPE:
        {
          VCO1->SetShape(VCOShape(value));
          break;
        }
        case PROPERTY_VCO1_WAVE_PW:
        {
          VCO1WavePW = value;
          break;
        }
        case PROPERTY_VCO1_SEMITONE:
        {
          VCO1Semitone = value;
          SetVCOFrequencies();
          break;
        }
        case PROPERTY_VCO1_ENVMOD:
        {
          VCO1EnvMod = value;
          break;
        }
        case PROPERTY_VCO2_SHAPE:
        {
          VCO2->SetShape(VCOShape(value));
          break;
        }
        case PROPERTY_VCO2_WAVE_PW:
        {
          VCO2WavePW = value;
          break;
        }
        case PROPERTY_VCO2_SEMITONE:
        {
          VCO2Semitone = value;
          SetVCOFrequencies();
          break;
        }
        case PROPERTY_VCO2_DETUNE:
        {
          VCO2Detune = value;
          SetVCOFrequencies();
          break;
        }
        case PROPERTY_VCO2_ENVMOD:
        {
          VCO2EnvMod = value;
          break;
        }
        case PROPERTY_SYNC:
        {
          Sync = value;
          break;
        }
        case PROPERTY_VCF1_MODE:
        {
          VCF1->SetMode(PropertyToInt(value, FILTER_MODE_COUNT));
          break;
        }
        case PROPERTY_VCF1_CUTOFF:
        {
          VCF1Cutoff = value;
          SetVCFFrequencies();
          break;
        }
        case PROPERTY_VCF1_RESONANCE:
        {
          VCF1->SetResonancedB(value * RESONANCE_SCALE);
          break;
        }
        case PROPERTY_VCF1_KEYFOLLOW:
        {
          VCF1KeyFollow = value;
          SetVCFFrequencies();
          break;
        }
        case PROPERTY_VCF1_ENVMOD:
        {
          VCF1EnvMod = value;
          break;
        }
        case PROPERTY_VCF2_MODE:
        {
          VCF2->SetMode(PropertyToInt(value, FILTER_MODE_COUNT));
          break;
        }
        case PROPERTY_VCF2_CUTOFF:
        {
          VCF2Cutoff = value;
          SetVCFFrequencies();
          break;
        }
        case PROPERTY_VCF2_RESONANCE:
        {
          VCF2->SetResonancedB(value * RESONANCE_SCALE);
          break;
        }
        case PROPERTY_VCF2_KEYFOLLOW:
        {
          VCF2KeyFollow = value;
          SetVCFFrequencies();
          break;
        }
        case PROPERTY_VCF2_ENVMOD:
        {
          VCF2EnvMod = value;
          break;
        }
        case PROPERTY_FILTER_ROUTING:
        {
          FilterRouting = PropertyToInt(value, FILTER_ROUTING_COUNT);
          break;
        }
        case PROPERTY_FORMANT_MIX:
        {
          FormantMix = value;
          break;
        }
        case PROPERTY_FORMANT_VOWEL:
        {
          FormantFilter->SetVowel(PropertyToInt(value, VOWEL_COUNT));
          break;
        }
        case PROPERTY_OSC_BAL:
        {
          OscBalance = value;
          break;
        }
        case PROPERTY_OSC_VOL:
        {
          if(value == 0.0f)
            OscVol = 0.0f;
          else
            OscVol = dBToPowerRatio((1.0f - value) * -VOLUME_SCALE);
          break;
        }
        case PROPERTY_OVERDRIVE:
        {
          if(value == 0.0f)
            Overdrive = 1.0f;
          else
            Overdrive = dBToPowerRatio(value * OVERDRIVE_SCALE);
          break;
        }
        case PROPERTY_NOISE:
        {
          if(value == 0.0f)
            Noise = 0.0f;
          else
            Noise = dBToPowerRatio((1.0f - value) * -VOLUME_SCALE);
          break;
        }
        case PROPERTY_SATURATION:
        {
          SaturationMode = PropertyToInt(value, SATURATION_MODE_COUNT);
          break;
        }
        case PROPERTY_RING_MOD:
        {
          if(value == 0.0f)
            RingMod = 0.0f;
          else
            RingMod = dBToPowerRatio((1.0f - value) * -VOLUME_SCALE);
          break;
        }

        // LFO 1
        
        case PROPERTY_LFO1_RATE:
        {
          LFO[0]->SetVoltage(MIN_LFO_VOLTAGE + ((MAX_LFO_VOLTAGE - MIN_LFO_VOLTAGE) * value));
          break;
        }
        case PROPERTY_LFO1_SHAPE:
        {
          LFOShapes[0] = LFOShape(value);
          SetLFOWave(LFOShapes[0], LFO[0]);
          break;
        }
        case PROPERTY_LFO1_FADE_IN:
        {
          LFOFadeIn[0] = PropertyToFade(value);
          break;
        }
        case PROPERTY_LFO1_AMP1:
        {
          LFOAmp[0][0] = value;
          break;
        }
        case PROPERTY_LFO1_AMP2:
        {
          LFOAmp[0][1] = value;
          break;
        }
        case PROPERTY_LFO1_AMP3:
        {
          LFOAmp[0][2] = value;
          break;
        }
        case PROPERTY_LFO1_SEND1:
        {
          LFOSend[0][0] = PropertyToSend(value);
          break;
        }
        case PROPERTY_LFO1_SEND2:
        {
          LFOSend[0][1] = PropertyToSend(value);
          break;
        }
        case PROPERTY_LFO1_SEND3:
        {
          LFOSend[0][2] = PropertyToSend(value);
          break;
        }

        // LFO 2
        
        case PROPERTY_LFO2_RATE:
        {
          LFO[1]->SetVoltage(MIN_LFO_VOLTAGE + ((MAX_LFO_VOLTAGE - MIN_LFO_VOLTAGE) * value));
          break;
        }
        case PROPERTY_LFO2_SHAPE:
        {
          LFOShapes[1] = LFOShape(value);
          SetLFOWave(LFOShapes[1], LFO[1]);
          break;
        }
        case PROPERTY_LFO2_FADE_IN:
        {
          LFOFadeIn[1] = PropertyToFade(value);
          break;
        }
        case PROPERTY_LFO2_AMP1:
        {
          LFOAmp[1][0] = value;
          break;
        }
        case PROPERTY_LFO2_AMP2:
        {
          LFOAmp[1][1] = value;
          break;
        }
        case PROPERTY_LFO2_AMP3:
        {
          LFOAmp[1][2] = value;
          break;
        }
        case PROPERTY_LFO2_SEND1:
        {
          LFOSend[1][0] = PropertyToSend(value);
          break;
        }
        case PROPERTY_LFO2_SEND2:
        {
          LFOSend[1][1] = PropertyToSend(value);
          break;
        }
        case PROPERTY_LFO2_SEND3:
        {
          LFOSend[1][2] = PropertyToSend(value);
          break;
        }
        
        // LFO 3
        
        case PROPERTY_LFO3_RATE:
        {
          LFO[2]->SetVoltage(MIN_LFO_VOLTAGE + ((MAX_LFO_VOLTAGE - MIN_LFO_VOLTAGE) * value));
          break;
        }
        case PROPERTY_LFO3_SHAPE:
        {
          LFOShapes[2] = LFOShape(value);
          SetLFOWave(LFOShapes[2], LFO[2]);
          break;
        }
        case PROPERTY_LFO3_FADE_IN:
        {
          LFOFadeIn[2] = PropertyToFade(value);
          break;
        }
        case PROPERTY_LFO3_AMP1:
        {
          LFOAmp[2][0] = value;
          break;
        }
        case PROPERTY_LFO3_AMP2:
        {
          LFOAmp[2][1] = value;
          break;
        }
        case PROPERTY_LFO3_AMP3:
        {
          LFOAmp[2][2] = value;
          break;
        }
        case PROPERTY_LFO3_SEND1:
        {
          LFOSend[2][0] = PropertyToSend(value);
          break;
        }
        case PROPERTY_LFO3_SEND2:
        {
          LFOSend[2][1] = PropertyToSend(value);
          break;
        }
        case PROPERTY_LFO3_SEND3:
        {
          LFOSend[2][2] = PropertyToSend(value);
          break;
        }
        
        case PROPERTY_PORTAMENTO:
        {
          Slide->SetPortamento(value * value * GLIDE_SCALE);
          break;
        }
        case PROPERTY_GLIDE:
        {
          Slide->SetGlide(value * value * GLIDE_SCALE);
          break;
        }
        case PROPERTY_SLIDE_CURVE:
        {
          Slide->SetCurve((value - 0.5f) * 16.0f);
          break;
        }
      }
      break;
    }
  }
}

void CBassHeadVoice::SetLFOWave(int shape, CVCO *lfo)
{
  switch(shape)
  {
    case LFO_SHAPE_SINE:
    case LFO_SHAPE_SH:
    case LFO_SHAPE_SG:
      lfo->SetWave(SinTable);
      break;
    case LFO_SHAPE_TRIANGLE:
      lfo->SetWave(Core->DSP->TriangleWave);
      break;
    case LFO_SHAPE_SAW:
      lfo->SetWave(Core->DSP->SawWave);
      break;
    case LFO_SHAPE_PULSE:
      lfo->SetWave(Core->DSP->SquareWave);
      break;
  }
}

int CBassHeadVoice::LFOShape(double value)
{
  return PropertyToInt(value, LFO_SHAPE_COUNT);
}

void CBassHeadVoice::GenerateLFO(CVCO *lfo, int buffer, int samples, int shape)
{
  if(shape == LFO_SHAPE_SH)
    lfo->GenerateSH(Buffer[buffer], samples);
  else if(shape == LFO_SHAPE_SG)
    lfo->GenerateSG(Buffer[buffer], samples);
  else
    lfo->Generate(Buffer[buffer], samples);
}

double CalcSendInner(double value, int sendProperty, int send)
{
  if(sendProperty == send)
    return value;
  else
    return 0.0f;
}

void CalcSend(double lfoAmp[3][3], int lfoSend[3][3], double send[3], int sendDestination)
{
  int i;

  for(i = 0; i < 3; i++)
  {
    send[i]  = CalcSendInner(lfoAmp[i][0], lfoSend[i][0], sendDestination);
    send[i] += CalcSendInner(lfoAmp[i][1], lfoSend[i][1], sendDestination);
    send[i] += CalcSendInner(lfoAmp[i][2], lfoSend[i][2], sendDestination);
  }
}

void CBassHeadVoice::GenerateBlock(int sample, int samples, double *leftOut, double *rightOut, double mix)
{
  int    i, j;
  double  v, *syncBuffer, oscVol, overdrive, balance, mixLevel, fadeRate;
 
  CalcSend(LFOAmp, LFOSend, LFOVCO1Amp,        PROPERTY_SEND_VCO1_AMP);
  CalcSend(LFOAmp, LFOSend, LFOVCO2Amp,        PROPERTY_SEND_VCO2_AMP);
  CalcSend(LFOAmp, LFOSend, LFOVCO1Pitch,      PROPERTY_SEND_VCO1_PITCH);
  CalcSend(LFOAmp, LFOSend, LFOVCO2Pitch,      PROPERTY_SEND_VCO2_PITCH);
  CalcSend(LFOAmp, LFOSend, LFOVCO1PulseWidth, PROPERTY_SEND_VCO1_PULSEWIDTH);
  CalcSend(LFOAmp, LFOSend, LFOVCO2PulseWidth, PROPERTY_SEND_VCO2_PULSEWIDTH);
  CalcSend(LFOAmp, LFOSend, LFOVCF1Pitch,      PROPERTY_SEND_VCF1_PITCH);
  CalcSend(LFOAmp, LFOSend, LFOVCF2Pitch,      PROPERTY_SEND_VCF2_PITCH);
  CalcSend(LFOAmp, LFOSend, LFOVCF1Resonance,  PROPERTY_SEND_VCF1_RESONANCE);
  CalcSend(LFOAmp, LFOSend, LFOVCF2Resonance,  PROPERTY_SEND_VCF2_RESONANCE);
    
  if(samples == 0)
    return;

  if(FilterADSR->IsTriggered())
  {
    // Generate Filter Envelope
    FilterADSR->Generate(samples, Buffer[BUFFER_FILTERADSR]);
  }

  // LFO Stage
  for(i = 0; i < 3; i++)
  {
    GenerateLFO(LFO[i], BUFFER_LFO1 + i, samples, LFOShapes[i]);
  
    if(LFOFade[i] < 1.0f)
    {
      fadeRate = (1.0f / ((double)SampleRate * LFOFadeIn[i]));
      for(j = 0; j < samples; j++)
      {
        LFOFade[i] += fadeRate;
        if(LFOFade[i] > 1.0f)
        {
          LFOFade[i] = 1.0f;
        }
        Buffer[BUFFER_LFO1 + i][j] *= LFOFade[i];
      }
    }
  } 

  if(AmpADSR->IsTriggered())
  {
    // Slide Stage
    Slide->Generate(samples, Buffer[BUFFER_SLIDE], true);

    // VCO Stage

    // VCO 1

    if(PropertyToInt(Sync, 2) == 0)
	  syncBuffer = 0;
    else
	  syncBuffer = Buffer[BUFFER_SYNC];

    MixBuffer(BUFFER_FM, BUFFER_SLIDE, samples, 1.0f, true, false);
    AddBuffer(BUFFER_FM, samples, ((double)(int)((VCO1Semitone - 0.5f) * (8.0f * 12.0f))) / 12.0f);

    for(i = 0; i < 3; i++)
	  if(LFOVCO1Pitch[i] != 0.0f)
	    MixBuffer(BUFFER_FM, BUFFER_LFO1 + i, samples, LFOVCO1Pitch[i], false, false);

    if(VCO1EnvMod > 0.0)
	    MixBuffer(BUFFER_FM, BUFFER_FILTERADSR, samples, VCO1EnvMod * ENVMOD_SCALE, false, false);

    ClearBuffer(BUFFER_PW, samples);
    for(i = 0; i < 3; i++)
	  if(LFOVCO1PulseWidth[i] != 0.0f)
	    MixBuffer(BUFFER_PW, BUFFER_LFO1 + i, samples, LFOVCO1PulseWidth[i], false, false);
    AddBuffer(BUFFER_PW, samples, -1.0f + (VCO1WavePW * 2.0f));

    VCO1->GenerateBLEP(Buffer[BUFFER_PW], 0, Buffer[BUFFER_FM], Buffer[BUFFER_VCO1], syncBuffer, samples);
    for(i = 0; i < 3; i++)
	  if(fabs(LFOVCO1Amp[i]) > 0.0001)
	    MixBuffer(BUFFER_VCO1, BUFFER_LFO1 + i, samples, LFOVCO1Amp[i], false, true);

    // VCO 2

    MixBuffer(BUFFER_FM, BUFFER_SLIDE, samples, 1.0f, true, false);
    AddBuffer(BUFFER_FM, samples, (VCO2Detune + ((double)(int)((VCO2Semitone - 0.5f) * (8.0f * 12.0f)))) / 12.0f);

    for(i = 0; i < 3; i++)
	  if(LFOVCO2Pitch[i] != 0.0f)
	    MixBuffer(BUFFER_FM, BUFFER_LFO1 + i, samples, LFOVCO2Pitch[i], false, false);

    if(VCO2EnvMod > 0.0)
  	  MixBuffer(BUFFER_FM, BUFFER_FILTERADSR, samples, VCO2EnvMod * ENVMOD_SCALE, false, false);

    ClearBuffer(BUFFER_PW, samples);
    for(i = 0; i < 3; i++)
	  if(LFOVCO2PulseWidth[i] != 0.0f)
	    MixBuffer(BUFFER_PW, BUFFER_LFO1 + i, samples, LFOVCO2PulseWidth[i], false, false);
    AddBuffer(BUFFER_PW, samples, -1.0f + (VCO2WavePW * 2.0f));

    VCO2->GenerateBLEP(Buffer[BUFFER_PW], syncBuffer, Buffer[BUFFER_FM], Buffer[BUFFER_VCO2], 0, samples);
    for(i = 0; i < 3; i++)
	  if(fabs(LFOVCO2Amp[i]) > 0.0001)
	    MixBuffer(BUFFER_VCO2, BUFFER_LFO1 + i, samples, LFOVCO2Amp[i], false, true);


    // Mixer Stage

    oscVol = OscVol;
    if(SaturationMode == SATURATION_MODE_NONE)
	  overdrive = 1.0f;
    else
	  overdrive = Overdrive;

    balance = 1.0f - OscBalance;

    if(FilterRouting != FILTER_ROUTING_SPLIT)
    {
	    for(i = 0; i < samples; i++)
	      Buffer[BUFFER_FILTER][i] = ((Buffer[BUFFER_VCO1][i] * balance) + (Buffer[BUFFER_VCO2][i] * OscBalance)) * oscVol;

	    if(Noise > 0.0f)
	      for(i = 0; i < samples; i++)
		    Buffer[BUFFER_FILTER][i] += (-1.0f + ((double)rand() / ((double)RAND_MAX / 2.0f))) * Noise;
     
	    if(RingMod > 0.0f)
	      for(i = 0; i < samples; i++)
		    Buffer[BUFFER_FILTER][i] += (Buffer[BUFFER_VCO1][i] * Buffer[BUFFER_VCO2][i]) * RingMod;

	    if(FilterRouting == FILTER_ROUTING_PARALELL)
	      MixBuffer(BUFFER_FILTER_2, BUFFER_FILTER, samples, 1.0f, true, false);

	    if(overdrive != 1.0f)
	      for(i = 0; i < samples; i++)
		    Buffer[BUFFER_FILTER][i] *= overdrive;
    }
    else
    {
	  for(i = 0; i < samples; i++)
	  {
	    Buffer[BUFFER_FILTER][i]   = (Buffer[BUFFER_VCO1][i] * balance)    * oscVol * overdrive;
	    Buffer[BUFFER_FILTER_2][i] = (Buffer[BUFFER_VCO2][i] * OscBalance) * oscVol;
	  }

	  if(Noise > 0.0f)
	    for(i = 0; i < samples; i++)
		  Buffer[BUFFER_FILTER_2][i] += (-1.0f + ((double)rand() / ((double)RAND_MAX / 2.0f))) * Noise;
    }

    // Filter Stage

    // Formant Filter

    if(FormantMix > 0.0f)
    {
	  double mix2;

	  mix2 = 1.0f - FormantMix;

	  FormantFilter->Process(samples, Buffer[BUFFER_FILTER], Buffer[BUFFER_FORMANT]);
	  for(i = 0; i < samples; i++)
	    Buffer[BUFFER_FILTER][i] = (Buffer[BUFFER_FILTER][i] * mix2) + (Buffer[BUFFER_FORMANT][i] * FormantMix); 
    }
  }
  else
  {
    ClearBuffer(BUFFER_FILTER, samples);
    ClearBuffer(BUFFER_FILTER_2, samples);
  }

  if(AmpADSR->IsTriggered())
  {
    // VCF 1

    mixLevel = (VCF1EnvMod * ENVMOD_SCALE) * (1.0f + ((Velocity - 0.5f)));
    MixBuffer(BUFFER_FILTERMOD, BUFFER_FILTERADSR, samples, mixLevel, true, false);
    MixBuffer(BUFFER_FILTERMOD, BUFFER_SLIDE, samples, (2.0f * (VCF1KeyFollow - 0.5f)), false, false);

    ClearBuffer(BUFFER_RESONANCE, samples);
    for(i = 0; i < 3; i++)
    {
      if(LFOVCF1Pitch[i] != 0.0f)
        MixBuffer(BUFFER_FILTERMOD, BUFFER_LFO1 + i, samples, LFOVCF1Pitch[i], false, false);
      if(LFOVCF1Resonance[i] != 0.0f)
        MixBuffer(BUFFER_RESONANCE, BUFFER_LFO1 + i, samples, LFOVCF1Resonance[i], false, false);
    }
    
    VCF1->Process(samples, Buffer[BUFFER_FILTER], Buffer[BUFFER_FILTERMOD], Buffer[BUFFER_RESONANCE]);
   

    // Saturation

    Saturate(Buffer[BUFFER_FILTER], samples);

    // VCF 2
    
    mixLevel = (VCF2EnvMod * ENVMOD_SCALE) * (1.0f + ((Velocity - 0.5f)));
    MixBuffer(BUFFER_FILTERMOD, BUFFER_FILTERADSR, samples, mixLevel, true, false);
    MixBuffer(BUFFER_FILTERMOD, BUFFER_SLIDE, samples, (2.0f * (VCF2KeyFollow - 0.5f)), false, false);
    
    ClearBuffer(BUFFER_RESONANCE, samples);
    for(i = 0; i < 3; i++)
    {
      if(LFOVCF2Pitch[i] != 0.0f)
        MixBuffer(BUFFER_FILTERMOD, BUFFER_LFO1 + i, samples, LFOVCF2Pitch[i], false, false);
      if(LFOVCF2Resonance[i] != 0.0f)
        MixBuffer(BUFFER_RESONANCE, BUFFER_LFO1 + i, samples, LFOVCF2Resonance[i], false, false);
    }
    
    if(FilterRouting == FILTER_ROUTING_PARALELL || FilterRouting == FILTER_ROUTING_SPLIT)
      VCF2->Process(samples, Buffer[BUFFER_FILTER_2], Buffer[BUFFER_FILTERMOD], Buffer[BUFFER_RESONANCE]);
    else
      VCF2->Process(samples, Buffer[BUFFER_FILTER], Buffer[BUFFER_FILTERMOD], Buffer[BUFFER_RESONANCE]);
  }
  else
  {
    VCF1->Reset();
    VCF2->Reset();
  }

  // Amp Stage
  
  if(AmpADSR->IsTriggered())
  {
    AmpADSR->Generate(samples, Buffer[BUFFER_AMPADSR]);

    VCA->Process(samples, Buffer[BUFFER_FILTER], Buffer[BUFFER_AMPADSR]);
    if(FilterRouting == FILTER_ROUTING_PARALELL || FilterRouting == FILTER_ROUTING_SPLIT)
      VCA->Process(samples, Buffer[BUFFER_FILTER_2], Buffer[BUFFER_AMPADSR]);

    if(FilterRouting == FILTER_ROUTING_PARALELL || FilterRouting == FILTER_ROUTING_SPLIT)
      for(i = 0; i < samples; i++)
      {
        leftOut[sample + i]  += Buffer[BUFFER_FILTER][i]   * mix;
        rightOut[sample + i] += Buffer[BUFFER_FILTER_2][i] * mix;
      }
    else
      for(i = 0; i < samples; i++)
      {
        v = Buffer[BUFFER_FILTER][i]; 
        leftOut[sample + i]  += v * mix;
        rightOut[sample + i] += v * mix;
      }
  }
}

void CBassHeadVoice::ClearBuffer(int index, int samples)
{
  double *buffer;

  buffer = Buffer[index];
  while(samples-- > 0)
    *(buffer++) = 0.0f;
}

void CBassHeadVoice::MixBuffer(int index1, int index2, int samples, double amount, bool replace, bool multiply)
{
  double *buffer1, *buffer2;
  buffer1 = Buffer[index1];
  buffer2 = Buffer[index2];
  MixBlocks(buffer1, buffer2, samples, amount, replace, multiply, Core->CPUFeatures);
}

void CBassHeadVoice::AddBuffer(int index, int samples, double value)
{
  double *buffer;

  buffer = Buffer[index];
  AddValueToBlock(buffer, value, samples, Core->CPUFeatures);
}

void CBassHeadVoice::Saturate(double *data, int samples)
{
  int   i;
  double v;

  switch(SaturationMode)
  {
    case SATURATION_MODE_SOFTCLIP:
    {
      for(i = 0; i < samples; i++)
      {
        v = data[i];
        if(v <= -1.0f)
          data[i] = -(2.0f / 3.0f);
        else if(v >= 1.0f)
          data[i] = (2.0f / 3.0f);
        else
          data[i] = v - (pow(v, 3.0) / 3.0);
      }
      break;
    }
    case SATURATION_MODE_DIGITAL:
    {
      for(i = 0; i < samples; i++)
      {
        v = data[i];
        if(v < -1.0f)
          data[i] = -1.0f;
        else if(v > 1.0f)
          data[i] = 1.0f;
      }
      break;
    }
    case SATURATION_MODE_DISTORT:
    {
      for(i = 0; i < samples; i++)
      {
        v = data[i];
        v = 1.5f * v - 0.5f * v * v * v;
        if(v < -1.0f)
          data[i] = -1.0f;
        else if(v > 1.0f)
          data[i] = 1.0f;
        else
          data[i] = v;
      }
      break;
    }
  }
}

void CBassHeadVoice::WriteMessage(SControlMessage *message)
{
  ControlBuffer->Write(message);
  if(message->Type == CONTROL_MESSAGE_NOTE_ON)
    Note = message->NoteOn.Note;
}

CBassHead::CBassHead(CHost *host, SBassHeadGraphics *graphics, bool polyphonic)
: CMachine(host)
{
  int i;

  GUI = 0;
  Graphics = graphics;
  SampleRate = host->GetSampleRate();
  BlockSamples = host->GetBlockSamples();

  Ports = new CPort*[2];
  Ports[0] = new CPort(host, this, PT_INPUT | PT_CONTROL, "Control In");
  Ports[1] = new CPort(host, this, PT_OUTPUT | PT_AUDIO | PT_STEREO, "Line Out");
  PortCount = 2;

  InitVoices(polyphonic ? BRAIN_VOICE_COUNT : 1);

  PropertyCount = PROPERTY_COUNT;
  srand(GetTickCount());
  Properties = new double[PropertyCount];
  for(i = 0; i < PropertyCount; i++)
    SetProperty(i, PropertyDefaults[i]);
}

CBassHead::~CBassHead()
{
  delete Ports[0];
  delete Ports[1];
  delete Ports;
  FreeVoices();
}

bool CBassHead::IsSynth()
{
  return true;
}

void CBassHead::SetBlockSamples(int blockSamples)
{
  int i;

  for(i = 0; i < VoiceCount; i++)
    Voices[i]->SetBlockSamples(blockSamples);
  BlockSamples = blockSamples;
}

void CBassHead::SetSampleRate(int sampleRate)
{  int i;

  for(i = 0; i < VoiceCount; i++)
    Voices[i]->SetSampleRate(sampleRate);
  SampleRate = sampleRate;
}

void CBassHead::Work()
{
  bool            messageRead;
  SControlMessage message;
  int             i;
 // double           mix;

  messageRead = Ports[0]->ControlBuffer->Read(&message);
  while(messageRead)
  {
    ProcessMessage(&message);
    messageRead = Ports[0]->ControlBuffer->Read(&message);
  }

  Ports[1]->AudioBuffer->Clear();
  /* mix = 1.0f / ((double)VoiceCount); */

  for(i = 0; i < VoiceCount; i++)
    Voices[i]->Work(Ports[1]->AudioBuffer->LeftData, Ports[1]->AudioBuffer->RightData, 1.0f /* mix */ );
}

void CBassHead::ProcessMessage(SControlMessage *message)
{
  int i;

  switch(message->Type)
  {
    case CONTROL_MESSAGE_NOTE_ON:
    {
      for(i = 0; i < VoiceCount; i++)
      {
        if(Voices[i]->Note == message->NoteOn.Note)
        {
          Voices[i]->WriteMessage(message);
          break;
        }
      }
      if(i == VoiceCount)
      {
        Voices[VoiceSelect++]->WriteMessage(message);
        if(VoiceSelect >= VoiceCount)
          VoiceSelect = 0;
        break;
      }
      break;
    }
    case CONTROL_MESSAGE_NOTE_OFF:
    {
      for(i = 0; i < VoiceCount; i++)
      {
        if(Voices[i]->Note == message->NoteOff.Note)
        {
          Voices[i]->WriteMessage(message);
          break;
        }
      }
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

void CBassHead::InitVoices(int voiceCount)
{
  int i;
  
  Voices = new CBassHeadVoice*[voiceCount];
  for(i = 0; i < voiceCount; i++)
    Voices[i] = new CBassHeadVoice(SampleRate, BlockSamples);
  VoiceCount = voiceCount;
  VoiceSelect = 0;
}

void CBassHead::FreeVoices()
{
  int i;

  if(VoiceCount != 0)
  {
    for(i = 0; i < VoiceCount; i++)
      delete Voices[i];
    delete Voices;
  }
}

void CBassHead::GetPropertyName(int index, char *text)
{
  if((index >= 0) && (index < PROPERTY_COUNT))
    strcpy(text, BassHeadPropertyNames[index]);
  else
    *text = 0;
}

void CBassHead::GetPropertyValueDisplay(int index, double value, char *text)
{
  if((index >= 0) && (index < PROPERTY_COUNT))
  {
    switch(PropertyFormat[index])
    {
      case PROPERTY_FORMAT_FORMANT_VOWEL:
      {
        strcpy(text, VowelTable[PropertyToInt(value, VOWEL_COUNT)]);
        break;
      }
      case PROPERTY_FORMAT_MS:
      {
        sprintf(text, "%.2fs", PropertyToTime(value));
        break;
      }
      case PROPERTY_FORMAT_PERCENT:
      {
        sprintf(text, "%.0f%%", value * 100.0f);
        break;
      }
      case PROPERTY_FORMAT_BIPOLAR:
      {
        if(value < 0.5f)
          sprintf(text, "-%.0f%%", fabs(value - 0.5) * 200.0);
        else
          sprintf(text, "+%.0f%%", (value - 0.5) * 200.0);
        break;
      }
      case PROPERTY_FORMAT_SHAPE:
      {
        strcpy(text, VCOWaveNames[PropertyToInt(value, VCO_WAVE_COUNT)]);
        break;
      }
      case PROPERTY_FORMAT_SEMITONES:
      {
        sprintf(text, "%ist", (int)((value - 0.5f) * (12 * 8)));
        break;
      }
      case PROPERTY_FORMAT_SEMITONES2:
      {
        sprintf(text, "%.2fst", value);
        break;
      }
      case PROPERTY_FORMAT_BOOLEAN:
      {
        if(PropertyToInt(value, 2) == 0)
          strcpy(text, "Off");
        else
          strcpy(text, "On");
        break;
      }
      case PROPERTY_FORMAT_FILTER_MODE:
      {
        strcpy(text, FilterModeText[PropertyToInt(value, FILTER_MODE_COUNT)]);
        break;
      }
      case PROPERTY_FORMAT_FILTER_ROUTING:
      {
        strcpy(text, FilterRoutingText[PropertyToInt(value, FILTER_ROUTING_COUNT)]);
        break;
      }
      case PROPERTY_FORMAT_VOLUME:
      {
        if(value == 0.0f)
          sprintf(text, "-oo dB");
        else if(value < 1.0f)
          sprintf(text, "-%.2fdB", (1.0 - value) * VOLUME_SCALE);
        else
          sprintf(text, "0dB");
        break;
      }
      case PROPERTY_FORMAT_OVERDRIVE:
      {
        if(value == 0.0f)
          sprintf(text, "0dB");
        else
          sprintf(text, "+%.2fdB", (value * OVERDRIVE_SCALE));
        break;
      }
      case PROPERTY_FORMAT_SATURATION:
      {
        strcpy(text, SaturationModeText[PropertyToInt(value, SATURATION_MODE_COUNT)]);
        break;
      }
      case PROPERTY_FORMAT_LFORATE:
      {
        value = MIN_LFO_VOLTAGE + ((MAX_LFO_VOLTAGE - MIN_LFO_VOLTAGE) * value);
        value = fastpow2(value - 5.0f) * 440.0f;
        sprintf(text, "%.2fHz", value);
        break;
      }
      case PROPERTY_FORMAT_LFOSHAPE:
      {
        int shape;

        shape = PropertyToInt(value, LFO_SHAPE_COUNT);
        if(shape < LFO_SHAPE_COUNT)
          strcpy(text, LFOShapeText[shape]);
        else
          sprintf(text, "%i", shape - LFO_SHAPE_COUNT);
        break;
      }
      case PROPERTY_FORMAT_FADEIN:
      {
        sprintf(text, "%.2fs", PropertyToFade(value)); 
        break;
      }
      case PROPERTY_FORMAT_SEND:
      {
        sprintf(text, PropertySendNames[PropertyToSend(value)]);
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

double CBassHead::GetProperty(int index)
{
  if((index >= 0) && (index < PROPERTY_COUNT))
    return Properties[index];
  else
    return 0.0f;
}

void CBassHead::SetProperty(int index, double value)
{
  SControlMessage message;
  int             i;

  if((index >= 0) && (index < PROPERTY_COUNT))
  {
    Properties[index] = value;
  
    memset(&message, 0, sizeof(SControlMessage));
    message.Type = CONTROL_MESSAGE_CONTROL_CHANGE;
    message.ControlChange.Controller = index;
    message.ControlChange.Value = value;
    for(i = 0; i < VoiceCount; i++)
    Voices[i]->WriteMessage(&message);
   
#ifndef DW_HEADLESS
    if(GUI)
      GUI->SetProperty(index, value, SettingProgram != 0 ? true : false);
#endif
  }
}

CBassHeadInfo::CBassHeadInfo(CHost *host)
{
  Loaded = false;
  Host = host;
  Name = "ES BassHead";
  UniqueID = "DarkPlug::ES BassHead";
  IconSmall = (HICON)LoadImage(AppInstance, MAKEINTRESOURCE(IDI_BRAIN), IMAGE_ICON,
    16, 16, LR_DEFAULTCOLOR);
  IconLarge = (HICON)LoadImage(AppInstance, MAKEINTRESOURCE(IDI_BRAIN), IMAGE_ICON,
    32, 32, LR_DEFAULTCOLOR);
  
}

CMachine *CBassHeadInfo::CreateMachine()
{
  if(!Loaded)
  {
    Graphics.Background = new CFrameBuffer(IDR_BASSHEAD_BACKGROUND);
    Graphics.SliderBack = new CFrameBuffer(IDR_BASSHEAD_SLIDERBACK);
    Graphics.SliderKnob = new CFrameBuffer(IDR_BASSHEAD_SLIDERKNOB);
    Graphics.Font       = new CFrameBuffer(IDR_BASSHEAD_FONT);
    Loaded = true;
  }
  return new CBassHead(Host, &Graphics, false);
}

CBassHeadPolyphonicInfo::CBassHeadPolyphonicInfo(CHost *host)
{
  Loaded = false;
  Host = host;
  Name = "ES BassHead (Polyphonic)";
  UniqueID = "DarkPlug::ES BassHead (Polyphonic)";
  IconSmall = (HICON)LoadImage(AppInstance, MAKEINTRESOURCE(IDI_BRAIN), IMAGE_ICON,
    16, 16, LR_DEFAULTCOLOR);
  IconLarge = (HICON)LoadImage(AppInstance, MAKEINTRESOURCE(IDI_BRAIN), IMAGE_ICON,
    32, 32, LR_DEFAULTCOLOR);
  
}

CMachine *CBassHeadPolyphonicInfo::CreateMachine()
{
  if(!Loaded)
  {
    Graphics.Background = new CFrameBuffer(IDR_BASSHEAD_BACKGROUND);
    Graphics.SliderBack = new CFrameBuffer(IDR_BASSHEAD_SLIDERBACK);
    Graphics.SliderKnob = new CFrameBuffer(IDR_BASSHEAD_SLIDERKNOB);
    Graphics.Font       = new CFrameBuffer(IDR_BASSHEAD_FONT);
    Loaded = true;
  }
  return new CBassHead(Host, &Graphics, true);
}

double CBassHeadVoice::FrequencyToVoltage(double frequency)
{
  return ((log(frequency) - Log440) * InvLog2) + 5.0;
}

int CBassHeadVoice::VCOShape(double value)
{
  int shape;
  
  shape = PropertyToInt(value, VCO_WAVE_COUNT);
  switch(shape)
  {
    case 1:
      return VCO_SHAPE_SQUARE;
    case 2:
      return VCO_SHAPE_TRIANGLE;
    default:
      return VCO_SHAPE_SAW;
  }
}

HWND CBassHead::CreateGUI(HWND parent)
{
#ifndef DW_HEADLESS
  if(GUI != 0)
  {
    delete GUI;
	  GUI = 0;
  }
  if(GUI == 0)
  {
    GUI = new CBassHeadGUI(parent, 0, this, Graphics);
  }
  return GUI->Handle;
#else
  return 0;
#endif
}

#ifndef DW_HEADLESS
CBassHeadGUI::CBassHeadGUI(HWND parent, CColorManager *colorManager, CBassHead *machine, SBassHeadGraphics *graphics)
: CLiteContainer(parent, graphics->Background->GetWidth(), graphics->Background->GetHeight(), graphics->Background)
{
  int i, p = 0;
  double x, y;

  Font = new CBitFont(graphics->Font, 12, 8, 12, 12, 6, 33, 122);
  Screen = new CLCDScreen(this, 416, 344, 176, 160, Font);
  LastWritten = -1;
  BassHead = machine;

  Sliders = new CLiteSlider*[PROPERTY_COUNT];
  for(i = 0; i < PROPERTY_COUNT; i++)
    Sliders[i] = 0;

  // Amp Envelope
  x = 104; y = 72;
  for(i = 0; i < 7; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  // Filter Envelope
  x = 304; y = 72;
  for(i = 0; i < 7; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  // VCO 1
  x = 104; y = 256;
  for(i = 0; i < 4; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  // VCO 2
  x = 304; y = 256;
  for(i = 0; i < 6; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  // VCF 1
  x = 104; y = 416;
  for(i = 0; i < 5; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  // VCF 2
  x = 304; y = 416;
  for(i = 0; i < 5; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  // Sound
  x = 504; y = 72;
  for(i = 0; i < 12; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  // LFO 1
  x = 104; y = 560;
  for(i = 0; i < 9; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  // LFO 2
  x = 304; y = 560;
  for(i = 0; i < 9; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  // LFO 3
  x = 504; y = 560;
  for(i = 0; i < 9; i++) 
  {
    Sliders[p] = new CLiteSlider(this, (int)x, (int)y, graphics->SliderBack->GetWidth() + 1, -6, graphics->SliderKnob, graphics->SliderBack, machine, p);
    p++;
    y += (24 * 0.8f);
  }

  Screen->Write("BassHead Ready");

  Render();
  Show();
}

CBassHeadGUI::~CBassHeadGUI()
{
  int i;

  for(i = 0; i < PROPERTY_COUNT; i++)
    if(Sliders[i])
      delete Sliders[i];

  delete Font;
  delete Screen;
}

void CBassHeadGUI::OnLiteSliderDown(CLiteSlider *slider, double value)
{
}

void CBassHeadGUI::OnLiteSliderChange(CLiteSlider *slider, double value)
{
}

void CBassHeadGUI::OnShowGUI()
{
}

void CBassHeadGUI::SetLCDText(char *text)
{
  //LCDText->SetText(text);
}

void CBassHeadGUI::ShowToolTip(CLiteControl *control, char *text)
{
  //LCDText->SetText(text);
}

void CBassHeadGUI::SetProperty(int index, double value, bool settingProgram)
{
  char display[1024];
  char prop[1024];
  char msg[1024];

  if(Sliders[index] != 0)
  {
    Sliders[index]->SetValue(value);
    if(!settingProgram)
    {
      BassHead->GetPropertyValueDisplay(index, value, display);
      BassHead->GetPropertyName(index, prop);
      sprintf(msg, "%s -> %s", prop, display);
      Screen->Write(msg, LastWritten != index);
      LastWritten = index;
    }
  }
}

CLCDScreen::CLCDScreen(CLiteContainer *parent, int x, int y, int width, int height, CBitFont *font)
: CLiteControl(parent, x + 4, y + 4, width - 8, height - 8)
{
  int i;

  Font   = font;
  Lines  = Height / Font->CellHeight;
  Length = Width / Font->SpacingX;

  Text = new char*[Lines];
  for(i = 0; i < Lines; i++)
  {
    Text[i] = new char[Length + 1];
    Text[i][0] = 0;
  }
}

CLCDScreen::~CLCDScreen()
{
  int i;

  for(i = 0; i < Lines; i++)
    delete Text[i];
  delete Text;
}

void CLCDScreen::Write(char *text)
{
  Write(text, true);
}

void CLCDScreen::Write(char *text, bool scroll)
{
  int i;

  if(scroll)
  {
    for(i = 0; i < (Lines - 1); i++)
      strcpy(Text[i], Text[i + 1]);
  }
  for(i = 0; (i <= Length) && (text[i] != 0); i++)
    Text[Lines - 1][i] = text[i];
  Text[Lines - 1][i] = 0;
  Parent->Render(X, Y, Width, Height);
}

void CLCDScreen::Draw(CFrameBuffer *frameBuffer)
{
  int i, y;

  y = Y;
  for(i = 0; i < Lines; i++)
  {
    Font->DrawText(frameBuffer, Text[i], X, y, Width, Font->CellHeight);
    y += Font->CellHeight;
  }
}
#endif
