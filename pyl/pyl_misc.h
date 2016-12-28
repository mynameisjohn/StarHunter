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

#include <functional>
#include <string>
#include <utility>
namespace pyl
{
    // This feels gross
    using voidptr_t = void *;
    
    // Utility for preprending tabs to python code
	const std::string py_tab = "    ";
	inline std::string getTabs(int n) {
		std::string ret;
		for (int i = 0; i < n; i++)
			ret += py_tab;
		return ret;
	}

	// Invoke some callable object with a std::tuple
	// Note that everything here is computed at compile time
	template<typename Func, typename Tup, std::size_t... index>
	decltype(auto) invoke_helper(Func&& func, Tup&& tup, std::index_sequence<index...>){
		return func(std::get<index>(std::forward<Tup>(tup))...);
	}

	template<typename Func, typename Tup>
	decltype(auto) invoke(Func&& func, Tup&& tup){
		constexpr auto Size = std::tuple_size<typename std::decay<Tup>::type>::value;
		return 
			invoke_helper(std::forward<Func>(func), std::forward<Tup>(tup), std::make_index_sequence<Size>{});
	}

    // This was also stolen from stack overflow
    // but I'm hoping to phase it out. It allows me to expose
    // std::functions as function pointers, which python
    // wants for its PyMethodDef buffer
	template <typename _UniqueTag, typename _Res, typename... _ArgTypes>
	struct fun_ptr_helper
	{
	public:
		typedef std::function<_Res(_ArgTypes...)> function_type;

		static void bind(function_type&& f)
		{
			instance().fn_.swap(f);
		}

		static void bind(const function_type& f)
		{
			instance().fn_ = f;
		}

		static _Res invoke(_ArgTypes... args)
		{
			return instance().fn_(args...);
		}

		typedef decltype(&fun_ptr_helper::invoke) pointer_type;
		static pointer_type ptr()
		{
			return &invoke;
		}

	private:
		static fun_ptr_helper& instance()
		{
			static fun_ptr_helper inst_;
			return inst_;
		}

		fun_ptr_helper() {}

		function_type fn_;
	};

	template <typename _UniqueTag, typename _Res, typename... _ArgTypes>
	typename fun_ptr_helper<_UniqueTag, _Res, _ArgTypes...>::pointer_type
		get_fn_ptr(const std::function<_Res(_ArgTypes...)>& f)
	{
		fun_ptr_helper<_UniqueTag, _Res, _ArgTypes...>::bind(f);
		return fun_ptr_helper<_UniqueTag, _Res, _ArgTypes...>::ptr();
	}

	template<typename T>
	std::function<typename std::enable_if<std::is_function<T>::value, T>::type>
		make_function(T *t)
	{
		return{ t };
	}

	// TODO
	//rewrite the above for class member functions

	int GetTotalRefCount();
}
