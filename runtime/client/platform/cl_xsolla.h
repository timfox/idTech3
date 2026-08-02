/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Xsolla client integration header - client-side API.
===========================================================================
*/

#ifndef _CL_XSOLLA_H_
#define _CL_XSOLLA_H_

#include "xsolla_shared.h"

// Client initialization
void CL_Xsolla_Init(void);
void CL_Xsolla_Shutdown(void);
void CL_Xsolla_Frame(void);

// Client commands
void CL_Xsolla_OpenLoginUI_f(void);
void CL_Xsolla_OpenPaymentUI_f(void);
void CL_Xsolla_CheckStatus_f(void);
void CL_Xsolla_GetUserInfo_f(void);
void CL_Xsolla_CreateUser_f(void);
void CL_Xsolla_GetPaymentToken_f(void);
void CL_Xsolla_ProcessPayment_f(void);

// Utility functions
qboolean CL_Xsolla_IsEnabled(void);
const char *CL_Xsolla_GetUserToken(void);
void CL_Xsolla_SetUserToken(const char *token);

#endif // _CL_XSOLLA_H_