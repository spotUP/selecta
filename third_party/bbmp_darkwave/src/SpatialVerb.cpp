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



#include "SpatialVerb.h"
#include "Core.h"
#include "RoomView.h"
#include "Util.h"
#include "Resource.h"

#include <stdio.h>

#define EARLY_TIME      00.075
#define MIN_ROOM_WIDTH  10.0
#define MAX_ROOM_WIDTH  30.0
#define MIN_ROOM_LENGTH MIN_ROOM_WIDTH
#define MAX_ROOM_LENGTH MAX_ROOM_WIDTH
#define MIN_WALL_ABSORB 0.6
#define MAX_WALL_ABSORB 0.001
#define FADE_SAMPLES    128
#define MIN_GAIN_DB     (-40.0)
#define MAX_GAIN_DB     (20.0)

#define PROPERTY_SRC_LEFT_X     0
#define PROPERTY_SRC_LEFT_Y     1
#define PROPERTY_SRC_RIGHT_X    2
#define PROPERTY_SRC_RIGHT_Y    3
#define PROPERTY_DST_LEFT_X     4
#define PROPERTY_DST_LEFT_Y     5
#define PROPERTY_DST_RIGHT_X    6
#define PROPERTY_DST_RIGHT_Y    7 
#define PROPERTY_ROOM_WIDTH     8
#define PROPERTY_ROOM_LENGTH    9
#define PROPERTY_WALL_ABSORB    10
#define PROPERTY_FDN_FEEDBACK   11
#define PROPERTY_FDN_CUTOFF     12
#define PROPERTY_FDN_GAIN       13

#define PROPERTY_COUNT          14

#define PROPERTY_FORMAT_PERCENT 0
#define PROPERTY_FORMAT_METERS  1
#define PROPERTY_FORMAT_CUTOFF  2
#define PROPERTY_FORMAT_GAIN    3

#define PROPERTY_FORMAT_COUNT   4

const double SpatialVerbPropertyDefaults[PROPERTY_COUNT] =
{
  0.2, // Src Left X
  0.2, // Src Left Y
  0.8, // Src Right X
  0.2, // Src Right Y
  0.4, // Dst Left X
  0.8, // Dst Left Y
  0.6, // Dst Right X
  0.8, // Dst Right Y
  0.0, // Room Width
  0.0, // Room Length
  0.5, // Wall Absorb
  0.5, // FDN Feedback
  1.0, // FDN Cutoff
  0.7  // FDN Gain
};

const char* SpatialVerbPropertyNames[PROPERTY_COUNT] =
{
  "Src Left X",
  "Src Left Y",
  "Src Right X",
  "Src Right Y",
  "Dst Left X",
  "Dst Left Y",
  "Dst Right X",
  "Dst Right Y",
  "Room Width",
  "Room Height",
  "Wall Absorb",
  "Feedback",
  "Cutoff",
  "Gain"
};

const int SpatialVerbPropertyFormats[PROPERTY_COUNT] =
{
  PROPERTY_FORMAT_PERCENT, // Src Left X
  PROPERTY_FORMAT_PERCENT, // Src Left Y
  PROPERTY_FORMAT_PERCENT, // Src Right X
  PROPERTY_FORMAT_PERCENT, // Src Right Y
  PROPERTY_FORMAT_PERCENT, // Dst Left X
  PROPERTY_FORMAT_PERCENT, // Dst Left Y
  PROPERTY_FORMAT_PERCENT, // Dst Right X
  PROPERTY_FORMAT_PERCENT, // Dst Right Y
  PROPERTY_FORMAT_METERS,  // Room Width
  PROPERTY_FORMAT_METERS,  // Room Height
  PROPERTY_FORMAT_PERCENT, // Wall Absorb
  PROPERTY_FORMAT_PERCENT, // FDN Feedback
  PROPERTY_FORMAT_CUTOFF,  // FDN Cutoff
  PROPERTY_FORMAT_GAIN     // FDN Gain
};

int SpatialVerbDelayLengths[4] =
{
  601,
  691,
  773,
  839
};

// CSpatialVerbGUI

#define KNOB_COUNT   4
#define KNOB_Y       158
#define KNOB_HEIGHT  38
#define SLIDER_COUNT 3
#define SLIDER_SIZE  81

