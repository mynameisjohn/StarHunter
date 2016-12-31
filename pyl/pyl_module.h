/*      This program is free software; you can redistribute it and/or modify
*      it under the terms of the GNU General Public License as published by
*      the Free Software Foundation; either version 3 of the License, or
*      (at your option) any later version.
*
*      This program is distributed in the hope that it will be useful,
*      but WITHOUT ANY WARRANTY; without even the implied warranty of
*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*      GNU General Public License for more details.
*
*      You should have received a copy of the GNU General Public License
*      along with this program; if not, write to the Free Software
*      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*      MA 02110-1301, USA.
*
*      Author:
*      John Joseph
*
*/

#pragma once

#include <typeindex>
#include <string>
#include <map>
#include <set>
#include <list>

#include "pyl_funcs.h"
#include "pyl_classes.h"
#include "pyl_misc.h"

namespace pyl
{
	/********************************************//*!
	pyl::ModuleDef
	\brief A Python module definition, encapsulated in a C++ class

	This class stores and manages a Python module that can be accessed
	from within the python interpreter. The constructor is private,
	and all modules must be added via the public static pyl::Module::Create method
	***********************************************/
	class ModuleDef
	{
		// Private constructor, static map, and static internal module get function
	private:
		static std::map<std::string, ModuleDef> s_mapPyModules;

		/*!
		\brief Construct module from name and docs

		The Module constructor is private because any modules that aren't constructed
		and properly registered within the internal module map will not be available to
		the python interpreter when it is initialized
		*/
		ModuleDef( const std::string& moduleName, const std::string& moduleDocs );

		// Private members

		std::map<std::type_index, _ExposedClassDef> m_mapExposedClasses;	/*!< A map of exposable C++ class types */
		std::list<_PyFunc> m_liExposedFunctions;							/*!< A list of exposed c++ functions */
		
		_MethodDefs m_ntMethodDefs;									/*!< A null terminated MethodDef buffer */
		std::list<std::string> m_liMethodDocs;
		std::set<std::string> m_setUsedMethodNames;

		PyModuleDef m_pyModDef;											/*!< The actual Python module def */
		std::string m_strModDocs;										/*!< The string containing module docs */
		std::string m_strModName;										/*!< The string containing the module name */
		std::function<PyObject *()> m_fnModInit;						/*!< Function called on import that creates the module*/
		std::function<void( Object )> m_fnCustomInit;

		// Sets up m_fnModInit
		void createFnObject();

		// Implementation of expose object function that doesn't need to be in this header file
		int exposeObject_impl( const std::type_index T, voidptr_t instance, const std::string& name, PyObject * mod );

		// Implementation of RegisterClass that doesn't need to be in this header file
		bool registerClass_impl( const std::type_index T, const  std::string& strClassName );
		bool registerClass_impl( const std::type_index T, const std::type_index P, const std::string& strClassName, const ModuleDef * const pParentMod );

		// Adds a method to the null terminated method def buf
		bool addMethod_impl( std::string strMethodName, PyCFunction fnPtr, int flags, std::string docs );

		// Calls the prepare function on all of our exposed classes
		void prepareClasses();

		// When pyl::ModuleDef::CreateModuleDef is called, the module created is added to the list of builtin modules via PyImport_AppendInittab.
		// The PyImport_AppendInittab function relies on a live char * to the module's name provided by this interface. 
		// Doing it any other way, or doing it in such a way that the char * will not remain valid, will prevent your module from 
		// being imported (which is why they're stored in a map, where references are not invalidated.) 
		const char * getNameBuf() const;

		// Add a new method def fo the Method Definitions of the module
		template <typename tag>
		bool addFunction( const _PyFunc pFn, const std::string methodName, const int methodFlags, const std::string docs )
		{
			// We need to store these where they won't move
			m_liExposedFunctions.push_back( pFn );

			// now make the function pointer (TODO figure out these ids, or do something else)
			PyCFunction fnPtr = get_fn_ptr<tag>( m_liExposedFunctions.back() );

			// You can key the methodName string to a std::function
			if ( addMethod_impl( methodName, fnPtr, methodFlags, docs ) )
				return true;

			m_liExposedFunctions.pop_back();
			return false;
		}

