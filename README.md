# What is LoRaBlue?  
LoRaBlue is a different thing to different people. To the truck driver it's a modern-day, private,
encrypted, affordable, and extremely long-range replacement to the CB radio (it was invented by a career truck driver afterall).
To the nature lover or survivalist, it's a device that let's you communicate via your cell phone when
there is no cell service or internet access. To the DIY electronics enthusiest, it's a fully open-source,
Arduino-Compatible development board with LoRa and Bluetooth built in. To the computer programmer, it's
an extremely easy to use device that can plug into a USB port and give it LoRa communication capabilities.
To the radio enthusiest, it's a way to communicate locally and easily experiment with the technology.
To everyone else, it is what you want it to be. It is designed to be used or reprogrammed by anybody.

# DefiChat Introduction  
DefiChat is an encrypted radio tranceiver where users can "echo" other users' messages.  
By default, the maximum range of a DefiChat message is about 3 miles, but echoes allow this range to be greatly increased.   

# DefiChat Data Privacy  
Messages themselves are not stored on the LoRaBlue device. Up to 100 Message IDs 
will be stored at a time. Newer Message IDs will replace the oldest Message IDs. Message IDs 
are stored in ram, so they will no longer exist after the device is reset.

# DefiChat Maximum Range  
DefiChat has underwent extensive testing to find the 
best possible settings for the network to function at it's best. While long range was a 
factor, reliability was deemed the most important because DefiChat uses echoes to greatly 
increase range anyway. When using the properly mounted 5.8dBi antennas, the typical range 
is 3.0 miles. This is highly comparible to the average CB radio, but with MAXECHO being set 
to '5' by default, this translates to a typical range of 3-15 miles. It is impossible to predict 
the maximum range for every case because there are so many factors that go into it. Officially we list 
the range as 2.5+ miles because we believe in accurate representations. In an urban environment, 
the range will typically be <1.0 miles due to noise and obstructions. Wooded areas 
will also provide ranges closer to that of urban environments.  

# How DefiChat Works  
DefiChat is a psudo-mesh protocol that allows for a virtually unlimited range. 
Messages are encrypted by default but can also be set to unencrypted. Even if the recieving 
device cannot decrypt the message, it will still echo it. When a DefiChat 
message is sent, it is given a unique ID number and the "echo byte" is set to 0. 
When a DefiChat receiver gets this this message, it first checks to see if it has 
logged this Message ID. Whether or not it has seen the message, it will increment 
the "echo byte". If the Message ID returns as one that has not been seen before and 
the "echo byte" of the message is less that the reciever's "MAXECHO" setting, the 
reciever will re-transmit the message. The only difference in the recieved message 
and the re-transmitted message will be the "echo byte", which is used to tell devices 
in the network when to stop echoeing it. Once the "echo byte" has reached a level 
higher than the MAXECHO setting of all devices which have heard the message, the echoes 
will not be able to continue and the message will have reached it's maximum range. 
In summary, when a message is sent it bounces along other DefiChat devices for as 
long as they are willing to forward that message, making the maximum range theoretically 
unlimited.  

# DefiChat Mobile App  
Currently, we only have an Android app but we hope to support iPhone soon.  
<a href="https://github.com/DefianceDigital/LoRaBlue/blob/main/Documentation/Android-Getting-Started.md">View the DefiChat for Android Tutorial</a>  
<a href="https://github.com/DefianceDigital/LoRaBlue/raw/main/AppInventor/DefiChat.apk">Download DefiChat for Android</a>  

## LoRaBlue Standard Kit (Estimated Price: $55)  
<b>&#x2022; LoRaBlue Tranciever</b>  
<b>&#x2022; 5.8dBi Antenna</b>  
<b>&#x2022; 3m Type-C Power Cord</b>  
<b>&#x2022; Type-C Male-Male Adapter</b>  
<b>&#x2022; 2x Adhesive Mounting Pads</b>  
<b>&#x2022; Developer Pin Header Adapter</b>  
<b>&#x2022; 5V 1A Wall Plug</b>  
<b>&#x2022; 5V 1A Automotive Plug</b><br>  
<img src="https://github.com/user-attachments/assets/0afdd023-803d-4f5a-bd0d-5930d8e7eddf" width="500" />  
<img src="https://github.com/user-attachments/assets/588a56ce-592f-416e-8bdb-ecd73c38ca2d" width="500" />  
<img src="https://github.com/user-attachments/assets/f27545bc-6c1c-45d0-aa2d-c74fd386b754" width="500" />  

## LoRaBlue Portability Expansion Kit  (Estimated Price: $45)    
<b>&#x2022; LoRaBlue-Bank (Power Bank)</b>  
<b>&#x2022; 3dBi Folding Antenna</b>  
<b>&#x2022; Portable Assembly Clip</b>  
<b>&#x2022; Assembly Stand</b><br>  
<img src="https://github.com/user-attachments/assets/860e0624-8b1f-401f-b1b0-0acebec728d3" width="500" />  
<img src="https://github.com/user-attachments/assets/745c5d30-c4bc-4657-a164-8e204d8aee5b" width="500" />  

# LoRaBlue Power Consumption
Transmit Current: <155mA  
Recieve Current: <31mA   
Sleep Current: <0.4mA  

# LoRaBlue-Bank Power Consumption
Capacity: 600mAh  
Charge Time: 2.5 hours   
Self-Discharge Current: <0.1mA  
Maximum Connected Standby Time: 20 hours  
Minimum Connected Current (LEDs On/Sleeping): <1mA  
Minimum Connected Current (LEDs Off/Sleeping): <0.5mA  
