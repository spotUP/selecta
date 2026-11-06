/*  ExperimentalScene DarkWave Studio
    Copyright (C) 2004-2012 Daniel Werner
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

#ifndef DARKPLUG_H
#define DARKPLUG_H

#include <windows.h>

#pragma pack(push)
#pragma pack(4)

#ifndef WIN64
#define DARKCALL __cdecl
#else
#define DARKCALL __stdcall
#endif

#define AUDIO_BUFFER_ALIGN 16

#define CPU_FEATURE_MMX   0x0001
#define CPU_FEATURE_SSE   0x0002
#define CPU_FEATURE_SSE2  0x0004
#define CPU_FEATURE_3DNOW 0x0008

#define PT_AUDIO   0x00000001
#define PT_CONTROL 0x00000002

#define PT_INPUT   0x00000010
#define PT_OUTPUT  0x00000020
#define PT_AUX     0x00000040

#define PT_MONO    0x00000000
#define PT_STEREO  0x00000100

#define CONTROL_MESSAGE_NOTE_ON        1
#define CONTROL_MESSAGE_NOTE_OFF       2
#define CONTROL_MESSAGE_VOICE_ON       3
#define CONTROL_MESSAGE_VOICE_OFF      4
#define CONTROL_MESSAGE_CONTROL_CHANGE 5
	
// Maximum Length Of Strings Including Terminating NULL Byte

#define MAX_PROGRAM_NAME_STRING     64
#define MAX_PROPERTY_DISPLAY_STRING 64
#define MAX_PROPERTY_NAME_STRING    64
#define MAX_VOICE_NAME_STRING       64

class CPort;
class CMachine;

class CMutex
{
public:
               CMutex();
  virtual     ~CMutex();
  virtual void DARKCALL Lock();
  virtual void DARKCALL Release();
  virtual bool DARKCALL IsLocked();

private:
  HANDLE        Mutex;
  int           LockCount;
};

class CMachineInfo;

enum EError
{
  errorOK,
  errorFailed,
  errorInvalidParameter,
  errorNotLocked,
  errorNotConnected,
  errorNotImplemented,
  errorInvalidCallDuringEvent,
  errorCantAccessFile,
  errorBadFile
};

enum ESupports
{
  supportsBankServer
};

class CBankClient;

class CBankServer : public CMutex
{
public:
  virtual EError DARKCALL Connect(CBankClient *client) = 0;
  virtual EError DARKCALL Disconnect(CBankClient *client) = 0;
  
  virtual EError DARKCALL AddBank(char *bankName, int *bankIndex) = 0;
  virtual EError DARKCALL AddBankInfo(int bankIndex, char *infoName, char *infoText, int *infoIndex) = 0;
  virtual EError DARKCALL GetBankName(int bankIndex, char *bankName) = 0;
  virtual EError DARKCALL GetBankNameLength(int bankIndex, int *nameLength) = 0;
  
  virtual EError DARKCALL GetBankInfoCount(int bankIndex, int *infoCount) = 0;
  virtual EError DARKCALL GetBankInfoNameLength(int bankIndex, int infoIndex, int *nameLength) = 0;
  virtual EError DARKCALL GetBankInfoName(int bankIndex, int infoIndex, char *infoName) = 0;
  virtual EError DARKCALL GetBankInfoTextLength(int bankIndex, int infoIndex, int *textLength) = 0;
  virtual EError DARKCALL GetBankInfoText(int bankIndex, int infoIndex, char *infoText) = 0;
  
  virtual EError DARKCALL AddPreset(int bankIndex, char *presetName, int *presetIndex) = 0;
  virtual EError DARKCALL GetPresetCount(int bankIndex, int *presetCount) = 0;
  virtual EError DARKCALL GetPresetName(int bankIndex, int presetIndex, char *presetName) = 0;
  virtual EError DARKCALL GetPresetNameLength(int bankIndex, int presetIndex, int *nameLength) = 0;
};

class CHost;

class CBankClient : public CMutex
{
public:
                   CBankClient(CHost *host);
                  ~CBankClient();
  virtual EError DARKCALL ConnectBankServer();
  virtual EError DARKCALL DisconnectBankServer();
  virtual bool   DARKCALL IsBankServerPresent();
  virtual void   DARKCALL OnAddBank(int bankIndex);
  virtual void   DARKCALL OnAddBankInfo(int bankIndex, int infoIndex);
  virtual void   DARKCALL OnAddPreset(int bankIndex, int presetIndex);

protected:
  CBankServer *BankServer;

private:
  bool         Connected, ServerPresent;
};

class CHost
{
public:
  virtual EError DARKCALL   RegisterMachineInfo(CMachineInfo *machineInfo) = 0;
  virtual EError DARKCALL   GetNoteFrequency(unsigned int note, double *frequency) = 0;
  virtual EError DARKCALL   GetBankServer(CBankServer **bankServer) = 0;
  virtual void   DARKCALL   SetPropertyAutomated(CMachine *machine, int index, double value) = 0;
  virtual int    DARKCALL   GetSamplePosition() = 0;
  virtual double DARKCALL   GetBPM() = 0;
  virtual double DARKCALL   GetTPB() = 0;
  virtual int    DARKCALL   GetSamplesPerTick() = 0;
  virtual int    DARKCALL   GetSampleRate() = 0;
  virtual int    DARKCALL   GetBlockSamples() = 0;
  virtual bool   DARKCALL   Supports(ESupports code) = 0;
  virtual int    DARKCALL   GetCPUFeatures() = 0;
};

class CAudioBuffer
{
public:
               CAudioBuffer(int samples, bool stereo);
  virtual     ~CAudioBuffer();
  virtual void DARKCALL Clear();
  virtual void DARKCALL Resize(int blockSamples);

  union
  {
    double *LeftData, *MonoData;
  };

  double *RightData;
  bool    Stereo;
  int     Samples;
};

struct SNoteOn
{
  unsigned char Note;     
  double         Velocity;
  double         Frequency;
};

struct SNoteOff
{
  unsigned char Note;
};

struct SVoiceOn
{
  unsigned int  Voice;     
  double         Velocity;
};

struct SVoiceOff
{
  unsigned int Voice;
};

struct SControlChange
{
  unsigned int Controller;
  double        Value;
};

struct SControlMessage
{
  int           Size;
  int           BlockSample;
  int           Type;
  LARGE_INTEGER Reserved;

  union
  {
    SNoteOn        NoteOn;
    SNoteOff       NoteOff;
    SVoiceOn       VoiceOn;
    SVoiceOff      VoiceOff;
    SControlChange ControlChange;
  };
};

struct SControlBufferItem
{
  SControlBufferItem *NextItem;
  SControlMessage     ControlMessage;
};

class CControlBuffer : private CMutex
{
public:
                         CControlBuffer();
  virtual               ~CControlBuffer();
  virtual bool DARKCALL  Read(SControlMessage  *message);
  virtual void DARKCALL  Write(SControlMessage *message);
  virtual void DARKCALL  Clear();
  virtual void DARKCALL  CopyTo(CControlBuffer *target);
  virtual void DARKCALL  MoveFuture(CControlBuffer *target, int samples);
  virtual int  DARKCALL  Count();

private:
  SControlBufferItem *FirstItem;
};

class CPort
{
public:
               CPort(CHost *host, CMachine *machine, int type, char *name);
  virtual     ~CPort();

  int             Type;
  CAudioBuffer   *AudioBuffer;
  CControlBuffer *ControlBuffer;
  CMachine       *Machine;
  char           *Name;
  int             Index;
};

class CProgram
{
public:
  CProgram(int propertyCount, int index, CMachine *machine);
 ~CProgram();

  int    PropertyCount;
  double *Properties;
  char   Name[MAX_PROGRAM_NAME_STRING];
};

class CMachine : public CBankClient
{
public:
                CMachine(CHost *host);
  virtual      ~CMachine();
  virtual void  DARKCALL Delete();
  virtual void  DARKCALL Work() = 0;
  virtual void  DARKCALL WorkAUX();
  virtual bool  DARKCALL GetGUIRect(short *x1, short *y1, short *x2, short *y2);
  virtual HWND  DARKCALL CreateGUI(HWND parent);
  virtual void  DARKCALL OnShowGUI();
  virtual void  DARKCALL OnHideGUI();
  virtual void  DARKCALL OnIdle();

  virtual double DARKCALL GetProperty(int index);
  virtual void  DARKCALL SetProperty(int index, double value);
  virtual void  DARKCALL SetPropertyAutomated(int index, double value);
  virtual void  DARKCALL GetPropertyName(int index, char *text);
  virtual void  DARKCALL GetPropertyDisplay(int index, char *text);
  virtual void  DARKCALL GetPropertyValueDisplay(int index, double value, char *text);
  virtual void  DARKCALL GetVoiceName(int index, char *text);
  virtual void  DARKCALL GetNoteLabel(int index, char *text);

  virtual void  DARKCALL SetProgram(int index);
  virtual void  DARKCALL GetProgramName(char *text);
  virtual void  DARKCALL SetProgramName(char *text);
  virtual void  DARKCALL GetProgramNameIndexed(int index, char *text);
  virtual int   DARKCALL GetProgram();

  virtual void  DARKCALL SetSampleRate(int sampleRate) = 0;
  virtual void  DARKCALL SetBlockSamples(int blockSamples) = 0;
  virtual void  DARKCALL ResizeAudioPorts(int blockSamples);

  virtual void  DARKCALL BeforeSetProgram();
  virtual void  DARKCALL AfterSetProgram();
  virtual void  DARKCALL SetName(char *name);
  virtual char* DARKCALL GetName();
  virtual void  DARKCALL ReadProperties(char *data, int length, bool isInner);
  virtual void  DARKCALL WriteProperties(char **data, int *length, bool *isInner);
  virtual bool  DARKCALL IsSynth() = 0;

  CPort **Ports;
  int     PortCount, PropertyCount, ProgramCount, Program, VoiceCount;
  void   *HostData, *Effect; 
  CHost     *Host;
  int     UniqueID;
  int Index;
protected:
  char   *Name;
  CProgram **Programs;
  int        SettingProgram;
};

class CMachineInfo
{
public:
                    CMachineInfo();
  virtual          ~CMachineInfo();
  virtual void      DARKCALL Delete();
  virtual CMachine* DARKCALL CreateMachine() = 0;
  
  char  *Name, *UniqueID, *OldUniqueID, *MenuPath;
  HICON  IconSmall, IconLarge;
};

#ifdef DARKPLUG_HOST

typedef void (DARKCALL *DarkPlugInit)(HINSTANCE dllInstance, CHost *host);

#else

extern "C"
{
  __declspec(dllexport) void DARKCALL DarkPlugInit(HINSTANCE dllInstance, CHost *host);
}

#endif

#pragma pack(pop)

#endif

