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

#ifndef FILTER_DESIGN_H
#define FILTER_DESIGN_H

#include "DarkPlug.h"
#include "Control.h"
#include "BackBuffer.h"

class CFilterDesign;

class CFilterView : public CControl
{
public:
               CFilterView(CControl *parent, int x, int y, int width, int height, double cutoff, double resonance);
              ~CFilterView();
  virtual void OnPaint();
  virtual bool OnNCPaint(HDC hDC);
  void         DrawControl();
  
private:
  double        Cutoff, Resonance;
};

class CFilterDesignGUI : public CControl
{
public:
  CFilterDesignGUI(CFilterDesign *filterDesign, HWND parent, double cutoff, double resonance);
 ~CFilterDesignGUI();

private:
  CFilterView *FilterView;
};

class CFilterDesign : public CMachine
{
public:
               CFilterDesign(CHost *host);
  virtual     ~CFilterDesign();
  virtual void Work();
  virtual void SetSampleRate(int sampleRate);
  virtual void SetBlockSamples(int blockSamples);
  virtual HWND CreateGUI(HWND parent);

private:
  CFilterDesignGUI *GUI;
  double             Cutoff, Resonance;
};

class CFilterDesignInfo : public CMachineInfo
{
public:
            CFilterDesignInfo(CHost *host);
           ~CFilterDesignInfo();
  CMachine *CreateMachine();

private:
  CHost *Host;
};

#endif