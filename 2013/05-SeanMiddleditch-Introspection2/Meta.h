#include <type_traits>
#include <vector>
#include <cstring>
#include <cassert>
#include <cstddef>

// forward declarations to keep order of code happy
namespace Meta { class TypeInfo; }
namespace internal { template <typename T> struct MetaHolder; }

// Get-programming trick to detect if a m_Type has a GetType member
namespace internal
{
	template <typename T> struct has_get_meta
	{
		template <typename U> static std::true_type test(decltype(U::GetType())*);
		template <typename U> static std::false_type test(...);
		static const bool value = std::is_same<decltype(test<T>(nullptr)), std::true_type>::value;
	};
}

// Get-programming trick to detect if a m_Type has a TypeInfo embedded in the m_Type; useful trick for initialization of privates
namespace internal
{
	template <typename T> struct has_inline_meta
	{
		template <typename U> static std::true_type test(decltype(U::MetaStaticHolder::s_TypeInfo)*);
		template <typename U> static std::false_type test(...);
		static const bool value = std::is_same<decltype(test<T>(nullptr)), std::true_type>::value;
	};
}

// Get the TypeInfo for a spcific m_Type
namespace internal
{
	template <typename Type, bool HasInlineMeta> struct meta_lookup
	{
		static const ::Meta::TypeInfo* Get() { return &Type::MetaStaticHolder::s_TypeInfo; }
	};
	template <typename Type> struct meta_lookup<Type, false>
	{
		static const ::Meta::TypeInfo* Get() { return &::internal::MetaHolder<Type>::s_TypeInfo; }
	};
	template <> struct meta_lookup<void, false> { static const ::Meta::TypeInfo* Get() { return nullptr; } };
	template <> struct meta_lookup<std::nullptr_t, false> { static const ::Meta::TypeInfo* Get() { return nullptr; } };
}

// Get the Get for a specific instance
namespace Meta
{
	template <typename Type> const TypeInfo* Get() { return ::internal::meta_lookup<Type, ::internal::has_inline_meta<Type>::value>::Get(); }
	template <typename Type> typename std::enable_if< ::internal::has_get_meta<Type>::value, const ::Meta::TypeInfo*>::type Get(const Type* type) { return type->getMeta(); }
	template <typename Type> typename std::enable_if<!::internal::has_get_meta<Type>::value, const ::Meta::TypeInfo*>::type Get(const Type*) { return ::Meta::Get<Type>(); }
}

// wrapper that holds on to a reference of a value with a m_Type
namespace Meta
{
	class Any
	{
		const TypeInfo* m_Type;
		void* m_Ref;
		bool m_IsConst;
	public:
		Any(const TypeInfo* type, void* ref, bool is_const = false) : m_Type(type), m_Ref(ref), m_IsConst(is_const) { }
		const TypeInfo* GetType() const { return m_Type; }
		void* GetRef() const { return m_Ref; }
		bool IsConst() const { return m_IsConst; }	
	};
}

// create Any m_Ref values
namespace Meta
{
	template <typename Type> static Any ToAny(Type* v) { return Any(Get<Type>(), v); }
	template <typename Type> static Any ToAny(const Type* v) { return Any(Get<Type>(), const_cast<void*>((const void*)v), true); }
	static Any ToAny(std::nullptr_t) { return Any(nullptr, nullptr, true); }
}

// information about a member variable
namespace Meta
{
	class Member
	{
		const char* m_Name;
		const TypeInfo* m_Owner;
		const TypeInfo* m_Type;
	protected:
		Member(const char* name, const TypeInfo* type) : m_Name(name), m_Type(type) { }
		virtual ~Member() { }
		virtual void DoGet(const void* obj, void* out) const = 0;
		virtual void DoSet(void* obj, const void* in) const = 0;
	public:
		void SetOwner(const TypeInfo* owner) { m_Owner = owner; }
		virtual bool IsMutable() const = 0;
		const char* GetName() const { return m_Name; }
		inline void Get(const Any& obj, const Any& out) const;
		template <typename T, typename U> void Get(const T* obj, U* out) { Get(ToAny(obj), ToAny(out)); }
		inline void Set(const Any& obj, const Any& in) const;
		template <typename T, typename U> void Set(T* obj, const U* in) { Set(ToAny(obj), ToAny(in)); }
	};
}

