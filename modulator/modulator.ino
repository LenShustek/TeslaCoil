/*******************************************************************************

   Tesla coil modulator
   By Len Shustek

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

   --------------------------------------------------------------------------------
   8/11/2011,  L. Shustek, first non-musical version for testing
   11/19/2011, L. Shustek, add monophonic music playing
   12/5/2011,  L. Shustek, updated for MEGA2560 board to play polyphonic music
   6/10/2015,  L. Shustek, updated for Arduino IDE v.1.6.4
   3/20/2021,  L. Shustek, various changes:
    -switch to Teensy 3.2
    -play on multiple Tesla coils, using the instrument information to direct
     the notes, and the note volume ("velocity") to control the pulse width
    -enforce rough limits on the Tesla coil duty cycle to avoid
     burning out te IGBTs or other components
    -control playback speed of a song with pot 4
    -change from 10-position rotary switch to a rotary encoder
    -add the 128x64 OLED display
   5/9/2021, L. Shustek,
    -implement per-coil limits on pulse width and duty cycle, because
     some coils (like OneTelsa) blow up sooner than others (like mine).
    -increase max channels from 8 to 16
   ------------------------------------------------------------------------------------
   The MIT License (MIT)
   Copyright (c) 2011, 2015, 2021 Len Shustek

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
**************************************************************************************/
#define VERSION "5.1"
#define DEBUG 0
#define TEST_VOLUME 0

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "Playtune_poll.h"  // include special version of Playtune_poll
#include "instruments.h"

// musical scores

struct coil_instr_t {  // an assignment of a coil to an instrument
   const byte coilnum; // 0xff: "end of coil assignments"
   const byte instrument; };
struct scoredescr_t {  // a score description
   const char *name;       // ptr to the name
   const int transpose;    // how many half-notes to transpose by, up or down
   const int speed;        // initial speed tweak
   const byte *scoreptr;   // ptr to the score
   const int scoresize;    // how long it is
   const struct coil_instr_t *assignments; }; // ptr to array of instrument assignments, or NULLP

#if 0 // boring!
#include "music\bach_allemande.h"
struct scoredescr_t const scoredescr_bach_bwv1013_allemande = {
   "Bach BWV1013 Allemande", 0,
   score_bach_bwv1013_allemande, sizeof (score_bach_bwv1013_allemande),
   NULL /* all coils play all instruments */ };
#endif

#include "music\bach_courante.h"
struct scoredescr_t const scoredescr_bach_bwv1013_courante = {
   "Bach BWV 1013 Courante", 0, 0,
   score_bach_bwv1013_courante, sizeof(score_bach_bwv1013_courante),
   NULL /* all coils play all instruments */ };

#include "music\bach_invent13_2instr.h"
struct coil_instr_t const instruments_bach_invent13_2instr [] = {
   {0, I_Acoustic_Grand }, // coil 0: left hand
   {0xff, 0xff } }; // coil 1: left hand (I_Bright_Acoustic)
struct scoredescr_t const scoredescr_bach_invent13_2instr = {
   "Bach 2-part Invention #13", 0, 0,
   bach_invent13_2instr, sizeof(bach_invent13_2instr),
   instruments_bach_invent13_2instr };

#include "music\bach_little_fugue_01_2coils.h"
struct coil_instr_t const instruments_bach_little_fugue [] = {
   {0, I_Viola }, // coil 0 gets viola
   {0xff, 0xff } }; // coil 1 gets everything else, ie violin
struct scoredescr_t const scoredescr_bach_little_fugue = {
   "Bach Little Fugue", 0, 0,
   bach_little_fugue_01_2coils, sizeof(bach_little_fugue_01_2coils),
   instruments_bach_little_fugue };

#include "music\maple_leaf_rag_CLBNduet.h"
struct coil_instr_t const instruments_maple_leaf_rag_CLBNduet [] = {
   {0, I_Bassoon }, // coil 0:
   {0xff, 0xff } }; // coil 1 gets everything else
struct scoredescr_t const scoredescr_mapleaf_rag = {
   "Scott Joplin's Mapleleaf Rag", -4, 0,
   maple_leaf_rag_CLBNduet, sizeof(maple_leaf_rag_CLBNduet),
   instruments_maple_leaf_rag_CLBNduet };

#include "music\Entertainer.h"
struct coil_instr_t const instruments_Entertainer [] = {
   {0, I_Clarinet }, // coil 0:
   {0xff, 0xff } }; // coil 1 gets everything else
struct scoredescr_t const scoredescr_Entertainer = {
   "Scott Joplin's Entertainer", -4, 10,
   Entertainer, sizeof(Entertainer),
   instruments_Entertainer };

#include "music\StLouisBlues.h"
struct coil_instr_t const instruments_StLouisBlues [] = {
   {0, I_Violin }, // coil 0:
   {0xff, 0xff } }; // coil 1 gets everything else
struct scoredescr_t const scoredescr_StLouisBlues = {
   "WC Handy's St Louis Blues", -8, 40,
   StLouisBlues, sizeof(StLouisBlues),
   instruments_StLouisBlues };

#include "music\DuelingCoils.h"
struct coil_instr_t const instruments_DuelingCoils [] = {
   {0, I_Banjo }, // coil 0 gets "banjo"
   {1, I_Acoustic_Guitar_steel }, // coil 1 gets "string guitar"
   {0xff, 0xff } }; // coil 1 also gets everything else
struct scoredescr_t const scoredescr_DuelingCoils = {
   "Arthur Smith's Dueling Banjos", 0, 0,
   DuelingCoils, sizeof(DuelingCoils),
   instruments_DuelingCoils };

#include "music\moneymoney.h"
struct coil_instr_t const instruments_moneymoney [] = {
   {0, I_Slap_Bass_2 }, // coil 0: "bright piano"
   {0xff, 0xff } }; // coil 1 gets everything else
struct scoredescr_t const scoredescr_moneymoney = {
   "ABBA's Money Money Money", -4, 0,
   moneymoney, sizeof (moneymoney),
   instruments_moneymoney };

#include "music\puttritz_6trk.h"
struct coil_instr_t const instruments_puttritz_6trk [] = {
   {0, I_Bassoon },
   {0, I_Tuba },
   {0, I_Trumpet },
   {0, I_Trombone },
   {1, 0xff } }; // coil 1 gets everything else
struct scoredescr_t const scoredescr_puttritz = {
   "Irving Berlin's Puttin' on the Ritz", -4, 0,
   puttritz_6trk, sizeof(puttritz_6trk),
   instruments_puttritz_6trk };

#include "music\bach_brandenburg3.h"
struct coil_instr_t const instruments_bach_brandenburg3 [] = {
   {0, I_Cello }, // coil 0 gets low: cello and contrabase
   {0, I_Contrabass },
   {0xff, 0xff } }; // coil 1 gets everything else
