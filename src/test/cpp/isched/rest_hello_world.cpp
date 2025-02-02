/**
 *
 */
#include <memory>
#include <cstdlib>
#include <iostream>
#include <restbed>

using namespace std;
using namespace restbed;

void post_method_handler( const shared_ptr< Session > pSession )
{
    const auto request = pSession->get_request( );

    int content_length = request->get_header( "Content-Length", 0 );

    pSession->fetch( content_length, [ ]( const shared_ptr< Session > session, const Bytes & body )
    {
        string myArg{body.begin(), body.end()};
        if (myArg=="exit") {
            exit(666);
        }
        cout << "Got: " << myArg << endl;
        session->close( OK, "Hello, World!", { { "Content-Length", "13" } } );
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
