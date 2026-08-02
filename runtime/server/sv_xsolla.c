/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Xsolla server integration - handles Xsolla webhook processing and server-side operations.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "server.h"
#include "xsolla_shared.h"

// Xsolla webhook processing
void SV_XsollaWebhook_f(void) {
    char signature[256];
    char body[4096];
    xsollaWebhookEvent_t event;
    xsollaApiResponse_t response;

    // Get signature from command line
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: xsolla_webhook <signature> [body]\n");
        return;
    }

    Q_strncpyz(signature, Cmd_Argv(1), sizeof(signature));

    // Get body if provided
    if (Cmd_Argc() >= 3) {
        Q_strncpyz(body, Cmd_Argv(2), sizeof(body));
    } else {
        body[0] = '\0';
    }

    // Process webhook
    if (XSOLLA_ProcessWebhook(signature, body, &event)) {
        // Handle webhook event
        switch (event.type) {
            case XSOLLA_WEBHOOK_PAYMENT:
                Com_Printf("Xsolla: Payment webhook received\n");
                // Handle payment webhook
                break;
            case XSOLLA_WEBHOOK_SUBSCRIPTION_CREATED:
                Com_Printf("Xsolla: Subscription created webhook received\n");
                // Handle subscription created webhook
                break;
            case XSOLLA_WEBHOOK_SUBSCRIPTION_CANCELLED:
                Com_Printf("Xsolla: Subscription cancelled webhook received\n");
                // Handle subscription cancelled webhook
                break;
            case XSOLLA_WEBHOOK_SUBSCRIPTION_RENEWED:
                Com_Printf("Xsolla: Subscription renewed webhook received\n");
                // Handle subscription renewed webhook
                break;
            case XSOLLA_WEBHOOK_SUBSCRIPTION_EXPIRED:
                Com_Printf("Xsolla: Subscription expired webhook received\n");
                // Handle subscription expired webhook
                break;
            case XSOLLA_WEBHOOK_REFUND:
                Com_Printf("Xsolla: Refund webhook received\n");
                // Handle refund webhook
                break;
            case XSOLLA_WEBHOOK_CUSTOM:
                Com_Printf("Xsolla: Custom webhook received\n");
                // Handle custom webhook
                break;
        }
    } else {
        Com_Printf("Xsolla: Failed to process webhook\n");
    }
}

// Xsolla payment processing command
void SV_XsollaPayment_f(void) {
    char payment_token[256];
    int user_id;
    int amount;
    char currency[8];
    xsollaApiResponse_t response;

    if (Cmd_Argc() < 4) {
        Com_Printf("Usage: xsolla_payment <user_id> <amount> <currency> [token]\n");
        return;
    }

    user_id = atoi(Cmd_Argv(1));
    amount = atoi(Cmd_Argv(2));
    Q_strncpyz(currency, Cmd_Argv(3), sizeof(currency));

    // Process payment
    if (XSOLLA_ProcessPayment(Cmd_Argv(4), &response)) {
        Com_Printf("Xsolla: Payment processed successfully\n");
        // Handle successful payment
    } else {
        Com_Printf("Xsolla: Payment processing failed: %s\n", response.error_message);
    }
}

// Xsolla subscription management command
void SV_XsollaSubscription_f(void) {
    char action[32];
    int user_id;
    int sku_id;
    xsollaSubscriptionInfo_t info;
    xsollaApiResponse_t response;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: xsolla_subscription <create|cancel|get> <user_id> [sku_id]\n");
        return;
    }

    Q_strncpyz(action, Cmd_Argv(1), sizeof(action));
    user_id = atoi(Cmd_Argv(2));

    if (Q_stricmp(action, "create") == 0) {
        if (Cmd_Argc() < 4) {
            Com_Printf("Usage: xsolla_subscription create <user_id> <sku_id>\n");
            return;
        }
        sku_id = atoi(Cmd_Argv(3));

        if (XSOLLA_CreateSubscription(user_id, sku_id, &info)) {
            Com_Printf("Xsolla: Subscription created (id: %d)\n", info.subscription_id);
        } else {
            Com_Printf("Xsolla: Failed to create subscription\n");
        }
    } else if (Q_stricmp(action, "cancel") == 0) {
        if (XSOLLA_CancelSubscription(user_id, &response)) {
            Com_Printf("Xsolla: Subscription cancelled\n");
        } else {
            Com_Printf("Xsolla: Failed to cancel subscription: %s\n", response.error_message);
        }
    } else if (Q_stricmp(action, "get") == 0) {
        if (XSOLLA_GetSubscription(user_id, &info)) {
            Com_Printf("Xsolla: Subscription info - status: %s, expires: %ld\n", info.status, (long)info.expires_at);
        } else {
            Com_Printf("Xsolla: Failed to get subscription\n");
        }
    } else {
        Com_Printf("Unknown action: %s\n", action);
    }
}

// Xsolla user management command
void SV_XsollaUser_f(void) {
    char action[32];
    int user_id;
    char email[128];
    char username[64];
    xsollaUserInfo_t info;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: xsolla_user <get|create> <user_id> [email] [username]\n");
        return;
    }

    Q_strncpyz(action, Cmd_Argv(1), sizeof(action));
    user_id = atoi(Cmd_Argv(2));

    if (Q_stricmp(action, "get") == 0) {
        if (XSOLLA_GetUserInfo(user_id, &info)) {
            Com_Printf("Xsolla: User info - username: %s, email: %s, verified: %d, banned: %d\n",
                      info.username, info.email, info.verified, info.banned);
        } else {
            Com_Printf("Xsolla: Failed to get user info\n");
        }
    } else if (Q_stricmp(action, "create") == 0) {
        if (Cmd_Argc() < 5) {
            Com_Printf("Usage: xsolla_user create <user_id> <email> <username>\n");
            return;
        }
        Q_strncpyz(email, Cmd_Argv(3), sizeof(email));
        Q_strncpyz(username, Cmd_Argv(4), sizeof(username));

        if (XSOLLA_CreateUser(email, username, &info)) {
            Com_Printf("Xsolla: User created (id: %d)\n", info.user_id);
        } else {
            Com_Printf("Xsolla: Failed to create user\n");
        }
    } else {
        Com_Printf("Unknown action: %s\n", action);
    }
}

// Initialize Xsolla server commands
void SV_XsollaInit(void) {
    if (!XSOLLA_Init()) {
        Com_Printf("Xsolla: Server-side initialization failed\n");
        return;
    }

    // Register server commands
    Cmd_AddCommand("xsolla_webhook", SV_XsollaWebhook_f);
    Cmd_AddCommand("xsolla_payment", SV_XsollaPayment_f);
    Cmd_AddCommand("xsolla_subscription", SV_XsollaSubscription_f);
    Cmd_AddCommand("xsolla_user", SV_XsollaUser_f);

    Com_Printf("Xsolla: Server commands registered\n");
}

// Shutdown Xsolla server
void SV_XsollaShutdown(void) {
    XSOLLA_Shutdown();

    // Remove server commands
    Cmd_RemoveCommand("xsolla_webhook");
    Cmd_RemoveCommand("xsolla_payment");
    Cmd_RemoveCommand("xsolla_subscription");
    Cmd_RemoveCommand("xsolla_user");

    Com_Printf("Xsolla: Server commands removed\n");
}

// Xsolla server frame
void SV_XsollaFrame(void) {
    XSOLLA_Frame();
}
