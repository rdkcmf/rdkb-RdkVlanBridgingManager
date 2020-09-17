/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**********************************************************************
   Copyright [2014] [Cisco Systems, Inc.]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
**********************************************************************/

#include "ansc_platform.h"
#include "vlan_dml.h"
#include "vlan_apis.h"
#include "plugin_main_apis.h"
#include "vlan_internal.h"

#if     CFG_USE_CCSP_SYSLOG
    #include <ccsp_syslog.h>
#endif
/***********************************************************************
 IMPORTANT NOTE:

 According to TR69 spec:
 On successful receipt of a SetParameterValues RPC, the CPE MUST apply
 the changes to all of the specified Parameters atomically. That is, either
 all of the value changes are applied together, or none of the changes are
 applied at all. In the latter case, the CPE MUST return a fault response
 indicating the reason for the failure to apply the changes.

 The CPE MUST NOT apply any of the specified changes without applying all
 of them.

 In order to set parameter values correctly, the back-end is required to
 hold the updated values until "Validate" and "Commit" are called. Only after
 all the "Validate" passed in different objects, the "Commit" will be called.
 Otherwise, "Rollback" will be called instead.

 The sequence in COSA Data Model will be:

 SetParamBoolValue/SetParamIntValue/SetParamUlongValue/SetParamStringValue
 -- Backup the updated values;

 if( Validate_XXX())
 {
     Commit_XXX();    -- Commit the update all together in the same object
 }
 else
 {
     Rollback_XXX();  -- Remove the update at backup;
 }

***********************************************************************/
/***********************************************************************

 APIs for Object:

    Ethernet.VLANTermination.{i}.

    *  Vlan_GetEntryCount
    *  Vlan_GetEntry
    *  Vlan_AddEntry
    *  Vlan_DelEntry
    *  Vlan_GetParamBoolValue
    *  Vlan_GetParamUlongValue
    *  Vlan_GetParamStringValue
    *  Vlan_SetParamBoolValue
    *  Vlan_SetParamUlongValue
    *  Vlan_SetParamStringValue
    *  Vlan_Validate
    *  Vlan_Commit
    *  Vlan_Rollback

***********************************************************************/

/**********************************************************************

    caller:     owner of this object

    prototype:

        ULONG
        Vlan_GetEntryCount
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to retrieve the count of the table.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The count of the table

**********************************************************************/

ULONG
Vlan_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PDATAMODEL_VLAN             pVLAN         = (PDATAMODEL_VLAN)g_pBEManager->hVLAN;

    return AnscSListQueryDepth( &pVLAN->Q_VlanList );

}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ANSC_HANDLE
        Vlan_GetEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG                       nIndex,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to retrieve the entry specified by the index.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG                       nIndex,
                The index of this entry;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle to identify the entry

**********************************************************************/

ANSC_HANDLE
Vlan_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PDATAMODEL_VLAN             pMyObject         = (PDATAMODEL_VLAN)g_pBEManager->hVLAN;
    PSINGLE_LINK_ENTRY          pSListEntry       = NULL;
    PCONTEXT_LINK_OBJECT        pCxtLink          = NULL;

    pSListEntry       = AnscSListGetEntryByIndex(&pMyObject->Q_VlanList, nIndex);

    if ( pSListEntry )
    {
        pCxtLink      = ACCESS_CONTEXT_LINK_OBJECT(pSListEntry);
        *pInsNumber   = pCxtLink->InstanceNumber;
    }

    return (ANSC_HANDLE)pSListEntry;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ANSC_HANDLE
        Vlan_AddEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to add a new entry.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle of new added entry.

**********************************************************************/

