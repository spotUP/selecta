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

#ifndef DRUMKIT_H
#define DRUMKIT_H

#include "DarkPlug.h"
#include "ToolBarButton.h"
#include "SampleView.h"
#include "ToolBar.h"
#include "AD.h"
#include "VCO.h"
#include "VCA.h"
#include "MultiFilter.h"
#include "LiteKnob.h"
#
class CDrumKit;

class CDrumKitVoice 
{
public:
                CDrumKitVoice(int sampleRate, int blockSamples, int bufferCount);
               ~CDrumKitVoice();
  void          Work(double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  void          WriteMessage(SControlMessage *message);
  void          ClearBuffer(int index, int samples);
  virtual void  ProcessMessage(SControlMessage *message) = 0;
  virtual void  GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel) = 0;
  void          DiodeClip(int buffer, int samples, double level);
  void          SetBlockSamples(int blockSamples);
  virtual void  SetSampleRate(int sampleRate) = 0;
  void          SetIdle(bool value);
  bool          IsIdle();

protected:
  CControlBuffer *ControlBuffer;
  int             BlockSamples, SampleRate, BufferCount;
  double         **Buffer, LeftLevelFollow, RightLevelFollow;
  bool            InitLevels;

private:
  bool            Idle;
};

class CBassDrum : public CDrumKitVoice
{
public:
                CBassDrum(int sampleRate, int blockSamples);
               ~CBassDrum();
  virtual void  ProcessMessage(SControlMessage *message);
  virtual void  GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  virtual void  SetSampleRate(int sampleRate);

private:
  CAD            *VCOAD, *AmpAD1, *AmpAD2;
  CVCO           *VCO;
  CVCA           *VCA1, *VCA2;
  CMultiFilter   *LPF1, *BPF1, *LPF2;
  double           BaseVoltage, SweepVoltage;
  bool            Trigger;
};

class CSnareDrum : public CDrumKitVoice
{
public:
                CSnareDrum(int sampleRate, int blockSamples);
               ~CSnareDrum();
  virtual void  ProcessMessage(SControlMessage *message);
  virtual void  GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  virtual void  SetSampleRate(int sampleRate);

private:
  CAD          *AmpAD1, *AmpAD2, *VCOAD, *LPAD, *HPAD;
  CVCO         *VCO1, *VCO2;
  CVCA         *VCA;
  CMultiFilter *LPF, *HPF;
  double         Snappy, SnappyFollow;
};

class CTomDrum : public CDrumKitVoice
{
public:
               CTomDrum(int sampleRate, int blockSamples, int tomType);
              ~CTomDrum();
  virtual void ProcessMessage(SControlMessage *message);
  virtual void GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  virtual void SetSampleRate(int sampleRate);
  void         SetTune(double value);
  void         SetDecay(double value);

private:
  CAD          *AmpAD1, *NoiseAD;
  CVCO         *VCO1, *VCO2, *VCO3;
  CVCA         *VCA;
  CMultiFilter *NoiseFilter;
  int           TomType;
};

class CRimShot : public CDrumKitVoice
{
public:
                CRimShot(int sampleRate, int blockSamples);
			         ~CRimShot();
  virtual void  ProcessMessage(SControlMessage *message);
  virtual void  GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  virtual void  SetSampleRate(int sampleRate);

private:
  CAD          *AD1, *AD2;
  CMultiFilter *PinkFilter, *LPFilter;
  CVCA         *VCA;

};

class CClap : public CDrumKitVoice
{
public:
                CClap(int sampleRate, int blockSamples);
			         ~CClap();
  virtual void  ProcessMessage(SControlMessage *message);
  virtual void  GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  virtual void  SetSampleRate(int sampleRate);

private:
  CAD          *AD1, *AD2;
  CMultiFilter *PinkFilter, *LPFilter;
  CVCA         *VCA;

};

class CHiHatClosed : public CDrumKitVoice
{
public:
                CHiHatClosed(int sampleRate, int blockSamples);
               ~CHiHatClosed();

