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

// pyliason.h
// Basically a gateway to the other header files.
// Most of the functions used by clients live in
// pyl_expose.h

// This header contains the functions
// used to expose functions and objects
// to the python interpreter

#include "pyl_misc.h"
#include "pyl_module.h"


/********************************************//*!
	\namespace pyl
	\brief The namespace in which Pyliaison functionality lives

	The Pyliaison (pyl) namespace contains all of the classes and functions
	used when facilitating communication between C++ and Python
***********************************************/
namespace pyl
{
	/********************************************//*!
	pyl::initialize
	\brief Initialize the python interpreter and all custom modules

	This function closes any pre-existing python interpreter (via Py_Finalize)
	and creates all python modules declared as pyl::ModuleDef instances via py::Module::InitAllModules. 
	Once that is done a call to Py_Initialize is made. 
	***********************************************/
	void initialize();
    
	/********************************************//*!
	pyl::finalize
	\brief Finalize the python interpreter via Py_Finalize
	***********************************************/
	void finalize();
    
	void print_error();
	void clear_error();
	void print_object(PyObject *obj);

	/********************************************//*!
	pyl::RunCmd
	\brief Executes a string command (in python) as if it was run via console

	\param[in] cmd The string based python command
	\param[out] ret The integer returned by PyRun_SimpleString
	***********************************************/
	int RunCmd( std::string cmd );
	
	/********************************************//*!
	pyl::RunFile
	\brief Executes a python script file on disk

	\param[in] fileName The full filename of the script
	\param[out] ret The integer returned by PyRun_SimpleString
	***********************************************/
    int RunFile(std::string fileName);
}
