/*
 * AWSIoTWiFiTest.c
 *
 *  Created on: May 21, 2025
 *      Author: lfiel
 */

//*****************************************************************************
//
// AWSIoTWiFiTest.c - WiFi Connection Test with AWS IoT Shadow Interface
//
// Tests WiFi connectivity and provides AWS IoT device shadow functionality.
// Designed to work with existing menu system initialization.
// Modified to send questions via enter key in text entry interface.
// Enhanced with question type-specific UI display.
//
//*****************************************************************************
#include "shared_defs.h"
#include <stdio.h>
#include <string.h>
#include "simplelink.h"
#include "hw_types.h"
#include "hw_ints.h"
#include <stdlib.h>
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"
#include "uart.h"
#include "pinmux.h"
#include "gpio_if.h"
#include "common.h"
#include "uart_if.h"
#include "utils/network_utils.h"
#include "gpio.h"
#include "wifiloading_bitmap.h"
#include "question_display.h"
#include "loading_screen_bitmap.h"
#include "connected_bitmap.h"

// custom text entry
#include "text_entry.h"

// AWS IoT Configuration
#define DATE                2    /* Current Date */
#define MONTH               6     /* Month 1-12 */
#define YEAR                2025  /* Current year */
#define HOUR                7     /* Time - hours */
#define MINUTE              54    /* Time - minutes */
#define SECOND              0     /* Time - seconds */

#define SERVER_NAME         "a15jh17gg8blx2-ats.iot.us-east-1.amazonaws.com" // CHANGE ME
#define AWS_IOT_PORT        8443

// Certificate paths
#define SL_SSL_CA_CERT      "/cert/rootCA.der"
#define SL_SSL_PRIVATE      "/cert/private.der"
#define SL_SSL_CLIENT       "/cert/client.der"

// HTTP Headers for AWS IoT
#define POSTHEADER          "POST /things/LoganField_CC3200Board/shadow HTTP/1.1\r\n"
#define GETHEADER           "GET /things/LoganField_CC3200Board/shadow HTTP/1.1\r\n"
#define HOSTHEADER          "Host: a15jh17gg8blx2-ats.iot.us-east-1.amazonaws.com\r\n" // CHANGE ME
#define CHEADER             "Connection: Keep-Alive\r\n"
#define CTHEADER            "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1           "Content-Length: "
#define CLHEADER2           "\r\n\r\n"

// Button pins for exit detection
#define BUTTON1_PIN         0x40    // PIN_15
#define BUTTON1_PORT        GPIOA2_BASE
#define BUTTON2_PIN         0x20    // PIN_21
#define BUTTON2_PORT        GPIOA1_BASE

// Display settings
#define TEXT_COLOR          WHITE
#define STATUS_COLOR        GREEN
#define ERROR_COLOR         RED

// Question type UI colors
#define PIN_LABELS_COLOR    BLUE
#define PIN_CONNECT_COLOR   YELLOW
#define COMP_PURPOSE_COLOR  MAGENTA

#define GET_REQUEST_DELAY 60000000


// Question type enumeration
typedef enum {
    QUESTION_TYPE_NONE = 0,
    QUESTION_TYPE_PIN_LABELS,
    QUESTION_TYPE_PIN_CONNECT,
    QUESTION_TYPE_COMP_PURPOSE
} question_type_t;

// Global variables
static bool g_initialized = false;
static bool g_connection_error = false;
static bool g_first_answer_frame = true;
static char g_error_message[64] = "";
static bool g_wifi_connected = false;
static bool g_aws_connected = false;
static char g_current_question[256] = "ft232h";
static char g_current_answer[512] = "No answer yet...";
static bool g_in_text_entry = false;
static question_type_t g_current_question_type = QUESTION_TYPE_NONE;

// Function prototypes
static void display_status(void);
static int SimplifiedWiFiConnect(void);
static int set_time(void);
static int tls_connect_aws(void);
static int http_post_question(int iTLSSockID, const char* question);
static int http_get_answer(int iTLSSockID);
static void parse_answer_from_response(char* response);
static void on_enter_pressed(const char* question);
static question_type_t get_question_type(const char* question);
static void display_pin_labels_ui(void);
static void display_pin_connect_ui(void);
static void display_comp_purpose_ui(void);
static void display_default_ui(void);
static void display_parse_fail_ui(void);

