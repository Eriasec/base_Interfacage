
#include "lvgl.h"
#include "Arduino.h"

#define LD2450_HEADER 0xAAFF0300

HardwareSerial Serial6(PC7, PC6);

lv_obj_t * text;
char inputBuffer[50] = {0}; // Buffer pour stocker les données reçues
String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete


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
  // Initialisations générales
  lv_obj_t * label;

  lv_obj_t * btn1 = lv_button_create(lv_screen_active());
  lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);
  lv_obj_remove_flag(btn1, LV_OBJ_FLAG_PRESS_LOCK);

  label = lv_label_create(btn1);
  lv_label_set_text(label, "Button");
  lv_obj_center(label);

  lv_obj_t * btn2 = lv_button_create(lv_screen_active());
  lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
  lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_height(btn2, LV_SIZE_CONTENT);

  label = lv_label_create(btn2);
  lv_label_set_text(label, "Toggle");
  lv_obj_center(label);

  text = lv_label_create(lv_screen_active());
  lv_label_set_text(text, "Hello, World!");
  lv_obj_align(text, LV_ALIGN_CENTER, 0, 0);
}

#ifdef ARDUINO

#include "lvglDrivers.h"

// à décommenter pour tester la démo
// #include "demos/lv_demos.h"

void mySetup()
{
  Serial.begin(921600);
  Serial.println("Initializing...");

  Serial6.begin(256000);
  Serial6.setTimeout(10000); // Set a timeout for Serial6 reads
  Serial.println("Serial6 initialized at 256000 baud");

  // reserve 200 bytes for the inputString:
  inputString.reserve(200);

  // à décommenter pour tester la démo
  // lv_demo_widgets();

  // Initialisations générales
  testLvgl();
}

void loop()
{
  while (Serial6.available()) {
    Serial6.readBytes(inputBuffer, 30); // Read 30 bytes 
    if(inputBuffer[0] == (0xAA) &&
       inputBuffer[1] == (0xFF) &&
       inputBuffer[2] == (0x03) &&
       inputBuffer[3] == (0x00))
    {
    } else {
      Serial.println("Received unknown data:");
      while(1); // Stop processing if the header is incorrect
    }
    for(int i = 0; i < 30; i++) {
      Serial.print(inputBuffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
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