struct scoredescr_t const scoredescr_bach_brandenburg3 = {
   "Bach's Brandenburg Concerto #3, 1st mvmt", 0, 0,
   bach_brandenburg3, sizeof(bach_brandenburg3),
   instruments_bach_brandenburg3 };

struct scoredescr_t const scoredescr_pulse_momentary = { // play a tone while the button is pushed
   "tone\npush to play", 0, 0, NULL, 0, NULL };
struct scoredescr_t const scoredescr_pulse_hold = { // play a tune continuously until the button is pushed again
   "tone\npush on/off", 0, 0, NULL, 0, NULL };

const byte PROGMEM score_monophonic_tones [] = { // play monotonic scales
#define NOTEa(n) 0x90,36+n, 2,0, 0x80, 1,150
#define NOTEb(n) 0x90,36+n, 1,0, 0x80, 0,150
   NOTEa(0), NOTEa(12), 3, 0,
   NOTEa(0), NOTEa(3), NOTEa(7), NOTEa(12), 3, 0,
   NOTEb(0), NOTEb(2), NOTEb(3), NOTEb(5), NOTEb(7), NOTEb(9), NOTEb(11), NOTEb(12),
   0xf0 };
struct scoredescr_t const scoredescr_monophonic_tones = {
   "monophonic scales", 0, 0,
   score_monophonic_tones, sizeof(score_monophonic_tones),
   NULL };

const byte PROGMEM score_polyphonic_tones [] = { // play polyphonic chords
#define SH 12
   0x90, 36 + SH, 6, 0, 0x91, 40 + SH, 6, 0, 0x92, 43 + SH, 6, 0, 0x93, 48 + SH, 6, 0,
   0x80, 0x81, 0x82, 0x83, 6, 0,
   0x90, 48 + SH, 6, 0, 0x91, 43 + SH, 6, 0, 0x92, 40 + SH, 6, 0, 0x93, 36 + SH, 6, 0,
   0x80, 0x81, 0x82, 0x83, 6, 0,
   0xf0 };
struct scoredescr_t const scoredescr_polyphonic_tones = {
   "polyphonic chords", 0, 0,
   score_polyphonic_tones, sizeof(score_polyphonic_tones),
   NULL };

#if TEST_VOLUME
const byte PROGMEM score_test_volume [] = {  // score for testing volume modulation
   'P', 't', 6, 0x80, 0x00,  2, // Playtune header: 2 channels, volume present
#define NOTE0(n,v) 0x90,36+n,v
#define NOTE1(n,v) 0x91,36+n,v
   NOTE0(0, 60), NOTE1(1, 100), 2, 0,
   NOTE0(2, 100), NOTE1(3, 127), 2, 0,
   0x81, NOTE0(4, 30), 2, 0,
   0xf0 };
struct scoredescr_t const scoredescr_test_volume = {
   "volume test", 0, 0,
   score_test_volume, sizeif(score_test_volume),
   NULL };
#endif

const byte PROGMEM score_scope_test [] = { // score for using an oscilloscope to test timing
   0x90, 48, 6, 0, 0x91, 52, 100, 0,
   0x80, 0x81, 0xf0 };
struct scoredescr_t const scoredescr_scope_test = {
   "scope test", 0, 0,
   score_scope_test, sizeof(score_scope_test),
   NULL };

void pulse_momentary(struct scoredescr_t const *);
void pulse_hold(struct scoredescr_t const *);
void play_score(struct scoredescr_t const *);

struct action_t {  // table of pushbutton action routines
   void (*rtn)(struct scoredescr_t const *); // ptr to action routine
   struct scoredescr_t const *descr; }       // score description to pass to it
actions[] = { // this is the order they appear as the knob is turned
   #if TEST_VOLUME
   {play_score, &scoredescr_test_volume },
   #endif
   {pulse_momentary, &scoredescr_pulse_momentary },
   {pulse_hold, &scoredescr_pulse_hold },
   {play_score, &scoredescr_monophonic_tones },
   {play_score, &scoredescr_polyphonic_tones },
   // {play_score, &scoredescr_bach_bwv1013_allemande },
   {play_score, &scoredescr_bach_bwv1013_courante },
   {play_score, &scoredescr_bach_invent13_2instr },
   {play_score, &scoredescr_bach_little_fugue },
   {play_score, &scoredescr_mapleaf_rag },
   {play_score, &scoredescr_Entertainer },
   {play_score, &scoredescr_StLouisBlues },
   {play_score, &scoredescr_DuelingCoils },
   {play_score, &scoredescr_moneymoney },
   {play_score, &scoredescr_puttritz },
   {play_score, &scoredescr_bach_brandenburg3 } };
#define NUM_ACTIONS ((int)(sizeof(actions)/sizeof(struct action_t)))

// parameters and hardware configuration

#define NUM_COILS 2              // number of Tesla coils
#define NUM_CHANS 16             // number of music channels == max number of simultaneous notes
#define NUM_INSTRUMENTS 128      // number of MIDI instruments
#define MAX_PULSEWIDTH_USEC 350  // maximum pulse width in usec (for tests, not for music-playing)
#define MIN_PULSEWIDTH_USEC 5    // minimum pulse width in usec (for tests, not for music-playing)
#define MAX_DUTYCYCLE_PCT 5      // maximum duty cycle in percent (for tests, not for music-playing)
#define MIN_VOLUME 50            // minimum MIDI volume we pin to; below this is all the same
#define MAX_VOLUME 127           // maximum MIDI volume we pin to; above this is all the same
#define MAX_FREQUENCY 2000       // maximum pulse frequency for pulse testing
#define MAX_MIDI_NOTE 96         // limit frequency to about 2 Khz, the highest piano note less one octave
#define MIN_PULSE_SEPARATION 10  // usec
#define SUMMATION_MSEC 25        // how long for overlapping duty cycle period calculation
#define DISPLAY_UPDATE_MSEC 250  // how often to update the music-playing display
#define DEBOUNCE_DELAY 10        // msec for debounce delay
#define POT_CHANGE_THRESHOLD 10  // fuzz magnitude for analog pots (about 1%)

#define GreenLED  11             /* green light */
#define RedLED    12             /* red light */
#define TeensyLED 13             // LED on the Teensy board
#define PushButton 6             // "go" pushbutton, active low (either button or rotary encoder)
#define ROTARY_CLK 2             // rotary encoder: high-to-low transition
#define ROTARY_DATA 3            // rotary encoder: high=CW, low=CCW

const byte coil_switch_pins[]
   = {7, 8, 9, 10 };                // pins for Tesla coil toggle switches
bool coil_on[NUM_COILS] = {0 };     // current state of the switches
bool coil_switches_changed = true;  // have any of them changed since last time

const byte pot_ports []
   = {A0, A1, A2, A3 };             // analog input ports for pots
enum pot_name_t {POT0, POT1, POT2, POT3 };

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);  // the OLED display
//#define FONT u8g2_font_cu12_hr  // 11 pixel height (capital A), common height, non-transparent, ASCII printable only
#define FONT u8g2_font_t0_15_tr // 10 pixel height (capital A), transparent, ASCII printable only

