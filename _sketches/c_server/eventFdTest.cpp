/* test to see how event_fd works, and if it works with select in a predictable way */

#include <sys/types.h>
#include <iostream>

#include <sys/select.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include <thread>
#include <time.h>


int my_fd1 = -1 ;
int my_fd2 = -1 ;
const size_t EVFD_SZ = sizeof( uint64_t ) ;

void ticks( int fd, int delay ) {
    uint64_t evfd_cnt = 1 ;
    size_t write_sz = 0;
    while( evfd_cnt > 0 ) {
        sleep( delay ) ;
        write_sz = write( fd, &evfd_cnt, EVFD_SZ ) ;
        if( write_sz != EVFD_SZ ) {
            std::cout << "ERROR writing" << std::endl ;
            exit( EXIT_FAILURE ) ;
        }
    }
}

int main( int argc, char* args[] ) {

    // create efd
    my_fd1 = eventfd( 0, 0 ) ;
   
    if( my_fd1 == -1 ) {
        std::cout << "ERROR creating eventfd" << std::endl ;
        exit( EXIT_FAILURE ) ;
    }
    my_fd2 = eventfd( 0, 0 ) ;
       
    if( my_fd2 == -1 ) {
        std::cout << "ERROR creating eventfd" << std::endl ;
        exit( EXIT_FAILURE ) ;
    }

    // setup metronome thread
    std::thread first(  ticks, my_fd1, 2 ) ;
    std::thread second( ticks, my_fd2, 3 ) ;

    // Go into select mode
    int num_fds = my_fd2 + 1 ;

    fd_set readable_fds ;
    struct timeval select_timeout ;
    int ready_fd = 0 ;
    size_t recv_size = 0 ;
    uint64_t evfd_cnt = 0 ;
    while( 1 ) {
        select_timeout.tv_sec = 5 ;
        select_timeout.tv_usec = 0 ;
        FD_ZERO( &readable_fds ) ;
        FD_SET( my_fd1, &readable_fds) ;
        FD_SET( my_fd2, &readable_fds) ;

        ready_fd = select( num_fds, &readable_fds, NULL, NULL, &select_timeout) ;
        if( ready_fd > 0) {
            if( FD_ISSET( my_fd1, &readable_fds ) ) {
                //
                recv_size = read( my_fd1, &evfd_cnt, EVFD_SZ ) ;
                if( recv_size != EVFD_SZ ) {
                    std::cout << "ERROR reading" << std::endl ;
                    exit( EXIT_FAILURE ) ;
                }
                std::cout << "1>" << evfd_cnt << "<" << std::endl ;
                FD_CLR( my_fd1, &readable_fds ) ;
            }

            if( FD_ISSET( my_fd2, &readable_fds ) ) {
                //
                recv_size = read( my_fd2, &evfd_cnt, EVFD_SZ ) ;
                if( recv_size != EVFD_SZ ) {
                    std::cout << "ERROR reading" << std::endl ;
                    exit( EXIT_FAILURE ) ;
                }
                std::cout << "2>" << evfd_cnt << "<" << std::endl ;
                FD_CLR( my_fd2, &readable_fds ) ;
            }

        }
    }
}
