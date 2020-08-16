import copy
import pickle
import json

def composeMeta():
    cam_meta = { }
    META_TEMPLATE = {
                 "TYPE"       : "Magic1k",
                 "RESOLUTION" : [0, 0],  
                 "HWID"       : "",
                 }
    with open( "cameras.meta", "r" ) as fh:
        line = "nope"
        cam_count = -1
        while line:
            line = fh.readline()
            if( "CameraInfo" in line ): # new camera
                cam_count += 1
                cam_meta[ cam_count ] = copy.deepcopy( META_TEMPLATE )
                continue
            if( "ImageWidth" in line ):
                _, width = line.split()
                cam_meta[ cam_count ][ "RESOLUTION" ][0] = int( width )
                continue
            if( "ImageHeight" in line ):
                _, height = line.split()
                cam_meta[ cam_count ][ "RESOLUTION" ][1] = int( height )
                continue

    out = json.dumps( cam_meta, indent=4 )
    
    with open( "temp_meta.json", "w" ) as fh:
        fh.write( out )
        
    return cam_meta


def decodeA2d( cams, file ):
    name, ext = file.split(".")
    
    frames = []
    curr_frame = -1
    last_frame = -1
    
    FRAME_TEMPLATE = [ [0], [], [] ]
    STRIDE = 0
    DETS = 1
    IDS = 2
    
    with open( file, "r" ) as fh:
        line = "nope"
        frame_count = 0
        while line:
            line = fh.readline()
            if( "CentroidData" in line ):
                continue
            if( "Frame" in line ):
                _, f_no = line.split()
                f_no = int( f_no )
                if( f_no > last_frame ):
                    # New frame
                    curr_frame += 1
                    last_frame = f_no
                    frames.append( copy.deepcopy( FRAME_TEMPLATE ) )
                continue
            if( "Centroid " in line ):
                # read some roids
                _, roid_count = line.split()
                roid_count = int( roid_count )

                # strides
                last_stride = frames[ curr_frame ][STRIDE][-1]
                this_stride = last_stride + roid_count
                frames[ curr_frame ][STRIDE].append( this_stride )
                
                for i in range( roid_count ):
                    det_line = fh.readline()
                    x, y, r, c = list( map( float, det_line.split() ) )

                    frames[ curr_frame ][DETS].append( (x, y) )
                    frames[ curr_frame ][IDS].append( 0 )
                    
    print( "loaded {} frames".format( len( frames ) ) )
    
    data = { "FRAMES"    : frames,
             "CAM_ORDER" : sorted( [k for k in cams.keys() if type(k)==int] ),
             "META"      : {
                 "RATE"           :  25,
                 "DIVI"           :   1,
                 "MULT"           :   4,
                 "SYNC"           :  25,
                 "FPS"            : 100,
                 "START_TIMECODE" : "01:02:03:04:01",
                 "LOGGING_NOTES"  : "S",
                 "TAKE_NAME"      : name,
                 
             },
         "CAM_META"  : cams,
    }

    # Pickle
    pickle.dump( data, open( name + ".pik", "wb" ) )

    # JSON for manual inspection
    out = json.dumps( frames[:20], indent=4 )
    with open( name + ".json", "w" ) as fh:
        fh.write( out )
             
    return data

def makeISO( data, name ):
    frames = data["FRAMES"]

    cams = [ [] for i in range( len( data["CAM_META"] ) ) ]

    for strides, dets, ids in frames:
        for i, (_in, out) in enumerate( zip( strides[:-1], strides[1:] ) ):
            # not done that in a while :D
            cams[ i ].append( dets[_in:out] )
            
    print( cams[0][:20] )
    
    for i, cam in enumerate( cams ):
        print(len( cam ) )
        pickle.dump( cam, open( name + "_cam{:0>2}.pik".format( i ), "wb" ) )

    
cams = composeMeta()

for name in [ "calibration.a2d", "ROM.a2d" ]:
    data = decodeA2d( cams, name )
    makeISO( data, name )
    

