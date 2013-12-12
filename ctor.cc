/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

#include <cstdio>
#include <iostream>

using namespace std;

class Obj {
public:
	Obj() {
		cout << "Hello kitty Obj ctor" << endl;
	}
};

static Obj o;

__attribute__((constructor))
static void init()
{
	printf("Hello kitty static ctor fn\n");
}
