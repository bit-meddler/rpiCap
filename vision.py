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
import numpy as np


class Region( object ):
    # touching flags
    NOTOUCH = 0
    NORTH   = 1 # BINARY
    SOUTH   = 2
    
    def __init__( self ):
        # bounding box
        self.bb_x, self.bb_y = 1e6, 1e6 # big number, to fail min(x, bb_x)
        self.bb_m, self.bb_n =   0,   0 # lo number, to fail max(x, bb_m)
        # scanline
        self.sl_x, self.sl_m =   0,   0 # LAST hot line of this region
        self.sl_n            =   0      # y index of last scanline
        # touching
        self.touching        = NOTOUCH  # if on a North/South boundary
        
        
    def reset( self ):
        self.bb_x, self.bb_y = 1e6, 1e6
        self.bb_m, self.bb_n =   0,   0
        self.sl_x, self.sl_m =   0,   0
        self.sl_n            =   0
        self.touching        =   0
        
        
    def push( self, x, y, m ):
        # grow BB
        self.bb_x = min( x, self.bb_x )
        self.bb_y = min( y, self.bb_y )
        self.bb_m = max( y, self.bb_m )
        self.bb_n = max( n, self.bb_n )
        # update scanlline
        self.sl_x, self.sl_m, self.sl_n = x, m, y
        
        
    def merge( self, other ):
        # combine regions self & other
        # expand BB
        self.bb_x = min( other.x, self.bb_x )
        self.bb_y = min( other.y, self.bb_y )
        self.bb_m = max( other.m, self.bb_m )
        self.bb_n = max( other.n, self.bb_n )
        # merge touching status
        self.touching |= other.touching
        # keep orig scanline
        
        
class SIMDsim( object ):
    """ This is a bit cruddy, but the idea is to simulate what we'd get with a
        few NEON commands.  Switching width (8/16) easily acomplished. """
    
    WIDTH   = 16 # Simulate 128-Bit Vector pipeline, or 64-Bit mode
    _INDEXS = tuple( range( WIDTH ) ) # conveniance
    
    
    def __init__( self ):
        pass
        
        
    @classmethod
    def wideGTE( data, comparison ):
        ret = [ 0 for _ in self._INDEXS ]
        for i in self._INDEXS:
            ret[i] = (data[i] >= comparison[i])
        return ret
        
        
    @classmethod
    def wideFlood( val ):
        return [ val for _ in self._INDEXS ]
    
    
    @classmethod
    def anyTrue( data ):
        for val in data:
            if val == True:
                return True
        return False
        
        
    @classmethod
    def idxOfTrue( data ):
        # I don't even know if this exists
        for i in self._INDEXS:
            if data[i]:
                return i
        return -1
        
        
def connected( data, data_wh, threshold ):
    """
        Assuming data is a flat (unravelled) array of uint8 from a sensor of dimentions data_wh
        
        skip data[i]<threshold, on encountering >= threshold do some processing
        
        desired output:
            list of x,y -> m,n regions - a bounding box enclosing a 'Blob'
    """
    
    d_w, d_h    = data_wh   # image extents
    idx         = 0         # index into data
    reg_idx     = 0         # index of last region in the region list
    reg_start   = 0         # index of first region touching current image line
    reg_list    = [[],[]]   # list of regions found so far 0=Can't possibly touch, 1=might touch
    last_y      = -1        # last y encountered, to see if we've skiped a line
    tmp_reg     = Region()  # temporary region for comparison & mergin
    tmp_x, tmp_y= 0, 0      # temp x & y position of bright pixel
    data_in     = 0         # frame to begin scanning
    data_out    = len( data )#frame to end scanning
    
    simd = SIMDsim()
    
    align       = 4         # 4-byte memory alignment
    vec_width   = simd.WIDTH# simulate 128-Bit SIMD lanes (64-Bit)
    vec_th      = simd.wideFlood( threshold )# all threshold
    vec_res     = simd.wideFlood( False )
    
    ended = False
    # BEGIN
    while( not ended ):
        # prep
        align_offset = idx % align
        skipping     = True
        vec_res      = simd.wideFlood( False )
        
        # skipping
        # ########
        # advance by single bytes, till we are in alignment
        for i in range( align_offset ):
            # possibly needs an ended test...
            idx += 1
            if( data[idx] >= threshold ):
                # got one !
                skipping   = False
                vec_res[idx] = True # Needed??
                break
        # skip over dark px, byte aligned        
        ended = (idx > data_out)
        while( skipping and not ended ):
            vec_res = simd.wideGTE( data[idx:idx+vec_width], vec_th )
            v_idx = simd.idxOfTrue( vec_res )
            if( v_idx > 0 ):
                #found one
                skipping = False
                idx += v_idx
                break
            idx += vec_width
            ended = (idx > data_out)
        # either we're out of data, or we've found a bright line
        if( ended ):
            return reg_list
        # we have a bright pixel, so start...
        
        # Scanning
        # ########
        tmp_reg.reset()
        # compute scanline x,y position in rectilinear image
        tmp_reg.sl_x, tmp_reg.sl_n = divmod( idx, d_w )
        line_end = (tmp_reg.sl_n+1) * d_h # check we don't looop
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
        while( (data[idx] >= threshold) and (line_end > idx) ):
            idx += 1
        tmp_reg.sl_m = idx - 1 # last bright px
        
        # Connecting
        # ##########
        # check for tmp_reg's scanline conection to existing regions
        
        # find a region that could plausably touch this scanline
        # TODO: What if this is first Region?
        reg_len = len( reg_list[1] )
        connecting = reg_len > 0
        merge_target = -1
        mode = "MERGE" # or INSERT
        while( connecting ):
            if( reg_list1[1][reg_idx].sl_m < tmp_reg.sl_x ):
                # ri is behind tmp, move on
                reg_idx += 1
                if( reg_idx == reg_len ):
                    # we want to insert at the end (reg_idx)
                    connecting = False
                    mode = "INSERT"
            elif( reg_list1[1][reg_idx].sl_m == tmp_reg.sl_x ):
                # tmp starts somewhere in this region
                mode = "MERGE"
                connecting = False
            else:
                # insert before ri
                connecting = False
                mode = "INSERT"
                
        # Now see if tmp extends into subsequent regions
        reg_len = len( reg_list[1] )
        # are there regions to try and merge into?
        merging = (reg_len > 0) and (reg_idx!=(reg_len-1))
        while( merging ):
            if( reg_list1[1][reg_idx+1].sl_x < tmp_reg.sl_m ):
                # merge this line in
                # del reg_list1[1][reg_idx+1]
                # if( reg_idx!=( len( reg_list[1] ) -1 ):
                # merging = False
                pass
            else:
                merging = False
                
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
        
        
        # region scanning notes
        
  
          x-ri-m   x-ri+1-m
               x---m
                tmp
        
        """
        # merge
        # otherwise this is a new region
        new_reg = Region()
        new_reg.push( tmp_reg.sl_x, tmp_reg.sl_n, tmp_reg.sl_m )
        reg_list.insert( reg_idx, new_reg )
        # Go back to sckipping dark Px
        
    # we can only be here if we've ended
    return reg_list
    
# Tests
a = ([10]*3 + [0]*3) * 3
A = np.array(a).reshape(3,6)
a.reverse()
B = np.array(a).reshape(3,6)
test = np.vstack( [A,B] )
shape = test.shape

regs = connected( test.ravel(), shape, 5 )