  virtual void  ProcessMessage(SControlMessage *message);
  virtual void  GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  virtual void  SetSampleRate(int sampleRate);

private:
  CAD           *AD;
  CMultiFilter  *BPFilter;
  CVCA          *VCA;
};

class CHiHatOpen : public CDrumKitVoice
{
public:
                CHiHatOpen(int sampleRate, int blockSamples);
               ~CHiHatOpen();

  virtual void  ProcessMessage(SControlMessage *message);
  virtual void  GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  virtual void  SetSampleRate(int sampleRate);

private:
  CAD           *AD;
  CMultiFilter  *BPFilter, *HPFilter;
  CVCA          *VCA;
};

class CCrashCymbal : public CDrumKitVoice
{
public:
                CCrashCymbal(int sampleRate, int blockSamples);
               ~CCrashCymbal();
  virtual void  ProcessMessage(SControlMessage *message);
  virtual void  GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  virtual void  SetSampleRate(int sampleRate);
          void  SetTune(double value);

private:
  CAD          *AD1, *AD2, *AD3;
  CMultiFilter *Filter1, *Filter2;
  CVCA         *VCA;
  CVCO         *VCO1, *VCO2, *VCO3, *VCO4, *VCO5, *VCO6;
};

class CRideCymbal : public CDrumKitVoice
{
public:
                CRideCymbal(int sampleRate, int blockSamples);
               ~CRideCymbal();
  virtual void  ProcessMessage(SControlMessage *message);
  virtual void  GenerateBlock(int sample, int samples, double *outLeft, double *outRight, double *commonNoise, double leftLevel, double rightLevel);
  virtual void  SetSampleRate(int sampleRate);

private:
  CAD          *AD1, *AD2, *AD3;
  CMultiFilter *Filter1, *Filter2;
  CVCA         *VCA;
  CVCO         *VCO1, *VCO2, *VCO3, *VCO4, *VCO5, *VCO6;
};

class CDrumKitGUI;
class CDrumKitInfo;

class CDrumKit : public CMachine
{
public:
                CDrumKit(CHost *host, CDrumKitInfo *info);
  virtual      ~CDrumKit();
  virtual void  DARKCALL Work();
  void          ProcessMessage(SControlMessage *message);
  virtual void  DARKCALL GetPropertyName(int index, char *text);
  virtual void  DARKCALL GetPropertyValueDisplay(int index, double value, char *text);
  virtual double DARKCALL GetProperty(int index);
  virtual void  DARKCALL SetProperty(int index, double value);
  virtual HWND  DARKCALL CreateGUI(HWND parent);
  int           NoteToVoice(int note);
  virtual void  DARKCALL GetVoiceName(int index, char *text);
  virtual void  DARKCALL SetSampleRate(int sampleRate);
  virtual void  DARKCALL SetBlockSamples(int blockSamples);
  virtual bool  DARKCALL GetGUIRect(short *x1, short *y1, short *x2, short *y2);
  virtual bool  DARKCALL IsSynth();

private:
  int            SampleRate, BlockSamples;
  double         *Properties, *NoiseBuffer;
  CDrumKitVoice **Voices;
  CDrumKitGUI   *GUI;
  CDrumKitInfo  *Info;
};

class CDrumKitInfo : public CMachineInfo
{
public:
                     CDrumKitInfo(CHost *host);
                    ~CDrumKitInfo();
  CMachine* DARKCALL CreateMachine();

  CFrameBuffer *Background, *Knob;

private:
  CHost *Host;
};

#ifndef DW_HEADLESS
class CDrumKitGUI : public CLiteContainer
{
public:
               CDrumKitGUI(HWND parent, CDrumKit *drumKit, CDrumKitInfo *info);
              ~CDrumKitGUI();
          void SetProperty(int index, double value);

private:
  CDrumKit       *DrumKit;
  CDrumKitInfo   *Info;
  CLiteKnob      **Knob;
  int            KnobCount;
};

#endif
#endif