#define STRINGIFY(s) #s
#define STRING(s) STRINGIFY(s)

struct { // limits for each Tesla coil
   const unsigned min_pulsewidth, max_pulsewidth; // in usec
   const unsigned max_dutycycle_pct; // in percent
} coil_limit[NUM_COILS] = {
   {10, 250, 10 }, // my 30" coil: pretty rugged
   {5, 150, 5 }   // the 12" OneTesla coil: more fragile
};

struct { // state information about our Tesla coils
   volatile bool doing_pulse;                     // is it currently doing a pulse
   unsigned int current_max_pulsewidth;           // width in usec, based on the volume pot and this coil's limits
   volatile unsigned long time_last_pulse_ended;  // micros() timestamp, which overflow in 70 minutes
   volatile unsigned int pulse_sum1_usec, pulse_sum2_usec; // overlapping period pulse summations for duty cycle calculation
   unsigned int active_usec;                      // how many usec pulses were active since the last display
   int last_barheight;                            // the last height, in pixels, of the activity bar
   bool duty_cycle_exceeded;                      // was the duty cycle exceeded in the last summation period?
   bool show_duty_cycle_exceeded;                 // is it to be noted on the display
   uint32_t instruments[NUM_INSTRUMENTS / 32];    // bit vector of what instruments we play
} tesla_coil[NUM_COILS] = {0 };

// state information about our channels, which are the note-playing timers
// each channel can play on several Tesla coils simultaneously, depending on the current instrument
byte channel_instrument[NUM_CHANS] = {0 }; // what instrument this channel is currently playing
byte channel_coils[NUM_CHANS] = {0 };      // bitmap of which coils this channel is playing on; LSB is coil 0
byte channel_volume[NUM_CHANS] = {100 };   // what volume this channel's note is playing at

/* We use the Cortex k20 processor's FTM 0 flexible timer to generate active-low
   oneshot pulses on any of the coils, starting at any time and for any
   duration up to a few milliseconds. We do that by running the FTM at a constant
   frequency and using up to 4 of its 8 "channels" in "set pin output high on match"
   mode, with each channel's target count computed to be the end of the pulse.
   We got tips on how to do this from this NXP forum posting:
   https://community.nxp.com/t5/S32K/S32K144-FTM-Output-Compare-for-Single-Pulse/m-p/844007#M3436

   Note that the FTM "channels" are unrelated to the "channels" of the Playtune music bytestream,
   which are really "tone generators".

   The pulses are output on Teensy 3.2 "pin numbers" 20 through 23.
   Pin number and port notations are confusing. Here's the deal:
     coil  Teensy "pin"   CPU port   chip pin   FTM0 channel
       0       20           D5          62         5
       1       21           D6          63         6
       2       22           C1          44         0
       3       23           C2          45         1
    Don't use any of these Teensy pins with pinMode() or digitalWrite()!
*/

unsigned int pot_values[] = {               // the current value of the potentiometers
   0, 0, 0, 0 };                            // 10-bit A-to-D converter, 0 to 1023
int rotary_value = 1, pushbutton_value = 1; // the current state of the switch and pushbutton
volatile int rotary_change = -1;            // rotary changes queued by the interrupt routine
volatile int notechange;                    // how much to transcribe the score up or down
unsigned int pulse_separation =             // required microseconds of separation between pulses when playing polyphonic music
   MIN_PULSE_SEPARATION;


// display utility routines

#if DEBUG
#define debug_print(format,...) debug_printer (F(format),__VA_ARGS__)
#define MAXLINE 120
void debug_printer(const __FlashStringHelper * format, ...) {
   char buf[MAXLINE];
   va_list argptr;
   va_start(argptr, format);
   vsnprintf_P(buf, MAXLINE, (PGM_P)(format), argptr);
   Serial.print(buf);
   va_end(argptr); }
#else
#define debug_print(...)
#endif

void assert(bool test, int code) {
   if (!test) {
      debug_print("ASSERTION FAILURE: %d\n", code);
      while (1) {
         digitalWrite(TeensyLED, 1);
         delay(500);
         digitalWrite(TeensyLED, 0);
         delay(500); } } }

// display a string on multiple text lines, keeping words intact where possible,
// and accepting \n to force a new line
void display_words(const char *msg, int xloc, int yloc /*bottom*/ ) {
   int dspwidth = display.getDisplayWidth(); // display width in pixels
   int strwidth = 0;  // string width in pixels
   char glyph[2]; glyph[1] = 0;
   for (const char *ptr = msg, *lastblank = NULL; *ptr; ++ptr) {
      while (xloc == 0 && (*msg == ' ' || *msg == '\n'))
         if (ptr == msg++) ++ptr; // skip blanks and newlines at the left edge
      glyph[0] = *ptr;
      strwidth += display.getStrWidth(glyph); // accumulate the pixel width
      if (*ptr == ' ')  lastblank = ptr; // remember where the last blank was
      else ++strwidth; // non-blanks will be separated by one additional pixel
      if (*ptr == '\n' ||   // if we found a newline character,
            xloc + strwidth > dspwidth) { // or if we ran past the right edge of the display
         int starting_xloc = xloc;
         // print to just before the last blank, or to just before where we got to
         while (msg < (lastblank ? lastblank : ptr)) {
            glyph[0] = *msg++;
            xloc += display.drawStr(xloc, yloc, glyph); }
         strwidth -= xloc - starting_xloc; // account for what we printed
         yloc += display.getMaxCharHeight(); // advance to the next line
         xloc = 0; lastblank = NULL; } }
   while (*msg) { // print any characters left over
      glyph[0] = *msg++;
      xloc += display.drawStr(xloc, yloc, glyph); } }

// clear a text line on the display so that we can use "transparent" fonts
void display_clearline(int yloc) { // yloc is of the bottom of the text line, like the default for drawStr
   display.setDrawColor(0);  // draw zeroes
   display.drawBox(0, yloc - display.getMaxCharHeight(), // origin here is upper left corner, unlike for text!
                   display.getDisplayWidth(), display.getMaxCharHeight());
   display.setDrawColor(1); } // go back to drawing ones

// initialization

