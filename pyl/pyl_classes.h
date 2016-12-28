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

#include <memory>

#include <Python.h>
#include <structmember.h>

#include "pyl_convert.h"

namespace pyl
{
	// Every python module function looks like this
	using _PyFunc = std::function<PyObject *(PyObject *, PyObject *)>;

	// Deleter that calls Py_XDECREF on the PyObject parameter.
	struct _PyObjectDeleter
	{
		void operator()( PyObject *obj )
		{
			Py_XDECREF( obj );
		}
	};
	// unique_ptr that uses Py_XDECREF as the destructor function.
	using pyunique_ptr = std::unique_ptr<PyObject, _PyObjectDeleter>;

	// Inherit from std::runtime_error... felt like the right thing to do
	class runtime_error : public std::runtime_error
	{
	public:
		runtime_error( std::string strMessage ) : std::runtime_error( strMessage ) {}
	};

	// Defines an exposed class (which is not per instance)
	// as well as a list of exposed instances
	using _MethodDefs = std::basic_string<PyMethodDef>;
	using _MemberDefs = std::basic_string<PyMemberDef>;
	class _ExposedClassDef
	{
	private:
		std::string m_strClassName;
		_MethodDefs m_ntMethodDefs;
		std::set<std::string> m_setUsedMethodNames;
		std::list<std::string> m_liMethodDocs;

		_MemberDefs m_ntMemberDefs;
		std::set<std::string> m_setUsedMemberNames;
		std::list<std::string> m_liMemberDocs;

		PyTypeObject m_TypeObject;

	public:
		bool AddMethod( std::string strMethodName, PyCFunction fnPtr, int flags, std::string docs = "" );
		bool AddMember( std::string strMemberName, int type, int offset, int flags, std::string doc = "" );

		void Prepare();
		bool IsPrepared() const;

		PyTypeObject * GetTypeObject() const;
		const char * GetName() const;
		void SetName( std::string strName );

		_ExposedClassDef();
		_ExposedClassDef( std::string strClassName );
		_ExposedClassDef( const _ExposedClassDef& other ) = default;

		_ExposedClassDef& operator=( const _ExposedClassDef& other ) = default;

		static int PyClsInitFunc( PyObject * self, PyObject * args, PyObject * kwargs );
		static PyObject * PyClsCallFunc( PyObject * co, PyObject * args, PyObject * kwargs );

		// All exposed objects inherit from this python type, which has a capsule
		// member holding a pointer to the original object
		struct _GenericPyClass
		{
			PyObject_HEAD
				PyObject * capsule{ nullptr };
		};
	};

	// TODO more doxygen!
	// This is the original pywrapper::object... quite the beast
	/**
	* \class Object
	* \brief This class represents a python object.
	*/
	class Object
	{
	public:
		/**
		* \brief Constructs a default python object
		*/
		Object();

		/**
		* \brief Constructs a python object from a PyObject pointer.
		*
		* This Object takes ownership of the PyObject* argument. That
		* means no Py_INCREF is performed on it.
		* \param obj The pointer from which to construct this Object.
		*/
		Object( PyObject *obj );

		// Construct an object from a script
		Object( std::string strScript );

		/**
		* \brief Calls the callable attribute "name" using the provided
		* arguments.
		*
		* This function might throw a std::runtime_error if there is
		* an error when calling the function.
		*
		* \param name The name of the attribute to be called.
		* \param args The arguments which will be used when calling the
		* attribute.
		* \return pyl::Object containing the result of the function.
		*/
		template<typename... Args>
		Object call( const std::string name, const Args... args )
		{
			pyunique_ptr func( load_function( name ) );
			// Create the tuple argument
			pyunique_ptr tup( PyTuple_New( sizeof...(args) ) );
			add_tuple_vars( tup, args... );
			// Call our object
			PyObject *ret( PyObject_CallObject( func.get(), tup.get() ) );
			if ( !ret )
			{
				PyErr_Print();
				throw pyl::runtime_error( "Failed to call function " + name );
			}
			return{ ret };
		}

