//***************************************************************************
//  SYS
//  Copyright (C) Premise Systems, Inc. 1998-2001.
//
//  File: StdAfx.h
//
//  Purpose/Usage:
//    This file contains the...
//
//  Prerequisites for Use:
//    FOO.H
//
//  Code File(s) Needed: OURFILE.CPP
//
//  To Do:
//    Nothing
//
//  History:
//    10-Jan-99     tchipman   Created
//
//***************************************************************************



// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#if !defined(AFX_STDAFX_H__B8697ABC_6B3F_4A12_A8B0_E873D917CA12__INCLUDED_)
#define AFX_STDAFX_H__B8697ABC_6B3F_4A12_A8B0_E873D917CA12__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define STRICT
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#define _ATL_APARTMENT_THREADED

#include <sysdbg.h>
#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
extern CComModule _Module;
#include <atlcom.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <driverutil.h>
#include <prcomm.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__B8697ABC_6B3F_4A12_A8B0_E873D917CA12__INCLUDED)
