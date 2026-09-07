# YOLOv5 module import
import cv2
import torch
import motor_control as mc
import distance_sensor as ds
from time import sleep

# Minimum distance to obstacle (in cm)
MIN_DISTANCE = 20

# YOLOv5 model load
# Make sure 'best.pt' is in the same directory
model = torch.hub.load('', 'custom', 'best.pt', source='local')

# Webcam Open
cap = cv2.VideoCapture(0)

# Webcam Frame set
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

# target_object classes (e.g., [0, 1, 2] for specific objects)
target_obj = [0, 1, 2]

print("YOLO detection started...")

try:
    while True:
        # 1. Obstacle Avoidance
        distance = ds.get_distance()
        if distance < MIN_DISTANCE:
            print(f"Obstacle detected at {distance}cm! Stopping.")
            mc.setIntegratedControl(mc.STOP)
            sleep(0.5) # Wait for a moment before re-evaluating
            continue

        # 2. Frame read and Object Detection
        ret, frame = cap.read()
        if not ret:
            print("Failed to grab frame")
            break

        # Detect Object use YOLOv5
        results = model(frame, size=640)
        
        # Filter for target objects
        target_results = [result for result in results.pred[0] if result[-1] in target_obj]

        # Visualize results
        rendered_frame = results.render()[0]
        cv2.imshow('YOLOv5 Object Detection', rendered_frame)

        # 3. Control Logic based on detection
        if not target_results:
            # No target found, search by turning right
            print("No target in sight. Searching...")
            mc.setIntegratedControl(mc.RIGHT)
        else:
            # Target(s) found, sort by confidence
            target_results.sort(key=lambda x: -x[4])
            best_target = target_results[0]
            
            x_min, y_min, x_max, y_max, conf, cls = best_target
            
            # Calculate center of the detected object
            x_center = (x_min + x_max) / 2
            frame_center = 640 / 2
            
            # Define a tolerance for being "centered"
            TOLERANCE = 80

            if abs(x_center - frame_center) < TOLERANCE:
                # Object is centered, move forward
                print("Target centered. Moving forward.")
                mc.setIntegratedControl(mc.FORWARD)
            elif x_center < frame_center:
                # Object is to the left, turn left
                print("Target on the left. Turning left.")
                mc.setIntegratedControl(mc.LEFT)
            else:
                # Object is to the right, turn right
                print("Target on the right. Turning right.")
                mc.setIntegratedControl(mc.RIGHT)

        # Press 'q' to exit
        if cv2.waitKey(1) & 0xFF == ord('q'):
            print("'q' pressed. Shutting down.")
            break

finally:
    # Cleanup
    print("Releasing resources.")
    mc.setIntegratedControl(mc.STOP) # Stop motors
    cap.release()
    cv2.destroyAllWindows()
    GPIO.cleanup()
