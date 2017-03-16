# devops_light
Arduino / ESP8266 &amp; WS2812B based availablity checker &amp; light

Code can run on any esp8266 based hardware, including ESP01 with limited memory.

STLs are for a 3d printed case, will provide links to Thingiverse in a while. The top of the case needs to be printed in a see through material.

ToDos:

- Resurrect the json content testing code as an optional test (Check "Status:Up")
- Find 24 byte memory leak
- Create debugging function that prints simultaneously to Serial and MQTT
- Consider refactoring to move availability checking code to a backend (is this sensible?)
- Include smooth transitions between colour states
- Create light based "error codes"
 