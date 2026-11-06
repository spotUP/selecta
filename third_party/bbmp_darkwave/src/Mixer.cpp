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

#include "Mixer.h"
#include "EQ.h"
#include "Util.h"
#include "Core.h"
#include <stdio.h>
#include "Resource.h"

#define TRACK_PROPERTY_LEVEL   0
#define TRACK_PROPERTY_BALANCE 1
#define TRACK_PROPERTY_LOW     2
#define TRACK_PROPERTY_MID     3
#define TRACK_PROPERTY_HIGH    4
#define TRACK_PROPERTY_AUX1    5
#define TRACK_PROPERTY_AUX2    6

#define TRACK_PROPERTY_COUNT     7
#define TRACK_PROPERTY_COUNT_AUX 5

CMixer::CMixer(CHost *host, SMixerGraphics *graphics, int tracks, int auxTracks)
: CMachine(host)
{
  int  i;
  char name[32];

  Host = host;
  Graphics = graphics;
  Tracks = tracks;
  AUXTracks = auxTracks;
  GUI = 0;

  BlockSamples = host->GetBlockSamples();

  PortCount = Tracks + (2 * AUXTracks) + 1;
  Ports = new CPort*[PortCount];

  Ports[0] = new CPort(host, this, PT_AUDIO | PT_OUTPUT | PT_STEREO, "OUTPUT");

  EQ = new CEQ**[Tracks + AUXTracks];

  for(i = 0; i < Tracks; i++)
  {
    sprintf(name, "TRK%iIN", i + 1);
    Ports[i + 1] = new CPort(host, this, PT_AUDIO | PT_INPUT | PT_STEREO, name);
    EQ[i] = new CEQ*[2];
    EQ[i][0] = new CEQ(EQ_DEFAULT_LOWFREQ, EQ_DEFAULT_HIGHFREQ, (double)host->GetSampleRate());
    EQ[i][1] = new CEQ(EQ_DEFAULT_LOWFREQ, EQ_DEFAULT_HIGHFREQ, (double)host->GetSampleRate());
  }

  for(i = 0; i < AUXTracks; i++)
  {
    sprintf(name, "AUX%iOUT", i + 1);
    Ports[Tracks + (AUXTracks * i) + 1] = new CPort(host, this, PT_AUDIO | PT_OUTPUT | PT_AUX | PT_STEREO, name);
    sprintf(name, "AUX%iIN", i + 1);
    Ports[Tracks + (AUXTracks * i) + 2] = new CPort(host, this, PT_AUDIO | PT_INPUT | PT_AUX | PT_STEREO, name);
    EQ[Tracks + i] = new CEQ*[2];
    EQ[Tracks + i][0] = new CEQ(EQ_DEFAULT_LOWFREQ, EQ_DEFAULT_HIGHFREQ, (double)host->GetSampleRate());
    EQ[Tracks + i][1] = new CEQ(EQ_DEFAULT_LOWFREQ, EQ_DEFAULT_HIGHFREQ, (double)host->GetSampleRate());
  }

  PropertyCount = (Tracks * TRACK_PROPERTY_COUNT) + (AUXTracks * TRACK_PROPERTY_COUNT_AUX);
  Properties = new double[PropertyCount];
  for(i = 0; i < PropertyCount; i++)
    SetProperty(i, 0.5);

  Max = new double[2 * (Tracks + AUXTracks)];
  for(i = 0; i < 2 * (Tracks + AUXTracks); i++)
    Max[i] = 0.0;

  Mute = new bool[Tracks + AUXTracks];
  for(i = 0; i < Tracks + AUXTracks; i++)
    Mute[i] = false;
  SoloTrack = -1;
}

CMixer::~CMixer()
{
  int i;

  delete Mute;

#ifndef DW_HEADLESS
  if(GUI)
    delete GUI;
#endif

  delete Max;

  for(i = 0; i < PortCount; i++)
    delete Ports[i];

  for(i = 0; i < Tracks + AUXTracks; i++)
  {
    delete EQ[i][0];
    delete EQ[i][1];
    delete EQ[i];
  }
  delete EQ;

  delete Properties;
}

