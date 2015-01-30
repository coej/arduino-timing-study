const String softwareVersion = "0311A";

const boolean debugMax = false;

boolean debugLess = false;      //not const so that it will flip to true if debugMax is set true

#define DATAMAX 590     /* maximum midi notes recorded (performer + stimulus loopback). Set as high as free memory allows.
                           program errors start to happen if the free memory at startup is less than around 250 bytes or so. */
#define TARGETMAX 175    //stores one 'long' representing the timing of each interval boundary as it is sent out from Arduino.
                         //(looped-back midi stimuli comes back to us about 2.4ms later.)

/*
stuff to do for clarity?

 --All of the variable-timing tasks could be consolidated into one timer function, since they're basically the same

*/

//our drum module setup:
//left pad: snare (38)
//right pad: low tom (48)


#include <SimpleTimer.h>
SimpleTimer timer;
int timer500ms;
int timer800ms;
int timer10msMulti;
int timer10msPerturb;
int timer10msPerturbTicksOnly;

#include <WString.h>

#define BAUDRATE 115200
//for exporting data to serial output. Is this actually saving us code length or memory?
const String comma = ",";
const String colon = ":";
const String LBracket = "[";
const String nullStr = "X"; 


String serialInputReceived;
char serialInputChar;



#include "MIDI.h"        //customized: Serial1 used (since Serial[0] is the USB i/o); COMPILE_MIDI_THRU turned off. 
                         //See: http://arduinomidilib.sourceforge.net/class_m_i_d_i___class.html
#define INPUTCHANNEL 0   //listen for all channels (1 is participant output, 5 is loopback stimuli
#define STIMULUSCHANNEL 5
#define PERFORMERCHANNEL 1
#define TICK          42                //AKA "CLOSED_HI_HAT"
#define TICK_VOLUME   127               // "velocity," maximum
#define MULTI_DRUM_INSTRUMENT_1     56  //COWBELL
#define MULTI_DRUM_INSTRUMENT_2     63  //OPEN_HI_CONGA
#define MULTI_DRUM_INSTRUMENT_3     80  //mute triangle
#define MULTI_DRUM_VOLUME_1     103
#define MULTI_DRUM_VOLUME_2     98
#define MULTI_DRUM_VOLUME_3     127


#include <MemoryFree.h>  // http://playground.arduino.cc/Code/AvailableMemory

const byte downButtonPin = 2;
const byte upButtonPin = 3;
const byte startButtonPin = 4;
//const byte flushButtonPin = 5;  //no longer needed

#include <LiquidCrystal.h>
LiquidCrystal lcd(7, 8, 9, 10, 11, 12); // putting up some digits in LCD takes about 4 milliseconds
                          // http://www.arduino.cc/en/Tutorial/LiquidCrystal
                          // http://learn.adafruit.com/character-lcds/using-a-character-lcd

#define LINEAR_800_STARTING_ISI     820   //  make interval #85 800ms; 970-(85*2)=800
#define LINEAR_800_PCHANGE_EVERY    5     // decrease by 10 ms every five intervals (avg. -2ms per interval)
#define LINEAR_800_PCHANGE_AMOUNT   -10

#define LINEAR_500_PCHANGE_EVERY    5     // increase by 10 ms every five intervals (avg. -2ms per interval)
#define LINEAR_500_PCHANGE_AMOUNT   10
#define LINEAR_500_STARTING_ISI     480   //  make interval #85 500ms; 670-(85*2)=500

class dataPoint
{
  public: byte channel; byte pitch; byte velocity; unsigned long microseconds;  //max 70 minutes
};

class dataList
{
  public: dataPoint data[DATAMAX]; unsigned int length;
};
dataList dList;   //use this to hold all non-flushed-out data. (But it will be flushed after each task.)
                  //note: classes appear to be defined in real-time rather than ahead of code execution
                  //      in this environment, so this object has to be declared below the class definition.

class targetList
{
  public: 
    unsigned long targetMicroseconds[TARGETMAX];
    unsigned int length;  //number of intervals, including silent; used to iterate through target list
};
targetList tList;


boolean taskRunning; //switch to start timers on running tasks


byte taskRunCount; //not in initial sets. Keep track of presentation order of tasks.
byte taskID;  //select task types with buttons, and later tag the flushed data  



/*
enum Tasks 
{
  Practice_Tick_8 = 0,
  Practice_Tick_5 = 1,
  T1_Iso_Tick_8 = 2,
  ISIP_8 = 3,
  T1_Iso_Tick_5 = 4,
  ISIP_5 = 5,
  x = 6,
  Iso_Jitter_8 = 7,
  Iso_Melody = 8,
  Iso_TickR2_8 = 9,
  Iso_TickR2_5 = 10,
  Pattern_Practice = 11,
  Pattern_Record = 12,
  Improv_Metronome = 13,
  Improv_Melody = 14,
  Phase_Jitter_8 = 15,
  Iso_Jitter_5 = 16,
  Phase_Jitter_5 = 17,
  Linear_Jitter_8 = 18,
  Linear_Jitter_5 = 19,
  Linear_Tick_8 = 20,
  Linear_Tick_5 = 21,
  Phase_Tick_8 = 22,
  Phase_Tick_5 = 23,
  Practice_jittr_8 = 24,
  Practice_jittr_5 = 25,
};
*/

#define TASKCOUNT     26

#define ID_TICKS_PRAC_800    0
#define ID_TICKS_PRAC_500    1
#define ID_T1_SMS_800    2
#define ID_ISIP_800    3
#define ID_T1_SMS_500    4
#define ID_ISIP_500    5
#define ID_REMOVED       6
#define ID_JITS_ISO_800      7
#define ID_MELODYACC          8
#define ID_TICKS_ISO_T2_800    9   
#define ID_TICKS_ISO_T2_500    10  
#define ID_PATT_PRAC        11
#define ID_PATT_REC        12
#define ID_IMPROV_METRO      13
#define ID_IMPROV_MELODY      14
#define ID_JITS_PHASESH_800        15
#define ID_JITS_ISO_500            16
#define ID_JITS_PHASESH_500    17
#define ID_JITS_LINEAR_800          18
#define ID_JITS_LINEAR_500          19
#define ID_TICKS_LINEAR_800          20
#define ID_TICKS_LINEAR_500          21
#define ID_TICKS_PHASESH_800     22
#define ID_TICKS_PHASESH_500     23
#define ID_JITS_PRAC_800    24
#define ID_JITS_PRAC_500    25


