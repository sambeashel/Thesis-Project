from machine import Pin, UART
import time

# Define Constants
ADC_RAIN_THRESHOLD = 50000

# Global Variables
global OPEN
global CLOSED

OPEN = 0
CLOSED = 1


# Initialise Pins

# Set direction pins for motor
dir_pin_one = Pin(3, Pin.OUT)
dir_pin_two = Pin(5, Pin.OUT)
    
# Set step pins for motor
step_pin_one = Pin(2, Pin.OUT)
step_pin_two = Pin(4, Pin.OUT)

# Set LED as an output
led_pin = Pin(25, Pin.OUT)

# Set limit switch pin as an input
limit_switch_one = Pin(21, Pin.IN, Pin.PULL_DOWN)
limit_switch_two = Pin(20, Pin.IN, Pin.PULL_DOWN)
limit_switch_three = Pin(19, Pin.IN, Pin.PULL_DOWN)
limit_switch_four = Pin(18, Pin.IN, Pin.PULL_DOWN)

# Set up ADC pins
rain_sensor = machine.ADC(28)
dew_sensor = machine.ADC(27)

# This stores the analog value from the rain sensors
rain_sensor_value = 0
dew_sensor_value = 0

# Define UART pin
uartPin = UART(0, 115200)

# Define the wake up pin
wake_up_pin = Pin(11, Pin.OUT)


"""
Reads the ADC pin connected to the rain sensor output

Parameters:
-----------
None

Returns:
--------
rain_sensor_value: int: The value of the rain sensor output
"""
def read_rain_sensor():
    rain_sensor_value = rain_sensor.read_u16()
    return rain_sensor_value
    

"""
Reads the ADC pin connected to the dew sensor output

Parameters:
-----------
None

Returns:
--------
rain_sensor_value: int: The value of the dew sensor output
"""
def read_dew_sensor():
    dew_sensor_value = dew_sensor.read_u16()
    return dew_sensor_value


"""
Sets the direction pins connected to the stepper motor drivers
to spin them clockwise.

Notice how stepper 1 is set to '1' and stepper 2 is set to '0'
This is because the steppers are facing opposite directions, thus
their clockwise spin direction must be opposite.

Parameters:
-----------
None

Returns:
--------
None
"""
def clockwise_rotation(): 
    # Set motor for clockwise rotation
    # (Note: these must be opoosite as motors are aligned on opposite sides of cover)
    dir_pin_one.value(1)
    dir_pin_two.value(0)


"""
Sets the direction pins connected to the stepper motor drivers
to spin them anticlockwise.

Notice how stepper 1 is set to '0' and stepper 2 is set to '1'
This is because the steppers are facing opposite directions, thus
their anticlockwise spin direction must be opposite.

Parameters:
-----------
None

Returns:
--------
None
"""
def anticlockwise_rotation(): 
    # Set motor for anticlockwise rotation
    # (Note: these must be opoosite as motors are aligned on opposite sides of cover)
    dir_pin_one.value(0)
    dir_pin_two.value(1)
    

"""
This rotates the first stepper which is associated with limit switches 1 (open) and 3 (close)
by sending pulses to the stepper driver.

Parameters:
-----------
None

Returns:
--------
None
"""
def rotate_stepper_one():
    # Send a rising and falling edge to the stepper driver
    step_pin_one.value(1)
    
    time.sleep(0.002)
    
    step_pin_one.value(0)
    
    time.sleep(0.002)
    

"""
This rotates the second stepper which is associated with limit switches 2 (open) and 4 (close)
by sending pulses to the stepper driver.

Parameters:
-----------
None

Returns:
--------
None
"""
def rotate_stepper_two():
    # Send a rising and falling edge to the stepper driver
    step_pin_two.value(1)
    
    time.sleep(0.002)
    
    step_pin_two.value(0)
    
    time.sleep(0.002)
    

"""
This function rotates the steppers to change the position of the cover to OPEN
The steppers will rotate until limit switches 1 and 2 have been hit by the gantry
plate.

Parameters:
-----------
None

Returns:
--------
None
"""
def open_cover():
    
    # Set the motors for clockwise rotation as we are opening the cover
    clockwise_rotation()
    start_time = time.time()

    # Rotate the steppers until limit switches 1 and 2 are closed
    while True:
        if not limit_switch_two.value():
            rotate_stepper_one()
            
        if not limit_switch_two.value():
            rotate_stepper_two()
            
        if limit_switch_two.value():
            return
        
        # Incase the limit switches haven't closed after 37 seconds, then stop them 
        # (as there may be a hardware error)
        if time.time() - start_time > 37:
            return

        
