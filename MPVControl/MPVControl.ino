#include <SparkFun_TB6612.h>
#include <ESP32Servo.h>
#include <ESP32PWM.h>
#include <PS4Controller.h>
#include <Stepper.h>
#include <FastLED.h>
#include <EEPROM.h>

#define EEPROM_SIZE 8

#define SERVO_TILT 32
#define PUMP 33

#define STATUS_LED 22
#define NUM_LEDS 2

#define STEPS_PER_REV 2048

#define STEP_IN1 18
#define STEP_IN2 19
#define STEP_IN3 23
#define STEP_IN4 5

#define PWMB 27
#define BIN2 26
#define BIN1 25
#define STBY 15
#define AIN1 13
#define AIN2 12
#define PWMA 14

#define SPEED_REDUCTION 0.75

struct CRGB status_leds[NUM_LEDS];

Servo servo_tilt;
Servo servo_pan;

Stepper stepper = Stepper(STEPS_PER_REV, STEP_IN1, STEP_IN3, STEP_IN2, STEP_IN4);

Motor drive_motor_left = Motor(AIN1, AIN2, PWMA, 1, STBY);
Motor drive_motor_right = Motor(BIN1, BIN2, PWMB, 1, STBY);

int servo_angle = 45;
int servo_angles[4] = {45, 45, 45, 45};

int stepper_angle = 90;

int pump_speed = 0;
int pump_mode = 0;
int pump_pressure[4] = {33, 40, 100, 255};

int drive_speed = 0;
int drive_speed_left = 0;
int drive_speed_right = 0;

int led_mode = 0;

void setup() {
  Serial.begin(115200);
  PS4.begin("1a:2b:3c:01:01:01");
  Serial.println("Ready.");

  EEPROM.begin(EEPROM_SIZE);
  load_vars();

  servo_tilt.attach(SERVO_TILT);
  servo_tilt.write(servo_angle);
  
  servo_pan.write(stepper_angle);
  
  stepper.setSpeed(5);

  FastLED.addLeds<WS2811, STATUS_LED, BRG>(status_leds, NUM_LEDS);
  FastLED.setBrightness(255);
}

void loop() {
  if (PS4.isConnected()) {    
    if (PS4.LStickY()) {
      drive_speed = map(PS4.LStickY(), -127, 128, -255 * SPEED_REDUCTION, 255 * SPEED_REDUCTION);
      forward(drive_motor_left, drive_motor_right, drive_speed);
    }
    if(PS4.RStickY()) {
      int RStickTemp = map(PS4.RStickY(), -127, 128, -3, 3);
      if (abs(RStickTemp) > 1) {
        servo_angle += RStickTemp / 2;
        delay(25);
      }
    }
    if (PS4.RStickX()) {
      int RStickTemp = map(PS4.RStickX(), -127, 128, 3, -3);
      if (abs(RStickTemp) > 1) {
        stepper_angle += RStickTemp / 2;
        stepper.step(RStickTemp / 2);
        delay(1);
      }
    }
    
    if (PS4.Right()) {
      stepper_angle++;
      stepper.step(1);
      delay(1);
    }
    else if (PS4.Left()) {
      stepper_angle--;
      stepper.step(-1);
      delay(1);
    }

    if (PS4.Up()) {
      servo_angle++;
      delay(25);
    }
    else if (PS4.Down()) {
      servo_angle--;
      delay(25);
    }
    
    if (PS4.R2()) {
      pump_speed = pow(0.95746f, (float) -0.5*(PS4.R2Value()));
      if(pump_speed > 255) {
        pump_speed = 255;
      }
      
      pump_drive(pump_speed);
    }
    else if(PS4.Circle()) {
      Serial.println(pump_mode % 4);
      Serial.println(pump_pressure[pump_mode % 4]);
      pump_speed = pump_pressure[pump_mode % 4];
      pump_drive(pump_speed);
    }
    else {
      pump_speed = 0;
      pump_drive(pump_speed);
    }

    if(PS4.Triangle()) {
      save_vars();
      delay(300);
    }

    if(PS4.Square()) {
      Effect_Police_Strobe();  
    }

    if(PS4.Options()) {
      led_mode++;
      if(led_mode>3) {
        led_mode = 0;
      }
      delay(300);
    }

    if(PS4.R1()) {
      pump_mode++;
            
      if(pump_mode>7) {
        pump_mode = 0;
      }

      Serial.println(pump_mode);
      update_mode();
      delay(300);
    }

    if(PS4.L1()) {
      pump_mode--;
            
      if(pump_mode<0) {
        pump_mode = 6;
      }

      Serial.println(pump_mode);
      update_mode();
      delay(300);
    }
  }
  
  if (stepper_angle > 180) {
    stepper_angle = 180;
  }
  else if (stepper_angle < 10) {
    stepper_angle = 10;
  }

  if (servo_angle > 180) {
    servo_angle = 180;
  }
  else if (servo_angle < 10) {
    servo_angle = 10;
  }

  FastLED.show();

  servo_tilt.write(servo_angle);
  
  delay(1);
}