void CMixer::SetMute(int track, bool isMute)
{
  Mute[track] = isMute;
}

void CMixer::SetSolo(int track, bool isSolo)
{
  if(SoloTrack == track)
  {
    if(isSolo)
      return;
    else
      SoloTrack = -1;
  }
  else
  {
    if(isSolo)
    {
      if(SoloTrack != -1)
#ifndef DW_HEADLESS
        if(GUI)
          GUI->Track[SoloTrack]->SetSolo(false);
#endif
      SoloTrack = track;
    }
  }
}

HWND DARKCALL CMixer::CreateGUI(HWND parent)
{
#ifndef DW_HEADLESS
  if(GUI)
  {
    delete GUI;
    GUI = 0;
  }

  if(GUI == 0)
    GUI = new CMixerGUI(parent, 0, this, Graphics, Tracks, AUXTracks);
  return GUI->Handle;
#else
  return 0;
#endif
}

void DARKCALL CMixer::OnIdle()
{
#ifndef DW_HEADLESS
  int i;

  if(GUI)
  {
    for(i = 0; i < (Tracks + AUXTracks); i++)
      GUI->Track[i]->SetVolume(Max[i * 2], Max[(i * 2) + 1]);
  }
#else
#endif
}

void DARKCALL CMixer::GetPropertyValueDisplay(int index, double value, char *text)
{
  if((index >= 0) && (index < PropertyCount))
  {
    if(index < (Tracks * TRACK_PROPERTY_COUNT))
    {
      int track;
      int prop;

      track = (index / TRACK_PROPERTY_COUNT);
      prop  = index - (track * TRACK_PROPERTY_COUNT);

      switch(prop)
      {
        case TRACK_PROPERTY_LEVEL:
        case TRACK_PROPERTY_BALANCE:
        case TRACK_PROPERTY_LOW:
        case TRACK_PROPERTY_MID:
        case TRACK_PROPERTY_HIGH:
        case TRACK_PROPERTY_AUX1:
        case TRACK_PROPERTY_AUX2:
          sprintf(text, "%i%%", (int)(value * 100.0f));
      }
    }
    else
    {
      int track;
      int prop;

      index -= (Tracks * TRACK_PROPERTY_COUNT);

      track = (index / TRACK_PROPERTY_COUNT_AUX);
      prop  = index - (track * TRACK_PROPERTY_COUNT_AUX);

      switch(prop)
      {
        case TRACK_PROPERTY_LEVEL:
        case TRACK_PROPERTY_BALANCE:
        case TRACK_PROPERTY_LOW:
        case TRACK_PROPERTY_MID:
        case TRACK_PROPERTY_HIGH:
          sprintf(text, "%i%%", (int)(value * 100.0f));
      }

    }
  }
  else
    text[0] = 0;
}

void DARKCALL CMixer::GetPropertyDisplay(int index, char *text)
{
  GetPropertyValueDisplay(index, Properties[index], text);
}
void DARKCALL CMixer::GetPropertyName(int index, char *text)
{
  if((index >= 0) && (index < PropertyCount))
  {
    if(index < (Tracks * TRACK_PROPERTY_COUNT))
    {
      int track;
      int prop;

      track = (index / TRACK_PROPERTY_COUNT);
      prop  = index - (track * TRACK_PROPERTY_COUNT);

      switch(prop)
      {
        case TRACK_PROPERTY_LEVEL:
          sprintf(text, "Track %i Level", track + 1);
          break;
        case TRACK_PROPERTY_BALANCE:
          sprintf(text, "Track %i Balance", track + 1);
          break;
        case TRACK_PROPERTY_LOW:
          sprintf(text, "Track %i Low", track + 1);
          break;
        case TRACK_PROPERTY_MID:
          sprintf(text, "Track %i Mid", track + 1);
          break;
        case TRACK_PROPERTY_HIGH:
          sprintf(text, "Track %i High", track + 1);
          break;
        case TRACK_PROPERTY_AUX1:
          sprintf(text, "Track %i AUX1", track + 1);
          break;
        case TRACK_PROPERTY_AUX2:
          sprintf(text, "Track %i AUX2", track + 1);
          break;
      }
    }
    else
    {
      int track;
      int prop;

      index -= (Tracks * TRACK_PROPERTY_COUNT);

      track = (index / TRACK_PROPERTY_COUNT_AUX);
      prop  = index - (track * TRACK_PROPERTY_COUNT_AUX);

      switch(prop)
      {
        case TRACK_PROPERTY_LEVEL:
          sprintf(text, "AUX %i Level", track + 1);
          break;
        case TRACK_PROPERTY_BALANCE:
          sprintf(text, "AUX %i Balance", track + 1);
          break;
        case TRACK_PROPERTY_LOW:
          sprintf(text, "AUX %i Low", track + 1);
          break;
        case TRACK_PROPERTY_MID:
          sprintf(text, "AUX %i Mid", track + 1);
          break;
        case TRACK_PROPERTY_HIGH:
          sprintf(text, "AUX %i High", track + 1);
          break;
      }
    }
  }
  else
    text[0] = 0;
}