void setup() {
   pinMode (GreenLED, OUTPUT);
   pinMode (RedLED, OUTPUT);
   pinMode (TeensyLED, OUTPUT);
   pinMode (PushButton, INPUT_PULLUP);
   pinMode(ROTARY_CLK, INPUT_PULLUP);
   pinMode(ROTARY_DATA, INPUT_PULLUP);
   attachInterrupt(digitalPinToInterrupt(ROTARY_CLK), rotary_interrupt, FALLING);
   for (unsigned i = 0; i < sizeof(coil_switch_pins); ++i)
      pinMode(coil_switch_pins[i], INPUT_PULLUP);
   check_coil_switches();

   #if DEBUG
   Serial.begin(115200);
   // while (!Serial) ; // fails if serial monitor isn't running
   delay(1000);
   Serial.println("Debugger...");
   debug_print("F_CPU=%d\n", F_CPU);
   #endif

   display.begin();     // start the OLED display
   display.clearBuffer();
   display.setFont(FONT);
   display_words("Music-playing Tesla Coil modulator, version " VERSION, 0, display.getMaxCharHeight());
   display.sendBuffer();
   delay(2000);

   // start Playtune for music generation
   tune_start_timer(0);

   // Set up four of the eight channels of FTM0 ("FlexTimer Module 0") for use as
   // a one-shot on any of up to four Tesla coils. (See pin assignments above.)
   //#define SCOPE 19 // for debugging
   //pinMode(SCOPE, OUTPUT); digitalWrite(SCOPE, HIGH); // trigger logic analyer
   FTM0_SC = 0;
   FTM0_CNT = 0;
   FTM0_MOD = 0xffff; // free running FTM counter wraps at 65535
   FTM0_MODE = FTM_MODE_WPDIS | FTM_MODE_FTMEN; // disable write protect, enable FTM
   FTM0_STATUS &= ~0xff; // clear all CHnF bits
   PORTD_PCR5 = PORT_PCR_MUX(4); // assign PTD5 (pin 62) to FTM0_CH5 via Alternate 4
   PORTD_PCR6 = PORT_PCR_MUX(4); // assign PTD6 (pin 63) to FTM0_CH6 via Alternate 4
   PORTC_PCR1 = PORT_PCR_MUX(4); // assign PTC1 (pin 44) to FTM0_CH0 via Alternate 4
   PORTC_PCR2 = PORT_PCR_MUX(4); // assign PTC2 (pin 45) to FTM0_CH1 via Alternate 4
   FTM0_POL = 0xff; // inactive state of all channels is high
   FTM0_OUTMASK = 0xff; // outputs are masked to inactive
   FTM0_OUTINIT = 0xff; //0x00; // initialization state is low for all channels
   FTM0_MODE |= FTM_MODE_INIT; // set output INIT state
   FTM0_C5SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA; // setup for "set output on match"
   FTM0_C6SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA; // setup for "set output on match"
   FTM0_C0SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA; // setup for "set output on match"
   FTM0_C1SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA; // setup for "set output on match"
   FTM0_SC = FTM_SC_CLKS(1) | FTM_SC_PS(5);  // run at system bus clock (CPUspeed/2) divided by 32
#define FTM_DIVISOR 32
#define FTM_FREQ (F_CPU/2/FTM_DIVISOR) // at 72 Mhz: 1.125 Mhz, or 0.888 uS/tick, or 58.25 msec per counter period
   NVIC_ENABLE_IRQ(IRQ_FTM0); // enable interrupt
   //   digitalWrite(SCOPE, LOW); // mark end of setup

   digitalWrite(GreenLED, HIGH);  /* indicate "ready" by blinking the green light */
   delay(250);
   digitalWrite(GreenLED, LOW);
   delay(250);
   digitalWrite(GreenLED, HIGH); }

/* Start an active-low single pulse for "usec" microseconds on a Tesla coil.
   While playing scores, this is called from the Playtune interval timer
   interrupt routine for each rising edge of each note currently being played.

   We use flexible timer FTM0, which free runs somewhat faster than 1 Mhz.
   To start a oneshot on a coil, we turn on the output by setting the
   pin to 0. We then set a target for that FTM0 channel that causes the
   pin to be set to 1 when the channel's counter hits that at the end of the
   oneshot period, and causes an interrupt which disables subsequent interrupts.
   We have at least 58 msec to service the interrupt without getting a useless
   but harmless second interrupt.
*/
void start_oneshot(byte coil, unsigned int usec) { // generate an active-low pulse for a Tesla coil
   // starting a pulse when one is in progress will update the ending time of the current pulse
   assert(coil < NUM_COILS, 1);
   //debug_print("start_oneshot(%d,%d)\n", coil, usec);
#define MAX_ONESHOT_USEC (0xffffffffUL-500000UL)/FTM_FREQ  // for 72 Mhz with divisor 32: 3817 usec
   // That guarantees no overflow with the 32-bit arithmetic below for count_target, but it
   // doesn't guarantee that the Tesla coil won't be destroyed! That's what max_dutycycle_pct at a higher level is for.
   if (usec > MAX_ONESHOT_USEC) usec = MAX_ONESHOT_USEC;
   uint32_t count_target = FTM0_CNT + (usec * FTM_FREQ + 500000UL) / 1000000UL; // set the (rounded) channel match value
   FTM0_OUTINIT = 0xff; //0x00; // set initial pin value to low
   FTM0_MODE |= FTM_MODE_INIT; // set output INIT state
   tesla_coil[coil].doing_pulse = true;
   tesla_coil[coil].pulse_sum1_usec += usec;
   tesla_coil[coil].pulse_sum2_usec += usec;
   if (coil == 0) {
      FTM0_C5V = count_target;
      FTM0_C5SC &= ~FTM_CSC_CHF; // reset any pending channel flag
      FTM0_C5SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA | FTM_CSC_CHIE; // set output on match, and interrupt
      FTM0_OUTMASK &= ~FTM_OUTMASK_CH5OM; } // enable output to cause it to go low
   else if (NUM_COILS >= 2 && coil == 1) {
      FTM0_C6V = count_target;
      FTM0_C6SC &= ~FTM_CSC_CHF; // reset any pending channel flag
      FTM0_C6SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA | FTM_CSC_CHIE; // set output on match, and interrupt
      FTM0_OUTMASK &= ~FTM_OUTMASK_CH6OM; }// enable output to cause it to go low
   else if (NUM_COILS >= 3 && coil == 2) {
      FTM0_C0V = count_target;
      FTM0_C0SC &= ~FTM_CSC_CHF; // reset any pending channel flag
      FTM0_C0SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA | FTM_CSC_CHIE; // set output on match, and interrupt
      FTM0_OUTMASK &= ~FTM_OUTMASK_CH0OM; } // enable output to cause it to go low
   else if (NUM_COILS >= 4 && coil == 3) {
      FTM0_C1V = count_target;
      FTM0_C1SC &= ~FTM_CSC_CHF; // reset any pending channel flag
      FTM0_C1SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA | FTM_CSC_CHIE; // set output on match, and interrupt
      FTM0_OUTMASK &= ~FTM_OUTMASK_CH1OM; } // enable output to cause it to go low
}
void ftm0_isr(void) { // interrupt routine
   //digitalWrite(SCOPE, 1 - digitalRead(SCOPE)); // logic analyzer mark
   unsigned long timenow = micros();
   if (FTM0_C5SC & FTM_CSC_CHF) { // if channel 5 is interrupting,
      tesla_coil[0].time_last_pulse_ended = timenow;
      tesla_coil[0].doing_pulse = false;
      FTM0_C5SC &= ~(FTM_CSC_CHF | FTM_CSC_CHIE); // acknowledge and turn off interrupts
      FTM0_OUTMASK |= FTM_OUTMASK_CH5OM; } // disable output
   if (NUM_COILS >= 2 && FTM0_C6SC & FTM_CSC_CHF) { // if channel 6 is interrupting,
      tesla_coil[1].time_last_pulse_ended = timenow;
      tesla_coil[1].doing_pulse = false;
      FTM0_C6SC &= ~(FTM_CSC_CHF | FTM_CSC_CHIE); // acknowledge and turn off interrupts
      FTM0_OUTMASK |= FTM_OUTMASK_CH6OM; } // disable output
   if (NUM_COILS >= 3 && FTM0_C0SC & FTM_CSC_CHF) { // if channel 0 is interrupting,
      tesla_coil[2].time_last_pulse_ended = timenow;
      tesla_coil[2].doing_pulse = false;
      FTM0_C0SC &= ~(FTM_CSC_CHF | FTM_CSC_CHIE); // acknowledge and turn off interrupts
      FTM0_OUTMASK |= FTM_OUTMASK_CH0OM; } // disable output
   if (NUM_COILS >= 4 && FTM0_C1SC & FTM_CSC_CHF) { // if channel 1 is interrupting,
      tesla_coil[3].time_last_pulse_ended = timenow;
      tesla_coil[3].doing_pulse = false;
      FTM0_C1SC &= ~(FTM_CSC_CHF | FTM_CSC_CHIE); // acknowledge and turn off interrupts
      FTM0_OUTMASK |= FTM_OUTMASK_CH1OM; } // disable output
}

