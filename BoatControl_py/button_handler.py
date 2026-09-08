import RPi.GPIO as GPIO
import subprocess
from time import sleep
import os

# BUTTON GPIO PIN
BUTTON_ON_PIN = 21
BUTTON_OFF_PIN = 17

# Get the directory of the current script
script_dir = os.path.dirname(os.path.abspath(__file__))
yolo_script_path = os.path.join(script_dir, 'yolo_main.py')

process = None

def start_process(channel):
    global process
    if process is None: 
        print("Process starting...")
        process = subprocess.Popen(['python3', yolo_script_path])
    else:
        print("Process is already running.")

def stop_process(channel):
    global process
    if process is not None: 
        print("Terminating process...")
        process.terminate()
        process = None
    else:
        print("No process is running.")

if __name__ == "__main__":
    # Set Control
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    # Button Start set
    GPIO.setup(BUTTON_ON_PIN, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    GPIO.setup(BUTTON_OFF_PIN, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    
    GPIO.add_event_detect(BUTTON_ON_PIN, GPIO.RISING, callback=start_process, bouncetime=300)
    GPIO.add_event_detect(BUTTON_OFF_PIN, GPIO.RISING, callback=stop_process, bouncetime=300)

    try:
        print("Button handler started. Press the ON button to start and OFF button to stop.")
        while True:
            sleep(0.1)
    except KeyboardInterrupt:
        if process is not None:
            process.terminate()
        GPIO.cleanup()
        print("\nGPIO cleaned up.")
