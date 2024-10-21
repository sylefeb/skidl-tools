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

# add the resistors
led0_r = Part(ref="led0_r",value="270",lib="Device", name='R', footprint='Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder')
led1_r,led2_r,led3_r = led0_r * 3
led1_r.ref  = "led1_r"
led2_r.ref  = "led2_r"
led3_r.ref  = "led3_r"

# connect everything!
_ = gnd & led0_r & led0 & pmod['p1']
_ = gnd & led1_r & led1 & pmod['p2']
_ = gnd & led2_r & led2 & pmod['p3']
_ = gnd & led3_r & led3 & pmod['p4']

# generate the board
generate_pcb(fp_libs=[".","C:\\Program Files\\KiCad\\7.0\\share\\kicad\\footprints"])
