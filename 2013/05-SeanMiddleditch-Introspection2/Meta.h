// Copyright (C) 2013 Sean Middleditch
// All rights reserverd.  This code is intended for instructional use only and may not be used
// in any commercial works nor in any student projects.

#include <type_traits>
#include <vector>
#include <cstring>
#include <cstddef>

//! Namespace that contains all introspection types and utilities.
namespace Meta
{
	class TypeInfo;

	namespace internal
	{
		template <typename T> struct MetaHolder;

		//! \brief Type trait to convert a possibly const reference or pointer to a non-const pointer
		template <typename T> struct to_pointer
		{
			typedef typename std::remove_const<typename std::remove_reference<typename std::remove_pointer<T>::type>::type>::type* type;
		};

		/*!\ brief Type trait to check if a given type has a GetMeta method. */
		template <typename T> class has_get_meta
		{
			template <typename U> static std::true_type test(decltype(U::GetType())*);
			template <typename U> static std::false_type test(...);

		public:
			/*! \brief True if the type has a GetMeta() method, false otherwise. */
			static const bool value = std::is_same<decltype(test<T>(nullptr)), std::true_type>::value;
		};

		/*! \brief Type trait to check if a class has a MetaStaticHolder::s_TypeInfo field. */
		template <typename T> class has_inline_meta
		{
			template <typename U> static std::true_type test(decltype(U::MetaStaticHolder::s_TypeInfo)*);
			template <typename U> static std::false_type test(...);

		public:
			/*! \brief True if the type has a GetMeta() method, false otherwise. */
			static const bool value = std::is_same<decltype(test<T>(nullptr)), std::true_type>::value;
		};

		/*! \brief Helper to find the TypeInfo for a type which has a MetaStaticHolder::s_TypeInfo. */
		template <typename Type, bool HasInlineMeta> struct meta_lookup
		{
			static const TypeInfo* Get() { return &Type::MetaStaticHolder::s_TypeInfo; }
		};

		/*! \brief Helper to find the TypeInfo for a type which has no MetaStaticHolder::s_TypeInfo. */
		template <typename Type> struct meta_lookup<Type, false>
		{
			static const TypeInfo* Get() { return &internal::MetaHolder<typename std::remove_const<typename std::remove_reference<Type>::type>::type>::s_TypeInfo; }
		};

		/*! \brief Helper to retrieve the null TypeInfo for a void type. */
		template <> struct meta_lookup<void, false> { static const TypeInfo* Get() { return nullptr; } };

		/*! \brief Helper to retrieve the null TypeInfo for a nullptr_t type. */
		template <> struct meta_lookup<std::nullptr_t, false> { static const TypeInfo* Get() { return nullptr; } };

		//! \brief Records information about a base type.
		struct BaseRecord
		{
			BaseRecord(const TypeInfo* t, ptrdiff_t o) : type(t), offset(o) { }

			const TypeInfo* type;
			ptrdiff_t offset;
		};
	}

	/*! \brief Get the TypeInfo for a specific type. */
	template <typename Type> const TypeInfo* Get() { return internal::meta_lookup<Type, internal::has_inline_meta<Type>::value>::Get(); }

	/*! \brief Get the TypeInfo for a specific instance supporting a GetMeta() method, usually used/needed in polymorphic interfaces. */
	template <typename Type> typename std::enable_if< internal::has_get_meta<Type>::value, const TypeInfo*>::type Get(const Type* type) { return type->getMeta(); }

	/*! \brief Get the TypeInfo for a specific instance which has no GetMeta() method and hence no support for polymorphism of introspection. */
	template <typename Type> typename std::enable_if<!internal::has_get_meta<Type>::value, const TypeInfo*>::type Get(const Type*) { return Get<Type>(); }

