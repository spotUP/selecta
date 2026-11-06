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



#include <math.h>
#include "Util.h"
#include "DarkPlug.h"
#include "Error.h"
#include <string.h>
#include <xmmintrin.h>
#include <emmintrin.h>

void ShowWebPage(char *url)
{
  ShellExecuteA(0, "open", url, 0, 0, SW_SHOWNORMAL);
}

double dBToPowerRatio(double dB)
{
  return pow(10.0, dB * 0.1);
}

double PowerRatioTodB(double powerRatio)
{
  return 10.0 * log10(powerRatio);
}

char *CopyString(char *string)
{
  int   c;
  char *newString;

  c = (int)strlen(string);
  newString = new char[c + 1];
  strcpy(newString, string);
  return newString;
}

char *CopyString(char *string, int *length)
{
  int   c;
  char *newString;

  c = (int)strlen(string);
  newString = new char[c + 1];
  strcpy(newString, string);
  *length = c;
  return newString;
}

void  AddValueToBlock(double *block, double val, int smpls, int cpuFeatures)
{
  register double *buffer, value;
  register int     samples;

  buffer  = block;
  value   = val;
  samples = smpls;

  if(cpuFeatures & CPU_FEATURE_SSE2)
  {
    __m128d t;
    double  add[4];

    add[0] = add[1] = add[2] = add[3] = value;
    t = _mm_loadu_pd(add);

    if(SSE_ALIGNED(buffer))
    { 
      // aligned
      while(samples >= 8)
      {
        _mm_store_pd(buffer, _mm_add_pd(_mm_load_pd(buffer), t));
        buffer += 4;
        _mm_store_pd(buffer, _mm_add_pd(_mm_load_pd(buffer), t));
        buffer += 4;
        samples -= 8;
      }
    }
    else
    {
#ifdef _DEBUG
		DebugPrint("AddValueToBlock(%x) - Unaligned Address\n", buffer);
#endif
		// unaligned
      while(samples >= 8)
      {
        _mm_storeu_pd(buffer, _mm_add_pd(_mm_loadu_pd(buffer), t));
        buffer += 4;
        _mm_storeu_pd(buffer, _mm_add_pd(_mm_loadu_pd(buffer), t));
        buffer += 4;
        samples -= 8;
	  }
    }
  }
  else
  {
    while(samples >= 8)
    {
      *(buffer++) += value;
      *(buffer++) += value;
      *(buffer++) += value;
      *(buffer++) += value;
      *(buffer++) += value;
      *(buffer++) += value;
      *(buffer++) += value;
      *(buffer++) += value;
      samples -= 8;
    }
  }

  while(samples-- > 0)
    *(buffer++) += value;
}

