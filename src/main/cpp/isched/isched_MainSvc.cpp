//
// Created by grobap on 3.2.2024.
//
#include <restbed>
#include <spdlog/spdlog.h>

#include "isched_MainSvc.hpp"

namespace isc = isched::v0_0_1;

using namespace std;
using namespace restbed;

namespace {
    void get_method_handler( const shared_ptr< Session > session )
    {
        const auto request = session->get_request( );

        int content_length = request->get_header( "Content-Length", 0 );

        session->fetch( content_length, [ ](const shared_ptr< Session > pSessionPtr, const Bytes & pBodyRef )
        {
            fprintf(stdout, "%.*s\n", ( int ) pBodyRef.size( ), pBodyRef.data( ) );
            pSessionPtr->close(OK, "[get] Hello, World!", {{"Content-Length", "13" } } );
        } );
    }

    void post_method_handler( const shared_ptr< Session > pSession )
    {
        const auto request = pSession->get_request( );

        int content_length = request->get_header( "Content-Length", 0 );

        pSession->fetch( content_length, [ ](const shared_ptr< Session > pSessionPtr, const Bytes & pBodyRef )
        {
            fprintf(stdout, "%.*s\n", ( int ) pBodyRef.size( ), pBodyRef.data( ) );
            pSessionPtr->close(OK, "[post] Hello, World!", {{"Content-Length", "13" } } );
        } );
    }
}

namespace isched::v0_0_1 {
    void MainSvc::run() {
        auto resource = make_shared< Resource >( );
        resource->set_path( "/graphql" );
        resource->set_method_handler( "GET", get_method_handler );
        resource->set_method_handler( "POST", post_method_handler );

        auto settings = make_shared< Settings >( );
        settings->set_port( 1984 );
        settings->set_default_header( "Connection", "close" );

        Service service;
        service.publish( resource );
        spdlog::info("Isched server starting, serving GraphQL API on 'http://localhost:1984/graphql'.");
        service.start( settings );
        spdlog::info("Isched server stopping...");

    }
} // isched::v0_0_1