//  Routines for reading the coil switches, parameter pots, and the "go" button

void rotary_interrupt(void) {
   if (digitalRead(ROTARY_DATA)) ++rotary_change;
   else --rotary_change; }

// update rotary_value; return true if changed
bool rotary_switch () { // constrained to the size of the action table
   if (rotary_change) {
      noInterrupts();
      if ((rotary_value = (rotary_value + rotary_change) % NUM_ACTIONS) < 0)
         rotary_value = NUM_ACTIONS + rotary_value;
      rotary_change = 0;
      interrupts();
      //debug_print("rotary changed to %d\n", rotary_value);
      return true; }
   return false; }

void show_rotary_name(void) {
   assert(rotary_value >= 0 && rotary_value < NUM_ACTIONS, 2);
   display.clearBuffer(); // display the associated text
   char number[5];
   sprintf(number, "%d: ", rotary_value + 1);
   int numberwidth = display.drawStr(0, display.getMaxCharHeight(), number);
   display_words(actions[rotary_value].descr->name, numberwidth, display.getMaxCharHeight());
   display.sendBuffer(); }

bool pot_changed (int pot) {  // Check if a pot has changed
   int newval = analogRead(pot_ports[pot]);
   if (abs(newval - pot_values[pot]) < POT_CHANGE_THRESHOLD) return false;
   pot_values[pot] = newval;
   return true; }

int scale_pot(int pot, const char *name, int low, int high, int init, unsigned initval) {
   // return a pot's interpolated variable between "low" and "high",
   // relative to the variable having been "init" when the pot value (0..1023) was "initval"
   int answer;
   pot_changed(pot);  // get current value
   unsigned curval = pot_values[pot];
   if (curval <= initval) // interpolate from low to init
      answer = (curval * (init - low)) / initval + low;
   else // interpolate from init to high
      answer = ((curval - initval) * (high - init)) / (1023 - initval) + init;
   debug_print("pot%d %s val %d\n", pot, name, answer);
   return answer; }

unsigned get_pot_pulsewidth(int pot) { // return pulse width in usec: MIN_PULSEWIDTH to MAX_PULSEWIDTH
   pot_changed(pot);
   return pot_values[pot] * (MAX_PULSEWIDTH_USEC - MIN_PULSEWIDTH_USEC) / 1023 + MIN_PULSEWIDTH_USEC; }

int get_pot_pulse_separation(int pot) { // return min pulse separation in usec: 10 to 2057
   pot_changed(pot);
   return (pot_values[pot] << 1) + MIN_PULSE_SEPARATION; }

int get_pot_frequency(int pot) { // return frequency in hertz: 1 to MAX_FREQUENCY
   pot_changed(pot);
   return (pot_values[pot] * (MAX_FREQUENCY - 1) / 1023) + 1; }

bool pushbutton(void) {  // read debounced pushbutton, return true if pushed
   int current = digitalRead(PushButton);
   if (current != pushbutton_value) {
      delay (DEBOUNCE_DELAY);
      pushbutton_value = digitalRead(PushButton);
      debug_print("button changed to %d\n", pushbutton_value); }
   return pushbutton_value == 0; }

bool check_coil_switch(int coil) { // see if a coil selector switch has changed
   int current = 1 - digitalRead(coil_switch_pins[coil]);
   if (current != coil_on[coil]) {
      delay (DEBOUNCE_DELAY);
      coil_on[coil] = 1 - digitalRead(coil_switch_pins[coil]);
      return true; }
   return false; }

bool check_coil_switches(void) { // see if any coil selector switch has changed
   bool changed = false;
   for (int coil = 0; coil < NUM_COILS; ++coil)
      changed |= check_coil_switch(coil);
   return changed; }

// Do instrument assignments to Tesla coils when a song starts, and any time the coil switches change.
// We allow coils to play multiple instruments, and allow instruments to be played on multiple coils.

void reset_coils(void) {
   memset(tesla_coil, 0, sizeof(tesla_coil)); }

#define set_coil_instrument(coil,instr) tesla_coil[coil].instruments[instr/32] |= 1 << (instr%32)
#define test_coil_instrument(coil,instr) (tesla_coil[coil].instruments[instr/32] & (1 << (instr%32)))

