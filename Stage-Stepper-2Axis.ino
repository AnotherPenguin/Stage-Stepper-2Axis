/*  Controlling two stepper motors, on an X/Y stage, using two GeckoDrive G203V stepper drivers.
      this driver uses Direction, Step, and Disable
      Direction and Step will be commanded by the arduino board (using Mega 2560)
      Disable is commanded by hardware switch, possibly also arduino. TBD.
    The arduino onboard led will flash with each step (imperceptible as implemented; it just looks ON or OFF)
    External bi-color LED indicates stepping and limit switches
    Motion is commanded by a D-pad plugged into either the JOG pins or the INCREMENT pins.
    
   Use JOG for initial positioning, reset the board to zero the position counter, and then use incrementing to follow a grid pattern

    To-do: implement acceleration
      accept commands over serial, including custom intervals (command X centimeters, translate into Y steps)
      Improve the telemetry. does not need to send every half-second, only needs to send when changes occur.
*/
int ledPin = 13; //Green of bi-color LED, also onboard LED
int ledLimitPin = 12; //not implemented yet. Red of bi-color LED
//motor controller pins. I think we want to use PWM-capable outputs because they're fast? Dunno.
int XDirPin = 11;
int XStepPin = 10;
int YDirPin = 9;
int YStepPin = 8;
//input pins. regular digital inputs will work; these are just buttons
int UpJogPin = 24; //for running one step at a time. hold "jog" to translate continuously
int DownJogPin = 22;
int LeftJogPin = 28;
int RightJogPin = 26;
int UpIncPin = 25; //for running in our pre-determined fixed increments e.g. 1 centimeter
int DownIncPin = 23;
int LeftIncPin = 29;
int RightIncPin = 27;
int UpLimitPin = 30; //limit switches.
int DownLimitPin = 31;
int LeftLimitPin = 32;
int RightLimitPin = 33;
//motor drive characteristics
int XIncrement = 4000; //steps per fixed distance interval. My X axis is 4000 steps per cm (10160 steps per inch)
int YIncrement = 2360; //the Y axis is ~2360 steps per centimeter
int XpulseWidthMicros = 75;// how long we pulse the STEP signal. Arduino suggests values above 3 microseconds for consistency/granularity of the Arduino clock.
int YpulseWidthMicros = 125;
// pins -> variables
boolean UpJog;
boolean DownJog;
boolean LeftJog;
boolean RightJog;
boolean UpInc;
boolean DownInc;
boolean LeftInc;
boolean RightInc;
// tracking of position (step counts). position is set to 0 at start, can go negative or positive
long XPosition = 0L;
long YPosition = 0L;
int Xincrements = 0; //tracking how many times we've incremented. good for counting distance (e.g. centimeters)
int Yincrements = 0;

long timer = 0L; //used for controlling periodicity of the telemetry