const int SpatialVerbKnobX[KNOB_COUNT] =
{
  180,
  264,
  332,
  392
};

const int SpatialVerbKnobProperties[KNOB_COUNT] =
{
  PROPERTY_WALL_ABSORB,
  PROPERTY_FDN_FEEDBACK,
  PROPERTY_FDN_CUTOFF,
  PROPERTY_FDN_GAIN
};

#ifndef DW_HEADLESS
CSpatialVerbGUI::CSpatialVerbGUI(HWND parent, CColorManager *colorManager, CSpatialVerb *machine, SSpatialVerbGraphics *graphics)
: CLiteContainer(parent, graphics->Background->GetWidth(), graphics->Background->GetHeight(), graphics->Background)
{
  int i;
  Graphics   = graphics; 

  LCDFont = new CBitFont(Graphics->LCDFont, 8, 8, 16, 13, 8, 33, 96); 

  Knobs = new CLiteKnob*[KNOB_COUNT];
  for(i = 0; i < KNOB_COUNT; i++)
    Knobs[i] = new CLiteKnob(this, machine, SpatialVerbKnobProperties[i], Graphics->Knob, SpatialVerbKnobX[i], KNOB_Y, KNOB_HEIGHT);

  Sliders = new CLiteSlider*[SLIDER_COUNT];

  Sliders[0] = new CLiteSlider(this, 32, 56,  SLIDER_SIZE, false, Graphics->SliderH, machine, PROPERTY_ROOM_WIDTH);
  Sliders[1] = new CLiteSlider(this, 16, 72,  SLIDER_SIZE, true,  Graphics->SliderV, machine, PROPERTY_ROOM_LENGTH);
  Sliders[2] = new CLiteSlider(this, 32, 160, SLIDER_SIZE, false, Graphics->SliderH, 0, 0);

  LCDText = new CLiteLCDText(this, 272 + 2, 48 + 2, 144 - 4, 16 - 4, LCDFont, 0.6f);

  RoomView = new CRoomView(this, 32 + 4, 72 + 4, 80 - 8, 80 - 8, 0.6f, Graphics->Nodes, machine);

  EarlyImpulseView = new CImpulseView(this, 128 + 4, 72 + 4, 96 - 8, 80 - 8, 60.0f, 0.6f, false);
  LateImpulseView = new CImpulseView(this, 240 + 4, 72 + 4, 176 - 8, 80 - 8, 60.0f, 0.6f, true);
    
  SpatialVerb = machine;

  Render();
  Show();
}

CSpatialVerbGUI::~CSpatialVerbGUI()
{
  int i;

  delete LCDFont;
  delete EarlyImpulseView;
  delete LateImpulseView;
  delete RoomView;
  delete LCDText;

  for(i = 0; i < KNOB_COUNT; i++)
    delete Knobs[i];
  delete Knobs;

  for(i = 0; i < SLIDER_COUNT; i++)
    delete Sliders[i];
  delete Sliders;
}

void CSpatialVerbGUI::OnButtonClick(CLiteButton *button)
{
  if(!(button->Visible))
    return;
}

void CSpatialVerbGUI::OnLiteSliderDown(CLiteSlider *slider, double value)
{
  char text[64];

  sprintf(text, "View Time %.2fms", value * EARLY_TIME * 1000.0f);
  LCDText->SetText(text);
}

void CSpatialVerbGUI::OnLiteSliderChange(CLiteSlider *slider, double value)
{
  char text[64];

  sprintf(text, "View Time %.0fms", value * EARLY_TIME * 1000.0f);
  LCDText->SetText(text);
  RoomView->SetTime(value);
}

void CSpatialVerbGUI::OnShowGUI()
{
  LCDText->SetText("Hello");
}

void CSpatialVerbGUI::SetLCDText(char *text)
{
  LCDText->SetText(text);
}

void CSpatialVerbGUI::UpdateRoom(CImageSource *leftImageSource, CImageSource *rightImageSource, SSize *roomSize, SPos *srcLeftPos, SPos *srcRightPos, SPos *dstLeftPos, SPos *dstRightPos, double totalTime)
{
  RoomView->Update(leftImageSource, rightImageSource, roomSize, srcLeftPos, srcRightPos, dstLeftPos, dstRightPos, totalTime);
}

