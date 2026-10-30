#include "main.h"
#include <CDJ800SPI.h>

CDJ800SPI cdj;

void setup() {
    cdj.setText("HELLO CDJ");
    cdj.setTrackNumber(7);
    cdj.setBpm(128);
    cdj.setTempo(-250);                 // -2.50 %
    cdj.setDisplaySeparator(on);
    cdj.setProgressPercentage(leftToRight, 50);
    cdj.setDisplayJogPos(33);
    cdj.begin();
}

void loop() {
    if (cdj.update()) {
        cdj.setIndicator(Indicator::Cue, cdj.status().btnCue);
        if (cdj.pressed(Button::Play)) {
            // Handle the PLAY press here.
        }
        cdj.commit();
    }
}