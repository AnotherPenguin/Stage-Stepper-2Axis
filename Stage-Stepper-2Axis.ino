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
int LeftJogPin = 26;
int RightJogPin = 28;
int UpIncPin = 25; //for running in our pre-determined fixed increments e.g. 1 centimeter
int DownIncPin = 23;
int LeftIncPin = 27;
int RightIncPin = 29;
int UpLimitPin = 30; //limit switches.
int DownLimitPin = 31;
int LeftLimitPin = 32;
int RightLimitPin = 33;
//motor drive characteristics
int XIncrement = 13740; //steps per fixed distance interval. My X axis is 4000 steps per cm (10160 steps per inch)
int XIncrementBig = 14140; //every-other cell gap is larger because wires.
int XIncrementSmall = 13740; //store this here so Big can overwrite XIncrement, and then the value can be restored.
int YIncrement = 15942; //the Y axis is ~2360 steps per centimeter
int XpulseWidthMicros = 75;// how long we pulse the STEP signal. Arduino suggests values above 3 microseconds for consistency/granularity of the Arduino clock.
int YpulseWidthMicros = 125;
// jog mode acceleration
int JogRate = 512;
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
  telemetry();
}

void loop(){
 //read and invert the input pins. We invert it because buttom pressed = LOW = FALSE (because INPUT_PULLUP), but I want operator input = TRUE
 //the way I'm using this information, it isn't crucial that I actually store this as variables... pins could be read directly at each function.
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
   Serial.println("Y+ jogging up");
   while ((digitalRead(UpJogPin) == LOW) && digitalRead(UpLimitPin)){ //Remember, inputs are inverse logic
     digitalWrite(ledPin, HIGH); //pulsing the LED
     digitalWrite(YStepPin, HIGH); //sending step signal
     delayMicroseconds(YpulseWidthMicros);
     digitalWrite(YStepPin, LOW);
     digitalWrite(ledPin, LOW);
     YPosition++; //increment our position value
     delayMicroseconds(YpulseWidthMicros); // this will give us something below a 50% duty cycle, exact rate relies on While loop cycle times
     delay(JogRate);
     JogRate = JogRate / 2;
   }
   JogRate = 512;
   telemetry();
 }
 if (UpInc && digitalRead(UpLimitPin)){ 
   digitalWrite(YDirPin, HIGH);
   delayMicroseconds(3);
   Serial.println("Y+ increment up");
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
  telemetry();
  delay(250);
 }
 
 if (DownJog){
   digitalWrite(YDirPin, LOW);
   delayMicroseconds(3);
   Serial.println("Y- jogging down");
   while (!(digitalRead(DownJogPin)) && digitalRead(DownLimitPin)){
     digitalWrite(ledPin, HIGH);
     digitalWrite(YStepPin, HIGH);
     delayMicroseconds(YpulseWidthMicros);
     digitalWrite(YStepPin, LOW);
     digitalWrite(ledPin, LOW);
     YPosition--;
     delayMicroseconds(YpulseWidthMicros);
     delay(JogRate);
     JogRate = JogRate / 2;
   }
   JogRate = 512;
   telemetry();
 }
 if (DownInc && digitalRead(DownLimitPin)){ 
   digitalWrite(YDirPin, LOW);
   delayMicroseconds(3);
   Serial.println("Y- increment down");
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
  telemetry();
  delay(250);
 }
 if (LeftJog){
   digitalWrite(XDirPin, LOW);
   delayMicroseconds(3);
   Serial.println("X- jogging left");
   while (!(digitalRead(LeftJogPin)) && digitalRead(LeftLimitPin)){
     digitalWrite(ledPin, HIGH);
     digitalWrite(XStepPin, HIGH);
     delayMicroseconds(XpulseWidthMicros);
     digitalWrite(XStepPin, LOW);
     digitalWrite(ledPin, LOW);
     XPosition--;
     delayMicroseconds(XpulseWidthMicros);
     delay(JogRate);
     JogRate = JogRate / 2;
   }
   JogRate = 512;
   telemetry();
 }
 if (LeftInc && digitalRead(DownLimitPin)){ 
   digitalWrite(XDirPin, LOW);
   delayMicroseconds(3);
   Serial.println("X- increment left");
   if (Xincrements % 2){ //if moving left, we use a large gap when leaving an odd-numbered increment space
     XIncrement = XIncrementBig;
   }
   else {
     XIncrement = XIncrementSmall;
   }
   for (int count = 0; count < XIncrement; count++){
     digitalWrite(ledPin, HIGH);
     digitalWrite(XStepPin, HIGH);
     delayMicroseconds(XpulseWidthMicros);
     digitalWrite(XStepPin, LOW);
     digitalWrite(ledPin, LOW);
     XPosition--;
     delayMicroseconds(XpulseWidthMicros);
     if (!(digitalRead(LeftLimitPin))) count = XIncrement;
  }
  Xincrements--;
  telemetry();
  delay(250);
 }
 
 if (RightJog){
   digitalWrite(XDirPin, HIGH);
   delayMicroseconds(3);
   Serial.println("X+ jogging right");
   while (!(digitalRead(RightJogPin)) && digitalRead(RightLimitPin)){
     digitalWrite(ledPin, HIGH);
     digitalWrite(XStepPin, HIGH);
     delayMicroseconds(XpulseWidthMicros);
     digitalWrite(XStepPin, LOW);
     digitalWrite(ledPin, LOW);
     XPosition++;
     delayMicroseconds(XpulseWidthMicros);
     delay(JogRate);
     JogRate = JogRate / 2;
   }
   JogRate = 512;
   telemetry();
 }
 if (RightInc && digitalRead(RightLimitPin)){ 
   digitalWrite(XDirPin, HIGH);
   delayMicroseconds(3);
   Serial.println("X+ increment right");
   if (Xincrements % 2){ //if moving right, we use a small gap when leaving an odd-numbered increment space
     XIncrement = XIncrementSmall;
   }
   else {
     XIncrement = XIncrementBig;
   }
   for (int count = 0; count < XIncrement; count++){
     digitalWrite(ledPin, HIGH);
     digitalWrite(XStepPin, HIGH);
     delayMicroseconds(XpulseWidthMicros);
     digitalWrite(XStepPin, LOW);
     digitalWrite(ledPin, LOW);
     XPosition++;
     delayMicroseconds(XpulseWidthMicros);
     if (!(digitalRead(RightLimitPin))) count = XIncrement;
  }
  Xincrements++;
  telemetry();
  delay(250);
 }
}

void telemetry(){
   Serial.println("position");
   Serial.print("X "); Serial.print(XPosition); Serial.print(" steps; "); Serial.print(Xincrements); Serial.println(" increments");
   Serial.print("Y "); Serial.print(YPosition); Serial.print(" steps; "); Serial.print(Yincrements); Serial.println(" increments");
 }

