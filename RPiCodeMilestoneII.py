import cv2

if __name__ == '__main__':
    try:
        video = cv2.VideoCapture(0)
        first_frame = None

        while True:
            # First parameter is a boolean that checks if the video can be read
            # Second parameter is a numPy array of the frame
            video_check, frame = video.read()

            detection_status = 0

            # Convert frame to gray scale image
            consequent_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            # Convert gray scale image to Gaussian blur
            other_frame = cv2.GaussianBlur(consequent_frame, (21, 21), 0)

            if first_frame is None:
                first_frame = consequent_frame
                continue

            frame_difference = cv2.absdiff(first_frame, consequent_frame)
            threshold_value = cv2.threshold(frame_difference, 30, 255, cv2.THRESH_BINARY)[1]
            threshold_value = cv2.dilate(threshold_value, None, iterations=0)
            (contours, _) = cv2.findContours(threshold_value.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

            for contour in contours:
                if cv2.contourArea(contour) < 10000:
                    continue
                detection_status = 1
                (x, y, w, h) = cv2.boundingRect(contour)
                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 3)

            # Show windows of the various frames
            cv2.imshow('Bounded Frame', frame)
            cv2.imshow('Frame', consequent_frame)
            cv2.imshow('Frame Difference', frame_difference)
            cv2.imshow('Threshold', threshold_value)

            # Wait one millisecond and captures a new frame
            key = cv2.waitKey(1)
            
    except KeyboardInterrupt:
        video.release()
        # CLose all windows
        cv2.destroyAllWindows()
        exit()