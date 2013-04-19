// Copyright (C) 2013 Sean Middleditch.  All right reserved.
// DigiPen Institute of Technology - Game Engine Architecture Club
// Do not use this code for any purpose besides study.  Besides
// me not giving you permission, only a lame programmer would use
// this for anything even remotely production quality.

#include "test.h"

#include <stdio.h>
#include <string.h>

META_DEFINE(TestBase)
META_DEFINE(TestDerived1)
META_DEFINE(TestDerived2)

META_BEGIN_ATTRS(TestBase)
	META_ATTR(last_input, MT_STRING)
	META_ATTR(counter, MT_SINT32)
META_END_ATTRS()

META_BEGIN_EVENTS(TestBase)
	META_EVENT(input, TestBase_event_input)
META_END_EVENTS()

META_BEGIN_ATTRS(TestDerived1)
	META_ATTR(health, MT_SINT32)
	META_ATTR(damage, MT_SINT32)
META_END_ATTRS()

META_BEGIN_EVENTS(TestDerived1)
	META_EVENT(damaged, TestDerived1_event_damaged)
META_END_EVENTS()

META_BEGIN_ATTRS(TestDerived2)
	META_ATTR(x, MT_FLOAT)
	META_ATTR(y, MT_FLOAT)
META_END_ATTRS()

META_BEGIN_EVENTS(TestDerived2)
	META_EVENT(jumped, TestDerived2_event_jumped)
META_END_EVENTS()

void TestBase_event_input(TestBase* base, const char* input)
{
	if (base->last_input != NULL && 0 == strcmp(input, base->last_input))
		++base->counter;
	else
	{
		base->counter = 1;
		base->last_input = input;
	}

	printf("Input on %p: %s [%d]\n", base, input, base->counter);
}

void TestDerived1_event_damaged(TestDerived1* d1, const int* amount)
{
	printf("Damage %p - %d -> %d [%d]\n", d1, d1->health, d1->health - *amount, *amount);
	d1->health -= *amount;
}

void TestDerived2_event_jumped(TestDerived2* d2, const float* height)
{
	printf("Jumped %p [%f]\n", d2, *height);
}