void CSpatialVerbGUI::UpdateImpulse(int samples, CStereoFIR *leftFIR, CStereoFIR *rightFIR)
{
  double *buffer;
  int    i;
  double  value;

  buffer = new double[samples];
  for(i = 0; i < samples; i++)
  {
    value = 0.5f * (leftFIR->ImpulseResponse(i) + rightFIR->ImpulseResponse(i));
    buffer[i] = value;
  }
  EarlyImpulseView->Update(samples, buffer);
  delete buffer;
}

void CSpatialVerbGUI::UpdateImpulse2(int samples, CFDN *fdn, int earlySamples)
{
  double *buffer, left, right;
  int    i;
  CFDN  *clone;

  buffer = new double[samples + earlySamples];
  clone = fdn->Clone();

  for(i = 0; i < earlySamples + samples; i++)
  {
    if(i == 0)
      clone->Process(1.0, 1.0, &left, &right);
    else
      clone->Process(0.0, 0.0, &left, &right);
    buffer[i] = 0.5 * (fabs(left) + fabs(right));
  }

  LateImpulseView->Update(samples, buffer + earlySamples);

  delete buffer;
  delete clone;
}

void CSpatialVerbGUI::ShowToolTip(CLiteControl *control, char *text)
{
  LCDText->SetText(text);
}

void CSpatialVerbGUI::SetProperty(int index, double value)
{
  int i;

  for(i = 0; i < KNOB_COUNT; i++)
    if(SpatialVerbKnobProperties[i] == index)
      Knobs[i]->SetValue(value);

  if(index == PROPERTY_ROOM_WIDTH)
    Sliders[0]->SetValue(value);

  if(index == PROPERTY_ROOM_LENGTH)
    Sliders[1]->SetValue(value);
}

// CSpatialVerb

#endif
CSpatialVerb::CSpatialVerb(CHost *host, SSpatialVerbGraphics *graphics)
: CMachine(host)
{
  int    i;

  nSamples  = 0;

  GUI              = 0;
  LeftImageSource  = 0;
  RightImageSource = 0;
  Graphics         = graphics;

  SampleRate   = Host->GetSampleRate();
  BlockSamples = Host->GetBlockSamples();
  EarlyTime    = EARLY_TIME;
  EarlySamples = (int)(EarlyTime * SampleRate);
  LeftFIR      = new CStereoFIR(EarlySamples, true);
  RightFIR     = new CStereoFIR(EarlySamples, true); 

  PropertyCount = PROPERTY_COUNT;
  Properties     = new double[PROPERTY_COUNT];

  for(i = 0; i < PROPERTY_COUNT; i++)
    Properties[i] = SpatialVerbPropertyDefaults[i];

  Ports = new CPort*[2];
  Ports[0] = new CPort(host, this, PT_INPUT | PT_AUDIO | PT_STEREO, "Line In");
  Ports[1] = new CPort(host, this, PT_OUTPUT | PT_AUDIO | PT_STEREO, "Line Out");
  PortCount = 2;

  FDN = new CFDN(SampleRate, SpatialVerbDelayLengths, PropertyToFeedback(Properties[PROPERTY_FDN_FEEDBACK]), PropertyToGain(Properties[PROPERTY_FDN_GAIN]), Properties[PROPERTY_FDN_CUTOFF]);

  ModelRoom();
  ModelFIRs();

  NeedToUpdateGUI = NeedToUpdateRoom = NeedToUpdateFIRs = NeedToUpdateFDN = false;
}

void CSpatialVerb::SetSampleRate(int sampleRate)
{
  delete LeftFIR;
  delete RightFIR;
  delete FDN;

  SampleRate   = sampleRate;
  EarlyTime    = EARLY_TIME;
  EarlySamples = (int)(EarlyTime * SampleRate);
  LeftFIR      = new CStereoFIR(EarlySamples, true);
  RightFIR     = new CStereoFIR(EarlySamples, true); 
  FDN          = new CFDN(SampleRate, SpatialVerbDelayLengths, PropertyToFeedback(Properties[PROPERTY_FDN_FEEDBACK]), PropertyToGain(Properties[PROPERTY_FDN_GAIN]), Properties[PROPERTY_FDN_CUTOFF]);

  ModelRoom();
  ModelFIRs();
#ifndef DW_HEADLESS
  if(GUI)
  {
    UpdateGUIRoom();
    UpdateGUIImpulse();
    UpdateGUIImpulse2();
  }
#endif
}

