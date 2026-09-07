import RPi.GPIO as GPIO
from time import sleep

# Motor controller pin
# Motor situation
FORWARD  = 1
BACKWORD = 2
RIGHT = 3
LEFT = 4
STOP = 5
# Motor channel
CH1 = 0
CH2 = 1

# PIN set
# PWM PIN
ENA = 26  #37 pin
ENB = 20   #38 pin
# MOTOR GPIO PIN
IN1 = 19  #35 pin
IN2 = 13  #33 pin
IN3 = 6   #31 pin
IN4 = 5   #29 pin

# GPIO pin setup for motor
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

# Set Pin(Motor)
def setPinConfig(EN, INA, INB):        
    GPIO.setup(EN, GPIO.OUT)
    GPIO.setup(INA, GPIO.OUT)
    GPIO.setup(INB, GPIO.OUT)
    # Work PWM 100khz
    pwm = GPIO.PWM(EN, 100) 
    # Stop PWM   
    pwm.start(0) 
    return pwm

# Motor control function
def setMotorControl(pwm, INA, INB, speed, stat):
    # Motor speed control PWM
    pwm.ChangeDutyCycle(speed)  
    if stat == FORWARD:
        GPIO.output(INA, GPIO.HIGH)
        GPIO.output(INB, GPIO.LOW)
    elif stat == BACKWORD:
        GPIO.output(INA, GPIO.LOW)
        GPIO.output(INB, GPIO.HIGH)
    elif stat == STOP:
        GPIO.output(INA, GPIO.LOW)
        GPIO.output(INB, GPIO.LOW)

# Motor control function rapping
def setMotor(ch, speed, stat):
    if ch == CH1:
        setMotorControl(pwmA, IN1, IN2, speed, stat)
    else:
        setMotorControl(pwmB, IN3, IN4, speed, stat)

# double Motor control function
def setIntegratedControl(stat):
    if stat == STOP:
        setMotor(CH1, 0, STOP)
        setMotor(CH2, 0, STOP)

    elif stat == FORWARD:
        setMotor(CH1, 50, FORWARD)
        setMotor(CH2, 50, FORWARD)

    elif stat == BACKWORD:
        setMotor(CH1, 50, BACKWORD)
        setMotor(CH2, 50, BACKWORD)

    elif stat == RIGHT:
        setMotor(CH1, 50, FORWARD)
        setMotor(CH2, 50, BACKWORD)
        
    elif stat == LEFT:
        setMotor(CH1, 50, BACKWORD)
        setMotor(CH2, 50, FORWARD)

# Motor PWM set
pwmA = setPinConfig(ENA, IN1, IN2)
pwmB = setPinConfig(ENB, IN3, IN4)
