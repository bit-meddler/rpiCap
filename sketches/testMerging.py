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
         
         
         -----------------
        -tmp-    -u-    -u--      <- Undiscovered regions
        
        
        
        
        # Merge in
        sl.update( tmp )
        ri.update( tmp)
        
        # find subsequent touched region(s)

        while( tmp.sl_m is > than ri.sl_m )
            masterline.reset()
            masterline.update( ri )
            ri.update( tmp )
            ri += 1
        # eat px till end of ri.sl_m
        sl_end = master_line
"""