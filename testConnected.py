"""
test of Connected Components

"""
import numpy as np
import cv2
import math
import random
import vision
    
# Tests
def drawDets( image, det_list, col=(0,0,200) ):
    i_h, i_w, _ = image.shape
    for x,y,r,s in det_list:
        x_, y_, r_ = map(int,(x,y,r))
        c_col = (0,0,200) if s < 0.66 else (200,0,0)
        cv2.circle( image, (x_,y_), max(1,r_), c_col, 1, 1, 0)
        cv2.line( image, (max(0,x_-2),y_), (min(i_w,x_+2),y_), col, 1 )
        cv2.line( image, (x_,max(0,y_-2)), (x_,min(i_h,y_+2)), col, 1 )
        
    
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
if False:
    a = ([10]*3 + [0]*13) * 3
    A = np.array(a).reshape(3,-1)
    a.reverse()
    B = np.array(a).reshape(3,-1)
    test = np.vstack( [A,B] )
    print test
    regs = vision.connectedOld( test.ravel(), test.T.shape, 5 )
    print vision.regs2dets( regs )

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
if False:
    A = np.zeros( (8,24), dtype=np.uint8 )
    B = np.ones( (3,3), dtype=np.uint8 ) * 10
    pos = ( (0,1), (0,5), (0,20), (4,3), (5, 19) )
    for y,x in pos:
        A[y:y+3, x:x+3] = B
    test = A
    print test
    regs = vision.connectedOld( test.ravel(), test.T.shape, 5 )
    print vision.regs2dets( regs )
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
if False:
    A = np.zeros( (8,24), dtype=np.uint8 )
    pos = ( (0,1), (0,5), (1,20), (3,3), (4, 19) )
    for y,x in pos:
        A[y:y+3, x:x+3] = B
    test = A
    print test
    regs = vision.connectedOld( test.ravel(), test.T.shape, 5 )
    print vision.regs2dets( regs )
"""
And an M merge...
    0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23  24 25 26 27 28 29 30 31
0 [ 0 10 10 10  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0   0  0  0  0  0  0  0  0]
1 [ 0 10 10 10  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0   0  0  0  0  0  0  0  0]
2 [ 0 10 10 10  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0   0  0  0  0  0  0  0  0]
3 [ 0  0  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0   0  0  0  0  0  0  0  0]
4 [ 0  0  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0 10 10  10  0  0  0  0  0  0  0]
5 [ 0  0  0 10 10 10  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0 10 10  10  0  0  0  0  0  0  0]
6 [ 0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0 10 10 10  0 10 10  10  0  0  0  0  0  0  0]
7 [ 0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0   0  0  0  0  0  0  0  0]
"""
if False:
    A = np.zeros( (8,32), dtype=np.uint8 )
    pos = ( (0,1), (0,5), (1,20), (3,3), (4, 18), (4, 22), (1, 10) )
    for y,x in pos:
        A[y:y+3, x:x+3] = B
    test = A
    print test
    regs = vision.connectedOld( test.ravel(), test.T.shape, 5 )
    print vision.regs2dets( regs )


# Test with basic Images    
if False:
    img = np.zeros( (100,100), dtype=np.uint8 )
    cv2.circle( img, (50,50), 10, (200), -1, 1, 0)
    regs = vision.connectedOld( img.ravel(), img.T.shape, 155 )
    print vision.regs2dets( regs )
    cv2.imshow( 'Test Image', img )
    cv2.waitKey( 0 )
    cv2.destroyAllWindows()

    
# Test Globbed Detections
if False:
    tests = {   "Test Image":  ((50,50,10),),
                "Test Blobs":  ((10,10,6), (10,15,6), (20,10,6), (50,50,6), (44,33,6), (60,60,6)),
                "Test Blobs2": ((10,10,6), (10,18,3), (20,20,8), (50,50,6)),
                "Test Dots":   ((10,10,6), (20,18,3), (40,40,8), (60,60,6))
    }
    for test in tests:
        img = np.zeros( (100,100), dtype=np.uint8 )
        for x,y,r in tests[test]:
            cv2.circle( img, (x,y), r, (200), -1, 1, 0)
        regs = vision.connectedOld( img.ravel(), img.T.shape, 155 )
        dets = vision.regs2dets( regs )
        print tests[test], dets
        retort = cv2.cvtColor( img, cv2.COLOR_GRAY2RGB )
        drawDets( retort, dets )
        cv2.imshow( test, retort )
        cv2.waitKey( 0 )
        cv2.destroyAllWindows()
    
