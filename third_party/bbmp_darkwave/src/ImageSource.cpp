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



#include "ImageSource.h"
#include <math.h>

#define SPEED_OF_SOUND 346.02f

#define POS_BEFORE 0
#define POS_INSIDE 1
#define POS_AFTER  2

inline double Radius2(double x, double y)
{
  return (x * x) + (y * y);
}

inline double Radius(double x, double y)
{
  return sqrt((x * x) + (y * y));
}

CImageSource::CImageSource(int level, SSize *roomSize, SPos *srcPos, double intensity, double wallAbsorb, double time)
{
  int   xPos, yPos;
  double r, childIntensity;
  SPos  pos;

  if(level == 0)
  {
    time = time * SPEED_OF_SOUND;
    time = time * time;
  }

  Position.x = srcPos->x;
  Position.y = srcPos->y;
  Intensity  = intensity;

  xPos = Position.x < 0.0f ? POS_BEFORE : Position.x > roomSize->width  ? POS_AFTER : POS_INSIDE;
  yPos = Position.y < 0.0f ? POS_BEFORE : Position.y > roomSize->height ? POS_AFTER : POS_INSIDE;

  ChildCount = 0;
  childIntensity = intensity * wallAbsorb;

  if(xPos == POS_INSIDE || xPos == POS_AFTER)
  {
    pos.x = -Position.x;
    pos.y = Position.y;
    r = yPos == POS_BEFORE ? Radius2(pos.x, pos.y) : yPos == POS_INSIDE ? Radius2(pos.x, 0.0f) : Radius2(pos.x, pos.y - roomSize->height);
    if(r < time)
      Children[ChildCount++] = new CImageSource(level + 1, roomSize, &pos, childIntensity, wallAbsorb, time);
  }
  if(xPos == POS_INSIDE || xPos == POS_BEFORE)
  {
    pos.x = (2 * roomSize->width) - Position.x;
    pos.y = Position.y;
    r = yPos == POS_BEFORE ? Radius2(pos.x - roomSize->width, pos.y) : yPos == POS_INSIDE ? Radius2(pos.x - roomSize->width, 0.0f) : Radius2(pos.x - roomSize->width, pos.y - roomSize->height);
    if(r < time)
      Children[ChildCount++] = new CImageSource(level + 1, roomSize, &pos, childIntensity, wallAbsorb, time);
  }
  if(yPos == POS_INSIDE || yPos == POS_AFTER)
  {
    pos.x = Position.x;
    pos.y = -Position.y;
    r = xPos == POS_BEFORE ? Radius2(pos.x, pos.y) : xPos == POS_INSIDE ? Radius2(0.0f, pos.y) : Radius2(pos.x - roomSize->width, pos.y);
    if(r < time)
      Children[ChildCount++] = new CImageSource(level + 1, roomSize, &pos, childIntensity, wallAbsorb, time);
  }
  if(yPos == POS_INSIDE || yPos == POS_BEFORE)
  {
    pos.x = Position.x;
    pos.y = (2 * roomSize->height) - Position.y;
    r = xPos == POS_BEFORE ? Radius2(pos.x, pos.y - roomSize->height) : xPos == POS_INSIDE ? Radius2(0.0f, pos.y - roomSize->height) : Radius2(pos.x - roomSize->width, pos.y - roomSize->height);
    if(r < time)
      Children[ChildCount++] = new CImageSource(level + 1, roomSize, &pos, childIntensity, wallAbsorb, time);
  }
}

CImageSource::~CImageSource()
{
  int i;

  for(i = 0; i < ChildCount; i++)
    delete Children[i];
}

double CImageSource::Converge(int level, SPos *dstPos, double t1, double t2)
{
  double r, intensity;
  int   i;

  if(level == 0)
  {
    t1 = Radius2(0.0f, t1 * SPEED_OF_SOUND);
    t2 = Radius2(0.0f, t2 * SPEED_OF_SOUND);
  }

  r = Radius2(dstPos->x - Position.x, dstPos->y - Position.y);
  if(r < t2)
  {
    if(r >= t1)
      return Intensity;
    if(ChildCount > 0)
    {
      intensity = 0.0f;
      for(i = 0; i < ChildCount; i++)
        intensity += Children[i]->Converge(level + 1, dstPos, t1, t2);
      return intensity;
    }
  }
  return 0.0f;
}

double CImageSource::ConvergeGraphical(int level, SPos *dstPos, double t, double radius)
{
  double r, intensity;
  int   i;

  if(level == 0)
    t = Radius(0.0f, t * SPEED_OF_SOUND);

  r = Radius(dstPos->x - Position.x, dstPos->y - Position.y);
  if(r < t + radius)
  {
    if(r >= t - radius)
      return Intensity * (1.0 - (fabs(r - t) / radius));
    if(ChildCount > 0)
    {
      intensity = 0.0f;
      for(i = 0; i < ChildCount; i++)
        intensity += Children[i]->ConvergeGraphical(level + 1, dstPos, t, radius);
      return intensity;
    }
  }
  return 0.0f;
}