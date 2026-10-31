# Mr. Personality
Mr. Personality is a device connects to the user's computer and display different faces depending on different information about the computer. I thought this would be a fun and useful extension I could add to my computer. It's cute, and it also tells you important information, such as if you are overworking your computer.


# Hardware
The hardware is very simple. It uses a ESP32 dev module, which can connect to bluetooth, and a monochrome OLED with an SH1106 controller. Here is the schematic for it:
<img width="947" height="443" alt="Screenshot 2026-10-31 at 2 45 45 PM" src="https://github.com/user-attachments/assets/d5f77746-6358-495b-8377-c74578871aac" />

And here is the pcb for it:

<img width="555" height="681" alt="Screenshot 2026-10-31 at 2 46 12 PM" src="https://github.com/user-attachments/assets/52f57d96-fb58-466b-9079-472743b3ab91" />


# Python script
The point of this script is to read stats from the computer and then send them to the microcontroller. You will need to run this script on your computer; it won't run automatically.
First, the readStats function reads the cpu usage, the battery level, and how long the computer has been on for. Then, the connect function establishes a connection with the microcontroller. Then the try section of the main function writes those stats to microcontroller.

# Arduino Sketch
This file first, in the parseLine function, reads the stats sent from the computer, and saves them as variables. Then, pickMood matches a mood to those stats. For example, if the battery is 23, then it make mood = "tired". Then, setTargets changes the values in the face struct to make a different face for each mood. easeFace then eases the current face values to the new ones, so it looks like the face is smoothly changing. Finally, the loop draws the face every frame and blinks periodically.