void set_coil_instruments(struct coil_instr_t const *assignments) { // assign coils to instruments for a score
   reset_coils(); // start with nothing assigned and all state information cleared
   check_coil_switches(); // update coil switch status
   if (assignments) {
      while (assignments->coilnum != 0xff && assignments->instrument != 0xff) { // do the assignments
         if (coil_on[assignments->coilnum]) // if this coil's switch is on
            set_coil_instrument(assignments->coilnum, assignments->instrument);
         ++assignments; }
      // Give all instruments not played by any other coil to the coil named with the 0xff instrument,
      // or, if it isn't playing or wasn't assigned, to the last coil
      int catchall_coil;
      if (assignments->coilnum != 0xff && coil_on[assignments->coilnum])
         catchall_coil = assignments->coilnum;
      else { // find the last coil that is playing
         catchall_coil = -1;
         for (int coil = 0; coil < NUM_COILS; ++coil)
            if (coil_on[coil]) catchall_coil = coil; }
      if (catchall_coil >= 0) { // if there is at least one coil playing
         uint32_t instruments_assigned[NUM_INSTRUMENTS / 32];
         for (int map = 0; map < NUM_INSTRUMENTS / 32; ++map)
            instruments_assigned[map] = 0;
         for (int coil = 0; coil < NUM_COILS; ++coil) // OR all the maps
            for (int map = 0; map < NUM_INSTRUMENTS / 32; ++map)
               instruments_assigned[map] |= tesla_coil[coil].instruments[map];
         for (int map = 0; map < NUM_INSTRUMENTS / 32; ++map) // give the last coil any not assigned
            tesla_coil[catchall_coil].instruments[map] |= ~instruments_assigned[map]; } }
   else // no assignments: play all instruments on all coils whose switches are on
      for (int coil = 0; coil < NUM_COILS; ++coil)
         if (coil_on[coil])
            for (int map = 0; map < NUM_INSTRUMENTS / 32; ++map)
               tesla_coil[coil].instruments[map] = 0xffffffff;
   if (DEBUG) { // show the assignments
      for (int coil = 0; coil < NUM_COILS; ++coil) {
         debug_print("coil %d instruments:", coil);
         for (int map = 0; map < NUM_INSTRUMENTS / 32; ++map) {
            debug_print(" %08X", tesla_coil[coil].instruments[map]); }
         Serial.println(); } } }

void set_coil_maxpulsewidths(int pot) {
   for (int coil = 0; coil < NUM_COILS; ++coil) {
      tesla_coil[coil].current_max_pulsewidth =
         pot_values[pot] * (coil_limit[coil].max_pulsewidth - coil_limit[coil].min_pulsewidth) / 1023 + coil_limit[coil].min_pulsewidth;
      debug_print("POT1 value %u coil %d max pulsewidth %u\n", pot_values[pot], coil, tesla_coil[coil].current_max_pulsewidth); } }

#if 0 // deprecated code
void set_channel_pulsewidth(byte channel, byte volume) {
   // Set the pulse width (volume) for this channel using the velocity (volume)
   // of the note being played (0..127) to scale the default pulse width.
   // The following accentuates the range of velocities between 50 and 127
#define MINVOL 50
   int pulsewidth = ((volume - MINVOL) * music_pulsewidth) / (127 - MINVOL);
   if (pulsewidth > fix: MAX_PULSEWIDTH_USEC) pulsewidth = MAX_PULSEWIDTH_USEC;
   if (pulsewidth < fix: MIN_PULSEWIDTH_USEC) pulsewidth = MIN_PULSEWIDTH_USEC;
   //debug_print("chan %d vol %d pulsewidth %d\n", channel, volume, pulsewidth);
   channel_pulsewidth[channel] = pulsewidth; }
void set_all_channel_pulsewidths(void) {
   for (int chan = 0; chan < NUM_CHANS; ++chan)
      set_channel_pulsewidth(chan, channel_volume[chan]); }
#endif

// Update duty cycle caculations for the coils, using two overlapping
// measurement periods, each of length 2 * SUMMATION_MSEC.
// When pulses are generated, both sums are incremented by the pulse width.

void duty_cycle_calcs(void) {
   static unsigned long last_calc_time = 0;
   static bool doing_sum1 = true;
   unsigned long timenow = millis();
   if (timenow - last_calc_time >= SUMMATION_MSEC) { // time to calculate
      for (int coil = 0; coil < NUM_COILS; ++coil)  {
         #if DEBUG
         bool oldval = tesla_coil[coil].duty_cycle_exceeded;
         #endif
         if (doing_sum1) {
            tesla_coil[coil].duty_cycle_exceeded =
               (tesla_coil[coil].pulse_sum1_usec * 100) / (2 * SUMMATION_MSEC * 1000)
               > coil_limit[coil].max_dutycycle_pct;
            if (tesla_coil[coil].duty_cycle_exceeded) tesla_coil[coil].show_duty_cycle_exceeded = true;
            #if DEBUG
            if (tesla_coil[coil].duty_cycle_exceeded)
               debug_print("%lu coil %d duty cycle %d%% > %d%%\n", millis(), coil,
                           (tesla_coil[coil].pulse_sum1_usec * 100) / (2 * SUMMATION_MSEC * 1000),
                           coil_limit[coil].max_dutycycle_pct);
            else if (oldval)
               debug_print("%lu coil %d duty cycle recovered\n", millis(), coil);
            #endif
            tesla_coil[coil].active_usec += tesla_coil[coil].pulse_sum1_usec;
            //debug_print("coil %d sum1=0\n", coil);
            tesla_coil[coil].pulse_sum1_usec = 0; }
         else { // doing sum2
            tesla_coil[coil].duty_cycle_exceeded =
               (tesla_coil[coil].pulse_sum2_usec * 100) / (2 * SUMMATION_MSEC * 1000)
               > coil_limit[coil].max_dutycycle_pct;
            #if DEBUG
            if (tesla_coil[coil].duty_cycle_exceeded)
               debug_print("%lu coil %d duty cycle %d%% > %d%%\n", millis(), coil,
                           (tesla_coil[coil].pulse_sum2_usec * 100) / (2 * SUMMATION_MSEC * 1000),
                           coil_limit[coil].max_dutycycle_pct);
            else if (oldval)
               debug_print("%lu coil %d duty cycle recovered\n", millis(), coil);
            #endif
            tesla_coil[coil].active_usec += tesla_coil[coil].pulse_sum1_usec;
            //debug_print("coil %d sum2=0\n", coil);
            tesla_coil[coil].pulse_sum2_usec = 0; }
         if (tesla_coil[coil].duty_cycle_exceeded) tesla_coil[coil].show_duty_cycle_exceeded = true; }
      doing_sum1 = 1 - doing_sum1;  // switch to other summation interval
      last_calc_time = timenow; } }