	/*! \brief Get the TypeInfo for a specific instance supporting a GetMeta() method, usually used/needed in polymorphic interfaces. */
	template <typename Type> typename std::enable_if<!std::is_pointer<Type>::value && internal::has_get_meta<Type>::value, const TypeInfo*>::type Get(const Type& type) { return type.getMeta(); }

	/*! \brief Get the TypeInfo for a specific instance which has no GetMeta() method and hence no support for polymorphism of introspection. */
	template <typename Type> typename std::enable_if<!std::is_pointer<Type>::value && !internal::has_get_meta<Type>::value, const TypeInfo*>::type Get(const Type&) { return Get<Type>(); }

	//! \brief Simple wrapper for references (pointers) to allow passing values to and from introspection routines.
	class AnyRef
	{
		const TypeInfo* m_Type; //!< The type of the value being referenced.
		void* m_Ref; //!< The reference pointer itself.
		bool m_IsConst; //!< A flag to indicate if this reference should be treated as a constant and non-mutable.

	public:
		//! \brief Default constructor
		AnyRef() : m_Type(nullptr), m_Ref(nullptr), m_IsConst(false) { }

		//! \brief Create an any reference from a given pointer.
		template <typename Type> inline AnyRef(Type* v) : m_Type(Get<Type>()), m_Ref(static_cast<void*>(v)), m_IsConst(false) { }

		//! \brief Create an any reference from a given pointer.
		template <typename Type> inline AnyRef(const Type* v) : m_Type(Get<Type>()), m_Ref(const_cast<void*>(static_cast<const void*>(v))), m_IsConst(true) { }

		//! \brief Create a null reference from a nullptr.
		inline AnyRef(std::nullptr_t) : m_Type(nullptr), m_Ref(nullptr), m_IsConst(true) { }

		//! \brief Get the TypeInfo associated with this reference.
		const TypeInfo* GetType() const { return m_Type; }

		//! \brief Get the reference pointer associated with this reference.
		void* GetRef() const { return m_Ref; }

		//! \brief Test if this reference should be treated as constant
		bool IsConst() const { return m_IsConst; }	
	};

	//! \brief Represents a member variable of a type. */
	class Member
	{
		const char* m_Name; //!< The name of this variable.
		const TypeInfo* m_Owner; //!< The type this member variable belongs to.
		const TypeInfo* m_Type; //!< The type of this member variable.

	protected:
		//! \brief Constructor for a member variable.
		//! \param name The name of the member.
		//! \parm type The type of the member.
		Member(const char* name, const TypeInfo* type) : m_Name(name), m_Type(type) { }
		virtual ~Member() { }

		//! \brief Reimplement to get the value of a member variable.
		//! \param obj Memory addres of the instance of the object the variable will be retrieved from.  It must be a valid instance of the owner type.
		//! \param out Memory address into which the value is stored.  It must be of the correct size and alignment for the variable type.
		virtual void DoGet(const void* obj, void* out) const = 0;

		//! \brief Reimplement to set the value of a member variable.
		//! \param obj Memory addres of the instance of the object the variable will be retrieved from.  It must be a valid instance of the owner type.
		//! \param in Memory address from which the value is retrieved.  It must be a valid instance of the variable type.
		virtual void DoSet(void* obj, const void* in) const = 0;

		//! \brief Sets the owner of the member variable.  Only meant to be called by TypeInfo.
		void SetOwner(const TypeInfo* owner) { m_Owner = owner; }

	public:
		//! \brief Checks if the variable is mutable (the value can be set).
		virtual bool IsMutable() const = 0;

		//! \brief Retrieves the name of the member variable.
		const char* GetName() const { return m_Name; }

		//! \brief Retrieves the type of the member variable.
		const TypeInfo* GetType() const { return m_Type; }

		//! \brief Retrieves the owner of the member variable.
		const TypeInfo* GetOwner() const { return m_Owner; }

		//! \brief Tests if the variable value can be retrieved into the given output any ref.
		//! \param obj An instance of the owner type.
		//! \param out An instance of variable type.
		//! \returns True if Get is safe to call with these arguments, false otherwise.
		inline bool CanGet(const AnyRef& obj, const AnyRef& out) const;

