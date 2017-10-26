# experiment to develop better region tidying algo

class MiniReg( object ):
    def __init__( self, x, m, n, id ):
        self.sl_x, self.sl_m, self.sl_n, self.id = x, m, n, id

    def __repr__( self ):
        return "Region: {} having {} to {} at {}".format(
                self.id, self.sl_x, self.sl_m, self.sl_n )

""" Some regions:

    last_y    = 13
    reg_idx   =  4
    reg_start =  0

    1   2   3   4   5
    --  __  --  --  __


    after tidying should be

    reg_idx   = 3
    reg_start = 3

    1   3   4   2   5
    --  --  --  __  __


"""
reg_idx, reg_start, last_y = -1, -1, -1


def tidy1( reg_list ):
    global reg_idx, reg_start, last_y
    
    # New method
    reg_scan = reg_start
    reg_idx  = reg_start
    reg_len  = len( reg_list )
    while( reg_scan < reg_len ):
        # print "--"
        # print "ri: {}, rs:{}".format( reg_idx, reg_scan ) 
        # retire this region, it can't touch
        if( reg_list[ reg_scan ].sl_n == last_y ):
            # can we do this in place?
            reg_list.insert( reg_idx, reg_list.pop( reg_scan ) )
            reg_idx += 1
        reg_scan += 1
        # print "ri: {}, rs:{}".format( reg_idx, reg_scan ) 
        # show( reg_list )        
    reg_start = reg_idx
    

def show( rl ):
    txt = ""
    for r in rl:
        txt += "{}@{}, ".format( r.id, r.sl_n )
    print txt[:-2]

test1 = ( (5,6,13, 1), (9,10,14, 2), (13,14,13, 3), (17,18,13, 4), (21,22,14, 5) )
reg_list = []
for x, m, n, id in test1:
    reg_list.append( MiniReg( x, m, n, id ) )
show( reg_list )

last_y = 13
reg_start = 0
reg_idx = 4

tidy1( reg_list )
show( reg_list )

# add new stuff
# update
reg_list[4].sl_n = 15
reg_list.insert( reg_idx, MiniReg( 2,5,15,6 ) )
reg_list.insert( 4, MiniReg( 12,15,15,7 ) )
show( reg_list )

last_y = 14

tidy1( reg_list )
show( reg_list )