void CSpatialVerb::SetBlockSamples(int blockSamples)
{
  BlockSamples = blockSamples;
}

void CSpatialVerb::OnShowGUI()
{
#ifndef DW_HEADLESS
  if(GUI != 0)
    GUI->OnShowGUI();
#else
#endif
}

CSpatialVerb::~CSpatialVerb()
{
  delete Ports[0];
  delete Ports[1];
  delete Ports;
#ifndef DW_HEADLESS
  if(GUI != 0)
    delete GUI;
#endif
  delete Properties;
  FreeRays();
  delete LeftFIR;
  delete RightFIR;
}

HWND CSpatialVerb::CreateGUI(HWND parent)
{
#ifndef DW_HEADLESS
  if(GUI != 0)
  {
    delete GUI;
	GUI = 0;
  }
  if(GUI == 0)
  {
    GUI = new CSpatialVerbGUI(parent, 0, this, Graphics);
    UpdateGUIRoom();
    UpdateGUIImpulse();
    UpdateGUIImpulse2();
  }
  return GUI->Handle;
#else
  return 0;
#endif
}

bool CSpatialVerb::GetGUIRect(short *x1, short *y1, short *x2, short *y2)
{
#ifndef DW_HEADLESS
  *x1 = 0;
  *x2 = Graphics->Background->GetWidth();
  *y1 = 0;
  *y2 = Graphics->Background->GetHeight();
  return true;
#else
  return false;
#endif
}


void CSpatialVerb::UpdateDstPos()
{
  DstLeftPos.x    = (0.1f + (Properties[PROPERTY_DST_LEFT_X]  * 0.8f)) * RoomSize.width;
  DstLeftPos.y    = (0.1f + (Properties[PROPERTY_DST_LEFT_Y]  * 0.8f)) * RoomSize.height;
  DstRightPos.x   = (0.1f + (Properties[PROPERTY_DST_RIGHT_X] * 0.8f)) * RoomSize.width;
  DstRightPos.y   = (0.1f + (Properties[PROPERTY_DST_RIGHT_Y] * 0.8f)) * RoomSize.height;
}

void CSpatialVerb::ModelRoom()
{
  FreeRays();
  
  RoomSize.width  = MIN_ROOM_WIDTH  + (Properties[PROPERTY_ROOM_WIDTH]  * (MAX_ROOM_WIDTH  - MIN_ROOM_WIDTH));
  RoomSize.height = MIN_ROOM_LENGTH + (Properties[PROPERTY_ROOM_LENGTH] * (MAX_ROOM_LENGTH - MIN_ROOM_LENGTH));
  WallAbsorb      = MIN_WALL_ABSORB + (Properties[PROPERTY_WALL_ABSORB] * (MAX_WALL_ABSORB - MIN_WALL_ABSORB));
  
  SrcLeftPos.x    = (0.1f + (Properties[PROPERTY_SRC_LEFT_X]  * 0.8f)) * RoomSize.width;
  SrcLeftPos.y    = (0.1f + (Properties[PROPERTY_SRC_LEFT_Y]  * 0.8f)) * RoomSize.height;
  SrcRightPos.x   = (0.1f + (Properties[PROPERTY_SRC_RIGHT_X] * 0.8f)) * RoomSize.width;
  SrcRightPos.y   = (0.1f + (Properties[PROPERTY_SRC_RIGHT_Y] * 0.8f)) * RoomSize.height;
  
  UpdateDstPos();

  LeftImageSource = new CImageSource(0, &RoomSize, &SrcLeftPos, 1.0f, WallAbsorb, EarlyTime);
  RightImageSource = new CImageSource(0, &RoomSize, &SrcRightPos, 1.0f, WallAbsorb, EarlyTime);
  
  UpdateGUIRoom();
}

