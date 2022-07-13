/* date = January 29th 2021 8:23 pm */

#ifndef FCODER_FLEURY_BASE_COMMANDS_H
#define FCODER_FLEURY_BASE_COMMANDS_H

//~ NOTE(jack)

enum Soft_Center_Pos {
    SoftCenterPos_None,
    SoftCenterPos_Top,
    SoftCenterPos_Center,
    SoftCenterPos_Bottom,
};

global Soft_Center_Pos JP_CurrentSoftPos;
global f32 JP_CurrentSoftPosDecayTime;

#endif // FCODER_FLEURY_BASE_COMMANDS_H
