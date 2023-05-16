/*
    SPDX-FileCopyrightText: 2022 Toni Schriber

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/******************************************************************************************************
* Angle calculations are based on position measurements of
* - Rotator angle in "Circular Angle (A)" mode (0 <> 359.99° CCW)
* - Camera offset angle & Camera position angle in "Position Angle (PA)" mode (180 <> -179.99° CCW)
* This leads to the following calculations:
* - Camera PA = calcCameraAngel(Rotator A)
* - Rotator A = calcRotatorAngle(Camera PA)
* - Camera offset PA = calcOffsetAngle(Rotator A, Camera PA)
*******************************************************************************************************/

#include "solverutils.h"
#include "rotatorutils.h"
#include "Options.h"
#include "auxiliary/ksnotification.h"

#include <indicom.h>
#include <basedevice.h>
#include <cmath>


ISD::Mount::PierSide RotatorUtils::m_CalPierside = ISD::Mount::PIER_WEST;
ISD::Mount::PierSide RotatorUtils::m_ImgPierside = ISD::Mount::PIER_UNKNOWN;
double RotatorUtils::m_Offset = Options::pAOffset();
bool RotatorUtils::m_flippedMount = false;

RotatorUtils::RotatorUtils() {}

RotatorUtils::~RotatorUtils(){}

void RotatorUtils::initRotatorUtils()
{
    for (auto &oneDevice : INDIListener::Instance()->getDevices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
            continue;
        if ((m_Mount = oneDevice->getMount()))
        {
            connect(m_Mount, &ISD::Mount::pierSideChanged, [=] (ISD::Mount::PierSide Side)
            {
                if (Side == m_CalPierside)
                    m_flippedMount = false;
                else
                    m_flippedMount = true;
                emit(changedPierside(Side));
            });
        }
        else
        {
            KSNotification::error(
                i18n("Rotator Utilities: Mount %1 is not ready!", oneDevice->getDeviceName()));
        }
    }
}

double RotatorUtils::calcRotatorAngle(double PositionAngle)
{
    if (m_flippedMount)
    {
        PositionAngle += 180;
    }
    return Range360(PositionAngle - m_Offset);
}

double RotatorUtils::calcCameraAngle(double RotatorAngle, bool flippedImage)
{
    double PositionAngle = 0;
    if (RotatorAngle > 180)
    {
        PositionAngle = (RotatorAngle - 360) + m_Offset;
    }
    else
    {
        PositionAngle = RotatorAngle + m_Offset;
    }
    if (!m_flippedMount != !flippedImage) // XOR
    {
        if (PositionAngle > 0)
        {
            PositionAngle -= 180;
        }
        else
        {
            PositionAngle += 180;
        }

    }
    return RangePA(PositionAngle);
}

double RotatorUtils::calcOffsetAngle(double RotatorAngle, double PositionAngle)
{
    double OffsetAngle = 0;
    if (RotatorAngle > 180)
    {
        OffsetAngle = PositionAngle - (RotatorAngle - 360);
    }
    else
    {
        OffsetAngle = PositionAngle - RotatorAngle;
    }
    if (m_flippedMount)
    {
        OffsetAngle -= 180;
    }
    return RangePA(OffsetAngle);
}

void RotatorUtils::updateOffset(double Angle)
{
    m_Offset = Angle;
    Options::setPAOffset(Angle);
}

ISD::Mount::PierSide RotatorUtils::getMountPierside()
{
    return(m_Mount->pierSide());
}

void RotatorUtils::setImagePierside(ISD::Mount::PierSide ImgPierside)
{
    m_ImgPierside = ImgPierside;
}

bool RotatorUtils::checkImageFlip()
{
    bool flipped = false;

    if (m_ImgPierside != ISD::Mount::PIER_UNKNOWN)
        if (!m_flippedMount != (m_ImgPierside == m_CalPierside)) // XOR
            flipped = true;
    return flipped;
}

// Redefiniton of 'rangePA'
double RotatorUtils::RangePA(double pa)
{
    {
        while (pa > 180.00)
            pa -= 360;
        while (pa <= -179.99) // uniqueness of angle (-180 = 180)
            pa += 360;
        return pa;
    }
}

// Redefiniton of 'range360'
double RotatorUtils::Range360(double r)
{
    double res = r;
    while (res < 0.00)
        res += 360.00;
    while (res > 359.99)  // uniqueness of angle (360 = 0)
        res -= 360.00;
    return res;
}

double RotatorUtils::DiffPA(double diff)
{
    if (diff > 180)
        return (360 - diff);
    else
        return diff;
}
