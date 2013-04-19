// Copyright (C) 2013 Sean Middleditch.  All right reserved.
// DigiPen Institute of Technology - Game Engine Architecture Club
// Do not use this code for any purpose besides study.  Besides
// me not giving you permission, only a lame programmer would use
// this for anything even remotely production quality.

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct meta meta;
typedef struct meta_attribute meta_attribute;
typedef struct meta_event meta_event;
typedef enum meta_type meta_type;

enum meta_type
{
	MT_VOID,
	MT_SINT32,
	MT_FLOAT,
	MT_STRING,
};

struct meta
{
	const char* name;
	const meta* super;
	const meta* next;
	unsigned size;
	meta_attribute* attrs;
	meta_event* events;
};

struct meta_attribute
{
	const char* name;
	const meta* parent;
	const meta_attribute* next;
	unsigned offset;
	meta_type type;
};

typedef void(*meta_event_cb)(void* receiver, const void* msg);

struct meta_event
{
	const char* name;
	const meta* parent;
	const meta_event* next;
	meta_event_cb cb;
};

void meta_add(meta* meta);
void meta_add_attribute(meta* meta, meta_attribute* attr);
void meta_add_event(meta* meta, meta_event* event);

const meta* meta_find(const char* name);
const meta_attribute* meta_find_attribute(const meta* meta, const char* name);
const meta_event* meta_find_event(const meta* meta, const char* name);

void meta_get(const meta_attribute* attr, const void* object, void* buffer);
void meta_set(const meta_attribute* attr, void* object, const void* buffer);
void meta_call(const meta_event* event, void* object, const void* message);

#define META(NAME) ((const meta*)&g_meta__ ## NAME)

#define META_DECLARE(NAME, SUPER) \
	extern meta g_meta__ ## NAME; \
	static const meta* g_super__ ## NAME = META(SUPER); \
	extern void init_meta_attrs__ ## NAME (); \
	extern void init_meta_events__ ## NAME (); \
	extern void init_meta__ ## NAME ();

#define META_INIT(NAME) (init_meta__ ## NAME ())

#define META_BASE_DECLARE(NAME) META_DECLARE(NAME, NAME)

#define META_DEFINE(NAME) \
	meta g_meta__ ## NAME; \
	\
	void init_meta__ ## NAME () { \
		memset(&g_meta__ ## NAME, 0, sizeof(meta)); \
		g_meta__ ## NAME.name = #NAME; \
		g_meta__ ## NAME.size = sizeof(NAME); \
		g_meta__ ## NAME.super = g_super__ ## NAME; \
		init_meta_attrs__ ## NAME (); \
		init_meta_events__ ## NAME (); \
		meta_add(&g_meta__ ## NAME); \
	}

#define META_BEGIN_ATTRS(NAME) \
	void init_meta_attrs__ ## NAME () { \
		typedef NAME this_type; \
		meta* this_meta = &g_meta__ ## NAME; \
		meta_attribute attr; \
		(void*)&attr;

#define META_ATTR(NAME, TYPE) \
	attr.name = #NAME; \
	attr.offset = offsetof(this_type, NAME); \
	attr.type = (TYPE); \
	meta_add_attribute(this_meta, &attr); 

#define META_BEGIN_EVENTS(NAME) \
	void init_meta_events__ ## NAME () { \
		typedef NAME this_type; \
		meta* this_meta = &g_meta__ ## NAME; \
		meta_event event; \
		(void*)&event;

#define META_EVENT(NAME, FUNCTION) \
	event.name = #NAME; \
	event.cb = (meta_event_cb)&FUNCTION; \
	meta_add_event(this_meta, &event); 

#define META_END_ATTRS() }
#define META_END_EVENTS() }