static const struct scoredescr_t *current_scoredescr;
void update_playing_status (void) {
   duty_cycle_calcs();
   static unsigned long last_display_time = 0;
   unsigned long timenow = millis();
   if (timenow - last_display_time >= DISPLAY_UPDATE_MSEC) { // time for a display update
      // -the top line is text: start of description
      // -then a horizonal progress bar graph showing how much of the song is done
      // -then a text line reserved for duty cycle overload warnings
      // -the rest is space for vertical activity bar graphs showing recent duty cycle relative to maximum
      //
      // The following are pseudo-constants: we don't know them at compile time, but we do after initialization.
      // (We could make them global variables, but we don't compute them very often.)
      int DSP_PROGBAR_TOP = display.getMaxCharHeight() + 4; // top of the progress bar is a little under the title text line
      int DSP_PROGBAR_HT = 3;  // how many pixels high is the progress bar
      int DSP_WARN_TEXT_BOT = DSP_PROGBAR_TOP + DSP_PROGBAR_HT + display.getMaxCharHeight() + 1; // bottom of the warning text line
      int DSP_ACTBAR_BOT = display.getDisplayHeight();  // base of the activity bars
      int DSP_ACTBAR_HEIGHT = DSP_ACTBAR_BOT - DSP_WARN_TEXT_BOT - 1; // max height of the activity bar
      int DSP_ACTBAR_WIDTH = display.getMaxCharWidth() * 2; // width of the activity bar
      // draw the progress bar
      extern const byte *score_cursor; // where Playtune is up to
      int percent = 100 * (score_cursor - current_scoredescr->scoreptr) / current_scoredescr->scoresize;
      if (percent < 0) percent = 0;
      if (percent > 100) percent = 100;
      display.drawBox(0, DSP_PROGBAR_TOP, // origin for boxes is the upper left corner, unlike for text!
                      display.getDisplayWidth()*percent / 100, DSP_PROGBAR_HT);
      // now draw the activity bars and the warning text line snippets for each coil
      display_clearline(DSP_WARN_TEXT_BOT);
      display.drawHLine(0, DSP_WARN_TEXT_BOT + 1, display.getDisplayWidth()); // target line at top of activity bars
      for (int coil = 0; coil < NUM_COILS; ++coil) {
         int xloc = coil * display.getMaxCharWidth() * 4; // left edge of 4 chars per coil
         display.drawStr(xloc, DSP_WARN_TEXT_BOT, tesla_coil[coil].show_duty_cycle_exceeded ? "OVL" : "    ");
         tesla_coil[coil].show_duty_cycle_exceeded = false;
         // height = maxheight * (active_usec/(UPDATE_MSEC*1000)) / (maxDC%/100)
         int barheight = DSP_ACTBAR_HEIGHT * tesla_coil[coil].active_usec  / (10 * DISPLAY_UPDATE_MSEC * coil_limit[coil].max_dutycycle_pct);
         //debug_print("%d usec %d height %d lastheight %d\n", coil, tesla_coil[coil].active_usec, barheight, tesla_coil[coil].last_barheight);
         if (barheight > DSP_ACTBAR_HEIGHT) barheight = DSP_ACTBAR_HEIGHT;
         int deltaheight = barheight - tesla_coil[coil].last_barheight;
         xloc += display.getMaxCharWidth();  // space out to the middle 2 of the 4 characters
         if (deltaheight > 0) {  // bar is growing: draw the new part
            //debug_print("%d grow at %d by %d\n", coil, DSP_ACTBAR_BOT - barheight, deltaheight);
            display.drawBox(xloc, DSP_ACTBAR_BOT - barheight, DSP_ACTBAR_WIDTH, deltaheight); }
         else if (deltaheight < 0) { // bar is shrinking: erase the old part
            //debug_print("%d shrink at %d by %d\n", coil, DSP_ACTBAR_BOT - tesla_coil[coil].last_barheight, - deltaheight);
            display.setDrawColor(0); // "erase"
            display.drawBox(xloc, DSP_ACTBAR_BOT - tesla_coil[coil].last_barheight, DSP_ACTBAR_WIDTH, - deltaheight);
            display.setDrawColor(1); } // "draw"
         //else debug_print("%d unchanged at %d\n", coil, DSP_ACTBAR_BOT - barheight);
         tesla_coil[coil].active_usec = 0;
         tesla_coil[coil].last_barheight = barheight; }
      display.sendBuffer();
      last_display_time = timenow; } }

// play a complete score

void reset_display(struct scoredescr_t const *descr) {
   display.clearBuffer();
   display.drawStr(0, display.getMaxCharHeight(), descr->name); } // as much as fits on the first line

void play_score (struct scoredescr_t const *descr) {
   digitalWrite(RedLED, HIGH);
   current_scoredescr = descr;
   pot_changed(POT0);
   unsigned notechange_initval = pot_values[POT0]; // initial note transposition point for score
   notechange = descr->transpose;
   set_coil_instruments(descr->assignments); // do the initial instrument assignments
   for (int chan = 0; chan < NUM_CHANS; ++chan) {
      channel_volume[chan] = 127; // set all channels to max volume
      teslacoil_change_instrument(chan, 0); } // and instrument 0
   pot_changed(POT1); // read the value of the volume pot
   set_coil_maxpulsewidths(POT1); // and set coil max pulse widths based on it
   //set_all_channel_pulsewidths();
   pulse_separation = get_pot_pulse_separation(POT2);
   pot_changed(POT3); // initial speed point for the score
   unsigned speed_initval = pot_values[POT3];
   tune_playscore(descr->scoreptr);   // start playing
   tune_speed(100 + descr->speed); // speed is 100% (normal) based on that pot position, tweaked by score description
   reset_display(descr);
   display.clearBuffer();
   display.drawStr(0, display.getMaxCharHeight(), descr->name); // as much as fits on the first line
   while (pushbutton()) update_playing_status(); // wait for button release
   while (!pushbutton() && rotary_change == 0 // wait for next button push, or rotary switch change,
          && tune_playing) { // or for tune to stop
      update_playing_status();
      if (check_coil_switches()) { // check for coil switch changes
         set_coil_instruments(descr->assignments); // if so, redo instruments assignments to coils
         for (int chan = 0; chan < NUM_CHANS; ++chan) // and recompute channel_coils[]
            teslacoil_change_instrument(chan, channel_instrument[chan] );
         set_coil_maxpulsewidths(POT1);
         reset_display(descr); }
      if (pot_changed(POT0)) {  // check for changed note transposition
         notechange = scale_pot(POT0, "notechange", -12, 12, descr->transpose, notechange_initval); } // +- one octave
      pot_changed(POT1); // update volume pot
      if (pot_changed(POT1)) { // check for changed volume
         set_coil_maxpulsewidths(POT1); }
      if (pot_changed(POT2)) { // check for changed min pulse separation
         pulse_separation = get_pot_pulse_separation(POT2); }
      if (pot_changed(POT3)) {  // check for changed speed
         tune_speed(scale_pot(POT3, "speed", 50, 200, 100 + descr->speed, speed_initval)); } } // 1/2x to 2x
   if (tune_playing) tune_stopscore();
   digitalWrite(RedLED, LOW);
   while (pushbutton());  // wait for button release
}

// This is called by the Playtunes music player when there is a rising edge on any channel
// playing a note. We use it to generate a single pulse on all the Tesla coil(s) that are
// playing this channel.  Multiple notes may be playing on the same coil simultaneously,
// and multiple coils may be playing the same note.
// Note that this is called from the timer interrupt routine, so don't do anything heavy-duty.

