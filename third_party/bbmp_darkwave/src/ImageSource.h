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

#ifndef IMAGESOURCE_H
#define IMAGESOURCE_H

struct SPos
{
  double x, y;
};

struct SSize
{
  double width, height;
};

class CImageSource
{
public:
        CImageSource(int level, SSize *roomSize, SPos *srcPos, double intensity, double wallAbsorb, double time);
       ~CImageSource();
  double Converge(int level, SPos *dstPos, double t1, double t2);
  double ConvergeGraphical(int level, SPos *dstPos, double t, double radius);

private:
  int            ChildCount;
  CImageSource  *Children[4];
  double          Intensity;
  SPos           Position;
};

#endif