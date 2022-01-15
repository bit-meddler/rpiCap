
"""
    A Trivial PLL.

    The concepet is, we want the camera to be ACAP (As Close As Possible) locked to the pi's sysclock.
    such that at 0.000000 secs we are in "Frame 0" of a timecode, and at 1.000000 - 0.016667 it
    is frame 59 (Assuming a 60fps framerate).  Of course technically we are syncing on the moment
    of frame delivery, so the image is taken one 'aperture period' before the time epoch. (conincidentally
    this is exactly the same as Vicon T-Series cameras, though I expect their timing method is a lot
    better).

    The system of cameras will be synced with PTP, so provided all frameas are sampled ACAP to the
    60fps 'ticks', they should be close to shutter sync.  If you want perfect, buy Standard Deviation or
    Vicon.  If you want a 12 camera system for less than the cost of a 'proper' camera, you have to compromise.

    Of course, Implementation and real world validation my destroy this elaborate plan.

    There could be some hysteresis as both the framerate and the sysclock get adjusted (sysclock by PTP),
    and corrections over/undershoot the desired sync point.

    TODO: Feed-backward?
    Currently we just feed-forward the framerate adjustment, and don't test what happened. There is the
    Camera / Videocore 'PTS' clock which is indiependent and doesn't get adjusted by PTP.  could this help
    adjust for PTP Jumps?  It would give the true "frame period" of any adjustment made.
"""
import random


# 'Consts' - could possibly target 50000/1000, 60000/1001, and 60000/1000 (PAL, NTSC, GAME)
framerate       = 60
rate_divisor    = 1000
frame_period_us = 1_000_000 // framerate
half_period_us  = frame_period_us // 2
fps_scaled      = framerate * rate_divisor

# Windows where perturbation takes place.
# Make smaller changes the closer we get to sync lock. Don't adjust bellow 'error_low'
error_hi  = 2000 # 2.0ms
error_mid = 1000 # 1.0ms
error_low =  100 # 0.1ms

# it's possible different speed changes are needed when going Forwards Vs Backwards
adjust_bck_hi  =  int( fps_scaled * 0.03  ) # 3.0% speed change
adjust_bck_mid =  int( fps_scaled * 0.01  ) # 1.0% speed change
adjust_bck_low =  int( fps_scaled * 0.005 ) # 0.5% speed change
adjust_fwd_hi  = -int( fps_scaled * 0.03  ) # 3.0% speed change
adjust_fwd_mid = -int( fps_scaled * 0.01  ) # 1.0% speed change
adjust_fwd_low = -int( fps_scaled * 0.005 ) # 0.5% speed change

# TODO: C Implementation
# MMAL_STATUS_T mmal_res ;
# MMAL_PARAMETER_FRAME_RATE_T adjust = {{MMAL_PARAMETER_VIDEO_FRAME_RATE, sizeof(adjust)}, {fps_scaled, rate_divisor}} ;
# struct timespec sys_time ;

# SIMULATION
sys_time_us =  (frame_period_us * 2)
sys_time_us += 3141 # simulate inital deviation
next_period =  0

for i in range( 120 ):

    # SIMULATION
    # apply previous correction and simulate some drift
    sys_time_us += next_period + random.randint( -6, 48 )

    # Roll over on the second
    if( sys_time_us > 1_000_000 ):
        sys_time_us -= 1_000_000

    # TODO: As soon as frame 'published' get the system seconds
    # clock_gettime( CLOCK_REALTIME, &sys_time )
    # sys_time_us = sys_time.tv_nsec / 1000 ;

    # get time difference
    distance_from_tick = sys_time_us %  frame_period_us
    tc_frame_count     = sys_time_us // frame_period_us
    
    frame_delta = 0

    if( distance_from_tick < half_period_us ):
        # pull back towards tick point
        # we need shorter duration frames, so increase the requested framerate
        frame_delta = distance_from_tick

        if( frame_delta < error_low ):
            perturbation = 0

        elif( frame_delta < error_mid ):
            perturbation = adjust_bck_low

        elif( frame_delta < error_hi ):
            perturbation = adjust_bck_mid

        elif( frame_delta >= error_hi ):
            perturbation = adjust_bck_hi

    else:
        # push forward towards the tick point
        # this needs longer frames, so decrease the framerate
        frame_delta = frame_period_us - distance_from_tick
    
        if( frame_delta < error_low ):
            perturbation = 0

        elif( frame_delta < error_mid ):
            perturbation = adjust_fwd_low

        elif( frame_delta < error_hi ):
            perturbation = adjust_fwd_mid

        elif( frame_delta >= error_hi ):
            perturbation = adjust_fwd_hi


    adjusted_framerate = fps_scaled + perturbation

    # TODO: Update camera framerate
    # adjust.frame_rate.num = adjusted_framerate ;
    # mmal_res = mmal_port_parameter_set( camera_port, &adjust.hdr ) ;

    # SIMULATION
    next_period = 1_000_000_000 // adjusted_framerate

    print( "[{: >2}] {:0>2} Dist {: >4} Delta {: >4} Correction {: >4} -> new rate {}fps next period {}us shift time {}us".format(
            i, tc_frame_count, distance_from_tick, frame_delta, perturbation,
            ( adjusted_framerate / rate_divisor ), next_period,
            ( frame_period_us - next_period ) ),
    )


