# rpiCap
Ok, this is the big one.  My project to create an Open Source MoCap Camera based on an RPi 3.

## My Contention(s)
1. The RPi 3 has a quad core, 64-Bit, 1.2GHz CPU, with an advanced Vectorized instruction set (NEON)

2. A 'Typical' scene imaged by a mocap camera will contain 20~60 bright spots (the _markers_ reflecting light back to the camera) being 2x2 to 20x20 pixels in size.  More than 98% of the image will be dark (below a threshold value)
2.1 I Argue that we can skip through the majority of the image very quickly by doing a vectorized 'less-than-or-equal' comparison to the threshold.  This can be upto 16 Bytes wide with 64-Bit NEON SIMD.
2.2 The Image could be split into upto 4 regions for parallel evaluation per core
2.3 Thus, the pi will be capable of centroid detection at a usable realtime framerate

3. The Rpi Cam2 is 1280x720 @ 60Hz, and is available in an IR mode and with an IR strobe ring. It's image is 'published' in a YUV format through the default API, so the first 'block' of uint8 bytes will be the B&W image _unraveled_.  Even though we get (unwanted) chroma information, we can skip it entirely.

## Challenges
1. Execute connected components to find RoIs in the image
2. develop some process to circle fit or compute a centroid
3. timely threaded execution on a non-RTS
4. Syncronization of multiple cameras
5. Testing function of prototype camera without MoCap system to configure it

## Tasks
### Camera
 - [ ] 'Main Loop' for camera receiving and interpreting commands, taking pictures, returning data
 - [ ] 'connected components' algo
 - [ ] 'centroid computation' algo
 - [ ] evaluate timing of these algos
 - [ ] possible threaded implementation of these algos & main loop
 - [ ] c implementation of best result
 - [ ] syncronization strategy research
 - [ ] RTOS R&D if needed

### Algo
- [ ] Connected components _Monolithic_
- [ ] Connected components _Paralel_
- [ ] Centroids _CircleFit_
- [ ] Centroids _AvgCenter_
- [ ] Centroids _HuMoments_
- [ ] Monolithic Vs Parallel Implementations
- [ ] SIMD Implementations

### Syncronization
- [ ] NTP Protocol
- [ ] PtP Protocol
- [ ] Multi-Cast trigger ??
- [ ] PtP Trigger ??
- [ ] GPIO Trigger ??

### Custom Distro
- [ ] Build minimal Raspian distribution ????
- [ ] Camera I/O
- [ ] Network I/O
- [ ] File system I/O
- [ ] Sync system
- [ ] Telnet

or alternativly, sync might require us to use an RTOS.  Or even worese, roll something.  Let's not.


### C&C Program
 - [ ] Emit simple 'Verb:Noun' commands (UDP, 8-Bit)
 - [ ] Communication broker to send & receive data from a camera
 - [ ] simulated camera(s) to emulate a full system to build C&C Program
 - [ ] Camera Setup tool
 - [ ] Data Recording tool

## Future
The logical conclusion is an Open Source MoCap system.
- [ ] Record Camera Data
- [ ] Detect & Label Wand from Centroids
- [ ] Calibrate Cameras from Labelled wand correspondence
- [ ] Solve 3D points from Centroids & Calibration
- [ ] Object Builder (Associate markers to joints, solve Joint T/R from marker configuration, IK solving, expression solving)
- [ ] Object Calibration
- [ ] Object Finger-Printing (Associate 3d cloud to specific object)
- [ ] Rigid Object Tracking
- [ ] Articulated Object Tracking
