# include <boost/python.hpp>
namespace bp=boost::python;

int test() {
	return 42 ;
} // int test()

BOOST_PYTHON_MODULE( bp_test2 ) {
	bp::def("test", test ) ;
}