		//! \brief Retrieves the value of the member variable.
		//! \param obj An instance of the owner type.
		//! \param out An instance of the variable type.  It must already be instantiated, as this calls operator=, not a constructor.
		inline void Get(const AnyRef& obj, const AnyRef& out) const;

		//! \brief Tests if the variable can be sets from the given input value.
		//! \param obj An instance of the owner type.
		//! \param in An instance of the variable type to set.
		//! \returns True if Set is safe to call with these arguments, false otherwise.
		inline bool CanSet(const AnyRef& obj, const AnyRef& in) const;

		//! \brief Sets the value of the member variable.
		//! \param obj An instance of the owner type.
		//! \param in An instance of the variable type to set.
		inline void Set(const AnyRef& obj, const AnyRef& in) const;

		friend class TypeInfo;
	};

	//! \brief Attempts to convert an any reference into a pointer to the actual C++ type.
	//! \param any An any reference to try to convert.
	//! \returns A pointer to the value if the conversion is correct, nullptr otherwise.
	template <typename Type>
	typename std::enable_if<std::is_pointer<Type>::value, Type>::type any_cast(const AnyRef& any)
	{
		assert(std::is_const<typename std::remove_pointer<Type>::type>::value || !any.IsConst());
		return reinterpret_cast<Type>(any.GetRef());
	}

	//! \brief Attempts to convert an any reference into a pointer to the actual C++ type.
	//! \param any An any reference to try to convert.
	//! \returns A reference to the value if the conversion is correct, nullptr otherwise.
	template <typename Type>
	typename std::enable_if<std::is_reference<Type>::value, Type>::type any_cast(const AnyRef& any)
	{
		assert(std::is_const<typename std::remove_reference<Type>::type>::value || !any.IsConst());
		return *reinterpret_cast<typename std::remove_reference<Type>::type*>(any.GetRef());
	}

	//! \brief Attempts to convert an any reference into a pointer to the actual C++ type.
	//! \param any An any reference to try to convert.
	//! \returns A value if the conversion is correct, nullptr otherwise.
	template <typename Type>
	typename std::enable_if<!std::is_pointer<Type>::value && !std::is_reference<Type>::value, Type>::type any_cast(const AnyRef& any)	
	{
		return *reinterpret_cast<Type*>(any.GetRef());
	}

	//! \brief A method attach to a type.
	class Method
	{
		const char* m_Name; //!< The name of the method.
		const TypeInfo* m_Owner; //!< The type that owns the method.

		//! \brief Sets the owner type.  Only to be called by TypeInfo.
		void SetOwner(const TypeInfo* m_Type) { m_Owner = m_Type; }

	protected:
		//! \brief Constuctor for the method.
		//! \param The name of the method.
		Method(const char* name) : m_Name(name) { }
		virtual ~Method() { }

		//! \brief Override to implement calling the method.
		//! \parma obj The memory location of an instance of the owner object.
		//! \param out The memory location that the return value, if any, is stored.
		//! \param argc The number of arguments to pass in to the method.
		//! \param argv A list of any references for the arguments.
		virtual void DoCall(const AnyRef& obj, const AnyRef& out, int argc, const AnyRef* argv) const = 0;

	public:
		//! \brief Get the name of the method.
		const char* GetName() const { return m_Name; }

		//! \brief Get the owner type of the method.
		const TypeInfo* GetOwner() const { return m_Owner; }

		//! \brief Get the TypeInfo of the return value.
		virtual const TypeInfo* GetReturnType() const = 0;

		//! \brief Get the TypeInfo of the paramater at the ith index.
		//! \brief i The index of the parameter, starting from 0.
		virtual const TypeInfo* GetParamType(int i) const = 0;

		//! \brief Get the number of parameters of the method.
		virtual int GetArity() const = 0;

