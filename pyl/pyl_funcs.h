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

#include <Python.h>
#include <functional>

#include "pyl_classes.h"

namespace pyl
{
	// Pretty ridiculous
	template <typename C>
	static C * _getCapsulePtr(PyObject * obj)
	{
		assert(obj);
		auto gpcPtr = static_cast<_ExposedClassDef::_GenericPyClass *>((voidptr_t)obj);
		assert(gpcPtr);
		PyObject * capsule = gpcPtr->capsule;
		assert(PyCapsule_CheckExact(capsule));
		return static_cast<C *>(PyCapsule_GetPointer(capsule, NULL));
	}


	template <typename R, typename ... Args>
	_PyFunc _getPyFunc_Case1(std::function<R(Args...)> fn) {
		_PyFunc pFn = [fn](PyObject * s, PyObject * a)
		{
			std::tuple<Args...> tup;
			convert(a, tup);
			R rVal = invoke(fn, tup);

			return alloc_pyobject(rVal);
		};
		return pFn;
	}

	template <typename ... Args>
	_PyFunc _getPyFunc_Case2(std::function<void(Args...)> fn) {
		_PyFunc pFn = [fn](PyObject * s, PyObject * a)
		{
			std::tuple<Args...> tup;
			convert(a, tup);
			invoke(fn, tup);

			Py_INCREF(Py_None);
			return Py_None;
		};
		return pFn;
	}

	template <typename R>
	_PyFunc _getPyFunc_Case3(std::function<R()> fn) {
		_PyFunc pFn = [fn](PyObject * s, PyObject * a)
		{
			R rVal = fn();
			return alloc_pyobject(rVal);
		};
		return pFn;
	}

	_PyFunc _getPyFunc_Case4(std::function<void()> fn);


	template <typename C, typename R, typename ... Args>
	_PyFunc _getPyFunc_Mem_Case1(std::function<R(Args...)> fn) {
		_PyFunc pFn = [fn](PyObject * s, PyObject * a) {
			// the first arg is the instance pointer, contained in s
			std::tuple<Args...> tup;
			std::get<0>(tup) = _getCapsulePtr<C>(s);

			// recurse till the first element, getting args from a
			add_to_tuple<sizeof...(Args)-1, 1, Args...>(a, tup);

			// Invoke function, get retVal	
			R rVal = invoke(fn, tup);

			// convert rVal to PyObject, return
			return alloc_pyobject(rVal);
		};
		return pFn;
	}

	template <typename C, typename ... Args>
	_PyFunc _getPyFunc_Mem_Case2(std::function<void(Args...)> fn) {
		_PyFunc pFn = [fn](PyObject * s, PyObject * a) {
			// the first arg is the instance pointer, contained in s
			std::tuple<Args...> tup;
			std::get<0>(tup) = _getCapsulePtr<C>(s);

			// recurse till the first element, getting args from a
			add_to_tuple<sizeof...(Args)-1, 1, Args...>(a, tup);

			// invoke function
			invoke(fn, tup);

			// Return None
			Py_INCREF(Py_None);
			return Py_None;
		};
		return pFn;
	}

	template <typename C, typename R>
	_PyFunc _getPyFunc_Mem_Case3(std::function<R(C *)> fn) {
		_PyFunc pFn = [fn](PyObject * s, PyObject * a) {
			// Nothing special here
			R rVal = fn(_getCapsulePtr<C>(s));

			return alloc_pyobject(rVal);
		};
		return pFn;
	}

	template<typename C>
	_PyFunc _getPyFunc_Mem_Case4(std::function<void(C *)> fn) {
		_PyFunc pFn = [fn](PyObject * s, PyObject * a) {
			// Nothing special here
			fn(_getCapsulePtr<C>(s));

			Py_INCREF(Py_None);
			return Py_None;
		};
		return pFn;
	}

}