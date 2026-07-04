#ifndef SEND_CMD_H
#define SEND_CMD_H

namespace printer {
namespace send {

namespace Commands {

extern const char *kRestart;

}

void send_stop();
void send_gcode(const char *gcode);
void send_move(const char *dir);
void send_cmd(const char *cmd);

}
}

#endif