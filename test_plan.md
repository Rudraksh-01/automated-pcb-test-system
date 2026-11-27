# Test Plan

Device under test (DUT): *describe your DUT here, e.g. a 5 V / 3.3 V regulator board with a small MCU.*

| # | Test | Method | Pass limit | Fail code |
|---|------|--------|-----------|-----------|
| 1 | Pre-power leakage check | Relay off. Read 5 V and 3.3 V rails through the analog mux. | Each rail < 0.30 V | E01 |
| 2 | Short-circuit check at power-up | Relay on. Read supply current with INA219 within 50 ms. | Current <= 300 mA | E02 |
| 3 | 5 V rail | Average 16 ADC samples after a 300 ms settle time. | 5.00 V +/- 5 % | E03 |
| 4 | 3.3 V rail | Same as above. | 3.30 V +/- 5 % | E04 |
| 5 | Current consumption | INA219 reading after rails are stable. | 5 mA to 300 mA | E02 / E05 |
| 6 | Functional test | Send `PING` over UART, expect `PONG` within 1 s. | Reply received | E06 |

All limits are defined as constants at the top of `firmware/pcb_tester/pcb_tester.ino`. Change them to match your DUT.

## Calibration

1. Measure each rail with a calibrated multimeter and compare with the station's reading.
2. Adjust `DIV_5V` / `DIV_3V3` until the readings agree.
3. Record the error in the README results table.

## Fault injection (to measure detection rate)

Prepare a set of deliberately faulty boards and record whether the station catches each one.

| Fault injected | Expected code | Detected? |
|----------------|---------------|-----------|
| Remove regulator | E03 / E04 | |
| Short a supply rail to GND | E02 | |
| Remove MCU | E05 / E06 | |
| Wrong resistor value on a divider | E03 / E04 | |
