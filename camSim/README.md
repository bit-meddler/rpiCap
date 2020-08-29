# Camera Simulator

## Intro ##
This is a standalone simulation of an rpiCam.  It will respond correctly to
imperatives (Setting C&C values, like 'Strobe Power', 'Shutter durration',
'threshold value'; and executing actions, like 'Start Streaming',
'Take picture').  It will stream out Centroid data with a correct packet
formatting.

Currently it is equiped with Simulated data for a 10 Camera system with a Human
ROM and a Calibration conducted with a 5 Marker T-Wand.  Several simCams can be
syncronized with the "multicastTx.py" program in /sketches/ a simSync Program is
in the works.

## Usage ##
To run several simCams you need to place them on separate "Machines" on account
of binding a specific network port.  You can do this with physical Computers,
Say using the 4 rpi 3Bs you bought for this project, or you can use a VM with a
super minimal Linux image and make several instances.  These methods will allow
simulation of a small mocap system, and give the midget C&C Code a good workout 
and help us on the road to all the MoCap Problems, like Calibration, Tracking,
Scaling, IK, and Realtime...

`camSim$ python3 dataResponder.py --help`

Will show the commandline args you might want to configure.


## Future Ideas ##
We could simulate some data, and give ground-truth data to a number of these
cameras to stream out.  Given a working MoCap system, this data would be
interpreted correctly.

We could simulate this data by getting a C3D of a Wand-Wave, or a ROM into Maya,
and parenting spheres to the locators, and rendering out 'virtual MoCap' Images
from any number of synthetic positions.  This synthisized data can test the
Connected Components / Circle Fit algo, and the MoCap system's calibration and
reconstruction algo.
