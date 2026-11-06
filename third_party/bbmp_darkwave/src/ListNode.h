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

#ifndef LISTNODE_H
#define LISTNODE_H

template <class T> class CListNode
{
public:
  CListNode()
  {
    NextNode     = 0;
    PreviousNode = 0;
    FirstNode    = 0;
  }

  void Attach(T **firstNode)
  {
    if(*firstNode == 0)
      *firstNode = (T*)this;
    else
    {
      PreviousNode = *firstNode;
      while(PreviousNode->NextNode != 0)
        PreviousNode = PreviousNode->NextNode;
      PreviousNode->NextNode = (T*)this;
    }
    FirstNode = firstNode;
  }

  void Detach()
  {
    if(PreviousNode)
      PreviousNode->NextNode = NextNode;
    else
      *FirstNode = NextNode;

    if(NextNode != 0)
      NextNode->PreviousNode = PreviousNode;

    PreviousNode = 0;
    NextNode = 0;
    FirstNode = 0;
  }

  T* GetNext()
  {
    return NextNode;
  }

protected:
  T  *NextNode;
  T  *PreviousNode;
  T **FirstNode;
};

#endif