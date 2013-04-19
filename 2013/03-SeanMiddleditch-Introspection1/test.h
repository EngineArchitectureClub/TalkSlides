// Copyright (C) 2013 Sean Middleditch.  All right reserved.
// DigiPen Institute of Technology - Game Engine Architecture Club
// Do not use this code for any purpose besides study.  Besides
// me not giving you permission, only a lame programmer would use
// this for anything even remotely production quality.

#include "meta.h"

struct TestBase {
	const char* last_input;
	int counter;
};

struct TestDerived1 {
	struct TestBase _base;

	int health;
	int damage;
};

struct TestDerived2 {
	struct TestBase _base;

	float x;
	float y;
};

void TestBase_event_input(TestBase* base, const char* key);
void TestDerived1_event_damaged(TestDerived1* d1, const int* amount);
void TestDerived2_event_jumped(TestDerived2* d2, const float* height);

META_BASE_DECLARE(TestBase)
META_DECLARE(TestDerived1, TestBase)
META_DECLARE(TestDerived2, TestBase)