ANSC_HANDLE
Vlan_AddEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG*                      pInsNumber
    )
{
    ANSC_STATUS                          returnStatus      = ANSC_STATUS_SUCCESS;
    PDATAMODEL_VLAN                      pVLAN             = (PDATAMODEL_VLAN)g_pBEManager->hVLAN;
    PDML_VLAN                            p_Vlan            = NULL;
    PCONTEXT_LINK_OBJECT                 pVlanCxtLink      = NULL;
    PSINGLE_LINK_ENTRY                   pSListEntry       = NULL;
    BOOL                                 bridgeMode;

    p_Vlan = (PDML_VLAN)AnscAllocateMemory(sizeof(DML_VLAN));

    if ( !p_Vlan )
    {
        return NULL;
    }

    pVlanCxtLink = (PCONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(CONTEXT_LINK_OBJECT));
    if ( !pVlanCxtLink )
    {
        goto EXIT;
    }

    /* now we have this link content */
    pVlanCxtLink->hContext = (ANSC_HANDLE)p_Vlan;
    pVlanCxtLink->bNew     = TRUE;

    /* Get InstanceNumber and Alias */
    VlanGenForTriggerEntry(NULL, p_Vlan);

    pVlanCxtLink->InstanceNumber = p_Vlan->InstanceNumber ;
    *pInsNumber                  = p_Vlan->InstanceNumber ;

    SListPushEntryByInsNum(&pVLAN->Q_VlanList, (PCONTEXT_LINK_OBJECT)pVlanCxtLink);
   
    return (ANSC_HANDLE)pVlanCxtLink;

EXIT:
    AnscFreeMemory(p_Vlan);

    return NULL;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ULONG
        Vlan_DelEntry
            (
                ANSC_HANDLE                 hInsContext,
                ANSC_HANDLE                 hInstance
            );

    description:

        This function is called to delete an exist entry.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ANSC_HANDLE                 hInstance
                The exist entry handle;

    return:     The status of the operation.

**********************************************************************/

ULONG
Vlan_DelEntry
    (
        ANSC_HANDLE                 hInsContext,
        ANSC_HANDLE                 hInstance
    )
{
    ANSC_STATUS                returnStatus      = ANSC_STATUS_SUCCESS;
    PDATAMODEL_VLAN            pVLAN             = (PDATAMODEL_VLAN)g_pBEManager->hVLAN;
    PCONTEXT_LINK_OBJECT       pVlanCxtLink      = (PCONTEXT_LINK_OBJECT)hInstance;
    PDML_VLAN                  p_Vlan            = (PDML_VLAN)pVlanCxtLink->hContext;


    if ( pVlanCxtLink->bNew )
    {
        /* Set bNew to FALSE to indicate this node is not going to save to SysRegistry */
        pVlanCxtLink->bNew = FALSE;
    }
    if ( AnscSListPopEntryByLink(&pVLAN->Q_VlanList, &pVlanCxtLink->Linkage) )
    {
        AnscFreeMemory(pVlanCxtLink->hContext);
        AnscFreeMemory(pVlanCxtLink);
    }

    return returnStatus;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        BOOL
        Vlan_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value;

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/

BOOL
Vlan_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    PCONTEXT_LINK_OBJECT       pCxtLink      = (PCONTEXT_LINK_OBJECT)hInsContext;
    PDML_VLAN                  p_Vlan        = (PDML_VLAN   )pCxtLink->hContext;

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        *pBool = p_Vlan->Enable;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}



/**********************************************************************

    caller:     owner of this object

    prototype:

        BOOL
        Vlan_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value;

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Vlan_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    PCONTEXT_LINK_OBJECT   pCxtLink  = (PCONTEXT_LINK_OBJECT)hInsContext;
    PDML_VLAN              p_Vlan    = (PDML_VLAN   )pCxtLink->hContext;

    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}
/**********************************************************************

    caller:     owner of this object

    prototype:

        BOOL
        Vlan_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value;

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/

BOOL
Vlan_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCONTEXT_LINK_OBJECT   pCxtLink  = (PCONTEXT_LINK_OBJECT)hInsContext;
    PDML_VLAN              p_Vlan    = (PDML_VLAN   )pCxtLink->hContext;

    /* check the parameter name and return the corresponding value */

    if( AnscEqualString(ParamName, "Status", TRUE))
    {
        if(ANSC_STATUS_SUCCESS == DmlGetVlanIfStatus(NULL, p_Vlan)) {
            *puLong = p_Vlan->Status;
        }
        return TRUE;
    }
    if( AnscEqualString(ParamName, "LastChange", TRUE))
    {
        *puLong = p_Vlan->LastChange;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "VLANID", TRUE))
    {
        *puLong = p_Vlan->VLANId;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "TPID", TRUE))
    {
        *puLong = p_Vlan->TPId;
        return TRUE;
    }
    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ULONG
        Vlan_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value;

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/

