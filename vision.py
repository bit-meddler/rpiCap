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
        # ID
        self.id              = self.BIG_NUM
        
        
    def reset( self ):
        self.bb_x, self.bb_y = self.BIG_NUM, self.BIG_NUM
        self.bb_m, self.bb_n =  0, 0
        self.sl_x, self.sl_m =  0, 0
        self.sl_n            =  0
        self.id              = self.BIG_NUM
        
        
    def update( self ):
        # Update SL into the BB
        self.bb_x = min( self.sl_x, self.bb_x )
        self.bb_y = min( self.sl_n, self.bb_y )
        self.bb_m = max( self.sl_m, self.bb_m )
        self.bb_n = max( self.sl_n, self.bb_n )

        
    def merge( self, other ):
        # combine regions self & other
        # expand BB
        self.bb_x = min( other.bb_x, self.bb_x )
        self.bb_y = min( other.bb_y, self.bb_y )
        self.bb_m = max( other.bb_m, self.bb_m )
        self.bb_n = max( other.bb_n, self.bb_n )
        # take incomming scanline
        self.sl_x = other.sl_x
        self.sl_m = other.sl_m
        self.sl_n = other.sl_n
        # region id
        self.id = min( other.id, self.id )
        
        
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
        Notionally, we record mergable regions and preserve the scanlines
        Now we need to merge them into one RoI.
        LUT is source_id : target_id
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
    
    simd = SIMDsim()
    tmp_reg     = Region()  # temporary region for comparison & mergin
    master_line = Region()  # Cache of Merge target's scanline for 242
    
    align       = simd.WIDTH# Align on Vec width modulus
    vec_width   = simd.WIDTH# simulate 128-Bit SIMD lanes (64-Bit)
    vec_th      = simd.wideFlood( threshold )# all threshold
    vec_res     = simd.wideFlood( False )
    sanity = {}
    ended = False
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
            print idx
            print reg_list
            
        for i in range( align_offset ):
            # possibly needs an ended test...
            idx += 1
            if( data[idx] >= threshold ):
                # got one !
                skipping     = False
                scanning     = True
                break
        # skip over dark px, byte aligned
        if( (not scanning) and ((data_out - idx) <= vec_width) ):
            skipping = False
        ended = (idx >= data_out)
        
        while( skipping and not ended ):
            print "skipping from:", idx
            vec_res = simd.wideGTE( data[idx:idx+vec_width], vec_th )
            v_idx = simd.idxOfTrue( vec_res )
            if( v_idx >= 0 ):
                #found one
                skipping = False
                scanning = True
                idx += v_idx
                break
            idx += vec_width
            ended = (idx > data_out)
            if( (data_out - idx) <= vec_width ):
                skipping = False
                scanning = True
        # either we're out of data, or we've found a bright line
        if( ended ):
            return regReconcile( reg_list, reg_lut )

        if( scanning ):
            # Scanning
            # ########
            tmp_reg.reset()
            # compute scanline x,y position in rectilinear image
            if( idx > row_end ):
                # recompute
                tmp_y, tmp_x = divmod( idx, d_w )
                row_start    = tmp_y * d_w
                row_end      = (tmp_y+1) * d_w
            else:
                tmp_x        = idx - row_start
                
            tmp_reg.sl_n, tmp_reg.sl_x = tmp_y, tmp_x
            
            print "Housekeeping at", tmp_x, tmp_y, "From", idx
            
            # housekeeping...
            if( tmp_y > last_y ):
                # Tidy list of found regions if we've advanced a line
                # reset reg_idx to begining of active section
                # TODO: Find a way to do this in place by swapping values, and not re-sizing the list loads.
                reg_scan = reg_start
                reg_idx  = reg_start
                reg_len  = len( reg_list )
                while( reg_scan < reg_len ):
                    # retire this region, it can't touch
                    if( reg_list[ reg_scan ].sl_n == last_y ):
                        reg_list.insert( reg_idx, reg_list.pop( reg_scan ) )
                        reg_idx += 1
                    reg_scan += 1
                reg_start = reg_idx
                last_y = tmp_y
                
            # scan bright px, break if we go round the corner to the next row which
            # happens to have a hot px at [y][0] (rare)
            while( (row_end > idx) and (data[idx] >= threshold) ):
                idx += 1
            tmp_reg.sl_m = (idx - row_start) # last bright px
        
            # Finalize our region based on this scanline
            tmp_reg.update()

            # Connecting
            # ##########
            # check for tmp_reg's scanline conection to existing regions
            # Only Advance the Reg idx here
            # find a region that could plausably touch this scanline
            reg_len = len( reg_list )
            # If this is first Region, we skip this phase
            merge_target = -1
            while( reg_idx != reg_len ):
                if( reg_list[reg_idx].sl_m < tmp_reg.sl_x ):
                    # ri is behind tmp, move on
                    reg_idx += 1

            # this should be the beginning of the merging process
            master_line.reset()
            merging = True
            if( reg_idx == reg_len ):
                # Insert at end, no more merging needed
                new_reg = factory.newRegion()
                new_reg.merge( tmp_reg )
                reg_list.insert( reg_idx, new_reg )
                merging = False
                # Bail out?
                
            while( merging ):
            	# Merging needs a rewrite, but this is better.
            	# TODO: Merging, with Front 242 Algo.
                if( (reg_list[reg_idx].sl_x == tmp_reg.sl_m) or \
                    (tmp_reg.sl_x <= reg_list[reg_idx].sl_m)): # Just inside
                    # tmp starts somewhere in this region
                    merge_target = reg_idx
                    break
                else:
                    # insert before region at ri
                    break

                if( merge_target < 0 ):
                    # insert at reg_idx
                    new_reg = factory.newRegion()
                    new_reg.merge( tmp_reg )
                    reg_list.insert( reg_idx, new_reg )
                else:
                    # merge with merge_target, store origenal Scanline
                    master_line.merge( reg_list[merge_target] )
                    reg_list[merge_target].merge( tmp_reg )
                    
                # Now see if tmp extends into subsequent regions
                reg_len = len( reg_list )
                # are there regions to try and merge into?
            	merging = (reg_len > 0) and (reg_idx != (reg_len-1))
                # meed to check behind, so we don't eat one of the M's legs
                # by propogating the id, we can keep the legs
                # this will require a 'region reconciliation' pass at the end
                # also deal with gutterballs by scanning region list for bb's touching
                # top or bottom line of the ROI we're inspecting
                """
                    There are two key merge conditions, W and M
                    
                    ---   ----   ----
                     ----tmp_reg---
                     
                     
                     ---temp_reg----
                    ---    ---    ----
                    
                    
                """
                if( reg_list[reg_idx+1].sl_x < tmp_reg.sl_m ):
                    # merge this line in
                    # TODO: preserve this Scanline, take 'above' regions's ID
                    # DEPRICATED BELOW
                    reg_list[reg_idx].merge( reg_list[reg_idx+1] )
                    del reg_list[reg_idx+1]
                    reg_len = len( reg_list )
                    if( reg_idx == ( reg_len - 1 ) ):
                        # reached the end of the list
                        merging = False
                else:
                    # can't reach the next region
                    merging = False
                # increment reg_idx to soak up W globs
                
            if( idx >= data_out ):
                ended = True
            # I think we're done!
        """
        # We're only interested in 4-connected Neighbours
        
              --     --     ---    --
             ---    ----    ---    ---
            ---      --     ---     --
        
        
        # Globbed Markers
        
              --  --
             ----.---   
              --  --
        the . would be a 'saddle point' where there is a brightness gradient
        
        # region scanning notes
        
  
          x-ri-m   x-ri+1-m
               x---m
                tmp
        
        """
        # Go back to skipping dark Pixels
        
    # we can only be here if we've ended
    return regReconcile( reg_list, reg_lut )
    
# Tests
"""
    0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
0 [10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0]
1 [10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0]
2 [10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0]
3 [ 0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10]
4 [ 0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10]
5 [ 0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10]
"""
# Basic test
a = ([10]*3 + [0]*13) * 3
A = np.array(a).reshape(3,-1)
a.reverse()
B = np.array(a).reshape(3,-1)
test = np.vstack( [A,B] )
print test
regs = connected( test.ravel(), test.T.shape, 5 )
print regs


# more complext regions
A = np.zeros( (8,24), dtype=np.uint8 )
B = np.ones( (3,3), dtype=np.uint8 ) * 10
pos = ( (0,1), (0,5), (0,20), (4,3), (5, 19) )
for y,x in pos:
    A[y:y+3, x:x+3] = B
test = A
print test
regs = connected( test.ravel(), test.T.shape, 5 )
print regs