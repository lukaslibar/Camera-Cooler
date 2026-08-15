# Camera Cooler
Cooling solution for Svbony SV7505C astronomy camera.

More about the project on my [website](https://luka.slibar.si).

---

This repository contains multiple components, each under its own license.

- Firmware - GPL-3.0
- Hardware - CERN-OHL-S-2.0
- Software - GPL-3.0

See the ***project-folder/LICENSES*** directory for the full license texts.

---

This project is an attempt to actively cool the Svbony SV7505C astronomy camera. The cooling is intended to achieve better image quality by cooling the camera sensor through the bottom PCB. With this setup, we have achieved a temperature difference of 20 °C, but we are still trying to improve the results through better insulation. A major problem is condensation, and great care must be taken to avoid short circuits on the camera's bottom PCB. The PCB can either be placed upside down or sealed with rubber or a similar material. The cooling system is powered by a 19–25 V DC power supply capable of supplying at least 6 A of current.

We are currently developing the REV B PCB, which will eliminate some issues with incorrect connectors (fen connectors) and add the ability to completely disable the Peltier element (TEC). At the moment, there are MicroPython example programs for controlling the device, but we are developing a C++ driver to make the system faster and more efficient. There is also a simple GUI, but the USB port currently needs to be configured manually.

## Instructions
First, the camera needs to be disassembled. Its top cover and upper PCB, which is connected with a short ribbon cable, need to be removed. The aluminium cooling block and the 3D-printed cooling block holder can then be assembled.

Place a thermal pad on the smaller side of the cooling block. The thermal pad should be cut to the same shape as that side of the block. There is a small hole on the side of the block for the temperature sensor, which must have wires soldered to it. A longer ribbon cable should be connected to the bottom PCB. A small amount of thermal paste can be applied between the temperature sensor and the cooling block to ensure more accurate temperature readings.

The entire assembly can now be fitted into the camera enclosure so that the screw holes are properly aligned. The main fan and heatsink can then be screwed together, and the fan cable can be routed through the upper hole into the PCB enclosure. The complete fan and heatsink assembly can then be fitted into the main enclosure.

The Peltier element (TEC) needs to be fitted to the bottom of the heatsink, with thermal paste applied between the two surfaces. There are holes for its wires alongside a slit for the longer ribbon cable. The camera and the cooler can then be joined together using thermal paste between their contact surfaces. The upper camera PCB and the custom TEC driver PCB have designated positions inside the enclosure on the side. An additional fan can also be installed to cool the enclosure.

### Additional Parts

[*list here]

### Electrical Connections

[*photo here]

### Mechanical Assembly

---

![Assembly of 3d printed parts](camera-cooler-assembly.gif)
*Assembly of 3d printed parts and pcb*