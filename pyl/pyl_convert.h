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

#include <vector>
#include <list>
#include <map>
#include <set>
#include <array>

#include <Python.h>

#include "pyl_module.h"
#include "pyl_overloads.h"

namespace pyl
{
	// ------------ Conversion functions ------------

	// Convert a PyObject to a std::string.
	bool convert(PyObject *obj, std::string &val);
	// Convert a PyObject to a std::vector<char>.
	bool convert(PyObject *obj, std::vector<char> &val);
	// Convert a PyObject to a bool value.
	bool convert(PyObject *obj, bool &value);
	// Convert a PyObject to any integral type.
	template<class T, typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
	bool convert(PyObject *obj, T &val) {
		if (!PyLong_Check(obj))
			return false;
		val = PyLong_AsLong(obj);
		return true;
	}

	// Convert a PyObject to an float.
	bool convert(PyObject *obj, double &val);
	bool convert(PyObject *obj, float &val);

	// Add to Tuple functions
	// These recurse to an arbitrary base b
	// and convert objects in a PyTuple to objects in a 
	// std::tuple. I added the b parameter because I wanted
	// to set it to 1 and leave the first element alone. 
	
	// Base case, when n==b, just convert and return
	template<size_t n, size_t b, class... Args>
	typename std::enable_if<n == b, bool>::type
		add_to_tuple(PyObject *obj, std::tuple<Args...> &tup) {
		return convert(PyTuple_GetItem(obj, n-b), std::get<n>(tup));
	}

	// Recurse down to b; note that this can't compile 
	// if n <= b because you'll overstep the bounds
	// of the tuple, which is a compile time thing
	template<size_t n, size_t b, class... Args>
	typename std::enable_if<n != b, bool>::type
		add_to_tuple(PyObject *obj, std::tuple<Args...> &tup) {
		add_to_tuple<n - 1, b, Args...>(obj, tup);
		return convert(PyTuple_GetItem(obj, n-b), std::get<n>(tup));
	}

	template<class... Args>
	bool convert(PyObject *obj, std::tuple<Args...> &tup) {
		if (!PyTuple_Check(obj) ||
			PyTuple_Size(obj) != sizeof...(Args))
			return false;
		return add_to_tuple<sizeof...(Args)-1, 0, Args...>(obj, tup);
	}
	// Convert a PyObject to a std::map
	template<class K, class V>
	bool convert(PyObject *obj, std::map<K, V> &mp) {
		if (!PyDict_Check(obj))
			return false;
		PyObject *py_key, *py_val;
		Py_ssize_t pos(0);
		while (PyDict_Next(obj, &pos, &py_key, &py_val)) {
			K key;
			if (!convert(py_key, key))
				return false;
			V val;
			if (!convert(py_val, val))
				return false;
			mp.insert(std::make_pair(key, val));
		}
		return true;
	}
    // Convert a PyObject to a std::set
    template<class C>
    bool convert(PyObject *obj, std::set<C>& s){
        if (!PySet_Check(obj))
            return false;
        PyObject *iter = PyObject_GetIter(obj);
        PyObject *item = PyIter_Next(iter);
        while (item){
            C val;
            if (!convert(item, val))
                return false;
            s.insert(val);
			item = PyIter_Next(iter);
        }
        return true;
    }
	// Convert a PyObject to a generic container.
	template<class T, class C>
	bool convert_list(PyObject *obj, C &container) {
		if (!PyList_Check(obj))
			return false;
		for (Py_ssize_t i(0); i < PyList_Size(obj); ++i) {
			T val;
			if (!convert(PyList_GetItem(obj, i), val))
				return false;
			container.push_back(std::move(val));
		}
		return true;
	}
	// Convert a PyObject to a std::list.
	template<class T> bool convert(PyObject *obj, std::list<T> &lst) {
		return convert_list<T, std::list<T>>(obj, lst);
	}
	// Convert a PyObject to a std::vector.
	template<class T> bool convert(PyObject *obj, std::vector<T> &vec) {
		return convert_list<T, std::vector<T>>(obj, vec);
	}
    
