#include "cdjay_audio.h"
#include <AudioTools.h>
#include "AudioTools/Communication/USB/USBAudioStream.h"

USBAudioStream audioIn;
I2SStream i2s;  // or any output
StreamCopy copier(i2s, audioIn);

// // TODO: Check how to offer 2 Audio Formats (44100 and 48000)
AudioInfo audioInfo(44100, 2, 16);

void audioSetup() {

  auto config = audioIn.defaultConfig(RX_MODE);
  config.copyFrom(audioInfo);
  audioIn.begin(config);


  auto i2s_cfg = i2s.defaultConfig(TX_MODE);
  i2s_cfg.copyFrom(audioInfo);
  i2s.begin(i2s_cfg);

}

void audioLoop() {
  // Just Copy the data from USB to I2S
  copier.copy();
}