ULONG
Vlan_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCONTEXT_LINK_OBJECT  pCxtLink    = (PCONTEXT_LINK_OBJECT)hInsContext;
    PDML_VLAN             p_Vlan      = (PDML_VLAN   )pCxtLink->hContext;
    PUCHAR                pString       = NULL;

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Alias", TRUE))
    {
        if ( AnscSizeOfString(p_Vlan->Alias) < *pUlSize)
        {
            AnscCopyString(pValue, p_Vlan->Alias);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(p_Vlan->Alias)+1;
            return 1;
        }
    }
    if( AnscEqualString(ParamName, "Name", TRUE))
    {
        AnscCopyString(pValue, p_Vlan->Name);
        return 0;
    }
    if( AnscEqualString(ParamName, "LowerLayers", TRUE))
    {
        AnscCopyString(pValue, p_Vlan->LowerLayers);
        return 0;
    }
    if( AnscEqualString(ParamName, "X_RDK_BaseInterface", TRUE))
    {
        AnscCopyString(pValue, p_Vlan->BaseInterface);
        return 0;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        BOOL
        Vlan_SetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL                        bValue
            );

    description:

        This function is called to set BOOL parameter value;

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL                        bValue
                The updated BOOL value;

    return:     TRUE if succeeded.

**********************************************************************/

BOOL
Vlan_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    PCONTEXT_LINK_OBJECT       pCxtLink  = (PCONTEXT_LINK_OBJECT)hInsContext;
    PDML_VLAN                  p_Vlan    = (PDML_VLAN) pCxtLink->hContext;

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* save update to backup */
        p_Vlan->Enable     = bValue;
        if(!p_Vlan->Enable)
        {
            DmlDeleteVlanInterface(NULL, p_Vlan);
        }
        else
        {
            DmlCreateVlanInterface(NULL, p_Vlan);
        }
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}


/**********************************************************************

    caller:     owner of this object

    prototype:

        BOOL
        Vlan_SetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int                         iValue
            );

    description:

        This function is called to set integer parameter value;

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int                         iValue
                The updated integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Vlan_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    /* check the parameter name and set the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        BOOL
        Vlan_SetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG                       uValue
            );

    description:

        This function is called to set ULONG parameter value;

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG                       uValue
                The updated ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/

BOOL
Vlan_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
    PCONTEXT_LINK_OBJECT       pCxtLink   = (PCONTEXT_LINK_OBJECT)hInsContext;
    PDML_VLAN                  p_Vlan     = (PDML_VLAN   )pCxtLink->hContext;

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "LastChange", TRUE))
    {
        p_Vlan->LastChange = uValue;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "VLANID", TRUE))
    {
        p_Vlan->VLANId = uValue;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "TPID", TRUE))
    {
        p_Vlan->TPId = uValue;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        BOOL
        Vlan_SetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pString
            );

    description:

        This function is called to set string parameter value;

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pString
                The updated string value;

    return:     TRUE if succeeded.

**********************************************************************/

