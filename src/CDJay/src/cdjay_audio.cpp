#include "cdjay_audio.h"

void audioSetup() {

  auto config = audioIn.defaultConfig(RX_MODE);
  config.copyFrom(info);
  audioIn.begin(config);


  auto i2s_cfg = i2s.defaultConfig(TX_MODE);
  i2s_cfg.copyFrom(info);
  i2s.begin(i2s_cfg);

}