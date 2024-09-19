# virtjs-btn

Based on [virtjs](https://github.com/sambazley/virtjs) by Sam Bazley.

Work around for [joystick detection in Proton/SDL](https://github.com/ValveSoftware/Proton/issues/5126), with specific changes to make it useful in Star Citizen.

* Instead of leaving the vendor, product and version values undefined, copies the first two from the cloned device and sets the version to 1.
* Creates a virtual button for every absolute axis.
* When an axis goes over 10% of its maximum value, the corresponding virtual button is pressed.

### Usage

```
make
./virtjs-btn /dev/input/by-id/usb-FANATEC_FANATEC_CSL_Pedals_LCK-event-if00
```