		//! \brief Invoke the method (with a return value).
		//! \param obj The instance of the object to call the method on.
		//! \param out The location the return value is assigned to.  Must be a validate instance of the type.
		//! \param argc The number of arguments to pass in to the method.
		//! \param argv A list of any references for the arguments.
		inline void Call(const AnyRef& obj, const AnyRef& out, int argc, const AnyRef* argv) const;

		//! \brief Invoke the method (with a return value).
		//! \param obj The instance of the object to call the method on.
		//! \param argc The number of arguments to pass in to the method.
		//! \param argv A list of any references for the arguments.
		inline void Call(const AnyRef& obj, int argc, const AnyRef* argv) const;

		//! \brief Invoke the method (with a return value).
		//! \param obj The instance of the object to call the method on.
		//! \param out The location the return value is assigned to.  Must be a validate instance of the type.
		//! \param argc The number of arguments to pass in to the method.
		//! \param argv A list of any references for the arguments.
		//! \returns True if the parameters are valid for a call to succeed.
		inline bool CanCall(const AnyRef& obj, const AnyRef& out, int argc, const AnyRef* argv) const;

		//! \brief Invoke the method (with a return value).
		//! \param obj The instance of the object to call the method on.
		//! \param argc The number of arguments to pass in to the method.
		//! \param argv A list of any references for the arguments.
		//! \returns True if the parameters are valid for a call to succeed.
		inline bool CanCall(const AnyRef& obj, int argc, const AnyRef* argv) const;

		friend class TypeInfo;
	};

	//! \brief Encodes information about a type.
	class TypeInfo
	{
		const char* m_Name; //!< The name of the type.

	protected:
		std::vector<internal::BaseRecord> m_Bases; //!< The list of base types.
		std::vector<Member*> m_Members; //!< The list of all members of this type.
		std::vector<Method*> m_Methods; //!< The list of all methods of this type.

	public:
		//! \brief Constructor for type info.
		//! \param name The name of the type.
		TypeInfo(const char* name) : m_Name(name) { }
		//! \brief Copy constructor for type info.
		TypeInfo(const TypeInfo& type) : m_Name(type.m_Name), m_Bases(type.m_Bases), m_Members(type.m_Members), m_Methods(type.m_Methods)
		{
			for (auto s_TypeInfo : m_Members)
				s_TypeInfo->SetOwner(this);

			for (auto s_TypeInfo : m_Methods)
				s_TypeInfo->SetOwner(this);
		}

		//! \brief Tests if this type is derived from another type.
		//! \parm base The type to test if this is derived from.
		//! \returns True if this type derives from base either directly or indirectly.
		bool IsDerivedFrom(const TypeInfo* base) const
		{
			for (auto& b : m_Bases)
				if (b.type == base || b.type->IsDerivedFrom(base))
					return true;

			return false;
		}

		//! \brief Tests is this type is derived from anothe type or is the same type.
		//! \parm base The type to test if this is derived from or the same as.
		//! \returns True if this type is equal to or derives from base either directly or indirectly.
		bool IsSameOrDerivedFrom(const TypeInfo* base) const
		{
			if (base == this)
				return true;
			if (IsDerivedFrom(base))
				return true;
			return false;
		}

		//! \brief Adjust a pointer of this type to a derived type.
		//! \param base The base type to convert to.
		//! \param ptr The pointer to convert.
		//! \returns The adjusted pointer, or nullptr if the adjustment is illegal (the given base type is not a base of the type).
		void* Adjust(const TypeInfo* base, void* ptr) const
		{
			if (base == this)
				return ptr;

			for (auto& b : m_Bases)
			{
				void* rs = b.type->Adjust(base, static_cast<char*>(ptr) + b.offset);
				if (rs != nullptr)
					return rs;
			}

			return nullptr;
		}

		
		//! \brief Adjust a pointer of this type to a derived type.
		//! \param base The base type to convert to.
		//! \param ptr The pointer to convert.
		//! \returns The adjusted pointer, or nullptr if the adjustment is illegal (the given base type is not a base of the type).
		const void* Adjust(const TypeInfo* base, const void* ptr) const
		{
			return Adjust(base, const_cast<void*>(ptr));
		}

