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

#ifndef BASSHEAD_H
#define BASSHEAD_H

#include "DSP.h"
#include "VCO.h"
#include "ADSR.h"
#include "MultiFilter.h"
#include "VCA.h"
#include "Slide.h"
#include "DarkPlug.h"
#include "FormantFilter.h"
#include "LiteContainer.h"
#include "LiteSlider.h"
#include "LiteLCDText.h"
#include "BitFont.h"

class CBassHead;

struct SBassHeadGraphics
{
  CFrameBuffer *Background;
  CFrameBuffer *SliderBack, *SliderKnob;
  CFrameBuffer *Font;
};

class CBassHeadGUI;
#ifndef DW_HEADLESS
class CLCDScreen : public CLiteControl
{
public:
       CLCDScreen(CLiteContainer *parent, int x, int y, int width, int height, CBitFont *font);
      ~CLCDScreen();
  void Write(char *text);
  void Write(char *text, bool scroll);
  void Draw(CFrameBuffer *frameBuffer);

private:
  CBitFont *Font;
  int       Lines, Length;
  char    **Text;
};

class CBassHeadGUI : public CLiteContainer
{
public:
               CBassHeadGUI(HWND parent, CColorManager *colorManager, CBassHead *machine, SBassHeadGraphics *graphics);
              ~CBassHeadGUI();
  virtual void ShowToolTip(CLiteControl *control, char *text);
  virtual void OnLiteSliderChange(CLiteSlider *slider, double value);
  virtual void OnLiteSliderDown(CLiteSlider *slider, double value);
  void         SetLCDText(char *text);
  void         SetProperty(int index, double value, bool settingProgram);
  void         OnShowGUI();

private:
  int                    LastWritten;
  CLiteSlider          **Sliders;
  CBitFont              *Font;
  CBassHead             *BassHead;
  CLCDScreen            *Screen;   
};

#endif
class CBassHeadVoice 
{
public:
          CBassHeadVoice(int sampleRate, int blockSamples);
         ~CBassHeadVoice();
  void    Work(double *leftOut, double *rightOut, double mix);
  void    ProcessMessage(SControlMessage *message);
  void    GenerateBlock(int sample, int samples, double *leftOut, double *rightOut, double mix);
  void    WriteMessage(SControlMessage *message);
  void    SetVCOFrequencies();
  void    SetVCFFrequencies();
  double   VCFVoltage(double cutoff, double keyFollow);
  void    Saturate(double *data, int samples);
  void    ClearBuffer(int index, int samples);
  void    MixBuffer(int index1, int index2, int samples, double amount, bool replace, bool multiply);
  int     LFOShape(double value);
  void    SetLFOWave(int shape, CVCO *lfo);
  void    GenerateLFO(CVCO *lfo, int buffer, int samples, int shape);
  double   FrequencyToVoltage(double frequency);
  void    AddBuffer(int index, int samples, double value);
  int     VCOShape(double value);
  void    SetSampleRate(int sampleRate);
  void    SetBlockSamples(int blockSamples);

  int Note;

private:
  CControlBuffer *ControlBuffer;
  CSlide         *Slide;
  CVCO           *VCO1, *VCO2, *LFO[3];
  CMultiFilter   *VCF1, *VCF2;
  CFormantFilter *FormantFilter;
  CADSR          *AmpADSR, *FilterADSR;
  CVCA           *VCA;
  int             BlockSamples, SampleRate, SaturationMode;
  double         **Buffer, Frequency;
  double           VCO1Shape, VCO1WavePW, VCO1Semitone;
  double           VCO2Shape, VCO2WavePW, VCO2Semitone, VCO2Detune;
  double           Sync, VCO1EnvMod, VCO2EnvMod;
  double           VCF1Cutoff, VCF1KeyFollow, VCF1EnvMod;
  double           VCF2Cutoff, VCF2KeyFollow, VCF2EnvMod;
  double           Log440, InvLog2, OscBalance, OscVol, RingMod, Noise;
  int             LFOMod[3], FilterRouting;
  double           LFOAmount[3], Velocity, Overdrive;
  double           LFOFadeIn[3], LFOFade[3], LFOVCO1Pitch[3], LFOVCO2Pitch[3], LFOVCO1PulseWidth[3], LFOVCO2PulseWidth[3];
  double           LFOVCO1Amp[3], LFOVCO2Amp[3], LFOVCF1Pitch[3], LFOVCF2Pitch[3], LFOVCF1Resonance[3], LFOVCF2Resonance[3];
  double           FormantMix, LFOAmp[3][3];
  int             LFOSend[3][3], LFOShapes[3];
};

class CBassHead : public CMachine
{
public:
                CBassHead(CHost *host, SBassHeadGraphics *graphics, bool polyphonic);
  virtual      ~CBassHead();
  virtual void  DARKCALL Work();
  virtual void  DARKCALL GetPropertyName(int index, char *text);
  virtual void  DARKCALL GetPropertyValueDisplay(int index, double value, char *text);
  virtual double DARKCALL GetProperty(int index);
  virtual void  DARKCALL SetProperty(int index, double value);
  virtual void  DARKCALL SetSampleRate(int sampleRate);
  virtual void  DARKCALL SetBlockSamples(int blockSamples);
  virtual HWND  DARKCALL CreateGUI(HWND parent);
  virtual bool  DARKCALL IsSynth();

private:
  void          InitVoices(int voiceCount);
  void          FreeVoices();
  void          ProcessMessage(SControlMessage *message);

  int                SampleRate, BlockSamples, VoiceCount, VoiceSelect;
  double             *Properties;
  CBassHeadVoice   **Voices;
  CBassHeadGUI      *GUI;
  SBassHeadGraphics *Graphics;
};

class CBassHeadInfo : public CMachineInfo
{
public:
            CBassHeadInfo(CHost *host);
  CMachine* DARKCALL CreateMachine();

private:
  CHost            *Host;
  SBassHeadGraphics Graphics;
  bool              Loaded;
};

class CBassHeadPolyphonicInfo : public CMachineInfo
{
public:
            CBassHeadPolyphonicInfo(CHost *host);
  CMachine* DARKCALL CreateMachine();

private:
  CHost            *Host;
  SBassHeadGraphics Graphics;
  bool              Loaded;
};

#endif
