// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#if !defined(AFX_STDAFX_H__21EF3CBF_4773_4DA7_8F84_D017B68A8948__INCLUDED_)
#define AFX_STDAFX_H__21EF3CBF_4773_4DA7_8F84_D017B68A8948__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define STRICT
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#define _ATL_FREE_THREADED

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <sysdbg.h>
#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
extern CComModule _Module;
#include <atlcom.h>
#include <winsock2.h>
#include <driverutil.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__21EF3CBF_4773_4DA7_8F84_D017B68A8948__INCLUDED)
