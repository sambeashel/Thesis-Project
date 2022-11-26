/*
*******************************************************************************
* Title: M5StickC Code for Thesis Project (ENGG4811)
* Project: Farmbot Smart Water Tank Cover
*
* Institution: The University of Queensland
*
* Name: Samuel Beashel
* Date: 29/9/22
* 
******************************************************************************
*/

#include <M5StickCPlus.h>
//#include <M5StickC.h>
#include <AXP192.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// This converts micro seconds to seconds
#define uS_TO_S_FACTOR 1000000

// This is the time the M5StickC will sleep (seconds)
#define TIME_TO_SLEEP  100

// If the M5StickC exceeds this time when connecting the WiFi, it will sleep
#define WIFI_TIMEOUT_MS 20000

// API information for ThingSpeak dashboard
#define API_SERVER_NAME "https://api.thingspeak.com/update"
#define API_KEY_WRITE "api_key=HTACRL5G50VW4SYL&field1=1"

#define WIFI_CONNECTED 1
#define WIFI_CONNECTION_FAILED 0

#define LENGTH_OF_EMPTY_JSON 2

#define THINGSPEAK_API_QUOTATION_NUMBER 5
#define WEATHERSTACK_API_SQUARE_BRACKET_NUMBER 2

#define HIGH_INPUT 1

#define ARRAY_SIZE 60

#define GPIO_PIN_26_M5 26
#define GPIO_PIN_0_M5 0

// WiFi network credentials
const char *WIFI_NETWORK_NAME = "infrastructure";
const char *WIFI_PASSWORD = "evSaEGgy-aGF";

// Global counter of how many times sleep mode has been activated
// RTC_DATA_ATTR is used so this is kept in memorynot  during deep sleep
RTC_DATA_ATTR int sleep_count = 0;
RTC_DATA_ATTR int uart_flag = 0;


/*
 * This function connects the M5 to the specified WiFi network in 
 * WIFI_NETWORK_NAME and WIFI_PASSWORD
 *
 * Parameters
 * ----------
 * None
 *
 *
 * Returns
 * -------
 * None
 *
*/
void connectToWifi () {

    M5.Lcd.println ("Connecting to WiFi");
  
    // Define M5Stick as a station (this will not be an access point)
    WiFi.mode (WIFI_STA);

    // Connect to the WiFi
    WiFi.begin (WIFI_NETWORK_NAME, WIFI_PASSWORD);
    
    // Measure the elapsed time and timeout if greater than WIFI_TIMEOUT_MS
    unsigned long start_attempt_time = millis ();

    while (WiFi.status () != WL_CONNECTED && millis () - start_attempt_time < WIFI_TIMEOUT_MS)
    {
      delay (500);
    }

    // Check status of connection
    if (WiFi.status () != WL_CONNECTED) {
        Serial2.print ("WiFi Connection Failed");
        
    }

    // This occurrs if connected to WiFi
    else {
      M5.Lcd.println ("Connected!");

    }
}


/*
 * This function writes a string to the ThingSpeak web dashboard
 * at API_SERVER_NAME and API_KEY_WRITE.
 * 
 * Every time the cover is opened, a '1' is sent to the API.
 * ThingSpeak API then updates the 'Total Times Cover Opened' graph
 * by adding a new data point.
 *
 * Parameters
 * ----------
 * None
 *
 *
 * Returns
 * -------
 * None
 *
*/
void writeToAPI () {
  HTTPClient client;

  client.begin (API_SERVER_NAME);
  M5.Lcd.println ("we are sending");
  client.POST (API_KEY_WRITE);
  M5.Lcd.println ("DATA SENT");
  client.end ();

}