		// Like the above but for member functions of exposed C++ clases
		template <typename tag, class C>
		bool addMemFunction( const std::string methodName, const _PyFunc pFn, const int methodFlags, const std::string docs )
		{
			auto it = m_mapExposedClasses.find( typeid(C) );
			if ( it == m_mapExposedClasses.end() )
				return false;

			// We need to store these where they won't move
			m_liExposedFunctions.push_back( pFn );

			// now make the function pointer (TODO figure out these ids, or do something else)
			PyCFunction fnPtr = get_fn_ptr<tag>( m_liExposedFunctions.back() );

			// Add function
			if ( it->second.AddMethod( methodName, fnPtr, methodFlags, docs ) )
				return true;

			m_liExposedFunctions.pop_back();
			return false;
		}

		// The public expose APIs
	public:

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Functions for registering non-member functions
		////////////////////////////////////////////////////////////////////////////////////////////////////

		/*! RegisterFunction
		\brief Register some R methodName(Args...)

		\tparam tag An undefined type used internally
		\tparam R The return type
		\tparam Args The variadic type containing all function arguments

		\param[in] methodName The name of the function as seen by Python
		\param[in] fn A std::function object wrapping the function
		\param[in] docs The optional documentation for the function, as seen by Python

		Use this function to register some non-member function that would be invoked like
		R returnedVal = methodName(Args...);
		*/
		template <typename tag, typename R, typename ... Args>
		bool RegisterFunction( std::string methodName, std::function<R( Args... )> fn, std::string docs = "" )
		{
			_PyFunc pFn = _getPyFunc_Case1( fn );

			return addFunction<tag>( pFn, methodName, METH_VARARGS, docs );
		}

		/*! RegisterFunction
		\brief Register some void methodName(Args...)

		\tparam tag An undefined type used internally
		\tparam Args The variadic type containing all function arguments

		\param[in] methodName The name of the function as seen by Python
		\param[in] fn A std::function object wrapping the function
		\param[in] docs The optional documentation for the function, as seen by Python

		Use this function to register some void non-member function that would be invoked like
		methodName(Args...);
		*/
		template <typename tag, typename ... Args>
		bool RegisterFunction( const std::string methodName, const std::function<void( Args... )> fn, const std::string docs = "" )
		{
			_PyFunc pFn = _getPyFunc_Case2( fn );

			return addFunction<tag>( pFn, methodName, METH_VARARGS, docs );
		}

		/*! RegisterFunction
		\brief Register some void methodName(Args...)

		\tparam tag An undefined type used internally
		\tparam R The return type

		\param[in] methodName The name of the function as seen by Python
		\param[in] fn A std::function object wrapping the function
		\param[in] docs The optional documentation for the function, as seen by Python

		Use this function to register some non-member function that would be invoked like
		R returnedVal = methodName();
		*/
		template <typename tag, typename R>
		bool RegisterFunction( std::string methodName, const std::function<R()> fn, const std::string docs = "" )
		{
			_PyFunc pFn = _getPyFunc_Case3( fn );

			return addFunction<tag>( pFn, methodName, METH_NOARGS, docs );
		}

		/*! RegisterFunction
		\brief Register some void methodName(Args...)

		\tparam tag An undefined type used internally

		\param[in] methodName The name of the function as seen by Python
		\param[in] fn A std::function object wrapping the function
		\param[in] docs The optional documentation for the function, as seen by Python

		Use this function to register some non-member function that would be invoked like
		methodName();
		*/
		template <typename tag>
		bool RegisterFunction( const std::string methodName, const std::function<void()> fn, const std::string docs = "" )
		{
			_PyFunc pFn = _getPyFunc_Case4( fn );

			return addFunction<tag>( pFn, methodName, METH_NOARGS, docs );
		}


		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Functions for registering C++ class member functions
		////////////////////////////////////////////////////////////////////////////////////////////////////