//*****************************************************************************
// Parse question type from question string
//*****************************************************************************
static question_type_t get_question_type(const char* question) {
    if (!question) {
        return QUESTION_TYPE_NONE;
    }

    int question_len = strlen(question);

    if (question_len >= 11 && strncmp(question, "pin labels/", 11) == 0) {
        return QUESTION_TYPE_PIN_LABELS;
    } else if (question_len >= 12 && strncmp(question, "pin connect/", 12) == 0) {
        return QUESTION_TYPE_PIN_CONNECT;
    } else if (question_len >= 13 && strncmp(question, "comp purpose/", 13) == 0) {
        return QUESTION_TYPE_COMP_PURPOSE;
    }

    return QUESTION_TYPE_NONE;
}

//*****************************************************************************
// Display UI for a parse fail
//*****************************************************************************
static void display_parse_fail_ui(void) {
    Outstrpretty("Sorry! The API was unable to produce a correctly formatted answer to your question", RED, BLACK, 0, 0, 128, 128);
    Outstrpretty("There are either too many pins or it is unfamiliar with the device.", RED, BLACK, 0, 65, 128, 128);
}

//*****************************************************************************
// Display UI for pin labels questions
//*****************************************************************************
static void display_pin_labels_ui(void) {
    QuestionDisplay_ShowPinLabels(g_current_question, g_current_answer);
}

//*****************************************************************************
// Display UI for pin connect questions
//*****************************************************************************
static void display_pin_connect_ui(void) {
    QuestionDisplay_ShowPinConnect(g_current_question, g_current_answer);
}

//*****************************************************************************
// Display UI for component purpose questions
//*****************************************************************************
static void display_comp_purpose_ui(void) {
    QuestionDisplay_ShowCompPurpose(g_current_question, g_current_answer);
}

//*****************************************************************************
// Display default UI for no question type or general info
//*****************************************************************************
static void display_default_ui(void) {
    fastFillScreen(BLACK);
    Outstrpretty("You have exited the text entry screen without a proper query, or an error has occured.", RED, BLACK, 10, 11, 120, 40);
    Outstrpretty("Press button 1 and 2 to exit. Press button 2 to return to the text entry screen.", RED, BLACK, 10, 75, 120, 128);

}

//*****************************************************************************
// Display UI for DNS lookup failure
//*****************************************************************************
static void display_dns_error_ui(void) {
    Outstrpretty("DNS Lookup Failed!", RED, BLACK, 10, 11, 120, 40);
    Outstrpretty("Cannot resolve AWS IoT server address. Check your internet connection.", RED, BLACK, 10, 40, 120, 70);
    Outstrpretty("Press button 1 & 2 to restart the program and try again.", RED, BLACK, 10, 80, 120, 100);
}

//*****************************************************************************
// Display UI for Undefined AWS connect failure
//*****************************************************************************

static void display_aws_error_ui(void) {
    Outstrpretty("Unable to connect to AWS!", RED, BLACK, 10, 11, 120, 40);
    Outstrpretty("Check your internet connection.", RED, BLACK, 10, 40, 120, 70);
    Outstrpretty("Press button 1 & 2 to restart the program and try again.", RED, BLACK, 10, 80, 120, 100);
}

static void display_loading_screen(void){
    int totalFrames = 12;
    int i=0;
    for(i=0; i<= totalFrames; i++){
        MAP_UtilsDelay(GET_REQUEST_DELAY/totalFrames);
        const uint8_t* loading_frame_bitmap = get_loading_screen_frame(i%LOADING_SCREEN_FRAME_COUNT);
        fastDrawBitmap(0, 0, loading_frame_bitmap, 128, 128, GREEN, BLACK, 1);
        // Draw loading bar filled rectangle
        fillRect(8, 69, (115/totalFrames)*(i), 6, BLUE);
    }
}

