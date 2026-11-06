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

#ifndef ARRAY_H
#define ARRAY_H

#include "Error.h"

template <class T> class CArray
{
public:
  CArray()
  {
    Items = 0;
    Count = 0;
  }

  ~CArray()
  {
    Clear();
  }


  int AddItem(T item)
  {
    T *newItems;
    int i;

    if(Count == 0)
    {
      Items = new T[1];
      Items[0] = item;
      Count = 1;
    }
    else
    {
      newItems = new T[Count + 1];
      for(i = 0; i < Count; i++)
        newItems[i] = Items[i];
      newItems[Count] = item;
      delete Items;
      Items = newItems;
      Count++;
    }
    return Count - 1;
  }

  void RemoveItem(int index)
  {
    T *newItems;
    int i, j;

    if(index < 0 || index >= Count)
      ThrowString("CArray::RemoveItem - Index Out Of Bounds (%i)", index);
    else
    {
      if(Count == 0)
      {
        delete Items;
        Items = 0;
        Count = 0;
      }
      else
      {
        newItems = new T[Count - 1];
        i = 0;
        for(j = 0; j < Count; j++)
          if(j != index)
            newItems[i++] = Items[j];
        delete Items;
        Items = newItems;
        Count--;
      }
    }
  }

  void Clear()
  {
    if(Count != 0)
    {
      delete Items;
      Items = 0;
      Count = 0;
    }
  }

  T  *Items;
 int  Count;
};

#endif