double DARKCALL CMixer::GetProperty(int index)
{
  if(index >= 0 && index < PropertyCount)
  {
    return Properties[index];
  }
  else
    return 0.0f;
}

void DARKCALL CMixer::SetProperty(int index, double value)
{
  int   track;
  int   prop;
  int   eq;
  double low, mid, high;

  if(index >= 0 && index < PropertyCount)
  {
    Properties[index] = value;
        if(index < (Tracks * TRACK_PROPERTY_COUNT))
    {
      track = (index / TRACK_PROPERTY_COUNT);
      prop  = index - (track * TRACK_PROPERTY_COUNT);

      switch(prop)
      {
        case TRACK_PROPERTY_LOW:
        case TRACK_PROPERTY_MID:
        case TRACK_PROPERTY_HIGH:
          eq   = track;
          low  = Properties[(track * TRACK_PROPERTY_COUNT) + TRACK_PROPERTY_LOW];
          mid  = Properties[(track * TRACK_PROPERTY_COUNT) + TRACK_PROPERTY_MID];
          high = Properties[(track * TRACK_PROPERTY_COUNT) + TRACK_PROPERTY_HIGH];
          EQ[eq][0]->SetGain(low, mid, high);
          EQ[eq][1]->SetGain(low, mid, high);
          break;
      }
    }
    else
    {
      index -= (Tracks * TRACK_PROPERTY_COUNT);

      track = (index / TRACK_PROPERTY_COUNT_AUX);
      prop  = index - (track * TRACK_PROPERTY_COUNT_AUX);

      switch(prop)
      {
        case TRACK_PROPERTY_LOW:
        case TRACK_PROPERTY_MID:
        case TRACK_PROPERTY_HIGH:
          eq   = Tracks + track;
          low  = Properties[(Tracks * TRACK_PROPERTY_COUNT) + (track * TRACK_PROPERTY_COUNT_AUX) + TRACK_PROPERTY_LOW];
          mid  = Properties[(Tracks * TRACK_PROPERTY_COUNT) + (track * TRACK_PROPERTY_COUNT_AUX) + TRACK_PROPERTY_MID];
          high = Properties[(Tracks * TRACK_PROPERTY_COUNT) + (track * TRACK_PROPERTY_COUNT_AUX) + TRACK_PROPERTY_HIGH];
          EQ[eq][0]->SetGain(low, mid, high);
          EQ[eq][1]->SetGain(low, mid, high);
          break;
      }

      index += (Tracks * TRACK_PROPERTY_COUNT);   
    }
#ifndef DW_HEADLESS
    if(GUI)
      GUI->SetProperty(index, value);
#endif
  }
}

