
/******************************************************************************

     Playtune_poll

     An Arduino Tune Generator library that uses interrupt polling
     to play a polyphonic musical score.


                               About Playtune

   Playtune is a family of music players for Arduino-like microcontrollers. They
   each intepret a bytestream of commands that represent a polyphonic musical
   score, and play it using different techniques.

   The original Playtune that was first released in 2011 uses a separate hardware timer
   to generate a square wave for each note played simultaneously. The timers run at twice
   the frequency of the note being played, and the interrupt routine flips the output bit.
   It can play only as many simultaneous notes as there are timers available.
   https://github.com/LenShustek/arduino-playtune

   This second ("polling") version uses only one hardware timer that interrupts often, by
   default at 20 Khz, or once every 50 microseconds. The interrupt routine determines
   which, if any, of the currently playing notes need to be toggled.
   https://github.com/LenShustek/playtune_poll

   This third version also uses only one hardware timer interrupting frequently, but uses
   the hardware digital-to-analog converter on high-performance microntrollers like the
   Teensy to generate an analog wave that is the sum of stored samples of sounds. The
   samples scaled to the right frequency and volume, and any number of instrument samples
   can be used and mapped to MIDI patches. It currently only supports Teensy.
   https://github.com/LenShustek/playtune_samp

     (4) The fourth version is an audio object for the PJRC Audio Library.
   https://www.pjrc.com/teensy/td_libs_Audio.html
   It allows up to 16 simultaneous sound generators that are internally mixed, at
   the appropriate volume, to produce one monophonic audio stream.
   Sounds are created from sampled one-cycle waveforms for any number of instruments,
   each with its own attack-hold-decay-sustain-release envelope. Percussion sounds
   (from MIDI channel 10) are generated from longer sampled waveforms of a complete
   instrument strike. Each generator's volume is independently adjusted according to
   the MIDI velocity of the note being played before all channels are mixed.
   www.github.com/LenShustek/Playtune_synth

   (5) The fifth version is for the Teensy 3.1/3.2, and uses the four Periodic Interval
   Timers in the Cortex M4 processor to support up to 4 simultaneous notes.
   It uses less CPU time than the polling version, but is limited to 4 notes at a time.
   (This was written to experiment with multi-channel multi-Tesla Coil music playing,
   where I use Flexible Timer Module FTM0 for generating precise one-shot pulses.
   But I ultimately switched to the polling version to play more simultaneous notes.)
   www.github.com/LenShustek/Playtune_Teensy***** Details about this version: Playtune_poll

   The advantage of this polling scheme is that you can play more simultaneous notes
   than there are hardware timers. The disadvantages are that it takes more CPU power,
   and that notes at higher frequencies have a little bit of jitter that adds some
   low frequency "undertones". That problem disappears if you use a microcontroller
   like the Teensy, which uses a fast 32-bit ARM processor instead of the slow 8-bit
   AVR processors used by most Arduinos so the interrupt frequency can be higher.

   The interrupt routine must be really fast, regardless of how many notes are playing
   and being toggled. We try hard to be efficient by using the following techniques:
    - Do division incrementally by only one subtraction per channel at each interrupt.
    - Use direct hardware register bit manipulation instead of digitalWrite().
      (That requires assigning output pins at compile time, not at runtime.)
    - Unroll the loop for checking on channel transitions at compile time.
      (If you are unfamiliar with loop unrolling, see https://en.wikipedia.org/wiki/Loop_unrolling)

   Once a score starts playing, all of the processing happens in the interrupt routine.
   Any other "real" program can be running at the same time as long as it doesn't
   use the timer or the output pins that Playtune is using.

   The easiest way to hear the music is to connect each of the output pins to a resistor
   (500 ohms, say).  Connect other ends of the resistors together and then to one
   terminal of an 8-ohm speaker.  The other terminal of the speaker is connected to
   ground.  No other hardware is needed!  An external amplifier is good, though.

   By default there is no volume modulation.  All tones are played at the same volume, which
   makes some scores sound strange.  This is definitely not a high-quality synthesizer.

   There are two optional features you can enable at compile time:

   (1) If the DO_VOLUME compile-time switch is set to 1, then we interpret MIDI "velocity"
   information and try to modulate the volume of the output by controlling the duty cycle of the
   square wave. That doesn't allow much dynamic range, though, so we make the high pulse of
   the square wave vary in duration only from 1 to 10 interrupt-times wide.

   There's a mapping table from MIDI velocity to pulse width that you can play with. The
   MIDI standard actually doesn't specify how velocity is to be interpreted; for an interesting
   emprical study of what synthesizers actually do, see this paper by Roger Dannenberg of CMU:
   http://www.cs.cmu.edu/~rbd/papers/velocity-icmc2006.pdf

   In order for volume modulation to work, the bytestream must include volume information,
   which can be generated by Miditones with the -v option. (It's best to also use -d, so
   that the self-describing header is generated.)

   (2) If the DO_PERCUSSION compile-time switch is also set to 1, then we interpret notes on
   MIDI channel 9 (10 if you start counting with 1) to be percussion sounds, which we play
   with a high-frequency chirp that is dithered with a pseudo-random number. Tables set
   the frequency and duration of the chirp differently for the various notes that represent
   different percussion instruments. It will only play one percussion sound at a time.

   In order for this to work, the bytestream must have the notes on the percussion track
   relocated from 0..127 to 128..255, which Miditones will do with the -pt option in
   addition to the -v option.  (Again, it best to also use -d.)


******  Using Playtune_poll  *****

   Unlike the original Playtune, this is not configured as a library because we must
   make compile-time changes for pin assignments. You should create a sketch directory
   with the following files in it:

     Playtune_poll.ino       This file, which has most of the code
     Playtune_poll.h         The header file, which defines the output pin configuration
     Playtune_poll_test.ino  The main program, which contains the score(s) you wish to
                              play, and any other code you want to run.

   In order to assign the output pins at compile time, you must add lines to the
   Playtune_poll.h file. How you do that depends on the kind of microcontroller you
   are using; see the instructions and examples in that file.

   You can define up to MAX_CHANS channels, which is 8 by default. You can
   increase MAX_CHANS up to 16. There is some inefficiency if MAX_CHANS is much
   larger than the number of channels actually being used.

   There are five public functions and one public variable that you can use
   in your runtime code in Playtune_poll_test.ino.

   void tune_start_timer(int microseconds)

    This is optional. Call it to set how often notes should be checked for transitions,
    from 5 to 50 microseconds. If you don't call it, we'll pick something that seems
    appropriate from the type of microcontroller and the frequency it's running at.

   void tune_playscore(byte *score)

     Call this pointing to a "score bytestream" to start playing a tune.  It will
     only play as many simultaneous notes as you have defined output pins; any
     more will be ignored.  See below for the format of the score bytestream.

   boolean tune_playing

     This global variable will be "true" if a score is playing, and "false" if not.
     You can use this to see when a score has finished.

   void tune_speed(unsigned int percent)

    New for the Teensy version, this changes playback speed to the specified percent
    of normal. The minimum is percent=20 (1/5 slow speed) and the maximum is
    percent=500 (5x fast speed).

   void tune_stopscore()

     This will stop a currently playing score without waiting for it to end by itself.

   void tune_stop_timer()

     This stops playing and also stops the timer interrupt.
     Do this when you don't want to play any more tunes.


   *****  The score bytestream  *****

   The bytestream is a series of commands that can turn notes on and off, and can
   start a waiting period until the next note change.  Here are the details, with
   numbers shown in hexadecimal.

   If the high-order bit of the byte is 1, then it is one of the following commands:

     9t nn  Start playing note nn on tone generator t.  Generators are numbered
            starting with 0.  The notes numbers are the MIDI numbers for the chromatic
            scale, with decimal 60 being Middle C, and decimal 69 being Middle A
            at 440 Hz.  The highest note is decimal 127 at about 12,544 Hz. except
            that percussion notes (instruments, really) range from 128 to 255.

            [vv]  If DO_VOLUME is set to 1, then we expect another byte with the velocity
            value from 1 to 127. You can generate this from Miditones with the -v switch.
            Everything breaks if vv is present with DO_VOLIUME set 0, and vice versa!

     8t     Stop playing the note on tone generator t.

     Ct ii  Change tone generator t to play instrument ii from now on. This version of
            Playtune ignores instrument information if it is present.

     F0     End of score: stop playing.

     E0     End of score: start playing again from the beginning.

   If the high-order bit of the byte is 0, it is a command to wait.  The other 7 bits
   and the 8 bits of the following byte are interpreted as a 15-bit big-endian integer
   that is the number of milliseconds to wait before processing the next command.
   For example,

     07 D0

   would cause a wait of 0x07d0 = 2000 decimal millisconds or 2 seconds.  Any tones
   that were playing before the wait command will continue to play.

   The score is stored in Flash memory ("PROGMEM") along with the program, because
   there's a lot more of that than data memory.


*  *****  Where does the score data come from?  *****

   Well, you can write the score by hand from the instructions above, but that's
   pretty hard.  An easier way is to translate MIDI files into these score commands,
   and I've written a program called "Miditones" to do that.  See the separate
   documentation for that program, which is also open source at
   https://github.com/lenshustek/miditones


*  *****  Nostalgia from me  *****

   Writing Playtune was a lot of fun, because it essentially duplicates what I did
   as a graduate student at Stanford University in about 1973.  That project used the
   then-new Intel 8008 microprocessor, plus three hardware square-wave generators that
   I built out of 7400-series TTL.  The music compiler was written in Pascal and read
   scores that were hand-written in a notation I made up, which looked something like
   this:     C  Eb  4G  8G+  2R  +  F  D#
   This was before MIDI had been invented, and anyway I wasn't a pianist so I would
   not have been able to record my own playing.  I could barely read music well enough
   to transcribe scores, but I slowly did quite a few of them. MIDI is better!

   Len Shustek, originally 4 Feb 2011,
   but updated for the polling version in July 2016.

   ------------------------------------------------------------------------------------

   The MIT License (MIT)
   Copyright (c) 2011, 2016, Len Shustek

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to use,
   copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
   Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

 **************************************************************************/
