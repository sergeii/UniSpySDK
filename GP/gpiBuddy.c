///////////////////////////////////////////////////////////////////////////////
// File:	gpiBuddy.c
// SDK:		GameSpy Presence and Messaging SDK
//
// Copyright (c) 2012 GameSpy Technology & IGN Entertainment, Inc. All rights
// reserved. This software is made available only pursuant to certain license
// terms offered by IGN or its subsidiary GameSpy Industries, Inc. Unlicensed
// use or use in a manner not expressly authorized by IGN or GameSpy Technology
// is prohibited.

//INCLUDES
//////////
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gpi.h"

//FUNCTIONS
GPResult gpiSendAuthBuddyRequest(GPConnection * connection,
								 GPIProfile * profile,
								 GPIBool autoSync)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;

	// Send the auth.
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\authadd\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\sesskey\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->sessKey);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\fromprofileid\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, profile->profileId);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\sig\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, profile->authSig);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\final\\");

	return GP_NO_ERROR;
}

GPResult
gpiProcessRecvBuddyMessage(
  GPConnection * connection,
  const char * input
)
{
	char buffer[4096];
	int type;
	int profileid;
	time_t date;
	GPICallback callback;
	GPIProfile * profile;
	GPIBuddyStatus * buddyStatus;
	char intValue[16];
	char * str;
	unsigned short port;
	int productID;
	GPIConnection * iconnection = (GPIConnection*)*connection;
	char strTemp[GS_MAX(GP_STATUS_STRING_LEN, GP_LOCATION_STRING_LEN)];

	// Check the type of bm.
	if(!gpiValueForKey(input, "\\bm\\", buffer, sizeof(buffer)))
		CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	type = atoi(buffer);

	// Get the profile this is from.
	if(!gpiValueForKey(input, "\\f\\", buffer, sizeof(buffer)))
		CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	profileid = atoi(buffer);

	// Get the time.
	if(!gpiValueForKey(input, "\\date\\", buffer, sizeof(buffer)))
		date = time(NULL);
	else
		date = atoi(buffer);

	// What type of message is this?
	switch(type)
	{
	case GPI_BM_MESSAGE:
		// Call the callback.
		callback = iconnection->callbacks[GPI_RECV_BUDDY_MESSAGE];
		if(callback.callback != NULL)
		{
			GPRecvBuddyMessageArg * arg;
			arg = (GPRecvBuddyMessageArg *)gsimalloc(sizeof(GPRecvBuddyMessageArg));
			if(arg == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");

			if(!gpiValueForKey(input, "\\msg\\", buffer, sizeof(buffer)))
				CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
#ifndef GSI_UNICODE
			arg->message = (char *)gsimalloc(strlen(buffer) + 1);
			if(arg->message == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");
			strcpy(arg->message, buffer);
			arg->profile = (GPProfile)profileid;
			arg->date = (unsigned int)date;
#else
			arg->message = (gsi_char *) gsimalloc( (1 + strlen(buffer) ) * sizeof(gsi_char) );
			if(arg->message == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");
			UTF8ToUCSString(buffer, arg->message);
			arg->profile = (GPProfile)profileid;
			arg->date = (unsigned int)date;
#endif
			CHECK_RESULT(gpiAddCallback(connection, callback, arg, NULL, GPI_ADD_MESSAGE));
		}
		break;
	case GPI_BM_UTM:
		// Call the callback.
		callback = iconnection->callbacks[GPI_RECV_BUDDY_UTM];
		if(callback.callback != NULL)
		{
			GPRecvBuddyUTMArg * arg;
			arg = (GPRecvBuddyUTMArg *)gsimalloc(sizeof(GPRecvBuddyUTMArg));
			if(arg == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");

			if(!gpiValueForKey(input, "\\msg\\", buffer, sizeof(buffer)))
				CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
#ifndef GSI_UNICODE
			arg->message = (char *)gsimalloc(strlen(buffer) + 1);
			if(arg->message == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");
			strcpy(arg->message, buffer);
			arg->profile = (GPProfile)profileid;
			arg->date = (unsigned int)date;
#else
			arg->message = (gsi_char*)gsimalloc( (1 + strlen(buffer) ) * sizeof(gsi_char) );
			if(arg->message == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");
			UTF8ToUCSString(buffer, arg->message);
			arg->profile = (GPProfile)profileid;
			arg->date = (unsigned int)date;
#endif
			CHECK_RESULT(gpiAddCallback(connection, callback, arg, NULL, GPI_ADD_BUDDYUTM));
		}
		break;

	case GPI_BM_REQUEST:
		// Get the profile, adding if needed.
		profile = gpiProfileListAdd(connection, profileid);
		if(!profile)
			Error(connection, GP_MEMORY_ERROR, "Out of memory.");

		// Get the reason.
		if(!gpiValueForKey(input, "\\msg\\", buffer, sizeof(buffer)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		// Find where the sig starts.
		str = strstr(buffer, "|signed|");
		if(str == NULL)
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		// Get the sig out of the message.
		*str = '\0';
		str += 8;
		if(strlen(str) != 32)
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
		freeclear(profile->authSig);
		profile->authSig = goastrdup(str);
		profile->requestCount++;

		// Call the callback.
		callback = iconnection->callbacks[GPI_RECV_BUDDY_REQUEST];
		if(callback.callback != NULL)
		{
			GPRecvBuddyRequestArg * arg;
			arg = (GPRecvBuddyRequestArg *)gsimalloc(sizeof(GPRecvBuddyRequestArg));
			if(arg == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");
#ifndef GSI_UNICODE
			strzcpy(arg->reason, buffer, GP_REASON_LEN);
#else
			UTF8ToUCSString(buffer, arg->reason);
#endif
			arg->profile = (GPProfile)profileid;
			arg->date = (unsigned int)date;

			CHECK_RESULT(gpiAddCallback(connection, callback, arg, NULL, GPI_ADD_BUDDDYREQUEST));
		}
		break;

	case GPI_BM_AUTH:
		// call the callback
		callback = iconnection->callbacks[GPI_RECV_BUDDY_AUTH];
		if(callback.callback != NULL)
		{
			GPRecvBuddyAuthArg * arg;
			arg = (GPRecvBuddyAuthArg *)gsimalloc(sizeof(GPRecvBuddyAuthArg));

			if (arg == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");
			arg->profile = (GPProfile)profileid;
			arg->date = (unsigned int)date;
			CHECK_RESULT(gpiAddCallback(connection, callback, arg, NULL, GPI_ADD_BUDDYAUTH));
		}
		break;

	case GPI_BM_REVOKE:
		// Call the callback.
		callback = iconnection->callbacks[GPI_RECV_BUDDY_REVOKE];
		if(callback.callback != NULL)
		{
			GPRecvBuddyRevokeArg * arg;
			arg = (GPRecvBuddyRevokeArg *)gsimalloc(sizeof(GPRecvBuddyRevokeArg));

			if (arg == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");
			arg->profile = (GPProfile)profileid;
			arg->date = (unsigned int)date;

			CHECK_RESULT(gpiAddCallback(connection, callback, arg, NULL, GPI_ADD_BUDDYREVOKE));
		}
		break;
		

	case GPI_BM_STATUS:


		// Get the profile, adding if needed.
		profile = gpiProfileListAdd(connection, profileid);
		if(!profile)
			Error(connection, GP_MEMORY_ERROR, "Out of memory.");

		// First check if we are connected, (i.e., the complete buddy list
		// exists), then check if have this buddy in the local buddy list.
		/////////////////////////////////////
		if ((iconnection->connectState == GPI_CONNECTED) &&
			 profile->deletedLocally)
		{
			// This buddy is not in our local buddy list. We got this 
			// message because of a race condition we must have been 
			// deleting. Ignore this buddy status message.

			gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_WarmError,
				"Deliberately ignoring Buddy Message Status update from profile id [%i], \
				because could not be found in the local buddy list. It must have been removed \n", 
				profileid);
			break;
		}
		// Make sure profile wasn't blocked prior to getting the status update.
        if (!profile->blocked)
        {
		    // This is a buddy.
		    if(!profile->buddyStatus)
		    {
			    profile->buddyStatus = (GPIBuddyStatus *)gsimalloc(sizeof(GPIBuddyStatus));
			    if(!profile->buddyStatus)
				    Error(connection, GP_MEMORY_ERROR, "Out of memory.");
			    memset(profile->buddyStatus, 0, sizeof(GPIBuddyStatus));
			    if (profile->buddyStatusInfo)
			    {
				    profile->buddyStatus->buddyIndex = profile->buddyStatusInfo->buddyIndex;
				    gpiRemoveBuddyStatusInfo(profile->buddyStatusInfo);
				    profile->buddyStatusInfo = NULL;
			    }
			    else
				    profile->buddyStatus->buddyIndex = iconnection->profileList.numBuddies++;
		    }

		    // Get the buddy status.
		    buddyStatus = profile->buddyStatus;

		    // Get the msg.
		    if(!gpiValueForKey(input, "\\msg\\", buffer, sizeof(buffer)))
			    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		    // Get the status.
		    if(!gpiValueForKey(buffer, "|s|", intValue, sizeof(intValue)))
		    {
			    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
		    }
		    else
		    {
			    buddyStatus->status = (GPEnum)atoi(intValue);
		    }
		    // Get the status string.
		    freeclear(buddyStatus->statusString);
		    if(!gpiValueForKey(buffer, "|ss|", strTemp, GP_STATUS_STRING_LEN))
			    strTemp[0] = '\0';
		    buddyStatus->statusString = goastrdup(strTemp);
		    if(!buddyStatus->statusString)
			    Error(connection, GP_MEMORY_ERROR, "Out of memory.");

		    // Get the location string.
		    freeclear(buddyStatus->locationString);
		    if(!gpiValueForKey(buffer, "|ls|", strTemp, GP_LOCATION_STRING_LEN))
			    strTemp[0] = '\0';
		    buddyStatus->locationString = goastrdup(strTemp);
		    if(!buddyStatus->locationString)
			    Error(connection, GP_MEMORY_ERROR, "Out of memory.");

		    // Get the ip.
		    if(!gpiValueForKey(buffer, "|ip|", intValue, sizeof(intValue)))
			    buddyStatus->ip = 0;
		    else
			    buddyStatus->ip = htonl((unsigned int)atoi(intValue));

		    // Get the port.
		    if(!gpiValueForKey(buffer, "|p|", intValue, sizeof(intValue)))
			    buddyStatus->port = 0;
		    else
		    {
			    port = (unsigned short)atoi(intValue);
			    buddyStatus->port = htons(port);
		    }

		    // Get the quiet mode flags.
		    if(!gpiValueForKey(buffer, "|qm|", intValue, sizeof(intValue)))
			    buddyStatus->quietModeFlags = GP_SILENCE_NONE;
		    else
			    buddyStatus->quietModeFlags = (GPEnum)atoi(intValue);

		    // Call the callback.
		    callback = iconnection->callbacks[GPI_RECV_BUDDY_STATUS];
		    if(callback.callback != NULL)		
		    {
			    GPRecvBuddyStatusArg * arg;
			    arg = (GPRecvBuddyStatusArg *)gsimalloc(sizeof(GPRecvBuddyStatusArg));
			    if(arg == NULL)
				    Error(connection, GP_MEMORY_ERROR, "Out of memory.");

			    arg->profile = (GPProfile)profileid;
			    arg->index = buddyStatus->buddyIndex;
			    arg->date = (unsigned int)date;

			    CHECK_RESULT(gpiAddCallback(connection, callback, arg, NULL, GPI_ADD_STATUS));
		    }
        }
		break;

	case GPI_BM_INVITE:
		// Get the msg.
		if(!gpiValueForKey(input, "\\msg\\", buffer, sizeof(buffer)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		// Find the productid.
		str = strstr(buffer, "|p|");
		if(str == NULL)
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		// Skip the |p|.
		str += 3;
		if(str[0] == '\0')
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		// Get the productid.
		productID = atoi(str);

		// Find the location string (optional - older versions won't have this).
		str = strstr(buffer, "|l|");
		if(str != NULL)
			strzcpy(strTemp, (str+3), sizeof(strTemp));
		else
			strTemp[0] = '\0'; // No location, set to empty string.
		
		// Call the callback.
		callback = iconnection->callbacks[GPI_RECV_GAME_INVITE];
		if(callback.callback != NULL)
		{
			GPRecvGameInviteArg * arg;
			arg = (GPRecvGameInviteArg *)gsimalloc(sizeof(GPRecvGameInviteArg));
			if(arg == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");

			arg->profile = (GPProfile)profileid;
			arg->productID = productID;
#ifdef GSI_UNICODE
			AsciiToUCSString(strTemp, arg->location);
#else
			gsiSafeStrcpyA(arg->location, strTemp, sizeof(arg->location));
#endif

			CHECK_RESULT(gpiAddCallback(connection, callback, arg, NULL, 0));
		}
		break;

	case GPI_BM_PING:
		// Get the msg.
		if(!gpiValueForKey(input, "\\msg\\", buffer, sizeof(buffer)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		// Send back a pong.
		gpiSendBuddyMessage(connection, profileid, GPI_BM_PONG, "1", 0, NULL);

		break;

#ifndef NOFILE
	case GPI_BM_PONG:
		// Lets the transfers handle this.
		gpiTransfersHandlePong(connection, profileid, NULL);

		break;
#endif
	}

	return GP_NO_ERROR;
}

GPResult gpiProcessRecvBuddyStatusInfo(GPConnection *connection, const char *input)
{
	char buffer[1024];
	int profileid;
	time_t date;
	GPICallback callback;
	GPIProfile * profile;
	GPIBuddyStatusInfo * buddyStatusInfo;
	GPIConnection * iconnection = (GPIConnection*)*connection;

	// This is what the message should look like. It's broken up for easy viewing.
	//
	// "\bsi\\state\\profile\\bip\\bport\\hostip\\hprivip\"
	// "\qport\\hport\\sessflags\\rstatus\\gameType\"
	// "\gameVnt\\gameMn\\product\\qmodeflags\"
	date = time(NULL);
	// Get the buddy's profile
	if(!gpiValueForKey(input, "\\profile\\", buffer, sizeof(buffer)))
		CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	profileid = atoi(buffer);

	// Get the profile from the SDK's list, adding it if needed.
	profile = gpiProfileListAdd(connection, profileid);
	if(!profile)
		Error(connection, GP_MEMORY_ERROR, "Out of memory.");

    // Make sure profile wasn't blocked prior to getting the status update or 
	// deleted from the local buddy list.
    if (!profile->blocked && !profile->deletedLocally)
    {
	    // This is a buddy.
	    if(!profile->buddyStatusInfo)
	    {
		    profile->buddyStatusInfo = (GPIBuddyStatusInfo *)gsimalloc(sizeof(GPIBuddyStatusInfo));
		    if(!profile->buddyStatusInfo)
			    Error(connection, GP_MEMORY_ERROR, "Out of memory.");
		    memset(profile->buddyStatusInfo, 0, sizeof(GPIBuddyStatusInfo));
		    if (profile->buddyStatus)
		    {
			    profile->buddyStatusInfo->buddyIndex = profile->buddyStatus->buddyIndex;
			    gpiRemoveBuddyStatus(profile->buddyStatus);
			    profile->buddyStatus = NULL;
		    }
		    else
			    profile->buddyStatusInfo->buddyIndex = iconnection->profileList.numBuddies++;
		    profile->buddyStatusInfo->extendedInfoKeys = ArrayNew(sizeof(GPIKey), GPI_INITIAL_NUM_KEYS, gpiStatusInfoKeyFree);
		    if (!profile->buddyStatusInfo->extendedInfoKeys)
			    Error(connection, GP_MEMORY_ERROR, "Out of memory.");
	    }

	    // Extract the buddy status information and fill in the appropriate 
		// information.
	    buddyStatusInfo = profile->buddyStatusInfo;
    	
	    if (!gpiValueForKey(input, "\\state\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->statusState = (GPEnum)atoi(buffer);

	    if (!gpiValueForKey(input, "\\bip\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->buddyIp = htonl((unsigned int)atoi(buffer));
    	
	    if (!gpiValueForKey(input, "\\bport\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->buddyPort = (unsigned short)atoi(buffer);
    	
	    if (!gpiValueForKey(input, "\\hostip\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->hostIp = htonl((unsigned int)atoi(buffer));

	    if (!gpiValueForKey(input, "\\hprivip\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->hostPrivateIp = htonl((unsigned int)atoi(buffer));

	    if (!gpiValueForKey(input, "\\qport\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->queryPort = (unsigned short)atoi(buffer);

	    if (!gpiValueForKey(input, "\\hport\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->hostPort = (unsigned short)atoi(buffer);
    	
	    if (!gpiValueForKey(input, "\\sessflags\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->sessionFlags = (unsigned int)atoi(buffer);

	    freeclear(buddyStatusInfo->richStatus);
	    if (!gpiValueForKey(input, "\\rstatus\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->richStatus = goastrdup(buffer);
    	
	    freeclear(buddyStatusInfo->gameType);
	    if (!gpiValueForKey(input, "\\gameType\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->gameType = goastrdup(buffer);

	    freeclear(buddyStatusInfo->gameVariant);
	    if (!gpiValueForKey(input, "\\gameVnt\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->gameVariant = goastrdup(buffer);

	    freeclear(buddyStatusInfo->gameMapName);
	    if (!gpiValueForKey(input, "\\gameMn\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->gameMapName = goastrdup(buffer);

	    if (!gpiValueForKey(input, "\\product\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->productId = (int)atoi(buffer);

	    if (!gpiValueForKey(input, "\\qmodeflags\\", buffer, sizeof(buffer)))
		    CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
	    buddyStatusInfo->quietModeFlags = (GPEnum)atoi(buffer);
    	
	    callback = iconnection->callbacks[GPI_RECV_BUDDY_STATUS];
	    if (callback.callback != NULL)
	    {
		    GPRecvBuddyStatusArg *anArg;
		    anArg = (GPRecvBuddyStatusArg *)gsimalloc(sizeof(GPRecvBuddyStatusArg));
		    if (anArg == NULL)
			    Error(connection, GP_MEMORY_ERROR, "Out of memory.");

		    anArg->date = (unsigned int)date;
		    anArg->index = buddyStatusInfo->buddyIndex;
		    anArg->profile = profileid;

		    CHECK_RESULT(gpiAddCallback(connection, callback, anArg, NULL, 0));
	    }
    }
	return GP_NO_ERROR;
	
}

GPResult
gpiProcessRecvBuddyList(
  GPConnection * connection,
  const char * input
)
{
    int i=0;
    unsigned int j=0;
    int num = 0;
    int index = 0;
    char c;
    const char *str = NULL;
    char buffer[512];
    GPIProfile * profile;
    GPProfile profileid;
    GPIConnection * iconnection = (GPIConnection*)*connection;

    // Check for an error.
    if(gpiCheckForError(connection, input, GPITrue))
        return GP_SERVER_ERROR;

    // Process Buddy List Retrieval msg - Format like:
    /* ===============================================
       \bdy\<num in list>\list\<block list - comma delimited>\final\
       =============================================== */

    if(!gpiValueForKeyWithIndex(input, "\\bdy\\", &index, buffer, sizeof(buffer)))
        CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
    num = atoi(buffer);

    // Check to make sure list is there.
    str = strstr(input, "\\list\\");
    if (str == NULL)
        CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

    // Then increment index to get ready for parsing.
    str += 6;
    index += 6;

    for (i=0; i < num; i++)
    {     
        if (i==0)
        {
            // Manually grab first profile in list - comma delimiter.
            for(j=0 ; (j < sizeof(buffer)) && ((c = str[j]) != '\0') && (c != ',') ; j++)
            {
                buffer[j] = c;
            }
            buffer[j] = '\0';
            index += j;
        }
        else
        {
            if(!gpiValueForKeyWithIndex(input, ",", &index, buffer, sizeof(buffer)))
                CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");
        }

        profileid = atoi(buffer);

        // Get the profile, adding if needed.
        profile = gpiProfileListAdd(connection, profileid);
        if(!profile)
            Error(connection, GP_MEMORY_ERROR, "Out of memory.");

        // Mark as offline buddy for now until we get the real status.
#ifdef GP_NEW_STATUS_INFO
        // Use new status info as placeholder.
        profile->buddyStatusInfo = (GPIBuddyStatusInfo *)gsimalloc(sizeof(GPIBuddyStatusInfo));
        if(!profile->buddyStatusInfo)
            Error(connection, GP_MEMORY_ERROR, "Out of memory.");
        memset(profile->buddyStatusInfo, 0, sizeof(GPIBuddyStatusInfo));

        profile->buddyStatusInfo->extendedInfoKeys = ArrayNew(sizeof(GPIKey), GPI_INITIAL_NUM_KEYS, gpiStatusInfoKeyFree);
        if (!profile->buddyStatusInfo->extendedInfoKeys)
            Error(connection, GP_MEMORY_ERROR, "Out of memory.");

        profile->buddyStatusInfo->buddyIndex = iconnection->profileList.numBuddies++;
        profile->buddyStatusInfo->statusState = GP_OFFLINE; 
#else
        // Use buddy status as placeholder.
        profile->buddyStatus = (GPIBuddyStatus *)gsimalloc(sizeof(GPIBuddyStatus));
        if(!profile->buddyStatus)
            Error(connection, GP_MEMORY_ERROR, "Out of memory.");
        memset(profile->buddyStatus, 0, sizeof(GPIBuddyStatus));
        profile->buddyStatus->buddyIndex = iconnection->profileList.numBuddies++;
        profile->buddyStatus->status = GP_OFFLINE; 

#endif
    }

    return GP_NO_ERROR;
}

GPResult
gpiSendServerBuddyMessage(
  GPConnection * connection,
  int profileid,
  int type,
  const char * message
)
{
	char buffer[3501];
	GPIConnection * iconnection = (GPIConnection*)*connection;

	// Copy the message into an internal buffer.
	strzcpy(buffer, message, sizeof(buffer));

	// Setup the message.
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\bm\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, type);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\sesskey\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->sessKey);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\t\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, profileid);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\msg\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, buffer);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\final\\");

	return GP_NO_ERROR;
}

GPResult
gpiSendBuddyMessage(
  GPConnection * connection,
  int profileid,
  int type,
  const char * message,
  int sendOption,
  GPIPeerOp *peerOp
)
{
	GPIPeer * peer;
	GPIProfile * profile;
	//GPIConnection *iconnection = (GPIConnection *)*connection;
	peer = gpiGetPeerByProfile(connection, profileid);
	if(!peer)
	{
		// Check if we should send this through the server.
		if(!gpiGetProfile(connection, profileid, &profile) ||
		   (!profile->buddyStatusInfo || !profile->buddyStatusInfo->buddyPort))
		{
			if (sendOption == GP_DONT_ROUTE)
				return GP_NETWORK_ERROR;
			return gpiSendServerBuddyMessage(connection, profileid, type, message);
		}

		// Create a new peer connection for this message.
		peer = gpiAddPeer(connection, profileid, GPITrue);
		if(!peer)
			return GP_MEMORY_ERROR;

		// Check if we need a sig.
		if(!profile->peerSig)
		{
			// Get the sig.
			CHECK_RESULT(gpiPeerGetSig(connection, peer));
		}
		else
		{
			// Try to connect to the peer.
			//////////////////////////////
			CHECK_RESULT(gpiPeerStartConnect(connection, peer));
		}
	}
	else if (peer->state == GPI_PEER_DISCONNECTED)
	{
		if (gpiGetProfile(connection, profileid, &profile))
		{
			// Clear the buddy port to prevent future messages from being sent
			// via UDP layer.
			if (profile->buddyStatusInfo)
				profile->buddyStatusInfo->buddyPort = 0;

			// Send the message through the server.
			if (sendOption == GP_DONT_ROUTE)
				return GP_NETWORK_ERROR;
			if (type < 100)
				return gpiSendServerBuddyMessage(connection, profileid, type, message);
		}
	}
	
	if (peerOp)
	{
		gpiPeerAddOp(peer, peerOp);
	}
	// Copy the message.
	CHECK_RESULT(gpiPeerAddMessage(connection, peer, type, message));

	return GP_NO_ERROR;
}

GPResult gpiBuddyHandleKeyRequest(GPConnection *connection, GPIPeer *peer)
{
	char *message;
	
	// Get all the keys and put them in the message part of bm.
	CHECK_RESULT(gpiSaveKeysToBuffer(connection, &message));
	
	// Done in case we haven't set any keys.
	if (message == NULL)
		message = "";
	
	CHECK_RESULT(gpiSendBuddyMessage(connection, peer->profile, GPI_BM_KEYS_REPLY, message, GP_DONT_ROUTE, NULL));
	
	if (strcmp(message, "")!= 0)
		freeclear(message);
	return GP_NO_ERROR;
}

GPResult gpiBuddyHandleKeyReply(GPConnection *connection, GPIPeer *peer, char *buffer)
{
	GPIProfile *pProfile;
		
	// Get the profile object to store the keys internally.
	
	if(!gpiGetProfile(connection, peer->profile, &pProfile))
		Error(connection, GP_PARAMETER_ERROR, "Invalid profile.");
	
	// Calculate the B64Decoded string len.
	if (strcmp(buffer, "") == 0)
	{
		GPIPeerOp *anIterator;
		
		for (anIterator = peer->peerOpQueue.first; anIterator != NULL; anIterator = anIterator->next)
			if (anIterator->type == GPI_BM_KEYS_REQUEST)
				break;
		
		if (!anIterator)
		{
			return GP_NO_ERROR;
		}
		else if (anIterator->type == GPI_BM_KEYS_REQUEST && anIterator->callback)
		{
			GPGetBuddyStatusInfoKeysArg *arg = (GPGetBuddyStatusInfoKeysArg *)gsimalloc(sizeof(GPGetBuddyStatusInfoKeysArg));
			GPICallback callback;
			callback.callback = anIterator->callback;
			callback.param = anIterator->userData;
			
			arg->keys = NULL;
			arg->numKeys = 0;
			arg->values = NULL;
			arg->profile = peer->profile;
			gpiAddCallback(connection, callback, arg, NULL, 0);
			gpiPeerRemoveOp(peer, anIterator);
		}
	}
	else
	{
		int decodedLen = 0,
			index = 0, numKeys, i;
		char keyName[512];
		char keyVal[512];
		char decodeKey[512];
		char decodeVal[512];
		gsi_char **keys;
		gsi_char **values;
		GPIPeerOp *anIterator;
		char *checkKey = NULL;

		// Start by getting the number of keys.
		gpiReadKeyAndValue(connection, buffer, &index,  keyName, keyVal);
		
		// Do not continue further if the header is missing.
		if (strcmp(keyName, "keys") != 0)
			CallbackError(connection, GP_NETWORK_ERROR, GP_PARSE, "Error reading keys reply message");
		
		numKeys = atoi(keyVal);

		if (numKeys == 0)
		{
			GPIPeerOp *anIterator2;
			
			for (anIterator2 = peer->peerOpQueue.first; anIterator2 != NULL; anIterator2 = anIterator2->next)
				if (anIterator2->type == GPI_BM_KEYS_REQUEST)
					break;

			if (!anIterator2)
			{
				return GP_NO_ERROR;
			}
			else if (anIterator2->type == GPI_BM_KEYS_REQUEST && anIterator2->callback)
			{
				GPGetBuddyStatusInfoKeysArg *arg = (GPGetBuddyStatusInfoKeysArg *)gsimalloc(sizeof(GPGetBuddyStatusInfoKeysArg));
				GPICallback callback;
				callback.callback = anIterator2->callback;
				callback.param = anIterator2->userData;

				arg->keys = NULL;
				arg->numKeys = 0;
				arg->values = NULL;
				arg->profile = peer->profile;
				gpiAddCallback(connection, callback, arg, NULL, 0);
				gpiPeerRemoveOp(peer, anIterator2);
			}
		}
		else
		{
			keys = (gsi_char **)gsimalloc(sizeof(gsi_char *) * numKeys);
			values = (gsi_char **)gsimalloc(sizeof(gsi_char *) * numKeys);

			for (i = 0; i < numKeys; i++)
			{			
				gpiReadKeyAndValue(connection, buffer, &index,  keyName, keyVal);
				B64Decode(keyName, decodeKey, (int)strlen(keyName), &decodedLen, 2);
				decodeKey[decodedLen] = '\0';
				B64Decode(keyVal, decodeVal, (int)strlen(keyVal), &decodedLen, 2);
				decodeVal[decodedLen] = '\0';
	#ifdef GSI_UNICODE
				keys[i] = UTF8ToUCSStringAlloc(decodeKey);
				values[i]= UTF8ToUCSStringAlloc(decodeVal);
	#else
				keys[i] = goastrdup(decodeKey);
				values[i] = goastrdup(decodeVal);
	#endif

				if (gpiStatusInfoCheckKey(connection, pProfile->buddyStatusInfo->extendedInfoKeys, decodeKey, &checkKey) == GP_NO_ERROR
					&& checkKey == NULL)
				{
					gpiStatusInfoAddKey(connection, pProfile->buddyStatusInfo->extendedInfoKeys, decodeKey, decodeVal);
				}
				else
				{
					gpiStatusInfoSetKey(connection, pProfile->buddyStatusInfo->extendedInfoKeys, decodeKey, decodeVal);
				}
			}
			
			for (anIterator = peer->peerOpQueue.first; anIterator != NULL; anIterator = anIterator->next)
				if (anIterator->type == GPI_BM_KEYS_REQUEST)
					break;

			if (!anIterator)
			{
				return GP_NO_ERROR;
			}
			else if (anIterator->type == GPI_BM_KEYS_REQUEST && anIterator->callback)
			{
				GPICallback callback;
				GPGetBuddyStatusInfoKeysArg *arg = (GPGetBuddyStatusInfoKeysArg *)gsimalloc(sizeof(GPGetBuddyStatusInfoKeysArg));
				
				callback.callback = anIterator->callback;
				callback.param = anIterator->userData;
				
				// Allocate a key array that points to each extended info key for that player.
				arg->numKeys = numKeys;

				arg->keys = keys;
				arg->values = values;
				arg->profile = peer->profile;
				
				gpiAddCallback(connection, callback, arg, NULL, GPI_ADD_BUDDYKEYS);
				gpiPeerRemoveOp(peer, anIterator);
			}
		}
	}
	
	return GP_NO_ERROR;
}

GPResult gpiAuthBuddyRequest
( 
	GPConnection * connection,
	GPProfile profile
)
{
	GPIProfile * pProfile;
	GPIConnection * iconnection = (GPIConnection*)*connection;

	// Get the profile object.
	if(!gpiGetProfile(connection, profile, &pProfile))
		Error(connection, GP_PARAMETER_ERROR, "Invalid profile.");

	// Check for a valid sig.
	if(!pProfile->authSig)
		Error(connection, GP_PARAMETER_ERROR, "Invalid profile.");

	// Send the request.
	CHECK_RESULT(gpiSendAuthBuddyRequest(connection, pProfile, GPIFalse));

	// freeclear the sig if no more requests.
	pProfile->requestCount--;
	if(!iconnection->infoCaching && (pProfile->requestCount <= 0))
	{
		freeclear(pProfile->authSig);
		if(gpiCanFreeProfile(pProfile))
			gpiRemoveProfile(connection, pProfile);
	}

	return GP_NO_ERROR;
}

GPIBool
gpiFixBuddyIndices(
  GPConnection * connection,
  GPIProfile * profile,
  void * data
)
{
#ifndef _PS2
	int baseIndex = (int)(intptr_t)data;

	GS_ASSERT((intptr_t)data <= INT_MAX);
#else
	int baseIndex = (int)data;
#endif

	GSI_UNUSED(connection);

	if(profile->buddyStatus && (profile->buddyStatus->buddyIndex > baseIndex))
		profile->buddyStatus->buddyIndex--;
	else if (profile->buddyStatusInfo && profile->buddyStatusInfo->buddyIndex > baseIndex)
		profile->buddyStatusInfo->buddyIndex--;
	return GPITrue;
}

GPResult gpiDeleteBuddy(GPConnection * connection,
						GPProfile profile,
						GPIBool sendServerRequest
)
{
	GPIProfile * pProfile;
	GPIConnection * iconnection = (GPIConnection*)*connection;
	int index;

	// Get the profile object.
	if(!gpiGetProfile(connection, profile, &pProfile))
		Error(connection, GP_PARAMETER_ERROR, "Invalid profile.");

	// Check that this is a buddy.
	// Removed - 092404 BED - User could be a buddy even though we don't have the status
	//if(!pProfile->buddyStatus)
	//	Error(connection, GP_PARAMETER_ERROR, "Profile not a buddy.");

	// Send the request.
	if (GPITrue == sendServerRequest)
	{
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\delbuddy\\");
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\sesskey\\");
		gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->sessKey);
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\delprofileid\\");
		gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, pProfile->profileId);
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\final\\");
	}

	// Need to fix up the buddy indexes.
	if (pProfile->buddyStatus)
	{
		index = pProfile->buddyStatus->buddyIndex;
		GS_ASSERT(index >= 0);
		freeclear(pProfile->buddyStatus->statusString);
		freeclear(pProfile->buddyStatus->locationString);
		freeclear(pProfile->buddyStatus);
		if(gpiCanFreeProfile(pProfile))
			gpiRemoveProfile(connection, pProfile);
		iconnection->profileList.numBuddies--;
		GS_ASSERT(iconnection->profileList.numBuddies >= 0);
#ifndef _PS2
		gpiProfileMap(connection, gpiFixBuddyIndices, (void *)(intptr_t)index);
#else
		gpiProfileMap(connection, gpiFixBuddyIndices, (void *)index);
#endif
	}
	if (pProfile->buddyStatusInfo)
	{
		index = pProfile->buddyStatusInfo->buddyIndex;
		GS_ASSERT(index >= 0);
		freeclear(pProfile->buddyStatusInfo->richStatus);
		freeclear(pProfile->buddyStatusInfo->gameType);
		freeclear(pProfile->buddyStatusInfo->gameVariant);
		freeclear(pProfile->buddyStatusInfo->gameMapName);
		if (pProfile->buddyStatusInfo->extendedInfoKeys)
		{
			ArrayFree(pProfile->buddyStatusInfo->extendedInfoKeys);
			pProfile->buddyStatusInfo->extendedInfoKeys = NULL;
		}
		freeclear(pProfile->buddyStatusInfo);

		if(gpiCanFreeProfile(pProfile))
			gpiRemoveProfile(connection, pProfile);
		iconnection->profileList.numBuddies--;
		GS_ASSERT(iconnection->profileList.numBuddies >= 0);
#ifndef _PS2
		gpiProfileMap(connection, gpiFixBuddyIndices, (void *)(intptr_t)index);
#else
		gpiProfileMap(connection, gpiFixBuddyIndices, (void *)index);
#endif
	}
	return GP_NO_ERROR;
}
