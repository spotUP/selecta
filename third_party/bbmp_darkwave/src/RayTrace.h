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

#ifndef RAYTRACE_H
#define RAYTRACE_H

struct SRay
{
  double *xPos;
  double *yPos;
  double *intensity;
  double xVelocity;
  double yVelocity;
};

void RayTrace(double roomWidth, double roomHeight, double sourceX, double sourceY,
  int rayCount, double wallAbsorb, SRay **rays, int sampleRate, int samples);

float ConvergeRays(int rayCount, SRay *rays, int sample, float x, float y, double r);

#endif
