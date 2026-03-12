// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_rest_hello_world.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Legacy Restbed hello-world sanity test (legacy).
 *
 * A minimal Restbed POST endpoint exercise, originally used to validate
 * the REST transport layer.  Kept for reference; scheduled for removal
 * in Phase 7 together with the REST transport.
 *
 * @deprecated REST layer is superseded by the GraphQL transport.
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