		//! \brief Get the name of the type.
		const char* GetName() const { return m_Name; }

		//! \brief Find a member of this type or any base type.
		//! \param name The name of the member to look up.
		//! \returns The Member named by name or nullptr if no such member exists.
		Member* FindMember(const char* name) const
		{
			for (auto s_TypeInfo : m_Members)
				if (std::strcmp(s_TypeInfo->GetName(), name) == 0)
					return s_TypeInfo;

			for (auto& b : m_Bases)
			{
				auto s_TypeInfo = b.type->FindMember(name);
				if (s_TypeInfo != nullptr)
					return s_TypeInfo;
			}

			return nullptr;
		}

		//! \brief Find a method of this type or any base type.
		//! \param name The name of the method to look up.
		//! \returns The Method named by name or nullptr if no such method exists.
		Method* FindMethod(const char* name) const
		{
			for (auto s_TypeInfo : m_Methods)
				if (std::strcmp(s_TypeInfo->GetName(), name) == 0)
					return s_TypeInfo;

			for (auto& b : m_Bases)
			{
				auto s_TypeInfo = b.type->FindMethod(name);
				if (s_TypeInfo != nullptr)
					return s_TypeInfo;
			}

			return nullptr;
		}
	};

	bool Member::CanGet(const AnyRef& obj, const AnyRef& out) const
	{
		if (!obj.GetType()->IsSameOrDerivedFrom(m_Owner))
			return false;
		if (m_Type != out.GetType())
			return false;
		if (out.IsConst())
			return false;
		return true;
	}

	void Member::Get(const AnyRef& obj, const AnyRef& out) const
	{
		DoGet(obj.GetType()->Adjust(GetOwner(), obj.GetRef()), out.GetRef());
	}

	bool Member::CanSet(const AnyRef& obj, const AnyRef& in) const
	{
		if (!IsMutable())
			return false;
		if (!obj.GetType()->IsSameOrDerivedFrom(m_Owner))
			return false;
		if (m_Type != in.GetType())
			return false;
		if (obj.IsConst())
			return false;
		return true;
	}

	void Member::Set(const AnyRef& obj, const AnyRef& in) const
	{
		DoSet(obj.GetType()->Adjust(GetOwner(), obj.GetRef()), in.GetRef());
	}

	void Method::Call(const AnyRef& obj, const AnyRef& out, int argc, const AnyRef* argv) const
	{
		return DoCall(obj.GetType()->Adjust(GetOwner(), obj.GetRef()), out, argc, argv);
	}

	void Method::Call(const AnyRef& obj, int argc, const AnyRef* argv) const
	{
		return DoCall(obj.GetType()->Adjust(GetOwner(), obj.GetRef()), nullptr, argc, argv);
	}

	bool Method::CanCall(const AnyRef& obj, const AnyRef& out, int argc, const AnyRef* argv) const
	{
		if (!obj.GetType()->IsSameOrDerivedFrom(m_Owner))
			return false;

		if (out.GetType() != GetReturnType())
			return false;

		if (argc != GetArity())
			return false;

		for (int i = 0; i < argc; ++i)
			if (argv[i].GetType() != GetParamType(i))
				return false;

		return true;
	}

	bool Method::CanCall(const AnyRef& obj, int argc, const AnyRef* argv) const
	{
		return CanCall(obj, nullptr, argc, argv);
	}

