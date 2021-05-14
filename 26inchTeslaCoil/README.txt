I started this 26" Tesla Coil back in 2010, with a friend who was going to build 
an identical one. 

We made minor modifications to the DRSSTC "miniBrute" design by
Don McCauley, as described in his 2009 book.
https://www.easternvoltageresearch.com/drsstc-minibrute-reference-design-book/
It was a half-bridge circuit using two IXYS IXGN 60N60 600V 100A IGBTs. It sorta 
worked and created smallish (1 foot) arcs, but often blew up and destroyed 
IGBTs, diodes, and transient voltage supressors. When it worked, though, it 
played music reasonably well using the Arduino-based modulator that is described 
in another directory here. 

In 2013 I designed my own driver circuit PCB for the Tesla Coil, using the same 
IGBTs and the Avago ACPL-P434 optically coupled gate drivers. It fared no 
better, and I set aside the project for a number of years. 

In 2021 the COVID pandemic lockup inspired me to try again. This time I used 
Don McCauley's "universal" DRSSTC controller.
https://www.easternvoltageresearch.com/universal-drsstc-controller-1000a /. 
Following his advice, I bagged the 60N60 IGBTs and switched to Mitsubishi 
CM300DY-12NF 600V 300A dual IGBT "bricks". I used two of them in a full-bridge 
(push-pull) configuration. It is working well, and I have yet to blow anything 
up. 
