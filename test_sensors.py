# Create a basic sensor for testing purposes using the Ghost Radar

from random import randint

def test_data():
    
    "Create random raw data simulating sensor data."

    def temp_sensor():
        return randint(0, 50) # DHT11 Sensor, Serial Connection, 0 to 50 Celcius Range

    def humidity_sensor():
        return randint(20, 90) # DHT11 Sensor, Serial Connection, 20 to 90 Percent Range
        
    def pressure_sensor():
        return randint(-100, 100)
    
    return temp_sensor() + humidity_sensor() + pressure_sensor()