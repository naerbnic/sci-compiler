//
// new
//
// This file contains the new and delete overloads.
//
// author: Stephen Nichols
//

#include <stdlib.h>
#include "new.hpp"

void *operator new ( size_t size )
{
    return calloc ( size, 1 );
}

void operator delete ( void *ptr )
{
    free ( ptr );
}
