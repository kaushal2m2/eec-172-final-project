//*****************************************************************************
//
// question_display.c - Question Type Display Interface Implementation
//
// Provides specialized display functions for different question types
// with appropriate formatting and visual styling.
//
//*****************************************************************************

#include "question_display.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Include necessary headers for display functionality
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "shared_defs.h"
#include "componentpurpose_bitmap.h"

//*****************************************************************************
// Constants and Definitions
//*****************************************************************************

// Display settings
#define TEXT_COLOR          WHITE
#define BACKGROUND_COLOR    BLACK

// Question type UI colors (matching AWS IoT implementation)
#define PIN_LABELS_COLOR    BLUE
#define PIN_CONNECT_COLOR   YELLOW
#define COMP_PURPOSE_COLOR  MAGENTA

// Layout constants
#define QUESTION_START_Y    20
#define ANSWER_START_Y      50
#define RECT_X              10
#define RECT_Y              90
#define RECT_WIDTH          108
#define RECT_HEIGHT         25
#define RECT_TEXT_X         15
#define RECT_TEXT_Y         95

#define MAX_PINS 16
#define MAX_CONNECTIONS 9
#define MAX_LABEL_LENGTH 64

// Data struct for pin connection info

typedef struct {
    int pin1_num;
    char pin1_name[MAX_LABEL_LENGTH];
    int pin2_num;
    char pin2_name[MAX_LABEL_LENGTH];
} PinConnection;

int parsePinConnections(const char* answer, PinConnection connections[], int maxConnections)
{
    char workBuffer[512];
    char* ptr;
    char* connectionStart;
    char* connectionEnd;
    int connectionCount = 0;
    int i, j;
    char tempConnection[128];

    /* Initialize */
    if (answer == NULL || connections == NULL) {
        return -1;
    }

    /* Copy to work buffer since we might modify it */
    if (strlen(answer) >= sizeof(workBuffer)) {
        return -1;
    }
    strcpy(workBuffer, answer);

    /* Find opening bracket */
    ptr = strchr(workBuffer, '[');
    if (ptr == NULL) return -1;
    ptr++; /* Move past '[' */

    /* Parse each connection */
    while (*ptr && connectionCount < maxConnections) {
        /* Skip whitespace */
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (*ptr == '\0' || *ptr == ']') break;

        /* Find end of this connection (comma or closing bracket) */
        connectionStart = ptr;
        connectionEnd = ptr;
        while (*connectionEnd && *connectionEnd != ',' && *connectionEnd != ']') {
            connectionEnd++;
        }

        /* Copy this connection to temp buffer */
        {
            int len = connectionEnd - connectionStart;
            if (len >= sizeof(tempConnection)) len = sizeof(tempConnection) - 1;
            strncpy(tempConnection, connectionStart, len);
            tempConnection[len] = '\0';
        }

        /* Parse this connection: "pin X (name) + pin Y (name)" */
        if (sscanf(tempConnection, "pin %d (%63[^)]) + pin %d (%63[^)])",
                   &connections[connectionCount].pin1_num,
                   connections[connectionCount].pin1_name,
                   &connections[connectionCount].pin2_num,
                   connections[connectionCount].pin2_name) == 4) {
            connectionCount++;
        }

        /* Move to next connection */
        ptr = connectionEnd;
        if (*ptr == ',') ptr++;
    }

    /* Sort by pin1_num (simple bubble sort for C90 compatibility) */
    for (i = 0; i < connectionCount - 1; i++) {
        for (j = 0; j < connectionCount - i - 1; j++) {
            if (connections[j].pin1_num > connections[j+1].pin1_num) {
                PinConnection temp = connections[j];
                connections[j] = connections[j+1];
                connections[j+1] = temp;
            }
        }
    }

    return connectionCount;
}

