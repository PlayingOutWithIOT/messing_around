//
//  testcontrollers.cpp
//  PSMoveOSX
//
//  Created by localadmin on 07/06/2018.
//

#include "BtBase.h"
#include "skipping.hpp"
#include "BaArchive.h"
#include "BtTime.h"
#include "SdSound.h"
#include "ShIMU.h"
#include "BtQueue.h"

#define MADGWICK
#include "MadgwickAHRS.h"
Madgwick madgwick[3];

void Skipping::setup()
{
    numControllers = psmove_count_connected();
    
    ShIMU::SetNumSensors( numControllers );
    
    printf("Connected PS Move controllers: %d\n", numControllers );
    
    // Connect all the controllers
    for( int i=0; i<numControllers; i++)
    {
        moveArr[i] = psmove_connect_by_id(i);
        
        // Set the rumble to 0
        psmove_set_rumble( moveArr[i], 0 );
        
        // Set the lights to 0
        psmove_set_leds( moveArr[i], 0, 0, 0 );
    }
    reset();
}

struct SkippingRope {
    BtQueue<BtFloat, 100> height;
};
SkippingRope rope[3];
MtVector3 v3RelativePosition[3];

void Skipping::reset()
{
#ifdef MADGWICK
    for( BtU32 i=0; i<numControllers; i++ )
    {
        Madgwick &madge = madgwick[i];
        madge.q0 = 1.0f; madge.q1 = madge.q2 = madge.q3 = 0;
        
        MtQuaternion quaternion = MtQuaternion( -madge.q1, -madge.q3, -madge.q2, madge.q0 );
        ShIMU::SetQuaternion( i, quaternion );
        
        rope[i].height.Empty();
    }
#endif
}

BtBool jump = BtFalse;
BtFloat avHeight[3];
BtFloat minHeight[3];
BtFloat maxHeight[3];
BtFloat currHeight[3];

void Skipping::update()
{
    jump = BtFalse;
    
    for( BtU32 i=0; i<numControllers; i++ )
    {
         PSMove *move = moveArr[i];
         
         int res = psmove_poll( move );
         if (res)
         {
            MtQuaternion quaternion;
            BtFloat fax, fay, faz;
            BtFloat fgx, fgy, fgz;
         
            for( BtU32 j=0; j<2; j++ )
            {
                PSMove_Frame frame = (PSMove_Frame)j;
                psmove_get_accelerometer_frame( move, frame, &fax, &fay, &faz );
                psmove_get_gyroscope_frame( move, frame, &fgx, &fgy, &fgz );
                
                Madgwick &madge = madgwick[i];
                madge.MadgwickAHRSupdateIMU( fgx, fgy, fgz, fax, fay, faz );
            }
         
            // Works vertically with x, z, y
            Madgwick &madge = madgwick[i];
            quaternion = MtQuaternion( -madge.q1, -madge.q3, -madge.q2, madge.q0 );
            ShIMU::SetQuaternion( i, quaternion );
            ShIMU::SetAccelerometer( i, MtVector3( fax, fay, faz ) );
         
            // Set their positions
            v3RelativePosition[i].x += fax * BtTime::GetTick();
            v3RelativePosition[i].y += fay * BtTime::GetTick();
            v3RelativePosition[i].z += faz * BtTime::GetTick();
            ShIMU::SetPosition(i, v3RelativePosition[i] );
            v3RelativePosition[i] *= 0.99f;
             
            MtMatrix4 m4Transform;
            m4Transform.SetQuaternion(quaternion);
            MtVector3 v3Grav = MtVector3( fax, fay, faz ) * m4Transform;
             
            if( i == 1 )
            {
                v3Grav.z -= 1.0f;
                if( MtSqrt( (v3Grav.x * v3Grav.x) + (v3Grav.y * v3Grav.y) + (v3Grav.z * v3Grav.z) ) > 1.0f )
                {
                    jump = BtTrue;
                    int a=0;
                    a++;
                }
            }
            else
            {
                // Only do the accelerometers checks once our buffer is full
                if( rope[i].height.IsRoom() == BtFalse )
                {
                    // Pop the oldest
                    rope[i].height.Pop();
                }
                
                // Push the latest reading
                rope[i].height.Push( v3Grav.z );
 
                // Set the current height
                currHeight[i] = v3Grav.z;
                
                // Get the averaged sampled height
                {
                    BtFloat maxLength = 0;
                    BtU32 numReadings = rope[i].height.GetItemCount();
                    
                    if( numReadings )
                    {
                        maxHeight[i] = rope[i].height.Peek(0);
                        maxLength += rope[i].height.Peek(0);
                        
                        for( BtU32 r=1; r<numReadings; r++ )
                        {
                            BtFloat currHeight = rope[i].height.Peek(r);
                            maxLength += currHeight;
                            maxHeight[i] = MtMax( maxHeight[i], currHeight );
                        }
                    }
                    
                    // Calculate the average amplitude of the movement
                    avHeight[i] = maxLength / numReadings;
                }
            }
        }
    }
    
    BtBool up = BtFalse;

    if( currHeight[0] > avHeight[0] * 1.30f )
    {
        if( currHeight[2] > avHeight[2] * 1.30f )
        {
            up = BtTrue;
            int a=0;
            a++;
        }
    }
    
    if( jump )
    {
        int a=0;
        a++;
    }
    
    if( up )
    {
        if( jump == BtFalse )
        {
            int a=0;
            a++;
        }
    }
    
    // Set the lights
    for( BtU32 i=0; i<numControllers; i++ )
    {
        PSMove *move = moveArr[i];
        
        // Setup the skipping rope
        if( i == 0 )
        {
            psmove_set_leds( move, 0, 255, 0 );
        }
        if( i == 1 )
        {
            psmove_set_leds( move, 0, 0, 255 );
        }
        if( i == 2 )
        {
            psmove_set_leds( move, 0, 255, 0 );
        }
        
        // Respond to the buttons
        unsigned int pressed, released;
        psmove_get_button_events( move, &pressed, &released);
        if( pressed == Btn_MOVE )
        {
            // Integrate the battery level and set the colour from RED to WHITE
            PSMove_Battery_Level batt = psmove_get_battery( move );
            
            switch( batt )
            {
                case Batt_MIN :
                    psmove_set_leds( move, 255, 0, 0 );
                break;
                case Batt_20Percent:
                    psmove_set_leds( move, 128, 128, 0 );
                break;
                case Batt_40Percent:
                    psmove_set_leds( move, 128, 200, 0 );
                break;
                case Batt_60Percent:
                    psmove_set_leds( move, 0, 200, 0 );
                break;
                case Batt_80Percent :
                    psmove_set_leds( move, 0, 220, 0 );
                break;
                case Batt_MAX:
                    psmove_set_leds( move, 0, 255, 0 );
                break;
                case Batt_CHARGING:
                    psmove_set_leds( move, 0, 0, 255 );
                break;
                case Batt_CHARGING_DONE:
                    psmove_set_leds( move, 255, 255, 255 );
                break;
            }
        }
        if( released )
        {
            psmove_set_leds( move, 0, 0, 0 );
        }
        
        // Update any changes to the lights
        psmove_update_leds( move );
    }
}
