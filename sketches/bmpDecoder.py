# bmp reader
import struct
import numpy as np

img_fq = "../benchmarkData/testing2_000_0000.bmp"

raw_img = np.fromfile( img_fq, dtype=np.uint8 )
type1, type2, size, _, _, os = struct.unpack( "<BBIHHI", raw_img[:14] )
if( (type1 == 66) and (type2 == 77) ):
	# expected header
	dib_sz, w, h, colPns, bpp, compression, imgSzhrez, vrez, numcols, num_imp_cols = struct.unpack( "<IiihhIIiiII", raw_img[14:(14+40)] )
	img = np.reshape(np.asarray( raw_img[os:] ), (w,h))
	rowMaj = img.T