void servo_step(int angle)
{
  int step_difference = angle - servo_angle;
  
  for(int i = 0; i < abs(step_difference); i++) {
      if(step_difference > 0) {
        servo_angle += 1;
      }
      else if(step_difference < 0) {
        servo_angle -= 1;
      }

      servo_tilt.write(servo_angle);
      delay(10);
  }
}

void load_vars() 
{
  pump_pressure[0] = EEPROM.read(0);
  servo_angles[0] = EEPROM.read(4);
  Serial.print("Green: ");
  Serial.println(pump_pressure[0]);

  pump_pressure[1] = EEPROM.read(1);
  servo_angles[1] = EEPROM.read(5);
  Serial.print("Blue: ");
  Serial.println(pump_pressure[1]);

  pump_pressure[2] = EEPROM.read(2);
  servo_angles[2] = EEPROM.read(6);
  Serial.print("Red: ");
  Serial.println(pump_pressure[2]);

  pump_pressure[3] = EEPROM.read(3);
  servo_angles[3] = EEPROM.read(7);
  Serial.print("Purple: ");
  Serial.println(pump_pressure[3]);
}

void save_vars() 
{
  Serial.print("Saving: ");
  Serial.println((int) pump_speed);

  int pump_mode_temp = pump_mode % 4;

  pump_pressure[pump_mode_temp] = (int) pump_speed;
  servo_angles[pump_mode_temp] = servo_angle;
  
  EEPROM.write(pump_mode_temp, (int) pump_speed);
  EEPROM.write(pump_mode_temp+4, servo_angle);
  EEPROM.commit();
}

void update_mode()
{
  servo_step(servo_angles[pump_mode % 4]);
  
  if(pump_mode == 0) {
    PS4.setLed(0, 255, 0);
    fill_solid(status_leds, NUM_LEDS, CRGB::Green);
  }
  else if (pump_mode == 1) {
    PS4.setLed(0, 0, 255);
    fill_solid(status_leds, NUM_LEDS, CRGB::Blue);
  }
  else if (pump_mode == 2) {
    PS4.setLed(255, 0, 0);
    fill_solid(status_leds, NUM_LEDS, CRGB::Red);
  }
  else if (pump_mode == 3) {
    PS4.setLed(255, 255, 0);
    fill_solid(status_leds, NUM_LEDS, CRGB::Magenta);
  } else if(pump_mode == 4) {
    PS4.setLed(0, 255, 0);
    fill_solid_edit(CRGB::Green);
  }
  else if (pump_mode == 5) {
    PS4.setLed(0, 0, 255);
    fill_solid_edit(CRGB::Blue);
  }
  else if (pump_mode == 6) {
    PS4.setLed(255, 0, 0);
    fill_solid_edit(CRGB::Red);
  }
  else if (pump_mode == 7) {
    PS4.setLed(255, 255, 0);
    fill_solid_edit(CRGB::Magenta);
  }

  PS4.sendToController();  
  FastLED.show();
}

void pump_drive(int pwm) {
  analogWrite(PUMP, pwm);  
}

void Effect_Police_Strobe()
{
  if(led_mode == 0) {
    Effect_Police_Strobe_1();
  }
  else if (led_mode == 1) {
    Effect_Police_Strobe_2();
  }
  else if (led_mode == 2) {
    Effect_Police_Strobe_3();
  }
}

void Effect_Police_Strobe_1()
{
    Strobe_Left(CRGB::Red, 75); 
    Strobe_Left(CRGB::Red, 75); 
    delay(150);  
    Strobe_Right(CRGB::Blue, 75); 
    Strobe_Right(CRGB::Blue, 75); 
    delay(150); 
}

void Effect_Police_Strobe_2()
{
    Strobe_Left(CRGB::Red, 75); 
    Strobe_Right(CRGB::Blue, 75); 
    delay(150);  
}

void Effect_Police_Strobe_3()
{
    Strobe_Left(CRGB::Red, 150); 
    delay(150);  
    Strobe_Right(CRGB::Blue, 150); 
    delay(150);
}

void Strobe_Left(CRGB strobe_colour, int strobe_delay)
{
    FastLED.clear();
  
    status_leds[1] = strobe_colour;
    FastLED.show();

    delay(strobe_delay);

    FastLED.clear();
    FastLED.show();

    delay(strobe_delay);  
}

void Strobe_Right(CRGB strobe_colour, int strobe_delay)
{
    FastLED.clear();
    
    status_leds[0] = strobe_colour;
    FastLED.show();

    delay(strobe_delay);

    FastLED.clear();
    FastLED.show();
    
    delay(strobe_delay);  
}
void fill_solid_edit(CRGB colour) {
  FastLED.clear();

  status_leds[0] = colour;
  status_leds[1] = 0xFFFF00;

  FastLED.show();
}
