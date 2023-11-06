import pyautogui
import serial
import argparse
import time
import logging
from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
from ctypes import cast, POINTER

class MyControllerMap:
    def __init__(self):
        self.button = {'A': 'L', 'B': 'J', 'C': 'K', 'D': 'F'} # Fast forward (10 seg) pro Youtube

class MyVolumeMap:
    def __init__(self):
        self.volume_dB = {
        0.00: -51,
        0.05: -40,
        0.10: -30.848,
        0.15: -26,
        0.20: -22.477,
        0.25: -20,
        0.30: -17.111,
        0.35: -15,
        0.40: -13.152,
        0.45: -11,
        0.50: -10.015,
        0.55: -8.5,
        0.60: -7.415,
        0.65: -6,
        0.70: -4.991,
        0.75: -4,
        0.80: -3.26,
        0.85: -2,
        0.90: -1.381,
        0.95: -0.6,
        1.0: 0
    }

class SerialControllerInterface:
    # Protocolo
    # byte 1 -> Botão 1 (estado - Apertado 1 ou não 0)
    # byte 2 -> EOP - End of Packet -> valor reservado 'X'

    def __init__(self, port, baudrate):
        self.ser = serial.Serial(port, baudrate=baudrate)
        self.mapping = MyControllerMap()
        self.mapping_v = MyVolumeMap()
        self.incoming = '0'
        pyautogui.PAUSE = 0  ## remove delay

        global devices, interface, volume_control
        devices = AudioUtilities.GetSpeakers()
        interface = devices.Activate(IAudioEndpointVolume._iid_, 7, None)  # Use 7 para CLSCTX_ALL
        volume_control = cast(interface, POINTER(IAudioEndpointVolume))

    def update(self):
        ## Sync protocol
        print("update")
        print("Incoming: ", self.incoming)
        while self.incoming != b'X':
            self.incoming = self.ser.read()
            logging.debug("Received INCOMING: {}".format(self.incoming))
            print("READING")

        data = self.ser.read()
        logging.debug("Received DATA: {}".format(data))

        #Pula linha
        print("")
        print("data: ", data)

        if data == b'W':
            self.ser.write(b'W')
            print("")
            print("HANDSHAKE")
            print("")
        elif data == b'1':
            print("datab1")
            logging.info("KEYDOWN A")
            pyautogui.keyDown(self.mapping.button['A'])
            logging.info("KEYUP B")
            pyautogui.keyUp(self.mapping.button['B'])
            logging.info("KEYUP C")
            pyautogui.keyUp(self.mapping.button['C'])
            logging.info("KEYUP D")
            pyautogui.keyUp(self.mapping.button['D'])
        elif data == b'0':
            print("datab0")
            logging.info("KEYUP A")
            pyautogui.keyUp(self.mapping.button['A'])
            logging.info("KEYUP B")
            pyautogui.keyUp(self.mapping.button['B'])
            logging.info("KEYUP C")
            pyautogui.keyUp(self.mapping.button['C'])
            logging.info("KEYUP D")
            pyautogui.keyUp(self.mapping.button['D'])
        elif data == b'5':
            logging.info("KEYUP A")
            pyautogui.keyUp(self.mapping.button['A'])
            logging.info("KEYUP B")
            pyautogui.keyUp(self.mapping.button['B'])
            logging.info("KEYUP C")
            pyautogui.keyUp(self.mapping.button['C'])
            logging.info("KEYDOWN D")
            pyautogui.keyDown(self.mapping.button['D'])
        elif data == b'7':
            logging.info("KEYUP A")
            pyautogui.keyUp(self.mapping.button['A'])
            logging.info("KEYDOWN B")
            pyautogui.keyDown(self.mapping.button['B'])
            logging.info("KEYUP C")
            pyautogui.keyUp(self.mapping.button['C'])
            logging.info("KEYUP D")
            pyautogui.keyUp(self.mapping.button['D'])
        elif data == b'9':
            logging.info("KEYUP A")
            pyautogui.keyUp(self.mapping.button['A'])
            logging.info("KEYUP B")
            pyautogui.keyUp(self.mapping.button['B'])
            logging.info("KEYDOWN C")
            pyautogui.keyDown(self.mapping.button['C'])
            logging.info("KEYUP D")
            pyautogui.keyUp(self.mapping.button['D'])

        else:
            volume_control.SetMasterVolumeLevel(data, None)
        self.incoming = self.ser.read()


class DummyControllerInterface:
    def __init__(self):
        self.mapping = MyControllerMap()

    def update(self):
        pyautogui.keyDown(self.mapping.button['A'])
        time.sleep(0.1)
        pyautogui.keyUp(self.mapping.button['A'])
        logging.info("[Dummy] Pressed A button")
        time.sleep(1)


if __name__ == '__main__':
    interfaces = ['dummy', 'serial']
    argparse = argparse.ArgumentParser()
    argparse.add_argument('serial_port', type=str)
    argparse.add_argument('-b', '--baudrate', type=int, default=9600)
    argparse.add_argument('-c', '--controller_interface', type=str, default='serial', choices=interfaces)
    argparse.add_argument('-d', '--debug', default=False, action='store_true')
    args = argparse.parse_args()
    if args.debug:
        logging.basicConfig(level=logging.DEBUG)

    print("Connection to {} using {} interface ({})".format(args.serial_port, args.controller_interface, args.baudrate))
    if args.controller_interface == 'dummy':
        controller = DummyControllerInterface()
    else:
        controller = SerialControllerInterface(port=args.serial_port, baudrate=args.baudrate)

    while True:
        controller.update()