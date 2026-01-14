# arduino-timing-study

(Software used for perception/cognition research project, completed in 2014.)

Arduino code used to conduct a study of synchronized performance accuracy. The Arduino's serial ports are connected via MIDI cables to an electronic drum module, which allows stimulus output and performance input to be processed by the Arduino. At the end of each task, the recorded timestamp data is sent via the USB serial port to the host PC. Tasks could be selected and started either by button-presses or by receiving serial text signals from a program on the host PC. Task status is read out on an LCD panel.

I learned C specifically to build this. Because I'm not a C programmer, really, there are all sorts of bad coding practices here (especially repetitive code blocks). But it did what it was supposed to do. I learned a lot about how to optimize code for the small amount of memory available on a microcontroller.
