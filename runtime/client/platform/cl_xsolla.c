/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Xsolla client integration - handles Xsolla Login and payment flow.
===========================================================================
*/

#include "client.h"
#include "cl_xsolla.h"
#include "xsolla_shared.h"
#include <stdio.h>
#include <string.h>

// Xsolla user token
static char xsollaUserToken[256] = {0};

// Xsolla initialization
void CL_Xsolla_Init(void) {
    Com_Printf("Xsolla: client initialization\n");
    
    // Initialize Xsolla shared state
    if (!XSOLLA_Init()) {
        Com_Printf("Xsolla: failed to initialize shared state\n");
        return;
    }
    
    Com_Printf("Xsolla: client initialized successfully\n");
}

void CL_Xsolla_Shutdown(void) {
    Com_Printf("Xsolla: client shutdown\n");
    
    // Shutdown Xsolla shared state
    XSOLLA_Shutdown();
    
    // Clear user token
    memset(xsollaUserToken, 0, sizeof(xsollaUserToken));
}

void CL_Xsolla_Frame(void) {
    if (!XSOLLA_IsInitialized()) {
        return;
    }
    
    // Xsolla client frame processing (if needed)
    XSOLLA_Frame();
}

// Open Xsolla Login UI
void CL_Xsolla_OpenLoginUI_f(void) {
    if (!XSOLLA_IsInitialized()) {
        Com_Printf("Xsolla: not initialized\n");
        return;
    }
    
    Com_Printf("Xsolla: opening login UI\n");
    
    // TODO: Implement Xsolla Login UI
    // This would open the Xsolla Login UI in a browser or webview
    // For now, just print a message
    Com_Printf("Xsolla: login UI would open here (project_id: %d)\n", xsolla_project_id->integer);
}

// Open Xsolla Payment UI
void CL_Xsolla_OpenPaymentUI_f(void) {
    if (!XSOLLA_IsInitialized()) {
        Com_Printf("Xsolla: not initialized\n");
        return;
    }
    
    Com_Printf("Xsolla: opening payment UI\n");
    
    // TODO: Implement Xsolla Payment UI
    // This would open the Xsolla Payment UI in a browser or webview
    // For now, just print a message
    Com_Printf("Xsolla: payment UI would open here (project_id: %d)\n", xsolla_project_id->integer);
}

// Check Xsolla status
void CL_Xsolla_CheckStatus_f(void) {
    if (!XSOLLA_IsInitialized()) {
        Com_Printf("Xsolla: not initialized\n");
        return;
    }
    
    Com_Printf("Xsolla: status\n");
    Com_Printf("  project_id: %d\n", xsolla_project_id->integer);
    Com_Printf("  enabled: %d\n", xsolla_enabled->integer);
    Com_Printf("  user_token: %s\n", xsollaUserToken[0] ? xsollaUserToken : "(not set)");
}

// Get user information command
void CL_Xsolla_GetUserInfo_f(void) {
    if (!XSOLLA_IsInitialized()) {
        Com_Printf("Xsolla: not initialized\n");
        return;
    }
    
    Com_Printf("Xsolla: get user info (not implemented)\n");
    
    // TODO: Implement Xsolla API call to get user info
}

// Create user command
void CL_Xsolla_CreateUser_f(void) {
    if (!XSOLLA_IsInitialized()) {
        Com_Printf("Xsolla: not initialized\n");
        return;
    }
    
    Com_Printf("Xsolla: create user (not implemented)\n");
    
    // TODO: Implement Xsolla API call to create user
}

// Get payment token command
void CL_Xsolla_GetPaymentToken_f(void) {
    if (!XSOLLA_IsInitialized()) {
        Com_Printf("Xsolla: not initialized\n");
        return;
    }
    
    Com_Printf("Xsolla: get payment token (not implemented)\n");
    
    // TODO: Implement Xsolla API call to get payment token
}

// Process payment command
void CL_Xsolla_ProcessPayment_f(void) {
    if (!XSOLLA_IsInitialized()) {
        Com_Printf("Xsolla: not initialized\n");
        return;
    }
    
    Com_Printf("Xsolla: process payment (not implemented)\n");
    
    // TODO: Implement Xsolla API call to process payment
}

// Check if Xsolla is enabled
qboolean CL_Xsolla_IsEnabled(void) {
    return xsolla_enabled && xsolla_enabled->integer;
}

// Get Xsolla user token
const char *CL_Xsolla_GetUserToken(void) {
    return xsollaUserToken;
}

// Set Xsolla user token
void CL_Xsolla_SetUserToken(const char *token) {
    if (token) {
        Q_strncpyz(xsollaUserToken, token, sizeof(xsollaUserToken));
    } else {
        memset(xsollaUserToken, 0, sizeof(xsollaUserToken));
    }
}