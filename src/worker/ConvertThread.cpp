﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "StdAfx.h"
#include "..\BatchEncoder.h"
#include "..\dialogs\BatchEncoderDlg.h"
#include "WorkThread.h"

DWORD WINAPI ConvertThread(LPVOID lpParam)
{
    CObList* pQueue = (CObList *)lpParam;
    while (!pQueue->IsEmpty())
    {
        ItemContext* pContext = (ItemContext*)pQueue->RemoveHead();
        bool bResult = ConvertItem(pContext);

        if (bResult = false && pContext->pDlg->m_Config.m_Options.bStopOnErrors == true)
        {
            delete pContext;
            return(1);
        }

        if (pContext->pDlg->bRunning == false)
        {
            delete pContext;
            return(1);
        }

        delete pContext;
    }
    return(0);
}