void CSpatialVerb::ModelFIRs()
{
  int    i, fadeSamples;
  double  t1, t2, tinc;
  double *leftIR, *rightIR;

  fadeSamples = FADE_SAMPLES;
  if(fadeSamples > BlockSamples)
    fadeSamples = BlockSamples;

  t1   = 0.0f;
  tinc = EarlyTime / EarlySamples;
  t2   = tinc;

  UpdateDstPos();

  leftIR  = new double[EarlySamples];
  rightIR = new double[EarlySamples];

  t1 = 0.0f;
  t2 = tinc;
  for(i = 0; i < EarlySamples; i++)
  {
    leftIR[i]  = LeftImageSource->Converge(0, &DstLeftPos, t1, t2);
    rightIR[i] = LeftImageSource->Converge(0, &DstRightPos, t1, t2);
    t1 = t2;
    t2 += tinc;
  }
  LeftFIR->SetImpulseResponse(leftIR, rightIR, fadeSamples);

  t1 = 0.0f;
  t2 = tinc;
  for(i = 0; i < EarlySamples; i++)
  {
    leftIR[i]  = RightImageSource->Converge(0, &DstLeftPos, t1, t2);
    rightIR[i] = RightImageSource->Converge(0, &DstRightPos, t1, t2);
    t1 = t2;
    t2 += tinc;
  }
  RightFIR->SetImpulseResponse(leftIR, rightIR, fadeSamples);

  delete leftIR;
  delete rightIR;

  UpdateGUIImpulse();
}

void CSpatialVerb::UpdateGUIImpulse()
{
#ifndef DW_HEADLESS
  if(GUI != 0)
  {
    GUI->Lock();
    GUI->UpdateImpulse(EarlySamples, LeftFIR, RightFIR);
    GUI->Release();
  }
#else
#endif
}

void CSpatialVerb::UpdateGUIRoom()
{
#ifndef DW_HEADLESS
  if(GUI != 0)
  {
    UpdateDstPos();
    GUI->Lock();
    GUI->UpdateRoom(LeftImageSource, RightImageSource, &RoomSize, &SrcLeftPos, &SrcRightPos, &DstLeftPos, &DstRightPos, EarlyTime);
    GUI->Release();
  }
#else
#endif
}

void CSpatialVerb::OnIdle()
{
#ifdef COREMACHINES
  Lock();
  Update();
  Release();
#endif
}

void CSpatialVerb::Work()
{
  int    i, delayOffset;
  double *pInLeft, *pInRight, *pOutLeft, *pOutRight;
  double  inLeft, inRight, outLeft, outRight, outLeft2, outRight2, outLeft3, outRight3, delayLeft, delayRight;
  
  Lock();

  Update();

  pInLeft   = Ports[0]->AudioBuffer->LeftData;
  pInRight  = Ports[0]->AudioBuffer->RightData;
  pOutLeft  = Ports[1]->AudioBuffer->LeftData;
  pOutRight = Ports[1]->AudioBuffer->RightData;

  delayOffset = -SpatialVerbDelayLengths[0];

  for(i = 0; i < BlockSamples; i++)
  {
    inLeft  = *(pInLeft++);
    inRight = *(pInRight++);
	  
    outLeft = outRight = 0.0;

    LeftFIR->Process(inLeft, &outLeft, &outRight, &delayLeft, delayOffset);
    
	  outLeft2 = outRight2 = 0.0f;
	  RightFIR->Process(inRight, &outLeft2, &outRight2, &delayRight, delayOffset);

    FDN->Process(delayLeft, delayRight, &outLeft3, &outRight3);

    *(pOutLeft++)  = 0.333 * (outLeft  + outLeft2  + outLeft3);
    *(pOutRight++) = 0.333 * (outRight + outRight2 + outRight3);
  }

  Release();
}

double CSpatialVerb::GetProperty(int index)
{
  if(index >= 0 && index < PROPERTY_COUNT)
    return Properties[index];
  else
    return 0.0;
}