// information about a member variable accessible by offset
namespace internal
{
	template <typename Type, typename MemberType> class TypeMember : public ::Meta::Member
	{
		ptrdiff_t m_Offset;
	public:
		TypeMember(const char* name, const ::Meta::TypeInfo* type, ptrdiff_t o) : Member(name, type), m_Offset(o) { }
		virtual bool IsMutable() const { return true; }
		virtual void DoGet(const void* obj, void* out) const
		{
			*static_cast<MemberType*>(out) = *reinterpret_cast<const MemberType*>(static_cast<const char*>(obj) + m_Offset);
		}
		virtual void DoSet(void* obj, const void* in) const
		{
			*reinterpret_cast<MemberType*>(static_cast<char*>(obj) + m_Offset) = *static_cast<const MemberType*>(in);
		}
	};
}

// information about a read-only member variable accessible via a getter
namespace internal
{
	template <typename Type, typename MemberType, typename Getter> class TypeMemberGetter : public ::Meta::Member
	{
		Getter m_Getter;
	public:
		TypeMemberGetter(const char* name, const ::Meta::TypeInfo* type, Getter getter) : Member(name, type), m_Getter(getter) { }
		virtual bool IsMutable() const { return false; }
		virtual void DoGet(const void* obj, void* out) const
		{
			*static_cast<MemberType*>(out) = (reinterpret_cast<const Type*>(obj)->*m_Getter)();
		}
		virtual void DoSet(void* obj, const void* in) const { }
	};
}

// information about a read-write member variable accessible via a getter and setter
namespace internal
{
	template <typename Type, typename MemberType, typename Getter, typename Setter> class TypeMemberGetterSetter : public TypeMemberGetter<Type, MemberType, Getter>
	{
		Setter m_Setter;
	public:
		TypeMemberGetterSetter(const char* name, const ::Meta::TypeInfo* type, Getter getter, Setter setter) : TypeMemberGetter<Type, MemberType, Getter>(name, type, getter), m_Setter(setter) { }
		virtual bool IsMutable() const { return true; }
		virtual void DoSet(void* obj, const void* in) const
		{
			(reinterpret_cast<Type*>(obj)->*m_Setter)(*static_cast<const MemberType*>(in));
		}
	};
}

// helper for converting any argsuments into method arguments
namespace internal
{
	template <typename Type>
	typename std::enable_if<std::is_pointer<Type>::value, Type>::type any_cast(const Meta::Any& any)
	{
		assert(std::is_const<typename std::remove_pointer<Type>::type>::value || !any.IsConst());
		return reinterpret_cast<Type>(any.GetRef());
	}
	template <typename Type>
	typename std::enable_if<std::is_reference<Type>::value, Type>::type any_cast(const Meta::Any& any)
	{
		assert(std::is_const<typename std::remove_reference<Type>::type>::value || !any.IsConst());
		return *reinterpret_cast<typename std::remove_reference<Type>::type*>(any.GetRef());
	}
	template <typename Type>
	typename std::enable_if<!std::is_pointer<Type>::value && !std::is_reference<Type>::value, Type>::type any_cast(const Meta::Any& any)	
	{
		return *reinterpret_cast<Type*>(any.GetRef());
	}
}

