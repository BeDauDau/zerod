#include "send_task.h"

#include <Arduino.h>
#include <string>
#include <vector>

#include "send_cmd.h"

static std::vector<std::string> _queue;
static bool _stop = false;
static SemaphoreHandle_t _semaphore = nullptr;

namespace printer {
namespace send {

namespace Commands {

const char *kRestart = "RESTART";

}

void send_task(void *param) {
  _semaphore = xSemaphoreCreateMutex();
  while (true) {
    xSemaphoreTake(_semaphore, portMAX_DELAY);
    if (_stop) {
      Serial.print("STOP\r\n");
      _stop = false;
      _queue.clear();
    }
    if (!_queue.empty()) {
      for (const std::string &cmd : _queue) {
        Serial.print(cmd.c_str());
        Serial.print('\r');
        Serial.print('\n');
      }
      _queue.clear();
    }
    xSemaphoreGive(_semaphore);
    delay(5);
  }
}

void send_stop() {
  xSemaphoreTake(_semaphore, portMAX_DELAY);
  _stop = true;
  xSemaphoreGive(_semaphore);
}

void send_gcode(const char *gcode) {
  xSemaphoreTake(_semaphore, portMAX_DELAY);
  _queue.push_back(std::string("GCODE ") + gcode);
  xSemaphoreGive(_semaphore);
}

void send_move(const char *dir) {
  xSemaphoreTake(_semaphore, portMAX_DELAY);
  _queue.push_back(std::string("MOVE ") + dir);
  xSemaphoreGive(_semaphore);
}

void send_cmd(const char *cmd) {
  xSemaphoreTake(_semaphore, portMAX_DELAY);
  _queue.push_back(cmd);
  xSemaphoreGive(_semaphore);
}

}
}