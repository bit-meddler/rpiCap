"""
computer vision experiments

IMPORTANT

x,y -> m,n convention

Assuming top left is (0,0)

x,y is the start of a region, m,n is the end:

  +-------------+
  |x,y          |
  |             |
  |             |
  |             |
  |             |
  |          m,n|
  +-------------+

  so m is a short hand for extend of a row begining at x
  and n is the same for a col starting at y

Interesting:
    https://www.youtube.com/watch?v=AAbUfZD_09s
    https://users.cs.cf.ac.uk/Paul.Rosin/resources/papers/Hu-circularity-PR-postprint.pdf
  
Other observations
    circularity = 4pi(area/perimeter^2)
"""
from copy import deepcopy
import numpy as np


class Region( object ):

    BIG_NUM = 10000 # bigger than expected image extents

    
    def __init__( self ):
        # bounding box
        self.bb_x, self.bb_y = self.BIG_NUM, self.BIG_NUM # to fail min(x, bb_x)
        self.bb_m, self.bb_n =  0, 0 # lo number, to fail max(x, bb_m)
        # scanline
        self.sl_x, self.sl_m =  0, 0 # LAST hot line of this region
        self.sl_n            =  0    # y index of last scanline
        # stats
        self.area            =  0    # area of glob
        self.perimeter       =  0    # parimiter of glob
        # ID
        self.id              = self.BIG_NUM
        
        
    def reset( self ):
        self.bb_x, self.bb_y = self.BIG_NUM, self.BIG_NUM
        self.bb_m, self.bb_n =  0, 0
        self.sl_x, self.sl_m =  0, 0
        self.sl_n            =  0
        self.area            =  0
        self.perimeter       =  0
        self.id              = self.BIG_NUM
        
        
    def update( self ):
        # Update SL into the BB
        self.bb_x = min( self.sl_x, self.bb_x )
        self.bb_y = min( self.sl_n, self.bb_y )
        self.bb_m = max( self.sl_m, self.bb_m )
        self.bb_n = max( self.sl_n, self.bb_n )
        
        
    def merge( self, other ):
        # combine regions
        # expand BB
        self.bb_x = min( other.bb_x, self.bb_x )
        self.bb_y = min( other.bb_y, self.bb_y )
        self.bb_m = max( other.bb_m, self.bb_m )
        self.bb_n = max( other.bb_n, self.bb_n )
        # take incomming scanline
        self.takeSL( other )
        # statistics
        self.area += other.area
        self.perimeter += other.perimeter
        # region id
        self.id = min( other.id, self.id )

        
    def takeSL( self, other ):
        # Take scanline data
        self.sl_x = other.sl_x
        self.sl_m = other.sl_m
        self.sl_n = other.sl_n
        
        
    def __repr__( self ):
        return "Region '{}' BB: {}-{}, {}-{}. SL: {}-{} @ {}".format(
            self.id,   self.bb_x, self.bb_y, self.bb_m, self.bb_n,
            self.sl_x, self.sl_m, self.sl_n
        )
        
        
class RegionFactory( object ):
    # Factory to automanage region ids
    def __init__( self ):
        self.region_count  = 0

        
    def newRegion( self ):
        new_reg = Region()
        new_reg.id = self.region_count
        self.region_count += 1
        return new_reg
        
        
class SIMDsim( object ):
    """ This is a bit cruddy, but the idea is to simulate what we'd get with a
        few NEON commands.  Switching width (8/16) easily acomplished. """
    
    WIDTH   = 8 # Simulate 128-Bit Vector pipeline, or 64-Bit mode
    _INDEXS = tuple( range( WIDTH ) ) # conveniance
    
    
    def __init__( self ):
        pass
        
        
    @staticmethod
    def wideGTE( data, comparison ):
        ret = [ False for _ in SIMDsim._INDEXS ]
        for i in SIMDsim._INDEXS:
            ret[i] = (data[i] >= comparison[i])
        return ret
        
        
    @staticmethod
    def wideFlood( val ):
        return [ val for _ in SIMDsim._INDEXS ]
    
    
    @staticmethod
    def anyTrue( data ):
        for val in data:
            if val == True:
                return True
        return False
        
        
    @staticmethod
    def idxOfTrue( data ):
        # I don't even know if this exists
        for i in SIMDsim._INDEXS:
            if data[i]:
                return i
        return -1
    
    
