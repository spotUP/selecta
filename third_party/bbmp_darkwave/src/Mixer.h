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

#ifndef MIXER_H
#define MIXER_H

#include "DarkPlug.h"
#include "EQ.h"
#include "LiteContainer.h"
#include "LiteKnob.h"
#include "LiteButton.h"
#include "LiteSlider.h"
#include "LiteLCDText.h"

struct SMixerGraphics
{
  CFrameBuffer *Panel;
  CFrameBuffer *Knob;
  CFrameBuffer *Mute, *MuteHover, *MuteClick;
  CFrameBuffer *Solo, *SoloHover, *SoloClick;
  CFrameBuffer *Level, *Balance;
  CFrameBuffer *Font;
  CFrameBuffer *Header;
  CBitFont     *BitFont;
};

class CMixerGUI;
#ifndef DW_HEADLESS
class CLiteMeter : public CLiteControl
{
public:
               CLiteMeter(CLiteContainer *parent, int x, int y, int width, int height);
              ~CLiteMeter();
  virtual void Draw(CFrameBuffer *frameBuffer);
  virtual void SetVolume(double volume);

private:
  double Volume;
};

class CMixer;
class CMixerTrackGUI;
class CMixerHeader;

class CMixerGUI : public CControl
{
public:
       CMixerGUI(HWND parent, CColorManager *colorManager, CMixer *machine, SMixerGraphics *graphics, int tracks, int auxTracks);
      ~CMixerGUI();
  void SetProperty(int index, double value);
  
  CMixerTrackGUI **Track;
  CMixerHeader    *Header;
  int              Tracks, AUXTracks;
};

class CMixerHeader : public CLiteContainer
{
public:
  CMixerHeader(CControl *parent, SMixerGraphics *graphics);
 ~CMixerHeader();
};

class CMixerTrackGUI : public CLiteContainer
{
public:
       CMixerTrackGUI(HWND parent, CColorManager *colorManager, CMixer *machine, SMixerGraphics *graphics, int auxTrack, bool isAUX, int firstProperty, int track);
      ~CMixerTrackGUI();
  void SetProperty(int index, double value);
  virtual void DrawBackground(int x1, int y1, int x2, int y2, int w, int h);
  void SetVolume(double left, double right);
  virtual void OnButtonClick(CLiteButton *button);
  virtual void SetSolo(bool isSolo);

private:
  bool            IsAUX;
  CMixer         *Machine;
  SMixerGraphics *Graphics;
  CLiteKnob      *LowKnob, *MidKnob, *HighKnob;
  CLiteKnob      *AUX1Knob, *AUX2Knob;
  CLiteButton    *MuteButton, *SoloButton;
  CLiteSlider    *LevelSlider, *BalanceSlider;
  int             Track, AUXTrack;
  CLiteMeter     *LevelLeft, *LevelRight;
};

#endif
class CMixer : public CMachine
{
public:
	             CMixer(CHost *host, SMixerGraphics *graphics, int tracks, int auxTracks);
 virtual      ~CMixer();
 virtual void  DARKCALL Work();
 virtual void  DARKCALL WorkAUX();
 virtual void  DARKCALL SetSampleRate(int sampleRate);
 virtual void  DARKCALL SetBlockSamples(int blockSamples);
 virtual bool  DARKCALL IsSynth();
 virtual void  DARKCALL GetPropertyValueDisplay(int index, double value, char *text);
 virtual void  DARKCALL GetPropertyDisplay(int index, char *text);
 virtual void  DARKCALL GetPropertyName(int index, char *text);
 virtual double DARKCALL GetProperty(int index);
 virtual void  DARKCALL SetProperty(int index, double value);
 virtual HWND  DARKCALL CreateGUI(HWND parent);
 virtual void  DARKCALL OnIdle();
 virtual void  DARKCALL SetMute(int track, bool isMute);
 virtual void  DARKCALL SetSolo(int track, bool isSolo);

private:
  double          *Properties;
  CEQ***          EQ;
  double*          Max;
  CHost          *Host;
  int             BlockSamples;
  CMixerGUI      *Mixer;
  int             Tracks, AUXTracks;
  SMixerGraphics *Graphics;
  CMixerGUI      *GUI;
  bool           *Mute;
  int             SoloTrack;
};

class CMixerInfo : public CMachineInfo
{
public:
                            CMixerInfo(CHost *host);
 virtual                   ~CMixerInfo();
 virtual CMachine* DARKCALL CreateMachine();

private:
  CHost         *Host;
  SMixerGraphics Graphics;
  bool           Loaded;
};

#endif
