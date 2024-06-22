/*
1200 baud (actual rate: 1205)
2400 baud (actual rate: 2396)
4800 baud (actual rate: 4808)
9600 baud (actual rate: 9598)
14400 baud (actual rate: 14414)
19200 baud (actual rate: 19208)
28800 baud (actual rate: 28829)
31250 baud
38400 baud (actual rate: 38462)
56000 baud (actual rate: 55944)
57600 baud (actual rate: 57762)
76800 baud (actual rate: 76923)
115200 baud (actual rate: 115942)
230400 baud (actual rate: 231884)
460800 baud (actual rate: 470588)
921600 baud (actual rate: 941176)
1000000 baud
*/

PROGMEM const char *help =
  "***AVAILABLE COMMANDS***\n"
  "Test2\n"
  "Use strlen(help) to see size\n"
  
  "\nEnter 'AT+[COMMAND]?' for more information on specified command. "
  "For example, try 'AT+DEBUG?' to see what the debug command does and what options are available"; // already prints \n at end
