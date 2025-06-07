#!/usr/bin/env python3
"""
AWS IoT Shadow REST API Client with ChatGPT Integration

Usage:
python iot_shadow_client.py --endpoint STUFF-ats.iot.us-east-1.amazonaws.com --thing-name LoganField_CC3200Board chatbot --api-key your_openai_api_key --verbose
"""
import json
import argparse
import boto3
from botocore.auth import SigV4Auth
from botocore.awsrequest import AWSRequest
import requests
import sys
import time
import os
import re

class IoTShadowClient:
    def __init__(self, endpoint, region="us-east-1"):
        """Initialize the IoT Shadow client with AWS endpoint and region."""
        self.endpoint = endpoint
        self.region = region
        self.service = "iotdata"
        
        # Get credentials from the default AWS profile
        try:
            session = boto3.Session()
            self.credentials = session.get_credentials()
            if self.credentials is None:
                sys.exit("Error: No AWS credentials found. Configure AWS CLI or set environment variables.")
        except Exception as e:
            sys.exit(f"Error initializing AWS session: {e}")
    
    def _send_request(self, method, thing_name, payload=None):
        """Send a signed request to the AWS IoT API."""
        url = f"https://{self.endpoint}/things/{thing_name}/shadow"
        
        # Create the request object
        request = AWSRequest(
            method=method,
            url=url,
            data=json.dumps(payload) if payload else None
        )
        
        # Add necessary headers
        request.headers.add_header("Content-Type", "application/json")
        
        # Sign the request with AWS SigV4
        SigV4Auth(self.credentials, self.service, self.region).add_auth(request)
        
        # Convert to a regular requests request and send
        prepared_request = request.prepare()
        
        try:
            response = requests.request(
                method=prepared_request.method,
                url=prepared_request.url,
                headers=prepared_request.headers,
                data=prepared_request.body
            )
            
            # Raise an exception for HTTP errors
            response.raise_for_status()
            
            # Return the response
            try:
                return response.json()
            except json.JSONDecodeError:
                return {"status": response.status_code, "text": response.text}
                
        except requests.exceptions.HTTPError as e:
            print(f"HTTP Error: {e}")
            try:
                error_details = response.json()
                print(f"Error details: {json.dumps(error_details, indent=2)}")
            except:
                print(f"Response text: {response.text}")
            raise
        except requests.exceptions.RequestException as e:
            print(f"Request Error: {e}")
            raise
    
    def get_shadow(self, thing_name):
        """Retrieve the current shadow document for a thing."""
        return self._send_request("GET", thing_name)
    
    def update_shadow(self, thing_name, state):
        """Update the shadow for a thing with the provided state."""
        # Ensure the state is properly formatted
        if isinstance(state, str):
            try:
                state = json.loads(state)
            except json.JSONDecodeError as e:
                sys.exit(f"Error parsing state JSON: {e}")
        
        # Create the proper shadow document structure if not provided
        if "state" not in state:
            payload = {"state": state}
        else:
            payload = state
        
        # Print the formatted payload for debugging
        print(f"\nSending shadow update: {json.dumps(payload, indent=2)}")
            
        return self._send_request("POST", thing_name, payload)
    
    def delete_shadow(self, thing_name):
        """Delete the shadow for a thing."""
        return self._send_request("DELETE", thing_name)

    def verify_openai_api(self, api_key):
        """Verify the OpenAI API key works by making a test API call."""
        url = "https://api.openai.com/v1/chat/completions"
        headers = {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {api_key}"
        }
        data = {
            "model": "gpt-4.1",
            "messages": [
                {"role": "system", "content": "You are a helpful assistant."},
                {"role": "user", "content": "Say hello"}
            ],
            "max_tokens": 10
        }
        
        try:
            print("Testing OpenAI API connection...")
            response = requests.post(url, headers=headers, json=data, timeout=30)
            response.raise_for_status()
            result = response.json()
            print("✓ OpenAI API connection successful!")
            return True
        except Exception as e:
            print(f"✗ Error connecting to OpenAI API: {e}")
            if hasattr(e, 'response') and e.response is not None:
                try:
                    error_data = e.response.json()
                    print(f"Error details: {json.dumps(error_data, indent=2)}")
                except:
                    print(f"Response status code: {e.response.status_code}")
                    print(f"Response text: {e.response.text}")
            return False
    
    def pin_connection_format_check(self, answer):
        """
        Check if the pin connection answer follows the exact required format.
        
        Expected format: "[pin # (pin name component 1) + pin # (pin name component 2), pin # (pin name component 1) + pin # (pin name component 2), ...]"
        
        Returns True if format is correct, False otherwise.
        """
        try:
            # Remove any leading/trailing whitespace
            answer = answer.strip()
            
            # Check if answer starts with '[' and ends with ']'
            if not (answer.startswith('[') and answer.endswith(']')):
                print(f"Format check failed: Answer must be enclosed in square brackets")
                print(f"Expected: [pin # (name) + pin # (name), ...]")
                print(f"Received: {answer}")
                return False
            
            # Remove the outer brackets
            inner_content = answer[1:-1].strip()
            
            if not inner_content:
                print(f"Format check failed: Empty content within brackets")
                return False
            
            # Split by comma to get individual connections
            connections = [conn.strip() for conn in inner_content.split(',')]
            
            if not connections:
                print(f"Format check failed: No connections found")
                return False
            
            # Validate each connection
            for i, connection in enumerate(connections):
                if not self._validate_single_connection(connection, i + 1):
                    return False
            
            print(f"✓ Pin connection format check passed: {len(connections)} connection(s) with correct format")
            return True
            
        except Exception as e:
            print(f"Format check failed with exception: {e}")
            return False

    def _validate_single_connection(self, connection, connection_num):
        """
        Validate a single pin connection string.

        Expected format: "pin # (pin name component 1) + pin # (pin name component 2)"

        Returns True if valid, False otherwise.
        """
        try:
            # Define regex pattern for a single connection
            # pin NUMBER (NAME) + pin NUMBER (NAME)
            pattern = r'^pin\s+(\d+)\s*\(([^)]+)\)\s*\+\s*pin\s+(\d+)\s*\(([^)]+)\)$'
            
            match = re.match(pattern, connection.strip())
            if not match:
                print(f"Format check failed: Connection {connection_num} doesn't match required pattern")
                print(f"Expected: pin # (name) + pin # (name)")
                print(f"Received: {connection}")
                return False
            
            # Extract components
            pin1_num_str, pin1_name, pin2_num_str, pin2_name = match.groups()
            
            # Validate pin numbers are integers
            try:
                pin1_num = int(pin1_num_str)
                pin2_num = int(pin2_num_str)
            except ValueError:
                print(f"Format check failed: Connection {connection_num} has invalid pin numbers")
                print(f"Pin numbers must be integers, got: '{pin1_num_str}' and '{pin2_num_str}'")
                return False
            
            # Validate pin numbers are positive
            if pin1_num <= 0 or pin2_num <= 0:
                print(f"Format check failed: Connection {connection_num} has non-positive pin numbers")
                print(f"Pin numbers must be positive integers, got: {pin1_num} and {pin2_num}")
                return False
            
            # Validate pin names are not empty
            pin1_name = pin1_name.strip()
            pin2_name = pin2_name.strip()
            
            if not pin1_name:
                print(f"Format check failed: Connection {connection_num} has empty pin 1 name")
                return False
            
            if not pin2_name:
                print(f"Format check failed: Connection {connection_num} has empty pin 2 name")
                return False
            
            # Optional: Check pin name length (keeping it reasonable)
            if len(pin1_name) > 20:
                print(f"Format check failed: Connection {connection_num} pin 1 name too long (max 20 chars)")
                print(f"Pin 1 name: '{pin1_name}' ({len(pin1_name)} chars)")
                return False
            
            if len(pin2_name) > 20:
                print(f"Format check failed: Connection {connection_num} pin 2 name too long (max 20 chars)")
                print(f"Pin 2 name: '{pin2_name}' ({len(pin2_name)} chars)")
                return False
            
            # Check for invalid characters in pin names (parentheses would break parsing)
            if '(' in pin1_name or ')' in pin1_name:
                print(f"Format check failed: Connection {connection_num} pin 1 name contains parentheses")
                print(f"Pin names cannot contain '(' or ')' characters: '{pin1_name}'")
                return False
            
            if '(' in pin2_name or ')' in pin2_name:
                print(f"Format check failed: Connection {connection_num} pin 2 name contains parentheses")
                print(f"Pin names cannot contain '(' or ')' characters: '{pin2_name}'")
                return False
            
            print(f"✓ Connection {connection_num} valid: pin {pin1_num} ({pin1_name}) + pin {pin2_num} ({pin2_name})")
            return True
            
        except Exception as e:
            print(f"Format check failed for connection {connection_num} with exception: {e}")
            return False
    
    def pin_label_format_check(self, answer):
        """
        Check if the pin label answer follows the exact required format.
        
        Expected format: "[number of pins: X] [1:label, 2:label, 3:label, ...]"
        
        Returns True if format is correct, False otherwise.
        """
        try:
            # Define the regex pattern for the exact format
            # Part 1: [number of pins: X] where X is a number
            # Part 2: [1:label, 2:label, ..., N:label] where N matches the number from part 1
            pattern = r'^\[number of pins: (\d+)\] \[(\d+:[^,\]]+(?:, \d+:[^,\]]+)*)\]$'
            
            match = re.match(pattern, answer.strip())
            if not match:
                print(f"Format check failed: Answer doesn't match basic pattern")
                print(f"Expected: [number of pins: X] [1:label, 2:label, ...]")
                print(f"Received: {answer}")
                return False
            
            # Extract the number of pins and the pin list
            declared_pins = int(match.group(1))
            pin_list_str = match.group(2)
            
            # Parse the pin list
            pin_entries = pin_list_str.split(', ')
            actual_pins = len(pin_entries)
            
            # Check if the declared number matches actual number of pins
            if declared_pins != actual_pins:
                print(f"Format check failed: Declared pins ({declared_pins}) doesn't match actual pins ({actual_pins})")
                return False
            
            # Check if pin numbers are sequential starting from 1
            expected_pins = set(range(1, declared_pins + 1))
            actual_pin_numbers = set()
            
            for entry in pin_entries:
                # Each entry should be "number:label"
                if ':' not in entry:
                    print(f"Format check failed: Pin entry '{entry}' missing colon")
                    return False
                
                pin_num_str, label = entry.split(':', 1)
                
                try:
                    pin_num = int(pin_num_str)
                    actual_pin_numbers.add(pin_num)
                except ValueError:
                    print(f"Format check failed: Invalid pin number '{pin_num_str}'")
                    return False
                
                # Check that label is not empty
                if not label.strip():
                    print(f"Format check failed: Empty label for pin {pin_num}")
                    return False
            
            # Check if pin numbers are exactly 1 to N
            if actual_pin_numbers != expected_pins:
                print(f"Format check failed: Pin numbers are not sequential 1-{declared_pins}")
                print(f"Expected: {sorted(expected_pins)}")
                print(f"Found: {sorted(actual_pin_numbers)}")
                return False
            
            print(f"✓ Pin label format check passed: {declared_pins} pins with correct format")
            return True
            
        except Exception as e:
            print(f"Format check failed with exception: {e}")
            return False

    def parse_question_type(self, question):
        """Parse the question to determine its type and extract the actual question."""
        question_types = {
            "pin labels/": "pin_labels",
            "pin connect/": "pin_connect", 
            "comp purpose/": "comp_purpose"
        }
        
        for prefix, question_type in question_types.items():
            if question.startswith(prefix):
                actual_question = question[len(prefix):]
                return question_type, actual_question
        
        return None, question

    def get_system_prompt(self, question_type):
        """Get the appropriate system prompt based on question type."""
        prompts = {
            "pin_labels": "You are acting as a mediating step in a component descriptor program. In this instruction there will be a component number. Please provide the number of pins as well as each pins label in as few letters as possible. Use the format [number of pins: #] [1:label, 2:label, 3:label]. There should be no characters preceding or following the outlined format. Use the format exactly: make sure to include the [number of pins: ] label. Do not use more than 3 letters for the labels.",
            
            "pin_connect": '''You are acting as a mediating step in a component descriptor program. In this instruction there will be a component,
            and something the user wants to connect to. This will be in the format: component + component to connect. Please provide information about how to connect this 
            component in the format [pin # (pin name component 1) + pin # (pin name component 2)]. Keep pin names less than 5 characters. 
            For example: [pin 1 (VCC) + pin 2 (V+), pin 2 (GND) + pin 3 (GND), pin 3 (MOSI) + pin 6 (GPIO1)].
            It is very important that you follow this format exactly, with absolutely no extra text preceding, following or missing. You must use integer numbers for pin #. Do not use the format pin AD0 (TCK)''',

            
            "comp_purpose": "You are acting as a mediating step in a component descriptor program. In this instruction there will be a component number. Please provide an explanation of what the component does in 90 characters or less. Use abbreviation and shortened language if needed."
        }
        
        return prompts.get(question_type, "")

    def chatbot_mode(self, thing_name, api_key, model="gpt-4.1", verbose=False):
        """ChatGPT integration mode - monitors for questions and provides answers."""
        print("\n=== ChatGPT Integration Mode ===")
        
        # First verify the API key works
        if not self.verify_openai_api(api_key):
            print("Error: Could not connect to OpenAI API. Please check your API key and internet connection.")
            return
        
        print("Monitoring device shadow for questions...")
        print("Press Ctrl+C to exit")
        
        # Initialize shadow version tracking
        last_version = -1  # Start with -1 to match CC3200 code's initialization
        
        # Track the last question we processed to avoid duplicates (with timestamps)
        # Format: {question: timestamp_when_processed}
        processed_questions = {}
        feedback_loop_timeout = 25  # seconds
        
        try:
            while True:
                # Get the current shadow state
                try:
                    shadow = self.get_shadow(thing_name)
                    
                    # Extract version number (same way CC3200 does)
                    current_version = shadow.get('version', 0)
                    
                    if verbose:
                        print(f"\nCurrent shadow version: {current_version}, Last version: {last_version}")
                    
                    # Check if shadow has been updated (version changed)
                    # This matches the CC3200 code's approach exactly
                    if current_version > last_version:
                        print(f"\nShadow update detected! Version changed from {last_version} to {current_version}")
                        
                        # If this is the first check, just capture state and continue
                        if last_version == -1:
                            print("Initial shadow state captured")
                            last_version = current_version
                            continue
                        
                        # Look for a Question in the reported state
                        current_question = None
                        if 'state' in shadow and 'reported' in shadow['state']:
                            # Try both capitalizations
                            if 'Question' in shadow['state']['reported']:
                                current_question = shadow['state']['reported']['Question']
                                print(f"Found 'Question' field: {current_question}")
                            elif 'question' in shadow['state']['reported']:
                                current_question = shadow['state']['reported']['question']
                                print(f"Found 'question' field: {current_question}")
                            else:
                                print("No Question field found in reported state:")
                                if verbose:
                                    print(json.dumps(shadow['state']['reported'], indent=2))
                        else:
                            print("No reported state found or it doesn't contain a question")
                            if verbose:
                                print(json.dumps(shadow, indent=2))
                        
                        # If we found a question, check if we should process it
                        if current_question:
                            current_time = time.time()
                            
                            # Check if this question was processed recently (within feedback_loop_timeout seconds)
                            recently_processed = (current_question in processed_questions and 
                                                current_time - processed_questions[current_question] < feedback_loop_timeout)
                            
                            if not recently_processed:
                                print(f"Processing new question: {current_question}")
                                
                                # NEW: Parse question type
                                question_type, actual_question = self.parse_question_type(current_question)
                                print(f"Question type: {question_type}")
                                print(f"Actual question: {actual_question}")
                                
                                # Check if we have a valid question type
                                if question_type is None:
                                    print("No valid question type prefix found. Returning error message.")
                                    answer = "please choose a question type"
                                else:
                                    # Call OpenAI API to get the answer with appropriate prompt
                                    try:
                                        answer = self.get_chatgpt_response_with_retry(
                                            actual_question, api_key, model, question_type, verbose, 
                                            thing_name=thing_name
                                        )
                                        print(f"Final ChatGPT response: {answer}")
                                    except Exception as e:
                                        print(f"Error getting response from ChatGPT: {e}")
                                        answer = "Error processing question"
                                
                                # Update the shadow with the answer - using var field to match CC3200 expectations
                                # Only skip update if we already set a status message during the retry process
                                # (i.e., the retry function already updated the shadow with "wait" or "parse fail")
                                if answer == "parse fail":
                                    print(f"Answer is 'parse fail' - shadow was already updated during retry process")
                                else:
                                    try:
                                        update_payload = {
                                            "state": {
                                                "desired": {
                                                    "Answer": answer,
                                                    "var": answer[:200]  # Add to var field for compatibility with CC3200 code
                                                }
                                            }
                                        }
                                        
                                        result = self.update_shadow(thing_name, update_payload)
                                        print("Shadow updated with answer")
                                        if verbose:
                                            print("Update result:")
                                            print(json.dumps(result, indent=2))
                                            
                                    except Exception as e:
                                        print(f"Error updating shadow with answer: {e}")
                                
                                # Add the question to our processed dict with current timestamp
                                processed_questions[current_question] = current_time
                                
                                # Clean up old entries to prevent memory issues (keep only recent entries)
                                cutoff_time = current_time - feedback_loop_timeout
                                processed_questions = {q: t for q, t in processed_questions.items() if t > cutoff_time}
                                
                            else:
                                time_since_processed = current_time - processed_questions[current_question]
                                print(f"Question '{current_question}' was processed {time_since_processed:.1f} seconds ago. "
                                      f"Skipping to avoid feedback loop (will allow after {feedback_loop_timeout} seconds).")
                        
                        # Update the last version regardless of whether we found a question
                        last_version = current_version
                        
                except Exception as e:
                    print(f"Error getting or processing shadow: {e}")
                
                # Wait before checking again
                time.sleep(1)
                
        except KeyboardInterrupt:
            print("\nExiting ChatGPT integration mode.")
    
    def get_chatgpt_response_with_retry(self, question, api_key, model="gpt-4.1", question_type=None, verbose=False, max_retries=3, thing_name=None):
        """Get a response from ChatGPT API with format checking and retry logic for pin_labels and pin_connect."""
        
        for attempt in range(max_retries):
            try:
                print(f"\nAttempt {attempt + 1} of {max_retries}")
                answer = self.get_chatgpt_response(question, api_key, model, question_type)
                
                # Check format based on question type
                format_check_passed = True
                
                if question_type == "pin_labels":
                    format_check_passed = self.pin_label_format_check(answer)
                    if format_check_passed:
                        print("✓ Pin labels format check passed!")
                    else:
                        print("✗ Pin labels format check failed!")
                        
                elif question_type == "pin_connect":
                    format_check_passed = self.pin_connection_format_check(answer)
                    if format_check_passed:
                        print("✓ Pin connection format check passed!")
                    else:
                        print("✗ Pin connection format check failed!")
                
                # If format check passed or no format checking needed, return the answer
                if format_check_passed:
                    return answer
                else:
                    # Format check failed
                    if attempt < max_retries - 1:
                        # Update shadow with "wait" status before retrying
                        if thing_name:
                            try:
                                wait_payload = {
                                    "state": {
                                        "desired": {
                                            "Answer": "wait",
                                            "var": "wait"
                                        }
                                    }
                                }
                                self.update_shadow(thing_name, wait_payload)
                                print("Shadow updated with 'wait' status")
                            except Exception as e:
                                print(f"Error updating shadow with 'wait' status: {e}")
                        
                        print("Retrying in 2 seconds...")
                        time.sleep(2)
                        continue
                    else:
                        print("Max retries reached. Format check failed.")
                        # Update shadow with "parse fail" status
                        if thing_name:
                            try:
                                fail_payload = {
                                    "state": {
                                        "desired": {
                                            "Answer": "parse fail",
                                            "var": "parse fail"
                                        }
                                    }
                                }
                                self.update_shadow(thing_name, fail_payload)
                                print("Shadow updated with 'parse fail' status")
                            except Exception as e:
                                print(f"Error updating shadow with 'parse fail' status: {e}")
                        return "parse fail"
                        
            except Exception as e:
                print(f"Error in attempt {attempt + 1}: {e}")
                if attempt < max_retries - 1:
                    # Update shadow with "wait" status before retrying
                    if thing_name:
                        try:
                            wait_payload = {
                                "state": {
                                    "desired": {
                                        "Answer": "wait",
                                        "var": "wait"
                                    }
                                }
                            }
                            self.update_shadow(thing_name, wait_payload)
                            print("Shadow updated with 'wait' status due to API error")
                        except Exception as shadow_e:
                            print(f"Error updating shadow with 'wait' status: {shadow_e}")
                    
                    print("Retrying in 2 seconds...")
                    time.sleep(2)
                else:
                    # Final attempt failed, update with error status
                    if thing_name:
                        try:
                            fail_payload = {
                                "state": {
                                    "desired": {
                                        "Answer": "parse fail",
                                        "var": "parse fail"
                                    }
                                }
                            }
                            self.update_shadow(thing_name, fail_payload)
                            print("Shadow updated with 'parse fail' status due to API errors")
                        except Exception as shadow_e:
                            print(f"Error updating shadow with 'parse fail' status: {shadow_e}")
                    raise
        
        # This should not be reached due to the logic above, but just in case
        raise Exception("All retry attempts failed")
    
    def get_chatgpt_response(self, question, api_key, model="gpt-4.1", question_type=None):
        """Get a response from ChatGPT API for the given question."""
        url = "https://api.openai.com/v1/chat/completions"
        
        headers = {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {api_key}"
        }
        
        # Get the appropriate system prompt based on question type
        system_prompt = self.get_system_prompt(question_type)
        
        if not system_prompt:
            raise ValueError(f"Invalid question type: {question_type}")
        
        data = {
            "model": model,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": question}
            ],
            "max_tokens": 150  # Limit response length
        }
        
        print(f"Sending request to OpenAI API with model: {model}")
        print(f"Question type: {question_type}")
        print(f"System prompt: {system_prompt}")
        print(f"User question: {question}")
        
        try:
            response = requests.post(url, headers=headers, json=data, timeout=30)
            response.raise_for_status()
            result = response.json()
            
            # Print full response for debugging
            if len(str(result)) < 1000:  # Only print if response is reasonable size
                print(f"Full API response: {json.dumps(result, indent=2)}")
            else:
                print("API response received (too large to display)")
            
            # Extract the assistant's response
            return result["choices"][0]["message"]["content"].strip()
            
        except Exception as e:
            print(f"Error calling ChatGPT API: {str(e)}")
            if hasattr(e, 'response') and e.response is not None:
                try:
                    error_data = e.response.json()
                    print(f"API error details: {json.dumps(error_data, indent=2)}")
                except:
                    print(f"Response status code: {e.response.status_code}")
                    print(f"Response text: {e.response.text}")
            raise


