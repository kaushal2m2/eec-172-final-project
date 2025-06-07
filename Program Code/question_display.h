//*****************************************************************************
//
// question_display.h - Question Type Display Interface
//
// Provides specialized display functions for different question types
// (pin labels, pin connections, component purpose) with appropriate
// formatting and visual styling.
//
//*****************************************************************************

#ifndef QUESTION_DISPLAY_H_
#define QUESTION_DISPLAY_H_

//*****************************************************************************
// Function Prototypes
//*****************************************************************************

//*****************************************************************************
//
//! Display pin labels question and answer
//!
//! Shows the question and answer for pin label queries with blue-themed
//! visual styling appropriate for pin labeling information.
//!
//! \param question - The original question string
//! \param answer - The answer string from AWS IoT
//!
//! \return None
//
//*****************************************************************************
void QuestionDisplay_ShowPinLabels(const char* question, const char* answer);

//*****************************************************************************
//
//! Display pin connections question and answer
//!
//! Shows the question and answer for pin connection queries with yellow-themed
//! visual styling appropriate for connection information.
//!
//! \param question - The original question string
//! \param answer - The answer string from AWS IoT
//!
//! \return None
//
//*****************************************************************************
void QuestionDisplay_ShowPinConnect(const char* question, const char* answer);

//*****************************************************************************
//
//! Display component purpose question and answer
//!
//! Shows the question and answer for component purpose queries with magenta-themed
//! visual styling appropriate for component functionality information.
//!
//! \param question - The original question string
//! \param answer - The answer string from AWS IoT
//!
//! \return None
//
//*****************************************************************************
void QuestionDisplay_ShowCompPurpose(const char* question, const char* answer);

#endif /* QUESTION_DISPLAY_H_ */
