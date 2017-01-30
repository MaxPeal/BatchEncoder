﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "StdAfx.h"
#include "BatchEncoder.h"
#include "BatchEncoderWorkerContext.h"

void CBatchEncoderWorkerContext::Init()
{
    this->timer.Start();

    pDlg->m_Progress.SetPos(0);
}

void CBatchEncoderWorkerContext::Next(int nItemId)
{
    this->nProcessedFiles++;
    this->nErrors = (this->nProcessedFiles - 1) - this->nDoneWithoutError;

    if (this->nThreadCount == 1)
    {
        CString szText;
        szText.Format(_T("Processing item %d of %d (%d Done, %d %s)"),
            this->nProcessedFiles,
            this->nTotalFiles,
            this->nDoneWithoutError,
            this->nErrors,
            ((this->nErrors == 0) || (this->nErrors > 1)) ? _T("Errors") : _T("Error"));
        pDlg->m_StatusBar.SetText(szText, 1, 0);

        this->nLastItemId = nItemId;
        pDlg->m_LstInputItems.EnsureVisible(nItemId, FALSE);
    }
}

void CBatchEncoderWorkerContext::Done()
{
    this->timer.Stop();
    this->nErrors = this->nProcessedFiles - this->nDoneWithoutError;

    CString szText;
    szText.Format(_T("Converted %d of %d (%d Done, %d %s) in %s"),
        this->nProcessedFiles,
        this->nTotalFiles,
        this->nDoneWithoutError,
        this->nErrors,
        ((this->nErrors == 0) || (this->nErrors > 1)) ? _T("Errors") : _T("Error"),
        ::FormatTime(this->timer.ElapsedTime(), 3));
    pDlg->m_StatusBar.SetText(szText, 1, 0);

    pDlg->FinishConvert();
}

bool CBatchEncoderWorkerContext::Callback(int nItemId, int nProgress, bool bFinished, bool bError)
{
    if (bError == true)
    {
        if (pDlg->pWorkerContext->pConfig->m_Options.bStopOnErrors == true)
        {
            pDlg->m_Progress.SetPos(0);
            this->bRunning = false;
        }

        return this->bRunning;
    }

    if (bFinished == false)
    {
        CString szProgress;
        szProgress.Format(_T("%d%%\0"), nProgress);
        pDlg->m_LstInputItems.SetItemText(nItemId, ITEM_COLUMN_STATUS, szProgress); // Status

        pDlg->pWorkerContext->nProgess[nItemId] = nProgress;

        static volatile bool bSafeCheck = false;
        if (bSafeCheck == false)
        {
            bSafeCheck = true;

            if (nItemId > this->nLastItemId)
            {
                this->nLastItemId = nItemId;
                pDlg->m_LstInputItems.EnsureVisible(nItemId, FALSE);
            }

            int nTotalProgress = 0;
            int nItems = pDlg->pWorkerContext->pConfig->m_Items.GetSize();
            for (int i = 0; i < nItems; i++)
            {
                if (pDlg->pWorkerContext->pConfig->m_Items.GetData(i).bChecked == TRUE)
                    nTotalProgress += pDlg->pWorkerContext->nProgess[i];
            }

            int nPos = nTotalProgress / pDlg->pWorkerContext->nTotalFiles;
            if (pDlg->m_Progress.GetPos() != nPos)
            {
                pDlg->m_Progress.SetPos(nPos);
            }

            bSafeCheck = false;
        }
    }

    return this->bRunning;
}

void CBatchEncoderWorkerContext::Status(int nItemId, CString szTime, CString szStatus)
{
    pDlg->m_LstInputItems.SetItemText(nItemId, ITEM_COLUMN_TIME, szTime); // Time
    pDlg->m_LstInputItems.SetItemText(nItemId, ITEM_COLUMN_STATUS, szStatus); // Status
};