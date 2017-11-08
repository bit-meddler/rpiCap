"""
    Experiments into python threading with data returns
"""
import threading
from multiprocessing import Process, Queue, Pool
import time

class compute( object ):
""" Dummy class to pretend to do a costly function
"""

    def __init__( self, private_data, config_data, id ):
        self.private_data = private_data
        self.config_data = config_data
        self.id = id
        
    def push( self, input ):
        time.sleep(2)
        print "Executing with {} & {} & {} on {}".format(
            self.private_data, self, config_data, input, self.id
        )
        time.sleep(2)
        return ( self.id, input )
        
        
if __name__ == "__main__":
    # Hello!