# rpiCap
Ok, this is the big one.  My project to create an Open Source MoCap Camera based on an RPi 3.

## My Contention(s)
1. The RPi 3 has a quad core, 64-Bit, 1.2GHz CPU, with an advanced Vectorized instruction set (NEON)

2. A 'Typical' scene imaged by a mocap camera will contain 20~60 bright spots (the _markers_ reflecting light back to the camera) being 2x2 to 20x20 pixels in size.  More than 98% of the image will be dark (below a threshold value)
    - 2.1. I Argue that we can skip through the majority of the image very quickly by doing a vectorized 'less-than-or-equal' comparison to the threshold.  This can be upto 16 Bytes wide with 64-Bit NEON SIMD.
    - 2.2. The Image could be split into upto 4 regions for parallel evaluation per core - however in testing 1 core performs well
    - 2.3. Thus, the pi will be capable of centroid detection at a usable realtime framerate

3. The Rpi Cam2 is 1280x720 @ 60Hz, and is available in an IR mode and with an IR strobe ring. It's image is 'published' in a YUV format through the default API, so the first 'block' of uint8 bytes will be the B&W image _unraveled_.  Even though we get (unwanted) chroma information, we can skip it entirely.

4. But wait! [This arducam](https://www.arducam.com/product/arducam-full-hd-color-global-shutter-camera-for-raspberry-pi-2-3mp-ar0234-wide-angle-pivariety-camera-module-b0353/) has a Global Shutter, and apparently an external sync (TTL?) Input.  Were here guys!  This might just do it.  It's Colour, and in the Visible spectrum (not really what we want for MoCap), but if I can grab the luma portion and it syncs correctly.  We're rock'in!

## Licence
This project is subject to the GPLv3 Licence, a copy is in the root, and will be refferenced in the source and resource files going forwards.

## Challenges
1. Execute connected components to find RoIs in the image **DONE**
2. Develop some process to circle fit or compute a centroid **DONE**
3. Timely threaded execution on a non-RTS _Threading not yet required_
4. Hook into Camera system and centroid detect at 60fps <- _We are here_
5. Syncronization of multiple cameras
6. Testing function of prototype camera without MoCap system to configure it

## Tasks
### Camera
 - [ ] 'Main Loop' for camera receiving and interpreting commands, taking pictures, returning data
 - [x] 'connected components' algo
 - [x] 'centroid computation' algo
 - [x] evaluate timing of these algos
 - [ ] possible threaded implementation of these algos & main loop
 - [x] c implementation of best result
 - [ ] syncronization strategy research c.f. Hermann-SW
 - [ ] RTOS R&D if needed

### Algo
- [x] Connected components _Monolithic_
- [x] Connected components _Paralel_
- [ ] Centroids _CircleFit_
- [ ] Centroids _AvgCenter_
- [x] Centroids _HuMoments_
- [ ] Monolithic Vs Parallel Implementations
- [x] SIMD Implementations

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
 - [x] Emit simple 'Verb:Noun' commands (UDP, 8-Bit)
 - [x] Communication broker to send & receive data from a camera
 - [x] simulated camera(s) to emulate a full system to build C&C Program
 - [x] Camera Setup tool
 - [ ] Data Recording tool

## Future
The logical conclusion is an Open Source MoCap system - See Gimli: https://github.com/bit-meddler/Gimli
