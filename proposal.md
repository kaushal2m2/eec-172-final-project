---
title: Project Proposal
---

# Project Proposal and Bill of Materials

## 1. Description (2 points)

We are building a **servo-controlled electronics workbench assistant**, designed to act as a versatile tool for prototyping and debugging. This robotic arm system uses a pan-tilt servo mechanism mounted on a stable base and can be manipulated using analog joysticks. It is capable of gripping or pointing at small components, simulating the functionality of a "helping hands" tool commonly used in electronics labs.

We are using the **TI CC3200 LaunchPad** as our main microcontroller. It handles PWM for servo control, ADC for reading joystick input, and will also connect to the **OpenAI GPT API over Wi-Fi**. An integrated OLED display will show mode information, arm status, and responses to questions. The GPT integration allows users to ask predefined or parameterized questions (e.g., “How do I reset the arm?” or “What does this button do?”) and receive helpful on-device answers.

---

## 2. Design

### Functional Specification (2 points)

The device will support three modes:

- **Manual Mode**: The user controls the servo arm’s pan and tilt via analog joysticks. ADC on the CC3200 interprets the input, and PWM signals control the servos.
- **Information Mode**: The user can trigger predefined GPT queries from physical buttons or a menu system, and responses are shown on the OLED screen.
- **Auto Mode (optional)**: The device performs preset behaviors, like centering itself or positioning at common angles.

A simple state machine governs transitions:

```
Idle --> Manual Control <--> Info Query Mode
        ↑             ↓
     Calibration <-- Error State
```

### System Architecture (2 points)

```
[Analog Joysticks]
       |
   [ADC - CC3200]
       |
   [Control Logic + State Machine]
       |
  ---------------------------------------------
  |                       |                   |
[PWM → Servo Motors]   [Wi-Fi → GPT API]   [OLED Display]
                            |
                  [Prefilled Query Selection]
```

- **ADC** reads joystick input.
- **PWM** drives pan/tilt servo motors.
- **Wi-Fi** is used to connect the CC3200 to the GPT API for sending queries and receiving answers.
- The **OLED display** shows GPT responses and system status.

---

## 3. Implementation Goals (2 points)

- **Minimal Goal**: Servo control with analog joystick input and OLED display showing position/mode.
- **Target Goal**: GPT integration with a few selectable prefilled questions and response output to the OLED.
- **Stretch Goal**: Add gripping mechanism or signal probe, enable contextual GPT queries (e.g., current servo position), or save response history.

---

## 4. Bill of Materials (2 points)

| Component                   | Qty | Approx. Cost | Source                 |
|----------------------------|-----|--------------|------------------------|
| TI CC3200 LaunchPad        | 1   | $0           | Provided in lab        |
| Pan-Tilt Servo Kit         | 1   | $10          | Already owned          |
| Analog Joysticks           | 2   | $4           | Amazon                 |
| OLED Display (I2C)         | 1   | $8           | Amazon                 |
| Wires & Prototyping Tools  | —   | $5           | Lab / Personal Stock   |
| Breadboard                 | 1   | $5           | Already owned          |
| GPT API Key (OpenAI)       | —   | Free–$5      | OpenAI                 |

**Estimated Total Cost:** ~$32–$35 (within $50 budget)

---

This project blends hardware control with intelligent interaction, creating a tool that is both functional and informative. It enhances the typical “helping hands” setup with joystick control, servo precision, and GPT-powered assistance — making it ideal for students and hobbyists working with electronics.
