#ifndef EQ_H
#define EQ_H

#define EQ_DEFAULT_LOWFREQ  880.0f
#define EQ_DEFAULT_HIGHFREQ 5000.0f

class CEQ
{
public:
       CEQ(double lowFreq, double highFreq, double sampleRate);
  void Work(double *in, double *out, double *outAux1, double *outAux2, int samples, double gain, double aux1, double aux2, double *max);
  void Work(double *in, double *out, int samples, double gain, double *max);
  void SetSampleRate(double lowFreq, double highFreq, double sampleRate);
  void SetGain(double low, double mid, double high);

private:
  // Filter #1 (Low band)

  double  lf;       // Frequency
  double  f1p0;     // Poles ...
  double  f1p1;    
  double  f1p2;
  double  f1p3;

  // Filter #2 (High band)

  double  hf;       // Frequency
  double  f2p0;     // Poles ...
  double  f2p1;
  double  f2p2;
  double  f2p3;

  // Sample history buffer

  double  sdm1;     // Sample data minus 1
  double  sdm2;     //                   2
  double  sdm3;     //                   3

  // Gain Controls

  double  lg, lgFollow;       // low  gain
  double  mg, mgFollow;       // mid  gain
  double  hg, hgFollow;       // high gain

  double   gainFollow, aux1Follow, aux2Follow;
};  

#endif 