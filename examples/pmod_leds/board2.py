# Sylvain Lefebvre - @sylefeb - 2024

from skidl import *
from kinet2pcb import *

set_default_tool(KICAD7)

# add a half-PMOD connector
pmod = Part(ref='PMOD', 
            lib='Connector_Generic', name='Conn_01x06',
            footprint="Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Horizontal")

# setup nets
vcc3v3, gnd = Net('VCC3V3'), Net('GND')
pmod['p5'] += gnd
pmod['p6'] += vcc3v3

# add the LEDs
led0 = Part(ref="led0", 
            lib='Device', name='LED', 
            footprint='LED_SMD:LED_0805_2012Metric_Pad1.15x1.40mm_HandSolder')
led1,led2,led3 = led0 * 3
# make sure each LED has a different ref (skidl-updater requires unique refs)
led1.ref  = "led1"
led2.ref  = "led2"
led3.ref  = "led3"

# generate the board
generate_pcb(fp_libs=[".","C:\\Program Files\\KiCad\\7.0\\share\\kicad\\footprints"])