prog_char string_0[] PROGMEM =  "Ticks_Pract_8";
prog_char string_1[] PROGMEM =  "Ticks_Pract_5";
prog_char string_2[] PROGMEM =  "T1_SMS_8";
prog_char string_3[] PROGMEM =  "ISIP_8";
prog_char string_4[] PROGMEM =  "T1_SMS_5";
prog_char string_5[] PROGMEM =  "ISIP_5";
prog_char string_6[] PROGMEM =  "x";
prog_char string_7[] PROGMEM =  "Jits_ISO_8";
prog_char string_8[] PROGMEM =  "Iso_Melody";
prog_char string_9[] PROGMEM =  "Ticks_ISO_T2_8";
prog_char string_10[] PROGMEM =  "Ticks_ISO_T2_5";
prog_char string_11[] PROGMEM =  "Pattern_Practice";
prog_char string_12[] PROGMEM =  "Pattern_Record";
prog_char string_13[] PROGMEM =  "Improv_Metronome";
prog_char string_14[] PROGMEM =  "Improv_Melody";
prog_char string_15[] PROGMEM =  "Jits_Phase_8";
prog_char string_16[] PROGMEM =  "Jits_ISO_5";
prog_char string_17[] PROGMEM =  "Jits_Phase_5";
prog_char string_18[] PROGMEM =  "Jits_Linear_8";
prog_char string_19[] PROGMEM =  "Jits_Linear_5";
prog_char string_20[] PROGMEM =  "Ticks_Linear_8";
prog_char string_21[] PROGMEM =  "Ticks_Linear_5";
prog_char string_22[] PROGMEM =  "Ticks_Phase_8";
prog_char string_23[] PROGMEM =  "Ticks_Phase_5";
prog_char string_24[] PROGMEM =  "Jits_Pract_8";  //  #define ID_JITS_PRAC_800  24
prog_char string_25[] PROGMEM =  "Jits_Pract_5"; //#define ID_JITS_PRAC_500  25

PROGMEM const char *progTaskNameTable[] = 	   // change "string_table" name to suit
{   
  string_0,
  string_1,
  string_2,
  string_3,
  string_4,
  string_5,
  string_6,
  string_7,
  string_8,
  string_9,
  string_10,
  string_11,
  string_12,
  string_13,
  string_14,
  string_15,
  string_16,
  string_17,
  string_18,
  string_19,
  string_20,
  string_21,
  string_22,
  string_23,
  string_24,
  string_25,
};

char progTaskNameBuffer[17];

String taskName(byte ID)
{
  strcpy_P(progTaskNameBuffer, (char*)pgm_read_word(&(progTaskNameTable[(int)ID])));
  return (String)progTaskNameBuffer;
}


const byte taskSetStimulusCount[ ] = {  //change if anything increases above 255!
  30, // "Practice 800",     //0
  40, // "Practice 500",     //1
  120, // "Paced 800",     //2
  30, // "Unpaced 800",       //3      //DIFF
  130, // "Paced 500",     //4
  40, // "Unpaced 500",       //5      //DIFF
  120, // "[sdev removed]",  //6
  120, // "Pc MultiDev 800",   //7   
  120, // "Paced: MelodyAcc",  //8
  120, // "T2: Paced 800 t2",  //9
  130, // "T2: Paced 500 t2",  //10
  80, // "Improv/patt prac",  //11     //remove?
  80, // "Improv/patt recd",  //12      //remove?
  140, // "Improv/metronome",  //13
  162, // "Improv/melodyAcc",  //14
  170, // "Ph.Jit/8"            //15
  120, // "Pc MultiDev 500",    //16
  170, // "Ph.Jit/5"            //17
  170, // LinMDv800, 18
  170, // LinMDv500, 19
  170, // LinISOv800, 20
  170, // LinISOv500, 21  
  170, //"Ph.Tic/8",    //22
  170, //"Ph.Tic/5",    //23
  15,  //  #define ID_JITS_PRAC_800  24
  15,  //#define ID_JITS_PRAC_500  25
  };

const byte taskSetRecordingIntervalCount[ ] = {  //change if anything increases above 255!
  30,            // "Practice 800", //0
  40,            // "Practice 500", //1
  120,           // "Paced 800",     //2    
  150, //  *DIFF*   "Unpaced 800",   //3    // 30 paced + 120 unpaced
  130,           // "Paced 500",     //4    
  160, //  *DIFF*   "Unpaced 500",   //5    // 40 paced + 120 unpaced
  120, //                                   "[sdev removed]",  //6
  120, // "Pc MultiDev 800",   //7
  120, // "Paced: MelodyAcc",  //8
  120, // "T2: Paced 800 t2",  //9
  130, // "T2: Paced 500 t2",  //10
  80,  // "Improv/patt prac",  //11
  80,  // "Improv/patt recd",  //12
  140, // "Improv/metronome",  //13
  162, // "Improv/melodyAcc"  //14
  170, // "Ph.Jit/8"          //15
  120, // "Pc MultiDev 500",    //16
  170, // "Ph.Jit/5"            //17
  170, // LinMDv800, 18
  170, // LinMDv500, 19
  170, // LinISOv800, 20
  170, // LinISOv500, 21    
  170, //"Ph.Tic/8",    //22
  170, //"Ph.Tic/5",    //23
  15,  //  #define ID_JITS_PRAC_800  24
  15,  //#define ID_JITS_PRAC_500  25
  };

unsigned long multiPrevMidpointMillis;
unsigned int multiCurrentMidpointISI;
boolean multiFired1;
boolean multiFired2;
boolean multiFired3;


#include "StimulusError.h" //storage of deviating timings