/*
   Change log
   19 January 2011, L.Shustek, V1.0
      - Initial release.
   23 February 2011, L. Shustek, V1.1
      - prevent hang if delay rounds to count of 0
    4 December 2011, L. Shustek, V1.2
      - add special TESLA_COIL mods
   10 June 2013, L. Shustek, V1.3
      - change for compatibility with Arduino IDE version 1.0.5
    6 April 2015, L. Shustek, V1.4
      - change for compatibility with Arduino IDE version 1.6.x
   28 May 2016, T. Wasiluk
      - added support for ATmega32U4
   10 July 2016, Nick Shvelidze
      - Fixed include file names for Arduino 1.6 on Linux.
   11 July 2017, Len Shustek
      - Start this derivative version that uses polling instead of a timer
        for each channel. It needs a fast processor, but can play an arbitrary
        number of notes simultaneously using only one timer.
   19 July 2016, Len Shustek
      - Add crude volume modulation and percussion sounds. Thanks go to
        Connor Nishijima for providing his code and goading us into doing this.
    9 August 2016, Len Shustek
      - Support the optional bytestream header, and instrument change data.
   23 April 2021, Len Shustek
      - Create TESLA_COIL version that only works for Teensy
      - Add tune_speed()
   21 June 2021, Len Shustek
      - swtich from TimerOne.h to IntervalTimer, which allos 4 objects
*/

#define TESLA_COIL 1        // special version for Tesla coils? 

#include <Arduino.h>
#include "Playtune_poll.h"  // this contains, among other things, the output pin definitions

#define DO_VOLUME 1         // generate volume-modulating code? Needs -v on Miditones.
#define DO_PERCUSSION 0     // generate percussion sounds? Needs DO_VOLUME, and -pt on Miditones

#define ASSUME_VOLUME  0    // if the file has no header, should we assume there is volume info?