void teslacoil_rising_edge(byte chan) {
   unsigned long timenow = micros();
   for (int coilnum = 0; coilnum < NUM_COILS; ++coilnum) { // see which coils are playing this channel
      //debug_print("checking coil %d chan %d against %02X\n", coilnum, chan, channel_coils[chan]);
      if (channel_coils[chan] & (1 << coilnum) // if this coil is playing this channel
            && !tesla_coil[coilnum].doing_pulse // and there isn't a pulse in progress
            // and it doesn't violate the minimum pulse separation constraint
            && timenow > (tesla_coil[coilnum].time_last_pulse_ended + pulse_separation)) {
         int pulsewidth;
         if (tesla_coil[coilnum].duty_cycle_exceeded)
            pulsewidth = coil_limit[coilnum].min_pulsewidth; // use minimum pulse width to reduce duty cycle
         else {// Set the pulse width for this channel based on the velocity of the note, the maximum
            // pulse width for this coil, and the current setting of the volume pot.
            // We accentuate the range of velocities between MIN_VOLUME (50) and MAX_VOLUME (127).
            // Other code guarantees that channel_volume[chan] meets that constraint, and similarly that
            // tesla_coil[coilnum].current_max_pulsewidth is between coil_limit[coilnum].min_pulsewidth and .max_pulsewidth,
            // so pulsewidth computed below will (work it out!) also obey the coil_limit constraints.
            pulsewidth = (channel_volume[chan] - MIN_VOLUME) * (tesla_coil[coilnum].current_max_pulsewidth - coil_limit[coilnum].min_pulsewidth)
                         / (MAX_VOLUME - MIN_VOLUME)
                         + coil_limit[coilnum].min_pulsewidth; }
         if (TEST_VOLUME) {
            debug_print("time %lu coil %d chan %d vol %d minwidth %d maxwidth %d width %d\n",
                        millis(), coilnum, chan, channel_volume[chan], coil_limit[coilnum].min_pulsewidth, tesla_coil[coilnum].current_max_pulsewidth, pulsewidth); }
         start_oneshot(coilnum, pulsewidth); } } }

// These are called by the Playtunes music player to let us record and maybe change
// something about the note to be played on a channel starting now. We figure out which
// coil(s) are to play it based on the instrument currently assigned to this channel.

byte teslacoil_checknote (byte chan, byte note) {
   int newnote;
   newnote = note + notechange;
   while (newnote < 0) newnote += 12;
   while (newnote > MAX_MIDI_NOTE) newnote -= 12;
   return newnote; }

void teslacoil_change_instrument(byte chan, byte instrument) {
   channel_instrument[chan] = instrument; // record an instrument change
   channel_coils[chan] = 0; // build a bitmap of which coils play this instrument
   for (int coil = 0; coil < NUM_COILS; ++coil) { // look at each coil
      //debug_print("chan %d coil %d instr %d tests %d\n", chan, coil, instrument, test_coil_instrument(coil, instrument));
      if (test_coil_instrument(coil, instrument)) // if it is defined as playing this instrument
         channel_coils[chan] |= (1 << coil); } }// record it for teslacoil_rising_edge() to use
//channel_coils[chan] could be zero if no coil switches are on

void teslacoil_change_volume(byte chan, byte volume) {
   if (volume < MIN_VOLUME) volume = MIN_VOLUME;
   if (volume > MAX_VOLUME) volume = MAX_VOLUME;
   channel_volume[chan] = volume; // record a volume change
   //set_channel_pulsewidth(chan, volume);
}

//  simple tone action routines

unsigned onepulse_pulsewidth, onepulse_delayusec, onepulse_dutycycle; // displayed values

void init_one_pulse(void) { // force update of display
   onepulse_pulsewidth = onepulse_delayusec = onepulse_dutycycle = 0; }

void do_one_pulse (void) { // do one pulse and pause for an inter-pulse delay
   // The delay (frequency) is set by POT0, and the pulse width (volume) by POT1.
   reset_coils();
   check_coil_switches();
   unsigned pulsewidth = get_pot_pulsewidth(POT1);
   unsigned delayusec = (1000000UL / get_pot_frequency(POT0)) - pulsewidth;
   unsigned dutycycle = (100 * pulsewidth) / (delayusec + pulsewidth);
   if (onepulse_pulsewidth != pulsewidth || onepulse_delayusec != delayusec || onepulse_dutycycle != dutycycle) { // update display
      int coil;
      for (coil = 0; coil < NUM_COILS; ++coil) // see if any coil exceeds its duty cycle
         if (dutycycle > coil_limit[coil].max_dutycycle_pct) break;
      bool dutycycle_ok = coil == NUM_COILS;
      char buf[50];
      sprintf(buf, "%u us, %u Hz", pulsewidth, 1000000 / (pulsewidth + delayusec));
      int yloc = display.getDisplayHeight() - 1 - display.getMaxCharHeight(); // second from bottom line
      display_clearline(yloc); // clear it first, to allow for transparent fonts
      display.drawStr(0, yloc, buf);  // then write the new line
      yloc = display.getDisplayHeight() - 1; // bottom line
      display_clearline(yloc); // clear it first, to allow for transparent fonts
      sprintf(buf, "%u%% %s", dutycycle, dutycycle_ok ? "" : "> " STRING(MAX_DUTYCYCLE_PCT) "% !!");
      display.drawStr(0, display.getDisplayHeight() - 1, buf);
      display.sendBuffer();
      onepulse_pulsewidth = pulsewidth;
      onepulse_delayusec = delayusec;
      onepulse_dutycycle = dutycycle; }
   for (int coil = 0; coil < NUM_COILS; ++coil)
      if (coil_on[coil]) // on all coils whose switches are on
         start_oneshot(coil, pulsewidth );
      else tesla_coil[coil].doing_pulse = false;
   bool busy; do { // wait for all coils to finish
      busy = false;
      for (int coil = 0; coil < NUM_COILS; ++coil)
         busy |= tesla_coil[coil].doing_pulse; }
   while (busy);
   if (delayusec > 10000L) delay (delayusec / 1000);
   else delayMicroseconds (delayusec); }

void pulse_momentary(struct scoredescr_t const * descr) {
   digitalWrite(RedLED, HIGH);
   init_one_pulse();
   while (pushbutton()) {
      do_one_pulse(); }
   digitalWrite(RedLED, LOW); }

void pulse_hold(struct scoredescr_t const * descr) {
   digitalWrite(RedLED, HIGH);
   init_one_pulse();
   bool button_released = false;
   do {
      do_one_pulse();
      if (!pushbutton()) button_released = true; }
   while ((!button_released || !pushbutton()) && rotary_change == 0);
   digitalWrite(RedLED, LOW);
   while (pushbutton()) ; } /* wait until button is released */

// the user input loop

void loop () {
   if (rotary_switch()) { // if rotary switch changed
      show_rotary_name(); }
   if (pushbutton()) {  // if button is pushed,
      pot_changed(0);   // first read all the parameter pots
      pot_changed(1);
      pot_changed(2);
      pot_changed(3);
      // then execute the action routine based on the switch position
      assert(rotary_value >= 0 && rotary_value < NUM_ACTIONS, 3);
      (actions[rotary_value].rtn)(actions[rotary_value].descr);
      show_rotary_name(); } }
//*
