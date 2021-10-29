#pragma once

#include "cinder/MotionManager.h"
#include "cinder/app/App.h"
#include "cinder/Log.h"

using namespace ci;
using namespace ci::app;

struct MotionHelper
{
    mat4            deviceRotation;

    void setup()
    {
        CI_LOG_V( "gyro available: " << MotionManager::isGyroAvailable() );
        
        MotionManager::enable( 1000.0f/*, MotionManager::SensorMode::Accelerometer*/ );
        
        auto updateFn = [this]
        {
            if ( MotionManager::isEnabled() )
            {
                deviceRotation = inverse( MotionManager::getRotationMatrix() );
            }
        };
        
        App::get()->getSignalUpdate().connect(updateFn);
    }
    
};
