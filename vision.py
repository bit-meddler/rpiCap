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
import numpy as np
import math


class Region( object ):

    BIG_NUM = 10000 # bigger than expected image extents
    INVALID = -1000 # Hint that this Region is invalidated
    
    
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
        # merge Flag
        self.globbed         = False # Is this a glob?
        
        
    def reset( self ):
        self.bb_x, self.bb_y = self.BIG_NUM, self.BIG_NUM
        self.bb_m, self.bb_n =  0, 0
        self.sl_x, self.sl_m =  0, 0
        self.sl_n            =  0
        self.area            =  0
        self.perimeter       =  0
        self.id              = self.BIG_NUM
        self.globbed         = False
        
        
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
        self.globbed |= other.globbed
        # region id
        self.id = min( other.id, self.id )

        
    def takeSL( self, other ):
        # Take scanline data
        self.sl_x = other.sl_x
        self.sl_m = other.sl_m
        self.sl_n = other.sl_n
        
        
    def __repr__( self ):
        return "Region '{}' BB: {}-{}, {}-{}. SL: {}-{} @ {} {}merged".format(
            self.id,   self.bb_x, self.bb_y, self.bb_m, self.bb_n,
            self.sl_x, self.sl_m, self.sl_n, "" if self.globbed else "not "
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
        1) Add every region's last scan line to it's perimeter -2
        2) if region in LUT.keys we need to drop it and merge into it's parent
        3) return BBs
        
        len( lut ) == number of deletions, so we can pre-size the return array
    """
    ret = []
    id2idx = {}
    for i, reg in enumerate( reg_list ):
        reg.perimeter += (reg.sl_m - reg.sl_x) -2
        id2idx[ reg.id ] = i
    for source, target in reg_lut.iteritems():
        if not source in id2idx:
            # source region may have been merged already
            continue
        s_idx = id2idx[ source ]
        t_idx = id2idx[ target ]
        reg_list[ t_idx ].merge( reg_list[ s_idx ] )
        reg_list[ t_idx ].globbed = True
        reg_list[ s_idx ].id = Region.INVALID
    for reg in reg_list:
        if reg.id != Region.INVALID:
            ret.append( reg )
    return ret
    
    
def regs2dets( reg_list, data, threshold ):
    pass
    
    
def regs2dets1( reg_list, data=None, threshold=None ):
    # Nieve 'center of a BB' computation
    ret = []
    for reg in reg_list:
        x = abs(reg.bb_m - reg.bb_x) / 2
        y = abs(reg.bb_n - reg.bb_y) / 2
        r = (x+y)/2
        if x<y:
            score = float(x) / float(y)
        else:
            score = float(y) / float(x)
        x += reg.bb_x
        y += reg.bb_y
        ret.append( (x,y,r,score) )
    return ret

    
def regs2dets2( reg_list, data, threshold ):
    """ solving mathematically
        1) pick 3 points on the circle
            (x_1, y_1), (x_2, y_2), (x_3, y_3)
        2) solve linear system, determinant method
          [ [ (x_1**2 + y_1**2), x_1, y_1 | 1 ],
            [ (x_2**2 + y_2**2), x_2, y_2 | 1 ],
            [ (x_3**2 + y_3**2), x_3, y_3 | 1 ] ]  = 0
            
          http://www.ambrsoft.com/TrigoCalc/Circle3D.htm
          decomposes this system nicely
          
          also c.f. https://stackoverflow.com/questions/4103405/what-is-the-algorithm-for-finding-the-center-of-a-circle-from-three-points
          for a geometric method
    """
    ret = []
    x_1 = y_1 = x_2 = y_2 = x_3 = y_3 = 0
    for reg in reg_list:
        # Trace CND logo to pick three points
        x = ((reg.bb_m - reg.bb_x)/2) + reg.bb_x
        y = reg.bb_y
        while( data[y][x] < threshold ):
            y += 1
        x_1, y_1 = map( float, (x, y) )
        
        x = reg.bb_x
        y = reg.bb_n
        while( data[y][x] < threshold ):
            y -= 1
            x += 1
        x_2, y_2 = map( float, (x, y) )
        
        x = reg.bb_m
        y = reg.bb_n
        while( data[y][x] < threshold ):
            y -= 1
            x -= 1
        x_3, y_3 = map( float, (x, y) )
        
        # precompute some squares
        x_1s = x_1**2
        y_1s = y_1**2
        x_2s = x_2**2
        y_2s = y_2**2
        x_3s = x_3**2
        y_3s = y_3**2
        
        A2 = (2*((x_1*(y_2 - y_3)) - (y_1*(x_2 - x_3)) + (x_2*y_3) - (x_3*y_2)))
        
        x = ( ((x_1s + y_1s)*(y_2 - y_3)) + ((x_2s + y_2s)*(y_3 - y_1)) + ((x_3s + y_3s)*(y_1 - y_2)) ) / A2
        y = ( ((x_1s + y_1s)*(x_3 - x_2)) + ((x_2s + y_2s)*(x_1 - x_3)) + ((x_3s + y_3s)*(x_2 - x_1)) ) / A2
        r = math.sqrt( (x - x_1)**2 + (y - y_1)**2 )
        
        # pixels are not 'little squares' !
        x += 0.5
        y += 0.5
        
        score = (4*math.pi) * (float(reg.area)/(float(reg.perimeter)**2))
        ret.append( (x,y,r,score) )
    return ret
    
    
def regs2dets3( reg_list, data, threshold ):
    """
        Image Moments
        see: https://en.wikipedia.org/wiki/Image_moment
             https://homepages.inf.ed.ac.uk/rbf/CVonline/LOCAL_COPIES/SHUTLER3/node8.html
             https://books.google.co.uk/books?id=SEAOgA-oCIkC&pg=PA12&lpg=PA12&dq=image+moments+radius&source=bl&ots=ni4XG7JuE2&sig=ZFJpVeIryGZAL0K9MaG7QxA8O30&hl=en&sa=X&ved=0ahUKEwjD4M_KyJ_XAhUI5KQKHRdUCrIQ6AEISTAF#v=onepage&q=image%20moments%20radius&f=false
    """
    ret = []
    for reg in reg_list:
        # scan the BB and acumulate Image moments
        M_00 = 1e-6 # div 0 guard
        M_10 = M_01 = M_11 = M_20 = M_02 = 0.
        x = y = r = score = 0.
        for j in range( reg.bb_y, reg.bb_n ):
            for i in range( reg.bb_x, reg.bb_m ):
                pix = data[j][i]
                if( pix > threshold ):
                    pi = pix * i
                    pj = pix * j
                    M_00 += pix
                    M_10 += pi
                    M_01 += pj
                    M_20 += pi * i
                    M_02 += pj * j
                    
        # Centroids from 1st order moments            
        M_00r = 1. / M_00 # normalize 
        x = M_10 * M_00r
        y = M_01 * M_00r
        
        # X and Y radius (see wikipedia) from 2nd Order moments
        # add a little to guard div by zero
        u_20_ = (M_20 * M_00r) - (x*x) + 1e-8
        u_02_ = (M_02 * M_00r) - (y*y) + 1e-8
        r = (u_02_ + u_20_) / 2.

        # score is ration of short / long extent of the elipse
        score = (u_20_ / u_02_) if( u_20_ < u_02_ ) else (u_02_ / u_20_)

        # fix px center
        x += 0.5
        y += 0.5
            
        ret.append( (x,y,r,score) )
    return ret
    
    
regs2dets = regs2dets3
    
def regStitch( top, gutter, bottom ):
    top_tgt = []
    bot_tgt = []
    top_gutter = (gutter - 1)

    for reg in top:
        if( reg.sl_n == top_gutter ):
            top_tgt.append( reg )

    t_len = len( top_tgt )
    if( t_len < 1 ):
        # no top candidates
        return top+bottom
    
    for reg in bottom:
        if( reg.bb_y == gutter ):
            bot_tgt.append( reg )
            
    b_len = len( bot_tgt )
    if( b_len < 1 ):
        # nop bottom candidates
        return top+bottom
        
    top_tgt.sort( key=lambda x: x.bb_x )
    bot_tgt.sort( key=lambda x: x.bb_x )
    
    tti = 0
    bti = 0
    
    while( (tti < t_len) or (bti < b_len) ):
        # check first top region for connectedness to bottom
        if( top_tgt[ tti ].bb_m < bot_tgt[ bti ].bb_x ):
            tti += 1
        elif( bot_tgt[ bti ].bb_m < top_tgt[ tti ].bb_x ):
            bti += 1
        else:
            # touching?
            top_tgt[ tti ].merge( bot_tgt[ bti ] )
            top_tgt[ tti ].globbed = True # ?
            bottom.remove( bot_tgt[ bti ] )
            tti += 1
            bti += 1
            
    return top+bottom
            
    
def connectedOld( data, data_wh, threshold ):
    return connected( data, data_wh, threshold, 0, len(data) )
    
    
def connected( data, data_wh, threshold, data_in, data_out ):
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
    tmp_x, tmp_y= 0, 0      # temp x & y position of bright pixel
    # data_in     = 0         # frame to begin scanning
    # data_out    = len(data) # frame to end scanning
    factory     = RegionFactory() # Region Factory
    
    tmp_reg     = Region()  # temporary region for comparison & mergin
    master_line = Region()  # Cache of master Region's scanline for 242
    scan_line   = Region()  # Cache of current scanline
    merge_target= Region()  # holding space for a merged region
    
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
    
    # BEGIN
    idx = data_in
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
                scanning = False # soak up last non-aligned bytes
        # either we're out of data, or we've found a bright line
        if( ended ):
            return regReconcile( reg_list, reg_lut )

        if( scanning ):
            # Scanning
            # ########
            tmp_reg.reset()
            # compute scanline x,y position in rectilinear image
            if( idx > row_end ):
                tmp_y, tmp_x = divmod( idx, d_w )
                row_start    = tmp_y * d_w
                row_end      = ((tmp_y+1) * d_w) - 1 # this row in a rect image
            else:
                tmp_x        = idx - row_start # Still in the same row
                
            tmp_reg.sl_n, tmp_reg.sl_x = tmp_y, tmp_x
            
            #print "Housekeeping test at", tmp_x, tmp_y, "From", idx, "last-y", last_y, "re", row_end
            
            # housekeeping...
            if( tmp_y > last_y ):# Tidy list of found regions if we've advanced a line
                # reset reg_idx to begining of active section
                # TODO: Find a way to do this in place by swapping values, and not re-sizing the list loads.
                reg_scan = reg_start
                reg_idx  = reg_start
                reg_len  = len( reg_list )
                previous_y = tmp_y-1
                while( reg_scan < reg_len ):
                    # retire this region, it can't touch
                    #print "?", reg_list[ reg_scan ].sl_n, last_y
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
            # Finalize our region based on this scanline
            tmp_reg.update()

            # Connecting
            # ##########
            # check for tmp_reg's scanline conection to existing regions
            # Only Advance the Reg idx here
            # find a region that could plausably touch this scanline
            reg_len = len( reg_list )
            # If this is first Region (reg_len==0, reg_idx==0), we skip this phase
            #print "regi", reg_idx, reg_len
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
                inserting = True
            elif( tmp_reg.sl_m < reg_list[reg_idx].sl_x ):
                inserting = True
                
            if( inserting ):
                # Insert at end, or in front of rl[ri] as they don't touch
                #print "New reg, insert"
                new_reg = factory.newRegion()
                new_reg.merge( tmp_reg )
                # Collect statistics HERE
                new_reg.area = new_reg.sl_m - new_reg.sl_x
                new_reg.perimeter = new_reg.area
                reg_list.insert( reg_idx, new_reg )
                merging = False
                # Bail out?

            if( merging ):
                #print "in Merge Test"
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
                    
                # statistics
                tmp_reg.area = tmp_reg.sl_m - tmp_reg.sl_x
                tmp_reg.perimeter += 2
                reg_list[reg_idx].merge( tmp_reg )
                
                
            while( merging ):                
                #print "in Merging", mode
                if( mode=="W" ):
                    #print "in W", reg_idx, reg_len
                    # advance through regions, swallowing them
                    while( (reg_idx < (reg_len-1)) ): # if ri==len, we're at the end
                        if( (reg_list[reg_idx+1].sl_x <= scan_line.sl_m) and \
                            (reg_list[reg_idx+1].sl_m >= scan_line.sl_x) ):
                            #print "W merge"
                            # merge region data
                            merge_target = reg_list[reg_idx+1]
                            reg_list[reg_idx].merge( merge_target )
                            reg_list[reg_idx].globbed = True
                            # update LUT
                            if merge_target.id in reg_lut:
                                reg_lut[ merge_target.id ] = reg_list[reg_idx].id
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
                    #print "in M"
                    # scan px in this row up to master_line.sl_m
                    # if there is a hot line, create a new region, add to lut
                    line_end = (scan_line.sl_n*d_w)+master_line.sl_m
                    #print idx, row_end, line_end
                    while( idx <= line_end ):
                        if( data[idx] > threshold ):
                            # found a hot line AGAIN
                            #print "New reg, M mode"
                            new_reg = factory.newRegion()
                            new_reg.sl_x = idx - row_start
                            new_reg.sl_n = tmp_y
                            while( idx < row_end ):
                                if( data[idx] > threshold ):
                                    idx += 1
                                else:
                                    break
                            new_reg.sl_m = idx - row_start
                            new_reg.update()
                            #print new_reg
                            # collect statistics
                            new_reg.area = new_reg.sl_m - new_reg.sl_x
                            new_reg.perimeter = new_reg.area
                            new_reg.globbed = True # Born of a merge
                            reg_idx += 1
                            reg_list.insert( reg_idx, new_reg )
                            reg_len += 1
                            # update LUT
                            master_id = master_line.id
                            # There exists a corner case where the ml.id is itself an orphen.  
                            if master_id in reg_lut:
                                master_id = reg_lut[ master_id ]
                            reg_lut[ new_reg.id ] = master_id
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
 
 
class detMan( object ):
    def __init__( self, data_wh, start_idx, end_idx, threshold ):
        self.data_wh    = data_wh
        self.start_idx  = start_idx
        self.end_idx    = end_idx
        self.threshold  = threshold
        
    def push( self, data ):
        return connected( data, self.data_wh, self.threshold, self.start_idx, self.end_idx )

        
class detManM( object ):
    def __init__( self, data_wh, start_idx, end_idx, threshold, id ):
        self.data_wh    = data_wh
        self.start_idx  = start_idx
        self.end_idx    = end_idx
        self.threshold  = threshold
        self.id         = id
        
    def push( self, data, que ):
        results = connected( data, self.data_wh, self.threshold, self.start_idx, self.end_idx )
        ret = ( (self.id, results) )
        if que==None:
            return ret
        else:
            que.put( ret )
        
if( __name__ == "__main__" ):
    print" This is not the file you're looking for.  He can go about his business.  Move along."