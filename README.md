# Automated PCB Testing System

An automated test station that checks a PCB (the device under test, DUT) for shorts, correct supply voltages, current draw and basic functionality, then logs pass/fail results with a failure code for every board.

> Fill in the parts marked **TODO** with your own photos and measured results.

## Features

- Pre-power leakage check and short-circuit detection at power-up
- 5 V and 3.3 V rail voltage tests with +/- 5 % limits
- Current consumption test with an INA219 sensor
- Functional test over UART (`PING` / `PONG`)
- Pogo-pin bed-of-nails fixture so boards are tested without soldering
- PC logger (Python) that records serial number, timestamp, measurements and fail code, and reports yield

## System overview

```
 PC (Python logger) <--USB serial--> ESP32 (test sequencer)
                                        |-- analog mux --> DUT test points (via pogo pins)
                                        |-- relay -------> DUT power
                                        |-- INA219 ------> DUT supply current
                                        |-- UART --------> DUT (functional test)
                                        '-- LEDs + buzzer: pass / fail indication
```

**TODO:** add a block diagram and a photo of the fixture in `docs/`.

## Hardware

| Part | Purpose |
|------|---------|
| ESP32 DevKit | Test controller |
| 16-channel analog mux (CD74HC4067) | Reads many test points with one ADC pin |
| INA219 module | Measures DUT supply current |
| Relay module | Switches DUT power |
| Pogo pins + fixture plate | Connects to DUT test points |
| Voltage dividers | Scale rails into the ESP32 ADC range |
| Green / red LED, buzzer | Result indication |

## Test sequence

1. **Pre-check:** with power off, rails must read below 0.30 V (E01).
2. **Power-up:** relay on, current checked within 50 ms for a short (E02).
3. **Rails:** 5 V and 3.3 V within tolerance (E03, E04).
4. **Current:** within 5 to 300 mA (E02, E05).
5. **Functional:** UART reply from the DUT (E06).
6. **Result:** LED and buzzer, then `RESULT,...` line sent to the PC and logged to CSV.

Full limits and fail codes are in [docs/test_plan.md](docs/test_plan.md).

## Getting started

### 1. Firmware
1. Open `firmware/pcb_tester/pcb_tester.ino` in the Arduino IDE.
2. Install the ESP32 board package and the **Adafruit INA219** library.
3. Edit the pin map and limits at the top of the file to match your wiring.
4. Select your ESP32 board and upload.

### 2. PC software
```bash
pip install -r software/requirements.txt
python software/test_logger.py --port COM3
```
Use `/dev/ttyUSB0` on Linux. Enter or scan a serial number for each board; results are appended to `results.csv`.

## Results

**TODO:** replace with your measured values.

| Metric | Result |
|--------|--------|
| Voltage accuracy vs. calibrated multimeter | TODO |
| Test time per board | TODO |
| Boards tested | TODO |
| Injected faults detected | TODO / TODO |

## Repository structure

```
automated-pcb-test-system/
├── firmware/pcb_tester/pcb_tester.ino   ESP32 test sequencer
├── software/test_logger.py              PC logger and yield report
├── software/requirements.txt
├── docs/test_plan.md                    Test limits, calibration, fault injection
└── README.md
```

## Future work

- Boundary-scan (JTAG) testing
- Machine-vision solder-joint inspection with OpenCV
- Publish results to a dashboard (MQTT / Grafana)

## Author

Rudraksh Charde: [LinkedIn](https://www.linkedin.com/in/rudraksh-charde-9b150029b/) | [GitHub](https://github.com/Rudraksh-01)
