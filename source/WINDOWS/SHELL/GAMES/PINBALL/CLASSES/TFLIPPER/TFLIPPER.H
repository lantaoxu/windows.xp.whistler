//
// Copyright (c) 2000  Microsoft Corporation
//

#include "../../pinball.h"

#ifndef PINBALL_TFLIPPER_H
#define PINBALL_TFLIPPER_H

/* 121 */
struct TFlipper;

TZmapList* __thiscall TFlipper::~TFlipper(TFlipper* this);
void TFlipper::TimerExpired(int, void*); // idb
int __thiscall TFlipper::Message(TFlipper* this, int, float); // idb
void __thiscall TFlipper::port_draw(TFlipper* __hidden this); // idb
TFlipper* __thiscall TFlipper::TFlipper(TFlipper* this, struct TPinballTable* a2, int a3);
TFlipper* __thiscall TFlipper::destroy(TFlipper *this, char a2);

void* TFlipper::vftable = &TFlipper::Message; // weak

#endif //PINBALL_TFLIPPER_H
