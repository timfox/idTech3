/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Xsolla integration header - shared between client and server.
===========================================================================
*/

#ifndef _XSOLLA_SHARED_H_
#define _XSOLLA_SHARED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"
#include "qcommon.h"

// Xsolla configuration
#define XSOLLA_API_URL_DEFAULT "https://api.xsolla.com"
#define XSOLLA_LOGIN_URL_DEFAULT "https://login.xsolla.com"
#define XSOLLA_WEBHOOK_URL_DEFAULT "/api/v1/webhooks/xsolla"

// Xsolla API endpoints
#define XSOLLA_ENDPOINT_TOKEN "/v1/token"
#define XSOLLA_ENDPOINT_USER "/v1/users"
#define XSOLLA_ENDPOINT_SKU "/v1/skus"
#define XSOLLA_ENDPOINT_SUBSCRIPTION "/v1/subscriptions"
#define XSOLLA_ENDPOINT_CHECKOUT "/v1/checkout"

// Xsolla configuration cvars
extern cvar_t *xsolla_project_id;
extern cvar_t *xsolla_project_secret;
extern cvar_t *xsolla_api_url;
extern cvar_t *xsolla_login_url;
extern cvar_t *xsolla_enabled;

// Xsolla state
typedef struct {
    qboolean initialized;
    int project_id;
    char project_secret[256];
    char api_url[256];
    char login_url[256];
    char webhook_url[256];
} xsollaState_t;

// Xsolla user information
typedef struct {
    int user_id;
    char username[64];
    char email[128];
    qboolean verified;
    qboolean banned;
} xsollaUserInfo_t;

// Xsolla payment token
typedef struct {
    char token[256];
    int amount;
    char currency[8];
    char sku_id[64];
    char order_id[64];
    qboolean is_test;
} xsollaPaymentToken_t;

// Xsolla subscription information
typedef struct {
    int subscription_id;
    int sku_id;
    int user_id;
    char status[32];
    time_t created_at;
    time_t expires_at;
    qboolean auto_renew;
} xsollaSubscriptionInfo_t;

// Xsolla webhook event types
typedef enum {
    XSOLLA_WEBHOOK_PAYMENT = 0,
    XSOLLA_WEBHOOK_SUBSCRIPTION_CREATED,
    XSOLLA_WEBHOOK_SUBSCRIPTION_CANCELLED,
    XSOLLA_WEBHOOK_SUBSCRIPTION_RENEWED,
    XSOLLA_WEBHOOK_SUBSCRIPTION_EXPIRED,
    XSOLLA_WEBHOOK_REFUND,
    XSOLLA_WEBHOOK_CUSTOM
} xsollaWebhookEventType_t;

// Xsolla webhook event
typedef struct {
    xsollaWebhookEventType_t type;
    int event_id;
    time_t timestamp;
    char payload[4096];
} xsollaWebhookEvent_t;

// Xsolla API response
typedef struct {
    int status_code;
    char error_message[256];
    qboolean success;
} xsollaApiResponse_t;

// Function declarations
qboolean XSOLLA_Init(void);
void XSOLLA_Shutdown(void);
void XSOLLA_Frame(void);

// User management
qboolean XSOLLA_GetUserInfo(int user_id, xsollaUserInfo_t *info);
qboolean XSOLLA_CreateUser(const char *email, const char *username, xsollaUserInfo_t *info);

// Payment
qboolean XSOLLA_GetPaymentToken(int user_id, int amount, const char *currency, xsollaPaymentToken_t *token);
qboolean XSOLLA_ProcessPayment(const char *payment_token, xsollaApiResponse_t *response);

// Subscription
qboolean XSOLLA_GetSubscription(int subscription_id, xsollaSubscriptionInfo_t *info);
qboolean XSOLLA_CreateSubscription(int user_id, int sku_id, xsollaSubscriptionInfo_t *info);
qboolean XSOLLA_CancelSubscription(int subscription_id, xsollaApiResponse_t *response);

// Webhook handling
qboolean XSOLLA_ProcessWebhook(const char *signature, const char *body, xsollaWebhookEvent_t *event);
qboolean XSOLLA_VerifyWebhookSignature(const char *signature, const char *body);

// Utility functions
const char *XSOLLA_GetProjectID(void);
const char *XSOLLA_GetProjectSecret(void);
qboolean XSOLLA_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif // _XOLLA_SHARED_H_
"