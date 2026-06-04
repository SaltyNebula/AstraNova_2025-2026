#include <Wire.h>
#include "M5_UNIT_8SERVO.h"
#include <M5Unified.h>

M5_UNIT_8SERVO walk_bot;

// Servo assignments
#define R_foot 0
#define R_knee 1
#define R_hip  2
#define L_foot 3
#define L_knee 4
#define L_hip  5

// --- Link geometry (cm) ---
#define L1            6.8   // Thigh length
#define L2            7.35  // Shin length

// --- Squat Parameters ---
// Standing pose (straight legs)
float stand_x = 0.0;
float stand_z = 14.15;      // Fully extended (L1 + L2) so all servos hit exactly 90 degrees
float stand_tilt = 0.0;

// Squatting pose (Calibrated exactly to your 35, 20, 125 target angles)
float squat_x = 3.67;       // Shift feet forward to keep balance
float squat_z = 11.0;       // Lower the hips
float squat_tilt = 50.0;    // Tilt the torso forward by 50 degrees

int move_duration = 3000;   // 3 seconds for smooth interpolation
int hold_duration = 1500;   // Pause at top and bottom

// Track current physical angles
int current_angles[6] = {90, 90, 90, 90, 90, 90};

// Track last sent angles to avoid spamming the I2C bus
int last_angles[6] = {-1, -1, -1, -1, -1, -1};

// Lazy update: Only sends the command if the angle has actually changed.
// This prevents the 8SERVO module from resetting its PWM timer, which causes twitching!
void lazySetServoAngle(int ch, int angle) {
  if (last_angles[ch] != angle) {
    walk_bot.setServoAngle(ch, angle);
    last_angles[ch] = angle;
  }
}

// ===================================================================
// Inverse Kinematics Core
// ===================================================================
void calculateIK(float x, float z, float tilt_deg, float &hipAngle, float &kneeAngle, float &ankleAngle) {
  float d = sqrt(x*x + z*z);
  
  float max_len = L1 + L2 - 0.01;
  float min_len = fabs(L1 - L2) + 0.01;
  if (d > max_len) d = max_len;
  if (d < min_len) d = min_len;

  // 1. Knee angle via Law of Cosines
  float cos_knee = (d*d - L1*L1 - L2*L2) / (2.0 * L1 * L2);
  cos_knee = constrain(cos_knee, -1.0, 1.0);
  float knee_rad = acos(cos_knee);

  // 2. Hip angle
  float alpha = atan2(x, z);
  float cos_beta = (L1*L1 + d*d - L2*L2) / (2.0 * L1 * d);
  cos_beta = constrain(cos_beta, -1.0, 1.0);
  float beta = acos(cos_beta);
  
  float hip_rad = alpha + beta;

  // 3. Ankle angle (compensating for body tilt to keep foot flat)
  float tilt_rad = tilt_deg * PI / 180.0;
  float ankle_rad = hip_rad - knee_rad + tilt_rad;

  hipAngle = hip_rad * 180.0 / PI;
  kneeAngle = knee_rad * 180.0 / PI;
  ankleAngle = ankle_rad * 180.0 / PI;
}

// Computes the final target servo array for a given posture
void getTargetAngles(float x, float z, float tilt, int targets[6]) {
  float r_hip, r_knee, r_ankle;
  float l_hip, l_knee, l_ankle;

  calculateIK(x, z, tilt, r_hip, r_knee, r_ankle);
  calculateIK(x, z, tilt, l_hip, l_knee, l_ankle);

  // Right Leg
  targets[R_foot] = constrain(round(90 + r_ankle), 5, 175);
  targets[R_knee] = constrain(round(90 - r_knee),  5, 175);
  targets[R_hip]  = constrain(round(90 - r_hip),   5, 175);

  // Left Leg
  targets[L_foot] = constrain(round(90 - l_ankle), 5, 175);
  targets[L_knee] = constrain(round(90 + l_knee),  5, 175);
  targets[L_hip]  = constrain(round(90 + l_hip),   5, 175);
}

// ===================================================================
// Smooth Servo Interpolator
// This bypasses intermediate IK math entirely. It computes the end 
// angles and just smoothly sweeps the servos to those targets.
// ===================================================================
void moveServosSmoothly(int targets[6], int duration_ms) {
  int step_ms = 40; 
  int steps = duration_ms / step_ms; 
  if (steps == 0) steps = 1;

  int start_angles[6];
  for (int i = 0; i < 6; i++) {
    start_angles[i] = current_angles[i];
  }

  for (int i = 1; i <= steps; i++) {
    float progress = (float)i / steps;
    // Cosine smoothing (easing in and out)
    float smooth_p = (1.0 - cos(progress * PI)) / 2.0;

    for (int ch = 0; ch < 6; ch++) {
      int new_angle = start_angles[ch] + round(smooth_p * (targets[ch] - start_angles[ch]));
      
      // Only send if changed to prevent I2C saturation
      if (new_angle != current_angles[ch]) {
        lazySetServoAngle(ch, new_angle);
        current_angles[ch] = new_angle;
      }
    }
    delay(step_ms);
  }
}

void setup() {
  Serial.begin(115200);

  M5.begin();
  M5.Power.setExtPower(true);

  walk_bot.begin(&Wire, 9, 10, M5_UNIT_8SERVO_DEFAULT_ADDR);
  walk_bot.setAllPinMode(SERVO_CTL_MODE);

  // Instantly command the standing pose
  int stand_targets[6];
  getTargetAngles(stand_x, stand_z, stand_tilt, stand_targets);
  
  for (int ch = 0; ch < 6; ch++) {
    lazySetServoAngle(ch, stand_targets[ch]);
    current_angles[ch] = stand_targets[ch];
  }
  
  delay(3000); 
}

void loop() {
  int squat_targets[6];
  getTargetAngles(squat_x, squat_z, squat_tilt, squat_targets);

  int stand_targets[6];
  getTargetAngles(stand_x, stand_z, stand_tilt, stand_targets);

  Serial.println("Squatting down...");
  moveServosSmoothly(squat_targets, move_duration);
  delay(hold_duration);
  
  Serial.println("Standing up...");
  moveServosSmoothly(stand_targets, move_duration);
  delay(hold_duration);
}
