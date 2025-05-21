---
title: TI-OS
---

# Project Proposal and Bill of Materials

## 1. Description (2 points)
Overview: TI-OS is a multi-functional program and physical interface that is meant to deliver a rudimentary simulation of what a personal computer based on the CC3200 would look like. A top level "desktop" menu will allow the user to select between several applications that demonstrate the different functionalities of the microcontroller. These applications will be focused on the use of TI-OS as an electronics assistant, such as an oscilliscope, function generator, and servo controlled "helping hand" for soldering. However, there will also be some demos of the real time graphical capabilities of the system, with a platformer video game and wireframe 3D physics simulation. The IoT implementation of the device will be an application that helps the user identify the pinout, function, and purpose of any named electronic component through the use of interfacing with the OpenAI GPT 3.5 Turbo API. 

Physical Interface: The user will interact with TI-OS via an analog stick to control a cursor, and two buttons to select and exit programs. In terms of output, the CC3200 will drive an OLED screen to display the graphical interface, and use a buzzer to provide basic sound feedback to the user. The "helping hand" will be articulated by two servo motors and controlled using the analog stick.

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
<img src="flowchart.png" alt="Flowchart" width="800" height="350">
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
| Pan-Tilt Servo Kit         | 1   | $13          | Alibaba (already owned)|
| Analog Joysticks           | 2   | $4           | Amazon                 |
| OLED Display (I2C)         | 1   | $8           | Amazon                 |
| Wires & Prototyping Tools  | —   | $5           | Lab / Personal Stock   |
| Breadboard                 | 1   | $5           | Already owned          |
| GPT API Key (OpenAI)       | —   | Free–$5      | OpenAI                 |

**Estimated Total Cost:** ~$32–$35 (within $50 budget)

---

This project blends hardware control with intelligent interaction, creating a tool that is both functional and informative. It enhances the typical “helping hands” setup with joystick control, servo precision, and GPT-powered assistance — making it ideal for students and hobbyists working with electronics.