void DARKCALL CMixer::Work()
{
  int i;
  double level, balance;
  double mixLeft, mixRight, aux1, aux2;
  bool  mute;

  Ports[0]->AudioBuffer->Clear();
  for(i = Tracks + 1; i < Tracks + 1 + (2 * AUXTracks); i++)
    Ports[i]->AudioBuffer->Clear();

  for(i = 0; i < Tracks; i++)
  {
    mute = Mute[i];
    if(SoloTrack != -1)
      if(SoloTrack != i)
        mute = true;

    level    = mute ? 0.0 : dBToPowerRatio((60.0 * Properties[(i * TRACK_PROPERTY_COUNT) + TRACK_PROPERTY_LEVEL]) - 48.0);
    balance  = Properties[(i * TRACK_PROPERTY_COUNT) + TRACK_PROPERTY_BALANCE];
    mixLeft  = (1.0 - balance) * level;
    mixRight = balance * level;

    aux1 = mute ? 0.0 : Properties[(i * TRACK_PROPERTY_COUNT) + TRACK_PROPERTY_AUX1];
    aux2 = mute ? 0.0 : Properties[(i * TRACK_PROPERTY_COUNT) + TRACK_PROPERTY_AUX2];
    EQ[i][0]->Work(Ports[i + 1]->AudioBuffer->LeftData, Ports[0]->AudioBuffer->LeftData, Ports[Tracks + 1]->AudioBuffer->LeftData, Ports[Tracks + 3]->AudioBuffer->LeftData, BlockSamples, mixLeft, aux1, aux2, &Max[(i * 2) + 0]);
    EQ[i][1]->Work(Ports[i + 1]->AudioBuffer->RightData, Ports[0]->AudioBuffer->RightData, Ports[Tracks + 1]->AudioBuffer->RightData, Ports[Tracks + 3]->AudioBuffer->RightData, BlockSamples, mixRight, aux1, aux2, &Max[(i * 2) + 1]);    
  }
}

void DARKCALL CMixer::WorkAUX()
{
  int   i;
  double level, balance;
  double mixLeft, mixRight;
  bool  mute;

  for(i = 0; i < AUXTracks; i++)
  {
    mute = Mute[Tracks + i];
    if(SoloTrack != -1)
      if(SoloTrack != Tracks + i)
        mute = true;

    level    = mute ? 0.0 : dBToPowerRatio((60.0 * Properties[(TRACK_PROPERTY_COUNT * Tracks) + (TRACK_PROPERTY_COUNT_AUX * i) + TRACK_PROPERTY_LEVEL]) - 48.0);
    balance  = Properties[(TRACK_PROPERTY_COUNT * Tracks) + (TRACK_PROPERTY_COUNT_AUX * i) + TRACK_PROPERTY_BALANCE];
    mixLeft  = (1.0 - balance) * level;
    mixRight = balance * level;

    EQ[Tracks + i][0]->Work(Ports[Tracks + 2 + (2 * i)]->AudioBuffer->LeftData, Ports[0]->AudioBuffer->LeftData, BlockSamples, mixLeft, &Max[(Tracks * 2) + (i * 2) + 0]); 
    EQ[Tracks + i][1]->Work(Ports[Tracks + 2 + (2 * i)]->AudioBuffer->RightData, Ports[0]->AudioBuffer->RightData, BlockSamples, mixRight, &Max[(Tracks * 2) + (i * 2) + 1]); 
  }
}

void DARKCALL CMixer::SetSampleRate(int sampleRate)
{
  int i;

  for(i = 0; i < Tracks + AUXTracks; i++)
  {
    EQ[i][0]->SetSampleRate(EQ_DEFAULT_LOWFREQ, EQ_DEFAULT_HIGHFREQ, (double)sampleRate);
    EQ[i][1]->SetSampleRate(EQ_DEFAULT_LOWFREQ, EQ_DEFAULT_HIGHFREQ, (double)sampleRate);
  }
}

void DARKCALL CMixer::SetBlockSamples(int blockSamples)
{
  BlockSamples = blockSamples;
}

bool DARKCALL CMixer::IsSynth()
{
  return false;
}

#ifndef DW_HEADLESS
CMixerHeader::CMixerHeader(CControl *parent, SMixerGraphics *graphics)
: CLiteContainer(parent, graphics->Header->GetWidth(), graphics->Header->GetHeight(), graphics->Header)
{
}