// information about a method
namespace Meta
{
	class Method
	{
		const char* m_Name;
		const TypeInfo* m_Owner;
	protected:
		Method(const char* name) : m_Name(name) { }
		virtual ~Method() { }
		virtual void DoCall(void* obj, void* out, int argc, const Any* argv) const = 0;
	public:
		void SetOwner(const TypeInfo* m_Type) { m_Owner = m_Type; }
		const char* GetName() const { return m_Name; }
		virtual const TypeInfo* GetReturnType() const = 0;
		virtual const TypeInfo* GetParamType(int name) const = 0;
		virtual int GetArity() const = 0;
		template <typename ObjectType, typename ReturnType> void Call(ObjectType* obj, ReturnType* out, int argc, const Any* argv) const
		{
			assert(CanCall(obj, out, argc, argv));
			DoCall(obj, out, argc, argv);
		}
		template <typename ObjectType> void Call(ObjectType* obj, int argc, const Any* argv) const
		{
			assert(CanCall(obj, argc, argv));
			DoCall(obj, nullptr, argc, argv);
		}
		template <typename ObjectType, typename ReturnType> bool CanCall(ObjectType* obj, ReturnType* out, int argc, const Any* argv) const
		{
			if (!Get<ObjectType>()->IsSameOrDerivedFrom(m_Owner))
				return false;
			if (Get<ReturnType>() != GetReturnType())
				return false;
			if (argc != GetArity())
				return false;
			for (int i = 0; i < argc; ++i)
				if (argv[i].GetType() != GetParamType(i))
					return false;
			return true;
		}
		template <typename ObjectType> bool CanCall(ObjectType* obj, int argc, const Any* argv) const
		{
			return CanCall<ObjectType, void>(obj, nullptr, argc, argv);
		}
	};
}

namespace internal
{
	// helpers to deal with the possibility of a void return m_Type
	template <typename Type, typename ReturnType> struct do_call0
	{
		static void call(ReturnType (Type::*method)(), Type* obj, ReturnType* out, int argc, const ::Meta::Any* argv)
		{
			*out = (obj->*method)();
		}
	};
	template <typename Type> struct do_call0<Type, void>
	{
		static void call(void (Type::*method)(), Type* obj, void* out, int argc, const ::Meta::Any* argv)
		{
			(obj->*method)();
		}
	};

	// method with no parameters
	template <typename Type, typename ReturnType>
	class Method0 : public ::Meta::Method
	{
		ReturnType (Type::*method)();
	public:
		virtual const ::Meta::TypeInfo* GetReturnType() const { return ::Meta::Get<ReturnType>(); }
		virtual const ::Meta::TypeInfo* GetParamType(int name) const { return nullptr; }
		virtual int GetArity() const { return 0; }
		Method0(const char* name, ReturnType (Type::*method)()) : Method(name), method(method) { }
		virtual void DoCall(void* obj, void* out, int argc, const ::Meta::Any* argv) const { do_call0<Type, ReturnType>::call(method, static_cast<Type*>(obj), static_cast<ReturnType*>(out), argc, argv); }
	};
}

namespace internal
{
	// helpers to deal with the possibility of a void return m_Type
	template <typename Type, typename ReturnType, typename ParamType0> struct do_call1
	{
		static void call(ReturnType (Type::*method)(ParamType0), Type* obj, ReturnType* out, int argc, const ::Meta::Any* argv)
		{
			*out = (obj->*method)(any_cast<ParamType0>(argv[0]));
		}
	};
	template <typename Type, typename ParamType0> struct do_call1<Type, void, ParamType0>
	{
		static void call(void (Type::*method)(ParamType0), Type* obj, void* out, int argc, const ::Meta::Any* argv)
		{
			(obj->*method)(any_cast<ParamType0>(argv[0]));
		}
	};

	// method with one parameter
	template <typename Type, typename ReturnType, typename ParamType0>
	class Method1 : public ::Meta::Method
	{
		ReturnType (Type::*method)(ParamType0);
	public:
		virtual const ::Meta::TypeInfo* GetReturnType() const { return ::Meta::Get<ReturnType>(); }
		virtual const ::Meta::TypeInfo* GetParamType(int name) const { if (name == 0) return ::Meta::Get<ParamType0>(); else return nullptr; }
		virtual int GetArity() const { return 1; }
		Method1(const char* name, ReturnType (Type::*method)(ParamType0)) : Method(name), method(method) { }
		virtual void DoCall(void* obj, void* out, int argc, const ::Meta::Any* argv) const { do_call1<Type, ReturnType, ParamType0>::call(method, static_cast<Type*>(obj), static_cast<ReturnType*>(out), argc, argv); }
	};
}

