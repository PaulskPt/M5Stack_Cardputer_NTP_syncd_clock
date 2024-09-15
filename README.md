# M5Stack_Cardputer_NTP_syncd_clock

This sketch shows you the current time from an NTP server on the builtin M5Stack Cardputer display.
Unfortunately the M5Stack StampS3 has no internal RTC otherwise the NTP datetimestamp could be
written to the internal RTC and be updated onces in an hour or so.

The flag "time_only" controls if:
a) only the time or 
b) date, time, weekday and time-zone (military: A-Z) will be shown.
This flag defaults to true.

The sketch has functionality to show am and pm (controlled by the use_12hr flag).

This sketch reads WiFi credentials, several flags and settings from the file secrets.h.
The flag ```vars_fm_sd``` (sketch line 115) controls to read variables from file secret.h on SD-card. 
This flag defaults to false.

Source of the NTP library is here [NTPClient](https://github.com/taranais/NTPClient).
I made some changes to this library