def pretty_print(data):
    """Print JSON data in a nicely formatted way."""
    if data:
        print(json.dumps(data, indent=2))
    else:
        print("No data returned")


def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description="AWS IoT Shadow REST API Client with ChatGPT Integration")
    parser.add_argument("--endpoint", required=True,
                        help="AWS IoT endpoint (e.g., a15jh17gg8blx2-ats.iot.us-east-1.amazonaws.com)")
    parser.add_argument("--region", default="us-east-1",
                        help="AWS region (default: us-east-1)")
    parser.add_argument("--thing-name", required=True,
                        help="Name of the IoT Thing")
    
    # Create subparsers for different commands
    subparsers = parser.add_subparsers(dest="command", help="Command to execute")
    subparsers.required = True
    
    # Get command
    get_parser = subparsers.add_parser("get", help="Get device shadow")
    
    # Update command
    update_parser = subparsers.add_parser("update", help="Update device shadow")
    update_parser.add_argument("--state", required=True, 
                               help="JSON string with the desired state (e.g., '{\"desired\":{\"var\":\"new_value\"}}')")
    
    # Delete command
    delete_parser = subparsers.add_parser("delete", help="Delete device shadow")
    
    # ChatGPT integration command
    chatbot_parser = subparsers.add_parser("chatbot", 
                                        help="Start ChatGPT integration mode")
    chatbot_parser.add_argument("--api-key", 
                             help="OpenAI API key (or set OPENAI_API_KEY environment variable)")
    chatbot_parser.add_argument("--model", default="gpt-4.1",
                             help="OpenAI model to use (default: gpt-4.1)")
    chatbot_parser.add_argument("--verbose", action="store_true",
                             help="Enable verbose debugging output")
    
    return parser.parse_args()