def regReconcile( reg_list, reg_lut ):
    """
        Finish up regions
        1) Add every region's last scan line to it's perimeter and area
        2) if region in LUT.keys we need to drop it and merge into it's parent
        3) return BBs
    """
    return reg_list
    
    
def connected( data, data_wh, threshold ):
    """
        Assuming data is a flat (unravelled) array of uint8 from a sensor of dimentions data_wh
        
        skip data[i]<threshold, on encountering data[i]>=threshold do some processing
        
        desired output:
            list of x,y -> m,n regions - a bounding box enclosing a 'Blob'
        
        'Classic' Connected components seeks to mark all pixels in a region.
        we are interested in ID'ing a region, and then finding it's centroid
        With a miss-shapen region, additional processing is needed Hough-Circles?
        
        Front 242 Method:
            my new solution is based on misshearing "headhunter" By Front 242
                https://www.youtube.com/watch?v=m1cRGVaJF7Y
                
            1) You lock the Target...
            2) You Paint* the Line...
            3) You slowly spread the Net...
            4) You catch the man!
                * actually bait
                
            1) So far so normal.  Skip Dark Pixels, till you Lock onto a bright one
            2) Do Region scanning, matching the Region's scanline to the Target's
            3)  if Master line > Target Line
                    Advance along Y to the end of the "master line".
                        cache the master-line before merging the Target scanline
                        if there is another 'hot line' (or more) inside, create a new region for it (after Master region) but give it the master's ID, add ID to a _set_ of IDs to post-merge.
                        Keep doing this till you reach end of master-line
                else
                    do region merging, eat subsequent regions till we reach the end of the Target scanline
            4) as a Post processess, scan the region list and merge any regions with IDs in the set
    """
    d_w, d_h    = data_wh   # image extents
    idx         = 0         # index into data
    reg_idx     = 0         # index of last region in the region list
    reg_start   = 0         # index of first region touching current image line
    reg_list    = []        # list of regions found so far, impossible regions bubble to left of reg_start
    reg_lut     = {}        # LUT of region IDs that need to be merged
    last_y      = -1        # last y encountered, to see if we've skiped a line
    row_end     = -1        # end of image row, so we don't have to divmod too much
    tmp_pos     = 0         # store of start idx of hot line
    tmp_x, tmp_y= 0, 0      # temp x & y position of bright pixel
    data_in     = 0         # frame to begin scanning
    data_out    = len(data) # frame to end scanning
    factory     = RegionFactory() # Region Factory
    
    tmp_reg     = Region()  # temporary region for comparison & mergin
    master_line = Region()  # Cache of master Region's scanline for 242
    scan_line   = Region()  # Cache of current scanline
    
    simd        = SIMDsim() # Proxy for what will be SIMD
    align       = simd.WIDTH# Align on Vec width modulus
    vec_width   = simd.WIDTH# simulate 128-Bit SIMD lanes (64-Bit)
    vec_th      = simd.wideFlood( threshold )# all threshold
    vec_res     = simd.wideFlood( False )
    
    ended       = False
    skipping    = False
    scanning    = False
    inserting   = False
    merging     = False
    
    sanity = {}
    # BEGIN
    while( not ended ):
        # prep
        # problem soaking up the last bytes...
        align_offset = align - (idx % align)
        skipping     = True
        scanning     = False
        vec_res      = simd.wideFlood( False )
        
        # skipping
        # ########
        # advance by single bytes, till we are in alignment
        # print "align", align_offset, idx, data_out, ended
        if idx>1:
            print idx, data_out
            print reg_list
            
        for i in range( align_offset ):
            # possibly needs an ended test...
            if( data[idx] >= threshold ):
                # got one !
                skipping     = False
                scanning     = True
                break
            idx += 1
        # skip over dark px, byte aligned
        if( (not scanning) and ((data_out - idx) <= vec_width) ):
            skipping = False
        ended = (idx >= (data_out-1))
        
        while( skipping and not ended ):
            print "skipping from:", idx
            vec_res = simd.wideGTE( data[idx:idx+vec_width], vec_th )
            v_idx = simd.idxOfTrue( vec_res )
            if( v_idx >= 0 ):
                #found one
                skipping = False
                scanning = True
                idx += v_idx
                print "^"
                break
            idx += vec_width
            ended = (idx > data_out)
            if( (data_out - idx) <= vec_width ):
                skipping = False
                scanning = False # soak up last non-aligned bytes
        # either we're out of data, or we've found a bright line
        if( ended ):
            return regReconcile( reg_list, reg_lut )

        if( scanning ):
            # Scanning
            # ########
            print "scanning", idx
            tmp_reg.reset()
            # compute scanline x,y position in rectilinear image
            if( idx > row_end ):
                tmp_y, tmp_x = divmod( idx, d_w )
                row_start    = tmp_y * d_w
                row_end      = ((tmp_y+1) * d_w) - 1 # this row in a rect image
            else:
                tmp_x        = idx - row_start # Still in the same row
                
            tmp_reg.sl_n, tmp_reg.sl_x = tmp_y, tmp_x
            
            print "Housekeeping test at", tmp_x, tmp_y, "From", idx, "last-y", last_y, "re", row_end
            
            # housekeeping...
            if( tmp_y > last_y ):
                # Tidy list of found regions if we've advanced a line
                # reset reg_idx to begining of active section
                # TODO: Find a way to do this in place by swapping values, and not re-sizing the list loads.
                reg_scan = reg_start
                reg_idx  = reg_start
                reg_len  = len( reg_list )
                previous_y = tmp_y-1
                while( reg_scan < reg_len ):
                    # retire this region, it can't touch
                    print "?", reg_list[ reg_scan ].sl_n, last_y
                    if( reg_list[ reg_scan ].sl_n < previous_y ):
                        reg_list.insert( reg_idx, reg_list.pop( reg_scan ) )
                        reg_idx += 1
                    reg_scan += 1
                reg_start = reg_idx
                last_y = tmp_y
            
            # Scan bright px, break if we go round the corner to the next row which
            # happens to have a hot px at [y][0] (rare)
            # idx is the FIRST bright px
            while( (row_end >= idx) and (data[idx] >= threshold) ):
                idx += 1
            
            tmp_reg.sl_m = (idx - row_start) -1  # last bright px
            print "sl ended on", idx, tmp_reg.sl_m, tmp_reg.sl_n
            # Finalize our region based on this scanline
            tmp_reg.update()

            # Connecting
            # ##########
            # check for tmp_reg's scanline conection to existing regions
            # Only Advance the Reg idx here
            # find a region that could plausably touch this scanline
            reg_len = len( reg_list )
            # If this is first Region (reg_len==0, reg_idx==0), we skip this phase
            print "regi", reg_idx, reg_len
            while( reg_idx != reg_len ):
                if( reg_list[reg_idx].sl_m < tmp_reg.sl_x ):
                    # ri is behind tmp, move on
                    reg_idx += 1
                else:
                    break
                    
            # reg_idx is either at the end of rl, touching rl[ri], or in front of rl[ri]
            merging   = True
            inserting = False
            # guard against 0 length list
            if( reg_idx == reg_len ):
                # Bug: what if this is desired.  we are marging into the first region?
                print "="
                inserting = True
            elif( tmp_reg.sl_m < reg_list[reg_idx].sl_x ):
                print "touch"
                inserting = True
                
            if( inserting ):
                # Insert at end, or in front of rl[ri] as they don't touch
                print "New reg, insert"
                new_reg = factory.newRegion()
                new_reg.merge( tmp_reg )
                # Collect statistics HERE
                new_reg.area = new_reg.sl_m - new_reg.sl_x
                new_reg.perimeter = new_reg.area
                reg_list.insert( reg_idx, new_reg )
                merging = False
                # Bail out?

            if( merging ):
                print "in Merge Test"
                mode = "N" # "W", "M"
                master_line.reset()
                scan_line.reset()
                scan_line.takeSL( tmp_reg )
                
                if( reg_list[reg_idx].sl_m > tmp_reg.sl_m ):
                    # M mode - possibly undiscovered scanlines
                    mode = "M"
                    # store the ML
                    master_line.takeSL( reg_list[reg_idx] )
                    master_line.id = reg_list[reg_idx].id
                elif( tmp_reg.sl_m > reg_list[reg_idx].sl_m ):
                    # W mode, we are looking for regions
                    mode = "W"
                else:
                    # Perfect Match, "W" is less expensive I think
                    mode = "W"
                    
                reg_list[reg_idx].merge( tmp_reg )
            sanity = 5
            while( merging ):                
                print "in Merging", mode
                sanity -= 1
                if sanity==0:
                    exit(0)
                if( mode=="W" ):
                    print "in W", reg_idx, reg_len
                    # advance through regions, swallowing them
                    while( (reg_idx < (reg_len-1)) ): # if ri==len, we're at the end
                        if( (reg_list[reg_idx+1].sl_x <= scan_line.sl_m) and \
                            (reg_list[reg_idx+1].sl_m >= scan_line.sl_x) ):
                            print "W merge"
                            # merge region data
                            reg_list[reg_idx].merge( reg_list[reg_idx+1] )
                            # preserve the scanline for future merging
                            reg_list[reg_idx].takeSL( scan_line )
                            # promote old region to master_line, and pop
                            # we'll need the ml if there is an overhang
                            master_line.takeSL( reg_list.pop( reg_idx+1 ) )
                            master_line.id = reg_list[reg_idx].id
                            reg_len -= 1
                        else:
                            break
                    # check for overhang, we might enter M mode
                    if( reg_list[reg_idx].sl_m > scan_line.sl_m ):
                        mode = "M"
                    else:
                        merging = False
                        
                if( mode=="M" ):
                    print "in M"
                    # scan px in this row up to master_line.sl_m
                    # if there is a hot line, create a new region, add to lut
                    line_end = (master_line.sl_n*d_w)+master_line.sl_m
                    print idx, row_end, line_end
                    while( idx <= line_end ):
                        if( data[idx] > threshold ):
                            # found a hot line AGAIN
                            print "New reg, M mode"
                            new_reg = factory.newRegion()
                            new_reg.sl_x = idx - row_start
                            new_reg.sl_y = tmp_y
                            while( idx < row_end ):
                                if( data[idx] > threshold ):
                                    idx += 1
                                else:
                                    break
                            new_reg.sl_m = idx - row_start
                            new_reg.update()
                            # collect statistics
                            new_reg.area = new_reg.sl_m - new_reg.sl_x
                            new_reg.perimeter = new_reg.area
                            reg_idx += 1
                            reg_list.insert( reg_idx, new_reg )
                            reg_len += 1
                            # update LUT
                            reg_lut[ new_reg.id ] = master_line.id
                            if( new_reg.sl_m > master_line.sl_m ):
                                # we're in W mode now
                                mode = "W"
                                break
                        idx += 1
                    if( idx >= master_line.sl_m ):
                        merging = False
                
            if( idx >= (data_out-1) ):
                ended = True
            # I think we're done!

        # Go back to skipping dark Pixels
        
    # we can only be here if we've ended
    return regReconcile( reg_list, reg_lut )
    
