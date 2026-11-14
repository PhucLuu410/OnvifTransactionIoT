#pragma once

#include <memory>
#include <map>
#include <string>

#include "Camera/Interfaces/ICameraService.hpp"

namespace camera
{
    class CameraManager
    {
    public:
        Status addCamera(const CameraConfig &config);
        Status startCamera(const std::string &camera_id);
        Status stopCamera(const std::string &camera_id);
        Status removeCamera(const std::string &camera_id);
        void stopAll();
        CameraState getCameraState(const std::string &camera_id) const;

    private:
        std::map<std::string, std::shared_ptr<ICameraService>> cameras_;
    };

}