//*****************************************************************************
// Callback function for when enter is pressed in text entry
//*****************************************************************************
static void on_enter_pressed(const char* question) {
    UART_PRINT("=== ENTER KEY PRESSED ===\n\r");
    UART_PRINT("Question from text entry: %s\n\r", question);

    //Clear the persistent toggle rectangles from the text entry
    Clear_toggle();

    // display first frame of loading bitmap before anything is done so it feels responsive
    // this is a very hacky way to do this, :(
    const uint8_t* loading_frame_bitmap = get_loading_screen_frame(0);
    fastDrawBitmap(0, 0, loading_frame_bitmap, 128, 128, GREEN, BLACK, 1);

    // Update our current question
    strncpy(g_current_question, question, sizeof(g_current_question) - 1);
    g_current_question[sizeof(g_current_question) - 1] = '\0';

    // Only proceed if WiFi is connected
    if (!g_wifi_connected) {
        UART_PRINT("Cannot send question - WiFi not connected\n\r");
        strcpy(g_current_answer, "WiFi not connected");
        //TextEntry_RequestExit();
        return;
    }

    // Send the question to AWS IoT
    UART_PRINT("Sending question to AWS IoT: %s\n\r", question);
    strcpy(g_current_answer, "Sending to AWS...");

    int socketId = tls_connect_aws();
    if(socketId >= 0) {
        g_aws_connected = true;

        // Send the question
        int post_result = http_post_question(socketId, g_current_question);
        if (post_result == 0) {
            UART_PRINT("Question sent successfully, waiting for response...\n\r");
            strcpy(g_current_answer, "wait");

            while(strcmp(g_current_answer, "wait") == 0){
                // Wait a moment for processing, use display_loading_screen to do this
                display_loading_screen();

                // Get the answer
                int get_result = http_get_answer(socketId);
                if (get_result == 0) {
                    UART_PRINT("Answer retrieved successfully\n\r");
                } else {
                    UART_PRINT("Failed to retrieve answer\n\r");
                    strcpy(g_current_answer, "Failed to get answer");
                }
            }
            Report(g_current_answer);
        } else {
            UART_PRINT("Failed to send question\n\r");
            strcpy(g_current_answer, "Failed to send question");
        }

        sl_Close(socketId);
        UART_PRINT("AWS transaction complete\n\r");
    } else {
        g_aws_connected = false;
        strcpy(g_current_answer, "aws_connect_failed");
        UART_PRINT("Failed to connect to AWS IoT: %d\n\r", socketId);
    }

    UART_PRINT("=== ENTER PROCESSING COMPLETE ===\n\r");

    // Request exit from text entry to show results
    TextEntry_RequestExit();
}

