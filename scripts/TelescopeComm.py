import serial
import asyncio
import copy

# Telescope serial communication class
class TelescopeComm:
    # When the mount is done, it returns a '#' byte
    STOPCHAR = b'#'
    STOPBYTE = ord(b'#')

    # Take in the device port as ctor arg
    def __init__(self, strDevice):
        # Create serial object
        self.ser = serial.Serial()
        self.ser.port = strDevice
        self.ser.baudrate = 9600
        self.ser.parity = serial.PARITY_NONE
        self.ser.stopbits = serial.STOPBITS_ONE

        # Set our cur speed to zero
        self.nAltSpeed = 0
        self.nAzmSpeed = 0

        # Open the serial port, handle exception if unable to open
        try:
            self.ser.open()
        except (FileNotFoundError, serial.serialutil.SerialException) as e:
            raise RuntimeError('Error: Unable to open serial port!')

        # Read back an echo value to test
        echoInput = bytearray([ord('K'), 69])
        expectedResp = bytearray([69, TelescopeComm.STOPBYTE])
        if self._executeCommand(echoInput) != expectedResp:
            raise RuntimeError('Error: unable to communicate with telescope!')

        # Success
        print('Telescope found at port', strDevice)

    # Slew at a variable rate (nSpeed in arcseconds)
    def slewVariable(self, strID, nSpeed):
        # Command needs high precision and low precision separate
        trackRateHigh = int(4 * abs(nSpeed)) >> 8
        trackRateLow = int(4 * abs(nSpeed)) % 256

        # Positive / Negative altitude / azimuth
        cmdDict = {
            'Alt+' : [3, 17, 6, trackRateHigh, trackRateLow],
            'Alt-' : [3, 17, 7, trackRateHigh, trackRateLow],
            'Azm+' : [3, 16, 6, trackRateHigh, trackRateLow],
            'Azm-' : [3, 16, 7, trackRateHigh, trackRateLow]
        }
        return self._slewCommand(strID, cmdDict, nSpeed < 0)

    # sends a slew command to the mount
    # strID should be Alt or Azm, nSpeed is a signed int
    def slewFixed(self, strID, nSpeed):
        # Positive / Negative altitude / azimuth
        cmdDict = {
            'Alt+' : [2, 17, 36, abs(nSpeed)],
            'Alt-' : [2, 17, 37, abs(nSpeed)],
            'Azm+' : [2, 16, 36, abs(nSpeed)],
            'Azm-' : [2, 16, 37, abs(nSpeed)]
        }
        return self._slewCommand(strID, cmdDict, nSpeed < 0)

    # Implementation of command for fixed and variable rates
    # assumes it will get a dict with positive/negative denoted by last char
    def _slewCommand(self, strID, cmdDict, bNeg):
        # Create local copy of string, append '+' or '-' depending
        strID = copy.copy(strID)
        if strID == 'Alt' or strID == 'Azm':
            # Store local speed values as well
            #if strID == 'Alt':
            #    self.nAltSpeed = nSpeed
            #else:
            #    self.nAzmSpeed = nSpeed

            # Append '+' or '-' depending on sign
            strID += '-' if bNeg else '+'

            # Create command bytes by appending dict value
            try:
                cmdSlew = [ord('P')] + [0] * 6
                cmdGuts = cmdDict[strID]
                cmdSlew[1:len(cmdGuts)] = cmdGuts
                cmdSlew = bytearray(cmdSlew)
            except KeyError:
                print('Where the fuck was it?')

            # execute command and return True (should check resp)
            resp = self._executeCommand(cmdSlew)
            return True

        # Return false if not handled properly
        return False

    def GetPosition(self):
        # Send get AZM-ALT command (not precise)
        resp = self._executeCommand(bytearray([ord('Z')]))
        # Sanity check
        if len(resp) == 0 or resp[-1] != TelescopeComm.STOPBYTE:
            raise RuntimeError('Error: Invalid response from get AZM-ALT command!')
        # Strip stop byte and split by comma and unpack
        # values are two 16 bit ints as hex string
        print(resp)
        return [int(r, 16) for r in resp[0:len(resp)-1].split(',')]

    # Internal function that sends a serial
    # command and waits for the stop byte
    def _executeCommand(self, cmd):
        # Send the command
        self.ser.write(cmd)

        # Read in the response until chResp is the stop byte
        bufResp = []
        chResp = None

        # Only try to read 100 chars
        for i in range(100):
            bufResp.append(self.ser.read())
            if bufResp[-1] == TelescopeComm.STOPCHAR:
                break
        # We "timed out"
        else:
            raise RuntimeError('Error: stop char not recieved!')

        # Return the response
        return bytearray(ord(b) for b in bufResp)

    # class method
    def FactoryFunc(strDevice):
        return TelescopeComm(strDevice)