BOOL
Vlan_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PDATAMODEL_VLAN            pVLAN          = (PDATAMODEL_VLAN      )g_pBEManager->hVLAN;
    PCONTEXT_LINK_OBJECT       pCxtLink       = (PCONTEXT_LINK_OBJECT)hInsContext;
    PDML_VLAN                  p_Vlan         = (PDML_VLAN   )pCxtLink->hContext;


    /* check the parameter name and set the corresponding value */
   
    if( AnscEqualString(ParamName, "Alias", TRUE))
    {
        AnscCopyString(p_Vlan->Alias, pString);
        return TRUE;
    }
    if( AnscEqualString(ParamName, "Name", TRUE))
    {
        AnscCopyString(p_Vlan->Name, pString);
        return TRUE;
    }
    if( AnscEqualString(ParamName, "LowerLayers", TRUE))
    {
        AnscCopyString(p_Vlan->LowerLayers, pString);
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_RDK_BaseInterface", TRUE))
    {
        AnscCopyString(p_Vlan->BaseInterface, pString);
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        BOOL
        Vlan_Validate
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pReturnParamName,
                ULONG*                      puLength
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pReturnParamName,
                The buffer (128 bytes) of parameter name if there's a validation.

                ULONG*                      puLength
                The output length of the param name.

    return:     TRUE if there's no validation.

**********************************************************************/

BOOL
Vlan_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    return TRUE;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ULONG
        Vlan_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/

ULONG
Vlan_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    ANSC_STATUS                returnStatus  = ANSC_STATUS_SUCCESS;
    PCONTEXT_LINK_OBJECT       pCxtLink      = (PCONTEXT_LINK_OBJECT)hInsContext;
    PDML_VLAN                  p_Vlan        = (PDML_VLAN   )pCxtLink->hContext;
    PDATAMODEL_VLAN            pVLAN         = (PDATAMODEL_VLAN      )g_pBEManager->hVLAN;

    if ( pCxtLink->bNew )
    {
        returnStatus = DmlAddVlan(NULL, p_Vlan );

        if ( returnStatus == ANSC_STATUS_SUCCESS )
        {
            pCxtLink->bNew = FALSE;
        }
        else
        {
            DML_VLAN_INIT(p_Vlan);
            _ansc_sprintf(p_Vlan->Name, "vlan%d", p_Vlan->InstanceNumber);
            if ( p_Vlan->Alias )
                AnscCopyString( p_Vlan->Alias, p_Vlan->Alias );
        }
    }
    else {
        if (p_Vlan->Enable)
        {
            returnStatus = DmlSetVlan(NULL, p_Vlan);
            if ( returnStatus != ANSC_STATUS_SUCCESS) {
                if( ANSC_STATUS_SUCCESS != DmlGetVlan(NULL, p_Vlan->InstanceNumber, p_Vlan)){
                    DML_VLAN_INIT(p_Vlan);
                    _ansc_sprintf(p_Vlan->Name, "vlan%d", p_Vlan->InstanceNumber);
                }
            }
        }
        else
        {
            /* If interface doesn't exists , return ANSC_STATUS_SUCCESS. */
            returnStatus = DmlDeleteVlanInterface(NULL, p_Vlan);
            if (returnStatus != ANSC_STATUS_SUCCESS)
            {
                CcspTraceError(("[%s][%d]Failed to delete vlan interface\n", __FUNCTION__, __LINE__));
            }
        }
    }

    return returnStatus;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ULONG
        Vlan_Rollback
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to roll back the update whenever there's a
        validation found.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/

ULONG
Vlan_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    ANSC_STATUS                returnStatus  = ANSC_STATUS_SUCCESS;
    PDATAMODEL_VLAN            pVLAN         = (PDATAMODEL_VLAN      )g_pBEManager->hVLAN;
    PCONTEXT_LINK_OBJECT       pCxtLink      = (PCONTEXT_LINK_OBJECT)hInsContext;
    PDML_VLAN                  p_Vlan        = (PDML_VLAN   )pCxtLink->hContext;

    if ( p_Vlan->Alias )
        AnscCopyString( p_Vlan->Alias, p_Vlan->Alias );

    if ( !pCxtLink->bNew )
    {
        /* We have nothing to do with this case unless we have one getbyEntry() */
    }
    else
    {
        DML_VLAN_INIT(p_Vlan);
        _ansc_sprintf(p_Vlan->Name, "vlan%d", p_Vlan->InstanceNumber);
    }

    return returnStatus;
}