//*****************************************************************************
// Set device time for TLS
//*****************************************************************************
static int set_time(void) {
    long retVal;

    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = SECOND;
    g_time.tm_hour = HOUR;
    g_time.tm_min = MINUTE;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                       SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                       sizeof(SlDateTime), (unsigned char *)(&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}

//*****************************************************************************
// TLS connection to AWS IoT
//*****************************************************************************
static int tls_connect_aws(void) {
    SlSockAddrIn_t    Addr;
    int    iAddrSize;
    unsigned char    ucMethod = SL_SO_SEC_METHOD_TLSV1_2;
    unsigned int uiIP;
    unsigned int uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256;
    long lRetVal = -1;
    int iSockID;

    // Get host IP
    lRetVal = sl_NetAppDnsGetHostByName(SERVER_NAME, strlen(SERVER_NAME),
                                       &uiIP, SL_AF_INET);
    if(lRetVal < 0) {
        UART_PRINT("DNS lookup failed: %d\n\r", lRetVal);
        // Check for specific DNS error code -161
        if(lRetVal == -161) {
            // Set specific error message for DNS failure
            strcpy(g_current_answer, "dns_lookup_failed");
        }
        return lRetVal;
    }

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(AWS_IOT_PORT);
    Addr.sin_addr.s_addr = sl_Htonl(uiIP);
    iAddrSize = sizeof(SlSockAddrIn_t);

    // Create secure socket
    iSockID = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, SL_SEC_SOCKET);
    if(iSockID < 0) {
        UART_PRINT("Socket creation failed: %d\n\r", iSockID);
        return iSockID;
    }

    // Configure TLS method
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECMETHOD,
                           &ucMethod, sizeof(ucMethod));
    if(lRetVal < 0) {
        sl_Close(iSockID);
        return lRetVal;
    }

    // Configure cipher
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECURE_MASK,
                            &uiCipher, sizeof(uiCipher));
    if(lRetVal < 0) {
        sl_Close(iSockID);
        return lRetVal;
    }

    // Set CA certificate
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
                           SL_SO_SECURE_FILES_CA_FILE_NAME,
                           SL_SSL_CA_CERT, strlen(SL_SSL_CA_CERT));
    if(lRetVal < 0) {
        sl_Close(iSockID);
        return lRetVal;
    }

    // Set client certificate
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
                           SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME,
                           SL_SSL_CLIENT, strlen(SL_SSL_CLIENT));
    if(lRetVal < 0) {
        sl_Close(iSockID);
        return lRetVal;
    }

    // Set private key
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
                           SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME,
                           SL_SSL_PRIVATE, strlen(SL_SSL_PRIVATE));
    if(lRetVal < 0) {
        sl_Close(iSockID);
        return lRetVal;
    }

    // Connect to AWS IoT
    lRetVal = sl_Connect(iSockID, (SlSockAddr_t *)&Addr, iAddrSize);
    if(lRetVal < 0) {
        sl_Close(iSockID);
        UART_PRINT("TLS connection failed: %d\n\r", lRetVal);
        return lRetVal;
    }

    UART_PRINT("Connected to AWS IoT successfully\n\r");
    return iSockID;
}

//*****************************************************************************
// HTTP POST to send question to device shadow
//*****************************************************************************
static int http_post_question(int iTLSSockID, const char* question) {
    char acSendBuff[1024];
    char acRecvbuff[1460];
    char cCLLength[200];
    char* pcBufHeaders;
    int lRetVal = 0;

    // Create JSON payload
    char jsonPayload[512];
    sprintf(jsonPayload, "{\n"
            "\"state\": {\r\n"
            "\"reported\" : {\r\n"
            "\"Question\" :\"%s\"\r\n"
            "}"
            "}"
            "}\r\n\r\n", question);

    int dataLength = strlen(jsonPayload);

    // Build HTTP POST request
    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, POSTHEADER);
    pcBufHeaders += strlen(POSTHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, CTHEADER);
    pcBufHeaders += strlen(CTHEADER);
    strcpy(pcBufHeaders, CLHEADER1);
    pcBufHeaders += strlen(CLHEADER1);

    sprintf(cCLLength, "%d", dataLength);
    strcpy(pcBufHeaders, cCLLength);
    pcBufHeaders += strlen(cCLLength);
    strcpy(pcBufHeaders, CLHEADER2);
    pcBufHeaders += strlen(CLHEADER2);

    strcpy(pcBufHeaders, jsonPayload);
    pcBufHeaders += strlen(jsonPayload);

    UART_PRINT("Sending question: %s\n\r", question);

    // Send the request
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed: %d\n\r", lRetVal);
        return lRetVal;
    }

    // Receive response
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Receive failed: %d\n\r", lRetVal);
        return lRetVal;
    }

    acRecvbuff[lRetVal] = '\0';
    UART_PRINT("POST response received\n\r");

    return 0;
}

//*****************************************************************************
// HTTP GET to retrieve answer from device shadow
//*****************************************************************************
static int http_get_answer(int iTLSSockID) {
    char acSendBuff[512];
    char acRecvbuff[2048];
    int lRetVal = 0;

    // Form the GET request
    strcpy(acSendBuff, GETHEADER);
    strcat(acSendBuff, HOSTHEADER);
    strcat(acSendBuff, CHEADER);
    strcat(acSendBuff, "\r\n\r\n");

    UART_PRINT("Sending GET request for answer...\n\r");

    // Send the GET request
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("GET failed: %d\n\r", lRetVal);
        return lRetVal;
    }

    // Receive response
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Receive failed: %d\n\r", lRetVal);
        return lRetVal;
    }

    acRecvbuff[lRetVal] = '\0';
    UART_PRINT("GET response received\n\r");

    // Parse the answer from the response
    parse_answer_from_response(acRecvbuff);

    return 0;
}