void CSpatialVerb::MoveNode(int node, SPos *pos)
{
#ifndef DW_HEADLESS
  SPos *nodePos;
  double x, y;
  int   xProp, yProp;

  switch(node)
  {
    case NODE_SRCLEFT:
      nodePos = &SrcLeftPos;
      break;
    case NODE_SRCRIGHT:
      nodePos = &SrcRightPos;
      break;
    case NODE_DSTLEFT:
      nodePos = &DstLeftPos;
      break;
    case NODE_DSTRIGHT:
      nodePos = &DstRightPos;
      break;
    default:
      return;
  }

  // 0.1 + prop * 0.8 [0.1 - 0.9]

  x = ((pos->x / RoomSize.width)  / 0.8) - 0.1;
  y = ((pos->y / RoomSize.height) / 0.8) - 0.1;

  if(x < 0.0)
    x = 0.0;
  else if(x > 1.0)
    x = 1.0;

  if(y < 0.0)
    y = 0.0;
  else if(y > 1.0)
    y = 1.0;

  switch(node)
  {
    case NODE_SRCLEFT:
      xProp = PROPERTY_SRC_LEFT_X;
      yProp = PROPERTY_SRC_LEFT_Y;
      break;

    case NODE_SRCRIGHT:
      xProp = PROPERTY_SRC_RIGHT_X;
      yProp = PROPERTY_SRC_RIGHT_Y;
      break;

    case NODE_DSTLEFT:
      xProp = PROPERTY_DST_LEFT_X;
      yProp = PROPERTY_DST_LEFT_Y;
      break;

    case NODE_DSTRIGHT:
      xProp = PROPERTY_DST_RIGHT_X;
      yProp = PROPERTY_DST_RIGHT_Y;
      break;
  
    default:
      return;
  }

  if(Properties[xProp] != x || Properties[yProp] != y)
  {
    char text[1024];

    text[0] = 0;

    switch(node)
    {
      case NODE_SRCLEFT:
        sprintf(text, "Src L %.0fM,%.0fM", pos->x, pos->y);
        break;

      case NODE_SRCRIGHT:
        sprintf(text, "Src R %.0fM,%.0fM", pos->x, pos->y);
        break;

      case NODE_DSTLEFT:
        sprintf(text, "Dst L %.0fM,%.0fM", pos->x, pos->y);
        break;

      case NODE_DSTRIGHT:
        sprintf(text, "Dst R %.0fM,%.0fM", pos->x, pos->y);
        break;
    }

    GUI->Lock();
    GUI->SetLCDText(text);
    GUI->Release();

    if(Properties[xProp] != x)
      SetPropertyAutomated(xProp, x);
    if(Properties[yProp] != y)
      SetPropertyAutomated(yProp, y);
  }
#else
#endif
}

void CSpatialVerb::AfterSetProgram()
{
  CMachine::AfterSetProgram();
  NeedToUpdateGUI = NeedToUpdateRoom = NeedToUpdateFIRs = NeedToUpdateFDN = true;
  Update();
}

void CSpatialVerb::SetProperty(int index, double value)
{
  if(index >= 0 && index < PROPERTY_COUNT)
  {
    Properties[index] = value;

    if(SettingProgram)
      return;

#ifndef DW_HEADLESS
    if(GUI)
      GUI->SetProperty(index, value);
#endif

    NeedToUpdateGUI = true;

    switch(index)
    {
      case PROPERTY_ROOM_WIDTH:
      case PROPERTY_ROOM_LENGTH:
      case PROPERTY_WALL_ABSORB:
      case PROPERTY_SRC_LEFT_X:
      case PROPERTY_SRC_LEFT_Y:
      case PROPERTY_SRC_RIGHT_X:
      case PROPERTY_SRC_RIGHT_Y:
      {
        NeedToUpdateRoom = true;
        break;
      }

      case PROPERTY_DST_LEFT_X:
      case PROPERTY_DST_LEFT_Y:
      case PROPERTY_DST_RIGHT_X:
      case PROPERTY_DST_RIGHT_Y:
      {
        NeedToUpdateFIRs = true;
        break;
      }

      case PROPERTY_FDN_FEEDBACK:
      case PROPERTY_FDN_GAIN:
      case PROPERTY_FDN_CUTOFF:
      {
        NeedToUpdateFDN = true;
        break;
      }
    }
  }
}

void CSpatialVerb::Update()
{
  int i;

  if(NeedToUpdateGUI)
  {
#ifndef DW_HEADLESS
    if(GUI)
      for(i = 0; i < PropertyCount; i++)
        GUI->SetProperty(i, Properties[i]);
#endif
    NeedToUpdateGUI = false;
  }

  if(NeedToUpdateRoom)
  {
    ModelRoom();
    ModelFIRs();
    UpdateGUIRoom();
    UpdateGUIImpulse();
    NeedToUpdateRoom = false;
    NeedToUpdateFIRs = false;
  }

  if(NeedToUpdateFIRs)
  {
    ModelFIRs();
    UpdateGUIRoom();
    UpdateGUIImpulse();
    NeedToUpdateFIRs = false;
  } 

  if(NeedToUpdateFDN)
  {
    ModelFDN();
    UpdateGUIImpulse2();
    NeedToUpdateFDN = false;
  }
}