byte mdPerturbIntervalsCount; //tracking how many have been "used" during the task, starts at zero
const byte mdPerturbIntervals_periodShift[ ] = {
  30, 31,   //shift, return  
  48, 49,
  64, 65,
  81, 82,
  97, 98,  
  114, 115,  
  131, 132,  
  150, 151,  
  //ends at 169
};

const int mdPerturbAmounts_phaseShift_800[ ] = {  //starts at 800
  780,  //shift -20    
  800,  //back
  820,  //shift +20
  800,  //back
  840,  //shift +40
  800,  //back
  760,  //shift -40
  800,
  720,  //shift -80
  800,
  880,  //shift +80
  800,  
  960,  //shift +160
  800,
  640,  //shift -160
  800,
};

const int mdPerturbAmounts_phaseShift_500[ ] = {
  490,  //shift -10    
  500,  //back
  510,  //shift +10
  500,  //back
  520,  //shift +20
  500,  //back
  480,  //shift -20
  500,
  450,  //shift -50
  500,
  550,  //shift +50
  500,  
  600,  //shift +100
  500,
  400,  //shift -100
  500,
};


unsigned long currentIntervalStartMillis;
unsigned int currentIntervalISI;

void new10msPerturbTicksOnly() {
  // "Midpoint" means the boundaries between tList.length advancements, which are 500 ms apart.
  // "countupfrommidpoint" reaches 250ms (e.g., for 500ms ITI) at the time when a 0ms-deviated stimulus would fire. so that's the "target" but shifted by 1/2 of ITI.
  //    (this is done so that we can do serial writes out between actual target times. 
  //      We are least likely to screw up a measurement by creating a serial-write delay halfway between intended tapping times.)

  
  if (taskRunning && (taskID == ID_TICKS_LINEAR_800 || taskID == ID_TICKS_LINEAR_500 || taskID == ID_TICKS_PHASESH_800 || taskID == ID_TICKS_PHASESH_500)) {
    unsigned long nowMicros = micros();    
    unsigned long nowMillis = millis();
    
    unsigned long multiDevErrorCountupFromIntStart = nowMillis - currentIntervalStartMillis;  //should max at interval duration
    
    //tList is used here for boundaries only
    
    if (tList.length >= taskSetRecordingIntervalCount[taskID]) {
      if (debugMax) { Serial.print(F("[stopping task at tList.length = ")); Serial.println(tList.length); }
      stopTask();    
    }  
    else {  //tList.length < taskSetRecordingIntervalCount[taskID]
      if (debugMax) { Serial.println(F("checkpoint 2")); }      
      if (multiDevErrorCountupFromIntStart >= currentIntervalISI) {    
        
        //finish the interval...         
        currentIntervalStartMillis = nowMillis;
        nextStimulusOut(STIMULUSCHANNEL, TICK, TICK_VOLUME);
        tList.targetMicroseconds[tList.length] = nowMicros;        
        tList.length++; 
        
        //we're ready to start the next interval once we know how long it should be...
        // (don't change it unless we have a match on the perturb list...)
        
        switch (taskID) {
          case ID_TICKS_LINEAR_800:                         
            if (tList.length % LINEAR_800_PCHANGE_EVERY == 0) {  //multiple of 5
              currentIntervalISI += LINEAR_800_PCHANGE_AMOUNT;   //speed up ISI by 10ms
            }
            break;
          case ID_TICKS_LINEAR_500:        
            if (tList.length % LINEAR_500_PCHANGE_EVERY == 0) {  //multiple of 5
              currentIntervalISI += LINEAR_500_PCHANGE_AMOUNT;   //speed up ISI by 10ms
            }          
            break;
          case ID_TICKS_PHASESH_800:                 
            if (tList.length == mdPerturbIntervals_periodShift[mdPerturbIntervalsCount]) {
              currentIntervalISI = mdPerturbAmounts_phaseShift_800[mdPerturbIntervalsCount];     
              mdPerturbIntervalsCount++; 
            }    
            break;               
          case ID_TICKS_PHASESH_500:                  
            if (tList.length == mdPerturbIntervals_periodShift[mdPerturbIntervalsCount]) {
              currentIntervalISI = mdPerturbAmounts_phaseShift_500[mdPerturbIntervalsCount];     
              mdPerturbIntervalsCount++; 
            }              
            break;            
        }          
      }
    }    
  }
}



char fromDevList1;  //used by "perturb" and "multi"
char fromDevList2;
char fromDevList3;   



