import os, sys
_git_root_ = os.path.dirname( os.path.dirname( os.path.dirname( os.path.realpath(__file__) ) ) )
sys.path.append( os.path.join( _git_root_, "Gimli", "Python" ) )

from Comms.piCam import TRAIT_LOCATIONS

c_t_l = { # C types LUT
    1 : "uint8_t",
    2 : "uint16_t",
}

line_fmt = "            {type: <9} {var: <20} // {rem}"

reg_info = [ (reg, sz, name) for name, (reg, sz, _) in TRAIT_LOCATIONS.items() ]

reg_info = sorted( reg_info, key=lambda x: x[0] )
reg_info.append( (1023, 1, "OUT") )

idx = 1
unknown = 0

print( "struct camRegs {" )
print( "    union {" )
print( "        uint8_t       reg[1024] ;          // index access" )
print( "        struct {                           // variable access" )

for reg, sz, name in reg_info:
    
    gap = reg - idx
    var = "__unknown_{} ;".format( unknown )
    
    if( gap != 0 ):
        # We need to pad
        if( gap == 1 ):
            print( line_fmt.format( type=c_t_l[1], var=var, rem="Unknown" ) )
            idx += 1
            unknown += 1
            
        else:
            # Sized
            tp = "uint8_t"
            var = "__unknown_{}[{}] ;".format( unknown, gap )
            rem = "Here be Dragons" if gap>40 else "Unknown"
            print( line_fmt.format( type=c_t_l[1], var=var, rem=rem ) )
            idx += gap
            unknown += 1

    if( name == "OUT" ):
        break
    var = name + " ;"
    print( line_fmt.format( type=c_t_l[sz], var=var, rem="reg {}".format(reg) ) )
    idx += sz
    
print( "        } // var struct" )
print( "    } // union" )
print("} // camRegs")