# Test ROI processing and gutterballs
if False:
    img = np.zeros( (200,200), dtype=np.uint8 )
    for x,y,r in ((20,20,10), (75,75,10), (120,120,5), (180,180,10), (100,100,10), (20,97,6)):
        cv2.circle( img, (x,y), r, (200), -1, 1, 0)
    split_line = 100
    split_point = split_line*200
    img_end = 200*200
    threshold = 155
    regs_0 = vision.connected( img.ravel(), img.T.shape, threshold, 0, split_point )
    regs_1 = vision.connected( img.ravel(), img.T.shape, threshold, split_point, img_end )
    
    dets_0 = vision.regs2dets( regs_0, img, threshold )
    dets_1 = vision.regs2dets( regs_1, img, threshold )
    
    print dets_0, dets_1
    
    retort = cv2.cvtColor( img, cv2.COLOR_GRAY2RGB )
    drawDets( retort, dets_0 )
    drawDets( retort, dets_1, (0,200,0))
    
    cv2.imshow( "RoI test 1", retort )
    cv2.waitKey( 0 )
    cv2.destroyAllWindows()
    
    retort = cv2.cvtColor( img, cv2.COLOR_GRAY2RGB )
    regs = vision.regStitch( regs_0, split_line, regs_1 )
    dets = vision.regs2dets( regs, img, threshold )
    print dets
    
    drawDets( retort, dets )
    
    cv2.imshow( "RoI test 2", retort )
    cv2.waitKey( 0 )
    cv2.destroyAllWindows()
    
if False:
    img = np.zeros( (720,1280), dtype=np.uint8 )
    for i in range( 18 ):
        x = random.randint(12, 1270)
        y = random.randint(10, 700)
        r = random.randint(2, 7)
        
        cv2.circle( img, (x,y), r, (200), -1, 1, 0)
    split_line  = 720/2
    split_point = split_line*200
    img_end = img.size
    threshold = 155
    regs_0 = vision.connected( img.ravel(), img.T.shape, threshold, 0, split_point )
    regs_1 = vision.connected( img.ravel(), img.T.shape, threshold, split_point, img_end )
    
    dets_0 = vision.regs2dets( regs_0, img, threshold )
    dets_1 = vision.regs2dets( regs_1, img, threshold )
    
    retort = cv2.cvtColor( img, cv2.COLOR_GRAY2RGB )
    drawDets( retort, dets_0 )
    drawDets( retort, dets_1, (0,200,0))
    
    cv2.imshow( "RoI test 1", retort )
    cv2.waitKey( 0 )
    cv2.destroyAllWindows()
    
    retort = cv2.cvtColor( img, cv2.COLOR_GRAY2RGB )
    regs = vision.regStitch( regs_0, split_line, regs_1 )
    dets = vision.regs2dets( regs, img, threshold )

    print dets
    
    drawDets( retort, dets )
    
    cv2.imshow( "RoI test 2", retort )
    cv2.waitKey( 0 )
    cv2.destroyAllWindows()
    
if True:
    # Test scatter/Gather processing
    import threading
    import Queue
    import time
    
    img_hw = (720,1280)
    img_wh = (1280,720)
    
    img = np.zeros( (720,1280), dtype=np.uint8 )
    
    num_cores   = 1
    split_line  = img_wh[1] / num_cores
    split_point = split_line*img_wh[0]
    img_end = img.size
    threshold = 155
    strip_list = []
    proc_list = []
    
    for i in range( num_cores ):
        strip_list.append( vision.dotManM( img_wh, i*split_point, (i+1)*split_point, threshold, i ) )
    t1 = time.time()
    
    for i in range( 78 ):
        x = random.randint(12, img_wh[0])
        y = random.randint(10, img_wh[1])
        r = random.randint(2, 7)
        cv2.circle( img, (x,y), r, (200), -1, 1, 0)
    t2 = time.time() 
    print "make img", t2-t1
    
    ret_q = Queue.Queue()
    data = img.ravel()
    for strip in strip_list:
        thread = threading.Thread( target=strip.push, args=(data, ret_q, ) )
        proc_list.append( thread )
        thread.start()
        
    for proc in proc_list:
        proc.join()
    t3 = time.time()
    print "proc", t3-t2
    
    ret_list = [ None ] * num_cores
    for _ in range(num_cores):
        idx, result = ret_q.get()
        ret_list[ idx ] = result
        
    regs = ret_list[0]    
    for i in range(len(ret_list)-1):
        regs = vision.regStitch( regs, i*split_line, ret_list[i+1] )

    dets = vision.regs2dets( regs, img, threshold )
    t4 = time.time()
    print "finalize", t4-t3
    
    #print dets
    
    retort = cv2.cvtColor( img, cv2.COLOR_GRAY2RGB )    
    drawDets( retort, dets )
    for i in range( num_cores ):
        y_ = i*split_line
        cv2.line( retort, (0,y_), (img_wh[0],y_), (250,250,250), 1 )
        
    cv2.imshow( "Map/Reduce Test", retort )
    cv2.waitKey( 0 )
    cv2.destroyAllWindows()
    
    