#define DBUG 0  // debugging? (screws up timing!)

#if TESLA_COIL
   #define DO_VOLUME 1
   #define DO_PERCUSSION 0
   #ifndef CORE_TEENSY
      Tesla coil version is only for teensy
   #endif
   void teslacoil_rising_edge(byte channel);
   byte teslacoil_checknote(byte channel, byte note);
   void teslacoil_change_instrument(byte channel, byte instrument);
   void teslacoil_change_volume(byte channel, byte volume);
#endif

// A general-purpose macro joiner that allow rescanning of the result
// (Yes, we need both levels! See http://stackoverflow.com/questions/1489932)
#define PASTER(x,y) x ## y
#define JOIN(x,y) PASTER(x,y)

volatile boolean tune_playing = false;
static byte num_chans = 	// how many channels
#if TESLA_COIL
   MAX_CHANS; // can be played on Tesla coils
#else
   0;         // have been assigned to pins
#endif
   static byte num_chans_used = 0;
static boolean pins_initialized = false;
static boolean timer_running = false;
static boolean volume_present = ASSUME_VOLUME;

static const byte *score_start = 0;
const byte *score_cursor = 0; // public so callers can monitor how far along we are
static unsigned _tune_speed = 100;

static unsigned /*short*/ int scorewait_interrupt_count;
static unsigned /*short*/ int millisecond_interrupt_count;
static unsigned /*short*/ int interrupts_per_millisecond;

static int32_t accumulator [MAX_CHANS];
static int32_t decrement [MAX_CHANS];
static byte playing [MAX_CHANS];
static byte pins [MAX_CHANS];
#if TESLA_COIL
static byte _tune_volume[MAX_CHANS] = {0};
#endif
#if DO_VOLUME && !TESLA_COIL
   static byte max_high_ticks[MAX_CHANS];
   static byte current_high_ticks[MAX_CHANS];
#endif
#if DO_PERCUSSION
   #define NO_DRUM 0xff
   static byte drum_chan = NO_DRUM; // channel playing drum now
   static byte drum_tick_count; // count ticks before bit change based on random bit
   static byte drum_tick_limit; // count to this
   static int drum_duration;   // limit on drum duration
#endif

struct file_hdr_t {  // the optional bytestream file header
   char id1;     // 'P'
   char id2;     // 't'
   unsigned char hdr_length; // length of whole file header
   unsigned char f1;         // flag byte 1
   unsigned char f2;         // flag byte 2
   unsigned char num_tgens;  // how many tone generators are used by this score
} file_header;
#define HDR_F1_VOLUME_PRESENT 0x80
#define HDR_F1_INSTRUMENTS_PRESENT 0x40
#define HDR_F1_PERCUSSION_PRESENT 0x20

// note commands in the bytestream
#define CMD_PLAYNOTE  0x90   /* play a note: low nibble is generator #, note is next byte, maybe volume */
#define CMD_STOPNOTE  0x80   /* stop a note: low nibble is generator # */
#define CMD_INSTRUMENT  0xc0 /* change instrument; low nibble is generator #, instrument is next byte */
#define CMD_RESTART 0xe0      /* restart the score from the beginning */
#define CMD_STOP  0xf0        /* stop playing */
/* if CMD < 0x80, then the other 7 bits and the next byte are a 15-bit big-endian number of msec to wait */

/* ------------------------------------------------------------------------------
    We use various tables to control how notes are played:

    -- accumulator decrement values for note half-periods, to set the frequency
    -- number of ticks the high output should last for different volumes
    -- approximate number of ticks per note periods, so the high isn't too long
    -- number of ticks between transitions for the percussion notes
    -- the maximum duration of percussion notes

   Some of these depend on the polling frequency and the accumulator restart value.

   To generate a tone, we basically do incremental division for each channel in the
   polling interrupt routine to determine when the output needs to be flipped:

        accum -= decrement
        if (accum < 0) {
            toggle speaker output
            accum += ACCUM_RESTART
        }

   The first table below contains the maximum decrement values for each note, corresponding
   to the maximum delay between interrupts of 50 microseconds (20 Khz). You can use the
   companion spreadsheet to calculate other values.

   Another table contains the minimum number of ticks for a full period, which we use to
   cap the number of ticks that the signal is high for volume modulation. This only has
   an effect on notes above about 92.

   Another table contains the minimum number of ticks to wait before changing the
   percussion output based on a random variable. This essentially sets the frequency
   of the percussion chirp.

   When we start the timer, we create the tables we actually use in RAM. Those are based on a
   possibly higher interrupt frequency that we are told to use, or we picked based on the type
   and speed of the processor.  The actual decrements will be smaller because the interrupt
   is happening more often, so there is no danger of introducing 32-bit overflow. The tick
   counts will be larger, so more volumes are possible for high notes.
   Everything gets better at higher frequencies, including how the notes sound!
*/

#define ACCUM_RESTART 1073741824L  // 2^30. Generates 1-byte arithmetic on 4-byte numbers!
#define MAX_NOTE 123 // the max note number; we thus allow MAX_NOTE+1 notes
#define MAX_POLLTIME_USEC 50
#define MIN_POLLTIME_USEC 5
const int32_t max_decrement_PGM[MAX_NOTE + 1] PROGMEM = {
   877870L, 930071L, 985375L, 1043969L, 1106047L, 1171815L, 1241495L, 1315318L,
   1393531L, 1476395L, 1564186L, 1657197L, 1755739L, 1860141L, 1970751L,
   2087938L, 2212093L, 2343631L, 2482991L, 2630637L, 2787063L, 2952790L,
   3128372L, 3314395L, 3511479L, 3720282L, 3941502L, 4175876L, 4424186L,
   4687262L, 4965981L, 5261274L, 5574125L, 5905580L, 6256744L, 6628789L,
   7022958L, 7440565L, 7883004L, 8351751L, 8848372L, 9374524L, 9931962L,
   10522547L, 11148251L, 11811160L, 12513488L, 13257579L, 14045916L, 14881129L,
   15766007L, 16703503L, 17696745L, 18749048L, 19863924L, 21045095L, 22296501L,
   23622320L, 25026976L, 26515158L, 28091831L, 29762258L, 31532014L, 33407005L,
   35393489L, 37498096L, 39727849L, 42090189L, 44593002L, 47244640L, 50053953L,
   53030316L, 56183662L, 59524517L, 63064029L, 66814011L, 70786979L, 74996192L,
   79455697L, 84180379L, 89186005L, 94489281L, 100107906L, 106060631L,
   112367325L, 119049034L, 126128057L, 133628022L, 141573958L, 149992383L,
   158911395L, 168360758L, 178372009L, 188978561L, 200215811L, 212121263L,
   224734649L, 238098067L, 252256115L, 267256044L, 283147915L, 299984767L,
   317822789L, 336721516L, 356744019L, 377957122L, 400431622L, 424242525L,
   449469299L, 476196134L, 504512230L, 534512088L, 566295831L, 599969533L,
   635645578L, 673443031L, 713488038L, 755914244L, 800863244L, 848485051L,
   898938597L, 952392268L, 1009024459L, 1069024176L };