/* Simple function to parse pin labels from answer string */
int parsePinLabels(const char* answer, int* numberOfPins, char labels[][MAX_LABEL_LENGTH])
{
    char workBuffer[512];
    char* ptr;
    char* labelStart;
    char* comma;
    int labelCount = 0;
    int i;

    /* Initialize */
    *numberOfPins = 0;

    /* Copy to work buffer since we might modify it */
    strcpy(workBuffer, answer);

    /* Extract number of pins using sscanf */
    if (sscanf(workBuffer, "[number of pins: %d]", numberOfPins) != 1) {
        return -1;
    }

    /* Find the start of labels section after first ']' */
    ptr = strchr(workBuffer, ']');
    if (ptr == NULL) return -1;
    ptr++;

    /* Find opening '[' of labels section */
    ptr = strchr(ptr, '[');
    if (ptr == NULL) return -1;
    ptr++; /* Move past '[' */

    /* Parse each label */
    while (*ptr && labelCount < *numberOfPins && labelCount < MAX_PINS) {
        /* Skip whitespace */
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        /* Skip pin number and colon (e.g., "1:", "2:") */
        while (*ptr && *ptr != ':') ptr++;
        if (*ptr != ':') break;
        ptr++; /* Move past ':' */

        /* Skip whitespace after colon */
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        /* Find label text until comma or ']' */
        labelStart = ptr;
        comma = ptr;
        while (*comma && *comma != ',' && *comma != ']') {
            comma++;
        }

        /* Copy label, removing trailing whitespace */
        i = 0;
        while (labelStart < comma && i < MAX_LABEL_LENGTH - 1) {
            if (*labelStart != ' ' && *labelStart != '\t') {
                labels[labelCount][i++] = *labelStart;
            } else if (i > 0 && labels[labelCount][i-1] != ' ') {
                labels[labelCount][i++] = ' ';
            }
            labelStart++;
        }

        /* Remove trailing space */
        if (i > 0 && labels[labelCount][i-1] == ' ') {
            i--;
        }
        labels[labelCount][i] = '\0';

        labelCount++;

        /* Move to next label */
        ptr = comma;
        if (*ptr == ',') ptr++; /* Skip comma */
        if (*ptr == ']') break; /* End of labels */
    }

    return labelCount;
}

//
void QuestionDisplay_ShowPinLabels(const char* question, const char* answer)
{
    int numberOfPins;
    char labels[MAX_PINS][MAX_LABEL_LENGTH];
    int numLabels;
    int i;
    char displayText[512] = "";
    char tempStr[64];
    char pinNumberDisplayText[32];
    int componentHeight = 20;
    int componentX, componentY = 20; /* Keep original y position */
    /* Variables for pin rectangles */
    int topPinsCount, bottomPinsCount;  /* Changed from pinsPerSide */
    int pinWidth = 9;
    int pinHeight = 9;
    int pinSpacing = 13;  /* Consistent spacing between pins */
    int componentMargin = 1; /* Extra margin on each side of component */
    int componentWidth;
    int startX;
    int maxBottomPinHeight = 36; /* Maximum height for bottom pins when >= 10 */
    /* Parse the answer string */
    numLabels = parsePinLabels(answer, &numberOfPins, labels);
    fastFillScreen(BLACK);
    if (numLabels <= 0) {
        /* Parsing failed, show original answer */
        Outstrpretty("Sorry! The CC3200 Script was unable to parse the answer.", RED, BLACK, 10, 10, 118, 128);
        return;
    }
    /* Calculate pin distribution - for odd numbers, put extra pin on bottom */
    topPinsCount = numLabels / 2;
    bottomPinsCount = numLabels - topPinsCount;  /* This handles odd numbers correctly */

    /* Calculate component width based on the side with more pins */
    int maxPinsOnOneSide = (topPinsCount > bottomPinsCount) ? topPinsCount : bottomPinsCount;
    if (maxPinsOnOneSide > 0) {
        /* Width = spacing intervals + margins */
        componentWidth = (maxPinsOnOneSide + 1) * pinSpacing + (2 * componentMargin);
    } else {
        /* Minimum width if no pins */
        componentWidth = 60;
    }
    /* Center horizontally on 128 pixel wide screen */
    componentX = (128 - componentWidth) / 2;
    /* Build simple display text */
    sprintf(pinNumberDisplayText, "Pins: %d", numberOfPins);
    int x_label = 7;
    int y_label = 63;
    for (i = 0; i < numLabels; i++) {
        sprintf(tempStr, "%d:%s", i + 1, labels[i]);
        Outstr(tempStr, GREEN, BLACK, x_label, y_label, x_label + 40, y_label + 16);
        y_label += 14;
        if(y_label >= 122){
            x_label += 40;
            y_label = 63;
        }
    }
    /* Draw main component rectangle */
    drawRect(componentX, componentY, componentWidth, componentHeight, GREEN);
    /* Draw pin rectangles */
    if (numLabels > 0) {
        startX = componentX + componentMargin + pinSpacing;

        /* Draw top pins */
        for (i = 0; i < topPinsCount; i++) {
            int topPinNumber = i + 1;
            int currentTopPinHeight = (topPinNumber >= 100) ? pinHeight * 2 : pinHeight;
            drawRect(startX + i * pinSpacing - pinWidth/2,
                     componentY - currentTopPinHeight,
                     pinWidth,
                     currentTopPinHeight,
                     GREEN);
            sprintf(tempStr, "%d", topPinNumber);
            Outstr(tempStr, GREEN, BLACK, startX + i * pinSpacing - pinWidth/2,
                   componentY - currentTopPinHeight,
                   pinWidth+6,
                   currentTopPinHeight);
        }

        /* Draw bottom pins */
        for (i = 0; i < bottomPinsCount; i++) {
            int bottomPinNumber = i + topPinsCount + 1;
            int currentBottomPinHeight;
            if (bottomPinNumber >= 10) {
                /* Pin number has more than 2 digits, make twice as tall */
                currentBottomPinHeight = pinHeight * 2;
            } else {
                currentBottomPinHeight = pinHeight;
            }
            drawRect(startX + i * pinSpacing - pinWidth/2,
                     componentY + componentHeight,
                     pinWidth,
                     currentBottomPinHeight,
                     GREEN);
            sprintf(tempStr, "%d", bottomPinNumber);
            Outstr(tempStr, GREEN, BLACK, startX + i * pinSpacing - pinWidth/2,
                   componentY + componentHeight,
                   pinWidth+6,
                   currentBottomPinHeight);
        }
    }
    /* Display the parsed content */
    Outstr(question, GREEN, BLACK, 45, 25, 128, 60);
}

