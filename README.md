# What is LoRaBlue?  
To understand LoRaBlue, you first have to understand DefiChat. DefiChat is the heart of the firmware that comes 
programmed on every LoRaBlue device. It's like a cross between a CB radio and an encrypted chat app. You can
connect to LoRaBlue with your mobile phone via bluetooth and it gives you the ability to communicate over LoRa.
LoRa is a license-free radio technology, capable on sending transmissions miles away with very low power. 
DefiChat takes advantage of this by providing a very user-friendly interface and some very unique features. 
The mobile app allows you to use either "Text Mode" or "Audio Mode". In text mode, you will be able to send 
and receive messages just like you would a text message or web chat. Audio mode coverts text into audible speech
and vice versa (think hold-to-speak on your phone and let up to listen like a cb radio). In either mode, messages are encrypted before they're
sent out and decrypted when they come in. The key for this encryption is done using a "passphrase". The 
default passphrase is "open sesame" and anybody using it can communicate with each other (like a public channel). 
Everybody with a matching passphrase can understand the message, but those who have a different passphrase cannot 
decrypt and understand the message. Two users could set their passphrase to say "digital rights are human rights" 
and they would be in a private chat. People using "open sesame" while you were using a different one would not be able to understand your messages 
and you would not be able to understand theirs. Now we'll bring that example into another feature of DefiChat 
called "echoeing". The typical range of LoRaBlue is 1-3 miles, but let's say you're trying to communicate with 
somebody 4 miles away. If there is another LoRaBlue device running DefiChat (with echoes enabled), it will
automatically take that recieved message and repeat it so that the device outside your range still receives it 
and can still reply. If all 3 devices are using the same passphrase, they are now in a group chat and will see 
each others' messages. If the device in the middle is using a different passphrase, they will still automatically 
relay messages to the other two devices, but they won't be able to understand the message they're passing. So 
DefiChat isn't just a radio that you can operate with your phone, but a communication network where privacy is 
considered your right. Your messages don't go through any web server or mobile network.
If you're in a location that has no wireless networks, you still will. Defiance Digital (us) do 
not collect ANY personal data at ANY point. We provide an off-grid private network and a device that can facilitate it. 
At no point do we see any messages and you can verify this by looking at the souce code. Absolutely nothing is hidden.
Not only do you have access to all the source code that makes the software and firmware, but we did it using entry-level 
tools. We wanted it to be easy for the average person to understand how the device works and change it if they so desire.
LoRaBlue is what's known as a "Arduino compatible Development Board" so it can be completely reprogrammed in any way 
you wish. If you ever wanted to learn coding, LoRaBlue makes it easy. It's modeled after Adafruit's "ItsyBitsy nRF52840 Express" so just install that
board in Arduino if you want to code with it. If you want to change the app, just import the "DefiChat.aia" file into 
"App Inventor". By default LoRaBlue is a "smart-radio", but it can literally be whatever you make it. 

# DefiChat Maximum Range  
DefiChat has underwent extensive testing to find the 
best possible settings for the network to function at it's best. While long range was a 
factor, reliability was deemed the most important because DefiChat uses echoes to greatly 
increase range anyway. When using the properly mounted 5.8dBi antennas, the typical range 
is 1-3 miles. This is highly comparible to the average CB radio, but with MAXECHO being set 
to '5' by default, this translates to a typical range of 1-15 miles. It is impossible to predict 
the maximum range for every case because there are so many factors that go into it. In an urban environment, 
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
