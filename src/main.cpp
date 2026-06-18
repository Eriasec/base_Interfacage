
#include "lvgl.h"
#include "../../lib/lvgl/examples/lv_examples.h"
#include "Arduino.h"

// LVGL configuration
#define CANVAS_WIDTH  80
#define CANVAS_HEIGHT  40

// PWM configuration
#define TIM3_PSC 64
#define TIM3_ARR 33750
#define PWM_PERIOD 20000                                      // 20us period for 50Hz PWM frequency
#define FT90M_MINIMUM_STEP (TIM3_ARR * 4 / PWM_PERIOD)        // 4us step for 50Hz PWM frequency
#define FT90M_RANGE_DEG 280                                   // 280 degrees full range for the stepper motor
#define FT90M_MIN_PERIOD 1000                                 // 1ms minimum period for the stepper motor
#define FT90M_MAX_PERIOD 2000                                 // 2ms maximum period for the stepper motor
#define PWM_MAX_TICS (float)(TIM3_ARR * FT90M_MAX_PERIOD) / PWM_PERIOD   // Maximum ticks for the PWM signal
#define PWM_MIN_TICS (float)(TIM3_ARR * FT90M_MIN_PERIOD) / PWM_PERIOD   // Minimum ticks for the PWM signal
#define PWM_USABLE_TICS (float)PWM_MAX_TICS - PWM_MIN_TICS               // Usable ticks for the PWM signal

// LD2450 configuration
#define LD2450_MAX_RANGE_MM 8000

// Class Target for LD2450
class Target {
  public:
    int16_t x;
    int16_t y;
    int16_t speed;
    int16_t resolution;
    uint16_t absoluteDistance;
    bool targetIsValid;
    float angle;

    Target() : x(0), y(0), speed(0), resolution(0), absoluteDistance(0), angle(140), targetIsValid(false) {}

    bool ProcessTargetData(char data[8]) {
      x = data[0];
      x |= ((data[1] & 0x7F) << 8);
      if(!(data[1] & 0x80)) { // Check if the sign bit is set
        x = -x;               // Convert to negative value if needed
      }
      y = data[2];
      y |= ((data[3] & 0x7F) << 8);
      if(!(data[3] & 0x80)) { // Check if the sign bit is set
        y = -y;               // Convert to negative value if needed
      }
      speed = data[4];
      speed |= ((data[5] & 0x7F) << 8);
      if(!(data[5] & 0x80)) { // Check if the sign bit is set
        speed = -speed;       // Convert to negative value if needed
      }
      resolution = (data[7] << 8) | data[6];
      absoluteDistance = abs(x) + abs(y); // Calculate absolute distance
      if(x | y) // Verifie that a valid target has been detected. The LD2450 sends empty frames (0x00) when no target is detected
      {
        targetIsValid = true;
        angle = (atan2(y, x) * 180 / PI) - 90; // Calculate angle in degrees
        return targetIsValid;
      } else {
        targetIsValid = false;
        angle = 0;
        return targetIsValid;
      }
    }
};

// Class LD2450 for the sensor
class LD2450 {
  private:
    char inputBuffer[30]; // Buffer to hold incoming data
    
  public:
    Target targets[3]; // Assuming a maximum of 3 targets
    void begin(HardwareSerial &serial) {
      serial.begin(256000);
      serial.setTimeout(1000); // Set a timeout for Serial reads
    }

    int read() {
      Serial6.readBytes(this->inputBuffer, 30); // Read 30 bytes 
      if(this->inputBuffer[0] == (0xAA) &&      // Verify that header is correct
        this->inputBuffer[1] == (0xFF) &&
        this->inputBuffer[2] == (0x03) &&
        this->inputBuffer[3] == (0x00))
      {
        targets[0].ProcessTargetData(&inputBuffer[4]);  // Process the target data for the first target
        targets[1].ProcessTargetData(&inputBuffer[12]); // Process the target data for the second target
        targets[2].ProcessTargetData(&inputBuffer[20]); // Process the target data for the third target
      } else {
        Serial.println("Received unknown data:");
        return -1; // Return -1 if the header does not match
      }
      return 1; // Return 1 if the header matches
    }
};

// Variables declaration
HardwareSerial Serial6(PC7, PC6);

