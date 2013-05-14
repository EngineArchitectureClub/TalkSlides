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

	//! Namespace containing helper code
	//! \internal
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

			const TypeInfo* type; //!< The type being used
			ptrdiff_t offset; //<! Offset that must be applied to convert a pointer from the deriving type to this base
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

	//! \brief Represents a specific C++ type with qualifiers (const, reference, pointer, etc.)
	struct TypeRecord
	{
		enum Qualifier //!< Qualifiers (not quite correct C++ terminology) specifying how the type is used
		{
			Void, //!< This is a void value and may not be access at all
			Value, //!< The encoded value is not a by-reference type but is a by-copy type and may not be reassigned.
			Reference, //!< The encoded value is a non-const reference and may be assigned to.
			ConstReference, //!< The encoded value is a const reference and may not be assigned to.
			Pointer, //<! The encoded value is a pointer and assignments point to a new value, and dereferencing allows changing the value.
			ConstPointer, //<! The encoded value is a pointer and assignments point to a new value, but dereferencing does not allow changing the value.
		};

		const TypeInfo* type; //!< The type encoded
		Qualifier qualifier; //!< Qualifier to the type denoting access pattern

		TypeRecord(const TypeInfo* t, Qualifier q) : type(t), qualifier(q) { }
		TypeRecord() : type(nullptr), qualifier(Void) { }
	};

	namespace internal
	{
		//! \brief Construct a TypeRecord for a specific type by value
		template <typename Type> struct make_type_record { static const TypeRecord type() { return TypeRecord(Get<Type>(), TypeRecord::Value); } };

		//! \brief Construct a TypeRecord for a specific type by pointer
		template <typename Type> struct make_type_record<Type*> { static const TypeRecord type() { return TypeRecord(Get<Type>(), TypeRecord::Pointer); } };

		//! \brief Construct a TypeRecord for a specific type by const pointer
		template <typename Type> struct make_type_record<const Type*> { static const TypeRecord type() { return TypeRecord(Get<Type>(), TypeRecord::ConstPointer); } };

		//! \brief Construct a TypeRecord for a specific type by reference
		template <typename Type> struct make_type_record<Type&> { static const TypeRecord type() { return TypeRecord(Get<Type>(), TypeRecord::Reference); } };

		//! \brief Construct a TypeRecord for a specific type by const reference
		template <typename Type> struct make_type_record<const Type&> { static const TypeRecord type() { return TypeRecord(Get<Type>(), TypeRecord::ConstReference); } };

		//! \brief Construct a TypeRecord for void
		template <> struct make_type_record<void> { static const TypeRecord type() { return TypeRecord(nullptr, TypeRecord::Void); } };

		template <typename Type> struct make_any_helper;
	}

	//! \brief Holds any type of value that can be handled by the introspection system
	class Any
	{
	private:
		union
		{
			double _align_me; //!< force allignment
			char m_Data[4 * sizeof(float)]; //!< large enough for 4 floats, e.g. a vec4
			void* m_Ptr; //!< convenien
		};
		TypeRecord m_TypeRecord; //!< Type record information stored in this Any

	public:
		//! \brief Checks if the value is const when dereferenced.
		bool IsConst() const { return m_TypeRecord.qualifier == TypeRecord::ConstReference || m_TypeRecord.qualifier == TypeRecord::ConstPointer; }

		//! \brief Gets the TypeInfo for the value stored in the Any, if any
		const TypeInfo* GetType() const { return m_TypeRecord.type; }

		//! \brief Gets the TypeRecord information associated with the value stored in this Any
		const TypeRecord& GetTypeRecord() const { return m_TypeRecord; }

		//! \brief Retrieves a pointer to the type stored in this Any
		void* GetPointer() const
		{
			switch (m_TypeRecord.qualifier)
			{
			case TypeRecord::Value: return const_cast<char*>(m_Data);
			case TypeRecord::Reference:
			case TypeRecord::ConstReference:
			case TypeRecord::Pointer:
			case TypeRecord::ConstPointer:
				return m_Ptr;
			default: return nullptr;
			}
		}

		//! \brief Retrieves a pointer to the type stored in this Any adjusted by base type offset if necessary
		inline void* GetPointer(const TypeInfo* type) const;

		//! \brief Retrieves a pointer to the type stored in this Any adjusted by base type offset if necessary
		template <typename Type> Type* GetPointer() const { return static_cast<Type*>(GetPointer(Get<Type>())); }

		//! \brief Retrieves a reference to the type stored in this Any adjusted by base type offset if necessary
		template <typename Type> Type& GetReference() const { return *static_cast<Type*>(GetPointer(Get<Type>())); }

		template <typename Type> friend struct internal::make_any_helper;
	};

	namespace internal
	{
		template <typename Type> struct make_any_helper { static Any make(Type value) { Any any; static_assert(sizeof(Type) <= sizeof(any.m_Data), "Type too large for by-value Any"); any.m_TypeRecord = make_type_record<Type>::type(); new (&any.m_Data) Type(value); return any; } };
		template <typename Type> struct make_any_helper<Type&> { static Any make(Type& value) { Any any; any.m_TypeRecord = make_type_record<Type&>::type(); any.m_Ptr = const_cast<typename std::remove_const<Type>::type*>(&value); return any; } };
	}

	//! \brief Convert any value into an Any
	template <typename Type> Any make_any(Type value) { return internal::make_any_helper<typename std::remove_reference<Type>::type>::make(value); }

	//! \brief Convert any value into an Any explicitly as a reference
	template <typename Type> Any make_any_ref(Type& value) { return internal::make_any_helper<Type&>::make(value); }

	//! \brief Convert any value into an Any explicitly as a reference
	template <typename Type> Any make_any_ref(const Type& value) { return internal::make_any_helper<const Type&>::make(value); }

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
		//! \param obj Object that owns the member.
		//! \returns Value of the member.
		virtual Any DoGet(const Any& obj) const = 0;

		//! \brief Reimplement to set the value of a member variable.
		//! \param obj Object that owns the member.
		//! \param in Value to set to.
		virtual void DoSet(const Any& obj, const Any& in) const = 0;

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
		//! \returns True if Get is safe to call with these arguments, false otherwise.
		inline bool CanGet(const Any& obj) const;

		//! \brief Retrieves the value of the member variable.
		//! \param obj An instance of the owner type.
		inline Any Get(const Any& obj) const;

		//! \brief Tests if the variable can be sets from the given input value.
		//! \param obj An instance of the owner type.
		//! \param in An instance of the variable type to set.
		//! \returns True if Set is safe to call with these arguments, false otherwise.
		inline bool CanSet(const Any& obj, const Any& in) const;

		//! \brief Sets the value of the member variable.
		//! \param obj An instance of the owner type.
		//! \param in An instance of the variable type to set.
		inline void Set(const Any& obj, const Any& in) const;

		friend class TypeInfo;
	};

	namespace internal
	{
		template <typename Type> struct any_cast_helper { static const Type& cast(const Any& any) { return any.GetReference<Type>(); } };
		template <typename Type> struct any_cast_helper<Type*> { static Type* cast(const Any& any) { return any.GetPointer<Type>(); } };
		template <typename Type> struct any_cast_helper<const Type*> { static const Type *cast(const Any& any) { return any.GetPointer<Type>(); } };
		template <typename Type> struct any_cast_helper<Type&> { static Type& cast(const Any& any) { return any.GetReference<Type>(); } };
		template <typename Type> struct any_cast_helper<const Type&> { static const Type& cast(const Any& any) { return any.GetReference<Type>(); } };
	}

	//! \brief Attempts to convert an any reference into a pointer to the actual C++ type.
	//! \param any An Any reference to try to convert.
	template <typename Type> Type any_cast(const Any& any)
	{
		return internal::any_cast_helper<Type>::cast(any);
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
		//! \returns The return value as an Any
		virtual Any DoCall(const Any& obj, int argc, const Any* argv) const = 0;

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
		//! \returns The return value as an Any
		inline Any Call(const Any& obj, int argc, const Any* argv) const;

		//! \brief Invoke the method (with a return value).
		//! \param obj The instance of the object to call the method on.
		//! \param argc The number of arguments to pass in to the method.
		//! \param argv A list of any references for the arguments.
		//! \returns True if the parameters are valid for a call to succeed.
		inline bool CanCall(const Any& obj, int argc, const Any* argv) const;

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
		TypeInfo(const char* name)
		{
			int nested = 0;
			// trim off the namespaces from the name; account for possible template specializations
			for (const char* m_Name = name + std::strlen(name) - 1; m_Name != name; --m_Name)
			{
				if (*m_Name == '>')
					++nested;
				else if (*m_Name == '<')
					--nested;
				else if (nested == 0 && *m_Name == ':')
					break;
			}
		}

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

	inline void* Any::GetPointer(const TypeInfo* type) const
	{
		return m_TypeRecord.type->Adjust(type, GetPointer());
	}

	bool Member::CanGet(const Any& obj) const
	{
		if (!obj.GetType()->IsSameOrDerivedFrom(m_Owner))
			return false;
		return true;
	}

	Any Member::Get(const Any& obj) const
	{
		return DoGet(obj);
	}

	bool Member::CanSet(const Any& obj, const Any& in) const
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

	void Member::Set(const Any& obj, const Any& in) const
	{
		DoSet(obj, in);
	}

	Any Method::Call(const Any& obj, int argc, const Any* argv) const
	{
		return DoCall(obj, argc, argv);
	}

	bool Method::CanCall(const Any& obj, int argc, const Any* argv) const
	{
		if (!obj.GetType()->IsSameOrDerivedFrom(m_Owner))
			return false;

		if (argc != GetArity())
			return false;

		for (int i = 0; i < argc; ++i)
			if (argv[i].GetType() != GetParamType(i))
				return false;

		return true;
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

			virtual bool IsMutable() const override { return true; }

			virtual Any DoGet(const Any& obj) const override
			{
				return make_any<MemberType>(obj.GetPointer<Type>()->*m_Member);
			}

			virtual void DoSet(const Any& obj, const Any& in) const override
			{
				obj.GetPointer<Type>()->*m_Member = in.GetReference<MemberType>();
			}
		};

		//! \brief A member that is accessed by a getter function.
		//! \internal
		template <typename Type, typename MemberType, typename Getter> class TypeMemberGetter : public Member
		{
			Getter m_Getter;

		public:
			TypeMemberGetter(const char* name, const TypeInfo* type, Getter getter) : Member(name, type), m_Getter(getter) { }

			virtual bool IsMutable() const override { return false; }

			virtual Any DoGet(const Any& obj) const override
			{
				return make_any<decltype((static_cast<Type*>(nullptr)->*m_Getter)())>(((obj.GetPointer<Type>())->*m_Getter)());
			}

			virtual void DoSet(const Any& obj, const Any& in) const override { }
		};

		//! \brief A member that is accessed by a getter function and setter function.
		//! \internal
		template <typename Type, typename MemberType, typename Getter, typename Setter> class TypeMemberGetterSetter : public TypeMemberGetter<Type, MemberType, Getter>
		{
			Setter m_Setter;

		public:
			TypeMemberGetterSetter(const char* name, const TypeInfo* type, Getter getter, Setter setter) : TypeMemberGetter<Type, MemberType, Getter>(name, type, getter), m_Setter(setter) { }

			virtual bool IsMutable() const override { return true; }

			virtual void DoSet(const Any& obj, const Any& in) const override
			{
				(obj.GetPointer<Type>()->*m_Setter)(in.GetReference<MemberType>());
			}
		};

		// helpers to deal with the possibility of a void return m_Type
		template <typename Type, typename ReturnType> struct do_call0
		{
			static Any call(ReturnType (Type::*method)(), Type* obj, int argc, const Any* argv)
			{
				return make_any<ReturnType>((obj->*method)());
			}
		};

		template <typename Type> struct do_call0<Type, void>
		{
			static Any call(void (Type::*method)(), Type* obj, int argc, const Any* argv)
			{
				(obj->*method)();
				return Any();
			}
		};

		//! \brief A method with no parameters.
		template <typename Type, typename ReturnType>
		class Method0 : public Method
		{
			ReturnType (Type::*method)();

		public:
			virtual const TypeInfo* GetReturnType() const override { return Get<ReturnType>(); }

			virtual const TypeInfo* GetParamType(int name) const override { return nullptr; }

			virtual int GetArity() const override { return 0; }

			Method0(const char* name, ReturnType (Type::*method)()) : Method(name), method(method) { }

			virtual Any DoCall(const Any& obj, int argc, const Any* argv) const override { return do_call0<Type, ReturnType>::call(method, obj.GetPointer<Type>(), argc, argv); }
		};

		// helpers to deal with the possibility of a void return m_Type
		template <typename Type, typename ReturnType, typename ParamType0> struct do_call1
		{
			static Any call(ReturnType (Type::*method)(ParamType0), Type* obj, int argc, const Any* argv)
			{
				return make_any<ReturnType>((obj->*method)(any_cast<ParamType0>(argv[0])));
			}
		};

		template <typename Type, typename ParamType0> struct do_call1<Type, void, ParamType0>
		{
			static Any call(void (Type::*method)(ParamType0), Type* obj, int argc, const Any* argv)
			{
				(obj->*method)(any_cast<ParamType0>(argv[0]));
				return Any();
			}
		};

		//! \brief A method with one parameter.
		template <typename Type, typename ReturnType, typename ParamType0>
		class Method1 : public Method
		{
			ReturnType (Type::*method)(ParamType0);

		public:
			virtual const TypeInfo* GetReturnType() const override { return Get<ReturnType>(); }

			virtual const TypeInfo* GetParamType(int name) const override { if (name == 0) return Get<ParamType0>(); else return nullptr; }

			virtual int GetArity() const override { return 1; }

			Method1(const char* name, ReturnType (Type::*method)(ParamType0)) : Method(name), method(method) { }

			virtual Any DoCall(const Any& obj, int argc, const Any* argv) const override { return do_call1<Type, ReturnType, ParamType0>::call(method, obj.GetPointer<Type>(), argc, argv); }
		};

		// helpers to deal with the possibility of a void return m_Type
		template <typename Type, typename ReturnType, typename ParamType0, typename ParamType1> struct do_call2
		{
			static Any call(ReturnType (Type::*method)(ParamType0, ParamType1), Type* obj, int argc, const Any* argv)
			{
				return internal::make_any_helper<ReturnType>::make((obj->*method)(any_cast<ParamType0>(argv[0]), any_cast<ParamType1>(argv[1])));
			}
		};

		template <typename Type, typename ParamType0, typename ParamType1>	struct do_call2<Type, void, ParamType0, ParamType1>
		{
			static Any call(void (Type::*method)(ParamType0, ParamType1), Type* obj, int argc, const Any* argv)
			{
				(obj->*method)(any_cast<ParamType0>(argv[0]), any_cast<ParamType1>(argv[1]));
				return Any();
			}
		};

		//! \brief A method with two parameters.
		template <typename Type, typename ReturnType, typename ParamType0, typename ParamType1>
		class Method2 : public Method
		{
			ReturnType (Type::*method)(ParamType0, ParamType1);

		public:
			virtual const TypeInfo* GetReturnType() const override { return Get<ReturnType>(); }

			virtual const TypeInfo* GetParamType(int name) const override { if (name == 0) return Get<ParamType0>(); else if (name == 1) return Get<ParamType1>(); else return nullptr; }

			virtual int GetArity() const override { return 2; }

			Method2(const char* name, ReturnType (Type::*method)(ParamType0, ParamType1)) : Method(name), method(method) { }

			virtual Any DoCall(const Any& obj, int argc, const Any* argv) const override { return do_call2<Type, ReturnType, ParamType0, ParamType1>::call(method, obj.GetPointer<Type>(), argc, argv); }
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
			template <typename MemberType> typename std::enable_if<!std::is_member_function_pointer<MemberType>::value, TypedInfo&>::type member(const char* name, MemberType Type::*member) { m_Members.push_back(new TypeMember<Type, typename std::remove_reference<MemberType>::type>(name, Get<MemberType>(), member)); return *this; }

			//! \brief Add a read-only member definition to this type which will be accessed by a getter member function.
			//! \param name The name of the member.
			//! \param getter A pointer to the getter member function.
			template <typename MemberType> TypedInfo& member(const char* name, MemberType (Type::*getter)() const) { m_Members.push_back(new TypeMemberGetter<Type, typename std::remove_reference<MemberType>::type, MemberType (Type::*)() const>(name, Get<MemberType>(), getter)); return *this; }

			//! \brief Add a read-only member definition to this type which will be accessed by a getter member function.  Allows passing nullptr as a third parameter to indicate the lack of a setter.
			//! \param name The name of the member.
			//! \param getter A pointer to the getter member function.
			template <typename MemberType> TypedInfo& member(const char* name, MemberType (Type::*getter)() const, std::nullptr_t) { m_Members.push_back(new TypeMemberGetter<Type, typename std::remove_reference<MemberType>::type, MemberType (Type::*)() const>(name, Get<MemberType>(), getter)); return *this; }

			//! \brief Add a read-write member definition to this type which will be accessed by getter and setter member functions.
			//! \param name The name of the member.
			//! \param getter A pointer to the getter member function.
			//! \param setter A pointer to the setter member function.
			template <typename MemberType, typename SetterType, typename SetterReturnType> TypedInfo& member(const char* name, MemberType (Type::*getter)() const, SetterReturnType (Type::*setter)(SetterType)) { m_Members.push_back(new TypeMemberGetterSetter<Type, typename std::remove_reference<MemberType>::type, MemberType (Type::*)() const, SetterReturnType (Type::*)(SetterType)>(name, Get<MemberType>(), getter, setter)); return *this; }

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