static int32_t decrement_table[MAX_NOTE + 1]; // values may be smaller in this actual table used

#if DO_VOLUME
const uint16_t min_ticks_per_period_PGM[MAX_NOTE + 1] PROGMEM = {
   2446, 2308, 2178, 2056, 1940, 1832, 1728, 1632, 1540, 1454, 1372,
   1294, 1222, 1154, 1088, 1028, 970, 916, 864, 816, 770, 726, 686, 646,
   610, 576, 544, 514, 484, 458, 432, 408, 384, 362, 342, 322, 304, 288,
   272, 256, 242, 228, 216, 204, 192, 180, 170, 160, 152, 144, 136, 128,
   120, 114, 108, 102, 96, 90, 84, 80, 76, 72, 68, 64, 60, 56, 54, 50,
   48, 44, 42, 40, 38, 36, 34, 32, 30, 28, 26, 24, 24, 22, 20, 20, 18,
   18, 16, 16, 14, 14, 12, 12, 12, 10, 10, 10, 8, 8, 8, 8, 6, 6, 6, 6, 6,
   4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 };
static uint16_t ticks_per_period[MAX_NOTE + 1];  // values may be larger in this actual table used

const byte min_high_ticks_PGM[128] PROGMEM = {
   // A table showing how many ticks output high should last for each volume setting.
   // This is pretty arbitrary, based on
   //   - where MIDI tracks tend to change most
   //   - that low numbers make quite distinct volumes.
   //   - that numbers above 10 are too big for even reasonable high notes
   // Some synthesizers use this mapping from score notation to the center of a range:
   // fff=120, ff=104, f=88, mf=72, mp=56, p=40, pp=24, ppp=8

   /*  00- 15 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   /*  16- 31 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   /*  32- 47 */  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   /*  48- 63 */  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
   /*  64- 79 */  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
   /*  80- 95 */  5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6,
   /*  96-111 */  7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
   /* 112-127 */  9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10 };
#endif
static byte high_ticks[128]; // values may be larger in this actual table used

#if DO_PERCUSSION
const byte min_drum_ticks_PGM[128] PROGMEM = { // how often to output bits, in #ticks
   // common values are: 35,36:bass drum, 38,40:snare, 42,44,46:highhat
   // This is a pretty random table!
   /*  00- 15 */  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   /*  16- 31 */  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   /*  32- 47 */  2, 2, 2, 8, 8, 2, 5, 3, 4, 3, 3, 2, 3, 2, 3, 2,
   /*  48- 63 */  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   /*  64- 79 */  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   /*  80- 95 */  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   /*  96-111 */  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   /* 112-127 */  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 };
static byte drum_ticks[128]; // values may be larger in this actual table used

const uint16_t min_drum_cap_PGM[128] PROGMEM = { // cap on drum duration, in #ticks. 500=25 msec
   /*  00- 15 */  500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
   /*  16- 31 */  500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
   /*  32- 47 */  500, 500, 500, 750, 750, 500, 800, 500, 800, 750, 750, 500, 750, 500, 750, 500,
   /*  48- 63 */  500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
   /*  64- 79 */  500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
   /*  80- 95 */  500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
   /*  96-111 */  500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
   /* 112-127 */  500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500 };
static uint16_t drum_cap[128]; // values may be larger in this actual table used
#endif

void tune_stopnote (byte chan);
void tune_stepscore (void);
void timer_ISR(void);
IntervalTimer playtune_timer;

//------------------------------------------------------------------------------
// Initialize and start the timer
//------------------------------------------------------------------------------

void tune_start_timer(int polltime) {
   // Decide on an interrupt poll time
   if (polltime) // set by the caller
      polltime = max( min(polltime, MAX_POLLTIME_USEC), MIN_POLLTIME_USEC);
   else { // polltime isn't specified; try to pick a good one
      polltime = MAX_POLLTIME_USEC;  // assume the worst
      if (F_CPU >= 48000000L) polltime = 25; // unless the clock is really fast
      #ifdef CORE_TEENSY
      if (F_CPU < 50000000L)
         polltime = 20;    // slow Teensies
      else polltime = 10; // fast teensies
      #endif
   }
   interrupts_per_millisecond = 1000 / polltime;  // this is a truncated approximation
   millisecond_interrupt_count = interrupts_per_millisecond;
   #if DBUG
   Serial.print("polltime in usec: "); Serial.println(polltime);
   #endif

   // Set up the real tables we use based on our chosen polling rate, which may be faster
   // than the 50 microseconds that the table was computed for. We use 32- or 64-bit arithmetic
   // for intermediate results to avoid errors, but we only do this during initialization.

   for (int note = 0; note <= MAX_NOTE; ++note) {
      decrement_table[note] =
         ((uint64_t) pgm_read_dword(max_decrement_PGM + note) * (uint64_t)polltime) / MAX_POLLTIME_USEC
         #if DO_VOLUME
         >> 1; // if we're doing volume, use the full note period, not the half period
      ticks_per_period[note] = ((uint32_t)pgm_read_word(min_ticks_per_period_PGM + note) * MAX_POLLTIME_USEC) / polltime;
         #endif
      ; }
   #if DO_VOLUME
   for (int index = 0; index < 128; ++index)
      high_ticks[index] = ((uint32_t)pgm_read_byte(min_high_ticks_PGM + index) * MAX_POLLTIME_USEC) / polltime;
   #endif

   #if DO_PERCUSSION
   for (int index = 0; index < 128; ++index) {
      drum_ticks[index] = ((uint32_t)pgm_read_byte(min_drum_ticks_PGM + index) * MAX_POLLTIME_USEC) / polltime;
      drum_cap[index] = ((uint32_t)pgm_read_word(min_drum_cap_PGM + index) * MAX_POLLTIME_USEC) / polltime; }
   #endif
   playtune_timer.begin(timer_ISR, polltime); // start the timer
   timer_running = true; }