//*****************************************************************************
// Parse answer from HTTP response and determine question type
//*****************************************************************************
static void parse_answer_from_response(char* response) {
    // Look for the "Question" field in the REPORTED state
    char* reportedStart = strstr(response, "\"reported\"");
    if (reportedStart) {
        // Look for Question field within the reported section
        char* questionStart = strstr(reportedStart, "\"Question\"");
        if (questionStart) {
            // Move to the beginning of the question value
            questionStart = strstr(questionStart, ":");
            if (questionStart) {
                questionStart++; // Skip the colon

                // Skip any whitespace after colon
                while (*questionStart == ' ' || *questionStart == '\t' || *questionStart == '\r' || *questionStart == '\n') {
                    questionStart++;
                }

                // Find the next quote that starts the string
                questionStart = strchr(questionStart, '\"');
                if (questionStart) {
                    questionStart++; // Skip the opening quote

                    // Find the ending quote
                    char* questionEnd = strchr(questionStart, '\"');
                    if (questionEnd) {
                        // Calculate length and extract the question
                        int questionLength = questionEnd - questionStart;

                        if (questionLength > 0 && questionLength < sizeof(g_current_question)) {
                            char temp_question[256];
                            memset(temp_question, 0, sizeof(temp_question));
                            strncpy(temp_question, questionStart, questionLength);
                            temp_question[questionLength] = '\0';

                            // Parse the question type from the extracted question
                            g_current_question_type = get_question_type(temp_question);

                            // Copy the extracted question to global variable for display
                            strncpy(g_current_question, temp_question, sizeof(g_current_question) - 1);
                            g_current_question[sizeof(g_current_question) - 1] = '\0';
                        }
                    }
                }
            }
        }
    }

    // Now look for the "Answer" field in the DESIRED state
    char* desiredStart = strstr(response, "\"desired\"");
    if (desiredStart) {
        // Look for Answer field within the desired section
        char* answerStart = strstr(desiredStart, "\"Answer\"");
        if (answerStart) {
            // Move to the beginning of the answer value
            answerStart = strstr(answerStart, ":");
            if (answerStart) {
                answerStart++; // Skip the colon

                // Skip any whitespace after colon
                while (*answerStart == ' ' || *answerStart == '\t' || *answerStart == '\r' || *answerStart == '\n') {
                    answerStart++;
                }

                // Find the next quote that starts the string
                answerStart = strchr(answerStart, '\"');
                if (answerStart) {
                    answerStart++; // Skip the opening quote

                    // Find the ending quote
                    char* answerEnd = strchr(answerStart, '\"');
                    if (answerEnd) {
                        // Calculate length and copy the answer
                        int answerLength = answerEnd - answerStart;

                        if (answerLength > 0 && answerLength < sizeof(g_current_answer)) {
                            // Clear existing buffer and copy new answer
                            memset(g_current_answer, 0, sizeof(g_current_answer));
                            strncpy(g_current_answer, answerStart, answerLength);
                            g_current_answer[answerLength] = '\0';

                            UART_PRINT("Answer retrieved: %s\n\r", g_current_answer);
                            return;
                        }
                    }
                }
            }
        }
    }

    UART_PRINT("Could not find answer in response\n\r");
    strcpy(g_current_answer, "No answer found");
}