namespace internal
{
	// helpers to deal with the possibility of a void return m_Type
	template <typename Type, typename ReturnType, typename ParamType0, typename ParamType1> struct do_call2
	{
		static void call(ReturnType (Type::*method)(ParamType0, ParamType1), Type* obj, ReturnType* out, int argc, const ::Meta::Any* argv)
		{
			*out = (obj->*method)(any_cast<ParamType0>(argv[0]), any_cast<ParamType1>(argv[1]));
		}
	};
	template <typename Type, typename ParamType0, typename ParamType1>	struct do_call2<Type, void, ParamType0, ParamType1>
	{
		static void call(void (Type::*method)(ParamType0, ParamType1), Type* obj, void* out, int argc, const ::Meta::Any* argv)
		{
			(obj->*method)(any_cast<ParamType0>(argv[0]), any_cast<ParamType1>(argv[1]));
		}
	};

	// method with two parameters
	template <typename Type, typename ReturnType, typename ParamType0, typename ParamType1>
	class Method2 : public Meta::Method
	{
		ReturnType (Type::*method)(ParamType0, ParamType1);
	public:
		virtual const ::Meta::TypeInfo* GetReturnType() const { return ::Meta::Get<ReturnType>(); }
		virtual const ::Meta::TypeInfo* GetParamType(int name) const { if (name == 0) return ::Meta::Get<ParamType0>(); else if (name == 1) return ::Meta::Get<ParamType1>(); else return nullptr; }
		virtual int GetArity() const { return 2; }
		Method2(const char* name, ReturnType (Type::*method)(ParamType0, ParamType1)) : Method(name), method(method) { }
		virtual void DoCall(void* obj, void* out, int argc, const ::Meta::Any* argv) const { do_call2<Type, ReturnType, ParamType0, ParamType1>::call(method, static_cast<Type*>(obj), static_cast<ReturnType*>(out), argc, argv); }
	};
}

// encodes information about a m_Type, including all member variables and m_Methods
namespace Meta
{
	class TypeInfo
	{
		const char* m_Name;
	protected:
		std::vector<const TypeInfo*> m_Bases;
		std::vector<Member*> m_Members;
		std::vector<Method*> m_Methods;
	public:
		TypeInfo(const char* name) : m_Name(name) { }
		TypeInfo(const TypeInfo& type) : m_Name(type.m_Name), m_Bases(type.m_Bases), m_Members(type.m_Members), m_Methods(type.m_Methods)
		{
			for (auto s_TypeInfo : m_Members)
				s_TypeInfo->SetOwner(this);
			for (auto s_TypeInfo : m_Methods)
				s_TypeInfo->SetOwner(this);
		}
		bool IsDerivedFrom(const TypeInfo* base) const
		{
			for (auto b : m_Bases)
				if (b == base || b->IsDerivedFrom(base))
					return true;
			return false;
		}
		bool IsSameOrDerivedFrom(const TypeInfo* base) const
		{
			if (base == this)
				return true;
			if (IsDerivedFrom(base))
				return true;
			return false;
		}
		const char* GetName() const { return m_Name; }
		// find a member by name
		Member* FindMember(const char* name) const
		{
			for (auto s_TypeInfo : m_Members)
				if (std::strcmp(s_TypeInfo->GetName(), name) == 0)
					return s_TypeInfo;
			for (auto b : m_Bases)
			{
				auto s_TypeInfo = b->FindMember(name);
				if (s_TypeInfo != nullptr)
					return s_TypeInfo;
			}
			return nullptr;
		}
		// find a method by name
		Method* FindMethod(const char* name) const
		{
			for (auto s_TypeInfo : m_Methods)
				if (std::strcmp(s_TypeInfo->GetName(), name) == 0)
					return s_TypeInfo;
			for (auto b : m_Bases)
			{
				auto s_TypeInfo = b->FindMethod(name);
				if (s_TypeInfo != nullptr)
					return s_TypeInfo;
			}
			return nullptr;
		}
	};
}