//------------------------------------------------------------------------------
// Set up output pins, compile-time macros for manipulating them,
// and runtime bit set and clear routines that are faster than digitalWrite
//------------------------------------------------------------------------------

void tune_init_pins(void) {

   #if SCOPE_TEST
   pinMode(SCOPE_PIN, OUTPUT);
   digitalWrite(SCOPE_PIN, 0);
   #endif

   #if TESLA_COIL
#define tune_initchan(pin) \
   if (num_chans < MAX_CHANS) \
      ++num_chans;
   #else
#define tune_initchan(pin)   \
   if (num_chans < MAX_CHANS) {  \
      pins[num_chans] = pin; \
      pinMode(pin, OUTPUT);  \
      ++num_chans;  \
   }
   #endif
   #ifndef CHAN_0_PIN
#define CHAN_0_PIN 0
#define CHAN_0_REG B
#define CHAN_0_BIT 0
   #else
   tune_initchan(CHAN_0_PIN)
   #endif
#define CHAN_0_PINREG JOIN(PIN,CHAN_0_REG)
#define CHAN_0_PORTREG JOIN(PORT,CHAN_0_REG)


   #ifndef CHAN_1_PIN
#define CHAN_1_PIN 0
#define CHAN_1_REG B
#define CHAN_1_BIT 0
   #else
   tune_initchan(CHAN_1_PIN)
   #endif
#define CHAN_1_PINREG JOIN(PIN,CHAN_1_REG)
#define CHAN_1_PORTREG JOIN(PORT,CHAN_1_REG)

   #ifndef CHAN_2_PIN
#define CHAN_2_PIN 0
#define CHAN_2_REG B
#define CHAN_2_BIT 0
   #else
   tune_initchan(CHAN_2_PIN)
   #endif
#define CHAN_2_PINREG JOIN(PIN,CHAN_2_REG)
#define CHAN_2_PORTREG JOIN(PORT,CHAN_2_REG)

   #ifndef CHAN_3_PIN
#define CHAN_3_PIN 0
#define CHAN_3_REG B
#define CHAN_3_BIT 0
   #else
   tune_initchan(CHAN_3_PIN)
   #endif
#define CHAN_3_PINREG JOIN(PIN,CHAN_3_REG)
#define CHAN_3_PORTREG JOIN(PORT,CHAN_3_REG)

   #ifndef CHAN_4_PIN
#define CHAN_4_PIN 0
#define CHAN_4_REG B
#define CHAN_4_BIT 0
   #else
   tune_initchan(CHAN_4_PIN)
   #endif
#define CHAN_4_PINREG JOIN(PIN,CHAN_4_REG)
#define CHAN_4_PORTREG JOIN(PORT,CHAN_4_REG)

   #ifndef CHAN_5_PIN
#define CHAN_5_PIN 0
#define CHAN_5_REG B
#define CHAN_5_BIT 0
   #else
   tune_initchan(CHAN_5_PIN)
   #endif
#define CHAN_5_PINREG JOIN(PIN,CHAN_5_REG)
#define CHAN_5_PORTREG JOIN(PORT,CHAN_5_REG)

   #ifndef CHAN_6_PIN
#define CHAN_6_PIN 0
#define CHAN_6_REG B
#define CHAN_6_BIT 0
   #else
   tune_initchan(CHAN_6_PIN)
   #endif
#define CHAN_6_PINREG JOIN(PIN,CHAN_6_REG)
#define CHAN_6_PORTREG JOIN(PORT,CHAN_6_REG)

   #ifndef CHAN_7_PIN
#define CHAN_7_PIN 0
#define CHAN_7_REG B
#define CHAN_7_BIT 0
   #else
   tune_initchan(CHAN_7_PIN)
   #endif
#define CHAN_7_PINREG JOIN(PIN,CHAN_7_REG)
#define CHAN_7_PORTREG JOIN(PORT,CHAN_7_REG)

   #ifndef CHAN_8_PIN
#define CHAN_8_PIN 0
#define CHAN_8_REG B
#define CHAN_8_BIT 0
   #else
   tune_initchan(CHAN_8_PIN)
   #endif
#define CHAN_8_PINREG JOIN(PIN,CHAN_8_REG)
#define CHAN_8_PORTREG JOIN(PORT,CHAN_8_REG)

   #ifndef CHAN_9_PIN
#define CHAN_9_PIN 0
#define CHAN_9_REG B
#define CHAN_9_BIT 0
   #else
   tune_initchan(CHAN_9_PIN)
   #endif
#define CHAN_9_PINREG JOIN(PIN,CHAN_9_REG)
#define CHAN_9_PORTREG JOIN(PORT,CHAN_9_REG)

   #ifndef CHAN_10_PIN
#define CHAN_10_PIN 0
#define CHAN_10_REG B
#define CHAN_10_BIT 0
   #else
   tune_initchan(CHAN_10_PIN)
   #endif
#define CHAN_10_PINREG JOIN(PIN,CHAN_10_REG)
#define CHAN_10_PORTREG JOIN(PORT,CHAN_10_REG)

   #ifndef CHAN_11_PIN
#define CHAN_11_PIN 0
#define CHAN_11_REG B
#define CHAN_11_BIT 0
   #else
   tune_initchan(CHAN_11_PIN)
   #endif
#define CHAN_11_PINREG JOIN(PIN,CHAN_11_REG)
#define CHAN_11_PORTREG JOIN(PORT,CHAN_11_REG)

   #ifndef CHAN_12_PIN
#define CHAN_12_PIN 0
#define CHAN_12_REG B
#define CHAN_12_BIT 0
   #else
   tune_initchan(CHAN_12_PIN)
   #endif
#define CHAN_12_PINREG JOIN(PIN,CHAN_12_REG)
#define CHAN_12_PORTREG JOIN(PORT,CHAN_12_REG)

   #ifndef CHAN_13_PIN
#define CHAN_13_PIN 0
#define CHAN_13_REG B
#define CHAN_13_BIT 0
   #else
   tune_initchan(CHAN_13_PIN)
   #endif
#define CHAN_13_PINREG JOIN(PIN,CHAN_13_REG)
#define CHAN_13_PORTREG JOIN(PORT,CHAN_13_REG)

   #ifndef CHAN_14_PIN
#define CHAN_14_PIN 0
#define CHAN_14_REG B
#define CHAN_14_BIT 0
   #else
   tune_initchan(CHAN_14_PIN)
   #endif
#define CHAN_14_PINREG JOIN(PIN,CHAN_14_REG)
#define CHAN_14_PORTREG JOIN(PORT,CHAN_14_REG)

   #ifndef CHAN_15_PIN
#define CHAN_15_PIN 0
#define CHAN_15_REG B
#define CHAN_15_BIT 0
   #else
   tune_initchan(CHAN_15_PIN)
   #endif
#define CHAN_15_PINREG JOIN(PIN,CHAN_15_REG)
#define CHAN_15_PORTREG JOIN(PORT,CHAN_15_REG)
   pins_initialized = true; }

