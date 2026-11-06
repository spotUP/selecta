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

#ifndef VCA_H
#define VCA_H

#define VCA_MODE_LINEAR      0
#define VCA_MODE_EXPONENTIAL 1

class CVCA 
{
public:
       CVCA(int mode, double scale);
  void Process(int samples, double *data, double *modData);
  
private:
  int   Mode;
  double Scale;
};

#endif