void new10msPerturb() {
  // "Midpoint" means the boundaries between tList.length advancements, which are 500 ms apart.
  // "countupfrommidpoint" reaches 250ms (e.g., for 500ms ITI) at the time when a 0ms-deviated stimulus would fire. so that's the "target" but shifted by 1/2 of ITI.
  //    (this is done so that we can do serial writes out between actual target times. 
  //      We are least likely to screw up a measurement by creating a serial-write delay halfway between intended tapping times.)

  
  if (taskRunning && (taskID == ID_JITS_PHASESH_800 
                   || taskID == ID_JITS_PHASESH_500 
                   || taskID == ID_JITS_LINEAR_800 
                   || taskID == ID_JITS_LINEAR_500)) {
    unsigned long nowMillis = millis();
    unsigned long nowMicros = micros();
    unsigned int multiDevErrorCountupFromMidpoint = nowMillis - multiPrevMidpointMillis;  //should max at 500ms
    int compareWithDev = multiDevErrorCountupFromMidpoint - (multiCurrentMidpointISI/2);                    //compareWithDev should range -250 to 250 for 500ms ISI

    
    //tList is used here for boundaries only
    
    if (tList.length >= taskSetRecordingIntervalCount[taskID]) {
      if (debugMax) { Serial.print(F("[stopping task at tList.length = ")); Serial.println(tList.length); }
      stopTask();    
    }  
    else {
      if (multiDevErrorCountupFromMidpoint >= multiCurrentMidpointISI) {
        multiFired1 = multiFired2 = multiFired3 = false;
        multiPrevMidpointMillis = nowMillis;
        tList.targetMicroseconds[tList.length] = nowMicros;  
        printDataFlush();
        tList.length++; 
        
        //determine the next interval's duration
    
        switch (taskID) {
          case ID_JITS_PHASESH_800:              
            if (tList.length == mdPerturbIntervals_periodShift[mdPerturbIntervalsCount]) {
              multiCurrentMidpointISI = mdPerturbAmounts_phaseShift_800[mdPerturbIntervalsCount];     
              mdPerturbIntervalsCount++; 
            }
            break;
          case ID_JITS_PHASESH_500:
            if (tList.length == mdPerturbIntervals_periodShift[mdPerturbIntervalsCount]) {
              multiCurrentMidpointISI = mdPerturbAmounts_phaseShift_500[mdPerturbIntervalsCount];      
              mdPerturbIntervalsCount++; 
            }
            break;     
          case ID_JITS_LINEAR_800:                         
            if (tList.length % LINEAR_800_PCHANGE_EVERY == 0) {  //multiple of, e.g., 5
              multiCurrentMidpointISI += LINEAR_800_PCHANGE_AMOUNT;   //speed up or slow down ISI as specified
            }
            break;
          case ID_JITS_LINEAR_500:        
            if (tList.length % LINEAR_500_PCHANGE_EVERY == 0) {  //multiple of, e.g., 5
              multiCurrentMidpointISI += LINEAR_500_PCHANGE_AMOUNT;   //speed up or slow down ISI as specified
              if (debugMax) Serial.println(F("adding or subtracting 10ms..."));
            }                 
        }          
      }
      else {  //multiDevErrorCountupFromMidpoint < multiCurrentMidpointISI, so check this interval's deviation values instead of advancing
        switch (taskID) {
          case ID_JITS_PHASESH_800: // 800ms
          case ID_JITS_LINEAR_800: // 800ms          
            fromDevList1 = devErrors800Part1[tList.length];
            fromDevList2 = devErrors800Part2[tList.length];
            fromDevList3 = devErrors800Part3[tList.length];
            break;
          case ID_JITS_PHASESH_500: // 500ms
          case ID_JITS_LINEAR_500: // 500ms
            fromDevList1 = devErrors500Part1[tList.length];
            fromDevList2 = devErrors500Part2[tList.length];
            fromDevList3 = devErrors500Part3[tList.length];
            break;
        }            
        if (!multiFired1 && compareWithDev >= fromDevList1) {
          nextStimulusOut(STIMULUSCHANNEL, MULTI_DRUM_INSTRUMENT_1, MULTI_DRUM_VOLUME_1);
          multiFired1 = true;
          if (debugMax) { 
            Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); 
            int devCast = fromDevList1;
            Serial.print(F("fired instrument 1 at a deviation of ")); Serial.println(devCast); 
          }
        }
        if (!multiFired2 && compareWithDev >= fromDevList2) {
          nextStimulusOut(STIMULUSCHANNEL, MULTI_DRUM_INSTRUMENT_2, MULTI_DRUM_VOLUME_2);
          multiFired2 = true;
          if (debugLess) { 
            int devCast = fromDevList2;
            Serial.print(F("fired instrument 2 at a deviation of ")); Serial.println(devCast); 
          }
        }        
        if (!multiFired3 && compareWithDev >= fromDevList3) {
          //nextStimulusOut(STIMULUSCHANNEL, LOW_CONGA, LOW_CONGA_VOLUME);
          nextStimulusOut(STIMULUSCHANNEL, MULTI_DRUM_INSTRUMENT_3, MULTI_DRUM_VOLUME_3);
          multiFired3 = true;
          if (debugMax) { 
            int devCast = fromDevList3;
            Serial.print(F("fired instrument 3 at a deviation of ")); Serial.println(devCast); 
          }        
        }        
      }
    }    
  }
}

     
void new10msMulti() {
  // "Midpoint" means the boundaries between tList.length advancements, which are 500 ms apart.
  // "countupfrommidpoint" reaches 250ms (e.g., for 500ms ITI) at the time when a 0ms-deviated stimulus would fire. so that's the "target" but shifted by 1/2 of ITI.
  //    (this is done so that we can do serial writes out between actual target times. 
  //      We are least likely to screw up a measurement by creating a serial-write delay halfway between intended tapping times.)

  if (taskRunning && (taskID == ID_JITS_PRAC_500 
                   || taskID == ID_JITS_PRAC_800 
                   || taskID == ID_JITS_ISO_500 
                   || taskID == ID_JITS_ISO_800)) {
    unsigned long nowMicros = micros();    
    unsigned long nowMillis = millis();
    unsigned int multiDevErrorCountupFromMidpoint = nowMillis - multiPrevMidpointMillis;  //should max at 500ms
    int compareWithDev = multiDevErrorCountupFromMidpoint - (multiCurrentMidpointISI/2);                    //compareWithDev should range -250 to 250 for 500ms ISI
    
    if (tList.length >= taskSetRecordingIntervalCount[taskID]) {
      if (debugMax) { Serial.print(F("[stopping task at tList.length = ")); Serial.println(tList.length); }
      stopTask();    
    }  
    else {
      if (multiDevErrorCountupFromMidpoint >= multiCurrentMidpointISI) {
        multiFired1 = multiFired2 = multiFired3 = false;
        multiPrevMidpointMillis = nowMillis;        
        printDataFlush();
        tList.targetMicroseconds[tList.length] = nowMicros;          
        tList.length++; 
      }
      else {  //so check this interval's deviation values instead of advancing
        switch (taskID) {
          case ID_JITS_ISO_800: 
          case ID_JITS_PRAC_800: 
            fromDevList1 = devErrors800Part1[tList.length];
            fromDevList2 = devErrors800Part2[tList.length];
            fromDevList3 = devErrors800Part3[tList.length];
            break;            
          case ID_JITS_ISO_500: 
          case ID_JITS_PRAC_500: 
            fromDevList1 = devErrors500Part1[tList.length];
            fromDevList2 = devErrors500Part2[tList.length];
            fromDevList3 = devErrors500Part3[tList.length];
            break;
        }
            
        if (!multiFired1 && compareWithDev >= fromDevList1) {
          nextStimulusOut(STIMULUSCHANNEL, MULTI_DRUM_INSTRUMENT_1, MULTI_DRUM_VOLUME_1);
          multiFired1 = true;
          if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        }
        if (!multiFired2 && compareWithDev >= fromDevList2) {
          nextStimulusOut(STIMULUSCHANNEL, MULTI_DRUM_INSTRUMENT_2, MULTI_DRUM_VOLUME_2);
          multiFired2 = true;
        }        
        if (!multiFired3 && compareWithDev >= fromDevList3) {
          nextStimulusOut(STIMULUSCHANNEL, MULTI_DRUM_INSTRUMENT_3, MULTI_DRUM_VOLUME_3);
          multiFired3 = true;
        }       
      }
    }  

  }
}