void setup() {  
  pinMode(XDirPin, OUTPUT);
  pinMode(XStepPin, OUTPUT);
  pinMode(YDirPin, OUTPUT);
  pinMode(YStepPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(UpJogPin, INPUT_PULLUP); //we use INPUT_PULLUP, which sets the pin high (pulled up with internal 20kohm resistor) to prevent floating-pin syndrome
  pinMode(DownJogPin, INPUT_PULLUP);
  pinMode(LeftJogPin, INPUT_PULLUP);
  pinMode(RightJogPin, INPUT_PULLUP);
  pinMode(UpIncPin, INPUT_PULLUP);
  pinMode(DownIncPin, INPUT_PULLUP);
  pinMode(LeftIncPin, INPUT_PULLUP);
  pinMode(RightIncPin, INPUT_PULLUP);
  pinMode(UpLimitPin, INPUT_PULLUP);
  pinMode(DownLimitPin, INPUT_PULLUP);
  pinMode(LeftLimitPin, INPUT_PULLUP);
  pinMode(RightLimitPin, INPUT_PULLUP);
  
  Serial.begin(38400); //I prefer to use a relatively fast speed so the arduino spends less time talking, more time doing real work.
  Serial.println("Initialized");
}

void loop(){
 if (millis() >= timer){ //periodically report the X/Y position. doubles as a heartbeat
   timer = millis() + 500;
   Serial.println("position");
   Serial.print("X"); Serial.print(XPosition); Serial.print(" steps; "); Serial.println(Xincrements);
   Serial.print("Y"); Serial.print(YPosition); Serial.print(" steps; "); Serial.println(Yincrements);
 }
 //read and invert the input pins. We invert it because buttom pressed = LOW = FALSE (because INPUT_PULLUP), but I want operator input = TRUE
 //the way I'm using this information, it isn't crucial that I actually store this as variables...
 UpJog = !(digitalRead(UpJogPin));
 DownJog = !(digitalRead(DownJogPin));
 LeftJog = !(digitalRead(LeftJogPin));
 RightJog = !(digitalRead(RightJogPin));
 UpInc = !(digitalRead(UpIncPin));
 DownInc = !(digitalRead(DownIncPin));
 LeftInc = !(digitalRead(LeftIncPin));
 RightInc = !(digitalRead(RightIncPin));
 /*
 I'm not worried about debouncing.
 Now we need to interpret control inputs into direction and step outputs.
 Let's break this into axis-direction sets.
 */
 //Note: GeckoDrive acts on leading edge of pulse. Needs direction input for 200ns before first pulse and 200ns after the last pulse.
 if (UpJog){
   digitalWrite(YDirPin, HIGH); //let's say Up = positve = HIGH, Down = negative = LOW.
   delayMicroseconds(3); //wait for the stepper driver to set the direction. made obsolete by the Serial.print() which follows...
   Serial.println("jogging up | Y+");
   while ((digitalRead(UpJogPin) == LOW) && digitalRead(UpLimitPin)){ //Remember, inputs are inverse logic
     digitalWrite(ledPin, HIGH); //pulsing the LED
     digitalWrite(YStepPin, HIGH); //sending step signal
     delayMicroseconds(YpulseWidthMicros);
     digitalWrite(YStepPin, LOW);
     digitalWrite(ledPin, LOW);
     YPosition++; //increment our position value
     delayMicroseconds(YpulseWidthMicros); // this will give us something below a 50% duty cycle, exact rate relies on While loop cycle times
   }
 }
 if (UpInc && digitalRead(UpLimitPin)){ 
   digitalWrite(YDirPin, HIGH);
   delayMicroseconds(3);
   Serial.println("Increment Up | Y+");
   for (int count = 0; count < YIncrement; count++){ //we want to complete the whole increment as smoothly as possible, so we ignore the rest of the program
     digitalWrite(ledPin, HIGH);
     digitalWrite(YStepPin, HIGH);
     delayMicroseconds(YpulseWidthMicros);
     digitalWrite(YStepPin, LOW);
     digitalWrite(ledPin, LOW);
     YPosition++;
     delayMicroseconds(YpulseWidthMicros);
     if (!(digitalRead(UpLimitPin))) count = YIncrement; //if we hit a limit switch, force the For loop to finish
  }
  Yincrements++;
  delay(250);
 }
 
 if (DownJog){
   digitalWrite(YDirPin, LOW);
   delayMicroseconds(3);
   Serial.println("jogging down | Y-");
   while (!(digitalRead(DownJogPin)) && digitalRead(DownLimitPin)){
     digitalWrite(ledPin, HIGH);
     digitalWrite(YStepPin, HIGH);
     delayMicroseconds(YpulseWidthMicros);
     digitalWrite(YStepPin, LOW);
     digitalWrite(ledPin, LOW);
     YPosition--;
     delayMicroseconds(YpulseWidthMicros);
   }
 }
 if (DownInc && digitalRead(DownLimitPin)){ 
   digitalWrite(YDirPin, LOW);
   delayMicroseconds(3);
   Serial.println("Increment Down | Y-");
   for (int count = 0; count < YIncrement; count++){
     digitalWrite(ledPin, HIGH);
     digitalWrite(YStepPin, HIGH);
     delayMicroseconds(YpulseWidthMicros);
     digitalWrite(YStepPin, LOW);
     digitalWrite(ledPin, LOW);
     YPosition--;
     delayMicroseconds(YpulseWidthMicros);
     if (!(digitalRead(DownLimitPin))) count = YIncrement;
  }
  Yincrements--;
  delay(250);
 }
 
 if (LeftJog){
   digitalWrite(XDirPin, HIGH);
   delayMicroseconds(3);
   Serial.println("jogging left | X+");
   while (!(digitalRead(LeftJogPin)) && digitalRead(LeftLimitPin)){
     digitalWrite(ledPin, HIGH);
     digitalWrite(XStepPin, HIGH);
     delayMicroseconds(XpulseWidthMicros);
     digitalWrite(XStepPin, LOW);
     digitalWrite(ledPin, LOW);
     XPosition++;
     delayMicroseconds(XpulseWidthMicros);
   }
 }
 if (LeftInc && digitalRead(DownLimitPin)){ 
   digitalWrite(XDirPin, HIGH);
   delayMicroseconds(3);
   Serial.println("Increment Left | X+");
   for (int count = 0; count < XIncrement; count++){
     digitalWrite(ledPin, HIGH);
     digitalWrite(XStepPin, HIGH);
     delayMicroseconds(XpulseWidthMicros);
     digitalWrite(XStepPin, LOW);
     digitalWrite(ledPin, LOW);
     XPosition++;
     delayMicroseconds(XpulseWidthMicros);
     if (!(digitalRead(LeftLimitPin))) count = XIncrement;
  }
  Xincrements++;
  delay(250);
 }
 
 if (RightJog){
   digitalWrite(XDirPin, LOW);
   delayMicroseconds(3);
   Serial.println("jogging right | X-");
   while (!(digitalRead(RightJogPin)) && digitalRead(RightLimitPin)){
     digitalWrite(ledPin, HIGH);
     digitalWrite(XStepPin, HIGH);
     delayMicroseconds(XpulseWidthMicros);
     digitalWrite(XStepPin, LOW);
     digitalWrite(ledPin, LOW);
     XPosition--;
     delayMicroseconds(XpulseWidthMicros);
   }
 }
 if (RightInc && digitalRead(RightLimitPin)){ 
   digitalWrite(XDirPin, LOW);
   delayMicroseconds(3);
   Serial.println("Increment right | X-");
   for (int count = 0; count < XIncrement; count++){
     digitalWrite(ledPin, HIGH);
     digitalWrite(XStepPin, HIGH);
     delayMicroseconds(XpulseWidthMicros);
     digitalWrite(XStepPin, LOW);
     digitalWrite(ledPin, LOW);
     XPosition--;
     delayMicroseconds(XpulseWidthMicros);
     if (!(digitalRead(RightLimitPin))) count = XIncrement;
  }
  Xincrements--;
  delay(250);
 }
}

