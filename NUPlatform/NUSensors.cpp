/*! @file NUSensors.cpp
    @brief Partial implementation of base sensor class

    @author Jason Kulk
 
 Copyright (c) 2009, 2010 Jason Kulk
 
 This file is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This file is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with NUbot.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "NUSensors.h"

#include "Infrastructure/NUBlackboard.h"
#include "Infrastructure/NUSensorsData/NUSensorsData.h"
#include "NUPlatform/NUPlatform.h"

#include "NUSensors/EndEffectorTouch.h"
#include "Kinematics/Horizon.h"
#include "Kinematics/Kinematics.h"
#include "Kinematics/OrientationUKF.h"

#include "Tools/Math/General.h"
#include "Motion/Tools/MotionFileTools.h"

#include "debug.h"
#include "debugverbositynusensors.h"
#include "nubotdataconfig.h"

#include <math.h>
#include <limits>
using namespace std;

/*! @brief Default constructor for parent NUSensors class, this will/should be called by children
 
    Creates the NUSensorsData instance (m_data) in which all of the sensor data is stored.
 */
NUSensors::NUSensors()
{
#if DEBUG_NUSENSORS_VERBOSITY > 0
    debug << "NUSensors::NUSensors" << endl;
#endif
    m_current_time = Platform->getTime();
    m_previous_time = -1000;
    m_data = new NUSensorsData();
    m_touch = new EndEffectorTouch();
	m_kinematicModel = new Kinematics();
	m_kinematicModel->LoadModel("None");
    m_orientationFilter = new OrientationUKF();
}

/*! @brief Destructor for parent NUSensors class.
 
    Deletes the NUSensorsData instance (m_data)
 */
NUSensors::~NUSensors()
{
#if DEBUG_NUSENSORS_VERBOSITY > 0
    debug << "NUSensors::~NUSensors" << endl;
#endif
	delete m_kinematicModel;
	m_kinematicModel = 0;
    delete m_orientationFilter;
    m_orientationFilter = 0;
}

/*! @brief Updates and returns the fresh NUSensorsData. Call this function everytime there is new data.
    @return a pointer to the freshly updated sensor data
 */
void NUSensors::update()
{
#if DEBUG_NUSENSORS_VERBOSITY > 0
    debug << "NUSensors::update()" << endl;
#endif
    m_data->PreviousTime = m_data->CurrentTime;
    m_current_time = Platform->getTime();
    m_data->CurrentTime = m_current_time;
    copyFromHardwareCommunications();       // the implementation of this function will be platform specific
    calculateSoftSensors();
    
#if DEBUG_NUSENSORS_VERBOSITY > 0
    m_data->summaryTo(debug);
#endif
    m_previous_time = m_current_time;
}

/*! @brief Returns a pointer to the NUSensorsData object used to store sensor values */
NUSensorsData* NUSensors::getNUSensorsData()
{
    return m_data;
}

/*! @brief The lower level function which gets the sensor data from the hardware itself. 
           This is a dummy function, and must be implemented by all children.
 */
void NUSensors::copyFromHardwareCommunications()
{
#if DEBUG_NUSENSORS_VERBOSITY > 4
    debug << "NUSensors::copyFromHardwareCommunications()" << endl;
#endif
    // Implementation in Platforms!
}

void NUSensors::calculateSoftSensors()
{
    calculateKinematics();
    calculateOrientation();
    calculateHorizon();
    calculateButtonDurations();

    m_touch->calculate();
    //m_zmp->calculate();

    calculateOdometry();
    calculateCameraHeight();
    calculateFallSense();
}

/*! @brief Updates the orientation estimate using the current sensor data
 */
