// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_MainSvc.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Public interface for the main REST service wrapper.
 *
 * Exposes the `MainSvc` class that manages the underlying REST service,
 * accepts resolvers and runs the server.
 */

#ifndef ISCHED_MAINSVC_HPP
#define ISCHED_MAINSVC_HPP

#include <memory>

#include "isched_BaseRestResolver.hpp"

namespace restbed {
class Service;
    class Settings;
}

namespace isched::v0_0_1 {

class MainSvc {
    std::unique_ptr<restbed::Service> mService_;
    std::shared_ptr<restbed::Settings> mSettings_;
    uint16_t mPort_;

public:

    explicit MainSvc(int pPort=1984);

    ~MainSvc();
    void addRestResolver(std::shared_ptr<BaseRestResolver> pResolver);
    void run();
};

} // v0_0_1
// isched

#endif //MAINSVC_HPP