/*
 * This function connects to the ThingSpeak web dashboard and pulls data from it
 * related to the 'open' and 'close' buttons.
 *
 * E.g. if the 'open' button is pressed, this function will read the JSON data in
 *      the API and request the Raspberry Pi Pico to open the cover.
 *
 * Parameters
 * ----------
 * None
 *
 *
 * Returns
 * -------
 * 'E': Error has occurred, or there is no new information on the dashboard
 *
 * 'O': The dashboard is requesting for the cover to be opened
 *
 * 'C': The dashboard is requesting for the cover to be closed
*/
char readFromDashboard () {
    // Print a status message
    M5.Lcd.println("Reading from Dashy");

    // If we are connected to WiFi, then read from the dashboard
    if (WiFi.status () == WL_CONNECTED){
        long rnd = random(1, 10);
        HTTPClient client;

        // Connect to the ThingSpeak API
        client.begin("https://api.thingspeak.com/talkbacks/46995/commands.json?api_key=BBX5Z1T3D26O6RTS");

        int httpCode = client.GET();

        if (httpCode > 0) {
          // Extract the JSON from the API and store it in 'payload' variable
          String payload = client.getString();

          // Find the length of the JSON
          int length_of_payload = payload.length();

          // If length == 2, then there is no data to be retreived because JSON is just "[ ]" - thus return 'E' 
            // i.e. the JSON hasn't been updated as there has been no buttons pressed
          if (length_of_payload == LENGTH_OF_EMPTY_JSON) {
              M5.Lcd.println ("No Data");
              return 'E';

          }

            // Count the number of quotation marks in the JSON
            int quote_counter = 0;
            int index_of_command_string = 0;

            // Find the command string in the JSON - the command string occurs after the 5th quotation mark
            for (int i = 0; i < length_of_payload; i++) {
              // We need to find the index of the fifth '"' character
              if (payload[i] == '"') {
                quote_counter++;

            }

                // Find the index of the fifth quotation mark in the JSON. The information between the fifth and 
                // sixth quotation marks is the request from the API i.e. shows if the cover needs to be opened/closed
                if (quote_counter == THINGSPEAK_API_QUOTATION_NUMBER) {
                // + 1 because we want the character after the quotation mark
                index_of_command_string = i + 1;
                break;

            }
          }

            // Now that we know the index of the start of the command inside the JSON, we need to store the
            // entire command section of the JSON inside command_string.
            char command_string[ARRAY_SIZE];

            // 'IndexCounter' variable used to index the 'command_string' variable seen below
            int indexCounter = 0;

          if (quote_counter == THINGSPEAK_API_QUOTATION_NUMBER) {
                
                // Iterate through JSON between beginning and end of "weather description" section to store this data in 'command_string' variable
                for (int i = index_of_command_string; i < length_of_payload; i++) {
                          // Cycle through the payload until the next quote mark if found
                if (payload[i] == '"') {
                    break;
                }

                          // If the character is not a ' " ' (quotation mark), then add it to the command_string
                else {
                    command_string[indexCounter] = payload[i];
                }

                          // Increase the index counter variable
                indexCounter++;

            }

              // Add a null character to the end of the 'command_string' so that it is actually a string
              command_string[indexCounter] = '\0';

          }

            // Print the API request (command_string) to the LCD display
            M5.Lcd.println (command_string);

            // Send a DELETE request back to the API - so the JSON is not used again
            int httpCode = client.sendRequest ("DELETE");

            // Check if the cover needs to be opened
            if (command_string[0] == 'O' and command_string[1] == 'P') {
                writeToAPI();
                M5.Lcd.println ("Open Sesame");
                return 'O';

          }

            // Check if the cover needs to be closed
            if (command_string[0] == 'C' and command_string[1] == 'L') {
                M5.Lcd.println ("Close Sesame");
                return 'C';

          }

          // If the JSON didn't specify to open/close the cover, then return 'E'
          else {
              return 'E';
          
            }

          delay (500);

      }

        // This will hit if there is an error in the HTTP request, it will return 'E' if so.
        else {
          Serial2.print ("Error on HTTP request");
          return 'E';
        
        }

    }

    // This will hit if WiFi is never connected, it will return 'E' if so.
    else {
        Serial2.print ("Wifi is Not Connected");
        return 'E';

    }

}