void NUSensors::calculateOrientation()
{
#if DEBUG_NUSENSORS_VERBOSITY > 4
    debug << "NUSensors::calculateOrientation()" << endl;
#endif
    static vector<float> orientation(3, 0.0f);
    static vector<float> orientationhardware(3, 0.0f);
    static vector<float> acceleration(3, 0.0f);
    static vector<float> gyros(3, 0.0f);
    static vector<float> gyroOffset(3, 0.0f);
    
    if (m_data->get(NUSensorsData::OrientationHardware, orientationhardware))
    {
        m_data->set(NUSensorsData::Orientation, m_current_time, orientationhardware);
    }
    else if (m_data->get(NUSensorsData::Gyro, gyros) && m_data->get(NUSensorsData::Accelerometer, acceleration))
    {
        if(!m_orientationFilter->Initialised())
        {
            float accelsum = sqrt(pow(acceleration[0],2) + pow(acceleration[1],2) + pow(acceleration[2],2));
            if (fabs(accelsum - 981) < 0.2*981)
            {
                m_orientationFilter->initialise(m_current_time,gyros,acceleration);
            }
        }
        else
        {
            m_orientationFilter->TimeUpdate(gyros, m_current_time);
            vector<float> supportLegTransformFlat;
            bool validKinematics = m_data->get(NUSensorsData::SupportLegTransform, supportLegTransformFlat);
            Matrix supportLegTransform = Matrix4x4fromVector(supportLegTransformFlat);
            if(validKinematics)
                orientation = Kinematics::OrientationFromTransform(supportLegTransform);
            m_orientationFilter->MeasurementUpdate(acceleration, validKinematics, orientation);
        }

        // Set orientation
        orientation[0] = m_orientationFilter->getMean(OrientationUKF::rollAngle);
        orientation[1] = m_orientationFilter->getMean(OrientationUKF::pitchAngle);
        orientation[2] = 0.0f;
        m_data->set(NUSensorsData::Orientation, m_current_time, orientation);
        // Set gyro offset values
        gyroOffset[0] = m_orientationFilter->getMean(OrientationUKF::rollGyroOffset);
        gyroOffset[1] = m_orientationFilter->getMean(OrientationUKF::pitchGyroOffset);
        gyroOffset[2] = 0.0f;
        m_data->set(NUSensorsData::GyroOffset, m_current_time, gyroOffset);
    }
}

/*! @brief Updates the Horizon Line using the current sensor data
    
 */
void NUSensors::calculateHorizon()
{
#if DEBUG_NUSENSORS_VERBOSITY > 4
    debug << "NUSensors::calculateHorizon()" << endl;
#endif
    Horizon HorizonLine;
    vector<float> orientation;
    float headYaw, headPitch;

    bool validdata = m_data->get(NUSensorsData::Orientation, orientation);
    validdata &= m_data->getPosition(NUSensorsData::HeadYaw, headYaw);
    validdata &= m_data->getPosition(NUSensorsData::HeadPitch, headPitch);
    int camera = 1;

    if (validdata)
    {
        HorizonLine.Calculate(orientation[1], orientation[0], headYaw, headPitch, camera);
        vector<float> line;
        line.reserve(3);
        line.push_back(HorizonLine.getA());
        line.push_back(HorizonLine.getB());
        line.push_back(HorizonLine.getC());
        m_data->set(NUSensorsData::Horizon, m_current_time, line);
    }
}


/*! @brief Calculates the duration of the last press on each of the buttons and bumpers
 */
void NUSensors::calculateButtonDurations()
{
    static float nextMainDuration = 0;
    static float nextLeftDuration = 0;
    static float nextRightDuration = 0;
    
    static float prevMainState = 0;
    static float prevLeftState = 0;
    static float prevRightState = 0;
    
	float buttonvalue;
    if (m_data->get(NUSensorsData::MainButton, buttonvalue))
    {
        if (buttonvalue > 0.5)
            nextMainDuration += (m_current_time - m_previous_time);
        else if (prevMainState > 0.5)
        {   // if this is a negative edge update the last press duration
            m_data->modify(NUSensorsData::MainButton, NUSensorsData::DurationId, m_current_time, nextMainDuration);
            nextMainDuration = 0;
        }
        prevMainState = buttonvalue;
    }
    
    if(m_data->get(NUSensorsData::LeftButton, buttonvalue))
    {
        if (buttonvalue > 0.5)
            nextLeftDuration += (m_current_time - m_previous_time);
        else if (prevLeftState > 0.5)
        {   // if this is a negative edge update the last press duration
            m_data->modify(NUSensorsData::LeftButton, NUSensorsData::DurationId, m_current_time, nextLeftDuration);
            nextLeftDuration = 0;
        }
        prevLeftState = buttonvalue;
    }
    
    if (m_data->get(NUSensorsData::RightButton, buttonvalue))
    {
        if (buttonvalue > 0.5)
            nextRightDuration += (m_current_time - m_previous_time);
        else if (prevRightState > 0.5)
        {   // if this is a negative edge update the last press duration
            m_data->modify(NUSensorsData::RightButton, NUSensorsData::DurationId, m_current_time, nextRightDuration);
            nextRightDuration = 0;
        }
        prevRightState = buttonvalue;
    }
}

