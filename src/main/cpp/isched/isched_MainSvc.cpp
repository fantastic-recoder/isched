//
// Created by grobap on 3.2.2024.
//
#include <memory>
#include <restbed>
#include <spdlog/spdlog.h>

#include "isched_MainSvc.hpp"

namespace isc = isched::v0_0_1;

using namespace std;
using namespace restbed;


namespace isched::v0_0_1 {
    MainSvc::MainSvc(int pPort) : mPort_(pPort) {
        mService_ = make_unique<Service>();
        mSettings_ = make_shared< Settings >( );
        mSettings_->set_port( mPort_ );
        mSettings_->set_default_header( "Connection", "close" );
    }

    MainSvc::~MainSvc() {
    }

    void MainSvc::addResolver(std::shared_ptr<BaseResolver> pResolver) {
        auto resource = make_shared< Resource >( );
        resource->set_path( pResolver->getPath() );
        resource->set_method_handler( pResolver->getMethod(), [pResolver]( const shared_ptr< Session > pSession ) {
            const auto request = pSession->get_request( );

            int content_length = request->get_header( "Content-Length", 0 );

            pSession->fetch( content_length, [pSession, pResolver](const shared_ptr< Session > pSessionPtr, const Bytes & pBodyRef )
            {
                fprintf(stdout, "%.*s\n", ( int ) pBodyRef.size( ), pBodyRef.data( ) );
                auto myResponse= pResolver->handle(pSession->get_request()->get_path());
                pSessionPtr->close(OK, myResponse, {{"Content-Length", std::to_string(myResponse.size()) } } );
            } );

        } );
        //resource->set_method_handler( "POST", post_method_handler );
        mService_->publish( resource );
    }

    void MainSvc::run() {
        spdlog::info("Isched server starting, serving GraphQL API on 'http://localhost:1984/graphql'.");
        mService_->start( mSettings_ );
        spdlog::info("Isched server stopping...");

    }
} // isched::v0_0_1