#ifdef CORE_TEENSY
// On a Teensy, digitalWrite is not lighting fast, but isn't too bad
#define chan_set_high(chan) digitalWrite(pins[chan], HIGH)
#define chan_set_low(chan) digitalWrite(pins[chan], LOW)

#else // ARDUINO
// See http://garretlab.web.fc2.com/en/arduino/inside/index.html for a great explanation of Arduino I/O definitions
const uint16_t chan_portregs_PGM[16] PROGMEM = { // port addresses for each channel
   (uint16_t) &CHAN_0_PORTREG, (uint16_t) &CHAN_1_PORTREG, (uint16_t) &CHAN_2_PORTREG, (uint16_t) &CHAN_3_PORTREG,
   (uint16_t) &CHAN_4_PORTREG, (uint16_t) &CHAN_5_PORTREG, (uint16_t) &CHAN_6_PORTREG, (uint16_t) &CHAN_7_PORTREG,
   (uint16_t) &CHAN_8_PORTREG, (uint16_t) &CHAN_9_PORTREG, (uint16_t) &CHAN_10_PORTREG, (uint16_t) &CHAN_11_PORTREG,
   (uint16_t) &CHAN_12_PORTREG, (uint16_t) &CHAN_13_PORTREG, (uint16_t) &CHAN_14_PORTREG, (uint16_t) &CHAN_15_PORTREG };
const byte chan_bitmask_PGM[16] PROGMEM = {  // bit masks for each channel
   (1 << CHAN_0_BIT), (1 << CHAN_1_BIT), (1 << CHAN_2_BIT), (1 << CHAN_3_BIT),
   (1 << CHAN_4_BIT), (1 << CHAN_5_BIT), (1 << CHAN_6_BIT), (1 << CHAN_7_BIT),
   (1 << CHAN_8_BIT), (1 << CHAN_9_BIT), (1 << CHAN_10_BIT), (1 << CHAN_11_BIT),
   (1 << CHAN_12_BIT), (1 << CHAN_13_BIT), (1 << CHAN_14_BIT), (1 << CHAN_15_BIT) };
// macros to control channel output that are faster than digitalWrite
#define chan_set_high(chan) *(volatile byte *)pgm_read_word(chan_portregs_PGM + chan) |= pgm_read_byte(chan_bitmask_PGM + chan)
#define chan_set_low(chan)  *(volatile byte *)pgm_read_word(chan_portregs_PGM + chan) &= ~pgm_read_byte(chan_bitmask_PGM + chan);
#endif // ARDUINO

//------------------------------------------------------------------------------
// Start playing a note on a particular channel
//------------------------------------------------------------------------------

#if DO_VOLUME
void tune_playnote (byte chan, byte note, byte volume) {
   #if DBUG
   Serial.print("chan="); Serial.print(chan);
   Serial.print(" note="); Serial.print(note);
   Serial.print(" vol="); Serial.println(volume);
   #endif
   if (chan < num_chans) {
      if (note <= MAX_NOTE) {
         if (volume > 127) volume = 127;
         #if TESLA_COIL
         note = teslacoil_checknote(chan, note); // let teslacoil modify the note
         if (chan < num_chans && _tune_volume[chan] != volume) { // if volume is changing
            _tune_volume[chan] = volume;
            teslacoil_change_volume(chan, volume); }
         #else
         max_high_ticks[chan] = min(high_ticks[volume], ticks_per_period[note]);
         current_high_ticks[chan] = 0; // we're starting with output low
         #if DBUG
         Serial.print("  max high ticks="); Serial.println(max_high_ticks[chan]);
         #endif
         #endif
         decrement[chan] = decrement_table[note];
         accumulator[chan] = ACCUM_RESTART;
         playing[chan] = true; }
      #if DO_PERCUSSION
      else if (note > 127) { // Notes above 127 are percussion sounds.
         // Following Connor Nishijima's suggestion, we only play one at a time.
         if (drum_chan == NO_DRUM) { // drum player is free
            drum_chan = chan; // assign it
            drum_tick_count = drum_tick_limit = drum_ticks[note - 128];
            drum_duration = drum_cap[note - 128]; // cap on drum note duration
            #if DBUG
            Serial.print("  drum tick limit="); Serial.println(drum_tick_limit);
            #endif
         } }
      #endif
   } }