lv_obj_t * targetPoint = nullptr;
lv_obj_t * targetPoint2 = nullptr;
lv_obj_t * targetPoint3 = nullptr;
lv_obj_t * targetLabel = nullptr;
int selectedTarget = -1;
bool trackingEnabled = false;
LD2450 ld2450;
int pwmCounter = PWM_USABLE_TICS/2;
int pwmGoalCounter = PWM_USABLE_TICS/2;

// Function prototypes
static void MX_TIM3_Init(void);

static void update_target_selection_style(void);
static void target_click_cb(lv_event_t * e);
static void button_click_cb(lv_event_t * e);
static void update_target_point(void);
static void update_target_point_obj(lv_obj_t * obj, int index);
static int map_value(int value, int in_min, int in_max, int out_min, int out_max);
int deg_to_tics(float deg);
int relative_deg_to_tics(float relativeDeg);
void LvglInit();

#ifdef ARDUINO

#include "lvglDrivers.h"

void mySetup()
{
  // Initialize STM
  HAL_Init();
  SystemClock_Config();

  // Initialize serial connection for debug
  Serial.begin(921600);
  Serial.println("Initializing...");

  // Initialize sensor's serial connection
  ld2450.begin(Serial6);
  Serial.println("Serial6 initialized at 256000 baud");

  // General initializations
  LvglInit();
  MX_TIM3_Init();

  // Place the sensor in neutral position
  TIM3->CCR1 = deg_to_tics(140);
}

void loop()
{
  while (Serial6.available()) { // Checks if serial connection is established
    if (ld2450.read()>=0) {     // Starts reading sensor data and checks if no error is generated
      if (lvglLock(pdMS_TO_TICKS(10))) {  // Wait for the screen to be free and locking it's access from other functions
        update_target_point();            // Update "target" points on the screen
        lvglUnlock();                     // Free the screen
      }
      if(trackingEnabled) {     // If user selected tracking mode, try keeping selected target in the center of the sensor's FOV
        pwmGoalCounter = relative_deg_to_tics(ld2450.targets[selectedTarget].angle);  // Calculate number of PWM tics to go
        if(pwmGoalCounter > -1) { // Catch error 
          pwmCounter = (pwmGoalCounter - pwmCounter) * 0.05 + pwmCounter; // Proportional control of the servo
          TIM3->CCR1 = pwmCounter;
        }
      }
    }
  }
}

void myTask(void *pvParameters)
{
  // Init
  TickType_t xLastWakeTime;
  // Lecture du nombre de ticks quand la tâche débute
  xLastWakeTime = xTaskGetTickCount();
  while (1)
  {
    // Loop

    // Endort la tâche pendant le temps restant par rapport au réveil,
    // ici 200ms, donc la tâche s'effectue toutes les 200ms
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200)); // toutes les 200 ms
  }
}