"""
This function rotates the steppers to change the position of the cover to OPEN
The steppers will rotate until limit switches 3 and 4 have been hit by the gantry
plate.

Parameters:
-----------
None

Returns:
--------
None
"""
def close_cover():
    
    # Set the motors for clockwise rotation as we are opening the cover
    anticlockwise_rotation()
    start_time = time.time()

    # Rotate the steppers until limit switches 3 and 4 are closed
    while True:
        if not limit_switch_three.value():
            rotate_stepper_one()
        
        if not limit_switch_four.value():
            rotate_stepper_two()
            
        if limit_switch_three.value() and limit_switch_four.value():
            return
        
        # Incase the limit switches haven't closed after 37 seconds, then stop them 
        # (as there may be a hardware error)
        if time.time() - start_time > 37:
            return

"""
This function will be called when the cover is in the closed state.
It reads both of the rain sensors and opens the cover accordingly.

If the rain sensor is wet and the dew sensor is dry, the cover will open.

If both sensors are wet, there may be dew or dew AND rain. Thus a message will
be send to the M5StickC over UART to check the WeatherStack API.

Parameters:
-----------
None

Returns:
--------
0: Cover Closed
1: Cover Opened
"""
def cover_closed_rain_sensor_check():

    # Read the rain and dew sensor values
    rain_sensor_value = read_rain_sensor()
    dew_sensor_value = read_dew_sensor()
    
    # Some print messages for debugging
    print(rain_sensor_value)
    print(dew_sensor_value)
    print("Here we are")
       
    # Check if one sensor is wet - if this is the case then we need to open the cover
    if ((rain_sensor_value < ADC_RAIN_THRESHOLD) and (dew_sensor_value > ADC_RAIN_THRESHOLD)):
        send_to_dashboard()
        open_cover()
        
        return 1
    
    # Check if both sensors are wet - if this is the case then there is dew present.
    # This means that there could be (dew) or (rain AND dew) so need to consult with the
    # M5 to find out if it is raining
    if ((rain_sensor_value < ADC_RAIN_THRESHOLD) and (dew_sensor_value < ADC_RAIN_THRESHOLD)):
        
        # Wake up the M5StickC by setting a pin to high
        wake_up_pin.value(1)
        time.sleep(2)
        wake_up_pin.value(0)
        
        # Print statement for debugging
        print("Sending R's")
        # Send a string of 'R' to the M5StickC. Wait for the M5 to reply and set this 
        # reply equal to "serial_string"
        serial_string = read_uart("RRRRRRRRRRRR")

        # Print statements for debugging
        print('serial string')
        print(serial_string)
        
        # If string == y, then API says rain is present
        if(serial_string == b'Y'):
            print("API detects rain")
            send_to_dashboard()
            open_cover()
    
            return 1        
            
        else:
            return 0
    
    
"""
This function will be called when the cover is in the open state.
It reads both of the rain sensors and closes the cover accordingly.

If neither of the sensors are wet, then the cover will be closed,
otherwise it will remain open.

Parameters:
-----------
None

Returns:
--------
0: Cover Opened
1: Cover Closed
"""
def cover_open_rain_sensor_check():
    # Read the rain and dew sensor
    rain_sensor_value = read_rain_sensor()
    dew_sensor_value = read_dew_sensor()
    
    # Some print statements for debugging
    print(rain_sensor_value)
    print(dew_sensor_value)
    
    # If both sensors are dry, then we need to close the cover
    if (rain_sensor_value > ADC_RAIN_THRESHOLD and dew_sensor_value > ADC_RAIN_THRESHOLD):
        close_cover()
        return 1
    
    else:
        return 0


"""
This function will be called when we need to check whether a command has
been sent from the ThingSpeak dashboard.

This function sends a message to the M5StickC to connect to the dashboard,
and then waits for a message back from the M5StickC.

If a b'Y' is received, then the cover will be opened
If a b'N' is received, then the cover will be closed
Otherwise the cover will remain in the same state

Parameters:
-----------
None

Returns:
--------
-1: Do nothing
 0: Open the cover
 1: Close the cover
"""
def check_online_dashboard():
    # Wake up the M5StickC by setting a pin to high
    wake_up_pin.value(1)
    time.sleep(2)
    wake_up_pin.value(0)
    
    # Take the current time
    start_time = time.time()
    count = 0
    
    # Send a string of 'D' to the M5StickC to request it to check the web dashboard.
    # Store the response in 'string_from_wifi'
    string_from_wifi = read_uart("DDDDDDDDDD")
    
    # Take action based on the response of the M5
    if string_from_wifi != None:
        # Some print statements for debugging
        print("String from WiFi")
        print(string_from_wifi)
        
        # If string == y, then online dashboard asks to open cover
        if string_from_wifi == b'Y':
            print("Dashboard Said to Open")
            led_pin.value(0)
            return 1
            
        # If string == 'N', then online dashboard asks to close cover
        elif string_from_wifi == b'N': 
            print("Dashboard Said to Close")
            led_pin.value(0)
            return 0
        
        # If string does not equal anything else, we assume the dashboard has not
        # been updated, thus we do nothing.
        else:
            print("Dashboard Not Updated at all")
            #led_pin.value(1)
            return -1
        
    else:
        # Print statement for debugging
        print("nothing received")
    

