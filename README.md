# Camera Cooler
Cooling solution for Svbony SV7505C astronomy camera.

More about the project on may [website](https://luka.slibar.si).

---

This repository contains multiple components, each under its own license.

- Firmware - GPL-3.0
- Hardware - CERN-OHL-S-2.0
- Software - GPL-3.0

See the ***project-folder/LICENSES*** directory for the full license texts.

---

This project is an atempt to actively cool the Svbony SV7505C astronomy camera. The cooling is done to achive better picture quality with cooling the camera sensor through the bottom pcb. With this setup we have achived 20 degrees of temperature diference but are still trying to improve the resoluts (improve insulation). A big problem is condensation and great care needs to be taken to avoid short circuit on the camera bottom PCB (can be placed upside down or sealed with gum or something simmilar). The cooling system is powered by 19V - 25V DC power supply that can supply at least 6A of current. We are developing the REV B pcb that will eliminate some problems with wrong connectors and add the ability to completely disable the paltier element (TEC). Currently there are micro python example programs to control the device but we are developing a driver in C++ to make it faster ans more efficient. There is also a simple GUI but the USB port needs to be configured manually.

## Instructions
Firstly the camera needs to be disassemlied and its top cover and upper PCB (connected with a short ribbon cable) need to be removed. Then the aluminium cooling block and the 3d printed cooling block holder need to be put together. On the smaller side of the cooling block place a thermal pad, which is cut in the same shape as that side of the block. on the side there is a small hole for the thermometer that must have soldered on wires. A longer ribbon cable should be inserted into the bottom PCB. You can put a bit of thermal paste in between the thermometer and the block to ensure more accurate readings. This assembly can now be fitted into the camera enclosure in a way that screehole positions match.  On the other hand the main fen and the heatsink can be screw together then the fen cable can be routed through the upper hole into the pcb enclosure and the whole fen and heasink assembly can be fitted into the main enclosure. On the bottom of the heatsink paltire element needs to be fiited and thermal paste should be aplied between them. Them the camera and the cooler can be joined togethe with thermal paste. The upper camera PCB and TEC driver custom PCB have their place in the enclosure on the side. Additionally another fen can be fitted to cool the enclosure.

### Additional Parts

[*list here]

### Electrical Connections

[*photo here]

### Mechanical Assembly

---

![Assembly of 3d printed parts](camera-cooler-assembly.gif)
*Assembly of 3d printed parts and pcb*