/*! @brief Updates the zero moment point estimate using the current sensor data
    @todo TODO: Implement this function. Fuse data from inverted pendulum ZMP, CoP and kinematic ZMP
 */
void NUSensors::calculateZMP()
{
#if DEBUG_NUSENSORS_VERBOSITY > 4
    debug << "NUSensors::calculateZMP()" << endl;
#endif
}

/*! @brief Updates the fall sense (ie BalanceFalling and BalanceFallen) using the current sensor data.
 
    Both BalanceFalling and BalanceFallen are [sum, left, right, forward, backward] where 0=not fall(ing) 1=fall(ing).
    This function should be called after calculateOrientation and calculateZMP so that those results are available.
 
    Except in special circumstances, a fall is always preceded by a falling. We transition into the falling state
    when no control action can prevent a fall. The falling is completed after the robot has impacted with the ground, 
    the impact is detected using the accelerometers, and once the robot has settled it is fallen and can start getting up.
 */
void NUSensors::calculateFallSense()
{
    static const float Fallen = 1.0;
    static const float Falling = 1.0;
    static const float FallenThreshold = 1.1;  
    static const float RollFallingThreshold = 0.55;
    static const float ForwardFallingThreshold = 0.55;
    static const float BackwardFallingThreshold = 0.45;
    static float fallen_time = 0;
#if DEBUG_NUSENSORS_VERBOSITY > 4
    debug << "NUSensors::calculateFallingSense()" << endl;
#endif
    
    vector<float> acceleration;
    vector<float> orientation;
    
    if (m_data->getAccelerometer(acceleration) and m_data->getOrientation(orientation))
    {
        float acceleration_mag = sqrt(pow(acceleration[0],2) + pow(acceleration[1],2) + pow(acceleration[2],2));
        // check if the robot has fallen over
        vector<float> fallen(5,0);
        if (fabs(acceleration_mag - 981) < 0.2*981 and (fabs(orientation[0]) > FallenThreshold or fabs(orientation[1]) > FallenThreshold))
            fallen_time += m_current_time - m_previous_time;
        else
            fallen_time = 0;
        
        if (fallen_time > 100)
        {   // To make this sensor robust to orientation sensors that fail when the robot rolls over after falling
            // We use only the angle to determine we have fallen, not the direction
            fallen[0] = Fallen;
            if (fabs(acceleration[0]) > fabs(acceleration[1]))
            {   
                if (acceleration[0] > 0)
                    fallen[3] = Fallen;
                else
                    fallen[4] = Fallen;
            }
            else
            {
                if (acceleration[1] > 0)
                    fallen[1] = Fallen;
                else
                    fallen[2] = Fallen;
            }
        }
        m_data->set(NUSensorsData::Fallen, m_current_time, fallen);
    
        // check if the robot is falling over
        vector<float> falling(5,0);
        if (not fallen[0])
        {
            if (orientation[0] < -RollFallingThreshold)
                falling[1] = Falling;
            if (orientation[0] > RollFallingThreshold)
                falling[2] = Falling;
            if (orientation[1] > ForwardFallingThreshold)
                falling[3] = Falling;
            if (orientation[1] < -BackwardFallingThreshold)
                falling[4] = Falling;
        }
        falling[0] = falling[1] + falling[2] + falling[3] + falling[4];
        m_data->set(NUSensorsData::Falling, m_current_time, falling);
    }
    else
    {
        m_data->setAsInvalid(NUSensorsData::Fallen);
        m_data->setAsInvalid(NUSensorsData::Falling);
    }
}