"""
This function whenever we need to send the M5StickC a message.

It firstly sends a message (in the input parameters) to the M5StickC via UART
It then waits for a response. If there is no response after 30 seconds, it assumes
an error has occurred and times out (returns b'b').


Parameters:
-----------
string_to_send: String: The string to be sent to the M5StickC

Returns:
--------
string_final: String: The string received from the M5StickC
"""
def read_uart(string_to_send):

    # Print statement for debugging
    print("sent a string")

    # Write the input parameter string to the M5StickC
    uartPin.write(string_to_send)
    
    # Take the current time
    starting_time = time.time()
    
    while 1:
        
        # If there is no response from the M5StickC after 30 seconds, then do nothing
        if time.time() - starting_time > 30:
            string_final = b'b'
            return
        
        # Red the UART RX pin for response from the M5StickC
        string_received = uartPin.read() 
        
        # Store the string we receive from the M5StickC in 'string_final'
        if string_received != None:
            string_final = string_received
            break
        
    return string_final
    

"""
This function whenever we need to check the ThingSpeak dashboard

It firstly calls the check_online_dashboard() function, and then based on
what this function returns, it opens/closes the cover.

If it opens the cover, it will also send a message to the M5StickC to update
the ThingSpeak dashboard 'Total Times Cover Opened' graph.


Parameters:
-----------
cover_state: int: The current state of the cover (OPEN/CLOSED)

Returns:
--------
cover_state: int: The new state of the cover (OPEN/CLOSED)
"""
def dashboard_action(cover_state):
    
    # Check the online dashboard for any commands
    dashboard_flag = check_online_dashboard()

    # Some print statements for debugging
    print("Dashboard_Flag")
    print(dashboard_flag)

    # If the cover is currently CLOSED, and the dashboard returned '1' (open cover),
    # then open the cover
    if dashboard_flag == 1 and cover_state == CLOSED:

        # Send a message to the M5StickC to update the 'Total Times Cover Opened' graph
        send_to_dashboard()

        # Open the cover
        open_cover()

        #Update the cover state
        cover_state = OPEN
        return cover_state
        
    # If the cover is currently OPEN, and the dashboard returned '0' (close cover),
    # then close the cover
    elif dashboard_flag == 0 and cover_state == OPEN:

        # Close the cover
        close_cover()

        # Update the cover state as closed
        cover_state = CLOSED
        return cover_state
        
    # If the dashboard has not updated, then do nothing and keep the cover state the same
    else:
        # Do nothing
        return cover_state
       
        

"""
This function whenever we need to check the rain sensor values

It firstly calls the cover_open_rain_sensor_check() or cover_closed_rain_sensor_check()
functions, and then based on what these functions return, the cover will either open or close.

Parameters:
-----------
cover_state: int: The current state of the cover (OPEN/CLOSED)

Returns:
--------
cover_state: int: The new state of the cover (OPEN/CLOSED)
"""
def rain_sensor_action(cover_state):

    # If the cover is open, then check the rain sensors and take action accordingly
    if cover_state == OPEN:
        if(cover_open_rain_sensor_check()):
            cover_state = CLOSED
            return cover_state
        
    # If the cover state is open, then we want to check for rain.
    # If there is rain then we will close the cover
    if cover_state == CLOSED:
        if(cover_closed_rain_sensor_check()):
            cover_state = OPEN
            return cover_state
    
    return cover_state
    

"""
This function sends data to the web dashboard when the cover is opened.

Each time the cover is opened, the 'Total Times Cover Opened' graph on the 
ThingSpeak dashboard needs to be updated.

The letter 'T' is sent to the dashboard for 'ThingSpeak'

Parameters:
-----------
None

Returns:
--------
None
"""
def send_to_dashboard():
    # Wake up the M5StickC by sending a rising and falling edge
    wake_up_pin.value(1)
    time.sleep(2)
    wake_up_pin.value(0)
    
    # Send a string of 'T' to the M5StickC to request it to update the
    # 'Total Times Cover Opened' graph on the ThingSpeak dashboard.
    uartPin.write("TTTTTTTTTTT")


"""
This is the main loop of the program.

Parameters:
-----------
None

Returns:
--------
None
"""
def main():
    
    # Firstly - open the cover to calibrate limit switches
    open_cover()
    cover_state = OPEN
    
    # Wait for 1 second before continuing
    time.sleep(1)
 
    # Main loop
    while True:
        # Check the ThingSpeak dashboard for any commands
        cover_state = dashboard_action(cover_state)

        # Some print statements for debugging
        print("State of Cover")
        print(cover_state)

        # Sleep for 30 seconds
        time.sleep(10)
        
        # Check the rain sensors and take action accordingly
        # If the cover state is open, then we want to check for no rain.
        # If the ADC is dry, then we will close the cover
        cover_state = rain_sensor_action(cover_state)
            
        # Sleep for 30 seconds
        time.sleep(10)
        

# Automatically run main on startup
if __name__ == "__main__":
    main()


