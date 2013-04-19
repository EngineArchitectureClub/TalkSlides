// Copyright (C) 2013 Sean Middleditch.  All right reserved.
// DigiPen Institute of Technology - Game Engine Architecture Club
// Do not use this code for any purpose besides study.  Besides
// me not giving you permission, only a lame programmer would use
// this for anything even remotely production quality.

#include "test.h"

#include <assert.h>

int main()
{
	TestDerived1 d1;
	TestDerived2 d2;
	const char* s;
	unsigned int i;
	float f;

	META_INIT(TestBase);
	META_INIT(TestDerived1);
	META_INIT(TestDerived2);

	const meta* meta = meta_find("TestDerived2");
	assert(meta != NULL);

	const meta_attribute* attr = meta_find_attribute(meta, "x");
	assert(attr != NULL);

	d2.x = 2.5f;
	d2.y = -17.3f;
	meta_get(attr, &d2, &f);
	assert(f == d2.x);

	d2._base.last_input = NULL;
	d2._base.counter = 0;
	
	const meta_event* event = meta_find_event(meta, "jumped");
	assert(event != NULL);
	f = 10.0f;
	meta_call(event, &d2, &f);

	event = meta_find_event(meta, "input");
	assert(event != NULL);
	meta_call(event, &d2, "key");
	meta_call(event, &d2, "key");
	meta_call(event, &d2, "key");

	assert(0 == strcmp(d2._base.last_input, "key"));
	assert(3 == d2._base.counter);

	meta = meta_find("TestBase");
	assert(meta != NULL);

	meta_call(event, &d2, "key");
	assert(4 == d2._base.counter);

	attr = meta_find_attribute(meta, "counter");
	assert(attr != NULL);

	meta_get(attr, &d2, &i);
	assert(4 == i);

	attr = meta_find_attribute(meta, "last_input");
	assert(attr != NULL);

	meta_get(attr, &d2, &s);
	assert(0 == strcmp("key", d2._base.last_input));

	meta = meta_find("TestDerived1");
	d1.health = 100;
	d1.damage = 17;

	event = meta_find_event(meta, "damaged");
	assert(event != NULL);

	meta_call(event, &d1, &d1.damage);
	meta_call(event, &d1, &d1.damage);
	meta_call(event, &d1, &d1.damage);

	attr = meta_find_attribute(meta, "health");
	assert(attr != NULL);
	meta_get(attr, &d1, &i);

	assert(49 == i);

	return 0;
}
