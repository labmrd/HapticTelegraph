/*
  Starter code for Haptic Telegraph.  Use for diagnostics and initial build. 
  Comment out uneeded sections in loop(): e.g., no pots, H bridge, or encoders?
  comment that stuff out to only display what you like.  Will run either way.
  Prof. Tim Kowalewski 2024
  Copyleft, public domain. 
*/

// #define ENCODER_USE_INTERRUPTS
#include <Encoder.h>         // Teensy encoder library Examples-->Teensy-->Encoder
#include "Adafruit_HX711.h"  // Library Manager-->serach HX711 Adafruit --> intall
#include <Servo.h>           // Default Arduino servo library, only needed for UNMODIFIED servos
// note: Servo.h defines refresh rate of the pulse driving the servo position.  It's only 50Hz by default
// to speed it up change #define REFRESH_INTERVAL    20000     // minumim time to refresh servos in microseconds
// to something like 10000 (100Hz instead of 50Hz)  or whatever max your servo can take.
// analog servos are around 50Hz or a bit more; digital servos can often go up to 100's of Hz.

#include "HapticTelegraphPinsEtc.h"    // a literal copy-and-paste by the pre-processor (# commands)
                                       // tweak your pins, calibration vals, etc. in this .h file.
unsigned long int printPeriod = 50;    // printing update rate in ms, use ~50 for Serial Plotter
bool bPrintAllValues = true;           // prints all internal diagnostics values to serial if true
bool bTeleoperatingOverSerial = false; // true: sends cmdB to serial receives B commands from serial
                                       // which drive local actuator B (overites: cmdB = cmbBremote)

// Handles incoming serial input.  When EOL-terminated strings come in, does the following:
//  'P'  -- toggles Printing out of all diagnostic data (bPrintAllValues)
//  'T'  -- toggles Teleoperation; (bTeleoperatingOverSerial)
//  B##.#-- writes ##.# to cmdBremote; when teleoperation is toggled on, cmdBrenite overwrites cmdB, 
//                  driving actuator B locally with the remote command.
//  Anything else -- returns an error
float handleSerial(float cmdA, float cmdB);  // definition at bottom

///////////////////////////////////////////////////////////////////////////////////////////////////////
//   SETUP  Code in this function only runs once; initialize stuff, default values
///////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {

  // put your setup code here, to run once:
  // pinMode(MotorEnablePin, INPUT); //not used by default, 3.3 vs 5V tolerant
  pinMode(LedPin, OUTPUT);
  digitalWrite(LedPin, HIGH);  // LED on means power is on
  pinMode(MotorA1pin, OUTPUT);
  pinMode(MotorA2pin, OUTPUT);
  pinMode(MotorB1pin, OUTPUT);
  pinMode(MotorB2pin, OUTPUT);
  analogWriteFrequency(MotorA1pin, PwmFrequency);
  analogWriteFrequency(MotorA2pin, PwmFrequency);
  analogWriteFrequency(MotorB1pin, PwmFrequency);
  analogWriteFrequency(MotorA2pin, PwmFrequency);
  analogWriteResolution(PwmResolution);  // analogWrite value 0 to 4095, or 4096 for high

  // turn motors off, etc.
  digitalWrite(MotorA1pin, LOW);
  digitalWrite(MotorA2pin, LOW);
  digitalWrite(MotorB1pin, LOW);
  digitalWrite(MotorB2pin, LOW);

  // Adafruit Load cell setup ...
  loadCellAdaA.begin();
  loadCellAdaB.begin();
  // read and toss 3 values each to tare each load cell (select origin, 0 grams)
  Serial.println("Tare-ing load cell ...");
  for (uint8_t t = 0; t < 3; t++) {
    loadCellA_offset = loadCellAdaA.readChannelRaw(CHAN_A_GAIN_128);
    loadCellB_offset = loadCellAdaB.readChannelRaw(CHAN_A_GAIN_128);
  }

  // set encoder value at current position; fyi encoder resets home each power cycle,
  // cycling the enable button (h bridge power) does not reset encoder "home" position
  encoderA.write(0);
  encoderB.write(0);

  // if (unmodified) servos are used, initialize them here ...  
  servoA.attach(ServoDrivePinA);
  servoA.write(ServoInitialPosA);
  servoB.attach(ServoDrivePinB);
  servoB.write(ServoInitialPosB);
  // move servos to confirm operation and sign (motion direction should be both up)
  delay(250);
  servoA.write(ServoInitialPosA - 10);
  servoB.write(ServoInitialPosB + 10);
  delay(250);
  servoA.write(ServoInitialPosA);
  servoB.write(ServoInitialPosB);
  // int i;
  // while(1){

  
  // if (Serial.available()){    
  //   i = Serial.parseInt();
  //   if (i==-1){
  //     servoA.write(0); delay(1000);    
  //     Serial.print("write(o)");
  //   }else if (i>1){
  //     Serial.println("Writing" + String(i));
  //     servoA.writeMicroseconds(i);
  //   }
  // }
  // i=0;
    
  //servoA.writeMicroseconds(700); delay(500);
  //servoA.write(90); delay(1000);  
  //servoA.writeMicroseconds(1500); delay(500);
  //servoA.write(180); delay(1000);
  //}
  
  // run serial at 115200 or greater; will need to interact over serial with decent latency
  Serial.begin(115200);
  Serial.println("Haptic Telegraph is running...");
  Serial.println("Press 'P<enter>' to toggle printing of all diagnostic data.");
  Serial.println("Type  'B12.3' to set HapticTelegraph cmdB to 12.3 to drive actuator B.");
}



