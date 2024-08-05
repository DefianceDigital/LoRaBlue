# What is LoRaBlue?  
LoRaBlue is a different thing to different people. To the truck driver it's a modern-day, private,
encrypted and affordable replacement to the CB radio (it was invented by a career truck driver afterall).
To the nature lover or survivalist, it's a device that let's you communicate via your cell phone when
there is no cell service or internet access. To the DIY electronics enthusiest, it's a fully open-source,
Arduino-Compatible development board with LoRa and Bluetooth built in. To the computer programmer, it's
an extremely easy to use device that can plug into a USB port and give it LoRa communication capabilities.
To the radio enthusiest, it's a way to communicate locally and easily experiment with the technology.
To everyone else, it is what you want it to be. It is designed to be used or reprogrammed by anybody.

---
## What is LoRa?
LoRa stands for "Long Range" and it's technology developed for battery powered IOT sensors to communicate
long distances with very little power. How it works is a little complicated but essentially, it achieves
it's long range by broadcasting messages at a very slow rate, on different frequencies. This is great if you're 
communicating small amounts of data such as a text message or sensor value, but utterly useless in 
sending a photo for example. It uses frequencies that are license-free and unlike CB radios or similar devices,
it's legal to encrypt your messages.
---
<br>  

## LoRaBlue Standard Kit (Estimated Price: $55)  
<b>&#x2022; LoRaBlue Tranciever</b>  
<b>&#x2022; 5.8dBi Antenna</b>  
<b>&#x2022; 3m Type-C Power Cord</b>  
<b>&#x2022; Type-C Male-Male Adapter</b>  
<b>&#x2022; 2x Adhesive Mounting Pads</b>  
<b>&#x2022; Developer Pin Header Adapter</b>  
<b>&#x2022; 5V 1A Wall Plug</b>  
<b>&#x2022; 5V 1A Automotive Plug</b>  
<img src="https://github.com/user-attachments/assets/0afdd023-803d-4f5a-bd0d-5930d8e7eddf" width="500" /> 

<br><br><br><br><br><br><br><br><br><br>
---
# DefiChat Introduction  
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

# DefiChat Data Privacy  
Messages themselves are not stored on the LoRaBlue device. Up to 100 Message IDs 
will be stored at a time. Newer Message IDs will replace the oldest Message IDs. Message IDs 
are stored in ram, so they will no longer exist after the device is reset.

# DefiChat Maximum Range  
The maximum range for DefiChat is actually much lower than LoRaBlue itself is capable of. 
This is due to many factors. Primarily it's due to the radio settings themselves, as 
well as the longer message lengths. DefiChat has underwent extensive testing to find the 
best possible settings for the network to function at it's best. While long range was a 
factor, reliability was deemed the most important because DefiChat uses echoes to greatly 
increase range anyway. When using the properly mounted 5.8dBi antennas, the typical range 
is 3.0 miles. This is highly comparible to the average CB radio, but with MAXECHO being set 
to '5' by default, this translates to a typical range of 3-15 miles. It is impossible to predict 
the maximum range for every case because there are so many factors that go into it. Officially we list 
the range as 2.5+ miles because we believe in accurate representations. In an urban environment, 
the range will typically be <1.0 miles due to noise and obstructions. Wooded areas 
will also provide ranges closer to that of urban environments.

# LoRaBlue Power Consumption (Taken via LoRaBlue-Bank)
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
