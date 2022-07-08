# VaLas1.0
Manual 722.6 controller, with support for 0,91" OLED display which is easily placed on w124 gauge cluster for mimicking oem appearance.  
V1.1 uses bigger 128x64 0.96" OLED display

<br/>

Goal for this project was to create very simple and cheap controller, which is also user friendly to adjust and drive daily. 
This contoller is now being used on 4 different vehicles with varying power output from estimated 250-500whp & +700nm of torque. 
My personal vehicle has been driven past +10 000km/10 months period, basically trouble free and daily driving with same code.  
My vehicle specs: om606+8mm pump, he531 turbocharger, 722.6 from 270 cdi(+500k odo).  

<br/>

Controller has gears 1-5+.  
Gears 1-5 are normal gears, and 5+ is overdrive gear which will lock turbine and reduce line pressure in transmission for reduced consumption when coasting +80km/h.  
OLED display helps knowing which gear you are, since this controller has no means to know it by its own.  

<br/>

How to use V1.1:  
1. When vehicle is started, use relay to turn controller on.
2. Controller should now start, displaying P, R, N, or D. If in D, after a while D2 will appear which means controller is in gear 2.
3. Now you can use shifter freely between P-R-N-D, and use +/- in D.

When in position D, you can use 2 electric switches for contolling gears 1-5+.  
Shifts are completed as whole, and cannot be overlapped even if switch is stuck at either +/- position.  

<br/>

Code and adjustments are from my vehicle, and should work smoothly even in paved surfaces. Shifting from 3-4 is meant for undrilled gearbox valve plate. 
After drilling valve plate, you can use base adjustments of 3-4 shift and tune from that.  

<br/>

Transmission has 6 solenoids controlled by arduino:  
- Shift pc     : pressure apllied during shifts :adjustment range 0-255 0=low pressure, pwm
- Mod pc       : pressure apllied when in gear  :adjustment range 0-255 0=low pressure, pwm
- Tcc          : engages turbine lock           :adjustment range 0-255 0=low pressure, pwm
- Shift 1-2,4-5: applied during shifts          :adjustment range LOW-HIGH
- Shift 2-3    : applied during shifts          :adjustment range LOW-HIGH
- Shift 3-4    : applied during shifts          :adjustment range LOW-HIGH

<br/>

Made by Toni Lassila  
Coding and pcb made with Teemu Vahtola  
V1.1 by IJskonijn  

If you are looking for more advanced controller with automatic shifts, speedo and many more features, please take a look https://github.com/mkovero/7226ctrl by Markus Kovero.  

04/2018 First tests and drives in completed in 04/2018.  
06/2018 0,91" OLED added.  
6/2018 Latest adjustments to controller code.  
