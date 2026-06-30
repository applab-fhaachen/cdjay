#include <AudioTools.h>
#include "AudioTools/Communication/USB/USBAudioStream.h"


USBAudioStream audioIn;
I2SStream i2s;  // or any output
StreamCopy copier(i2s, audioIn);

// TODO: Check how to offer 2 Audio Formats (44100 and 48000)
AudioInfo info(44100, 2, 16);

void audioSetup();