void NUSensors::calculateOdometry()
{
#if DEBUG_NUSENSORS_VERBOSITY > 4
    debug << "NUSensors::calculateOdometry()" << endl;
#endif

    const float turnMultiplier = 0.7;       // sd: 0.1rad (0.032 rad/rad). Measured on 12/6/2010 with ALWalkCrab
    const float xMultiplier = 1.0;         // 1.35 sd: 7.9cm (0.023 cm/cm). Measured on 12/6/2010 with ALWalkCrab
    const float yMultiplier = -1.09;        // 1.48 sd: 4.2cm (0.021 cm/cm). Measured on 12/6/2010 with ALWalkCrab

    static float prevHipYaw = 0.0;
    static float prevLeftX = 0.0;
    static float prevRightX = 0.0;
    static float prevLeftY = 0.0;
    static float prevRightY = 0.0;

    static vector<float> leftFootPosition(3,0.0f);
    static vector<float> rightFootPosition(3,0.0f);
    vector<float> odometeryData;
    if (not m_data->getOdometry(odometeryData))
        odometeryData = vector<float>(3,0);

    float hipYawPitch;
    bool yawPositionOk = m_data->getPosition(NUSensorsData::LHipYawPitch, hipYawPitch);

    const int arrayWidth = 4;
    const int translationCol = 3;

    // Get the left foot position relative to the origin
    bool leftPositionOk = false;
    bool rightPositionOk = false;
    vector<float> llegvector, rlegvector;
    Matrix leftFootTransform, rightFootTransform;
    if (m_data->get(NUSensorsData::LLegTransform, llegvector))
    {
        leftFootTransform = Matrix4x4fromVector(llegvector);
        leftFootPosition[0] = leftFootTransform[0][3];
        leftFootPosition[1] = leftFootTransform[1][3];
        leftFootPosition[2] = leftFootTransform[2][3];
        leftPositionOk = true;
    }
    if(m_data->get(NUSensorsData::RLegTransform, rlegvector))
    {
        rightFootTransform = Matrix4x4fromVector(rlegvector);
        rightFootPosition[0] = rightFootTransform[0][3];
        rightFootPosition[1] = rightFootTransform[1][3];
        rightFootPosition[2] = rightFootTransform[2][3];
        rightPositionOk = true;
    }

    if(!rightPositionOk or !leftPositionOk or !yawPositionOk) return;

    bool leftFootSupport = false;
    bool rightFootSupport = false;

    float leftForce, rightForce;
    m_data->getForce(NUSensorsData::LLeg,leftForce);
    m_data->getForce(NUSensorsData::RLeg,rightForce);

    if(leftForce > rightForce)
    {
        leftFootSupport = true;
        rightFootSupport = false;
    }
    else
    {
        leftFootSupport = false;
        rightFootSupport = true;
    }
    
    // Calculate movement
    float deltaX;
    float deltaY;
    float deltaTheta;
    
    if(leftFootSupport && !rightFootSupport)
    {
        deltaX = xMultiplier * (leftFootPosition[0] - prevLeftX);
        deltaY = yMultiplier * (leftFootPosition[1] - prevLeftY);
        deltaTheta = turnMultiplier * (hipYawPitch - prevHipYaw);
    }
    else if(!leftFootSupport && rightFootSupport)
    {
        deltaX = xMultiplier * (rightFootPosition[0] - prevRightX);
        deltaY = yMultiplier * (rightFootPosition[1] - prevRightY);
        deltaTheta = turnMultiplier * (prevHipYaw - hipYawPitch);
    }
    
    odometeryData[0] += deltaX;
    odometeryData[1] += deltaY;
    odometeryData[2] += deltaTheta;

    m_data->set(NUSensorsData::Odometry, m_data->CurrentTime, odometeryData);

    // Save the historical data
    prevHipYaw = hipYawPitch;
    prevLeftX = leftFootPosition[0];
    prevRightX = rightFootPosition[0];
    prevLeftY = leftFootPosition[1];
    prevRightY = rightFootPosition[1];
}

