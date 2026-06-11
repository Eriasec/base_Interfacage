
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

    Target() : x(0), y(0), speed(0), resolution(0) {}

    void ProcessTargetData(char data[8]) {
      x = data[0];
      x |= ((data[1] & 0x7F) << 8);
      if(data[1] & 0x80) { // Check if the sign bit is set
        x = -x; // Convert to negative value if needed
      }
      y = data[2];
      y |= ((data[3] & 0x7F) << 8);
      if(data[3] & 0x80) { // Check if the sign bit is set
        y = -y; // Convert to negative value if needed
      }
      speed = data[4];
      speed |= ((data[5] & 0x7F) << 8);
      if(data[5] & 0x80) { // Check if the sign bit is set
        speed = -speed; // Convert to negative value if needed
      }
      resolution = (data[7] << 8) | data[6];
      Serial.print(data[0], HEX);
      Serial.print(" ");
      Serial.print(data[1], HEX);
      Serial.println("");
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

lv_obj_t * text;
Target target1;
LD2450 ld2450;


static void event_handler(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if(code == LV_EVENT_CLICKED) {
      LV_LOG_USER("Clicked");
  }
  else if(code == LV_EVENT_VALUE_CHANGED) {
        LV_LOG_USER("Toggled");
  }
}

void testLvgl()
{
    /*Create a buffer for the canvas*/
    LV_DRAW_BUF_DEFINE_STATIC(draw_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_ARGB8888);
    LV_DRAW_BUF_INIT_STATIC(draw_buf);

    /*Create a canvas and initialize its palette*/
    lv_obj_t * canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(canvas, &draw_buf);
    lv_obj_center(canvas);

    /*Red background (There is no dedicated alpha channel in indexed images so LV_OPA_COVER is ignored)*/
    lv_canvas_fill_bg(canvas, lv_palette_main(LV_PALETTE_BLUE), LV_OPA_COVER);

    /*Create hole on the canvas*/
    int32_t x;
    int32_t y;
    for(y = 0; y < 0; y++) {
        for(x = 0; x < CANVAS_WIDTH; x++) {
            lv_canvas_set_px(canvas, x, y, lv_palette_main(LV_PALETTE_BLUE), LV_OPA_50);
        }
    }

    for(y = 20; y < 30; y++) {
        for(x = 5; x < 75; x++) {
            lv_canvas_set_px(canvas, x, y, lv_palette_main(LV_PALETTE_BLUE), LV_OPA_20);
        }
    }

    for(y = 30; y < 40; y++) {
        for(x = 5; x < 75; x++) {
            lv_canvas_set_px(canvas, x, y, lv_palette_main(LV_PALETTE_BLUE), LV_OPA_0);
        }
    }
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
    ld2450.read();
    Serial.print("X: ");
    Serial.print(ld2450.targets[0].x);
    Serial.print(" Y: ");
    Serial.print(ld2450.targets[0].y);
    Serial.print(" Speed: ");
    Serial.print(ld2450.targets[0].speed);
    Serial.print(" Resolution: ");
    Serial.println(ld2450.targets[0].resolution);
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