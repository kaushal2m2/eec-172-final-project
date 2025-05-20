---
title: Project Proposal
---

# Project Proposal and Bill of Materials

## Description (2 points)
A description of the product you intend to prototype.

## Design
---
title: Project Proposal
---

# Project Proposal and Bill of Materials

## 1. Description (2 points)

We are designing a **servo-controlled electronics workbench assistant** that functions like a modern version of a "helping hands" tool used in electronics labs. Our system uses a pan-tilt servo mount to precisely position an articulated arm over a workspace. The arm can be controlled manually with analog sticks (using ADC), or automatically through programmable signals. It can hold wires, probes, or even deliver test signals to a breadboard. The system is intended to make circuit debugging and prototyping easier and more efficient by acting as a second pair of hands or a signal delivery tool.

---

## 2. Design

### Functional Specification (2 points)

The device has two main modes:

- **Manual Mode**: The user can control the position of the arm via two analog joysticks, adjusting the pan and tilt angle via PWM signals to the servo motors.
- **Automated Mode**: Predefined signal tasks (e.g., delivering a square wave, probing a certain part of the circuit) can be activated through switch inputs.

A state machine will manage the system states:

```
Idle --> Manual Control <--> Automated Task
        ↑             ↓
     Calibration <-- Error State
```

Each state handles specific behaviors for input, motor control, or signal output. The user can toggle between modes using buttons or DIP switches.

### System Architecture (2 points)

```
[Analog Joysticks]
       |
   [ADC - MSP430]
       |
   [State Machine Control Logic]
       |
  -----------------------------
  |                           |
[PWM → Servo Motors]     [Signal Output System (DAC or GPIO toggling)]
                           |
                [Optional Probe/Delivery Interface]
```

- **ADC** is used to read analog joystick positions.
- **PWM** outputs control the servo motors.
- A microcontroller (e.g., MSP430) runs the state machine and translates input into motion or output signals.
- Optional probing features could use GPIOs or DACs to generate or monitor signals.

---

## 3. Implementation Goals (2 points)

- **Minimal Goal**: Two-axis servo control using joystick input, stable arm control with basic "pan and tilt" functionality.
- **Target Goal**: Add automated signal delivery (e.g., square wave generator), toggle between manual and auto modes.
- **Stretch Goal**: Enable probing (e.g., using voltage sensing) and display live signal data on a simple screen or LEDs, or log via UART.

---

## 4. Bill of Materials (2 points)

| Component                 | Qty | Approx. Cost | Source               |
|--------------------------|-----|--------------|----------------------|
| Pan-Tilt Servo Kit       | 1   | $10          | Already owned        |
| Analog Joysticks         | 2   | $4           | Amazon               |
| MSP430 or Microcontroller| 1   | $0 (provided)| Lab Kit              |
| Wires & Prototyping Tools| —   | $5           | Lab / Personal Stock |
| Breadboard               | 1   | $5           | Already owned        |
| Optional: Signal Probe   | 1   | $10          | Digikey / Amazon     |
| Optional: LEDs or OLED   | 1   | $5–10        | Amazon               |

**Estimated Total Cost:** ~$30 (within $50 budget)

---

The proposal is 1 page long (excluding this BOM), and all implementation goals, block diagrams, and ideas are subject to refinement as development continues.