CMixerHeader::~CMixerHeader()
{
}

CMixerGUI::CMixerGUI(HWND parent, CColorManager *colorManager, CMixer *machine, SMixerGraphics *graphics, int tracks, int auxTracks)
: CControl(parent, 0, "Mixer", 0, 0, graphics->Panel->GetWidth() * (tracks + auxTracks), graphics->Panel->GetHeight() + graphics->Header->GetHeight(), false)
{
  int i, firstProperty;
  bool isAUX;

  Header = new CMixerHeader(this, graphics);
  Header->Show();
  
  Border = false;
  Tracks = tracks;
  AUXTracks = auxTracks;
  Track = new CMixerTrackGUI*[Tracks + AUXTracks];

  for(firstProperty = i = 0; i < Tracks + AUXTracks; i++)
  {
    isAUX = i < Tracks ? false : true;
    Track[i] = new CMixerTrackGUI(Handle, colorManager, machine, graphics, i, isAUX, firstProperty, isAUX ? i - Tracks : i);
    Track[i]->Move(i * graphics->Panel->GetWidth(), graphics->Header->GetHeight());
    Track[i]->Show();
    firstProperty += isAUX ? TRACK_PROPERTY_COUNT_AUX : TRACK_PROPERTY_COUNT;
  }
}

CMixerGUI::~CMixerGUI()
{
  int i;

  delete Header;

  for(i = 0; i < Tracks + AUXTracks; i++)
    delete Track[i];
  delete Track;
}

void CMixerGUI::SetProperty(int index, double value)
{
  int track;

  if(index < Tracks * TRACK_PROPERTY_COUNT)
  {
    track = index / TRACK_PROPERTY_COUNT;
    Track[track]->SetProperty(index - (track * TRACK_PROPERTY_COUNT), value);
  }
  else
  {
    index -= Tracks * TRACK_PROPERTY_COUNT;
    track = Tracks + (index / TRACK_PROPERTY_COUNT_AUX);
    Track[track]->SetProperty(index - ((track - Tracks) * TRACK_PROPERTY_COUNT_AUX), value);
  }
}

CMixerTrackGUI::CMixerTrackGUI(HWND parent, CColorManager *colorManager, CMixer *machine, SMixerGraphics *graphics, int track, bool isAUX, int firstProperty, int auxTrack)
: CLiteContainer(parent, graphics->Panel->GetWidth(), graphics->Panel->GetHeight(), graphics->Panel)
{
  const int auxY  = 24;
  const int eqY   = 66;
  const int eqGap = 30;
  int x;

  Machine = machine;
  Graphics = graphics;
  IsAUX = isAUX;
  Track = track;
  AUXTrack = auxTrack;

  x = 4;
  LowKnob = new CLiteKnob(this, machine, firstProperty + TRACK_PROPERTY_LOW, graphics->Knob, x, eqY, 38);
  x += eqGap;
  MidKnob = new CLiteKnob(this, machine, firstProperty + TRACK_PROPERTY_MID, graphics->Knob, x, eqY, 38);
  x += eqGap;
  HighKnob = new CLiteKnob(this, machine, firstProperty + TRACK_PROPERTY_HIGH, graphics->Knob, x, eqY, 38);
  x += eqGap;
  LowKnob->Show();
  MidKnob->Show();
  HighKnob->Show();

  if(!isAUX)
  {
    x = 20;
    AUX1Knob = new CLiteKnob(this, machine, firstProperty + TRACK_PROPERTY_AUX1, graphics->Knob, x, auxY, 38);
    x += eqGap;
    AUX2Knob = new CLiteKnob(this, machine, firstProperty + TRACK_PROPERTY_AUX2, graphics->Knob, x, auxY, 38);
    
    AUX1Knob->Show();
    AUX2Knob->Show();
  }
  else
  {
    AUX1Knob = 0;
    AUX2Knob = 0;
  }

  MuteButton = new CLiteButton(this, 4,  4, graphics->Mute->GetWidth(), graphics->Mute->GetHeight(), graphics->Mute, graphics->MuteHover, graphics->MuteClick);
  MuteButton->Show();
  MuteButton->Sticky = true;
  if(!isAUX)
  {
    SoloButton = new CLiteButton(this, 48, 4, graphics->Solo->GetWidth(), graphics->Solo->GetHeight(), graphics->Solo, graphics->SoloHover, graphics->SoloClick);
    SoloButton->Show();
    SoloButton->Sticky = true;
  }
  else
  {
    SoloButton = 0;
  }

  LevelSlider   = new CLiteSlider(this, 36, 112, 78, true,  graphics->Level,   machine, firstProperty + TRACK_PROPERTY_LEVEL);
  BalanceSlider = new CLiteSlider(this, 27, 188, 49, false, graphics->Balance, machine, firstProperty + TRACK_PROPERTY_BALANCE);
  LevelSlider->Show();
  BalanceSlider->Show();

  LevelLeft  = new CLiteMeter(this, 10, 114, 12, 84);
  LevelRight = new CLiteMeter(this, 74, 114, 12, 84);
  LevelLeft->Show();
  LevelRight->Show();

  Render();
}

