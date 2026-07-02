Full Speed device @ 2 (0x02110000): .............................................   Composite device: "PIONEER CDJ-850"
    Port Information:   0x0018
           Not Captive
           External Device
           Connected
           Enabled
    Number Of Endpoints (includes EP0):   
        Total Endpoints for Configuration 1 (current):   5
    Device Descriptor   
        Descriptor Version Number:   0x0200
        Device Class:   0   (Composite)
        Device Subclass:   0
        Device Protocol:   0
        Device MaxPacketSize:   64
        Device VendorID/ProductID:   0x08E4/0x0157   (Pioneer Corporation)
        Device Version Number:   0x0112
        Number of Configurations:   1
        Manufacturer String:   1 "PIONEER Corporation."
        Product String:   2 "PIONEER CDJ-850"
        Serial Number String:   0 (none)
    Configuration Descriptor (current config)   
        Length (and contents):   198
            Raw Descriptor (hex)    0000: 09 02 C6 00 05 01 00 C0  00 09 04 00 00 00 01 01  
            Raw Descriptor (hex)    0010: 00 03 09 24 01 00 01 1E  00 01 01 0C 24 02 01 01  
            Raw Descriptor (hex)    0020: 01 00 02 03 00 00 00 09  24 03 02 01 03 00 01 00  
            Raw Descriptor (hex)    0030: 09 04 01 00 00 01 02 00  00 09 04 01 01 01 01 02  
            Raw Descriptor (hex)    0040: 00 00 07 24 01 01 00 01  00 0E 24 02 01 02 02 10  
            Raw Descriptor (hex)    0050: 02 44 AC 00 80 BB 00 09  05 01 01 C8 00 01 00 00  
            Raw Descriptor (hex)    0060: 07 25 01 01 00 00 00 09  04 02 00 00 01 01 00 04  
            Raw Descriptor (hex)    0070: 09 24 01 00 01 09 00 01  03 09 04 03 00 01 01 03  
            Raw Descriptor (hex)    0080: 00 05 07 24 01 00 01 24  00 09 24 03 01 01 01 02  
            Raw Descriptor (hex)    0090: 01 00 06 24 02 02 02 00  09 05 83 02 40 00 00 00  
            Raw Descriptor (hex)    00a0: 00 05 25 01 01 01 09 04  04 00 02 03 00 00 06 09  
            Raw Descriptor (hex)    00b0: 21 10 01 00 01 22 34 00  07 05 06 03 24 00 01 07  
            Raw Descriptor (hex)    00c0: 05 87 03 14 00 01 
        Number of Interfaces:   5
        Configuration Value:   1
        Attributes:   0xC0 (self-powered)
        MaxPower:   0 mA
        Interface #0 - Audio/Control ..............................................   "PIONEER CDJ-850"
            Alternate Setting   0
            Number of Endpoints   0
            Interface Class:   1   (Audio)
            Interface Subclass;   1   (Control)
            Interface Protocol:   0
            Audio Control Class Specific Header   
                Descriptor Version Number:   01.00
                Class Specific Size:   30
                Number of Audio Interfaces:   1
                Audio Interface Number:   1
                Dump Contents (hex):   09 24 01 00 01 1E 00 01 01 
            Audio Class Specific Input Terminal   
                Terminal ID:   1
                Input Terminal Type:   0x101 (USB streaming)
                OutTerminal ID:   0 [NONE]
                Number of Channels:   2
                Spatial config of channels:   0000000000000011
                                  ^.  Left Front
                                 ^..  Right Front
                String index for first logical channel:   0
                Terminal Name String Index:   0 [NONE]
            Audio Class Specific Output Terminal   
                Terminal ID:   2
                Output Terminal Type:   0x301 (Speaker)
                InTerminal ID:   0 [NONE]
                Source ID:   1
                Terminal Name String Index:   0 [NONE]
        Interface #1 - Audio/Streaming   
            Alternate Setting   0
            Number of Endpoints   0
            Interface Class:   1   (Audio)
            Interface Subclass;   2   (Streaming)
            Interface Protocol:   0
        Interface #1 - Audio/Streaming (#1)   
            Alternate Setting   1
            Number of Endpoints   1
            Interface Class:   1   (Audio)
            Interface Subclass;   2   (Streaming)
            Interface Protocol:   0
            Audio Control Class Specific Header   
                Audio Stream General   
                    Endpoint Terminal ID:   1
                    Delay:   0 frames     (Delay NOT SUPPORTED)
                    Format Tag:   0x0001 (PCM)
            Audio Class Specific Audio Data Format   
                Audio Stream Format Type Desc.   
                    Format Type:   1 PCM
                    Number Of Channels:   2 STEREO
                    Sub Frame Size:   2
                    Bit Resolution:   16
                    Sample Frequency Type:   0x02 (Discrete)
                    Sample Frequency:   44100 Hz
                    Sample Frequency:   48000 Hz
            Endpoint 0x01 - Isochronous Output   
                Address:   0x01  (OUT)
                Attributes:   0x01  (Isochronous no synchronization data endpoint)
                Max Packet Size:   200
                Polling Interval:   1 ms
            Class-Specific AS Audio EndPoint   
                Attributes:   0x01  Sample Frequency,  
                bLockDelayUnits:   0x00  (UNDEFINED)
                wLockDelay:   0 
        Interface #2 - Audio/Control ..............................................   "PIONEER CDJ-850 MIDI"
            Alternate Setting   0
            Number of Endpoints   0
            Interface Class:   1   (Audio)
            Interface Subclass;   1   (Control)
            Interface Protocol:   0
            Audio Control Class Specific Header   
                Descriptor Version Number:   01.00
                Class Specific Size:   9
                Number of Audio Interfaces:   1
                Audio Interface Number:   3
                Dump Contents (hex):   09 24 01 00 01 09 00 01 03 
        Interface #3 - Audio/Streaming ..............................................   "USB MIDI Interface2"
            Alternate Setting   0
            Number of Endpoints   1
            Interface Class:   1   (Audio)
            Interface Subclass;   3   (Streaming)
            Interface Protocol:   0
            Uknown Interface SubClass Type   
            Uknown Interface SubClass Type   
            Uknown Interface SubClass Type   
            Endpoint 0x83 - Bulk Input   
                Address:   0x83  (IN)
                Attributes:   0x02  (Bulk)
                Max Packet Size:   64
                Polling Interval:   0 ms
            Class-Specific AS Audio EndPoint   
                Attributes:   0x01  Sample Frequency,  
                bLockDelayUnits:   0x01  (Milliseconds)
                wLockDelay:   1033 ms
        Interface #4 - HID ..............................................   "PIONEER CDJ-850 HID"
            Alternate Setting   0
            Number of Endpoints   2
            Interface Class:   3   (HID)
            Interface Subclass;   0
            Interface Protocol:   0
            HID Descriptor   
                Descriptor Version Number:   0x0110
                Country Code:   0
                Descriptor Count:   1
                Descriptor 1   
                    Type:   0x22  (Report Descriptor)
                    Length (and contents):   52
                        Raw Descriptor (hex)    0000: 06 A0 FF 09 01 A1 01 09  02 A1 00 06 A1 FF 09 03  
                        Raw Descriptor (hex)    0010: 09 04 15 80 25 7F 35 00  45 FF 75 08 95 14 81 02  
                        Raw Descriptor (hex)    0020: 09 05 09 06 15 80 25 7F  35 00 45 FF 75 08 95 24  
                        Raw Descriptor (hex)    0030: 91 02 C0 C0 
                    Parsed Report Descriptor:   
                          Usage Page    (Vendor defined 160) 
                          Usage 1 (0x1)    
                              Collection (Application)    
                                Usage 2 (0x2)    
                                    Collection (Physical)    
                                      Usage Page    (Vendor defined 161) 
                                      Usage 3 (0x3)    
                                      Usage 4 (0x4)    
                                      Logical Minimum.........    (-128)  
                                      Logical Maximum.........    (127)  
                                      Physical Minimum........    (0)  
                                      Physical Maximum........    (-1)  
                                      Report Size.............    (8)  
                                      Report Count............    (20)  
                                      Input...................   (Data, Variable, Absolute, No Wrap, Linear, Preferred State, No Null Position, Bitfield) 
                                      Usage 5 (0x5)    
                                      Usage 6 (0x6)    
                                      Logical Minimum.........    (-128)  
                                      Logical Maximum.........    (127)  
                                      Physical Minimum........    (0)  
                                      Physical Maximum........    (-1)  
                                      Report Size.............    (8)  
                                      Report Count............    (36)  
                                      Output..................   (Data, Variable, Absolute, No Wrap, Linear, Preferred State, No Null Position, Nonvolatile, Bitfield) 
                                    End Collection     
                              End Collection     
            Endpoint 0x06 - Interrupt Output   
                Address:   0x06  (OUT)
                Attributes:   0x03  (Interrupt)
                Max Packet Size:   36
                Polling Interval:   1 ms
            Endpoint 0x87 - Interrupt Input   
                Address:   0x87  (IN)
                Attributes:   0x03  (Interrupt)
                Max Packet Size:   20
                Polling Interval:   1 ms

