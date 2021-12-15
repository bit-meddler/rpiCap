/* messing around to make a Queue with a usable payload */
#include <iostream>
#include <queue>
#include <cstring>

struct QPacket {
    int priority{} ;
    int size{} ; // size of the buffer
    char* data[] ; // pointer to an unknown sized dynamic buffer
 
    friend bool operator< (QPacket const& lhs, QPacket const& rhs) {
        // note: reversing!
        return lhs.priority > rhs.priority ;
    }
 
    friend std::ostream& operator<< (std::ostream& os, QPacket const& e) {
        return os << "{" << e.priority << ", " << e.size << ", '"  << *e.data << "'} " ;
    }
} ;

int main( void ) {

    std::priority_queue< QPacket > priorityQueue ;

    // add some elements
    char* msg = new char[6] ;
    strcpy( msg, "hello" ) ;
    priorityQueue.push( QPacket{ 0, 1, msg } ) ;
    //priorityQueue.push( QPacket{ 9, 4, 'B' } ) ;
    //priorityQueue.push( QPacket{ 4, 3, 'A' } ) ;
    //priorityQueue.push( QPacket{ 1, 2, 'C' } ) ;

    // inspect
    for( ; !priorityQueue.empty(); priorityQueue.pop() ) {
        QPacket const& x = priorityQueue.top();
        std::cout << x << ' ' ;
        //delete x.data ;
    }
    std::cout << std::endl ;

    return 0 ;
}