    // Convert a PyObject to a contiguous buffer (very unsafe, but hey)
    template<class T> bool convert_buf(PyObject *obj, T * arr, int N){
        if (!PyList_Check(obj))
            return false;
        Py_ssize_t len = PyList_Size(obj);
        if (len > N) len = N;
        for (Py_ssize_t i(0); i < len; ++i) {
            T& val = arr[i];
			PyObject * pItem = PyList_GetItem( obj, i );
            if (!convert(pItem, val))
                return false;
        }
        return true;
    }
    
    // Convert a PyObject to a std::array, safe version of above
    template<class T, size_t N> bool convert(PyObject *obj, std::array<T, N>& arr){
        return convert_buf<T>(obj, arr.data(), int(N));
    }

	// Generic convert function used by others
	template<class T> bool generic_convert(PyObject *obj,
		const std::function<bool(PyObject*)> &is_obj,
		const std::function<T(PyObject*)> &converter,
		T &val) {
		if (!is_obj(obj))
			return false;
		val = converter(obj);
		return true;
	}

	// Convert to a pyl::Object; useful if function can unpack it
	class Object;
	bool convert(PyObject * obj, pyl::Object& pyObj);

	// This gets invoked on calls to member functions, which require the instance ptr
    // It may be dangerous, since any pointer type will be interpreted
    // as a PyCObject, but so far it's been useful. To protect yourself from collisions,
	// try and specialize any type that you don't want getting caught in this conversion
	template<typename T>
	bool convert(PyObject * obj, T *& val) {
		// Try getting the pointer from the capsule
		T * pRet = static_cast<T *>(PyCapsule_GetPointer( obj, NULL ));
		if ( pRet )
		{
			val = pRet;
			return true;
		}
		
		// If that doesn't work, try converting from a size_t
		return convert<size_t>( obj, (size_t&) val );
	}

	// -------------- PyObject allocators ----------------

	// Creates a PyObject from any integral type(gets converted to PyLong)
	template<class T, typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
	PyObject *alloc_pyobject(T num) {
		return PyLong_FromLong(num);
	}

	// Creates a PyString from a std::string
	PyObject *alloc_pyobject(const std::string &str);

	// Creates a PyByteArray from a std::vector<char>
	PyObject *alloc_pyobject(const std::vector<char> &val, size_t sz);

	// Creates a PyByteArray from a std::vector<char>
	PyObject *alloc_pyobject(const std::vector<char> &val);

	// Creates a PyString from a const char*
	PyObject *alloc_pyobject(const char *cstr);

	// Creates a PyBool from a bool
	PyObject *alloc_pyobject(bool value);

	// Creates a PyFloat from a double
	PyObject *alloc_pyobject(double num);

	// Creates a PyFloat from a float
	PyObject *alloc_pyobject(float num);

    // I guess this is kind of a catch-all for pointer types
    template <typename T>
    PyObject * alloc_pyobject(T * ptr){
        return PyCapsule_New((voidptr_t)ptr, NULL, NULL);
    }

	// Generic python list allocation
	template<class T> static PyObject *alloc_list(const T &container) {
		PyObject *lst(PyList_New(container.size()));

		Py_ssize_t i(0);
		for (auto it(container.begin()); it != container.end(); ++it)
			PyList_SetItem(lst, i++, alloc_pyobject(*it));

		return lst;
	}
    
	// Creates a PyList from a std::vector
	template<class T> PyObject *alloc_pyobject(const std::vector<T> &container) {
		return alloc_list(container);
	}

	// Creates a PyList from a std::list
	template<class T> PyObject *alloc_pyobject(const std::list<T> &container) {
		return alloc_list(container);
	}

	// Creates a PyDict from a std::map
	template<class T, class K> PyObject *alloc_pyobject(
		const std::map<T, K> &container) {
		PyObject *dict(PyDict_New());

		for (auto it(container.begin()); it != container.end(); ++it)
			PyDict_SetItem(dict,
				alloc_pyobject(it->first),
				alloc_pyobject(it->second)
				);

		return dict;
	}
    
    // Creates a PySet from a std::set
    template<class C> PyObject *alloc_pyobject(const std::set<C>& s){
        PyObject * pSet(PySet_New(NULL));
        for (auto& i : s){
            PySet_Add(pSet, alloc_pyobject(i));
        }
        return pSet;
    }
    
    // TODO unordered maps/sets
}