/*
 * This function connects to the WeatherStack API and pulls data from it
 * related to the current weather of the area.
 *
 * This is required when both of the rain sensors are wet, so it is not certain whether
 * dew or rain has occurred.
 *
 * E.g. if the both sensors are wet, then we are unsure whether dew is present, or a combination
 *      of dew and rain are present. This answers that question.
 *
 * Parameters
 * ----------
 * None
 *
 *
 * Returns
 * -------
 * 1: There is rain present in the area
 *
 * 0: There is no rain present in the area, or there is an error (so we assume no rain)
*/
int readFromAPI () {

    // If we are connected to WiFi, then read from the WeatherStack API
    if (WiFi.status () == WL_CONNECTED) {
        long rnd = random (1, 10);
        HTTPClient client;

        // Connect to the WeatherStack API
        client.begin("http://api.weatherstack.com/current?access_key=51775c78bcde4910c305f71db159049e&query=Tennyson");
        int httpCode = client.GET ();

        if (httpCode > 0) {
                // Extract the JSON from the API and store it in 'payload' variable
          String payload = client.getString();

                // Find the length of the JSON
          int length_of_payload = payload.length();

                // This variable counts the number of square brackets in the JSON
                // This is important as the 'weather description' information is always
                // located after the square bracket
          int square_bracket_counter = 0;

                // This is the index of the beginning of the 'weather descrition' information
          int index_of_weather_activity = 0;

          // Find the weather descriptions - this will tell us if it is raining or not
          for (int i = 0; i < length_of_payload; i++) {

              // We need to find the index of the second '[' character, as this is where the weather
                    // description is always stored
              if (payload[i] == '[') {
                square_bracket_counter++;

                if (square_bracket_counter == WEATHERSTACK_API_SQUARE_BRACKET_NUMBER) {
                            // + 2 because there is a quotation mark after the square bracket before the data begins
                            // E.g. ( [" )
                    index_of_weather_activity = i + 2;  
                    break;

                }
            }       
          }

            // Stores the weather activity section of the JSON
            char weather_activity[ARRAY_SIZE];

            // 'IndexCounter' variable used to index the 'command_string' variable seen below
            int indexCounter = 0;

                // This if statement will trigger if we have successfully found the 2nd square bracket in the JSON
          if (square_bracket_counter == WEATHERSTACK_API_SQUARE_BRACKET_NUMBER) {

                // Now we need to store the 'weather description' information (payload) in a string
                for (int i = index_of_weather_activity; i < length_of_payload; i++) {

              // If we have found a quotation mark, the we know we are at the end of the
        // weather_description information.
        if (payload[i] == '"') {
        break;
                
                    }

        // If we don't see a quotation mark, the add the information to the weather_activity array
        else {
      weather_activity[indexCounter] = payload[i];
                
                    }
                    
                    // Increment the index counter
        indexCounter++;
    
    }

                // Add a null character to the end of the array to ensure that we have turned it into a string
                weather_activity[indexCounter] = '\0';

          }
            
            // Some print messages for the LCD display on the M5StickC
            M5.Lcd.println("We are in");
            M5.Lcd.println(weather_activity);

            // Now that we have the weather_activity array, we need to check for rain inside it.
            // Here, we are cycling through the array checking for the letters 'R' and 'a' 
            // next to each other which indicate rain.
            for (int i = 1; i < sizeof (weather_activity) - 1; i++) {

                // Check for the letters 'R' and 'a' next to each other
                if ((weather_activity[i] == 'a') and (weather_activity[i - 1] == 'R')) {
                    // Some print messages for the LCD of the M5StickC
                    M5.Lcd.println ("LOWELL RAINS");
                    Serial2.print ('Y');

                    // Update the web dashboard to show cover opening
                    writeToAPI();

                    // If there is rain, then return 1
                    return 1;

                }
            }
            
            // Delay for 500ms
      delay (500);

  }

        // This will hit if there is an error in the HTTP request, it will return 0 if so.
        else {
            Serial2.print ("Error on HTTP request");
            return 0;
  }

    }

    // This will hit if WiFi is never connected, it will return 0 if so.
    else {
        Serial2.print ("Wifi is Not Connected");
        return 0;

    }
    
    // If there are any other errors, this will return 0 (this should never trigger)
    return 0;

}