void CSpatialVerb::GetPropertyName(int index, char *text)
{
  if(index >= 0 && index < PROPERTY_COUNT)
    strcpy(text, SpatialVerbPropertyNames[index]);
  else
    *text = 0;
}

void CSpatialVerb::GetPropertyValueDisplay(int index, double value, char *text)
{
  if(index >= 0 && index < PROPERTY_COUNT)
  {
    switch(SpatialVerbPropertyFormats[index])
    {
      case PROPERTY_FORMAT_METERS:
        sprintf(text, "%.0fm", MIN_ROOM_WIDTH + ((MAX_ROOM_WIDTH - MIN_ROOM_WIDTH) * value));
        break;

      case PROPERTY_FORMAT_CUTOFF:
        sprintf(text, "%.0fHz", VoltageToFrequency(value * FrequencyToVoltage(0.25f * SampleRate)));
        break;

      case PROPERTY_FORMAT_GAIN:
        sprintf(text, "%.2fdB", MIN_GAIN_DB + ((MAX_GAIN_DB - MIN_GAIN_DB) * value));
        break;

      case PROPERTY_FORMAT_PERCENT:
      default:
        sprintf(text, "%i%%", (int)(value * 100));
        break;
    }
  }
  else
    *text = 0;
}

void CSpatialVerb::FreeRays()
{
  if(LeftImageSource != 0)
  {
    delete LeftImageSource;
    LeftImageSource = 0;
  }
  if(RightImageSource != 0)
  {
    delete RightImageSource;
    RightImageSource = 0;
  }
}

double CSpatialVerb::PropertyToGain(double value)
{
  return dBToPowerRatio(MIN_GAIN_DB + (value * (MAX_GAIN_DB - MIN_GAIN_DB))); 
}

double CSpatialVerb::PropertyToFeedback(double value)
{
  value = 1.0 - value;
  value = value * value * value;
  value = 1.0 - value;
  return value * 0.999;
}

void CSpatialVerb::ModelFDN()
{
  FDN->SetFeedback(PropertyToFeedback(Properties[PROPERTY_FDN_FEEDBACK]));
  FDN->SetGain(PropertyToGain(Properties[PROPERTY_FDN_GAIN]));
  FDN->SetCutoff(Properties[PROPERTY_FDN_CUTOFF]);
}

void CSpatialVerb::UpdateGUIImpulse2()
{
#ifndef DW_HEADLESS
  if(GUI != 0)
  {
    GUI->Lock();
    GUI->UpdateImpulse2(EarlySamples * 2, FDN, EarlySamples);
    GUI->Release();
  }
#else
#endif
}

bool CSpatialVerb::IsSynth()
{
  return false;
}

// CSpatialVerbInfo

CSpatialVerbInfo::CSpatialVerbInfo(CHost *host)
{
  Host   = host;
  Name   = "ES SpatialVerb";
  UniqueID = "DarkPlug::ES SpatialVerb";
  Loaded = false;
}

CSpatialVerbInfo::~CSpatialVerbInfo()
{
  if(Loaded)
  {
    delete Graphics.Background;
    delete Graphics.Knob;
    delete Graphics.SliderH;
    delete Graphics.SliderV;
    delete Graphics.Nodes;
    delete Graphics.LCDFont;
  }
}

CMachine *CSpatialVerbInfo::CreateMachine()
{
  if(!Loaded)
  {
    Graphics.Background = new CFrameBuffer(IDR_SPATIALVERB_BACKGROUND);
    Graphics.Knob       = new CFrameBuffer(IDR_SPATIALVERB_KNOB);
    Graphics.SliderH    = new CFrameBuffer(IDR_SPATIALVERB_SLIDER_H);
    Graphics.SliderV    = new CFrameBuffer(IDR_SPATIALVERB_SLIDER_V);
    Graphics.Nodes      = new CFrameBuffer(IDR_SPATIALVERB_NODES);
    Graphics.LCDFont    = new CFrameBuffer(IDR_Y2KMUTTER);

    Loaded              = true;
  }
  return new CSpatialVerb(Host, &Graphics);
}
