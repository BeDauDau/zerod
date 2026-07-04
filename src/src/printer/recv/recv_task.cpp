#include "recv_task.h"
#include "recv_state.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "printer/printer.h"

namespace printer {
namespace recv {

static const int _LINE_MAX = 384;
static char _line[_LINE_MAX];
static int _pos = 0;

static State _state;
static SemaphoreHandle_t _semaphore = nullptr;

// Parse a pipe-delimited status line.
// Format: S:2|W:1|P:0|H:1|HT:210|HTT:220|B:60|BT:65|C:0|CT:0|PR:45|TR:0|G:...
// Keys checked longest-first to avoid prefix collisions (HTT before HT, etc.)
static void _parse_line(const char *line);

void recv_task(void *param) {
  _state.status = Status::kDisconnected;
  _semaphore = xSemaphoreCreateMutex();

  while (true) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        _line[_pos] = '\0';
        if (_pos > 0) {
          _parse_line(_line);
        }
        _pos = 0;
      } else if (_pos < _LINE_MAX - 1) {
        if (c != '\r') {
          _line[_pos++] = c;
        }
      }
    }
    delay(5);
  }
}

void _parse_line(const char *line) {
  write([&](State *state) {
    const char *p = line;
    while (p && *p) {
      const char *colon = strchr(p, ':');
      if (!colon) break;

      int key_len = colon - p;

      // key_len excludes the colon; check longest keys first to avoid prefix collision
      if      (key_len == 3 && strncmp(p, "HTT", 3) == 0) state->hotend_target  = atoi(colon + 1);
      else if (key_len == 2 && strncmp(p, "HT", 2) == 0)  state->hotend_temp     = atoi(colon + 1);
      else if (key_len == 2 && strncmp(p, "BT", 2) == 0)  state->bed_target      = atoi(colon + 1);
      else if (key_len == 2 && strncmp(p, "CT", 2) == 0)  state->chamber_target  = atoi(colon + 1);
      else if (key_len == 2 && strncmp(p, "PR", 2) == 0)  state->progress        = atoi(colon + 1);
      else if (key_len == 2 && strncmp(p, "TR", 2) == 0)  state->tram_type       = static_cast<TramType>(atoi(colon + 1));
      else if (key_len == 1 && *p == 'S')                 state->status          = static_cast<Status>(atoi(colon + 1));
      else if (key_len == 1 && *p == 'W')                 state->working         = atoi(colon + 1) != 0;
      else if (key_len == 1 && *p == 'P')                 state->paused          = atoi(colon + 1) != 0;
      else if (key_len == 1 && *p == 'H')                { int h = atoi(colon + 1) != 0; state->homed_x = h; state->homed_y = h; state->homed_z = h; }
      else if (key_len == 1 && *p == 'B')                 state->bed_temp        = atoi(colon + 1);
      else if (key_len == 1 && *p == 'C')                 state->chamber_temp    = atoi(colon + 1);
      else if (key_len == 1 && *p == 'G') {
        strncpy(state->gcodes, colon + 1, sizeof(state->gcodes) - 1);
        state->gcodes[sizeof(state->gcodes) - 1] = '\0';
      }

      p = strchr(colon + 1, '|');
      if (p) p++;
    }
  });
}

void try_read(std::function<void(const State&)> cb) {
  if (!_semaphore) {
    return;
  }
  if (xSemaphoreTake(_semaphore, 0) == pdTRUE) {
    cb(_state);
    xSemaphoreGive(_semaphore);
  }
}

void write(std::function<void(State*)> cb) {
  xSemaphoreTake(_semaphore, portMAX_DELAY);
  cb(&_state);
  xSemaphoreGive(_semaphore);
}

}
}