void new800ms() { 
  if (taskRunning) {
    if(tList.length < taskSetRecordingIntervalCount[taskID]) {
      unsigned long nowMicros = micros();    
      //unsigned long now = millis();
      switch (taskID) {
        case ID_TICKS_PRAC_800:
        case ID_T1_SMS_800: //"Paced 800"
        case ID_TICKS_ISO_T2_800: //"T2: Paced 800"
        case ID_ISIP_800: //"Unpaced 800"
          nextStimulusOut(STIMULUSCHANNEL, TICK, TICK_VOLUME);
          break;
      }
      tList.targetMicroseconds[tList.length] = nowMicros;        
      tList.length++; 
    }
    else {
      if (debugMax) { Serial.print(F("[stopping task at tList.length = ")); Serial.println(tList.length); }
      stopTask();      
    }    
  }
}



void new500ms() { 
  //if (debugMax) { Serial.print(F("[new500ms() triggered at ")); Serial.println(millis()); }
  if (taskRunning) {
    if(tList.length >= taskSetRecordingIntervalCount[taskID]) {
      //if (debugMax) { Serial.print(F("[stopping task at tList.length = ")); Serial.println(tList.length); }
      stopTask();      
    }
    else {
      unsigned long nowMicros = micros();    
      //unsigned long now = millis();

      switch (taskID) {
        case ID_TICKS_PRAC_500: //"Practice 500"
        case ID_T1_SMS_500: //"Paced 500"
        case ID_TICKS_ISO_T2_500: //"T2: Paced 500"
        case ID_MELODYACC: //"Paced: MelodyAcc"
        case ID_ISIP_500: //"Unpaced 500"
        case ID_PATT_PRAC: //"Improv/patt prac"
        case ID_PATT_REC: //"Improv/patt recd"
        case ID_IMPROV_METRO: //"Improv/metronome"
          nextStimulusOut(STIMULUSCHANNEL, TICK, TICK_VOLUME);
          break;
        case ID_IMPROV_MELODY: //"Improv/melodyAcc"
          nextMelodyAccentStep(5, 2);  //channels for ticks / chords
          break;  

      }
      tList.targetMicroseconds[tList.length] = nowMicros;        
      tList.length++; 
    }
  }
}

  
void printDataHeaders()
{
  Serial.print(F("taskRunCount,")); Serial.print(F("taskID,"));   Serial.print(F("taskName,"));
  Serial.print(F("i,"));            Serial.print(F("channel,"));  Serial.print(F("pitch,")); 
  Serial.print(F("velocity,"));     Serial.println(F("microseconds,"));
}

void printTargetsFlush()
{
  for (int i=0; i<tList.length; i++) {
    unsigned long t = tList.targetMicroseconds[i];
    Serial.println(taskRunCount + comma + taskID + comma + taskName(taskID) + comma
                + i + comma
                + "IntervalOut" + comma
                + nullStr + comma 
                + nullStr + comma
                + t + comma
                );
  }
  tList.length = 0;
}
  
void printDataFlush()
{
  for (int i=0; i<dList.length; i++) {
    dataPoint d = dList.data[i];    
    Serial.println(taskRunCount + comma + taskID + comma + taskName(taskID) + comma
                  + i + comma
                  + d.channel + comma
                  + d.pitch + comma 
                  + d.velocity + comma
                  + d.microseconds + comma
                  );
    if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
  }
  dList.length = 0;
}

void printDataEndMark()
{
  //marker for end of a task's flush
  String endMark = "end";
  Serial.println(taskRunCount + comma + taskID + comma + taskName(taskID) + comma
                + endMark + comma
                + endMark + comma
                + endMark + comma 
                + endMark + comma 
                + endMark
                );
}


void HandleNoteOn(byte channel, byte pitch, byte velocity) { 
  //MIDI library sets up callback to this function. Must have the correct params and return void.  
  
  if (taskRunning) 
  {
    unsigned long nowMicros = micros();      

    if (dList.length >= DATAMAX)
    {
      printDataFlush();
      Serial.print(F("[OF-t_")); Serial.print(nowMicros); Serial.print(F("-")); Serial.println(micros());
    }
    
    unsigned int c = dList.length;
    
    dataPoint d;  
    d.channel = channel;
    d.pitch = pitch;
    d.velocity = velocity;  
    d.microseconds = nowMicros;
    
    //the new data point (zero-indexed) is the same as the previous length (counting from 1)  
    dList.data[c].channel = d.channel;
    dList.data[c].pitch = d.pitch;
    dList.data[c].velocity = d.velocity;  
    dList.data[c].microseconds = d.microseconds;
    dList.length++;
  }
  else
  {
    if (debugMax) { Serial.println(F("[midi input registered outside of task]")); }
  }
}



void nextStimulusOut(byte channel, byte pitch, byte velocity)
{
  if (debugMax) { Serial.print(F("[nextStimulusOut called during tList.length: ")); Serial.print(tList.length); Serial.print(F(" and time ")); Serial.println(millis()); }
  if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
  if (tList.length < taskSetStimulusCount[taskID])
  {
    if (debugMax) { Serial.print(F("[MIDI.send(NoteOn)")); Serial.println(millis()); }
    MIDI.send(NoteOn, pitch, velocity, channel);
    MIDI.send(NoteOff, pitch, velocity, channel);
  }
}

byte melodyStep;

int melodyNotes1[ ] = {
  48, //C3
  43, //G2
  45, //A2
  41  //F2
  };

int melodyNotes2[ ] = {
  72, //C3 3rd
  67, //G2 3rd
  69, //Am 3rd
  65  //F2 3rd
  };

