"""Klipper extra module for ZeroD.

Text-based pipe-delimited protocol matching the ESP32-C3 firmware.
Serial I/O runs on a background thread to avoid blocking the reactor.
"""

import dataclasses
import enum
import queue
import threading

import serial


_BAUD_RATE = 115200

_RECV_PERIOD = 0.05
_SEND_PERIOD = 0.1

_GCODES_MAX_LEN = 255


class PrinterStatus(enum.Enum):
  DISCONNECTED = 0x00
  IDLE = 0x01
  PRINTING = 0x02
  SHUTDOWN = 0x03


class PrinterTramType(enum.Enum):
  NONE = 0x00
  ZTA = 0x01
  QGL = 0x02


@dataclasses.dataclass(frozen=True)
class PrinterState:
  status: PrinterStatus = PrinterStatus.DISCONNECTED

  working: bool = False
  paused: bool = False
  homed_x: bool = False
  homed_y: bool = False
  homed_z: bool = False

  hotend_temp: float = 0
  hotend_target: float = 0

  bed_temp: float = 0
  bed_target: float = 0

  chamber_temp: float = 0
  chamber_target: float = 0

  progress: float = 0

  tram_type: PrinterTramType = PrinterTramType.NONE

  gcodes: str = ""


class ZeroD:

  def __init__(self, config):
    self.printer = config.get_printer()
    self.reactor = self.printer.get_reactor()
    self.name = config.get_name()

    self.gcode = None
    self.heaters = None
    self.toolhead = None
    self.virtual_sdcard = None
    self.print_stats = None

    self.serial = None
    self.update_timer = None
    self.read_timer = None
    self.last_busy = 0
    self.pending_line = ""

    # Background write thread (keeps serial writes off the reactor thread)
    self._write_queue = queue.Queue(maxsize=16)
    self._write_thread = None
    self._write_stop = threading.Event()
    self._write_error = False

    self.tram_type = PrinterTramType.NONE
    self.gcodes = ""

    self.config_serial = config.get("serial")

    self.config_hotend = config.get("heater_hotend", "extruder")
    self.config_bed = config.get("heater_bed", "heater_bed")

    self.config_heater_chamber = config.get("heater_chamber", "")
    self.config_sensor_chamber = config.get("sensor_chamber", "")
    if self.config_heater_chamber and self.config_sensor_chamber:
      raise config.error(
          "Only one of heater_chamber and sensor_chamber can be specified",
      )

    self.config_move = [
        config.getfloat("move_x", 10.0),
        config.getfloat("move_y", 10.0),
        config.getfloat("move_z", 10.0),
    ]
    self.config_speed = [
        config.getfloat("speed_x", 100.0),
        config.getfloat("speed_y", 100.0),
        config.getfloat("speed_z", 100.0),
    ]

    self.config_gcodes = config.get("gcodes", "")
    gcodes = [g.strip() for g in self.config_gcodes.split(",")]
    gcodes = [g for g in gcodes if g]
    self.gcodes = "\n".join(gcodes)
    if len(self.gcodes) > _GCODES_MAX_LEN:
      raise config.error(
          f"Too many G-codes specified ({len(self.gcodes)} > "
          f"{_GCODES_MAX_LEN})",
      )

    self.printer.register_event_handler("klippy:connect", self._handle_connect)
    self.printer.register_event_handler("klippy:ready", self._handle_ready)
    self.printer.register_event_handler("klippy:shutdown", self._handle_shutdown)
    self.printer.register_event_handler("klippy:disconnect", self._handle_disconnect)

  def _handle_connect(self):
    self.serial = serial.Serial(self.config_serial, _BAUD_RATE, timeout=0)
    self._write_error = False
    self._write_stop.clear()
    self._write_thread = threading.Thread(target=self._write_worker, daemon=True)
    self._write_thread.start()

  def _write_worker(self):
    """Background thread: drains the write queue and sends to serial."""
    while not self._write_stop.is_set():
      try:
        line = self._write_queue.get(timeout=0.25)
      except queue.Empty:
        continue
      if line is None:  # sentinel → shutdown
        break
      if not self.serial or not self.serial.is_open:
        continue
      try:
        self.serial.write(line.encode("utf-8"))
      except serial.SerialException:
        self._write_error = True
        break

  def _handle_ready(self):
    self.gcode = self.printer.lookup_object("gcode")
    self.heaters = self.printer.lookup_object("heaters")
    self.toolhead = self.printer.lookup_object("toolhead")
    self.virtual_sdcard = self.printer.lookup_object("virtual_sdcard")
    self.print_stats = self.printer.lookup_object("print_stats")

    if self.printer.lookup_object("z_tilt", None):
      self.tram_type = PrinterTramType.ZTA
    elif self.printer.lookup_object("quad_gantry_level", None):
      self.tram_type = PrinterTramType.QGL
    else:
      self.tram_type = PrinterTramType.NONE

    self.update_timer = self.reactor.register_timer(
        self._handle_update, self.reactor.NOW,
    )
    self.read_timer = self.reactor.register_timer(
        self._handle_read, self.reactor.NOW,
    )

  def _handle_shutdown(self):
    self._send_shutdown_state()

  def _handle_disconnect(self):
    self._send_state(PrinterState(status=PrinterStatus.DISCONNECTED))
    self._write_stop.set()
    try:
      self._write_queue.put_nowait(None)
    except queue.Full:
      pass
    if self._write_thread:
      self._write_thread.join(timeout=2)
    if self.serial:
      self.serial.close()

  def _handle_update(self, eventtime):
    self._send_update(eventtime)
    return eventtime + _SEND_PERIOD

  def _handle_read(self, eventtime):
    if not self.serial or not self.serial.is_open:
      return eventtime + _RECV_PERIOD

    # Check if the write thread encountered an error
    if self._write_error:
      self.printer.invoke_shutdown(f"Lost connection with {self.name}")
      self.serial = None
      return eventtime + _RECV_PERIOD

    while True:
      data = self.serial.read(256)
      if not data:
        break
      for byte in data:
        if byte == 0x0A:  # \n
          line = self.pending_line.strip()
          if line:
            self._process_cmd(line)
          self.pending_line = ""
        elif byte != 0x0D:  # strip \r
          self.pending_line += chr(byte)

    return eventtime + _RECV_PERIOD

  def _send_update(self, eventtime):
    if self.printer.is_shutdown():
      self._send_shutdown_state()
      return

    status = PrinterStatus.IDLE
    paused = False
    if self.print_stats.state == "printing":
      status = PrinterStatus.PRINTING
    if self.print_stats.state == "paused":
      status = PrinterStatus.PRINTING
      paused = True

    _, _, lookahead_empty = self.toolhead.check_busy(eventtime)
    if not lookahead_empty:
      self.last_busy = eventtime
    working = eventtime - self.last_busy < 1

    toolhead_status = self.toolhead.get_status(eventtime)
    homed_x = "x" in toolhead_status["homed_axes"]
    homed_y = "y" in toolhead_status["homed_axes"]
    homed_z = "z" in toolhead_status["homed_axes"]

    hotend = self.heaters.lookup_heater(self.config_hotend)
    hotend_temp, hotend_target = hotend.get_temp(eventtime)

    bed = self.heaters.lookup_heater(self.config_bed)
    bed_temp, bed_target = bed.get_temp(eventtime)

    chamber_temp = 0
    chamber_target = 0
    if self.config_heater_chamber:
      chamber_heater = self.heaters.lookup_heater(self.config_heater_chamber)
      chamber_temp, chamber_target = chamber_heater.get_temp(eventtime)
    if self.config_sensor_chamber:
      chamber_sensor = self.printer.lookup_object(
          f"temperature_sensor {self.config_sensor_chamber}",
      )
      chamber_temp, _ = chamber_sensor.get_temp(eventtime)

    # Workaround for Danger Klipper.
    if hasattr(self.virtual_sdcard, "progress"):
      provider = self.virtual_sdcard
    else:
      provider = self.virtual_sdcard.get_virtual_sdcard_gcode_provider()
    progress = provider.progress() * 100

    state = PrinterState(
        status=status,
        paused=paused,
        working=working,
        homed_x=homed_x,
        homed_y=homed_y,
        homed_z=homed_z,
        hotend_temp=hotend_temp,
        hotend_target=hotend_target,
        bed_temp=bed_temp,
        bed_target=bed_target,
        chamber_temp=chamber_temp,
        chamber_target=chamber_target,
        progress=progress,
        tram_type=self.tram_type,
        gcodes=self.gcodes,
    )
    self._send_state(state)

  def _send_shutdown_state(self):
    message, _ = self.printer.get_state_message()
    message = message[:_GCODES_MAX_LEN]
    message = message.split("\n")[0]
    message = message.upper()
    self._send_state(PrinterState(
        status=PrinterStatus.SHUTDOWN,
        gcodes=message,
    ))

  def _send_state(self, state):
    if not self.serial or not self.serial.is_open:
      return

    homed = 1 if (state.homed_x or state.homed_y or state.homed_z) else 0

    # Pipe-delimited status line matching recv_task.cpp parser
    # Format: S:2|W:1|P:0|H:1|HT:210|HTT:220|B:60|BT:65|C:0|CT:0|PR:45|TR:0|G:...
    parts = [
        f"S:{state.status.value}",
        f"W:{1 if state.working else 0}",
        f"P:{1 if state.paused else 0}",
        f"H:{homed}",
        f"HT:{int(state.hotend_temp)}",
        f"HTT:{int(state.hotend_target)}",
        f"B:{int(state.bed_temp)}",
        f"BT:{int(state.bed_target)}",
        f"C:{int(state.chamber_temp)}",
        f"CT:{int(state.chamber_target)}",
        f"PR:{int(state.progress)}",
        f"TR:{state.tram_type.value}",
    ]
    if state.gcodes:
      parts.append(f"G:{state.gcodes}")

    line = "|".join(parts) + "\n"

    # Push to background thread — never blocks the reactor
    try:
      self._write_queue.put_nowait(line)
    except queue.Full:
      pass  # drop update if backlogged

  def _process_cmd(self, line):
    """Process a command line received from the ESP32-C3 firmware.

    Firmware send_task.cpp sends:
      GCODE <gcode>
      MOVE <dir>      (e.g. X+, X-, Y+, Y-, Z+, Z-)
      STOP
      RESTART
    """
    try:
      if line == "STOP":
        self.printer.invoke_shutdown(f"Stop requested by {self.name}")
        return
      if line == "RESTART":
        self.gcode.request_restart("firmware_restart")
        return
      if line.startswith("GCODE "):
        gcode = line[6:]
        self.gcode.run_script(gcode)
        return
      if line.startswith("MOVE "):
        move = line[5:]
        try:
          axis_char = move[0].upper()
          direction_char = move[1]
          axis = {"X": 0, "Y": 1, "Z": 2}[axis_char]
          direction = 1 if direction_char == "+" else -1
          pos = self.toolhead.get_position()
          pos[axis] += self.config_move[axis] * direction
          self.toolhead.manual_move(pos, self.config_speed[axis])
        except (IndexError, KeyError):
          pass
    except Exception:
      pass


def load_config(config):
  return ZeroD(config)
