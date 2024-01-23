#include <restbed>
#include <spdlog/spdlog.h>

#include "isched.hpp"

namespace isc = isched::v0_0_1;

using namespace std;
using namespace restbed;

void get_method_handler( const shared_ptr< Session > session )
{
    const auto request = session->get_request( );

    int content_length = request->get_header( "Content-Length", 0 );

    session->fetch( content_length, [ ](const shared_ptr< Session > pSessionPtr, const Bytes & pBodyRef )
    {
        fprintf(stdout, "%.*s\n", ( int ) pBodyRef.size( ), pBodyRef.data( ) );
        pSessionPtr->close(OK, "Hello, World!", {{"Content-Length", "13" } } );
    } );
}

int main( const int, const char** )
{
    auto resource = make_shared< Resource >( );
    resource->set_path( "/graphql" );
    resource->set_method_handler( "GET", get_method_handler );

    auto settings = make_shared< Settings >( );
    settings->set_port( 1984 );
    settings->set_default_header( "Connection", "close" );

    Service service;
    service.publish( resource );
    spdlog::info("Isched server starting, serving GraphQL API on 'http://localhost:1984/graphql'.");
    service.start( settings );
    spdlog::info("Isched server stopping...");

    return EXIT_SUCCESS;
}

