# Getting Started With LoRaBlue and Android  

### Step 1: Click "Scan for devices"  
<img src="https://github.com/user-attachments/assets/b06a8a57-7412-48d3-93eb-f6dba1b7c2eb" width="200" />  

### Step 2: Select device that displays "LoRaBlue"  
<img src="https://github.com/user-attachments/assets/322e52ef-2f97-40bf-9c79-eabb9f2fcb56" width="200" />  

### Step 3: Enter the default pin (123456)  
<img src="https://github.com/user-attachments/assets/9e90dffd-92fd-4cb9-a31f-88fcbc3bf7c7" width="200" />  

### Step 4: Enter a username  
This just tells other people listening who you want to be known as.<br>  
<img src="https://github.com/user-attachments/assets/75191872-247a-4a2b-8d15-5a59664883cd" width="200" />  

### Step 5: Enter Configuration menu  
Click "Configuration" at the top-right.<br>  
<img src="https://github.com/user-attachments/assets/9dcd2f35-ef3e-4a19-99dc-2b5735ba2db9" width="200" />  

### Step 6: Change your pin and press "Set" (must be 6 digits)  
This pin not only prevents unauthorized people from accessing your device, it also sets encryption keys
for the BLE connection, encrypting messages between your phone and LoRaBlue device.<br>  
<img src="https://github.com/user-attachments/assets/4f979734-c947-44db-b803-38b7262a63b0" width="200" />
<img src="https://github.com/user-attachments/assets/ae3b49f8-aded-4428-8866-5df3ac0caf8c" width="200" />
<img src="https://github.com/user-attachments/assets/e34d304d-6eab-4bb5-a435-5cc3091ad7b8" width="200" />
<img src="https://github.com/user-attachments/assets/c5efaa6b-b90b-460e-9d0a-f65c4a2dd4c1" width="200" />  

### Step 7: Decide whether to use "Text Mode" or "Audio Mode"  
Here we recieved a message from username "Brad". You can send messages just like you would a text message<br>  
<img src="https://github.com/user-attachments/assets/e63a9d65-a39b-4c1f-a25e-70a0753e1fdf" width="200" />
<img src="https://github.com/user-attachments/assets/b2e121ec-a2ee-4373-9a1b-ee5cefd8a0cd" width="200" />
<img src="https://github.com/user-attachments/assets/a0dd82a4-da24-45d6-a13b-cf568d1aef4c" width="200" />  

### Step 8: If Audio Mode is desired, simply select it.  
To send a message, simply hold down on the microphone and speak. Let up on the microphone when you're finished.
The app will convert this into text, encrypt it using the "Encryption Passphrase", and send it out. It is highly
resistant to noise, so it's perfect for a noisy truck. It also uses your phone's microphone and media output source rather than 
any connected headset, so it's independent of any call you may be in. When a message is recieved, it will convert the recieved 
text into audible speech and read it out loud to you. Whether you're using Text or Audio mode doesn't matter to other devices. 
They recieve messages in their preferred way. If you didn't quite understand a message recieved in Audio mode, you can switch to 
text mode and find it there. Once read, simply go back to Audio mode.<br>  
<img src="https://github.com/user-attachments/assets/587196b3-4c2c-473f-bf47-d2cfefda6a74" width="200" />  

### Step 9: Basic configuration.  
<b>&#x2022; Led Setting: </b>This allows you to change when the two leds are turned on or not. By default, the blue light will turn
on when a bluetooth connection is activated. Here you can switch it off. You can also turn on the amber light, which will flash when 
radio messages are being sent or recieved.<br><br>

<b>&#x2022; Max Echo: </b>This sets when to echo a message. Each time a message is echoed by another device, the count is increased 
in the message being sent out. So if two other devices have echoed the message already, you would recieve a count of 2. If your 
"Max Echo" setting is 5, you would echo this message. If your Max Echo was set to say 0 or 1, you would not echo the message
because it would be more than your Max Echo setting. Setting Max Echo to 0 means you will never echo a message.<br><br>

<b>&#x2022; Encryption Passphrase: </b>This sets the encryption key to use for radio messages. By default, "open sesame" is the passphrase.
If you change this passphrase, anybody using a different passphrase will not be able to understand your messages, and you will not be able
to understand theirs. Using the passphrase "open sesame" essentially makes your messages public to all other LoRaBlue devices using it.
If you change it to something like "for green trucks only", only those using that passphrase will be able to understand each other.
You would not be able to understand devices using the public "open sesame" passphrase and they will not be able to understand you.
This is because you are using different encryption keys. The only way to decrypt messages is by knowing the passphrase they are using. It is
important to note that this does not effect Echoes. Messages are echoed even if the device that recieves the message cannot understand the
encryption. Two devices recieving messages using different passphrases will still echo the messages, as long as the echo count of the recieved
message is not greater than their Max Echo setting.<br>  

<img src="https://github.com/user-attachments/assets/e0509d3c-c746-4a1e-ad8a-28efbcd51705" width="200" />  

### Step 10: Advanced Radio Settings.  
This is not really for the average user. This is for playing around with the LoRa technology itself. If you know what you're doing and coordinate
with another device, you can greatly extend the maximum range of the radio between the devices. Changing anything here will likely mean you cannot
communicate with other devices not using these exact settings. It's only recommended to change these settings if you know what you're doing and
the other device does as well. A possible reason for doing this is if you are out in a heavily wooded area and want to increase the range at which 
devices can communicate. DefiChat settings are not intended to reach maximum range because the longer the range, the longer it takes to send the 
message. Default DefiChat settings are a balance of all factors to make the network function at it's best. Don't be afraid to play around with these 
settings though, as you can simply click "Set Radio Parameters Back To DefiChat Default" and you will be back in the DefiChat network.<br>  
<img src="https://github.com/user-attachments/assets/7d1e7d2a-5556-4a54-abc1-5608ccbc6a77" width="200" />  
