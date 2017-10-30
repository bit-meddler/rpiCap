# Some ideas around Merging
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
                
                
           0     1     2            3
         ---   ---   ---          ---
                         -----
                    
                    
                
        # There are two key merge conditions, W and M
                    
        ---   ----   ----
         ----tmp_reg---
         
         
         ----------------- -rouge-
        -tmp-    -u-    -u--      <- Undiscovered regions
        
        
        
        ri touches tmp
        
        
        # Merge in
        sl.update( tmp )
        ml.update( ri  )
        ri.update( tmp )  
        
        if sl.m > ri.m:
        	mode = 'W'
    	else:
    		mode = 'M'
    		
        # find subsequent touched region(s)
		if mode=='M' :
        	while( sl.m is > ri.m )
				ri.update( tmp )
				ri += 1
            
        # eat px till end of ri.sl_m
        sl_end = master_line
        idx += 1
        while( idx < sl_end ):
        	while( idx < sl_end );
        		idx += 1
        		if( data[idx] > threashold ):
        			# New Region
        			while( data[idx] > threashold ):
        				idx += 1
        			# complete region
        			ri += 1
        			# add region at ri
        # finally test for rouge region we extended into
        if( ri.sl_m > ri+1.sl_x ):
        	# sl_
            
            
    
    # update statistics idea
    def update( self, perm=None ):
        # ...
        a = self.sl_m - self.sl_x
        p = 2 # any 'body' line adds 2 to the perimeter
        if( perim != None ):
            # this is the first or last line
            p = a
        self.perimeter += p
        self.area += a
        
"""

