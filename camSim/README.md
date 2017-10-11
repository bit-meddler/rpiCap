# Camera Simulator
This is a standalone simulation of an rpiCam.  It will respond correctly to imperatives (Setting C&C values, like 'Strobe Power', 'Shutter durration', 'threshold value'; and executing actions, like 'Start Streaming', 'Take picture').  It will stream out realistic Centroid data with correct packet formatting.

We could simulate some data, and give ground-truth data to a number of these cameras to stream out.  Given a working MoCap system, this data would be interpreted correctly.

We could simulate this data by getting a C3D of a Wand-Wave, or a ROM into Maya, and parenting spheres to the locators, and rendering out 'virtual MoCap' Images from any number of synthetic positions.  This synthisized data can test the Connected Components / Circle Fit algo, and the MoCap system's calibration and reconstruction algo.