#else // non-volume version
void tune_playnote (byte chan, byte note) {
   if (chan < num_chans) {
      if (note > MAX_NOTE) note = MAX_NOTE;
      decrement[chan] = decrement_table[note];
      accumulator[chan] = ACCUM_RESTART;
      playing[chan] = true; } }
#endif

//------------------------------------------------------------------------------
// Stop playing a note on a particular channel
//------------------------------------------------------------------------------

void tune_stopnote (byte chan) {
   if (chan < num_chans) {
      #if DO_PERCUSSION
      if (chan == drum_chan) { // if this is currently the drum channel
         drum_chan = NO_DRUM; // stop playing drum
         #if DBUG
         Serial.print("  stopping drum chan "); Serial.println(chan);
         #endif
      }
      #endif
      if (playing[chan]) {
         playing[chan] = false;
         chan_set_low(chan);
         #if DBUG
         Serial.print("  stopping chan "); Serial.println(chan);
         #endif
      } } }

//------------------------------------------------------------------------------
// Start playing a score
//------------------------------------------------------------------------------

void tune_playscore (const byte * score) {
   if (!pins_initialized)
      tune_init_pins();
   if (tune_playing) tune_stopscore();
   if (!timer_running)
      tune_start_timer(0);
   score_start = score;
   volume_present = ASSUME_VOLUME;
   num_chans_used = MAX_CHANS;

   // look for the optional file header
   memcpy_P(&file_header, score, sizeof(file_hdr_t)); // copy possible header from PROGMEM to RAM
   if (file_header.id1 == 'P' && file_header.id2 == 't') { // validate it
      volume_present = file_header.f1 & HDR_F1_VOLUME_PRESENT;
      num_chans_used = max(1, min(MAX_CHANS, file_header.num_tgens));
      #if DBUG
      Serial.print("header: volume_present="); Serial.print(volume_present);
      Serial.print(", #chans="); Serial.println(num_chans_used);
      #endif
      score_start += file_header.hdr_length; // skip the whole header
   }
   score_cursor = score_start;
   #if DBUG
   Serial.println("playing score");
   #endif
   tune_stepscore();  /* execute initial commands */
   tune_playing = true; }

void tune_speed (unsigned speed) {
   if (speed > 500) speed = 500; // pin between 1/5 and 5x speed
   if (speed < 20) speed = 20;
   _tune_speed = speed; }

void tune_stepscore (void) { // continue in the score
   byte cmd, opcode, chan, note, vol;
   /* Do score commands until a "wait" is found, or the score is stopped.
      This is called initially from tune_playscore, but then is called
      from the slow interrupt routine when waits expire. */
   while (1) {
      cmd = pgm_read_byte(score_cursor++);
      if (cmd < 0x80) { /* wait count in msec. */
         scorewait_interrupt_count = ((unsigned)cmd << 8) | (pgm_read_byte(score_cursor++));
         if (_tune_speed != 100) {
            scorewait_interrupt_count =
               (unsigned) (((unsigned long)scorewait_interrupt_count * 100UL) / _tune_speed);
            if (scorewait_interrupt_count == 0) scorewait_interrupt_count = 1; }
         //Serial.println((int)(score_cursor-score_start+6),HEX);
         #if DBUG
         Serial.print("waitcount="); Serial.println(scorewait_interrupt_count);
         #endif
         break; }
      opcode = cmd & 0xf0;
      chan = cmd & 0x0f;
      if (opcode == CMD_STOPNOTE) { /* stop note */
         tune_stopnote (chan); }
      else if (opcode == CMD_PLAYNOTE) { /* play note */
         note = pgm_read_byte(score_cursor++); // argument evaluation order is undefined in C!
         #if DBUG
         Serial.print("chan "); Serial.print(chan); Serial.print(" note "); Serial.println(note);
         #endif
         #if DO_VOLUME
         vol = volume_present ? pgm_read_byte(score_cursor++) : 127;
         tune_playnote (chan, note, vol);
         #else
         if (volume_present) ++score_cursor; // skip volume byte
         tune_playnote (chan, note);
         #endif
      }
      else if (opcode == CMD_INSTRUMENT) { /* change a channel's instrument */
         #if TESLA_COIL
         byte instrument = pgm_read_byte(score_cursor);
         #if DBUG
         Serial.print("chan "); Serial.print(chan); Serial.print(" instrument "); Serial.println(instrument);
         #endif
         if (chan < num_chans) teslacoil_change_instrument(chan, instrument);
         #endif
         ++score_cursor; } // just ignore it and its data
      else if (opcode == CMD_RESTART) { /* restart score */
         score_cursor = score_start; }
      else if (opcode == CMD_STOP) { /* stop score */
         tune_playing = false;
         break; } } }

//------------------------------------------------------------------------------
// Stop playing a score
//------------------------------------------------------------------------------

void tune_stopscore (void) {
   int i;
   for (i = 0; i < num_chans; ++i)
      tune_stopnote(i);
   tune_playing = false; }

//------------------------------------------------------------------------------
// Stop all channels
//------------------------------------------------------------------------------

void tune_stop_timer(void) {
   tune_stopscore();
   playtune_timer.end(); 
   timer_running = false; }

//------------------------------------------------------------------------------
//  Timer interrupt Service Routine
//
// We look at each playing note on the (up to) 16 channels,
// and determine whether it is time to create the next edge of the square wave
//------------------------------------------------------------------------------

#if DO_PERCUSSION
static byte seed = 23;
byte random_bit(void) {
   // This is an 8-bit version of the 2003 George Marsaglia XOR pseudo-random number generator.
   // It has a full period of 255 before repeating.
   // See http://www.arklyffe.com/main/2010/08/29/xorshift-pseudorandom-number-generator/
   seed ^= (byte)(seed << 7); // The casts are silly, but are needed to keep the compiler
   seed ^= (byte)(seed >> 5); // from generating "mul 128" for "<< 7"! Shifts are faster.
   seed ^= (byte)(seed << 3);
   return seed & 1; }