		/**
		* \brief Calls a callable attribute using no arguments.
		*
		* This function might throw a std::runtime_error if there is
		* an error when calling the function.
		*
		* \sa pyl::Object::call_function.
		* \param name The name of the callable attribute to be executed.
		* \return pyl::Object containing the result of the function.
		*/
		Object call( const std::string name );

		/**
		* \brief Finds and returns the attribute named "name".
		*
		* This function might throw a std::runtime_error if an error
		* is encountered while fetching the attribute.
		*
		* \param name The name of the attribute to be returned.
		* \return pyl::Object representing the attribute.
		*/
		Object get_attr( const std::string &name );

		// Like above, but without converts as well
		template<typename T>
		bool get_attr( const std::string strName, T& obj )
		{
			Object o = get_attr( strName );
			if ( o.get() != nullptr )
				return o.convert( obj );
			return false;
		}

		/**
		* \brief Checks whether this object contains a certain attribute.
		*
		* \param name The name of the attribute to be searched.
		* \return bool indicating whether the attribute is defined.
		*/
		bool has_attr( const std::string &name );

		template<typename T>
		bool set_attr( const std::string &name, T obj )
		{
			PyObject * pyObj = alloc_pyobject( obj );
			int success = PyObject_SetAttrString( this->get(), name.c_str(), pyObj );
			return (success == 0);
		}

		/**
		* \brief Returns the internal PyObject*.
		*
		* No reference increment is performed on the PyObject* before
		* returning it, so any DECREF applied to it without INCREF'ing
		* it will cause undefined behaviour.
		* \return The PyObject* which this Object is representing.
		*/
		PyObject *get() const { return py_obj.get(); }

		template<class T>
		bool convert( T &param )
		{
			return pyl::convert( py_obj.get(), param );
		}

		/**
		* \brief Constructs a pyl::Object from a script.
		*
		* The returned Object will be the representation of the loaded
		* script. If any errors are encountered while loading this
		* script, a std::runtime_error is thrown.
		*
		* \param script_path The path of the script to be loaded.
		* \return Object representing the loaded script.
		*/
		static Object from_script( const std::string &script_path );

		void Reset();

	protected:
		typedef std::shared_ptr<PyObject> pyshared_ptr;

		PyObject *load_function( const std::string &name );

		pyshared_ptr make_pyshared( PyObject *obj );

		// Variadic template method to add items to a tuple
		template<typename First, typename... Rest>
		void add_tuple_vars( pyunique_ptr &tup, const First &head, const Rest&... tail )
		{
			add_tuple_var(
				tup,
				PyTuple_Size( tup.get() ) - sizeof...(tail) -1,
				head
				);
			add_tuple_vars( tup, tail... );
		}


		void add_tuple_vars( pyunique_ptr &tup, PyObject *arg )
		{
			add_tuple_var( tup, PyTuple_Size( tup.get() ) - 1, arg );
		}

		// Base case for add_tuple_vars
		template<typename Arg>
		void add_tuple_vars( pyunique_ptr &tup, const Arg &arg )
		{
			add_tuple_var( tup,
						   PyTuple_Size( tup.get() ) - 1, alloc_pyobject( arg )
						   );
		}

		// Adds a PyObject* to the tuple object
		void add_tuple_var( pyunique_ptr &tup, Py_ssize_t i, PyObject *pobj )
		{
			PyTuple_SetItem( tup.get(), i, pobj );
		}

		// Adds a PyObject* to the tuple object
		template<class T> void add_tuple_var( pyunique_ptr &tup, Py_ssize_t i,
											  const T &data )
		{
			PyTuple_SetItem( tup.get(), i, alloc_pyobject( data ) );
		}

		pyshared_ptr py_obj;
	};
}