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


#include <malloc.h>
#include <stdio.h>
#include "DarkPlug.h"

CBankClient::CBankClient(CHost *host)
{
  EError error;

  Connected     = false;
  ServerPresent = false;
  BankServer    = 0;

  if(host == 0)
    return;

  if(host->Supports(supportsBankServer))
  {
    error = host->GetBankServer(&BankServer);
    if(error == errorOK)
      ServerPresent = true;
  }
}

CBankClient::~CBankClient()
{
  if(Connected)
    DisconnectBankServer();
}

bool DARKCALL CBankClient::IsBankServerPresent()
{
  return ServerPresent;
}

void DARKCALL CBankClient::OnAddBank(int bankIndex)
{
}

void DARKCALL CBankClient::OnAddBankInfo(int bankIndex, int infoIndex)
{
}

void DARKCALL CBankClient::OnAddPreset(int bankIndex, int presetIndex)
{
}

EError DARKCALL CBankClient::ConnectBankServer()
{
  EError error;

  if(Connected || (!ServerPresent))
    return errorFailed;
  BankServer->Lock();
  error = BankServer->Connect(this);
  BankServer->Release();
  if(error == errorOK)
    Connected = true;
  return error;
}

EError DARKCALL CBankClient::DisconnectBankServer()
{
  EError error;

  if(Connected)
  {
    BankServer->Lock();
    error = BankServer->Disconnect(this);
    BankServer->Release();
    Connected = false;
    return error;
  }
  else
    return errorNotConnected;
}

CProgram::CProgram(int propertyCount, int index, CMachine *machine)
{
  int i;

  PropertyCount = propertyCount;
  Properties = new double[PropertyCount];
  for(i = 0; i < PropertyCount; i++)
    Properties[i] = machine->GetProperty(i);
  
  sprintf(Name, "Program %03i", index);
}

CProgram::~CProgram()
{
  delete Properties;
}

CMachine::CMachine(CHost *host)
: CBankClient(host)
{
  SettingProgram = 0;
  Ports = 0;
  PortCount = 0;
  HostData = 0;
  Host = host;
  PropertyCount = 0;
  VoiceCount = 0;
  Effect = 0;
  ProgramCount = 0;
  Program = 0;
  Programs = 0;
  Name = 0;
  UniqueID = 0;
}

CMachine::~CMachine()
{
  if(Name)
    delete Name;
  if(Programs)
    delete Programs;
}

void DARKCALL CMachine::Delete()
{
  delete this;
}

void DARKCALL CMachine::WorkAUX()
{
}

HWND DARKCALL CMachine::CreateGUI(HWND parent)
{
  return 0;
}

bool DARKCALL CMachine::GetGUIRect(short *x1, short *y1, short *x2, short *y2)
{
  return false;
}

void DARKCALL CMachine::OnShowGUI()
{
}

void DARKCALL CMachine::OnHideGUI()
{
}

void DARKCALL CMachine::OnIdle()
{
}

void DARKCALL CMachine::SetName(char *name)
{
  if(Name)
    delete Name;
  Name = new char[strlen(name) + 1];
  strcpy(Name, name);
}

char* DARKCALL CMachine::GetName()
{
  return Name;
}

CMachineInfo::CMachineInfo()
{
  Name        = "New Machine";
  UniqueID    = "";
  OldUniqueID = 0;
  MenuPath    = "Native";
  IconSmall   = NULL;
  IconLarge   = NULL;
}

CMachineInfo::~CMachineInfo()
{
}

void DARKCALL CMachineInfo::Delete()
{
  delete this;
}

CPort::CPort(CHost *host, CMachine *machine, int type, char *name)
{
  bool stereo;
  int  blockSamples;
  size_t len;

  AudioBuffer = 0;
  ControlBuffer = 0;
  Machine = machine;
  len = strlen(name) + 1;
  Name = new char[len];
  strcpy(Name, name);

  if((type & PT_AUDIO) == PT_AUDIO)
  {
    stereo = ((type & PT_STEREO) == PT_STEREO);
    blockSamples = host->GetBlockSamples();
    AudioBuffer = new CAudioBuffer(blockSamples, stereo);
  }

  if((type & PT_CONTROL) == PT_CONTROL)
  {
    ControlBuffer = new CControlBuffer();
  }

  Type = type;
}