// __________ Initialization of style, function and position of lvgl objects __________ //
void LvglInit()
{
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101820), 0);

  targetPoint = lv_obj_create(lv_screen_active());
  lv_obj_set_size(targetPoint, 10, 10);
  lv_obj_set_style_radius(targetPoint, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(targetPoint, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_width(targetPoint, 0, 0);
  lv_obj_set_style_pad_all(targetPoint, 0, 0);
  lv_obj_add_flag(targetPoint, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(targetPoint, target_click_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_set_pos(targetPoint, 0, 0);

  targetPoint2 = lv_obj_create(lv_screen_active());
  lv_obj_set_size(targetPoint2, 10, 10);
  lv_obj_set_style_radius(targetPoint2, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(targetPoint2, lv_color_hex(0x00FF00), 0);
  lv_obj_set_style_border_width(targetPoint2, 0, 0);
  lv_obj_set_style_pad_all(targetPoint2, 0, 0);
  lv_obj_add_flag(targetPoint2, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(targetPoint2, target_click_cb, LV_EVENT_CLICKED, (void *)1);
  lv_obj_set_pos(targetPoint2, 0, 0);

  targetPoint3 = lv_obj_create(lv_screen_active());
  lv_obj_set_size(targetPoint3, 10, 10);
  lv_obj_set_style_radius(targetPoint3, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(targetPoint3, lv_color_hex(0x00A5FF), 0);
  lv_obj_set_style_border_width(targetPoint3, 0, 0);
  lv_obj_set_style_pad_all(targetPoint3, 0, 0);
  lv_obj_add_flag(targetPoint3, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(targetPoint3, target_click_cb, LV_EVENT_CLICKED, (void *)2);
  lv_obj_set_pos(targetPoint3, 0, 0);

  targetLabel = lv_label_create(lv_screen_active());
  lv_obj_set_size(targetLabel, lv_pct(55), LV_SIZE_CONTENT);
  lv_obj_set_style_text_color(targetLabel, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(targetLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_bg_opa(targetLabel, LV_OPA_90, 0);
  lv_obj_set_style_bg_color(targetLabel, lv_color_hex(0x000000), 0);
  lv_obj_set_style_pad_all(targetLabel, 8, 0);
  lv_obj_set_style_radius(targetLabel, 6, 0);
  lv_obj_align(targetLabel, LV_ALIGN_TOP_LEFT, 6, 6);
  lv_label_set_text(targetLabel, "Attente des mesures...");

  lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(btn1, LV_ALIGN_BOTTOM_LEFT, 6, -6);
  lv_obj_add_event_cb(btn1, button_click_cb, LV_EVENT_CLICKED, (void *)0);

  lv_obj_t * label = lv_label_create(btn1);
  lv_label_set_text(label, "Changer cible");
  lv_obj_center(label);

  lv_obj_t * btn2 = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btn2, button_click_cb, LV_EVENT_ALL, (void *) 2);
  lv_obj_align(btn2, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
  lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_height(btn2, LV_SIZE_CONTENT);

  label = lv_label_create(btn2);
  lv_label_set_text(label, "Traque");
  lv_obj_center(label);

  lv_obj_t * btn3 = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn3, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(btn3, LV_ALIGN_TOP_RIGHT, -6, 6);
  lv_obj_add_event_cb(btn3, button_click_cb, LV_EVENT_CLICKED, (void *)1);

  label = lv_label_create(btn3);
  lv_label_set_text(label, "Centrer");
  lv_obj_center(label);
}

#else

#include "lvgl.h"
#include "app_hal.h"
#include <cstdio>

int main(void)
{
  printf("LVGL Simulator\n");
  fflush(stdout);

  lv_init();
  hal_setup();

  testLvgl();

  hal_loop();
  return 0;
}

#endif

// __________ Function that updates the style of the point to show when it's selected __________ //
static void update_target_selection_style(void)
{
  lv_obj_t * points[] = { targetPoint, targetPoint2, targetPoint3 };
  const lv_color_t colors[] = { lv_color_hex(0xFF0000), lv_color_hex(0x00FF00), lv_color_hex(0x00A5FF) };

  for (int i = 0; i < 3; ++i) {
    if (points[i] == nullptr) continue;

    bool is_selected = (selectedTarget == i);
    lv_obj_set_size(points[i], is_selected ? 14 : 10, is_selected ? 14 : 10);
    lv_obj_set_style_bg_color(points[i], colors[i], 0);
    lv_obj_set_style_border_color(points[i], is_selected ? lv_color_white() : lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(points[i], is_selected ? 2 : 0, 0);
  }
}

// __________ Call Back handling a click on a target __________ //
static void target_click_cb(lv_event_t * e)
{
  int index = (int)(intptr_t)lv_event_get_user_data(e); // Get the n° passed through the callback (0 to 2)
  selectedTarget = index; // Updates the global variable
}

// __________ Update target related lvgl objects __________ //
static void update_target_point(void)
{
  char labelText[50];
  // Update points on screen
  update_target_point_obj(targetPoint, 0);
  update_target_point_obj(targetPoint2, 1);
  update_target_point_obj(targetPoint3, 2);

  // Update style to show which target is selected
  update_target_selection_style();

  // Update label to show target coordinates
  if (targetLabel != nullptr && selectedTarget < 0) {
    lv_label_set_text_fmt(targetLabel, "T1:%d/%d  T2:%d/%d  T3:%d/%d",
                          ld2450.targets[0].x, ld2450.targets[0].y,
                          ld2450.targets[1].x, ld2450.targets[1].y,
                          ld2450.targets[2].x, ld2450.targets[2].y);
  } else {
    sprintf(labelText, "Cible selectionnee: T%d (%d°)", selectedTarget, (int)ld2450.targets[selectedTarget].angle);
    lv_label_set_text_fmt(targetLabel, labelText);
  }
}

// __________ Update a point representing the target on the screen __________ //
static void update_target_point_obj(lv_obj_t * obj, int index)
{
  if (obj == nullptr) return;

  int x = ld2450.targets[index].x;
  int y = ld2450.targets[index].y;

  int screen_w = lv_obj_get_width(lv_screen_active());
  int screen_h = lv_obj_get_height(lv_screen_active());

  int px = map_value(x, -LD2450_MAX_RANGE_MM/2, LD2450_MAX_RANGE_MM/2, 0, screen_w - 10);
  int py = map_value(y, 0, LD2450_MAX_RANGE_MM, screen_h - 10, 0);

  px = LV_CLAMP(px, 0, screen_w - 10);
  py = LV_CLAMP(py, 0, screen_h - 10);

  lv_obj_set_pos(obj, px, py);
}

// __________ Call Back handling button clicks __________ //
static void button_click_cb(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_CLICKED) {  // If the callback was triggered by a button click
    if((int)(intptr_t)lv_event_get_user_data(e) == 0) {         // Checks if button clicked is "change target"
      selectedTarget += 1;  // cycle target
      if(selectedTarget >=3) {
        selectedTarget = 0;
      } 
    } else if((int)(intptr_t)lv_event_get_user_data(e) == 1) {  // Checks if button clicked is "center"
      TIM3->CCR1 = deg_to_tics(140);  // center the sensor
    }
  } else if(code == LV_EVENT_VALUE_CHANGED) { // Checks if the callback was triggered by a change of the buttons value (toggle)
    trackingEnabled = !trackingEnabled; // toggles tracking
  }
}

// __________ Function that initiates the PWM __________ //
static void MX_TIM3_Init(void)
{
  RCC->AHB1ENR |= 1<<1;               // Enable GPIOB
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; // Enable TIM3
  
  GPIOB->MODER &= 1<<9;   // PB4 in alternate function
  GPIOB->AFR[0] |= 2<<16; // PB4 AF2 (TIM3_CH1)

  TIM3->CCMR1 |= 6<<4;          // PWM mode 1 on channel 1
  TIM3->CCER |= TIM_CCER_CC1E;  // Enable channel 1

  // Configurate PWM's frequency and initial duty cycle 
  TIM3->PSC = TIM3_PSC - 1;
  TIM3->ARR = TIM3_ARR - 1; 
  TIM3->CCR1 = TIM3_ARR / 2;
  Serial.println("TIM3 initialized with ARR: " + String(TIM3->ARR) + " and CCR1: " + String(TIM3->CCR1));

  TIM3->CR1 |= TIM_CR1_CEN; // Enable TIM3
}

// __________ Function that scales the points coordinates to accommodate the sensor's range and the screen's size __________ //
static int map_value(int value, int in_min, int in_max, int out_min, int out_max)
{
  if (in_max == in_min) return out_min;
  long scaled = (long)(value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  return (int)scaled;
}

// __________ Function calculating the duty cycle needed to reach a given angle __________ //
int deg_to_tics(float deg) {
  int counter = (deg / FT90M_RANGE_DEG) * (PWM_USABLE_TICS) + PWM_MIN_TICS;
  if(abs((int)(counter - TIM3->CCR1)) > FT90M_MINIMUM_STEP) // Checks if the period difference between the old value and the new one is greater than minimum imposed in datasheet
  {
    return counter;
  } else {
    return -1;  // If diference if inferior, return an error
  }
}

// __________ Function calculating the duty cycle needed to reach a given relative angle __________ //
int relative_deg_to_tics(float relativeDeg) {
  float currentDeg = ((TIM3->CCR1 - PWM_MIN_TICS) / (PWM_USABLE_TICS*1.0)) * FT90M_RANGE_DEG;
  float newDeg = currentDeg + relativeDeg;
  if (newDeg < 0) newDeg = 0;
  if (newDeg > FT90M_RANGE_DEG) newDeg = FT90M_RANGE_DEG;
  // Serial.println("Traveling relative by " + String(relativeDeg) + " degrees. Current: " + String(currentDeg) + " degrees, New: " + String(newDeg) + " degrees.");
  // Serial.println("CCR: " + TIM3->CCR1);
  return deg_to_tics(newDeg);
}