void NUSensors::calculateKinematics()
{
    //! TODO: Need to get these values from somewhere else.
    const int cameraNumber = 1;
    const double time = m_data->CurrentTime;		

    static vector<float> leftLegJoints(6,0.0f);
    bool leftLegJointsSuccess = m_data->getPosition(NUSensorsData::LLeg, leftLegJoints);
    static vector<float> rightLegJoints(6,0.0f);
    bool rightLegJointsSuccess = m_data->getPosition(NUSensorsData::RLeg, rightLegJoints);
    static vector<float> headJoints(2,0.0f);
    bool headJointsSuccess = m_data->getPosition(NUSensorsData::Head, headJoints);

    // Note that the kinematics uses the Matrix class, however, at this stage the NUSensorsData stores vector<float> and vector<vector<float>>
    // In this early version we continue to use the method used in 2010:
    //		- Matrices are stored in NUSensorsData as flattened vector<float> using Matrix.asVector()
    //		- Matrices are then loaded from NUSensorsData into a temporary vector<float> then a Matrix is constructed from it using Matrix4x4fromVector
	// There is no doubt this is messy, however, meh
    Matrix rightLegTransform;
    Matrix leftLegTransform;
    Matrix bottomCameraTransform;
    Matrix* supportLegTransform = 0;
    Matrix* cameraTransform = 0;

    // Calculate the transforms
    if(rightLegJointsSuccess)
    {
        rightLegTransform = m_kinematicModel->CalculateTransform(Kinematics::rightFoot,rightLegJoints);
        m_data->set(NUSensorsData::RLegTransform, time, rightLegTransform.asVector());
        m_data->modify(NUSensorsData::RLegEndEffector, NUSensorsData::EndPositionXId, time, Kinematics::PositionFromTransform(rightLegTransform));
        m_data->modify(NUSensorsData::RLegEndEffector, NUSensorsData::EndPositionRollId, time, Kinematics::OrientationFromTransform(rightLegTransform));
    }
    else
    {
        m_data->setAsInvalid(NUSensorsData::RLegTransform);
    }
    if(leftLegJointsSuccess)
    {
        leftLegTransform = m_kinematicModel->CalculateTransform(Kinematics::leftFoot,leftLegJoints);
        m_data->set(NUSensorsData::LLegTransform, time, leftLegTransform.asVector());
        m_data->modify(NUSensorsData::LLegEndEffector, NUSensorsData::EndPositionXId, time, Kinematics::PositionFromTransform(leftLegTransform));
        m_data->modify(NUSensorsData::LLegEndEffector, NUSensorsData::EndPositionRollId, time, Kinematics::OrientationFromTransform(leftLegTransform));
    }
    else
    {
        m_data->setAsInvalid(NUSensorsData::LLegTransform);
    }
    if(headJointsSuccess)
    {
        bottomCameraTransform = m_kinematicModel->CalculateTransform(Kinematics::bottomCamera,headJoints);        
    }

    // Select the appropriate ones for further calculations.
    // Choose camera.
    if(headJointsSuccess && (cameraNumber == 1))
    {
        cameraTransform = &bottomCameraTransform;
        m_data->set(NUSensorsData::CameraTransform, time, cameraTransform->asVector());
    }
    else
    {
        m_data->setAsInvalid(NUSensorsData::CameraTransform);
    }


    bool leftFootSupport = false, rightFootSupport = false;
    m_data->getSupport(NUSensorsData::LLeg,leftFootSupport);
    m_data->getSupport(NUSensorsData::RLeg,rightFootSupport);

    // Choose support leg.
    if((!leftFootSupport && rightFootSupport) && rightLegJointsSuccess)
    {
        supportLegTransform = &rightLegTransform;
    }
    else if((leftFootSupport && !rightFootSupport) && leftLegJointsSuccess)
    {
        supportLegTransform = &leftLegTransform;
    }
    else if((leftFootSupport && rightFootSupport) && leftLegJointsSuccess && rightLegJointsSuccess)
    {
        supportLegTransform = &leftLegTransform;
    }
    else
    {
        supportLegTransform = 0;
    }

    if(supportLegTransform)
    {
        m_data->set(NUSensorsData::SupportLegTransform, time, supportLegTransform->asVector());

        // Calculate transfrom matrix to convert camera centred coordinates to ground centred coordinates.
        Matrix cameraToGroundTransform = Kinematics::CalculateCamera2GroundTransform(*supportLegTransform, *cameraTransform);
        m_data->set(NUSensorsData::CameraToGroundTransform, time, cameraToGroundTransform.asVector());
    }
    else
    {
        m_data->setAsInvalid(NUSensorsData::SupportLegTransform);
        m_data->setAsInvalid(NUSensorsData::CameraToGroundTransform);
    }
    return;
}

void NUSensors::calculateCameraHeight()
{
#if DEBUG_NUSENSORS_VERBOSITY > 4
    debug << "NUSensors::calculateCameraHeight()" << endl;
#endif
    vector<float> cgtvector;
    if (m_data->get(NUSensorsData::CameraToGroundTransform, cgtvector))
    {
        Matrix cameraGroundTransform = Matrix4x4fromVector(cgtvector);
        m_data->set(NUSensorsData::CameraHeight, m_data->CurrentTime, static_cast<float>(cameraGroundTransform[2][3]));
    }
    else
    {
        m_data->setAsInvalid(NUSensorsData::CameraHeight);
    }
}



