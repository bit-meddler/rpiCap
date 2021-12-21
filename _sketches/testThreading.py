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
        
    def push( self, input, output=None ):
        time.sleep(1)
        print "Executing with {} & {} on {} @ {}\n".format(
            self.private_data, self.config_data, input, self.id
        )
        time.sleep(1)
        
        ret = ( self.id, time.time() )
        
        if( output == None ):
            return ret
        else:
            output.put( ret )
            
        
if __name__ == "__main__":
    num_cores = 4
    # Serial
    imperatives = [ "egg", "bacon", "beans", "spam", "lobster thermidor" ]
    func_list = [ Compute( "spam", imperatives[i], i ) for i in range( num_cores ) ]

    res_list = [None] * num_cores
    proc_list = []
    ret_q = Queue()
    t_pool= Pool( processes=num_cores )
    
    t1 = time.time()

    data = "Bacon!"
    for func in func_list:
        #worker = threading.Thread( target=func.push, args=(data,ret_q,) )
        #worker = Process( target=func.push, args=(data,ret_q,) )
        worker = t_pool.apply_async( func.push, args=(data,ret_q,) )
        
        #worker.start()
        proc_list.append( worker )
    
    #t_pool.close()
    
    for proc in proc_list:
        #proc.join()
        idx, res =  proc.get()
        res_list[ idx ] = res
        
    t2 = time.time()
    
    for i in range(num_cores):
        #idx, res =  ret_q.get()
        idx, res =  ret_q.get()
        
        res_list[ idx ] = res

    print "Total", t2-t1
    
    for i, res in enumerate( res_list ):
        print i, res-t1

    exit()
    for task in func_list:
        idx, res =  task.push( "egg" )
        res_list[ idx ] = res