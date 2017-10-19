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
    reg_list    = [[],[]]   # list of regions found so far 0=Can't possibly touch, 1=might touch
    last_y      = -1        # last y encountered, to see if we've skiped a line
    tmp_reg     = Region()  # temporary region for comparison & mergin
    tmp_pos     = 0         # store of start idx of hot line
    tmp_x, tmp_y= 0, 0      # temp x & y position of bright pixel
    data_in     = 0         # frame to begin scanning
    data_out    = len(data) # frame to end scanning
    factory     = RegionFactory() # Region Factory
    
    simd = SIMDsim()
    
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
        i = idx + data_out
        if not i in sanity:
            sanity[i] = 0
        sanity[i] += 1
        if sanity[i] > 6:
            exit(0)
            
        for i in range( align_offset ):
            # possibly needs an ended test...
            idx += 1
            if( data[idx] >= threshold ):
                # got one !
                skipping     = False
                scanning     = True
                break
        # skip over dark px, byte aligned
        # TODO: what if remnants are not boundery aligned
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
            return reg_list

        if( scanning ):
            # Scanning
            # ########
            tmp_reg.reset()
            # compute scanline x,y position in rectilinear image
            tmp_reg.sl_n, tmp_reg.sl_x = divmod( idx, d_w )
            tmp_pos = idx
            print "Housekeeping at", tmp_reg.sl_x, tmp_reg.sl_n, "From", idx
            line_end = (tmp_reg.sl_n+1) * d_w # check we don't loop, LE==Exclusive
            # housekeeping...
            if( tmp_reg.sl_n > last_y ):
                # Tidy list of found regions if we've advanced a line
                # reset reg_idx to begining of active list
                last_y = tmp_reg.sl_n
                touching_y = last_y - 1 # y of a scanline that can tocuh the current scanline
                # A bit hokey, but it will work
                done = False
                reg_inspect = 0
                move_list = [] # idxs of active reg list to move to inactive
                while( not done ):
                    # maybe go through the list backwards, bubbling non-touching regions up to
                    # reg_start++ ?  Like a gnome sort??  Would happen in place to a 1D list
                    candidates = len( reg_list[1] )
                    if( candidates < 1 ):
                        # out of candidates, need to append to reg_list
                        done = True
                        break
                    if( reg_inspect >= candidates ):
                        # runout of regions to test
                        done = True
                        break
                        
                    if reg_list[1][reg_inspect].sl_n != touching_y:
                        # this region can't touch the line, drop it
                        move_list.append( reg_inspect )
                        reg_inspect += 1
                    else:
                        # keep this in the active list, move inspection idx along
                        reg_inspect += 1

                # we have a list of idxs to demote from the active list to the finished list
                for i in move_list:
                    reg_list[0].append( reg_list[1].pop( i ) )
                # move reg_idx back to start of active region list
                reg_idx = 0
                
            # scan bright px, break if we go round the corner to the next row which
            # happens to have a hot px at [y][0] (rare)
            while( (line_end > idx) and (data[idx] >= threshold) ):
                idx += 1
            tmp_reg.sl_m = tmp_reg.sl_x + (idx - tmp_pos) # last bright px
        
            # Finalize our region based on this scanline
            tmp_reg.update()

            # Connecting
            # ##########
            # check for tmp_reg's scanline conection to existing regions
            
            # find a region that could plausably touch this scanline
            reg_len = len( reg_list[1] )
            # If this is first Region, we skip this phase
            merge_target = -1
            while( reg_idx <= (reg_len-1) ):
                if( reg_list[1][reg_idx].sl_m < tmp_reg.sl_x ):
                    # ri is behind tmp, move on
                    reg_idx += 1
                    if( reg_idx == reg_len ):
                        # we want to insert at the end (reg_idx)
                        break

                if( (reg_list[1][reg_idx].sl_x == tmp_reg.sl_m) or \
                    (tmp_reg.sl_x <= reg_list[1][reg_idx].sl_m)): # Just inside
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
                reg_list[1].insert( reg_idx, new_reg )
            else:
                # merge with merge_target
                reg_list[1][merge_target].merge( tmp_reg )
                    
            # Now see if tmp extends into subsequent regions
            reg_len = len( reg_list[1] )
            # are there regions to try and merge into?
            merging = (reg_len > 0) and (reg_idx != (reg_len-1))
            print merging
            while( merging ):
                # meed to check behind, so we don't eat one of the M's legs
                # by propogating the id, we can keep the legs
                # this will require a 'region reconciliation' pass at the end
                # also deal with gutterballs by scanning region list for bb's touching
                # top or bottom line of the ROI we're inspecting
                if( reg_list[1][reg_idx+1].sl_x < tmp_reg.sl_m ):
                    # merge this line in
                    # TODO: preserve this Scanline, take 'above' regions's ID
                    # DEPRICATED BELOW
                    reg_list[1][reg_idx].merge( reg_list[1][reg_idx+1] )
                    del reg_list[1][reg_idx+1]
                    reg_len = len( reg_list[1] )
                    if( reg_idx == ( reg_len - 1 ) ):
                        # reached the end of the list
                        merging = False
                    pass
                else:
                    # can't reach the next region
                    merging = False
                    
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
    return reg_list
    
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
a = ([10]*3 + [0]*13) * 3
A = np.array(a).reshape(3,-1)
a.reverse()
B = np.array(a).reshape(3,-1)
test = np.vstack( [A,B] )
shape = test.T.shape #escape from Row Mjr !
print test
regs = connected( test.ravel(), shape, 5 )
print regs