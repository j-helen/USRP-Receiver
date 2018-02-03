#Implementation of USRP Receiver Block in System Studio
OBJECTIVE
To develop a prototyping platform with a combination of sensible hardware and software. The Software Defined Radio (SDR) used in this project is a Universal Software Radio Peripheral (USRP) N Series and the software used is System Studio.
The objective of the project is to:
1. Interface the USRP device with the host system.
2. Develop a USRP receiver block in System Studio that reads samples from USRP device.
3. To display and analyze the Power Spectral Density (PSD) and constellation plots of the received spectrum.

SOLUTION
1. usrp_src.cpp
The solution was implemented by writing C++ Wrapper files that links the USRP source code to the System Studio interface. The preliminary function of the wrapper file was to initialize the device, start the device, read the I and Q samples and to stop the device.

PROJECT FOLLOW-UP
Due to the extensible nature of the USRP device, there is potential for future work in this area. The wrapper files are an advantage for developers as additional coding is simplified. A prospective follow-up of this project is to develop a re-sampler block in System Studio to facilitate the processing of UMTS signals.