		/*! RegisterMemFunction
		\brief Register some R C::methodName(Args...)

		\tparam C The C++ class that this function is a member of
		\tparam tag An undefined type used internally
		\tparam R The return type
		\tparam Args The variadic type containing all function arguments

		\param[in] methodName The name of the function as seen by Python
		\param[in] fn A std::function object wrapping the function
		\param[in] docs The optional documentation for the function, as seen by Python

		Use this function to register some member function of class C that would be invoked like
		C instance;
		...
		R returnedVal = c.methodName(Args...);
		*/
		template <typename C, typename tag, typename R, typename ... Args,
			typename std::enable_if<sizeof...(Args) != 1, int>::type = 0>
		bool RegisterMemFunction( const std::string methodName, const std::function<R( Args... )> fn, const std::string docs = "" )
		{
			_PyFunc pFn = _getPyFunc_Mem_Case1<C>( fn );
			return addMemFunction<tag, C>( methodName, pFn, METH_VARARGS, docs );
		}

		/*! RegisterMemFunction
		\brief Register some void C::methodName(Args...)

		\tparam C The C++ class that this function is a member of
		\tparam tag An undefined type used internally
		\tparam Args The variadic type containing all function arguments

		\param[in] methodName The name of the function as seen by Python
		\param[in] fn A std::function object wrapping the function
		\param[in] docs The optional documentation for the function, as seen by Python

		Use this function to register some member function of class C that would be invoked like
		C instance;
		...
		c.methodName(Args...);
		*/
		template <typename C, typename tag, typename ... Args>
		bool RegisterMemFunction( const std::string methodName, std::function<void( Args... )> fn, const std::string docs = "" )
		{
			_PyFunc pFn = _getPyFunc_Mem_Case2<C>( fn );
			return addMemFunction<tag, C>( methodName, pFn, METH_VARARGS, docs );
		}

		/*! RegisterMemFunction
		\brief Register some void C::methodName(Args...)

		\tparam C The C++ class that this function is a member of
		\tparam tag An undefined type used internally
		\tparam R The return type

		\param[in] methodName The name of the function as seen by Python
		\param[in] fn A std::function object wrapping the function
		\param[in] docs The optional documentation for the function, as seen by Python

		Use this function to register some member function of class C that would be invoked like
		C instance;
		...
		R returnedVal = c.methodName();
		*/
		template <typename C, typename tag, typename R>
		bool RegisterMemFunction( const std::string methodName, std::function<R( C * )> fn, const std::string docs = "" )
		{
			_PyFunc pFn = _getPyFunc_Mem_Case3<C>( fn );
			return addMemFunction<tag, C>( methodName, pFn, METH_NOARGS, docs );
		}

		/*! RegisterMemFunction
		\brief Register some void C::methodName()
		\tparam C The C++ class that this function is a member of
		\tparam tag An undefined type used internally

		\param[in] methodName The name of the function as seen by Python
		\param[in] fn A std::function object wrapping the function
		\param[in] docs The optional documentation for the function, as seen by Python
		\param[out] success An int indicating the success of the operation, should be 0

		Use this function to register some member function of class C that would be invoked like
		C instance;
		...
		c.methodName();
		*/
		template <typename C, typename tag>
		bool RegisterMemFunction( const std::string methodName, const std::function<void( C * )> fn, const std::string docs = "" )
		{
			_PyFunc pFn = _getPyFunc_Mem_Case4<C>( fn );
			return addMemFunction<tag, C>( methodName, pFn, METH_NOARGS, docs );
		}


		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Functions for registering C++ types with the module
		////////////////////////////////////////////////////////////////////////////////////////////////////

		/*! RegisterClass
		\brief Register a C++ type as a python type in this module
		\tparam C The type of the C++ object you'd like to register

		\param[in] className The name of the type you're exposing, as seen by Python

		Use this function to register a C++ type as a type inside this module. This function
		creates a class definition for the desired C++ type and declares it as a class within the module. 
		Because it modifies the module it must be called prior to import. 
		*/
		template <class C>
		bool RegisterClass( std::string className )
		{
			return registerClass_impl( typeid(C), className );
		}

		template <class C, class P>
		bool RegisterClass( std::string className, const ModuleDef * const pParentClassMod )
		{
			return registerClass_impl( typeid(C), typeid(P), className, pParentClassMod );
		}


		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Functions for exposing existing C++ class instances whose types are declared to in the module
		////////////////////////////////////////////////////////////////////////////////////////////////////

