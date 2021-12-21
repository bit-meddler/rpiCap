import pickle

# Reading the Source data
file_fq = r"C:\code\rpiCap\benchmarkData\testing.detps"

with open( file_fq, "rb" ) as fh:
    X = pickle.load( fh )

print( X["CAM_META"][0], X["META"]["TAKE_NAME"] )


# Overview of MoCap Data Structure.  As ever it's a dict of nested structures
data = { "FRAMES"    : [], # list of frames... more details below
         "CAM_ORDER" : [0],
         "META"      : {
             # Important System Metadata
             "RATE"           : 30, # Timecode standard (24, 25, 30)
             "DIVI"           :  1, # Divisor (1.0, 1.001)
             "MULT"           :  2, # Timecode multiplier
             "SYNC"           : 30, # Actual Timecode (23.976, 24, 25, 29.97, 30, 47.95, 50, 59.94, 60, etc... )
             "FPS"            : 60, # Human Timecode  (23, 24, 25, 29, 30)
             
             # Take Logging, embedded in file
             "START_TIMECODE" : "01:02:03:04:01",
             "LOGGING_NOTES"  : "This is simulated data",
             "TAKE_NAME"      : "testing2",
             
         },
         "CAM_META"  : { # Keyed by ID number
             0 : { # Should be obvious
                 "TYPE"       : "SimulatedCamera",  # Could be pi720, pi480, or even Vicon, Mac, etc in the future.  I can dream.
                 "RESOLUTION" : (548, 342),         # Native Res baked into this file. for NDC conversion.
                 "HWID"       : "666",              # Camera UID
                 } # I may extend this with a "CAL" field holding the K, R, T, (k1, k2) values as applicable at capture time.
         }
}
frame = ( (0, 15), # Camera Strides, as this is one camera, only 1 stride
          [ (247, 134), (101, 306), (521, 102), (539, 139), ( 20, 223), # list of detections (x,y) in camera co-ords
            (251,  43), (364, 142), (239, 148), (216, 332), (133,  46), # I might change this to NDCs in range -1.0 ... +1.0
            (160,  10), (161, 153), (127,   2), (393, 116), (480,  62) ],
          [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15] # detection IDs, 0=unknown, -1=skip
)

example_multicam = ( (0, 2, 4, 6), # 3 Cameras
                     [ (247, 134), (101, 306),  # Cam 1 ids ?, 1
                       (251,  43), (364, 142),  # Cam 2 ids 1, 2
                       (160,  10), (161, 153) ],# Cam 3 ids ?, 3
                     [ 0, 1, 1, 2, 0, 3 ] # Strides apply to IDs as well.
)
                       
