#ifndef Voice_h
#define Voice_h
class Voice {
public:

  int arpNum = 0;
  int delay = 0;
  int octave = 0;
  Voice();
  int UpdateVoice();

  void SetNote(int val, bool delay, int optOctave,int optInstrument);
  void SetVolume(int val);
  void SetOctave(int val);
  void SetDelay(int val);
  void SetEnvelopeNum(int val);
  void SetEnvelopeLength(int val);
  void SetArpNum(int val);

private:
  // In-class initializers: on-device these relied on static zero-init of the
  // global Tracker; stack instances (native tests) must be safe too.
  int effect_ = 0;
  bool isDelay_ = false;
  int arpCount_ = 0;
  int envelopeLength_ = 60000;
  int envelope_ = 0;
  int envelopeNum_ = 0;
  int voiceNum_ = 0;
  int output_ = 0;
  int note_ = 0;
  int sampleHistory_[2000] = {};
  int sampleHistoryIndex_=0;

  float baseFreq_ = 1;
  float sampleIndex_ = 0;
  float sampleIndexNext_ = 0;
  float volume_ = 1;
  
  int ReadWaveform();
  int ReadDrumWaveform();
  int ReadSfxWaveform();
  float GetBaseFreq(int val,int ioctave);
  float GetVolumeRatio();
  float LerpSample(int sampleA, int sampleB, float ratio);
  void UpdateHistory(int);
  int GetHistorySample(int backOffset);
};
#endif