#define MELODY_INTERVALS_BETWEEN_CHORD_CHANGES 8

  
void nextMelodyAccentStep(byte channelTicks, byte channelChords)
{
  byte velocityForTicks = 110; //non-accented
  byte velocityForNotes = 80;
  
  if (debugMax) { Serial.print(F("[starting interval # ")); Serial.println(tList.length); }
  if (tList.length < taskSetRecordingIntervalCount[taskID])
  {
    //if (debugMax) { Serial.print(F("MIDI.send(NoteOn) every interval")); Serial.println(millis()); }
    if ((tList.length) % 4 == 0)
    {
          velocityForTicks = TICK_VOLUME; //accented
    }
    
    if ((tList.length) % MELODY_INTERVALS_BETWEEN_CHORD_CHANGES == 0)
    {
      //if (debugMax) { Serial.print(F("MIDI.send(NoteOn) every 4 intervals")); Serial.println(millis()); }
      MIDI.send(NoteOff, melodyNotes1[melodyStep], velocityForNotes, 2);
      MIDI.send(NoteOff, melodyNotes2[melodyStep], velocityForNotes, 2);

      if (melodyStep == 3) {
        melodyStep = 0;
      }
      else {
        melodyStep++;
      }
      
      MIDI.send(NoteOn, melodyNotes1[melodyStep], 80, 2);
      MIDI.send(NoteOn, melodyNotes2[melodyStep], 80, 2);
      
        
    }
    
    //every interval
    MIDI.send(NoteOn, TICK, velocityForTicks, channelTicks);  //will be accented every 4 due to velocity variable
    tList.targetMicroseconds[tList.length] = micros();
    MIDI.send(NoteOff, TICK, velocityForTicks, channelTicks);
    
    //every four intervals

  }
  else {
    if (debugMax) { Serial.print(F("[stopping task at tList.length = ")); Serial.println(tList.length); }
    stopTask();      
  }
}


void startTask() 
{
  if (!taskRunning) { 
    printDataHeaders();
    delay(50);
    printDataFlush();
    
    /*
    lcd.clear();         lcd.print(taskName(taskID));
    lcd.setCursor(0,1);  lcd.print(F("Starting in 2s...")); lcd.print(taskID);  
    */
    UIshowLines(taskName(taskID), F("Starting in 2s.."));
    
    delay(2000);
    
    UIshowLines(taskName(taskID), F("Run for "));    
    UIappend((String)taskSetStimulusCount[taskID]); 
    UIappend(F(":")); 
    UIappend((String)taskSetRecordingIntervalCount[taskID]);
    
    //lcd.clear();
    //lcd.print(taskName(taskID));
    //lcd.setCursor(0,1);
    //lcd.print(F("Run for ")); lcd.print(taskSetStimulusCount[taskID]); lcd.print(colon); lcd.print(taskSetRecordingIntervalCount[taskID]);
      
    delay(250);

    //reset all counters and timestamps from all tasks
    mdPerturbIntervalsCount = 0;        
    multiFired1 = multiFired2 = multiFired3 = false;
    tList.length = 0;
    taskRunCount++;
    taskRunning = true;    

    multiPrevMidpointMillis = millis();     //midpoint-based (jitter) tasks. Basically the same as interval-based (linear/perturb ticks) tasks.
    currentIntervalStartMillis = millis();            

    //populateTaskTraits();
    
    switch (taskID) {
      case ID_TICKS_PRAC_800: //"Practice 800"
      case ID_T1_SMS_800: //"Paced 800"
      case ID_TICKS_ISO_T2_800: //"D.A.Paced 800 t2"
      case ID_ISIP_800: //"Unpaced 800"
        timer.restartTimer(timer800ms);
        timer.enable(timer800ms);
        break;        
      case ID_TICKS_PRAC_500: //"Practice 500"
      case ID_T1_SMS_500: //"Paced 500"
      case ID_ISIP_500: //"Unpaced 500"
      case ID_PATT_PRAC: //"Improv/patt prac"
      case ID_PATT_REC: //"Improv/patt recd"
      case ID_IMPROV_METRO: //"Improv/metronome"
      case ID_IMPROV_MELODY: //"Improv/melodyAcc"
      case ID_MELODYACC: //"C.PacedMelodyAcc"        
      case ID_TICKS_ISO_T2_500: //"D.B.Paced 500 t2"
        timer.restartTimer(timer500ms);
        timer.enable(timer500ms);      
        break;        
        
      case ID_JITS_PRAC_500:
      case ID_JITS_ISO_500: //"B.PacedMultiDev 500ms"
        multiCurrentMidpointISI = 500;   
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msMulti);
        timer.enable(timer10msMulti);
        break;        
      case ID_JITS_PRAC_800:
      case ID_JITS_ISO_800: //"B.PacedMultiDev 800ms
        multiCurrentMidpointISI = 800;   
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msMulti);
        timer.enable(timer10msMulti);
        break;
        
      case ID_TICKS_LINEAR_800:              
        currentIntervalISI = LINEAR_800_STARTING_ISI;
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msPerturbTicksOnly);
        timer.enable(timer10msPerturbTicksOnly);
        break;  
      case ID_TICKS_LINEAR_500:              //#21
        currentIntervalISI = LINEAR_500_STARTING_ISI;
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msPerturbTicksOnly);
        timer.enable(timer10msPerturbTicksOnly);
        break;  
      case ID_JITS_LINEAR_800:
        multiCurrentMidpointISI = LINEAR_800_STARTING_ISI;
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msPerturb);
        timer.enable(timer10msPerturb);
        break;  
      case ID_JITS_LINEAR_500:              //#19
        multiCurrentMidpointISI = LINEAR_500_STARTING_ISI;
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msPerturb);
        timer.enable(timer10msPerturb);
        break;          
   
      case ID_TICKS_PHASESH_500:              //#23
        currentIntervalISI = 500;
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msPerturbTicksOnly);
        timer.enable(timer10msPerturbTicksOnly);
        break;
      case ID_TICKS_PHASESH_800:              //#22
        currentIntervalISI = 800;
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msPerturbTicksOnly);
        timer.enable(timer10msPerturbTicksOnly);
        break;   
      case ID_JITS_PHASESH_500: //"Perturb"
        multiCurrentMidpointISI = 500;
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msPerturb);
        timer.enable(timer10msPerturb);
        break;        
      case ID_JITS_PHASESH_800: //"Perturb"
        multiCurrentMidpointISI = 800;
        if (debugMax) { Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println("]"); }
        timer.restartTimer(timer10msPerturb);
        timer.enable(timer10msPerturb);
        break;        
    }     
  }
}