CPort::~CPort()
{
  delete Name;

  if(AudioBuffer != 0)
    delete AudioBuffer;

  if(ControlBuffer != 0)
    delete ControlBuffer;
}

CAudioBuffer::CAudioBuffer(int samples, bool stereo)
{
  size_t bufferSize;

  bufferSize = sizeof(double) * samples;

  if(stereo)
  {
    LeftData  = (double*)_mm_malloc(bufferSize, AUDIO_BUFFER_ALIGN);
    RightData = (double*)_mm_malloc(bufferSize, AUDIO_BUFFER_ALIGN);
  }
  else
  {
    MonoData  = (double*)_mm_malloc(bufferSize, AUDIO_BUFFER_ALIGN);
    RightData = 0;
  }
  Samples = samples;
  Stereo = stereo;

  Clear();
}

void DARKCALL CAudioBuffer::Resize(int blockSamples)
{
  size_t bufferSize;

  if(Samples != blockSamples)
  {
    bufferSize = sizeof(double) * blockSamples;
  
    if(Stereo)
    {
      _mm_free(LeftData);
      _mm_free(RightData);
      LeftData  = (double*)_mm_malloc(bufferSize, AUDIO_BUFFER_ALIGN);
      RightData = (double*)_mm_malloc(bufferSize, AUDIO_BUFFER_ALIGN);
    }
    else
    {
      _mm_free(MonoData);
      MonoData = (double*)_mm_malloc(bufferSize, AUDIO_BUFFER_ALIGN);
    }
    Samples = blockSamples;
  }

  Clear();
}

CAudioBuffer::~CAudioBuffer()
{
  if(RightData)
    _mm_free(RightData);
  if(LeftData)
    _mm_free(LeftData);
}

void DARKCALL CAudioBuffer::Clear()
{
  register int     count;
  register double *buffer1, *buffer2;

  count = Samples;

  if(Stereo)
  {
    buffer1 = LeftData;
    buffer2 = RightData;
    
    while(count >= 8)
    {
      buffer1[--count] = 0.0;
      buffer2[count]   = 0.0;
      buffer1[--count] = 0.0;
      buffer2[count]   = 0.0;
      buffer1[--count] = 0.0;
      buffer2[count]   = 0.0;
      buffer1[--count] = 0.0;
      buffer2[count]   = 0.0;
      buffer1[--count] = 0.0;
      buffer2[count]   = 0.0;
      buffer1[--count] = 0.0;
      buffer2[count]   = 0.0;
      buffer1[--count] = 0.0;
      buffer2[count]   = 0.0;
      buffer1[--count] = 0.0;
      buffer2[count]   = 0.0;
    }

    while(count >= 1)
    {
      buffer1[--count] = 0.0;
      buffer2[count]   = 0.0;
    }
  }
  else
  {
    buffer1 = MonoData;
    
    while(count >= 8)
    {
      buffer1[--count] = 0.0;
      buffer1[--count] = 0.0;
      buffer1[--count] = 0.0;
      buffer1[--count] = 0.0;
      buffer1[--count] = 0.0;
      buffer1[--count] = 0.0;
      buffer1[--count] = 0.0;
      buffer1[--count] = 0.0;
    }

    while(count >= 1)
      buffer1[--count] = 0.0;
  }
}

CControlBuffer::CControlBuffer()
{
  FirstItem = 0;
}

CControlBuffer::~CControlBuffer()
{
  if(FirstItem != 0)
    Clear();
}

int DARKCALL CControlBuffer::Count()
{
  int                 n = 0;
  SControlBufferItem *item;

  Lock();

  item = FirstItem;
  while(item != 0)
  {
    n++;
    item = item->NextItem;
  }

  Release();

  return n;
}

