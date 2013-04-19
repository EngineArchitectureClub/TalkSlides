// Copyright (C) 2013 Sean Middleditch.  All right reserved.
// DigiPen Institute of Technology - Game Engine Architecture Club
// Do not use this code for any purpose besides study.  Besides
// me not giving you permission, only a lame programmer would use
// this for anything even remotely production quality.

#include "meta.h"

#include <string.h>
#include <assert.h>

static meta* s_head = 0;

void meta_add(meta* meta)
{
	assert(meta != NULL);
	meta->next = s_head;
	s_head = meta;
}

void meta_add_attribute(meta* meta, meta_attribute* attr)
{
	assert(meta != NULL && attr != NULL);
	meta_attribute* copy = (meta_attribute*)malloc(sizeof(meta_attribute));
	assert(copy != NULL);
	memcpy(copy, attr, sizeof(meta_attribute));
	copy->next = meta->attrs;
	meta->attrs = copy;
	copy->parent = meta;
}

void meta_add_event(meta* meta, meta_event* event)
{
	assert(meta != NULL && event != NULL);
	meta_event* copy = (meta_event*)malloc(sizeof(meta_event));
	assert(copy != NULL);
	memcpy(copy, event, sizeof(meta_event));
	copy->next = meta->events;
	meta->events = copy;
	copy->parent = meta;
}

const meta* meta_find(const char* name)
{
	assert(name != NULL);
	const meta* m = s_head;
	while (m != 0)
	{
		if (0 == strcmp(name, m->name))
			return m;
		m = m->next;
	}
	return 0;
}

const meta_attribute* meta_find_attribute(const meta* meta, const char* name)
{
	assert(meta != NULL && name != NULL);
	const meta_attribute* a = meta->attrs;
	while (a != 0)
	{
		if (0 == strcmp(name, a->name))
			return a;
		a = a->next;
	}
	if (meta->super != meta)
		return meta_find_attribute(meta->super, name);
	else
		return 0;
}

const meta_event* meta_find_event(const meta* meta, const char* name)
{
	assert(meta != NULL && name != NULL);
	const meta_event* e = meta->events;
	while (e != 0)
	{
		if (0 == strcmp(name, e->name))
			return e;
		e = e->next;
	}
	if (meta->super != meta)
		return meta_find_event(meta->super, name);
	else
		return 0;
}

void meta_get(const meta_attribute* attr, const void* object, void* buffer)
{
	assert(attr != NULL && object != NULL && buffer != NULL);
	switch (attr->type)
	{
	case MT_SINT32:
		*(int*)buffer = *(const int*)((const char*)object + attr->offset);
		break;
	case MT_FLOAT:
		*(float*)buffer = *(const float*)((const char*)object + attr->offset);
		break;
	case MT_STRING:
		*(const char**)buffer = *(const char**)((const char*)object + attr->offset);
		break;
	default:
		assert(false && "unknown type");
	}
}

void meta_set(const meta_attribute* attr, void* object, const void* buffer)
{
	assert(attr != NULL && object != NULL && buffer != NULL);
	switch (attr->type)
	{
	case MT_SINT32:
		*(int*)((char*)object + attr->offset) = *(const int*)buffer;
		break;
	case MT_FLOAT:
		*(float*)((char*)object + attr->offset) = *(const float*)buffer;
		break;
	case MT_STRING:
		*(const char**)((char*)object + attr->offset) = *(const char**)buffer;
		break;
	default:
		assert(false && "unknown type");
	}
}

void meta_call(const meta_event* event, void* object, const void* msg)
{
	assert(event != NULL && object != NULL);
	event->cb(object, msg);
}