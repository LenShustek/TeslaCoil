   Tesla coil modulator 

   The purpose of the modulator is to produce binary pulses that
   turn Tesla coils on and off to produce polyphonic music.
   
   The only thing a Telsa coil can do is create an arc with a pulse that lasts no longer
   than about 500 microseconds. To sound a note, we generate pulses at the frequency of the
   note; imagine them happening at the rising edge of the sine wave. To play simultaneous
   notes, we send a pulse at the rising edge of any of the overlapping sine waves -- except
   that a pulse is omitted if one is already in progress for another note, or if there
   hasn't been a long enough rest time since the end of last pulse. (The maximum duty
   cycle to avoid blowing up the electronics is 5-10%.)  The width of the pulse
   controls the length of the arc and hence the volume, not the frequency.

   The software limits the pulses to keep the duty cycle, on average, below a
   specified maximum for the type of coil.

   The music comes from MIDI files converted to a simplified bytestream by the
   miditones program, https://github.com/LenShustek/miditones.
   - If instrument information for notes is included in the score using the -i option,
     it is used to direct which Tesla coil should play them, as specified by an array of
     structures (coil_instr_t) that give instrument assignments. They are reassigned if
     any of the "coil on" toggle switches are changed, even while the music is playing.
   - If volume ("velocity") information is included in the score with the -v option,
     it is used to modulate the volume by adjusting the pulse width for each note.
     
   The music player is a variation of the "polling" version of the Playtune family,
   https://github.com/LenShustek/playtune_poll.
   
   The front of the modulator box has the following controls and indicators:
     -power switch
     -green LED (used to mean "powered on")
     -red LED (used to mean "playing")
     -"action" pushbutton
     -rotary encoder for choosing tests or songs, www.amazon.com/gp/product/B07F26CT6B
     -128x64 1.3 inch OLED display, www.amazon.com/gp/product/B08J1D212N
     -four rotary "parameter" knobs connected to potentiometers
     -four "Tesla coil on" toggle switches

   The parameter knobs are used as follows:
     1: for music, pitch change from the default; start music with the knob centered
        for test pulse generation, the frequency from 1 Hz to 2 kHz
     2: for music, the overall volume, which controls how note volume becomes pulse width
        for test pulse generation, the pulse width from 5 to 350 usec
     3: for music, the required separation between pulses, from 10 to 2000 usec
     4: for music, tempo change from the default; start music with the knob centered
     
   The insides of the modulator box has the following:
     Teensy 3.2 Cortex M4 processor board with 256 KB flash,
       64 KB RAM, 4 interval timers, and 3 "flexible" timers
     9V battery and voltage regulators
     fiber optic transmitters for the various Tesla coils
     phono jack for driving an oscilloscope or speaker
     USB jack for downloading software

   For some videos of it in operation, see 
   https://www.youtube.com/playlist?list=PLsyKadpUj0KD5guph3T1-Ugx1_Yjd8lQO
   
Len Shustek, 2021