bool DARKCALL CControlBuffer::Read(SControlMessage *message)
{
  SControlBufferItem *item, *firstItem, **previousItem, **previousFirstItem;
  int                 itemBlockSample, firstItemBlockSample;

  Lock();

  if(FirstItem == 0)
  {
    Release();
    return false;
  }

  firstItem         = FirstItem;
  previousFirstItem = &FirstItem;
  item              = FirstItem->NextItem;
  previousItem      = &FirstItem->NextItem;

  while((FirstItem != 0) && (item != 0))
  {
    itemBlockSample = item->ControlMessage.BlockSample;
    firstItemBlockSample = firstItem->ControlMessage.BlockSample;

    if(itemBlockSample == firstItemBlockSample)
    {
      if(item->ControlMessage.Type == CONTROL_MESSAGE_NOTE_OFF &&
         firstItem->ControlMessage.Type != CONTROL_MESSAGE_NOTE_OFF)
      {
        firstItem         = item;
        previousFirstItem = previousItem; 
      }
    }
    else if(itemBlockSample < firstItemBlockSample)
    {
      firstItem         = item;
      previousFirstItem = previousItem; 
    }
  
    previousItem = &item->NextItem;
    item         = item->NextItem;
  }

  memcpy(message, &(firstItem->ControlMessage), sizeof(SControlMessage));
  *previousFirstItem = firstItem->NextItem;
  delete firstItem;
  
  Release();
  
  return true;
}

void DARKCALL CControlBuffer::Write(SControlMessage *message)
{
  SControlBufferItem *newItem, *item, **itemPtr;

  Lock();

  newItem = new SControlBufferItem;
  memcpy(&(newItem->ControlMessage), message, sizeof(SControlMessage));

  item    = FirstItem;
  itemPtr = &FirstItem;
  while(item != 0)
  {
    if(item->ControlMessage.BlockSample > newItem->ControlMessage.BlockSample)
    {
      *itemPtr = newItem;
      newItem->NextItem = item;
      Release();
      return;
    }
    else
    {
      itemPtr = &(item->NextItem);
      item = item->NextItem;
    }
  }
  *itemPtr = newItem;
  newItem->NextItem = 0;
  Release();
}

void DARKCALL CControlBuffer::Clear()
{
  SControlBufferItem *item, *nextItem;

  Lock();

  item = FirstItem;
  while(item != 0)
  {
    nextItem = item->NextItem;
    delete item;
    item = nextItem;
  }
  FirstItem = 0;

  Release();
}

void DARKCALL CControlBuffer::CopyTo(CControlBuffer *target)
{
  SControlBufferItem *item;

  Lock();

  item = FirstItem;
  while(item != 0)
  {
    target->Write(&(item->ControlMessage));
    item = item->NextItem;
  }

  Release();
}

void DARKCALL CControlBuffer::MoveFuture(CControlBuffer *target, int samples)
{
  SControlBufferItem **item, *d;

  Lock();

  item = &FirstItem;
  while((FirstItem != 0) && (*item != 0))
  {
    if((*item)->ControlMessage.BlockSample < samples)
    {
      target->Write(&((*item)->ControlMessage));
      d = *item;
      *item = (*item)->NextItem;
      delete d;
    }
    else
    {
      (*item)->ControlMessage.BlockSample -= samples;
      item = &(*item)->NextItem;
    }
  }

  Release();
}

double DARKCALL CMachine::GetProperty(int index)
{
  return 0.0;
}

void DARKCALL CMachine::SetProperty(int index, double value)
{
  if((index >= 0) && (index < PropertyCount))
    if(Programs)
      if((Program >= 0) && (Program < ProgramCount))
        Programs[Program]->Properties[index] = value;
}

void DARKCALL CMachine::GetPropertyDisplay(int index, char *text)
{
  GetPropertyValueDisplay(index, GetProperty(index), text);
}

void DARKCALL CMachine::GetPropertyValueDisplay(int index, double value, char *text)
{
  text[0] = 0;
}

void DARKCALL CMachine::GetPropertyName(int index, char *text)
{
  text[0] = 0;
}

void DARKCALL CMachine::GetVoiceName(int index, char *text)
{
  text[0] = 0;
}

