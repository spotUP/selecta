#ifndef ADSR_H
#define ADSR_H

class CADSR
{
public:
       CADSR(int sampleRate, double attack, double decay, double sustain, double release);
  void SetAttack(double value);
  void SetDecay(double value);
  void SetSustain(double value);
  void SetRelease(double value);
  void SetAttackCurve(double value);
  void SetDecayCurve(double value);
  void SetReleaseCurve(double value);
  void TriggerAttack();
  void TriggerRelease();
  void Generate(int samples, double *envOut);
  void SetSampleRate(int sampleRate);
  bool IsTriggered();

private:
  double Attack, Decay, Sustain, Release;
  double AttackRate,  DecayRate,  ReleaseRate;
  double AttackCurve, DecayCurve, ReleaseCurve;
  double AttackScale, DecayScale, ReleaseScale;
  double Time;
  int   State, SampleRate;
};

#endif