void CMixerTrackGUI::OnButtonClick(CLiteButton *button)
{
  if(button == SoloButton)
  {
    Machine->SetSolo(Track, SoloButton->State == STATE_CLICK);
  }

  if(button == MuteButton)
  {
    Machine->SetMute(Track, MuteButton->State == STATE_CLICK);
  }
}

void CMixerTrackGUI::SetSolo(bool isSolo)
{
  if(SoloButton)
    SoloButton->SetState(isSolo ? STATE_CLICK : STATE_IDLE);
}

void CMixerTrackGUI::DrawBackground(int x1, int y1, int x2, int y2, int w, int h)
{
  char text[32];

  Background->Draw(RenderBuffer, x1, y1, x1, y1, w, h);
  Graphics->BitFont->DrawText(RenderBuffer, "LOW",  -1 + 9,  4 + 96, 64, 32);
  Graphics->BitFont->DrawText(RenderBuffer, "MID",  -1 + 39, 4 + 96, 64, 24);
  Graphics->BitFont->DrawText(RenderBuffer, "HIGH", -1 + 66, 4 + 96, 64, 24);
  if(!IsAUX)
  {
    Graphics->BitFont->DrawText(RenderBuffer, "AUX1", -1 + 21, 2 + 56, 64, 24);
    Graphics->BitFont->DrawText(RenderBuffer, "AUX2", -1 + 52, 2 + 56, 64, 24);
  }

  if(IsAUX)
    sprintf(text, "AUX %i", AUXTrack + 1);
  else
    sprintf(text, "TRACK %i", AUXTrack + 1);
  Graphics->BitFont->DrawText(RenderBuffer, text, IsAUX ? 33 : 27, 210, 64, 24);
}

CMixerTrackGUI::~CMixerTrackGUI()
{
  delete LowKnob;
  delete MidKnob;
  delete HighKnob;
  if(AUX1Knob)
    delete AUX1Knob;
  if(AUX2Knob)
    delete AUX2Knob;
  delete MuteButton;
  if(SoloButton)
    delete SoloButton;
  delete LevelLeft;
  delete LevelRight;
}

void CMixerTrackGUI::SetProperty(int index, double value)
{
  switch(index)
  {
    case TRACK_PROPERTY_LOW:
      LowKnob->SetValue(value);
      break;
    case TRACK_PROPERTY_MID:
      MidKnob->SetValue(value);
      break;
    case TRACK_PROPERTY_HIGH:
      HighKnob->SetValue(value);
      break;
    case TRACK_PROPERTY_AUX1:
      if(AUX1Knob)
        AUX1Knob->SetValue(value);
      break;
    case TRACK_PROPERTY_AUX2:
      if(AUX2Knob)
        AUX2Knob->SetValue(value);
      break;
    case TRACK_PROPERTY_LEVEL:
      LevelSlider->SetValue(value);
      break;
    case TRACK_PROPERTY_BALANCE:
      BalanceSlider->SetValue(value);
      break;
  }
}

