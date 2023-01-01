# GhostRadar

## THEORY:
- Designed to work with ESP32 for an OpenSource alternative to expensive Ovilus devices. 
- Will work with most ESP32s and give the user a modular platform to design and create their own modules for customer investigative solutions.
- Intial design and will be coded in Python, and/or MicroPython.
- Obviously, will need to rewritten C++ code. For performance reasons.

## HARDWARE:
- 1.) ESP32
- 2.) Touch Screen LCD Display
- 3.) Various Sensors (Temperature, Pressure, Humidity)
- 4.) Accelerometer
- 5.) Ultrasonic Sensor
- 6.) Magnetometer
- 7.) Hall Sensor

## LAYOUT:
- 1.) Build the test bench, we will need to simulate random data from the sensors. (Random Temps, Accelerometer, Hall Sensor)
- 2.) Write the setup function that will create the wifi ap.
- 3.) Intialize the LCD screen, and create the main menu.
- 4.) Upon user activation from the touch screen, we will need to intialize any currently attached sensors.
- 5.) After activated will currently read in to memory environment data.

## ALGORITHM:
- All sensor data is activated at the same time inside of a nested start function. Data is returned and parsed as a single raw data stream.
- Data is mapped to english alphabet including numbers and the data stream is returned as mapped_data
- Using the sample_rate (This is your word length), the mapped_data is checked to see if a word can be found that is anything less then the length of the sample_rate.
- If the mapped_data is true, it checks if the said data is a match in the word bank.
- If there is a match the word is printed to the screen and through the ap interface.

