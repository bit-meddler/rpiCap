"""
    Experiments into python threading with data returns
"""
import threading
from multiprocessing import Process, Queue, Pool
import time

class Compute( object ):
    """ Dummy class to pretend to do a costly function
    """

    def __init__( self, private_data, config_data, id ):
        self.private_data = private_data
        self.config_data = config_data
        self.id = id
        
    def push( self, input ):
        time.sleep(1)
        print "Executing with {} & {} on {} @ {}".format(
            self.private_data, self.config_data, input, self.id
        )
        time.sleep(1)
        return ( self.id, input )
        
        
if __name__ == "__main__":
    num_cores = 4
    # Serial
    imperatives = [ "egg", "bacon", "beans", "spam", "lobster thermidor" ]
    task_list = [ Compute( "spam", imperatives[i], i ) for i in range( num_cores ) ]

    for task in task_list:
        print( task.push( "egg" ) )