///////////////////////////////////////////////////////////////////////////////////////////////////////
//   LOOP   This function will repeat until power down;  Put your main code here
///////////////////////////////////////////////////////////////////////////////////////////////////////
int cmdPWM = 0, delta = 1;
unsigned long t0, tPrint = 0;  // timestamps
float forceA, forceB, posA, posB;
float cmdA, cmdB, cmdBremote, errA = 0, errB = 0;
const float degToMicroseconds = (2000.0)/180.0;  //HS 422 hs 500us at 0deg, 2500us at 180deg
void loop() {

  t0 = millis();

  // Read forces; for blocking call, will take 1/80Hz (~13ms)each; for nonblocking, returns immediately
  // // BLOCKING:
  forceA = getLoadA();
  forceB = getLoadB();
  // NON BLOCKING: (useful if you can read position and write to H-bridge between loadcell reads)
  // if (!loadCellAdaA.isBusy())  forceA = getLoadA();
  // if (!loadCellAdaB.isBusy())  forceB = getLoadB();


  // posA = getModServoPosA();        // reads pot and returns degree pos from MODIFIED servo.
  // posB = getModServoPosB();
  // For unmodified servo. servoA.read() will just return whatever you sent via .write(), so not the
  // true position of the servo. But when needed you can still use:
  posA = servoA.read();  // UNMODIFIED SERVO, does not read true position, just written val
  posB = servoB.read();  // UNMODIFIED SERVO

  // read position via encoder (if installed, e.g. N20)
  // posA = encoderA.read();
  // posB = encoderB.read();

  ////////////////////////////////////////////////////////////////////
  // Print all values for serial plotter:
  // after printPeriod ms since the last print, go ahead and print...
  if (bPrintAllValues && millis() - tPrint >= printPeriod) {
    Serial.print(" ForceA:");
    Serial.print(forceA);
    Serial.print(" ForceB:");
    Serial.print(forceB);

    // Modified or unmodified servo pos.
    Serial.print("   ServoPosA:");
    Serial.print(posA);
    Serial.print(" ServoPosB:");
    Serial.print(posB);

    // // N20 encoder position in deg, if encoders are unconnected, will just read 0.
    // Serial.print("  EncA:");  Serial.print(encoderA.read() * EncCountsToDeg);
    // Serial.print(" EncB:" );  Serial.print(encoderB.read() * EncCountsToDeg);

    // // H-Bridge DC motor command
    // Serial.print("  cmdPWM:");  Serial.print(cmdPWM);
    // Serial.print(" MaxPWM:"+String(MaxPWM)); Serial.print(", ");

    // final commands to be sent to actuator A and B (inputs to plant)
    Serial.print("   cmdA:");
    Serial.print(cmdA);
    Serial.print(" cmdB:");
    Serial.print(cmdB);

    Serial.print("  deltaT[ms]:");
    Serial.print(millis() - t0);
    tPrint = millis();
    Serial.println("");  //Serial.flush();
  }


  /////////////////////////////////////////////////////////////////////
  // UNMODIFIED servo and force control (note, servos opposite signs)
  //
  // // single actuator force control (e.g control force error to zero)
  int targetForce = 0;  // grams
  // computer error to regulate:  could be force, position, impedance...
  errA = targetForce - forceA;
  errB = targetForce - forceB;

  // simple proportional control about setpoint, rejecting error disturbance
  cmdA = -(0.0) * errA + 90;  // a virtual spring about the 90 deg origin
  cmdB =  (1/5.0)*errB + 90;  // a virtual spring about the 90 deg origin
  // OBSERVE: *gently,slowly* pushing on A-> high reaction force, B->low

  ///////////////////////////////////////////////////////////////////////
  //
  // ***  LOCAL TELEOPERATION (on same telegraph between A and B)  ***
  //
  ///////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////
  // * Unidirectional A to B, no feedback from B to A *  //

  // // (i) Make an open loop dial gauge: B position displays force on A:
  // // observe: finger force on A causes sensisitve dial-like motion at B
  // // even if touching the "floor", easy to crush soft object;
  // // unmodified servo has a proportaionl controller (very stiff spring)
  // // observe: hold down base lid above usb port, tilting on short edge
  // //       once it touches floor, physically linked, A "kicks back"
  // // observe: hold down base lid near servoA, tilting along long edge,
  // //       once it touches floor, physically linked,  A "flees" from finger
  // cmdA =  90;                  // locally regulate A to constant position
  // cmdB = -forceA / 5.0 + 90;   // teleoperating forceA to position B

  // (ii) Make A a virtual (local) spring, to get a position deflection; can
  // be used as an input device; position of A maps to position of B
  // observe: force on A moves both A and B; touching B does nothing to A
  // observe: touching floor at B is more stable, still easily crushes soft object at B
  // cmdA =  forceA / 50.0 + 90;  // a local spring at A
  // cmdB = -forceA / 50.0 + 90;  // transmit force to B

  // (iii) B mirrors A position but w/ a (different) virtual spring implemented locally at B too
  // observe: move "spring" A with your finger, B should copy motion of A;
  // observe: move spring B with finger, deflects like spring but no motion at A
  // observe: touch soft object with B using A; should be more genle,
  //    like touching  with remote spring at B  (very rudimentary impedance/admitance control)
  // may need to increase denominator scale to lessen effect of forces to stabilize system
  // cmdA =  forceA / 50.0 + 90;  // a local spring at A to transmit a position command to B
  // cmdB = -forceA / 50.0 + 90 - forceB / 50.0; // cmdA minus B "spring" forces

  //////////////////////////////////////////////////////////////////////////////////////
  // * Bidriectional, A and B exchange signals (only forces for unmodified servo )  * //

  // (iv) bilateral force causality: force A to pos B, force of B to pos A
  // observe: move paddle A; locally like a stiff spring bc of unmodified servo position P-control
  //          B should move more than A, like the force dial gauge again, but less sensitive here.
  //          Now if you move B, it should likewise move A.  This is like *bilateral* force-causality
  //          haptics (though we are faking it bc we can't command motor torque, only servo pos)
  // observe: try to crush a soft object with A, should feel a kickback force and pos on A upon touch
  //          compare with touching the rigid floor and trying to go through it.
  // TIP:  tilt the base so actuator B just touches target, then apply force to A
  // cmdA =  forceB / 50.0 + 90;  // teleoperating forceB to positionA
  // cmdB = -forceA / 50.0 + 90;  // teleoperating forceA to positionB

  /////////////////////////////////////////////////////////////////////////////////////
  //
  // ***  REMOTE TELEOPERATION  (over python serial link, localhost or remote IP) ***
  //
  /////////////////////////////////////////////////////////////////////////////////////
  // send motion commands from A over serial; consume commands from B over serial

  // sends cmdB over serial to drive distant actuatorB,
  // receives remcmdB to drive local actuatorB from cmdBremote
  if (bTeleoperatingOverSerial) {
    
    // write local cmdB to serial to run distant actuatorB;   FORMAT: "B#.##x\n" aka 'B'<float>'\n'
    Serial.print("B"); Serial.println(cmdB); // sends cmbB to distant location
    // overwrite local cmdB with any commands that have been received externally (over serial)
    cmdB = cmdBremote;     
  }
  // //Note: to render local virtual spring at B about  remote command add:
  // cmdB = cmdBremote - forceB / 50.0;  // rudimentary impedance control 

  // command servos to their position
  //servoA.write(cmdA); // only integer values between 0 and 180 (lower resolution)
  //servoB.write(cmdB);  
  servoA.writeMicroseconds(500.0 + cmdA * degToMicroseconds);  // 1000->0 deg; 2000->180 deg
  servoB.writeMicroseconds(500.0 + cmdB * degToMicroseconds);

  // // Introduce time delay or reduction in sampling rate to explore destabilization effects...
  delay(1); //delay(250);

  /////////////////////////////////////////////////////////////////
  // H Bridge and position sensing-based control schemes ...
  // run N20 motors at 15% of full voltage supplied to H-bridge.
  //runMotorA( MaxPWM * 0.15 );
  //runMotorB( MaxPWM * 0.15 );

  // //drive N20 motors forwards and backwards by small PWM ramp (20% of MaxPWM = 0.2*MaxPWM )
  // int v = MaxPWM * 0.2f;
  // runMotorA( cmd );  runMotorB( cmd );
  // if (cmd >  v) delta = -1;
  // if (cmd < -v) delta = 1;
  // cmd += delta;

  // // Force control; 100g
  // cmdPWM = -(loadCellA.get_units()-100) * 20;
  handleSerial();
}


// full definition of serial handling; B##.# P T
void handleSerial() {

  // parse info from remote sender and update, overwirte cmdB if it comes in
  // incoming command expected FORMAT:  "B#.##\n" or "P\n" 
  // parse any other serial commands here. 
  while (Serial.available()) {

    String s = Serial.readStringUntil('\n');

    // based on the first character, interperet the rest of the string
    switch (s[0]) {
      
      // cmdB // B124.5 is a float for cmdB, to drive actuator B
      case 'B':   
        cmdBremote = s.substring(1).toFloat();
        //Serial.println("#Extracted Float:" + String(cmdB));
        break;
      
      // toggle bPrintAllValues to print all data or not
      case 'P':   
        bPrintAllValues = !bPrintAllValues; // toggle
        break;

      // toggle Teleoperation bTeleoperatingOverSerial
      case 'T':   
        bTeleoperatingOverSerial = !bTeleoperatingOverSerial; // toggle        
        break;

      default:
         Serial.println("#ERROR: Failed To Parse B##.##");
    }         
  }
  return ;
}
