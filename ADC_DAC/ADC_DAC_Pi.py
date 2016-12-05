'''
Adaptation of the DAQT7_Objective library for the Raspberry Pi.

Author: Damon Hutley
Date: 21st November 2016
'''


from ABE_ADCDACPi import ADCDACPi
import time
import numpy as np
import sys


class DetectPi:
	
	def __init__(self):
		'''
		Initialise ADC-DAC Pi. The DAC gain factor is set to 1, which allows 
		the DAC output voltage to be set between 0 and 2.048 V. The DAC gain 
		factor may also be set to 2, which gives an output range of 0 to 3.3 V.
		'''
		
		# Create instance of ADCDACPi, with gain set to 1
		self.adcdac = ADCDACPi(1)
		
		# Set reference voltage to 3.3 V
		self.adcdac.set_adc_refvoltage(3.3)
		
		return
		
	
	def getDetails(self):
		'''
		Return details of ADC_DAC Pi. This function does not currently return 
		any details, but this may change later.
		'''
		
		return
		
	
	def writePort(self, Port, Volt):
		'''
		Write values to a DAC pin. Values may be written to either channel 1 
		or channel 2. The maximum voltage is specified by the gain factor.
		Note: Setting a voltage of 5 V will return an error for exceeding 
		the maximum voltage.
		'''
		
		# Convert DAQT7 DAC ports to DAC Pi channels
		if Port = "DAC0":
			channel = 1
		elif Port = "DAC1":
			channel = 2
		
		# Set DAC output voltage <Volt> on channel <channel>
		self.adcdac.set_dac_voltage(channel, Volt)
		
		return
		
	
	def readPort(self, Port):
		'''
		Read values from an ADC pin. Values may be read from either channel 1 
		or channel 2. 
		'''
		
		# Convert DAQT7 AIN ports to ADC Pi channels
		if Port = "AIN0":
			channel = 1
		elif Port = "AIN1":
			channel = 2
		
		# Read voltage from channel <channel> in single ended mode
		voltRead = self.adcdac.read_adc_voltage(channel, 0)
		
		return np.float(voltRead), time.time()
		
	
	def streamRead(self, scanRate, scansPerRead, Port):
		'''
		Read analogue input values from an ADC pin, in stream mode.
		'''
		
		# Initialise array and time values
		Read = [0, 1, 2]
		StartingMoment = 0
		FinishingMoment = 0
		scansPerRead = int(scansPerRead)
		
		# Determine timing characteristics
		duration = scansPerRead/scanRate
		dt = 1/scanRate
		StartingMoment = time.time()
		
		# Loop for the duration
		while (time.time()-StartingMoment) < duration:
			# Read the ADC value and append to an array
			voltRead = self.readPort(Port)[0]
			Read[0].append(voltRead)
			
			# Wait for the program to run at the correct frequency
			lastReadTime = readTime
			readTime = time.time()
			if readTime - lastReadTime < dt:
				time.sleep(dt - readTime + lastReadTime)
			
		# Calculate and print elapsed time
		FinishingMoment = time.time()
		print ('Elapsed time %f seconds' %(FinishingMoment - StartingMoment))
		
		return Read, StartingMoment, FinishingMoment
		
	
	def close(self):
		'''
		Close ADC-DAC Pi.
		'''
		
		pass


		