void  MixBlocks(double *buffer1, double *buffer2, int samples, double amount, bool replace, bool multiply, int cpuFeatures)
{
  __m128d m;
  
  if(cpuFeatures & CPU_FEATURE_SSE2)
    m = _mm_load1_pd(&amount);

  if(replace)
  {
    if(cpuFeatures & CPU_FEATURE_SSE2)
    {
      if(SSE_ALIGNED2(buffer1, buffer2))
      { 
        // aligned
        while(samples >= 8)
        {
          _mm_store_pd(buffer1, _mm_mul_pd(m, _mm_load_pd(buffer2)));
          buffer1 += 2;
          buffer2 += 2;
          _mm_store_pd(buffer1, _mm_mul_pd(m, _mm_load_pd(buffer2)));
          buffer1 += 2;
          buffer2 += 2;
          _mm_store_pd(buffer1, _mm_mul_pd(m, _mm_load_pd(buffer2)));
          buffer1 += 2;
          buffer2 += 2;
          _mm_store_pd(buffer1, _mm_mul_pd(m, _mm_load_pd(buffer2)));
          buffer1 += 2;
          buffer2 += 2;
          samples -= 8;
        }
      }
      else
      {
#ifdef _DEBUG
		DebugPrint("MixBlocks(%x, %x) - Unaligned Address\n", buffer1, buffer2);
#endif
        // unaligned
        while(samples >= 8)
        {
          _mm_storeu_pd(buffer1, _mm_mul_pd(m, _mm_loadu_pd(buffer2)));
          buffer1 += 2;
          buffer2 += 2;
          _mm_storeu_pd(buffer1, _mm_mul_pd(m, _mm_loadu_pd(buffer2)));
          buffer1 += 2;
          buffer2 += 2;
          _mm_storeu_pd(buffer1, _mm_mul_pd(m, _mm_loadu_pd(buffer2)));
          buffer1 += 2;
          buffer2 += 2;
          _mm_storeu_pd(buffer1, _mm_mul_pd(m, _mm_loadu_pd(buffer2)));
          buffer1 += 2;
          buffer2 += 2;
          samples -= 8;
        }
      }
    }
    else
    {
      while(samples >= 8)
      {
      *(buffer1++) = *(buffer2++) * amount;
      *(buffer1++) = *(buffer2++) * amount;
      *(buffer1++) = *(buffer2++) * amount;
      *(buffer1++) = *(buffer2++) * amount;
      *(buffer1++) = *(buffer2++) * amount;
      *(buffer1++) = *(buffer2++) * amount;
      *(buffer1++) = *(buffer2++) * amount;
      *(buffer1++) = *(buffer2++) * amount;
      samples -= 8;
      }
    }
    while(samples-- > 0)
      *(buffer1++) = *(buffer2++) * amount;
  }
  else
  {
    if(multiply)
    {
      if(cpuFeatures & CPU_FEATURE_SSE2)
      {
        __m128d a, b;

        if(SSE_ALIGNED2(buffer1, buffer2))
        { 
         // aligned
          while(samples >= 8)
          {
            a = _mm_load_pd(buffer1); 
            b = _mm_load_pd(buffer2);
            b = _mm_mul_pd(m, _mm_mul_pd(a, b)); 
            a = _mm_add_pd(a, b);
            _mm_store_pd(buffer1, a);
            buffer1 += 2;
            buffer2 += 2;
            a = _mm_load_pd(buffer1); 
            b = _mm_load_pd(buffer2);
            b = _mm_mul_pd(m, _mm_mul_pd(a, b)); 
            a = _mm_add_pd(a, b);
            _mm_store_pd(buffer1, a);
            buffer1 += 2;
            buffer2 += 2;
            a = _mm_load_pd(buffer1); 
            b = _mm_load_pd(buffer2);
            b = _mm_mul_pd(m, _mm_mul_pd(a, b)); 
            a = _mm_add_pd(a, b);
            _mm_store_pd(buffer1, a);
            buffer1 += 2;
            buffer2 += 2;
            a = _mm_load_pd(buffer1); 
            b = _mm_load_pd(buffer2);
            b = _mm_mul_pd(m, _mm_mul_pd(a, b)); 
            a = _mm_add_pd(a, b);
            _mm_store_pd(buffer1, a);
            buffer1 += 2;
            buffer2 += 2;
            samples -= 8;
          }
        }
        else
        {
#ifdef _DEBUG
  DebugPrint("MixBlocks(%x, %x) - Unaligned Address\n", buffer1, buffer2);
#endif
           // unaligned
          while(samples >= 8)
          {
            a = _mm_loadu_pd(buffer1); 
            b = _mm_loadu_pd(buffer2);
            b = _mm_mul_pd(m, _mm_mul_pd(a, b)); 
            a = _mm_add_pd(a, b);
            _mm_storeu_pd(buffer1, a);
            buffer1 += 2;
            buffer2 += 2;
            a = _mm_loadu_pd(buffer1); 
            b = _mm_loadu_pd(buffer2);
            b = _mm_mul_pd(m, _mm_mul_pd(a, b)); 
            a = _mm_add_pd(a, b);
            _mm_storeu_pd(buffer1, a);
            buffer1 += 2;
            buffer2 += 2;
            a = _mm_loadu_pd(buffer1); 
            b = _mm_loadu_pd(buffer2);
            b = _mm_mul_pd(m, _mm_mul_pd(a, b)); 
            a = _mm_add_pd(a, b);
            _mm_storeu_pd(buffer1, a);
            buffer1 += 2;
            buffer2 += 2;
            a = _mm_loadu_pd(buffer1); 
            b = _mm_loadu_pd(buffer2);
            b = _mm_mul_pd(m, _mm_mul_pd(a, b)); 
            a = _mm_add_pd(a, b);
            _mm_storeu_pd(buffer1, a);
            buffer1 += 2;
            buffer2 += 2;
            samples -= 8;
          }
        }
      }
      else
      {
        while(samples >= 8)
        {
          buffer1[0] += buffer1[0] * buffer2[0] * amount;
          buffer1[1] += buffer1[1] * buffer2[1] * amount;
          buffer1[2] += buffer1[2] * buffer2[2] * amount;
          buffer1[3] += buffer1[3] * buffer2[3] * amount;
          buffer1[4] += buffer1[4] * buffer2[4] * amount;
          buffer1[5] += buffer1[5] * buffer2[5] * amount;
          buffer1[6] += buffer1[6] * buffer2[6] * amount;
          buffer1[7] += buffer1[7] * buffer2[7] * amount;
          samples -= 8;
          buffer1 += 8;
          buffer2 += 8;
        }
      }
      while(samples-- > 0)
      {
        *(buffer1) += *(buffer1) * *(buffer2++) * amount;
        buffer1++;
      }
    }
    else
    {
      // buffer1 += buffer2 * amount
      if(cpuFeatures & CPU_FEATURE_SSE2)
      {
        if(SSE_ALIGNED2(buffer1, buffer2))
        { 
          // aligned
          while(samples >= 8)
          {
            _mm_store_pd(buffer1, _mm_add_pd(_mm_load_pd(buffer1), _mm_mul_pd(m, _mm_load_pd(buffer2)))); 
            buffer1 += 2;
            buffer2 += 2;
            _mm_store_pd(buffer1, _mm_add_pd(_mm_load_pd(buffer1), _mm_mul_pd(m, _mm_load_pd(buffer2)))); 
            buffer1 += 2;
            buffer2 += 2;
            _mm_store_pd(buffer1, _mm_add_pd(_mm_load_pd(buffer1), _mm_mul_pd(m, _mm_load_pd(buffer2)))); 
            buffer1 += 2;
            buffer2 += 2;
            _mm_store_pd(buffer1, _mm_add_pd(_mm_load_pd(buffer1), _mm_mul_pd(m, _mm_load_pd(buffer2)))); 
            buffer1 += 2;
            buffer2 += 2;
            samples -= 8;
          }
        }
        else
        {
#ifdef _DEBUG
  DebugPrint("MixBlocks(%x, %x) - Unaligned Address\n", buffer1, buffer2);
#endif
          // unaligned
          while(samples >= 8)
          {
            _mm_storeu_pd(buffer1, _mm_add_pd(_mm_loadu_pd(buffer1), _mm_mul_pd(m, _mm_loadu_pd(buffer2)))); 
            buffer1 += 2;
            buffer2 += 2;
            _mm_storeu_pd(buffer1, _mm_add_pd(_mm_loadu_pd(buffer1), _mm_mul_pd(m, _mm_loadu_pd(buffer2)))); 
            buffer1 += 2;
            buffer2 += 2;
            _mm_storeu_pd(buffer1, _mm_add_pd(_mm_loadu_pd(buffer1), _mm_mul_pd(m, _mm_loadu_pd(buffer2)))); 
            buffer1 += 2;
            buffer2 += 2;
            _mm_storeu_pd(buffer1, _mm_add_pd(_mm_loadu_pd(buffer1), _mm_mul_pd(m, _mm_loadu_pd(buffer2)))); 
            buffer1 += 2;
            buffer2 += 2;
            samples -= 8;
          }
        }
      }
      else
      {
        while(samples >= 8)
        {
          *(buffer1++) += *(buffer2++) * amount;
          *(buffer1++) += *(buffer2++) * amount;
          *(buffer1++) += *(buffer2++) * amount;
          *(buffer1++) += *(buffer2++) * amount;
          *(buffer1++) += *(buffer2++) * amount;
          *(buffer1++) += *(buffer2++) * amount;
          *(buffer1++) += *(buffer2++) * amount;
          *(buffer1++) += *(buffer2++) * amount;
          samples -= 8;
        }
      }

      while(samples-- > 0)
        *(buffer1++) += *(buffer2++) * amount;
    }
  }
}

void CopyFixedString(int length, char *src, char *dst)
{
  int i;

  for(i = 0; i < length; i++)
  {
    dst[i] = src[i];
    if(dst[i] == 0)
      return;
  }
  dst[length] = 0;
}