		/*! Expose_Object
		\brief Expose an existing instance of class C
		\tparam C The type of the C++ object you'd like to expose

		\param[in] instance The address of the object being exposed
		\param[in] name The name of the object as seen by Python
		\param[in] mod The python module you'd like to have the instance live in (i.e the main module)

		Use this function to expose an existing C++ class instance into some python module, provided that the
		type has been declared inside this module. The pointer to the instance must remain valid for as long
		as you expect to be able to use this object, and any calls made to this object in pythono will be
		made by this instance
		*/
		template <class C>
		int Expose_Object( C * instance, const std::string name, PyObject * mod = nullptr )
		{
			// Make sure it's a valid pointer
			if ( !instance )
				return -1;

			// Call the implementation function, which makes sure it's in our type map and creates the PyObject
			return exposeObject_impl( typeid(C), static_cast<voidptr_t>(instance), name, mod );
		}


		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Innocent functions
		////////////////////////////////////////////////////////////////////////////////////////////////////

		/*! GetModuleDef
		\brief Retreive a pointer to an existing pyl::Module definition

		\param[in] moduleName The name of the module you'd like to retreive
		\param[out] pModule A pointer to the module you've requested, returns nullptr if not found	

		Use this function to get a pointer to a previously created module definition. If you plan on 
		modifying the definition you must do it prior to initializing the interpreter
		*/
		static ModuleDef * GetModuleDef( const std::string moduleName );

		/*! CreateModule
		\brief Create a new pyl::Module object

		\param[in] moduleName The name of the module you'd like to create
		\param[in] moduleName The optional docString of the module, as seen by Python
		\param[out] pModule A pointer to the module you've just greated, nullptr if something went wrong

		Use this function to get create a new Python module (meaning it must be called prior to initializing the interpreter.)
		The module will be available to to the interpreter under the name moduleName, and if you provide a documentation string
		then that will be available to the interpreter via the __help__ function (?)
		*/
		template <typename tag>
		static ModuleDef * Create( const std::string moduleName, const std::string moduleDocs = "" )
		{
			if ( ModuleDef * pExistingDef = GetModuleDef( moduleName ) )
				return pExistingDef;

			// Add to map
			ModuleDef& mod = s_mapPyModules[moduleName] = ModuleDef( moduleName, moduleDocs );

			// Create an initialize m_fnModInit
			mod.createFnObject();

			// Add this module to the list of builtin modules, and ensure m_fnModInit gets called on import
			int success = PyImport_AppendInittab( mod.getNameBuf(), get_fn_ptr<tag>( mod.m_fnModInit ) );
            if (success != 0)
            {
                throw pyl::runtime_error("Error creating module " + moduleName);
                return nullptr;
            }
    
			return &mod;
		}

		/*! AsObject
		\brief Get the Python module as a pyl::Object
		
		\param[out] pylObject The module object as a pyl::Object (compares true to nullptr if something went wrong)

		Use this function to get a module as a pyl::Object. Once you have it as an object, you can invoke functions like
		call_function or get_attr to access the module from C++ code. Note that this is different from having a handle
		to the module definition; this is the real PyObject, owning references to all objects exposed within the module
		and retreived by calling PyImport_ImportModule directly
		*/
		Object AsObject() const;

		/*! InitAllModules
		\brief Initialize all modules in the internal module map

		This function invokes Module::prepareClasses() on every function in the internal module map, which bakes any 
		existing class definitions and creates the corresponding type objects inside this modules Python representation.
		This function should only be called once before the interpreter is initialized, and should not be called again
		until the interpreter has been shut down. 
		*/
		static int InitAllModules();

		void SetCustomModuleInit( std::function<void( Object )> fnInit );

		// Don't ever call this... it isn't even implemented, but some STL containers demand that it exists
		ModuleDef();
	};

	Object GetMainModule();
	Object GetModule( std::string modName );
}

#define S1(x) #x
#define S2(x) S1(x)

#define CreateMod(strModName)\
	pyl::ModuleDef::Create<struct __st_##strModName>(#strModName)

#define CreateModWithDocs(strModName, strModDocs)\
	pyl::ModuleDef::Create<struct __st_#strModName>(strModName, strModDocs)

#define AddFnToMod(M, F)\
	M->RegisterFunction<struct __st_fn##F>(#F, pyl::make_function(F))

#define AddMemFnToMod(M, C, F, R, ...)\
	std::function<R(C *, ##__VA_ARGS__)> fn##F = &C::F;\
	M->RegisterMemFunction<C, struct __st_fn##C##F>(#F, fn##F)
