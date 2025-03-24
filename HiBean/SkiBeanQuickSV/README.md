# QuickPID Commands - HiBean ESP32 BLE Roaster Control

This document lists the available commands for controlling the roaster using QuickPID in the HiBean ESP32 BLE Roaster Control firmware.

## **PID Commands & Behavior**
The **QuickPID** library is used to regulate heating power based on the measured temperature. It operates in **two modes**:
- **Automatic (PID ON):** Uses PID logic to adjust the heater output.
- **Manual (PID OFF):** Allows manual control of heater power.

## **Available Commands (case-INsensitive)**
| **Command**     | **Description** |
|-----------------|----------------|
| `PID;ON`        | Enables PID control (automatic mode). |
| `PID;OFF`       | Disables PID control (switches to manual mode). |
| `PID;SV;XXX`    | Sets the PID **setpoint temperature** (XXX is in °C, e.g., `PID;SV;250` sets the target to 250°C). |
| `PID;KP;XX.X`   | Temporarily apply the proportional tuning of the PID control. |
| `PID;KI;XX.X`   | Temporarily apply the integral tuning of the PID control. |
| `PID;KD;XX.X`   | Temporarily apply the derivative tuning of the PID control. |
| `PID;ST;XXX`    | Temporarily sets PID sample time to XXXms. |
| `PID;PM;E`      | Temporarily change pMode: E = P_ON_E to M = P_ON_M(default), or reverse. |
| `OT1;XX`        | Manually sets heater power to **XX%** (only works in MANUAL mode). |
| `READ`          | Retrieves current temperature, set temperature, heater, and vent power. |

## **Other Control Commands**
| **Command**     | **Description** |
|-----------------|----------------|
| `OT2;XX`        | Sets the vent power to **XX%**. |
| `OFF`           | Shuts down the system. |
| `ESTOP`         | Emergency stop: Sets heater to 0% and vent to 100%. |
| `DRUM;XX`       | Starts/stops the drum motor (1 = ON, 0 = OFF). |
| `FILTER;XX`     | Controls filter fan power (1 fastest - 4 slowest; 0 off). |
| `COOL;XX`       | Activates cooling function (0-100%). |
| `CHAN`          | Sends active channel configuration. |
| `UNITS;C/F`     | Sets temperature units to **Celsius (C)** or **Fahrenheit (F)**. |

## **Usage Example**
- Enable PID control:
  ```
  PID;ON
  ```
- Set target temperature to 250°C:
  ```
  PID;SV;250
  ```
- Manually set heater to 70% power:
  ```
  OT1;70
  ```
- Read current system status:
  ```
  READ
  ```

This document serves as a quick reference for controlling the roaster over BLE using the HiBean ESP32 BLE Roaster Control firmware.

TODO
- BLE on Core1
- PID storage via Preference
- sTune Autotune Function
- RTD Max31865 Implementation
- TC Max31855 Implementation
- AutoDrop Servo