void CMixerTrackGUI::SetVolume(double left, double right)
{
  LevelLeft->SetVolume(left);
  LevelRight->SetVolume(right);
}

#endif
CMixerInfo::CMixerInfo(CHost *host)
{
  Host = host;
  Name = "ES Mixer";
  UniqueID = "DarkPlug::ES Mixer-Stereo-8-2";
  Loaded = false;
}

CMixerInfo::~CMixerInfo()
{
  if(Loaded)
  {
    delete Graphics.Panel;
    delete Graphics.Knob;
    delete Graphics.Solo;
    delete Graphics.SoloHover;
    delete Graphics.SoloClick;
    delete Graphics.Mute;
    delete Graphics.MuteHover;
    delete Graphics.MuteClick;
    delete Graphics.Level;
    delete Graphics.Balance;
    delete Graphics.BitFont;
    delete Graphics.Font;
    delete Graphics.Header;
   }
}

CMachine* DARKCALL CMixerInfo::CreateMachine()
{
  if(!Loaded)
  {
    Graphics.Panel = new CFrameBuffer(IDR_MIXER_PANEL);
    Graphics.Knob = new CFrameBuffer(IDR_MIXER_KNOB);
    Graphics.Mute = new CFrameBuffer(IDR_MUTE);
    Graphics.MuteHover = new CFrameBuffer(IDR_MUTEHOVER);
    Graphics.MuteClick = new CFrameBuffer(IDR_MUTECLICKED);
    Graphics.Solo = new CFrameBuffer(IDR_SOLO);
    Graphics.SoloHover = new CFrameBuffer(IDR_SOLOHOVER);
    Graphics.SoloClick = new CFrameBuffer(IDR_SOLOCLICKED);
    Graphics.Level = new CFrameBuffer(IDR_MIXER_LEVEL);
    Graphics.Balance = new CFrameBuffer(IDR_MIXER_BALANCE);
    Graphics.Font = new CFrameBuffer(IDR_MIXER_FONT);
    Graphics.Header = new CFrameBuffer(IDR_MIXER_HEADER);
    Graphics.BitFont = new CBitFont(Graphics.Font, 8, 6, 8, 9, 6, 48, 90); 
    Loaded = true;
  }
  return new CMixer(Host, &Graphics, 8, 2);
}

#ifndef DW_HEADLESS
CLiteMeter::CLiteMeter(CLiteContainer *parent, int x, int y, int width, int height)
: CLiteControl(parent, x, y, width, height)
{
  Volume = 0.0f;
}

CLiteMeter::~CLiteMeter()
{
}

void CLiteMeter::Draw(CFrameBuffer *frameBuffer)
{
  const double maxDB    =  16.0f;
  const double dBYellow = -16.0f;
  const double dBRed    =   1.0f;
  const double dBStep   =   3.0f;

  double dBY;

  double dB = PowerRatioTodB(Volume);
     
  const COLORREF red    = RGB(0xFF, 0x00, 0x00);
  const COLORREF yellow = RGB(0xFF, 0xFF, 0x00);
  const COLORREF green  = RGB(0x00, 0xFF, 0x00);

  COLORREF color;
 
  dBY = maxDB;
  for(int y = 0; y < (Height - 4); y += 4)
  {
    if(dB >= dBY)
    {
      color = dBY >= dBRed ? red : dBY >= dBYellow ? yellow : green; 
      frameBuffer->Fill(color, X + 2, Y + y, Width - 4, 2);
    }
    else
    {
      frameBuffer->Fill(0,     X + 2, Y + y, Width - 4, 2);
    }
    dBY -= dBStep;
  }

  // frameBuffer->Fill(RGB((char)(255.0f * power), 255 - (char)(255.0f * power), 0), X, Y + (Height - height), Width, height);
}

void CLiteMeter::SetVolume(double volume)
{
  Volume = volume;
  Parent->Render(X, Y, Width, Height);
}
#endif
