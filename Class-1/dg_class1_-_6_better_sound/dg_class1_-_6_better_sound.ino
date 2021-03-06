/*
  It's a big leap from making sound we way we did before to something that sounds like a "real" synth
  You can only ever do one thing at a time on any computer and you can pretty much only do it at one speed.
  The Teensy 4.0 can do a simple operation hundreds of millions of times a seconds. Things take a lot of little operations to do but still it's fast
  but what if you want a light blinking at something faster than 1 millisecond but slower than 150MHz? https://forum.pjrc.com/threads/57185-Teensy-4-0-Bitbang-FAST

  You can interrupt the loop after a set amount of micro or even nano seconds have passed. We can kind of thinking this like multitasking
  the loop is still looping but ever X nano seconds it stops what it's doing and does the things that have in the interval

*/


IntervalTimer timer1; //Make a new interval timer called timer1

int led1_pin =  10;
int led1_state = LOW;
int led2_pin =  9;
int led2_state = 0;
unsigned long  previous_time1 = 0;
unsigned long  previous_time2 = 0;
unsigned long  previous_time3 = 0;
unsigned long current_time;
unsigned long interval1 = 50;
unsigned long interval2 = 50;
unsigned long interval3 = 50;
int button_pin;
int button_state;
int pot1_value;
int pot1_pin = A0;
int pot2_value;
int pot2_pin = A1;
float volume_pot; //floats can hold decimals
float freq1;
float freq2;
int out1, out2, out3;// You can define variables like this too. All of them will be integers equal to 0
uint32_t dds_tune;
uint32_t dds_rate;

void setup() {
  pinMode(led1_pin, OUTPUT);
  pinMode(led2_pin, OUTPUT);
  pinMode(button_pin, INPUT_PULLUP);

  analogWriteResolution(8); //we can make this up to 12 bits but it won't really help us with better quality audio and blinky LEDs
  analogWriteFrequency(20, 187500); //set the PWM rate well above the audio rage to limit noise

  //set the rate of our interrupt timer
  dds_rate = 20; //20 microseconds = 50KHz
  dds_tune = (1.00 / (dds_rate * .000001)); //used to make the oscillation at the frequency we want them to be
  timer1.begin(osc1, dds_rate);

}

void osc1() { // code that is run whenever the timer goes off, every 20 micros
  //these functions are not hidden away like the onces we've used so far. They are at the bottom of this code
  out1 = oscillator(0, freq1, 1, .5); //oscillator(voice select, frequency, amplitude, shape)
  out2 = oscillator(1, freq2, 1, .3); //  More info on how this works at the bottom of the code
  out3 = fold((out1 + out2) * volume_pot); //folds the input instead of clipping it based on the level of the volume pot.

  analogWrite(20, (out3 + 2048) >> 4); // The oscillators produce numbers between -2048 and 2048 but the DAC can't output negative numbers so we add the offset back in
  // >> is bit shifting and is much faster than dividing. The oscillator is 12 bit resolution but we're outputting 8
}

void loop()
{
  //everything happens like normal in the loop it just takes a little longer to finish, but only like milliseconds longer.
  current_time = millis();

  button_state = digitalRead(button_pin); //if the button is not being pressed it will read HIGH. if it pressed it will read LOW

  pot1_value = analogRead(pot1_pin); //Read the analog voltage at pin A0. Returns 0 for 0 volts and 1023 for the max voltage (3.3V)
  interval1 = pot1_value / 2; //these aren't floats so we basically loose the remainders but it's not a critical setting.
  interval2 = pot1_value / 5;
  freq1 = pot1_value / 2.0; //divide by a float to make sure we get a float out
  freq2 = freq1 * .501; //have the second oscillator be an octave lower than the first but a little detuned

  pot2_value = analogRead(pot2_pin) / 4; //analogRead returns 0-1024 but analogWrite is 0-255 so we divide by 4. 10 bits to 8 bits. We'll talk about these funny numbers later
  volume_pot = (analogRead(pot2_pin) / 1023.00) * 8.0; //Take the reading, make it from 0.0 to 1.0, then multiply to get 0.0 to 8.0

  if (current_time - previous_time1 > interval1) {
    previous_time1 = current_time;

    if (led1_state == LOW) {
      led1_state = HIGH;
    }
    else {
      led1_state = LOW;
    }
    analogWrite(led1_pin, led1_state * pot2_value); //since the state is 1 or 0 it's off half the time, on at our new pot reading the other half
  }


  if (current_time - previous_time2 > interval2) {
    previous_time2 = current_time;

    if (led2_state == 0) {
      led2_state = 1;
    }
    else {
      led2_state = 0;
    }
    analogWrite(led2_pin, led2_state * pot2_value);
  }
}

//you can declare "global" variable wherever but the code above it can't see them.
// This is why you should usually do your declaring at the very top
// I put all this down here as only the oscillator function needs these variables and to avoid confusion at the top.
int  wavelength = 4095;
unsigned long accumulator[8] = {}; //these are arrays here where are 8 separate variables called accumulator. accumulator[0],accumulator[1],etc
unsigned long increment[8] = {};
unsigned long waveindex[8] = {};


//Here's a function I made to generate a waveform that can be adjusted from saw to ramp

//(voice select, frequency, amplitude, shape)
// Each separate oscillator needs a unique voice select number
// Frequency is a floating point to allow for finer control
// Amplitude is a float 0.0 to 1.0, off to full amplitude
// 0.0 shape is a saw, 0.5 is triangle and 1.0 is ramp

int oscillator(byte sel, float freq, float amp, float tri_shape) {
  int tout;  //"local" variables can only be seen in the {} they are in and anything in sub {}
  int waveamp = amp * 4095.00;

  int knee = tri_shape * 4095.00;

  if (knee < 0)
  {
    knee = 0;
  }

  if (knee > wavelength - 1)
  {
    knee = wavelength - 1;
  }

  //basic DDS http://interface.khm.de/index.php/lab/interfaces-advanced/arduino-dds-sinewave-generator/index.html
  increment[sel] = (4294967296.00 * (freq)) / (dds_tune); //(2^32) * the frequency we want to output divided buy the calculation we made up in setup. Gives 32 bits of frequency resolution

  accumulator[sel] += increment[sel];
  waveindex[sel] = ((accumulator[sel]) >> (32 - 12)); //wavelength is 12 bits

  if (waveindex[sel] < knee) {
    tout = ((waveindex[sel]) * waveamp) / knee;
  }

  if (waveindex[sel] >= knee) {
    tout = ((((waveindex[sel] - knee) * waveamp) / (wavelength - knee)) * -1) + waveamp;
  }

  return tout - (waveamp / 2); //output a signal that goes from -2048 to 2048

}

//This is meant to "fold" a waveform, creating new harmonics
// just give it an input and it returns a folded output
int fold(int input) {
  int foldv = input;

  int h_res = 2040;
  int ih_res = h_res * -1;

  for (int i = 0; i < 8; i++) {
    if (foldv > h_res) {
      foldv -= ((foldv - h_res) * 2);
    }
    if (foldv < ih_res) {
      foldv += ((foldv * -1) - (h_res)) * 2;
    }
  }
  return foldv;
}
