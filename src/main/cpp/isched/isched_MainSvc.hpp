//
// Created by grobap on 3.2.2024.
//

#ifndef ISCHED_MAINSVC_HPP
#define ISCHED_MAINSVC_HPP

#include <memory>

#include "isched_BaseResolver.hpp"

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

    MainSvc(int pPort=1984);

    ~MainSvc();
    void addResolver(std::shared_ptr<BaseResolver> presolver);
    void run();
};

} // v0_0_1
// isched

#endif //MAINSVC_HPP