//*****************************************************************************
// Simplified WiFi connection that bypasses heavy SimpleLink reconfiguration
//*****************************************************************************
static int SimplifiedWiFiConnect(void) {
    long lRetVal = -1;
    SlSecParams_t secParams = {0};

    UART_PRINT("Starting simplified WiFi connection...\n\r");

    // Initialize status variables
    g_ulStatus = 0;
    g_ulGatewayIP = 0;
    g_Host = g_app_config.host;
    g_port = g_app_config.port;
    memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));

    // Set up security parameters
    secParams.Key = SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;

    // Attempt to connect to WiFi
    UART_PRINT("Attempting connection to: %s\n\r", SSID_NAME);
    lRetVal = sl_WlanConnect(SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);
    if (lRetVal < 0) {
        UART_PRINT("WiFi connection failed: %d\n\r", lRetVal);
        return lRetVal;
    }

    UART_PRINT("WiFi connection initiated, waiting for events...\n\r");

    // Wait for connection with timeout
    int timeout_count = 0;
    int max_timeout = 30; // 30 iterations

    while ((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus))) {
        // Process SimpleLink events
        _SlNonOsMainLoopTask();

        // Small delay
        MAP_UtilsDelay(800000);

        // Print status periodically
        if (timeout_count % 10 == 0) {
            UART_PRINT("Status - Connected: %s, IP: %s\n\r",
                       IS_CONNECTED(g_ulStatus) ? "YES" : "NO",
                       IS_IP_ACQUIRED(g_ulStatus) ? "YES" : "NO");
        }

        timeout_count++;
        if (timeout_count >= max_timeout) {
            UART_PRINT("Timeout waiting for connection/IP\n\r");
            break;
        }
    }

    // Check final status
    if (IS_CONNECTED(g_ulStatus)) {
        UART_PRINT("WiFi connected successfully!\n\r");
        if (IS_IP_ACQUIRED(g_ulStatus)) {
            UART_PRINT("IP address acquired!\n\r");
        } else {
            UART_PRINT("Connected but no IP address\n\r");
        }
        return 0; // Success
    } else {
        UART_PRINT("WiFi connection failed\n\r");
        return -1;
    }
}

//*****************************************************************************
// Function to display the status on the OLED screen with question type UI
//*****************************************************************************
static void display_status(void) {
    if(g_first_answer_frame){
        g_first_answer_frame = false;
        if(strcmp(g_current_answer, "parse fail") == 0){
            fastFillScreen(BLACK);
            display_parse_fail_ui();
        }else if(strcmp(g_current_answer, "dns_lookup_failed") == 0){
            fastFillScreen(BLACK);
            display_dns_error_ui();
        }else if(strcmp(g_current_answer, "aws_connect_failed") == 0){
            fastFillScreen(BLACK);
            display_aws_error_ui();
        }else{
            switch (g_current_question_type) {
                case QUESTION_TYPE_PIN_LABELS:
                    display_pin_labels_ui();
                    break;
                case QUESTION_TYPE_PIN_CONNECT:
                    display_pin_connect_ui();
                    break;
                case QUESTION_TYPE_COMP_PURPOSE:
                    display_comp_purpose_ui();
                    break;
                case QUESTION_TYPE_NONE:
                default:
                    display_default_ui();
                    break;
            }
        }
    }
}

