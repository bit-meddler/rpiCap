/* messing around to make a Queue with a usable payload */
#include <iostream>
#include <queue>
#include <cstring>

struct QPacket {
    int      priority ; // priority
    size_t   size ;     // size of the buffer
    uint8_t* data ;     // pointer to a dynamic buffer (The datagram)
 
    QPacket( const int priority, const size_t size, uint8_t* datagram ) :
             priority(priority), size(size), data(datagram) {}

    friend bool operator< (QPacket const& lhs, QPacket const& rhs) {
        // note: reversing for "lowest is best" ordering
        return lhs.priority > rhs.priority ;
    }
 
    friend std::ostream& operator<< (std::ostream& os, QPacket const& p) {
        return os << "{" << p.priority << " [" << p.size << "]} " ;
    }
} ;

void hexdump( const uint8_t* data, const size_t size ) {
    size_t idx = 0 ;
    while( idx < size ) {
        for( size_t i=0; i<16 && idx<size; i++ ) {
            printf( "%02x ", data[ idx ] ) ;
            idx++ ;
            if( i==7 ){
                printf( ". " ) ;
            }
        }
        printf( "\n" ) ;
    }
}

void arrfill( uint8_t* data, const size_t size ) {
    for( size_t i=0; i<size; i++ ) {
        data[i] = i ;
    }
}


int main( void ) {

    std::priority_queue< QPacket > priorityQueue ;

    // Make a buffer
    size_t SIZE = 64 ;
    uint8_t* msg = new uint8_t[ SIZE ]{0} ;
    arrfill( msg, SIZE ) ;

    // Make a packet
    QPacket* dgm = new QPacket(1, SIZE, msg ) ;
    std::cout << *dgm << std::endl ;

    priorityQueue.push( *dgm ) ;

    // inspect
    std::cout << "Inspecting" << std::endl ;
    for( ; !priorityQueue.empty(); priorityQueue.pop() ) {
        QPacket x = priorityQueue.top();
        std::cout << x << std::endl ;
        printf( "WTF is going on %d, [%d]\n", x.size, x.data[12] ) ;
        delete [] x.data ;
    }
    std::cout << "Eggs" << std::endl ;

    return 0 ;
}