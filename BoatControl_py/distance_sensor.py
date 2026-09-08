import RPi.GPIO as GPIO
from time import sleep , time

# ECHO SENSOR GPIO PIN
TRIGGER = 23
ECHO = 24

# GPIO Set
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(TRIGGER,GPIO.OUT)
GPIO.setup(ECHO,GPIO.IN)

def get_distance():
    GPIO.output(TRIGGER,False)
    sleep(0.5)
        
    GPIO.output(TRIGGER,True)
    sleep(0.00001)
    GPIO.output(TRIGGER,False)
        
    start_time = time()
    stop_time = time()

    while GPIO.input(ECHO) == 0:
        start_time = time()
            
    while GPIO.input(ECHO) == 1:
        stop_time = time()
            
    time_interval = stop_time - start_time
    distance = time_interval * 17000
    distance = round(distance, 2)
    return distance
