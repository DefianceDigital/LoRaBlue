# DefiChat  
DefiChat is a psudo-mesh protocol that allows for a virtually unlimited range. 
Messages are encrypted by default but can also be unencrypted. Even if the recieving 
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
will not be able to continue and the message will have reched it's maximum range.

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