	namespace internal
	{
		//! \brief A member that is accessed directly in memory.
		//! \internal
		template <typename Type, typename MemberType> class TypeMember : public Member
		{
			MemberType Type::*m_Member;

		public:
			TypeMember(const char* name, const TypeInfo* type, MemberType Type::*member) : Member(name, type), m_Member(member) { }

			virtual bool IsMutable() const { return true; }

			virtual void DoGet(const void* obj, void* out) const
			{
				*static_cast<typename internal::to_pointer<MemberType>::type>(out) = static_cast<const Type*>(obj)->*m_Member;
			}

			virtual void DoSet(void* obj, const void* in) const
			{
				static_cast<Type*>(obj)->*m_Member = *static_cast<const MemberType*>(in);
			}
		};

		//! \brief A member that is accessed by a getter function.
		//! \internal
		template <typename Type, typename MemberType, typename Getter> class TypeMemberGetter : public Member
		{
			Getter m_Getter;

		public:
			TypeMemberGetter(const char* name, const TypeInfo* type, Getter getter) : Member(name, type), m_Getter(getter) { }

			virtual bool IsMutable() const { return false; }

			virtual void DoGet(const void* obj, void* out) const
			{
				*static_cast<typename internal::to_pointer<MemberType>::type>(out) = (reinterpret_cast<const Type*>(obj)->*m_Getter)();
			}

			virtual void DoSet(void* obj, const void* in) const { }
		};

		//! \brief A member that is accessed by a getter function and setter function.
		//! \internal
		template <typename Type, typename MemberType, typename Getter, typename Setter> class TypeMemberGetterSetter : public TypeMemberGetter<Type, MemberType, Getter>
		{
			Setter m_Setter;

		public:
			TypeMemberGetterSetter(const char* name, const TypeInfo* type, Getter getter, Setter setter) : TypeMemberGetter<Type, MemberType, Getter>(name, type, getter), m_Setter(setter) { }

			virtual bool IsMutable() const { return true; }

			virtual void DoSet(void* obj, const void* in) const
			{
				(reinterpret_cast<Type*>(obj)->*m_Setter)(*static_cast<const MemberType*>(in));
			}
		};

		// helpers to deal with the possibility of a void return m_Type
		template <typename Type, typename ReturnType> struct do_call0
		{
			static void call(ReturnType (Type::*method)(), Type* obj, ReturnType* out, int argc, const AnyRef* argv)
			{
				*out = (obj->*method)();
			}
		};

		template <typename Type> struct do_call0<Type, void>
		{
			static void call(void (Type::*method)(), Type* obj, void* out, int argc, const AnyRef* argv)
			{
				(obj->*method)();
			}
		};

		//! \brief A method with no parameters.
		template <typename Type, typename ReturnType>
		class Method0 : public Method
		{
			ReturnType (Type::*method)();

		public:
			virtual const TypeInfo* GetReturnType() const { return Get<ReturnType>(); }

			virtual const TypeInfo* GetParamType(int name) const { return nullptr; }

			virtual int GetArity() const { return 0; }

			Method0(const char* name, ReturnType (Type::*method)()) : Method(name), method(method) { }

			virtual void DoCall(const AnyRef& obj, const AnyRef& out, int argc, const AnyRef* argv) const { do_call0<Type, ReturnType>::call(method, static_cast<Type*>(obj.GetRef()), static_cast<ReturnType*>(out.GetRef()), argc, argv); }
		};

		// helpers to deal with the possibility of a void return m_Type
		template <typename Type, typename ReturnType, typename ParamType0> struct do_call1
		{
			static void call(ReturnType (Type::*method)(ParamType0), Type* obj, ReturnType* out, int argc, const AnyRef* argv)
			{
				*out = (obj->*method)(any_cast<ParamType0>(argv[0]));
			}
		};

		template <typename Type, typename ParamType0> struct do_call1<Type, void, ParamType0>
		{
			static void call(void (Type::*method)(ParamType0), Type* obj, void* out, int argc, const AnyRef* argv)
			{
				(obj->*method)(any_cast<ParamType0>(argv[0]));
			}
		};