# Tests
"""
Basic test
    0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
0 [10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0]
1 [10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0]
2 [10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0]
3 [ 0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10]
4 [ 0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10]
5 [ 0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10]
"""
a = ([10]*3 + [0]*13) * 3
A = np.array(a).reshape(3,-1)
a.reverse()
B = np.array(a).reshape(3,-1)
test = np.vstack( [A,B] )
print test
regs = connected( test.ravel(), test.T.shape, 5 )
print regs

"""
more complext regions
    0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
0 [ 0 10 10 10  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0]
1 [ 0 10 10 10  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0]
2 [ 0 10 10 10  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0]
3 [ 0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0]
4 [ 0  0  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0]
5 [ 0  0  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0  0]
6 [ 0  0  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0  0]
7 [ 0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0  0]
"""
A = np.zeros( (8,24), dtype=np.uint8 )
B = np.ones( (3,3), dtype=np.uint8 ) * 10
pos = ( (0,1), (0,5), (0,20), (4,3), (5, 19) )
for y,x in pos:
    A[y:y+3, x:x+3] = B
test = A
print test
regs = connected( test.ravel(), test.T.shape, 5 )
print regs
"""
Now for a blobby one...
    0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
0 [ 0 10 10 10  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0]
1 [ 0 10 10 10  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0]
2 [ 0 10 10 10  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0]
3 [ 0  0  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0]
4 [ 0  0  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0  0]
5 [ 0  0  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0  0]
6 [ 0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0  0]
7 [ 0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0]
    
"""
A = np.zeros( (8,24), dtype=np.uint8 )
pos = ( (0,1), (0,5), (1,20), (3,3), (4, 19) )
for y,x in pos:
    A[y:y+3, x:x+3] = B
test = A
print test
regs = connected( test.ravel(), test.T.shape, 5 )
print regs