void DARKCALL CMachine::GetNoteLabel(int index, char *text)
{
  text[0] = 0;
}

void DARKCALL CMachine::SetProgram(int index)
{
  int i;

  BeforeSetProgram();

  Program = index;
  for(i = 0; i < PropertyCount; i++)
  {
    SetProperty(i, Programs[index]->Properties[i]);
  }

  AfterSetProgram();
}

void DARKCALL CMachine::BeforeSetProgram()
{
  SettingProgram++;
}

void DARKCALL CMachine::AfterSetProgram()
{
  SettingProgram--;
}

void DARKCALL CMachine::GetProgramName(char *text)
{
  if(Programs)
    strcpy(text, Programs[Program]->Name);
  else
    *text = 0;
}

int DARKCALL CMachine::GetProgram()
{
  return Program;
}

void DARKCALL CMachine::SetProgramName(char *text)
{
  if(Programs)
    strcpy(Programs[Program]->Name, text);
}

void DARKCALL CMachine::GetProgramNameIndexed(int index, char *text)
{
  if((Programs != 0) && (index < ProgramCount) && (index >= 0))
    strcpy(text, Programs[index]->Name);
  else
	*text = 0;
}

void DARKCALL CMachine::SetPropertyAutomated(int index, double value)
{
  Host->SetPropertyAutomated(this, index, value);
  SetProperty(index, value);
}

void DARKCALL CMachine::ResizeAudioPorts(int blockSamples)
{
  int i;

  for(i = 0; i < PortCount; i++)
    if((Ports[i]->Type & PT_AUDIO) == PT_AUDIO)
	{
      Ports[i]->AudioBuffer->Resize(blockSamples);
	}
}

#define PROPERTIES_HEADER_MAGIC 'PH64'

#pragma pack(push)
#pragma pack(1)

struct PropertiesHeader
{
  int Magic;
  int PropertyCount;
};

struct Property
{
  double value;
};

#pragma pack(pop)

void DARKCALL CMachine::ReadProperties(char *data, int length, bool isInner)
{
   PropertiesHeader *ph;
   Property         *p;
   float             f;

   if(length >= sizeof(PropertiesHeader))
   {
     ph = (PropertiesHeader*)data;  
   }

   int i;

   if(ph->Magic == 'DwPh')
   {
     for(i = 0; i < ph->PropertyCount; i++)
     {
       f = *(float*)(data + sizeof(PropertiesHeader) + (sizeof(float) * i));
       this->SetPropertyAutomated(i, (double)f);
     }
   }
   else
   {
     for(i = 0; i < ph->PropertyCount; i++)
     {
       p = (Property*)(data + sizeof(PropertiesHeader) + (sizeof(Property) * i));
       this->SetPropertyAutomated(i, p->value);
     }
   }
}

void DARKCALL CMachine::WriteProperties(char **data, int *length, bool *isInner)
{
  *length = sizeof(PropertiesHeader) + (sizeof(Property) * PropertyCount);
  *data = new char[*length]; 

  PropertiesHeader *ph;
  Property *p;

  ph = (PropertiesHeader*)*data;  
  ph->Magic = PROPERTIES_HEADER_MAGIC;
  ph->PropertyCount = this->PropertyCount;

  int i;

  for(i = 0; i < ph->PropertyCount; i++)
  {
     p = (Property*)(*data + sizeof(PropertiesHeader) + sizeof(Property) * i);
     p->value = this->GetProperty(i);
  }

  *isInner = true;
}

CMutex::CMutex()
{
  Mutex = CreateMutex(NULL, FALSE, NULL);
  LockCount = 0;
}

CMutex::~CMutex()
{
  if(LockCount > 0)
    Release();
  CloseHandle(Mutex);
}

void DARKCALL CMutex::Lock()
{
  WaitForSingleObject(Mutex, INFINITE);
  LockCount++;
}

void DARKCALL CMutex::Release()
{
  LockCount--;
  ReleaseMutex(Mutex);
}

bool DARKCALL CMutex::IsLocked()
{
  return LockCount > 0;
}