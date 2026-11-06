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

#define PI 3.14159265f

/////////////////////////////////////////////////////////
// data shuffling into bit-reversed order
// data: array of doubles:
// re(0),im(0),re(1),im(1),...,re(size-1),im(size-1)
// it means that size=array_length/2 !!
//
// Source: Rabiner-Gold: Theory and Application of DSP,
// Prentice Hall,1978 

void unshuffle(double *data, long size)
{
	long i,j,k,l,m;
	double re,im;

  l=size-1;
  m=size>>1;
  for (i=0,j=0; i<l ; i++){
	    if (i<j){
				  re=data[j+j]; im=data[j+j+1];
				  data[j+j]=data[i+i]; data[j+j+1]=data[i+i+1];
				  data[i+i]=re; data[i+i+1]=im;
				  }
	    k=m;
	    while (k<=j){
				  j-=k;
				  k>>=1;	
				  }
	    j+=k;
        }
}

/////////////////////////////////////////////////////////
// Decimation-in-freq radix-2 in-place butterfly
// data: array of doubles:
// re(0),im(0),re(1),im(1),...,re(size-1),im(size-1)
// it means that size=array_length/2 !!
// 
// suggested use:
// intput in normal order
// output in bit-reversed order
//
// Source: Rabiner-Gold: Theory and Application of DSP,
// Prentice Hall,1978

void dif_butterfly(double *data,long size){

	long angle,astep,dl;
	double xr,yr,xi,yi,wr,wi,dr,di,ang;
	double *l1, *l2, *end, *ol2;

     astep=1;
	 end=data+size+size;
     for(dl=size;dl>1;dl>>=1,astep+=astep){
					  l1=data;
					  l2=data+dl;
                      for(;l2<end;l1=l2,l2=l2+dl){
							ol2=l2;
                            for(angle=0;l1<ol2;l1+=2,l2+=2){
                               ang=2*PI*angle/size;
                               wr=cos(ang);
                               wi=-sin(ang);
                               xr=*l1+*l2;
                               xi=*(l1+1)+*(l2+1);
                               dr=*l1-*l2;
                               di=*(l1+1)-*(l2+1);
                               yr=dr*wr-di*wi;
                               yi=dr*wi+di*wr;
                               *(l1)=xr;*(l1+1)=xi;
                               *(l2)=yr;*(l2+1)=yi;
                               angle+=astep;
							   }
					  }
	 }
}

/////////////////////////////////////////////////////////
// Decimation-in-time radix-2 in-place inverse butterfly
// data: array of doubles:
// re(0),im(0),re(1),im(1),...,re(size-1),im(size-1)
// it means that size=array_length/2 !!
//
// suggested use:
// intput in bit-reversed order
// output in normal order
//
// Source: Rabiner-Gold: Theory and Application of DSP,
// Prentice Hall,1978

void inverse_dit_butterfly(double *data,long size){

	long angle,astep,dl;
	double xr,yr,xi,yi,wr,wi,dr,di,ang;
	double *l1, *l2, *end, *ol2;

     astep=size>>1;
	 end=data+size+size;
     for(dl=2;astep>0;dl+=dl,astep>>=1){
					  l1=data;
					  l2=data+dl;
                      for(;l2<end;l1=l2,l2=l2+dl){
							ol2=l2;
                            for(angle=0;l1<ol2;l1+=2,l2+=2){
                               ang=2*PI*angle/size;
                               wr=cos(ang);
                               wi=sin(ang);
                               xr=*l1;
                               xi=*(l1+1);
                               yr=*l2;
                               yi=*(l2+1);
                               dr=yr*wr-yi*wi;
                               di=yr*wi+yi*wr;
                               *(l1)=xr+dr;*(l1+1)=xi+di;
                               *(l2)=xr-dr;*(l2+1)=xi-di;
                               angle+=astep;
							   }
					  }
	 }
}

/////////////////////////////////////////////////////////
// in-place Radix-2 FFT for complex values
// data: array of doubles:
// re(0),im(0),re(1),im(1),...,re(size-1),im(size-1)
// it means that size=array_length/2 !!
// 
// output is in similar order, normalized by array length
//
// Source: see the routines it calls ...

void FFT(double *data, long size)
{

	//double *l, *end;

	dif_butterfly(data,size);
	unshuffle(data,size);

	//end=data+size+size;
	//for(l=data;l<end;l++){*l=*l/size;};
}


/////////////////////////////////////////////////////////
// in-place Radix-2 inverse FFT for complex values
// data: array of doubles:
// re(0),im(0),re(1),im(1),...,re(size-1),im(size-1)
// it means that size=array_length/2 !!
// 
// output is in similar order, NOT normalized by 
// array length
//
// Source: see the routines it calls ...

void InverseFFT(double* data, long size)
{
  double *l, *end;

	unshuffle(data,size);
	inverse_dit_butterfly(data,size);

	end=data+size+size;
	for(l=data;l<end;l++){*l=*l/size;};
}
