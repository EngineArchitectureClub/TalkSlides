#include "Meta.h"

// a test class
class A
{
	int a;
	float b;

	int bar(float method) { return (int)(method * 0.5f); }
	float baz(double d, char is_const) { return d > (double)is_const ? (float)(d * 0.5) : 0.f; }

public:
	int getA() const { return a; }
	void setA(int _) { a = _; }

	float getB() const { return b; }

	void foo() { a *= 3; }

	META_DECLARE(A);
};

// another test, with derivation
class B : public A
{
	float gar(float m) { return c += m; }

public:
	float c;

	META_DECLARE(B);
};

// a test with extern Get
namespace TestC
{
	struct C
	{
		float x, y;
	};
}

// create m_Type infos
META_TYPE(A)
	.member("a", &A::a) // note that we can bind private m_Members with this style of Get
	.member("b", &A::b)
	.member("a2", &A::getA)
	.member("a3", &A::getA, nullptr)
	.member("a4", &A::getA, &A::setA)
	.method("foo", &A::foo)
	.method("bar", &A::bar)
	.method("baz", &A::baz);

META_TYPE(B)
	.base<A>()
	.member("c", &B::c)
	.method("gar", &B::gar);

META_EXTERN(TestC::C)
	.member("x", &TestC::C::x)
	.member("y", &TestC::C::y);

// test routine
int main(int argc, char* argv[])
{
	B b;
	b.setA(12);
	int i;
	assert(::Meta::Get(&b)->FindMember("a")->IsMutable());
	::Meta::Get(&b)->FindMember("a")->Get(&b, &i);
	assert(i == 12);
	i = 0;
	assert(!::Meta::Get(&b)->FindMember("a2")->IsMutable());
	::Meta::Get(&b)->FindMember("a2")->Get(&b, &i);
	assert(i == 12);
	i = 0;
	assert(!::Meta::Get(&b)->FindMember("a3")->IsMutable());
	::Meta::Get(&b)->FindMember("a3")->Get(&b, &i);
	assert(i == 12);
	i = 0;
	assert(::Meta::Get(&b)->FindMember("a4")->IsMutable());
	::Meta::Get(&b)->FindMember("a4")->Get(&b, &i);
	assert(i == 12);
	float method = 7.f;
	::Meta::Get(&b)->FindMember("b")->Set(&b, &method);
	assert(b.getB() == 7.f);
	b.setA(0);
	i = 99;
	::Meta::Get(&b)->FindMember("a4")->Set(&b, &i);
	assert(b.getA() == 99);
	::Meta::Get(&b)->FindMethod("foo")->Call(&b, 0, 0);
	assert(b.getA() == 297);
	b.c = 17.f;
	method = 7.f;
	::Meta::Any args[2] = { ::Meta::ToAny(&method), ::Meta::ToAny(nullptr) };
	::Meta::Get(&b)->FindMethod("gar")->Call(&b, &method, 1, args);
	assert(b.c == 24.f);
	method = 11.f;
	args[0] = ::Meta::ToAny(&method);
	assert(::Meta::Get(&b)->FindMethod("bar")->CanCall(&b, &i, 1, args));
	::Meta::Get(&b)->FindMethod("bar")->Call(&b, &i, 1, args);
	assert(i == 5);
	char is_const = 7;
	double d = 5.0;
	args[0] = ::Meta::ToAny(&d);
	args[1] = ::Meta::ToAny(&is_const);
	assert(::Meta::Get(&b)->FindMethod("baz")->CanCall(&b, &method, 2, args));
	::Meta::Get(&b)->FindMethod("baz")->Call(&b, &method, 2, args);
	assert(method == 0.f);
	d = 20.0;
	::Meta::Get(&b)->FindMethod("baz")->Call(&b, &method, 2, args);
	assert(method == 10.f);
	assert (!::Meta::Get(&b)->FindMethod("foo")->CanCall(&b, &method, 2, args));
}