/*
 * This function scans the UART RX pin on the M5StickC for any messages from the Raspberry Pi Pico
 * 
 * Possible messages include:
 * 'R': The Raspberry Pi Pico is wanting the M5StickC to check for rain in the WeatherStack API
 * 'D': The Raspberry Pi Pico is wanting the M5StickC to check for the Thingspeak dashboard for any
 *      button presses (e.g. open  cover, close cover)
 * 'T': The Raspberry Pi Pico is wanting the M5StickC to upload 'open' data to the web dashboard's
 *       'Total Times Cover Opened' graph
 *
 * Parameters
 * ----------
 * None
 *
 *
 * Returns
 * -------
 * -1: If there is no message received from the Raspberry Pi Pico (do nothing)
 *
 * 0: If there is an invalid message received from the Raspberry Pi Pico (do nothing)
 *
 * 1: If an 'R' is received from the Raspberry Pi Pico, prompting the M5StickC to check the WeatherStack API
 *
 * 2: If a 'D' is received from the Raspberry Pi Pico, prompting the M5StickC to check the web dashboard
*/
int scanUART () {

    // Scan the UART RX pin of the M5StickC for any transmissions from the Raspberry Pi Pico
    if (Serial2.available ()) {

        // Store any received character inside the 'receivedString' variable
        char receivedString = Serial2.read();
        

        // Some printed messages to the LCD screen
        M5.Lcd.println("rec string");
        M5.Lcd.println(receivedString);
        M5.Lcd.println(int(receivedString));

        // If an 'R' is received, then we need to check the WeatherStack API for weather data.
        // Thus return 1
        if (receivedString == 'R') {
          M5.Lcd.print("received R");
          uart_flag = 1;
          return 1;
        }

        // If a 'D' is received, then we need to check the the ThingSpeak dashboard for any button presses
        // Thus return 2
        if (receivedString == 'D') {
            M5.Lcd.println ("It Worked!");
            uart_flag = 2;
            return 2;  
        }

        // If a 68 is received, then we need to check the the ThingSpeak dashboard for any button presses
        // Thus return 2 (68 is ASCII for 'D') - this if statement should be irrelevant
        if (int(receivedString) == 68) {
            M5.Lcd.println ("integer worked");
            uart_flag = 2;
            return 2;
        }

        // If a 'T' is received, then we need to post a '1' to the web dashboard to update the 'Total Times Cover Opened'
        // graph.
        if (receivedString == 'T') {
            uart_flag = 3;
            return 3;

      }

        // This will trigger if there is an invalid message received from the Raspberry Pi Pico. Thus, nothing will happen.
        uart_flag = 0;
        return 0;

    }

    // If there is no message received from the Raspberry Pi Pico, then we return -1, and nothing will happen.
    return -1;

}


/*
 * Initialisation function for the M5StickC, this will run when the system is turned on 
 * or woken up from sleep
 *
 * Parameters
 * ----------
 * None
 *
 *
 * Returns
 * -------
 * None
*/
void setup () {

    // Initialise M5StickC
    M5.begin ();

    // Set up a serial connection 
    Serial2.begin (115200, SERIAL_8N1, GPIO_PIN_26_M5, GPIO_PIN_0_M5);


    // Fill the screen Black (testing)
    M5.Lcd.fillScreen (BLACK);

    // Turn off the screen
    //M5.Lcd.writecommand(ST7735_DISPOFF);
    //M5.Axp.ScreenBreath(0);

    // Set the deep sleep wakeup to be when GPIO pin 36 reads a high input
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, HIGH_INPUT);

}


/*
 * This function is the main loop of the program. It will always execute after the setup() function.
 *
 * Parameters
 * ----------
 * None
 *
 *
 * Returns
 * -------
 * None
*/
void loop () {

    // The 'sleep_count' variable tracks the number of times the M5StickC has slept
    if (sleep_count != 0) {
        
        // Take the 'reference' time in ticks
        long int start_time = millis();

        // Scan the UART RX pin for a message from pico
        while (scanUART() == -1) {

            // Take the current time in ticks
          long int current_time = millis();

            // If there is no message received from the Raspberry Pi Pico after 20 seconds
            // then go back to deep sleep
          if (current_time - start_time >= 20000) {
              esp_deep_sleep_start();
          
            }
       
        }

        // Connect to the wifi
        connectToWifi();
        
        // Some print messages
        M5.Lcd.println("UART FLAG");
        M5.Lcd.println(uart_flag);


        // If the uart_flag == 1, then there has been a message to read the WeatherStack API
        if (uart_flag == 1) {
          // Read from the API to check if it's raining
          if (readFromAPI() == 1) {
              // Send a message to the Raspberry Pi Pico to open the cover
              Serial2.print('Y');

          }
      
            else {
                // Send a message to the Raspberry Pi Pico to close the cover
              Serial2.print('N');
                
          }

      }


        // This will trigger if a message has been sent from the online dashboard
        if (uart_flag == 2) {
      char dashboard_flag;

            // A print message for the screen
      M5.Lcd.println("Checking Dashboard");

            // Read from the dashboard
      dashboard_flag = readFromDashboard();

      // Open the cover
      if (dashboard_flag == 'O') {
          Serial2.print('Y');

      }

      // Close the cover
      if (dashboard_flag == 'C') {
          Serial2.print('N');

      }

      // Error/do nothing
      if (dashboard_flag == 'E') {
          Serial2.print('Z');

      }
  }

        // Send data to thingspeak dashboard
        if (uart_flag == 3) {
      writeToAPI();

  }

    }

    // Reset the UART flag to 0
    uart_flag = 0;

    // Increment the sleep count
    sleep_count += 1;

    // Deep sleep the M5StickC
    esp_deep_sleep_start();

}

