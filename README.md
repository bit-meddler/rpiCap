# rpiCap
Ok, this is the big one.  My project to create an Open Source MoCap Camera based on an RPi 3.

## My Contention(s)
1. The RPi 3 has a quad core, 64-Bit, 1.2GHz CPU, with an advanced Vectorized instruction set (NEON)

2. A 'Typical' scene imaged by a mocap camera will contain 20~60 bright spots (the _markers_ reflecting light back to the camera) being 2x2 to 20x20 pixels in size.  More than 98% of the image will be dark (below a threshold value)
2.1 I Argue that we can skip through the majority of the image very quickly by doing a vectorized 'less-than-or-equal' comparison to the threshold.  This can be upto 16 Bytes wide with NEON SIMD.
2.2 The Image could be split into upto 4 regions for parallel evaluation per core
2.3 Thus, the pi will be capable of centroid detection at a usable realtime framerate

3. The Rpi Cam2 is 1280x720 @ 60Hz, and is available in an IR mode, and with an IR strobe ring, it's image is 'published' in a YUV format, so the first block of uint8 bytes will be the B&W image, even though we get (unwanted) colour information.

## Challenges
1. Execute connected components to find RoIs in the image
2. develop some process to circle fit or compute a centroid
3. timely threaded execution on a non-RTS
4. Syncronization of multiple cameras
5. Testing function of prototype camera with MoCap system to configure it

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

### C&C
 - [ ] Emit simple 'Verb:Noun' commands
 - [ ] Communication broker to send & receive data from a camera
 - [ ] simulated camera(s) to emulate a full system to build C&C
 - [ ] Camera Setup tool
 - [ ] Data Recording tool

## Future
The logical conclusion is an Open MoCap system.