void stopTask()
{
  timer.disable(timer500ms);
  timer.disable(timer800ms);
  //timer.disable(timer10ms);
  timer.disable(timer10msMulti);
  timer.disable(timer10msPerturb);  
  timer.disable(timer10msPerturbTicksOnly);
  printDataFlush();
  printTargetsFlush(); 
  printDataEndMark();
  
  taskRunning = false;  
  for (int i=0; i<TARGETMAX; i++)
  {
    //keep old target timestamps from carrying through to the next task.
    //(this is necessary because tList.length will contain no-stimulus intervals
    // in unpaced tasks, but tList.length is also being used to determine how many
    // tList.target[] timestamps to print out at the end of the task-- so if we don't
    // null them out manually, they all print out as duplicates of the previous task's
    // timestamps. This way, they print out as zeros, and can't get confused with real data.
    tList.targetMicroseconds[i] = 0;
  }
  tList.length = 0;  
  
  melodyStep = 3; //using this so that the next step will NoteOff 3 and NoteOn 0 (wrapping around to 0).
  
  UIshowLines(taskName(taskID), F("task stopped."));
  
  
  //check whether some dingus sent us serial commands while a task was in progress
  while(Serial.available()) {
    serialInputChar = Serial.read();
    delay(10);
    serialInputReceived.concat(serialInputChar);
  }  
  if (serialInputReceived == "") {
    //do nothing
  }
  else {
    Serial.print(F("[Serial commands received during previous task: ")); Serial.println(serialInputReceived);
    serialInputReceived = "";
  }
  
}


void setup() {
  
  if (debugMax) debugLess=true;
  
  // ---- HARDWARE BUSINESS ----  
  pinMode(downButtonPin, INPUT);
  pinMode(upButtonPin, INPUT);
  pinMode(startButtonPin, INPUT);  
  //pinMode(flushButtonPin, INPUT); 

  MIDI.begin(INPUTCHANNEL);    // (input channel is default set to 1, midi.h is modified to use Serial1
  MIDI.setHandleNoteOn(HandleNoteOn);  
  delay(300);
  
  Serial.begin(BAUDRATE);
  delay(100);
  
  lcd.begin(16, 2);  //16 cols, 2 rows
  //lcd.print(millis());

  // ---- SET UP TASK SET (?) ----   
  
  //taskList[ID_TICKS_PRAC_800].Name = "ID_TICKS_PRAC_800";
  //taskList[ID_TICKS_PRAC_800].StimCount = 30;
  //taskList[ID_TICKS_PRAC_800].RecIntervalCount = 30;

  
  // ---- INITIAL VALUES OF COUNTERS/etc ----        
  dList.length = 0;  
  taskID = 0;  
  taskRunCount = 0;
  melodyStep = 3; //using this so that the next step will NoteOff 3 and NoteOn 0 (wrapping around to 0).
  
  Serial.println(F("[Restarted]"));
  Serial.print(F("[Software version ")); Serial.print(softwareVersion); Serial.println(F("]"));
  Serial.print(F("[DATAMAX="));       Serial.print(DATAMAX);    Serial.println(F("]"));
  Serial.print(F("[freeMemory()=")); Serial.print(freeMemory());  Serial.println(F("]"));
  if (debugLess) { Serial.print(F("[DEBUG MODE ON!]")); }
        
  lcd.clear();
  lcd.print(taskName(taskID));
  lcd.setCursor(0,1);
  lcd.print(F("Startup. ")); lcd.print(taskID);
  //lcd.clear();
  //lcd.print(taskID);
  //lcd.setCursor(0,1);
  //lcd.print(taskName(taskID));

  delay(500);
    
  timer800ms = timer.setInterval(800, new800ms);
  timer.disable(timer800ms);
  
  timer500ms = timer.setInterval(500, new500ms);
  timer.disable(timer500ms);
  
  timer10msMulti = timer.setInterval(10, new10msMulti);
  timer.disable(timer10msMulti);

  timer10msPerturb = timer.setInterval(10, new10msPerturb);
  timer.disable(timer10msPerturb);  

  timer10msPerturbTicksOnly = timer.setInterval(10, new10msPerturbTicksOnly);
  timer.disable(timer10msPerturbTicksOnly);    
  
  //serial.print out this software version's task names and numbers for reference later
  for (int i=0; i<TASKCOUNT; i++) {
    Serial.print(F("[")); Serial.print(i); Serial.print(F(" = ")); Serial.println(taskName(i));
  }

}

const char autoSelectTask00[4] = "T00";
const char autoSelectTask01[4] = "T01";
const char autoSelectTask02[4] = "T02";
const char autoSelectTask03[4] = "T03";
const char autoSelectTask04[4] = "T04";
const char autoSelectTask05[4] = "T05";
const char autoSelectTask06[4] = "T06";
const char autoSelectTask07[4] = "T07";
const char autoSelectTask08[4] = "T08";
const char autoSelectTask09[4] = "T09";
const char autoSelectTask10[4] = "T10";
const char autoSelectTask11[4] = "T11";
const char autoSelectTask12[4] = "T12";
const char autoSelectTask13[4] = "T13";
const char autoSelectTask14[4] = "T14";
const char autoSelectTask15[4] = "T15";
const char autoSelectTask16[4] = "T16";
const char autoSelectTask17[4] = "T17";
const char autoSelectTask18[4] = "T18";
const char autoSelectTask19[4] = "T19";
const char autoSelectTask20[4] = "T20";
const char autoSelectTask21[4] = "T21";
const char autoSelectTask22[4] = "T22";
const char autoSelectTask23[4] = "T23";
const char autoSelectTask24[4] = "T24";
const char autoSelectTask25[4] = "T25";
const char autoStartTask[3] = "ST";
//const char autoLogText[3] = "LG";


