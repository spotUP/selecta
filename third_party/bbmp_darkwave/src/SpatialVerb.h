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

#ifndef SPATIALVERB_H
#define SPATIALVERB_H

#include "FIR.h"
#include "FDN.h"
#include "ImpulseView.h"
#include "ImageSource.h"
#include "DarkPlug.h"
#include "Control.h"
#include "BackBuffer.h"
#include "ToolTip.h"
#include "LiteKnob.h"
#include "LiteSlider.h"
#include "LiteLCDText.h"
#include "BitFont.h"
#include "LiteButton.h"

struct SSpatialVerbGraphics
{
  CFrameBuffer *Background;
  CFrameBuffer *Knob;
  CFrameBuffer *SliderV, *SliderH;
  CFrameBuffer *Nodes;
  CFrameBuffer *LCDFont;
};

class CSpatialVerb;
class CSpatialVerbGUI;
class CRoomView;

#ifndef DW_HEADLESS
class CSpatialVerbGUI : public CLiteContainer
{
public:
               CSpatialVerbGUI(HWND parent, CColorManager *colorManager, CSpatialVerb *machine, SSpatialVerbGraphics *graphics);
              ~CSpatialVerbGUI();
  void         UpdateRoom(CImageSource *leftImageSource, CImageSource *rightImageSource, SSize *roomSize, SPos *srcLeftPos, SPos *srcRightPos, SPos *dstLeftPos, SPos *dstRightPos, double totalTime);
  void         UpdateImpulse(int samples, CStereoFIR *leftFIR, CStereoFIR *rightFIR);
  void         UpdateImpulse2(int samples, CFDN *fdn, int earlySamples);
  virtual void ShowToolTip(CLiteControl *control, char *text);
  virtual void OnLiteSliderChange(CLiteSlider *slider, double value);
  virtual void OnLiteSliderDown(CLiteSlider *slider, double value);
  void         SetLCDText(char *text);
  void         SetProperty(int index, double value);
  void         OnShowGUI();
  void         OnButtonClick(CLiteButton *button);

private:
  SSpatialVerbGraphics  *Graphics;
  CLiteKnob            **Knobs;
  CRoomView             *RoomView;
  CImpulseView          *EarlyImpulseView, *LateImpulseView;
  CLiteSlider          **Sliders;
  CLiteLCDText          *LCDText;
  CBitFont              *LCDFont;
  CSpatialVerb          *SpatialVerb;
};

#endif
class CSpatialVerb : public CMachine
{
public:
                CSpatialVerb(CHost *Host, SSpatialVerbGraphics *graphics);
  virtual      ~CSpatialVerb();
  virtual void  DARKCALL Work();
  virtual HWND  DARKCALL CreateGUI(HWND parent);
  virtual double DARKCALL GetProperty(int index);
  virtual void  DARKCALL SetProperty(int index, double value);
  virtual void  DARKCALL GetPropertyName(int index, char *text);
  virtual void  DARKCALL GetPropertyValueDisplay(int index, double value, char *text);
  void          ModelRoom();
  void          FreeRays();
  void          UpdateGUIRoom();
  void          ModelFIRs();
  void          UpdateGUIImpulse();
  void          UpdateGUIImpulse2();
  void          ModelFDN();
  void          UpdateDstPos();
  double         PropertyToGain(double value);
  double         PropertyToFeedback(double value);
  void          MoveNode(int node, SPos *pos);
  virtual void  DARKCALL SetSampleRate(int sampleRate);
  virtual void  DARKCALL SetBlockSamples(int blockSamples);
  virtual bool  DARKCALL GetGUIRect(short *x1, short *y1, short *x2, short *y2);
  virtual void  DARKCALL OnShowGUI();
  virtual void  DARKCALL AfterSetProgram();
  void          Update();
  virtual void  DARKCALL OnIdle();
  virtual bool  DARKCALL IsSynth();

private:
  CSpatialVerbGUI      *GUI;
  double                *Properties;
  double                 EarlyTime, WallAbsorb;
  int                   SampleRate, EarlySamples, BlockSamples;
  SPos                  SrcLeftPos, SrcRightPos, DstLeftPos, DstRightPos;
  SSize                 RoomSize;
  CImageSource         *LeftImageSource, *RightImageSource;
  CStereoFIR           *LeftFIR, *RightFIR;
  CFDN                 *FDN;
  SSpatialVerbGraphics *Graphics;
  bool                  NeedToUpdateGUI, NeedToUpdateRoom, NeedToUpdateFIRs, NeedToUpdateFDN;
  int                   nSamples;
};

class CSpatialVerbInfo : public CMachineInfo
{
public:
                    CSpatialVerbInfo(CHost *host);
  virtual          ~CSpatialVerbInfo();
  virtual CMachine* DARKCALL CreateMachine();
  
private:
  CHost               *Host;
  SSpatialVerbGraphics Graphics;
  bool                 Loaded;
};

#endif