// member definitions needing a typeinfo definition
void Meta::Member::Get(const Any& obj, const Any& out) const
{
	assert(obj.GetType()->IsSameOrDerivedFrom(m_Owner));
	assert(m_Type == out.GetType());
	assert(!out.IsConst());
	DoGet(obj.GetRef(), out.GetRef());
}
void Meta::Member::Set(const Any& obj, const Any& in) const
{
	assert(IsMutable());
	assert(obj.GetType()->IsSameOrDerivedFrom(m_Owner));
	assert(m_Type == in.GetType());
	assert(!obj.IsConst());
	DoSet(obj.GetRef(), in.GetRef());
}

// holds onto Get information for types that don'type have an inline Get m_Type
namespace internal
{
	template <typename Type> struct MetaHolder
	{
		static const ::Meta::TypeInfo s_TypeInfo;
	};
}

// creates m_Type info for a very specific m_Type
namespace internal
{
	template <typename Type, bool IsClass> struct TypedInfo : public ::Meta::TypeInfo
	{
		TypedInfo(const char* name) : TypeInfo(name) { }

		// add a base class
		template <typename MemberType> TypedInfo& base() { static_assert(std::is_base_of<MemberType, Type>::value && !std::is_same<MemberType, Type>::value, "incorrect base"); m_Bases.push_back(::Meta::Get<MemberType>()); return *this; }

		// add member definitions to the m_Type
		template <typename MemberType> TypedInfo& member(const char* name, MemberType Type::*member) { m_Members.push_back(new TypeMember<Type, MemberType>(name, ::Meta::Get<MemberType>(), reinterpret_cast<ptrdiff_t>(&(static_cast<Type*>(0)->*member)))); return *this; }
		template <typename MemberType> TypedInfo& member(const char* name, MemberType (Type::*getter)() const) { m_Members.push_back(new TypeMemberGetter<Type, MemberType, MemberType (Type::*)() const>(name, ::Meta::Get<MemberType>(), getter)); return *this; }
		template <typename MemberType> TypedInfo& member(const char* name, MemberType (Type::*getter)() const, std::nullptr_t) { m_Members.push_back(new TypeMemberGetter<Type, MemberType, MemberType (Type::*)() const>(name, ::Meta::Get<MemberType>(), getter)); return *this; }
		template <typename MemberType, typename SetterType, typename SetterReturnType> TypedInfo& member(const char* name, MemberType (Type::*getter)() const, SetterReturnType (Type::*setter)(SetterType)) { m_Members.push_back(new TypeMemberGetterSetter<Type, MemberType, MemberType (Type::*)() const, SetterReturnType (Type::*)(SetterType)>(name, ::Meta::Get<MemberType>(), getter, setter)); return *this; }

		// add method definitions for a m_Type
		template <typename ReturnType> TypedInfo& method(const char* name, ReturnType (Type::*method)()) { m_Methods.push_back(new Method0<Type, ReturnType>(name, method)); return *this; }
		template <typename ReturnType, typename ParamType0> TypedInfo& method(const char* name, ReturnType (Type::*method)(ParamType0)) { m_Methods.push_back(new Method1<Type, ReturnType, ParamType0>(name, method)); return *this; }
		template <typename ReturnType, typename ParamType0, typename ParamType1> TypedInfo& method(const char* name, ReturnType (Type::*method)(ParamType0, ParamType1)) { m_Methods.push_back(new Method2<Type, ReturnType, ParamType0, ParamType1>(name, method)); return *this; }
	};
	template <typename Type> struct TypedInfo<Type, false> : public ::Meta::TypeInfo
	{
		TypedInfo(const char* name) : TypeInfo(name) { }
	};
}

// macros to make this all easier
#define META_DECLARE(m_Type) \
	public: \
	virtual const ::Meta::TypeInfo*  GetType() const { return &m_Type::MetaStaticHolder::s_TypeInfo; } \
	struct MetaStaticHolder { static const ::Meta::TypeInfo s_TypeInfo; };
#define META_EXTERN(T) template<> const ::Meta::TypeInfo internal::MetaHolder<T>::s_TypeInfo = ::internal::TypedInfo<T, !std::is_fundamental<T>::value>(#T)
#define META_TYPE(T) const ::Meta::TypeInfo T::MetaStaticHolder::s_TypeInfo = ::internal::TypedInfo<T, true>(#T)