//*****************************************************************************
// Display pin connections question and answer
//*****************************************************************************
void QuestionDisplay_ShowPinConnect(const char* question, const char* answer)
{
    PinConnection connections[MAX_CONNECTIONS];
    int numConnections;
    int i;
    char reportText[128];
    char pinText[32];
    /* Layout constants */
    int leftRectX = 10;
    int leftRectWidth = 25;
    int rightRectX = 93;
    int rightRectWidth = 25;
    int rectY = 20;
    int rectHeight = 95;
    int pinWidth = 12;
    int pinHeight = 12;
    int pinSpacing;
    int startY;
    /* Label positioning constants */
    int rightLabelStartX = 95;
    int rightLabelEndX = 128;  /* End just before left label */
    int rightLabelWidth = 30;  /* Reasonable width for right labels */
    int leftLabelStartX = 10;
    /* Component name variables */
    char component1Name[64];
    char component2Name[64];
    char* plusPos;
    /* Parse the connections */
    numConnections = parsePinConnections(answer, connections, MAX_CONNECTIONS);

    /* Extract component names from question string */
    /* Initialize component names */
    strcpy(component1Name, "Component 1");
    strcpy(component2Name, "Component 2");

    /* Find the "+" separator */
    plusPos = strchr(question, '+');
    if (plusPos != NULL) {
        int len1 = plusPos - question;
        int len2 = strlen(question) - len1 - 1;

        /* Extract first component name */
        if (len1 > 0 && len1 < 63) {
            strncpy(component1Name, question, len1);
            component1Name[len1] = '\0';
            /* Trim trailing whitespace */
            while (len1 > 0 && component1Name[len1-1] == ' ') {
                component1Name[--len1] = '\0';
            }
        }

        /* Extract second component name */
        if (len2 > 0 && len2 < 63) {
            strncpy(component2Name, plusPos + 1, len2);
            component2Name[len2] = '\0';
            /* Trim leading whitespace */
            char* start = component2Name;
            while (*start == ' ') start++;
            if (start != component2Name) {
                memmove(component2Name, start, strlen(start) + 1);
            }
        }
    }

    /* Clear screen */
    fastFillScreen(BLACK);
    if (numConnections <= 0) {
        /* Parsing failed, show original answer */
        OutstrBlack("Parse Error:");
        Outstr(answer, GREEN, BLACK, 7, 40, 121, 70);
        Report("Failed to parse connections");
        Report("Raw answer: %s", answer);
        return;
    }
    /* Calculate pin spacing based on number of connections */
    if (numConnections > 1) {
        pinSpacing = (rectHeight - pinHeight) / (numConnections - 1);
        if (pinSpacing < pinHeight + 2) {
            pinSpacing = pinHeight + 2; /* Minimum spacing */
        }
    } else {
        pinSpacing = 0;
    }
    startY = rectY + 5; /* Start pins a bit below top of rectangle */
    /* Draw left component rectangle */
    drawRect(leftRectX, rectY, leftRectWidth, rectHeight, GREEN);
    /* Draw right component rectangle */
    drawRect(rightRectX, rectY, rightRectWidth, rectHeight, GREEN);

    /* Display component names above rectangles */
    Outstr(component1Name, WHITE, BLACK,
           leftRectX, rectY - 15,
           leftRectX + leftRectWidth + 20, rectY - 3);

    Outstr(component2Name, WHITE, BLACK,
           rightRectX - 20, rectY - 15,
           rightRectX + rightRectWidth, rectY - 3);
    /* Draw pins and labels */
    for (i = 0; i < numConnections; i++) {
        int currentPinY = startY + (i * pinSpacing);
        int leftPinWidth, rightPinWidth;
        int leftPinX, rightPinX;

        /* Determine pin widths based on number of digits */
        leftPinWidth = (connections[i].pin1_num >= 10) ? (pinWidth * 2) : pinWidth;
        rightPinWidth = (connections[i].pin2_num >= 10) ? (pinWidth * 2) : pinWidth;

        /* Calculate pin positions */
        leftPinX = leftRectX + leftRectWidth;
        rightPinX = (connections[i].pin2_num >= 10) ? (rightRectX - (pinWidth * 2)) : (rightRectX - pinWidth);

        /* Draw left pin (component 1) */
        drawRect(leftPinX, currentPinY, leftPinWidth, pinHeight, GREEN);

        /* Draw right pin (component 2) */
        drawRect(rightPinX, currentPinY, rightPinWidth, pinHeight, GREEN);

        /* Display pin 1 number and name on left pin */
        sprintf(pinText, "%d", connections[i].pin1_num);
        Outstr(pinText, GREEN, BLACK,
               leftPinX + 1, currentPinY + 1,
               leftPinX + leftPinWidth - 1, currentPinY + 6);

        /* Display pin 1 name below the pin */
        Outstr(connections[i].pin1_name, YELLOW, BLACK,
               leftLabelStartX, currentPinY,
               leftLabelStartX + 36, currentPinY + 12);

        /* Display pin 2 number and name on right pin */
        sprintf(pinText, "%d", connections[i].pin2_num);
        Outstr(pinText, GREEN, BLACK,
               rightPinX + 1, currentPinY + 1,
               rightPinX + rightPinWidth - 1, currentPinY + 6);

        /* Display pin 2 name to the left of the pin - FIXED POSITIONING */
        Outstr(connections[i].pin2_name, YELLOW, BLACK,
               rightLabelStartX, currentPinY,
               rightLabelEndX, currentPinY + 12);

        /* Draw connection line between pins */
        drawLine(leftPinX + leftPinWidth, currentPinY + pinHeight/2,
                 rightPinX, currentPinY + pinHeight/2, BLUE);
    }
    /* Also send to Report for debugging/logging */
    Report("Question: %s", question);
    Report("Pin Connections:");
    for (i = 0; i < numConnections; i++) {
        sprintf(reportText, "pin %d \"%s\" is connected to pin %d \"%s\"",
                connections[i].pin1_num, connections[i].pin1_name,
                connections[i].pin2_num, connections[i].pin2_name);
        Report(reportText);
    }
}

//*****************************************************************************
// Display component purpose question and answer
//*****************************************************************************
void QuestionDisplay_ShowCompPurpose(const char* question, const char* answer)
{

    const uint8_t* componentpurpose_frame_bitmap = get_componentpurpose_frame(0);
    fastDrawBitmap(0, 0, componentpurpose_frame_bitmap, COMPONENTPURPOSE_WIDTH, COMPONENTPURPOSE_HEIGHT, GREEN, BLACK, 1);

    Outstrpretty(answer, GREEN, BLACK, 19,36,111,111);
}
