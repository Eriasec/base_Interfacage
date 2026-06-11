
#include "lvgl.h"
#include "../../lib/lvgl/examples/lv_examples.h"
#include "Arduino.h"

#define CANVAS_WIDTH  80
#define CANVAS_HEIGHT  40

class Target {
  public:
    int16_t x;
    int16_t y;
    int16_t speed;
    int16_t resolution;
    uint16_t absoluteDistance;
    int8_t angle;

    Target() : x(0), y(0), speed(0), resolution(0), absoluteDistance(0), angle(0) {}

    void ProcessTargetData(char data[8]) {
      x = data[0];
      x |= ((data[1] & 0x7F) << 8);
      if(!(data[1] & 0x80)) { // Check if the sign bit is set
        x = -x; // Convert to negative value if needed
      }
      y = data[2];
      y |= ((data[3] & 0x7F) << 8);
      if(!(data[3] & 0x80)) { // Check if the sign bit is set
        y = -y; // Convert to negative value if needed
      }
      speed = data[4];
      speed |= ((data[5] & 0x7F) << 8);
      if(!(data[5] & 0x80)) { // Check if the sign bit is set
        speed = -speed; // Convert to negative value if needed
      }
      resolution = (data[7] << 8) | data[6];
      absoluteDistance = abs(x) + abs(y); // Calculate absolute distance as the sum of absolute x and y values
      angle = (atan2(y, x) * 180 / PI) - 90; // Calculate angle in degrees
    }
};

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
      if(this->inputBuffer[0] == (0xAA) &&
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

    String getLastTargetMessage() {
      // This function should return a string representation of the last target message
      // For demonstration purposes, we will return an empty string here
      return "";
    }
};

HardwareSerial Serial6(PC7, PC6);

lv_obj_t * targetPoint = nullptr;
lv_obj_t * targetPoint2 = nullptr;
lv_obj_t * targetPoint3 = nullptr;
lv_obj_t * targetLabel = nullptr;
int selectedTarget = -1;
Target target1;
LD2450 ld2450;

#define LD2450_MAX_RANGE_MM 8000

static void update_target_selection_style(void);

static int map_value(int value, int in_min, int in_max, int out_min, int out_max)
{
  if (in_max == in_min) return out_min;
  long scaled = (long)(value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  return (int)scaled;
}

static void update_target_point_obj(lv_obj_t * obj, int index)
{
  if (obj == nullptr) return;

  int x = ld2450.targets[index].x;
  int y = ld2450.targets[index].y;

  int screen_w = lv_obj_get_width(lv_screen_active());
  int screen_h = lv_obj_get_height(lv_screen_active());

  int px = map_value(x, -LD2450_MAX_RANGE_MM, LD2450_MAX_RANGE_MM, 0, screen_w - 10);
  int py = map_value(y, 0, LD2450_MAX_RANGE_MM, screen_h - 10, 0);

  px = LV_CLAMP(px, 0, screen_w - 10);
  py = LV_CLAMP(py, 0, screen_h - 10);

  lv_obj_set_pos(obj, px, py);
}

static void update_target_point(void)
{
  update_target_point_obj(targetPoint, 0);
  update_target_point_obj(targetPoint2, 1);
  update_target_point_obj(targetPoint3, 2);

  update_target_selection_style();

  if (targetLabel != nullptr && selectedTarget < 0) {
    lv_label_set_text_fmt(targetLabel, "T1:%d/%d  T2:%d/%d  T3:%d/%d",
                          ld2450.targets[0].x, ld2450.targets[0].y,
                          ld2450.targets[1].x, ld2450.targets[1].y,
                          ld2450.targets[2].x, ld2450.targets[2].y);
  }
}


static void target_click_cb(lv_event_t * e)
{
  int index = (int)(intptr_t)lv_event_get_user_data(e);
  selectedTarget = index;

  if (targetLabel != nullptr) {
    lv_label_set_text_fmt(targetLabel, "Cible selectionnee: T%d (X:%d Y:%d)",
                          index + 1,
                          ld2450.targets[index].x,
                          ld2450.targets[index].y);
  }
}

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

void testLvgl()
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
  lv_obj_set_size(targetLabel, lv_pct(95), LV_SIZE_CONTENT);
  lv_obj_set_style_text_color(targetLabel, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(targetLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_bg_opa(targetLabel, LV_OPA_90, 0);
  lv_obj_set_style_bg_color(targetLabel, lv_color_hex(0x000000), 0);
  lv_obj_set_style_pad_all(targetLabel, 8, 0);
  lv_obj_set_style_radius(targetLabel, 6, 0);
  lv_obj_align(targetLabel, LV_ALIGN_TOP_LEFT, 6, 6);
  lv_label_set_text(targetLabel, "Attente des mesures...");
}

#ifdef ARDUINO

#include "lvglDrivers.h"

// à décommenter pour tester la démo
// #include "demos/lv_demos.h"

void mySetup()
{
  Serial.begin(921600);
  Serial.println("Initializing...");

  ld2450.begin(Serial6);
  Serial.println("Serial6 initialized at 256000 baud");

  // à décommenter pour tester la démo
  // lv_demo_widgets();

  // Initialisations générales
  testLvgl();
}

void loop()
{
  while (Serial6.available()) {
    if (ld2450.read()) {
      if (lvglLock(pdMS_TO_TICKS(10))) {
        update_target_point();
        lvglUnlock();
      }
      // Serial.print("X: ");
      // Serial.print(ld2450.targets[0].x);
      // Serial.print(" Y: ");
      // Serial.print(ld2450.targets[0].y);
      // Serial.print(" Speed: ");
      // Serial.print(ld2450.targets[0].speed);
      // Serial.print(" Resolution: ");
      // Serial.print(ld2450.targets[0].resolution);
      // Serial.print(" Absolute Distance: ");
      // Serial.print(ld2450.targets[0].absoluteDistance);
      // Serial.print(" Angle: ");
      // Serial.println(ld2450.targets[0].angle);
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