#endif

void timer_ISR(void) {  //**** THE TIMER INTERRUPT COMES HERE ****

   #if SCOPE_TEST // turn on the scope probe output
   #ifdef CORE_TEENSY
   digitalWriteFast(SCOPE_PIN, HIGH);
   #else
   JOIN(PIN, SCOPE_REG) = (1 << SCOPE_BIT);
   #endif
   #endif

   // first, check for millisecond timing events
   if (--millisecond_interrupt_count == 0) {
      millisecond_interrupt_count = interrupts_per_millisecond;
      // decrement score wait counter
      if (tune_playing && scorewait_interrupt_count && --scorewait_interrupt_count == 0) {
         // end of a score wait, so execute more score commands
         tune_stepscore ();  // execute commands
      } }

   // Now, check for playing notes that need to be toggled.
   // We've "unrolled" this code with a macro to avoid loop overhead,
   // non-constant subscripts, and non-constant arguments to the I/O calls.

   #ifdef CORE_TEENSY  // fast I/O for Teensy ARM microcontrollers
   // make sure all arguments are compile-time constants
   #if DO_VOLUME
   #if TESLA_COIL
#define dospeaker(chan) if (chan < MAX_CHANS && playing[chan]) { \
      accumulator[chan] -= decrement[chan];   \
      if (accumulator[chan]<0) { \
         teslacoil_rising_edge(chan); \
         accumulator[chan] += ACCUM_RESTART; \
      } }
   #else // not tesla coil version
#define dospeaker(chan) if (chan < MAX_CHANS && playing[chan]) {  \
      if (current_high_ticks[chan] && --current_high_ticks[chan] == 0) \
         digitalWriteFast(CHAN_##chan##_PIN,0); /* end of high pulse */ \
      accumulator[chan] -= decrement[chan];   \
      if (accumulator[chan]<0) { \
         if (digitalReadFast(CHAN_##chan##_PIN)) \
            digitalWriteFast(CHAN_##chan##_PIN,0); \
         else digitalWriteFast(CHAN_##chan##_PIN,1);/*TODO: only ever need to do this??*/ \
         accumulator[chan] += ACCUM_RESTART;    \
         current_high_ticks[chan] = max_high_ticks[chan]; \
      } \
   }
   #endif
   #else // non-volume version
#define dospeaker(chan) if (chan < MAX_CHANS && playing[chan]) {  \
      accumulator[chan] -= decrement[chan];   \
      if (accumulator[chan]<0) { \
         if (digitalReadFast(CHAN_##chan##_PIN)) \
            digitalWriteFast(CHAN_##chan##_PIN,0); \
         else digitalWriteFast(CHAN_##chan##_PIN,1); \
         accumulator[chan] += ACCUM_RESTART;    \
      } \
   }
   #endif

   #else // fast I/O for Arduino AVR microcontrollers
   // just writing a one to the PINx register flips the bit!
   #if DO_VOLUME
#define dospeaker(chan) if (chan < MAX_CHANS && playing[chan]) {  \
      if (current_high_ticks[chan] && --current_high_ticks[chan] == 0) \
         CHAN_##chan##_PINREG = (1<<CHAN_##chan##_BIT); /* end of high pulse */ \
      accumulator[chan] -= decrement[chan];   \
      if (accumulator[chan]<0) {  \
         CHAN_##chan##_PINREG = (1<<CHAN_##chan##_BIT); \
         accumulator[chan] += ACCUM_RESTART;    \
         current_high_ticks[chan] = max_high_ticks[chan]; \
      } \
   }
   #else // non-volume version
#define dospeaker(chan) if (chan < MAX_CHANS && playing[chan]) {  \
      accumulator[chan] -= decrement[chan];   \
      if (accumulator[chan]<0) {  \
         CHAN_##chan##_PINREG = (1<<CHAN_##chan##_BIT); \
         accumulator[chan] += ACCUM_RESTART;    \
      } \
   }
   #endif
   #endif

   #if 0 // slow I/O non-volume version, for testing
#define dospeaker(chan) if (chan < MAX_CHANS && playing[chan]) {  \
      accumulator[chan] -= decrement[chan];   \
      if (accumulator[chan]<0) { \
         if (digitalRead(CHAN_##chan##_PIN)) \
            digitalWrite(CHAN_##chan##_PIN,0); \
         else digitalWrite(CHAN_##chan##_PIN,1); \
         accumulator[chan] += ACCUM_RESTART;    \
      } \
   }
   #endif

   // Generate code for up to 16 simultaneous notes.
   // Note that the "if chan < MAX_CHANS" compile-time test in the macro will cause no code to be generated
   // for cases above MAX_CHANS. Testing unused channels below MAX_CHANS takes a few instructions,
   // so it will run a little faster if MAX_CHANS is not set too much higher than the actual number of
   // channels that will be used for any song.

   if (tune_playing) {
      dospeaker(0);
      dospeaker(1);
      dospeaker(2);
      dospeaker(3);
      dospeaker(4);
      dospeaker(5);
      dospeaker(6);
      dospeaker(7);
      dospeaker(8);
      dospeaker(9);
      dospeaker(10);
      dospeaker(11);
      dospeaker(12);
      dospeaker(13);
      dospeaker(14);
      dospeaker(15);

      #if DO_PERCUSSION // Finally, check for transition on the percussion channel
      if (drum_chan != NO_DRUM) { // if playing drum
         if (--drum_tick_count == 0) { // if time to generate
            drum_tick_count = drum_tick_limit;
            if (random_bit())
               chan_set_high(drum_chan);
            else chan_set_low(drum_chan); }
         if (--drum_duration == 0) // early stop for drums
            tune_stopnote(drum_chan); }
      #endif
   }

   #if SCOPE_TEST // turn off the scope probe output
   #ifdef CORE_TEENSY
   digitalWriteFast(SCOPE_PIN, LOW);
   #else
   JOIN(PIN, SCOPE_REG) = (1 << SCOPE_BIT);
   #endif
   #endif

}
