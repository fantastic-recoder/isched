
#include <cstdlib>

#include "isched.hpp"

using isched::v0_0_1::MainSvc;

int main( const int, const char** )
{
    MainSvc mySvc;
    mySvc.run();
    return EXIT_SUCCESS;
}