		//! \brief A method with one parameter.
		template <typename Type, typename ReturnType, typename ParamType0>
		class Method1 : public Method
		{
			ReturnType (Type::*method)(ParamType0);

		public:
			virtual const TypeInfo* GetReturnType() const { return Get<ReturnType>(); }

			virtual const TypeInfo* GetParamType(int name) const { if (name == 0) return Get<ParamType0>(); else return nullptr; }

			virtual int GetArity() const { return 1; }

			Method1(const char* name, ReturnType (Type::*method)(ParamType0)) : Method(name), method(method) { }

			virtual void DoCall(const AnyRef& obj, const AnyRef& out, int argc, const AnyRef* argv) const { do_call1<Type, ReturnType, ParamType0>::call(method, static_cast<Type*>(obj.GetRef()), static_cast<ReturnType*>(out.GetRef()), argc, argv); }
		};

		// helpers to deal with the possibility of a void return m_Type
		template <typename Type, typename ReturnType, typename ParamType0, typename ParamType1> struct do_call2
		{
			static void call(ReturnType (Type::*method)(ParamType0, ParamType1), Type* obj, ReturnType* out, int argc, const AnyRef* argv)
			{
				*out = (obj->*method)(any_cast<ParamType0>(argv[0]), any_cast<ParamType1>(argv[1]));
			}
		};

		template <typename Type, typename ParamType0, typename ParamType1>	struct do_call2<Type, void, ParamType0, ParamType1>
		{
			static void call(void (Type::*method)(ParamType0, ParamType1), Type* obj, void* out, int argc, const AnyRef* argv)
			{
				(obj->*method)(any_cast<ParamType0>(argv[0]), any_cast<ParamType1>(argv[1]));
			}
		};

		//! \brief A method with two parameters.
		template <typename Type, typename ReturnType, typename ParamType0, typename ParamType1>
		class Method2 : public Method
		{
			ReturnType (Type::*method)(ParamType0, ParamType1);

		public:
			virtual const TypeInfo* GetReturnType() const { return Get<ReturnType>(); }

			virtual const TypeInfo* GetParamType(int name) const { if (name == 0) return Get<ParamType0>(); else if (name == 1) return Get<ParamType1>(); else return nullptr; }

			virtual int GetArity() const { return 2; }

			Method2(const char* name, ReturnType (Type::*method)(ParamType0, ParamType1)) : Method(name), method(method) { }

			virtual void DoCall(const AnyRef& obj, const AnyRef& out, int argc, const AnyRef* argv) const { do_call2<Type, ReturnType, ParamType0, ParamType1>::call(method, static_cast<Type*>(obj.GetRef()), static_cast<ReturnType*>(out.GetRef()), argc, argv); }
		};

		//! \brief Holder for TypeInfo external to a type, used when adding introspection to types that cannot be modified.
		template <typename Type> struct MetaHolder
		{
			static const TypeInfo s_TypeInfo;
		};