//*****************************************************************************
// Initialize AWS IoT Test
//*****************************************************************************
void AWSIoT_Initialize(void) {
    long lRetVal = -1;

    // Reset state variables
    g_initialized = false;
    g_connection_error = false;
    g_error_message[0] = '\0';
    g_wifi_connected = false;
    g_aws_connected = false;
    g_current_question_type = QUESTION_TYPE_NONE;

    // Print debug info
    UART_PRINT("\n\rStarting AWS IoT Test...\n\r");

    // Initialize g_app_config for network utils
    g_app_config.host = SERVER_NAME;
    g_app_config.port = AWS_IOT_PORT;

    UART_PRINT("g_app_config initialized\n\r");

    // Connect to WiFi
    int wifiFrame = 0;
    lRetVal = -1;

    // While trying to connect display wifi loading bitmap
    while(lRetVal < 0){
        const uint8_t* wifiloading_frame_bitmap = get_wifiloading_frame(wifiFrame%WIFILOADING_FRAME_COUNT);
        fastDrawBitmap(0, 0, wifiloading_frame_bitmap, 128, 128, GREEN, BLACK, 1);
        wifiFrame++;
        lRetVal = SimplifiedWiFiConnect();
        //force exit if button 2 is pressed
        if(!(GPIOPinRead(BUTTON2_PORT, BUTTON2_PIN) == 0)){
            return;
        }
    }

    // Successfully connected to WiFi
    g_wifi_connected = true;
    g_connection_error = false;

    UART_PRINT("Connected to Wi-Fi!\n\r");

    const uint8_t* connected_bitmap = get_connected_frame(0);
    fastDrawBitmap(0, 0, connected_bitmap, 128, 128, GREEN, BLACK, 1);

    // Set time for TLS
    lRetVal = set_time();
    if(lRetVal < 0) {
        UART_PRINT("Failed to set time: %d\n\r", lRetVal);
        strcpy(g_error_message, "Time setting failed");
    } else {
        UART_PRINT("Time set successfully\n\r");
    }

    // Turn on green LED to indicate success
    GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);

    // Wait a moment before switching to the main display
    MAP_UtilsDelay(1600000);

    // Mark as initialized
    g_initialized = true;

    // START IN TEXT ENTRY MODE with callback
    if (g_wifi_connected) {
        UART_PRINT("Starting in text entry mode with enter callback...\n\r");
        g_in_text_entry = true;
        TextEntry_Initialize(g_current_question, on_enter_pressed);
    }
}

//*****************************************************************************
// Run one frame of the AWS IoT Test
//*****************************************************************************
bool AWSIoT_RunFrame(void) {
    if (!g_initialized) {
        return false;
    }

    // If we're in text entry mode, handle that instead
    if (g_in_text_entry) {
        if (!TextEntry_RunFrame()) {
            // Text entry completed, get the result and update our question
            const char* new_question = TextEntry_GetCurrentText();
            strncpy(g_current_question, new_question, sizeof(g_current_question) - 1);
            g_current_question[sizeof(g_current_question) - 1] = '\0';

            TextEntry_Cleanup();
            g_in_text_entry = false;
            display_status(); // Refresh the main display

            UART_PRINT("Text entry completed. New question: %s\n\r", g_current_question);
        }
        return true; // Stay in AWS IoT mode
    }

    // Check button states
    bool button1Pressed = !(GPIOPinRead(BUTTON1_PORT, BUTTON1_PIN) == 0);
    bool button2Pressed = !(GPIOPinRead(BUTTON2_PORT, BUTTON2_PIN) == 0);

    // Handle button 2 - enter text entry mode
    static bool button2_was_pressed = false;
    if (button2Pressed && !button2_was_pressed) {
        g_first_answer_frame = true;
        UART_PRINT("Button 2 pressed - entering text entry mode\n\r");
        g_in_text_entry = true;
        TextEntry_Initialize(g_current_question, on_enter_pressed);
    }
    button2_was_pressed = button2Pressed;

    // Toggle green LED if connected to indicate activity
    if (g_wifi_connected) {
        static bool toggle_led = false;
        static int toggle_counter = 0;

        // Toggle LED every few frames for better visibility
        if (toggle_counter++ > 10) {
            toggle_counter = 0;
            if (toggle_led) {
                GPIO_IF_LedOff(MCU_GREEN_LED_GPIO);
            } else {
                GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);
            }
            toggle_led = !toggle_led;
        }
    }

    // Update the display periodically
    static int refresh_counter = 0;
    if (refresh_counter++ % 100 == 0) {
        display_status();
    }

    return true;
}

//*****************************************************************************
// Clean up AWS IoT Test resources
//*****************************************************************************
void AWSIoT_Cleanup(void) {
    // Turn off LEDs
    GPIO_IF_LedOff(MCU_RED_LED_GPIO);
    GPIO_IF_LedOff(MCU_GREEN_LED_GPIO);
    GPIO_IF_LedOff(MCU_ORANGE_LED_GPIO);

    // Mark as not initialized
    g_initialized = false;
    g_aws_connected = false;

    UART_PRINT("AWS IoT closed\n\r");
}