void loop() {
  timer.run();  
  MIDI.read();
  
  if (!taskRunning) {
      
    /* --- SERIAL INPUT (for external control --- */
  
    //if we want to restore "select task but don't run it yet" functionality later, we could just add a "setting"
    //variable that we can toggle it with a serial command. Then we could check that setting in the steps below
    //and run the task or not depending on that setting.
      
    while(Serial.available()) {
      serialInputChar = Serial.read();
      delay(10);
      serialInputReceived.concat(serialInputChar);
    }  
    
    boolean runImmediately = true;
    
    //Serial.println(taskRunCount + comma + taskID + comma + taskName(taskID) + comma ...
    
    if (serialInputReceived == "") {
      //do nothing
    }
    else if (serialInputReceived == autoStartTask) {
      serialInputReceived = "";
      Serial.println(F("[ST command received. Starting task.]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate();
    }    
    else if (serialInputReceived == autoSelectTask00) {
      serialInputReceived = "";           taskID = 0;  
      Serial.print(F("[T00 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask01) {
      serialInputReceived = "";           taskID = 1;  
      Serial.print(F("[T01 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask02) {
      serialInputReceived = "";           taskID = 2;  
      Serial.print(F("[T02 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask03) {
      serialInputReceived = "";           taskID = 3;  
      Serial.print(F("[T03 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask04) {
      serialInputReceived = "";           taskID = 4;  
      Serial.print(F("[T04 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask05) {
      serialInputReceived = "";           taskID = 5;  
      Serial.print(F("[T05 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask06) {
      serialInputReceived = "";           taskID = 6;  
      Serial.print(F("[T06 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask07) {
      serialInputReceived = "";           taskID = 7;  
      Serial.print(F("[T07 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask08) {
      serialInputReceived = "";           taskID = 8;  
      Serial.print(F("[T08 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask09) {
      serialInputReceived = "";           taskID = 9;  
      Serial.print(F("[T09 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask10) {
      serialInputReceived = "";           taskID = 10;  
      Serial.print(F("[T10 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask11) {
      serialInputReceived = "";           taskID = 11;  
      Serial.print(F("[T11 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask12) {
      serialInputReceived = "";           taskID = 12;  
      Serial.print(F("[T12 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask13) {
      serialInputReceived = "";           taskID = 13;  
      Serial.print(F("[T13 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask14) {
      serialInputReceived = "";           taskID = 14;  
      Serial.print(F("[T14 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask15) {
      serialInputReceived = "";           taskID = 15;  
      Serial.print(F("[T15 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask16) {
      serialInputReceived = "";           taskID = 16;  
      Serial.print(F("[T16 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask17) {
      serialInputReceived = "";           taskID = 17;  
      Serial.print(F("[T17 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask18) {
      serialInputReceived = "";           taskID = 18;  
      Serial.print(F("[T18 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask19) {
      serialInputReceived = "";           taskID = 19;  
      Serial.print(F("[T19 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }    
    else if (serialInputReceived == autoSelectTask20) {
      serialInputReceived = "";           taskID = 20;  
      Serial.print(F("[T20 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask21) {
      serialInputReceived = "";           taskID = 21;  
      Serial.print(F("[T21 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask22) {
      serialInputReceived = "";           taskID = 22;  
      Serial.print(F("[T22 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask23) {
      serialInputReceived = "";           taskID = 23;  
      Serial.print(F("[T23 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask24) {
      serialInputReceived = "";           taskID = 24;  
      Serial.print(F("[T24 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
    else if (serialInputReceived == autoSelectTask25) {
      serialInputReceived = "";           taskID = 25;  
      Serial.print(F("[T25 command received. Starting task ")); Serial.print(taskID); Serial.println(F("]"));
      if (runImmediately) startTask(); else taskSelectLcdUpdate(); 
    }
      else if (serialInputReceived == autoStartTask) {
      serialInputReceived = "";
      Serial.println(F("[ST command received. Starting task.]"));
      startTask();
    }    
    else {
      Serial.print(F("[UNKNOWN SERIAL INPUT RECEIVED! received text: ")); Serial.println(serialInputReceived);
      serialInputReceived = "";
    }
    
    
    
    /* --- BUTTON FUNCTIONS --- */  
    int readingDownButton = digitalRead(downButtonPin);
    int readingUpButton = digitalRead(upButtonPin);
    int readingStartButton = digitalRead(startButtonPin);     
    //int readingFlushButton = digitalRead(flushButtonPin);       
    
    if (readingDownButton == HIGH) {
      taskID = taskID==0 ? TASKCOUNT-1 : --taskID;  
      taskSelectLcdUpdate();
      delay(250);
    }
    
    if (readingUpButton == HIGH) {
      taskID = taskID==TASKCOUNT-1 ? 0 : ++taskID;
      taskSelectLcdUpdate();
      delay(250);
    } 

    if (readingStartButton == HIGH) {
      startTask();
    }
  }
}



void taskSelectLcdUpdate()
{
  UIshowLines(taskName(taskID), F("selecting: "));
  UIappend((String)taskID);
}

void UIshowLines(String line1, String line2) 
{  
    lcd.clear();         lcd.print(line1);
    lcd.setCursor(0,1);  lcd.print(line2);
}
void UIshowLines(String line1, const __FlashStringHelper* line2) {  
    lcd.clear();         lcd.print(line1);
    lcd.setCursor(0,1);  lcd.print(line2);
}
void UIshowLines(const __FlashStringHelper* line1, String line2)
{  
    lcd.clear();         lcd.print(line1);
    lcd.setCursor(0,1);  lcd.print(line2);
}
void UIshowLines(const __FlashStringHelper* line1, const __FlashStringHelper* line2)
{  
    lcd.clear();         lcd.print(line1);
    lcd.setCursor(0,1);  lcd.print(line2);
}

void UIappend(String text)
{  
    lcd.print(text);
}
void UIappend(const __FlashStringHelper* text)
{  
    lcd.print(text);
}