def main():
    """Main function to run the client."""
    args = parse_args()
    
    # Create the client
    client = IoTShadowClient(args.endpoint, args.region)
    
    try:
        # Execute the requested command
        if args.command == "get":
            result = client.get_shadow(args.thing_name)
            print("\nCurrent Shadow State:")
            pretty_print(result)
            
        elif args.command == "update":
            state = json.loads(args.state)
            result = client.update_shadow(args.thing_name, state)
            print("\nShadow Update Result:")
            pretty_print(result)
            
        elif args.command == "delete":
            result = client.delete_shadow(args.thing_name)
            print("\nShadow Delete Result:")
            pretty_print(result)
            
        elif args.command == "chatbot":
            # Get API key from args or environment variable
            api_key = args.api_key or os.environ.get("OPENAI_API_KEY")
            if not api_key:
                sys.exit("Error: OpenAI API key is required. Provide it using --api-key or set OPENAI_API_KEY environment variable.")
            
            # Make sure we have a valid API key format
            if not api_key.startswith(('sk-', 'sk-proj-')):
                print("Warning: API key format doesn't match expected pattern (should start with 'sk-' or 'sk-proj-')")
            
            # Start ChatGPT integration mode
            client.chatbot_mode(args.thing_name, api_key, args.model, args.verbose)
            
    except Exception as e:
        sys.exit(f"Error: {e}")


if __name__ == "__main__":
    main()