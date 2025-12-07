/**
 *
 */
#include <memory>
#include <cstdlib>
#include <iostream>
#include <restbed>

using namespace std;
using namespace restbed;

static void post_method_handler( const shared_ptr< Session > pSession )
{
    const auto request = pSession->get_request( );

    const int myContentLen = request->get_header( "Content-Length", 0 );

    pSession->fetch( myContentLen, [ ]( const shared_ptr< Session > pSessionPtr, const Bytes & body )
    {
        const string myArg{body.begin(), body.end()};
        if (myArg=="exit") {
            pSessionPtr->close( OK, "Bye, World!", { { "Content-Length", "13" } } );
            exit(0);
        }
        cout << "Got: " << myArg << endl;
        pSessionPtr->close( OK, "Hello, World!", { { "Content-Length", "13" } } );
    } );
}

int main( const int, const char** )
{
    auto resource = make_shared< Resource >( );
    resource->set_path( "/resource" );
    resource->set_method_handler( "POST", post_method_handler );

    auto settings = make_shared< Settings >( );
    settings->set_port( 1984 );
    settings->set_default_header( "Connection", "close" );

    Service service;
    service.publish( resource );
    service.start( settings );

    return EXIT_SUCCESS;
}