		//! \brief Utility class to initialize a TypeInfo, invoked by META_DEFINE_EXTERN or META_DEFINE.
		template <typename Type, bool IsClass> struct TypedInfo : public TypeInfo
		{
			//! \brief Construct the type.
			TypedInfo(const char* name) : TypeInfo(name) { }

			//! \brief Add a base to this type.
			template <typename BaseType> TypedInfo& base() { static_assert(std::is_base_of<BaseType, Type>::value && !std::is_same<BaseType, Type>::value, "incorrect base"); m_Bases.push_back(BaseRecord(Get<BaseType>(), reinterpret_cast<ptrdiff_t>(static_cast<const BaseType*>(reinterpret_cast<const Type*>(0x1000))) - 0x1000)); return *this; }

			//! \brief Add a read-write member definition to this type which will be accessed directly.
			//! \param name The name of the member.
			//! \param member A pointer to the member.
			template <typename MemberType> typename std::enable_if<!std::is_member_function_pointer<MemberType>::value, TypedInfo&>::type member(const char* name, MemberType Type::*member) { m_Members.push_back(new TypeMember<Type, MemberType>(name, Get<MemberType>(), member)); return *this; }

			//! \brief Add a read-only member definition to this type which will be accessed by a getter member function.
			//! \param name The name of the member.
			//! \param getter A pointer to the getter member function.
			template <typename MemberType> TypedInfo& member(const char* name, MemberType (Type::*getter)() const) { m_Members.push_back(new TypeMemberGetter<Type, MemberType, MemberType (Type::*)() const>(name, Get<MemberType>(), getter)); return *this; }

			//! \brief Add a read-only member definition to this type which will be accessed by a getter member function.  Allows passing nullptr as a third parameter to indicate the lack of a setter.
			//! \param name The name of the member.
			//! \param getter A pointer to the getter member function.
			template <typename MemberType> TypedInfo& member(const char* name, MemberType (Type::*getter)() const, std::nullptr_t) { m_Members.push_back(new TypeMemberGetter<Type, MemberType, MemberType (Type::*)() const>(name, Get<MemberType>(), getter)); return *this; }

			//! \brief Add a read-write member definition to this type which will be accessed by getter and setter member functions.
			//! \param name The name of the member.
			//! \param getter A pointer to the getter member function.
			//! \param setter A pointer to the setter member function.
			template <typename MemberType, typename SetterType, typename SetterReturnType> TypedInfo& member(const char* name, MemberType (Type::*getter)() const, SetterReturnType (Type::*setter)(SetterType)) { m_Members.push_back(new TypeMemberGetterSetter<Type, MemberType, MemberType (Type::*)() const, SetterReturnType (Type::*)(SetterType)>(name, Get<MemberType>(), getter, setter)); return *this; }

			// add method definitions for a m_Type
			template <typename ReturnType> TypedInfo& method(const char* name, ReturnType (Type::*method)()) { m_Methods.push_back(new Method0<Type, ReturnType>(name, method)); return *this; }
			template <typename ReturnType, typename ParamType0> TypedInfo& method(const char* name, ReturnType (Type::*method)(ParamType0)) { m_Methods.push_back(new Method1<Type, ReturnType, ParamType0>(name, method)); return *this; }
			template <typename ReturnType, typename ParamType0, typename ParamType1> TypedInfo& method(const char* name, ReturnType (Type::*method)(ParamType0, ParamType1)) { m_Methods.push_back(new Method2<Type, ReturnType, ParamType0, ParamType1>(name, method)); return *this; }
		};

		//! \brief Utility class to initialize a TypeInfo, invoked by META_DEFINE_EXTERN, but which cannot have base types, member variables, or member functions (e.g. primitive types).
		template <typename Type> struct TypedInfo<Type, false> : public TypeInfo
		{
			TypedInfo(const char* name) : TypeInfo(name) { }
		};
	}
}

//! \brief Add to a type declaration to make it participate in polymorphic type lookups and to allow binding private members and methods.
//! \param T The type to annotate.
#define META_DECLARE(T) \
	public: \
	struct MetaStaticHolder { static const ::Meta::TypeInfo s_TypeInfo; }; \
	virtual const ::Meta::TypeInfo*  GetType() const { return &MetaStaticHolder::s_TypeInfo; }

//! \brief Put outside of any declaration to begin annotating a type.
//! \param T The type to annotate.
#define META_DEFINE_EXTERN(T) template<> const ::Meta::TypeInfo Meta::internal::MetaHolder<T>::s_TypeInfo = ::Meta::internal::TypedInfo<T, !std::is_fundamental<T>::value>(#T)

//! \brief Put outside of any declaration to begin annotating a type marked up with META_DECLARE(T)
//! \param T The type to annotate.
#define META_DEFINE(T) const ::Meta::TypeInfo T::MetaStaticHolder::s_TypeInfo = ::Meta::internal::TypedInfo<T, true>(#T)