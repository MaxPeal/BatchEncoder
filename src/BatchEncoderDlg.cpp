﻿//
// BatchEncoder (Audio Conversion GUI)
// Copyright (C) 2005-2017 Wiesław Šoltés <wieslaw.soltes@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "StdAfx.h"
#include "BatchEncoder.h"
#include "Utilities.h"
#include "UnicodeUtf8.h"
#include "Utf8String.h"
#include "XmlResource.h"
#include "BatchEncoderDlg.h"
#include "PresetsDlg.h"
#include "AboutDlg.h"
#include "FormatsDlg.h"
#include "CopyFileDlg.h"
#include "AdvancedDlg.h"

#include "WorkThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

typedef struct TDRAGANDDROP
{
    CBatchEncoderDlg *pDlg;
    HDROP hDropInfo;
} DRAGANDDROP, *PDRAGANDDROP;

static volatile bool bHandleDrop = true;
static HANDLE hDDThread;
static DWORD dwDDThreadID;
static DRAGANDDROP m_DDParam;

#define IDC_FOLDERTREE          0x3741
#define IDC_TITLE               0x3742
#define IDC_STATUSTEXT          0x3743
#define IDC_CHECK_RECURSE       0x3744
#define IDC_BROWSE_NEW_FOLDER   0x3746

static CString szLastBrowse;
static CString szLastBrowseAddDir;
static WNDPROC lpOldWindowProc;
static bool bRecurseChecked = true;
static HWND hWndBtnRecurse = NULL;
static HWND hWndStaticText = NULL;

int CALLBACK BrowseCallbackProc(HWND hWnd, UINT uMsg, LPARAM lp, LPARAM pData) 
{
    TCHAR szPath[MAX_PATH + 1] = _T("");
    wsprintf(szPath, _T("%s\0"), szLastBrowse);

    if((uMsg == BFFM_INITIALIZED))
        ::SendMessage(hWnd, BFFM_SETSELECTION, TRUE, (LPARAM) szPath);

    return(0);
}

LRESULT CALLBACK BrowseDlgWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == WM_COMMAND)
    {
        if((HIWORD(wParam) == BN_CLICKED) && ((HWND) lParam == hWndBtnRecurse))
        {
            if(::SendMessage(hWndBtnRecurse, BM_GETCHECK, (WPARAM) 0, (LPARAM) 0) == BST_CHECKED)
                bRecurseChecked = true;
            else
                bRecurseChecked = false;
        }
    }
    return ::CallWindowProc(lpOldWindowProc, hWnd, uMsg, wParam, lParam);
}

int CALLBACK BrowseCallbackAddDir(HWND hWnd, UINT uMsg, LPARAM lp, LPARAM pData) 
{
    TCHAR szPath[MAX_PATH + 1] = _T("");
    wsprintf(szPath, _T("%s\0"), szLastBrowseAddDir);

    if(uMsg == BFFM_INITIALIZED)
    {
        HWND hWndTitle = NULL;
        HFONT hFont;
        RECT rc, rcTitle, rcTree, rcWnd;

        hWndTitle = ::GetDlgItem(hWnd, IDC_TITLE);

        ::GetWindowRect(hWndTitle, &rcTitle);
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_FOLDERTREE), &rcTree);
        ::GetWindowRect(hWnd, &rcWnd);

        rc.top = 8;
        rc.left = rcTree.left - rcWnd.left;
        rc.right = rcTree.right - rcTree.left;
        rc.bottom = (rcTitle.bottom - rcTitle.top) + 8;

        hWndBtnRecurse = ::CreateWindowEx(0, 
            _T("BUTTON"), 
            _T("Recurse subdirectories"),
            BS_CHECKBOX | BS_AUTOCHECKBOX | WS_CHILD | WS_TABSTOP | WS_VISIBLE,
            rc.left, rc.top, 
            rc.right-rc.left, rc.bottom-rc.top, 
            hWnd, 
            NULL, NULL, NULL);
        if(hWndBtnRecurse != NULL)
        {
            ::ShowWindow(hWndTitle, SW_HIDE);
            ::ShowWindow(::GetDlgItem(hWnd, IDC_STATUSTEXT), SW_HIDE);

            if(bRecurseChecked == true)
                ::SendMessage(hWndBtnRecurse, BM_SETCHECK, (WPARAM) BST_CHECKED, (LPARAM) 0);
            else
                ::SendMessage(hWndBtnRecurse, BM_SETCHECK, (WPARAM) BST_UNCHECKED, (LPARAM) 0);

            // disable warnings 4311 and 4312
            #pragma warning(push)
            #pragma warning(disable:4311)
            #pragma warning(disable:4312)

            lpOldWindowProc = (WNDPROC) SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) BrowseDlgWindowProc);
            ::ShowWindow(hWndBtnRecurse, SW_SHOW);

            // enable warnings 4311 and 4312
            #pragma warning(pop)

            hFont = (HFONT) ::SendMessage(hWnd, WM_GETFONT, 0, 0);
            ::SendMessage(hWndBtnRecurse, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));
        }

        ::SendMessage(hWnd, BFFM_SETSELECTION, TRUE, (LPARAM) szPath);
    }
    return(0);
}

int CALLBACK BrowseCallbackOutPath(HWND hWnd, UINT uMsg, LPARAM lp, LPARAM pData) 
{
    TCHAR szPath[MAX_PATH + 1] = _T("");
    wsprintf(szPath, _T("%s\0"), szLastBrowse);

    if(uMsg == BFFM_INITIALIZED)
    {
        TCHAR szText[256] = _T("");
        HWND hWndTitle = NULL;
        HFONT hFont;
        RECT rc, rcTitle, rcTree, rcWnd;

        hWndTitle = ::GetDlgItem(hWnd, IDC_TITLE);

        ::GetWindowText(hWndTitle, szText, 256);

        ::GetWindowRect(hWndTitle, &rcTitle);
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_FOLDERTREE), &rcTree);
        ::GetWindowRect(hWnd, &rcWnd);

        rc.top = 8;
        rc.left = rcTree.left - rcWnd.left;
        rc.right = rcTree.right - rcTree.left;
        rc.bottom = (rcTitle.bottom - rcTitle.top) + 8;

        hWndStaticText = ::CreateWindowEx(0, 
            _T("STATIC"), 
            szText,
            SS_CENTERIMAGE | WS_CHILD | WS_TABSTOP | WS_VISIBLE,
            rc.left, rc.top, 
            rc.right-rc.left, rc.bottom-rc.top, 
            hWnd, 
            NULL, NULL, NULL);
        if(hWndStaticText != NULL)
        {
            ::ShowWindow(hWndTitle, SW_HIDE);
            ::ShowWindow(::GetDlgItem(hWnd, IDC_STATUSTEXT), SW_HIDE);

            // disable warnings 4311 and 4312
            #pragma warning(push)
            #pragma warning(disable:4311)
            #pragma warning(disable:4312)

            lpOldWindowProc = (WNDPROC) SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) BrowseDlgWindowProc);
            ::ShowWindow(hWndStaticText, SW_SHOW);

            // enable warnings 4311 and 4312
            #pragma warning(pop)

            hFont = (HFONT) ::SendMessage(hWnd, WM_GETFONT, 0, 0);
            ::SendMessage(hWndStaticText, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));
        }

        ::SendMessage(hWnd, BFFM_SETSELECTION, TRUE, (LPARAM) szPath);
    }
    return(0);
}

DWORD WINAPI DragAndDropThread(LPVOID lpParam)
{
    PDRAGANDDROP m_ThreadParam = (PDRAGANDDROP) lpParam;

    m_ThreadParam->pDlg->HandleDropFiles(m_ThreadParam->hDropInfo);
    bHandleDrop = true;

    return ::CloseHandle(hDDThread);
}

IMPLEMENT_DYNAMIC(CBatchEncoderDlg, CDialog)
CBatchEncoderDlg::CBatchEncoderDlg(CWnd* pParent /*=NULL*/)
: CResizeDialog(CBatchEncoderDlg::IDD, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    this->m_pFo = NULL;

    szMainConfigFile = ::GetExeFilePath() + MAIN_APP_CONFIG;
    bShowTrayIcon = false;

    szPresetsWndResize = _T("");
    szPresetsListColumns = _T("");
    szFormatsWndResize = _T("");
    szFormatsListColumns = _T("");

    for(int i = 0; i < NUM_COMMADLINE_TOOLS; i++)
    {
        szFormatPath[i] = g_szDefaultPath[i];
        bFormatInput[i] = g_bDefaultInPipes[i];
        bFormatOutput[i] = g_bDefaultOutPipes[i];
    }

    nThreadPriorityIndex = 3;
    nProcessPriorityIndex = 1;
    bDeleteOnError = true;
    bStopOnErrors = false;
    szLogFileName = MAIN_APP_LOG;
    nLogEncoding = 2;

    bForceConsoleWindow = false;

    bIsHistogramVisible = false;
    bIsCnvStatusVisible = false;
}

CBatchEncoderDlg::~CBatchEncoderDlg()
{

}

void CBatchEncoderDlg::DoDataExchange(CDataExchange* pDX)
{
    CResizeDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PROGRESS_WORK, m_FileProgress);
    DDX_Control(pDX, IDC_COMBO_PRESETS, m_CmbPresets);
    DDX_Control(pDX, IDC_COMBO_FORMAT, m_CmbFormat);
    DDX_Control(pDX, IDC_EDIT_INPUT_FILES, m_LstInputFiles);
    DDX_Control(pDX, IDC_CHECK_OUT_PATH, m_ChkOutPath);
    DDX_Control(pDX, IDC_EDIT_OUT_PATH, m_EdtOutPath);
    DDX_Control(pDX, IDC_STATIC_TEXT_PRESET, m_StcPreset);
    DDX_Control(pDX, IDC_STATIC_TEXT_FORMAT, m_StcFormat);
    DDX_Control(pDX, IDC_BUTTON_RUN, m_BtnConvert);
    DDX_Control(pDX, IDC_BUTTON_BROWSE_PATH, m_BtnBrowse);
}

BEGIN_MESSAGE_MAP(CBatchEncoderDlg, CResizeDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    //}}AFX_MSG_MAP
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_DROPFILES()
    ON_WM_HELPINFO()
    ON_WM_SIZE()
    ON_MESSAGE(WM_TRAY, OnTrayIconMsg) 
    ON_MESSAGE(WM_ITEMCHANGED, OnListItemChaged) 
    ON_COMMAND(ID_TRAYMENU_EXIT, OnTrayMenuExit)
    ON_NOTIFY(LVN_KEYDOWN, IDC_EDIT_INPUT_FILES, OnLvnKeydownListInputFiles)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_EDIT_INPUT_FILES, OnLvnItemchangedListInputFiles)
    ON_NOTIFY(NM_RCLICK, IDC_EDIT_INPUT_FILES, OnNMRclickListInputFiles)
    ON_NOTIFY(NM_DBLCLK, IDC_EDIT_INPUT_FILES, OnNMDblclkListInputFiles)
    ON_CBN_SELCHANGE(IDC_COMBO_PRESETS, OnCbnSelchangeComboPresets)
    ON_CBN_SELCHANGE(IDC_COMBO_FORMAT, OnCbnSelchangeComboFormat)
    ON_BN_CLICKED(IDC_BUTTON_RUN, OnBnClickedButtonConvert)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_PATH, OnBnClickedButtonBrowsePath)
    ON_BN_CLICKED(IDC_CHECK_OUT_PATH, OnBnClickedCheckOutPath)
    ON_EN_CHANGE(IDC_EDIT_OUT_PATH, OnEnChangeEditOutPath)
    ON_EN_SETFOCUS(IDC_EDIT_OUT_PATH, OnEnSetFocusEditOutPath)
    ON_EN_KILLFOCUS(IDC_EDIT_OUT_PATH, OnEnKillFocusEditOutPath)
    ON_MESSAGE(WM_NOTIFYFORMAT, OnNotifyFormat)
    ON_COMMAND(ID_FILE_LOADLIST, OnFileLoadList)
    ON_COMMAND(ID_FILE_SAVELIST, OnFileSaveList)
    ON_COMMAND(ID_FILE_CLEARLIST, OnFileClearList)
    ON_COMMAND(ID_FILE_CREATE_BATCH_FILE, OnFileCreateBatchFile)
    ON_COMMAND(ID_FILE_EXIT, OnFileExit)
    ON_COMMAND(ID_EDIT_ADDFILES, OnEditAddFiles)
    ON_COMMAND(ID_EDIT_REMOVECHECKED, OnEditRemoveChecked)
    ON_COMMAND(ID_EDIT_REMOVEUNCHECKED, OnEditRemoveUnchecked)
    ON_COMMAND(ID_EDIT_CHECKSELECTED, OnEditCheckSelected)
    ON_COMMAND(ID_EDIT_UNCHECKSELECTED, OnEditUncheckSelected)
    ON_COMMAND(ID_EDIT_RENAME, OnEditRename)
    ON_COMMAND(ID_EDIT_OPEN, OnEditOpen)
    ON_COMMAND(ID_EDIT_EXPLORE, OnEditExplore)
    ON_COMMAND(ID_EDIT_CROP, OnEditCrop)
    ON_COMMAND(ID_EDIT_SELECTNONE, OnEditSelectNone)
    ON_COMMAND(ID_EDIT_INVERTSELECTION, OnEditInvertSelection)
    ON_COMMAND(ID_EDIT_REMOVE, OnEditRemove)
    ON_COMMAND(ID_EDIT_SELECTALL, OnEditSelectAll)
    ON_COMMAND(ID_EDIT_RESETOUTPUT, OnEditResetOutput)
    ON_COMMAND(ID_EDIT_RESETTIME, OnEditResetTime)
    ON_COMMAND(ID_EDIT_ADDDIR, OnEditAddDir)
    ON_COMMAND(ID_VIEW_STARTWITHEXTENDEDPROGRESS, OnViewStartWithExtendedProgress)
    ON_COMMAND(ID_VIEW_TOOGLEEXTENDEDPROGRESS, OnViewToogleExtendedProgress)
    ON_COMMAND(ID_VIEW_TOOGLEHISTOGRAMWINDOW, OnViewToogleHistogramWindow)
    ON_COMMAND(ID_VIEW_SHOWGRIDLINES, OnViewShowGridLines)
    ON_COMMAND(ID_ACTION_CONVERT, OnActionConvert)
    ON_COMMAND(ID_OPTIONS_CONFIGUREPRESETS, OnOptionsConfigurePresets)
    ON_COMMAND(ID_OPTIONS_CONFIGUREFORMAT, OnOptionsConfigureFormat)
    ON_COMMAND(ID_OPTIONS_SHUTDOWN_WHEN_FINISHED, OnOptionsShutdownWhenFinished)
    ON_COMMAND(ID_OPTIONS_STAYONTOP, OnOptionsStayOnTop)
    ON_COMMAND(ID_OPTIONS_SHOWTRAYICON, OnOptionsShowTrayIcon)
    ON_COMMAND(ID_OPTIONS_DELETESOURCEFILEWHENDONE, OnOptionsDeleteSourceFileWhenDone)
    ON_COMMAND(ID_OPTIONS_SHOWLOGLIST, OnOptionsShowLog)
    ON_COMMAND(ID_OPTIONS_DELETELOG, OnOptionsDeleteLog)
    ON_COMMAND(ID_OPTIONS_LOGCONSOLEOUTPUT, OnOptionsLogConsoleOutput)
    ON_COMMAND(ID_OPTIONS_DO_NOT_SAVE, OnOptionsDoNotSave)
    ON_COMMAND(ID_OPTIONS_FORCECONSOLEWINDOW, OnOptionsForceConsoleWindow)
    ON_COMMAND(ID_OPTIONS_ADVANCED, OnOptionsAdvanced)
    ON_COMMAND(ID_HELP_WEBSITE, OnHelpWebsite)
    ON_COMMAND(ID_HELP_ABOUT, OnHelpAbout)
    ON_COMMAND(ID_ACCELERATOR_CTRL_L, OnFileLoadList)
    ON_COMMAND(ID_ACCELERATOR_CTRL_S, OnFileSaveList)
    ON_COMMAND(ID_ACCELERATOR_CTRL_E, OnFileClearList)
    ON_COMMAND(ID_ACCELERATOR_CTRL_B, OnFileCreateBatchFile)
    ON_COMMAND(ID_ACCELERATOR_CTRL_SHIFT_L, LoadUserSettings)
    ON_COMMAND(ID_ACCELERATOR_CTRL_SHIFT_S, SaveUserSettings)
    ON_COMMAND(ID_ACCELERATOR_CTRL_SHIFT_D, LoadDefaultSettings)
    ON_COMMAND(ID_ACCELERATOR_F3, OnEditResetTime)
    ON_COMMAND(ID_ACCELERATOR_F4, OnEditResetOutput)
    ON_COMMAND(ID_ACCELERATOR_ALT_F4, OnFileExit)
    ON_COMMAND(ID_ACCELERATOR_F5, OnEditAddFiles)
    ON_COMMAND(ID_ACCELERATOR_F6, OnEditAddDir)
    ON_COMMAND(ID_ACCELERATOR_F2, OnEditRename)
    ON_COMMAND(ID_ACCELERATOR_CTRL_A, OnEditSelectAll)
    ON_COMMAND(ID_ACCELERATOR_CTRL_N, OnEditSelectNone)
    ON_COMMAND(ID_ACCELERATOR_CTRL_I, OnEditInvertSelection)
    ON_COMMAND(ID_ACCELERATOR_CTRL_O, OnEditOpen)
    ON_COMMAND(ID_ACCELERATOR_CTRL_SHIFT_O, OnEditExplore)
    ON_COMMAND(ID_ACCELERATOR_SHIFT_PLUS, OnEditCheckSelected)
    ON_COMMAND(ID_ACCELERATOR_SHIFT_MINUS, OnEditUncheckSelected)
    ON_COMMAND(ID_ACCELERATOR_CTRL_PLUS, OnEditRemoveChecked)
    ON_COMMAND(ID_ACCELERATOR_CTRL_MINUS, OnEditRemoveUnchecked)
    ON_COMMAND(ID_ACCELERATOR_CTRL_G, OnViewShowGridLines)
    ON_COMMAND(ID_ACCELERATOR_CTRL_SHIFT_P, OnViewStartWithExtendedProgress)
    ON_COMMAND(ID_ACCELERATOR_F9, OnBnClickedButtonConvert)
    ON_COMMAND(ID_ACCELERATOR_F10, OnOptionsStayOnTop)
    ON_COMMAND(ID_ACCELERATOR_CTRL_X, OnOptionsDoNotSave)
    ON_COMMAND(ID_ACCELERATOR_F7, OnOptionsConfigurePresets)
    ON_COMMAND(ID_ACCELERATOR_F8, OnOptionsConfigureFormat)
    ON_COMMAND(ID_ACCELERATOR_CTRL_Q, OnOptionsShutdownWhenFinished)
    ON_COMMAND(ID_ACCELERATOR_F12, OnOptionsLogConsoleOutput)
    ON_COMMAND(ID_ACCELERATOR_CTRL_F12, OnOptionsShowLog)
    ON_COMMAND(ID_ACCELERATOR_SHIFT_F12, OnOptionsDeleteLog)
    ON_COMMAND(ID_ACCELERATOR_F11, OnOptionsShowTrayIcon)
    ON_COMMAND(ID_ACCELERATOR_CTRL_SHIFT_F, OnOptionsForceConsoleWindow)
    ON_COMMAND(ID_ACCELERATOR_CTRL_SHIFT_A, OnOptionsAdvanced)
    ON_COMMAND(ID_ACCELERATOR_CTRL_D, OnOptionsDeleteSourceFileWhenDone)
    ON_COMMAND(ID_ACCELERATOR_CTRL_H, OnShowHistogram)
    ON_COMMAND(ID_ACCELERATOR_CTRL_P, OnShowCnvStatus)
    ON_WM_NCLBUTTONDOWN()
    //ON_WM_NCHITTEST()
END_MESSAGE_MAP()

BOOL CBatchEncoderDlg::OnInitDialog()
{
    CResizeDialog::OnInitDialog();

    InitCommonControls();

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    // create statusbar control
    VERIFY(m_StatusBar.Create(WS_CHILD | WS_VISIBLE | CCS_BOTTOM | SBARS_SIZEGRIP,
        CRect(0, 0, 0, 0), 
        this, 
        IDC_STATUSBAR));

    int nStatusBarParts[2] = { 100, -1 };
    m_StatusBar.SetParts(2, nStatusBarParts);

    // load accelerators
    m_hAccel = ::LoadAccelerators(::GetModuleHandle(NULL), 
        MAKEINTRESOURCE(IDR_ACCELERATOR_BATCHENCODER));

    if(bShowTrayIcon == true)
        this->EnableTrayIcon(true);

    // required when using UnicoWS.dll under Win9x with UNICODE
    // also handle the OnNotifyFormat message (WM_NOTIFYFORMAT)
#ifdef _UNICODE
    m_LstInputFiles.SendMessage(CCM_SETUNICODEFORMAT, (WPARAM) (BOOL) TRUE, 0);
#endif

    // enable or disable '<< same as source file >>' option
    bSameAsSourceEdit = true;

    // main dialog title with version number
    this->SetWindowText(MAIN_APP_NAME);

    // output file format combobox 
    for(int i = 0; i < NUM_PRESET_FILES; i++)
        m_CmbFormat.InsertString(i, g_szPresetNames[i]);

    ::SetComboBoxHeight(this->GetSafeHwnd(), IDC_COMBO_FORMAT);

    // set flag to non running state
    bRunning = false;

    // clear progress status
    m_FileProgress.SetRange(0, 100);
    m_FileProgress.SetPos(0);

    // update list style
    DWORD dwExStyle = m_LstInputFiles.GetExtendedStyle();
    dwExStyle |= LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
    m_LstInputFiles.SetExtendedStyle(dwExStyle);

    // insert ListCtrl columns
    m_LstInputFiles.InsertColumn(0, _T("Name"), LVCFMT_LEFT, 165);
    m_LstInputFiles.InsertColumn(1, _T("Type"), LVCFMT_LEFT, 50);
    m_LstInputFiles.InsertColumn(2, _T("Size (bytes)"), LVCFMT_LEFT, 80);
    m_LstInputFiles.InsertColumn(3, _T("Output"), LVCFMT_LEFT, 50);
    m_LstInputFiles.InsertColumn(4, _T("Preset#"), LVCFMT_LEFT, 55);
    m_LstInputFiles.InsertColumn(5, _T("Time"), LVCFMT_LEFT, 90);
    m_LstInputFiles.InsertColumn(6, _T("Status"), LVCFMT_LEFT, 85);

    // set ListCtrl columns order
    /*
    INT nOrder[7] = { 0, 1, 2, 3, 4, 5, 6 };

    this->m_LstInputFiles.GetHeaderCtrl()->SetOrderArray(7, nOrder);
    this->m_LstInputFiles.Invalidate();
    */

    // set to bold Convert/Stop button font style
    m_BtnConvert.SetBold(true);

    // hide ProgressBar when not running conversion process
    this->m_FileProgress.ShowWindow(SW_HIDE);

    // disable window toogle items
    this->GetMenu()->EnableMenuItem(ID_VIEW_TOOGLEEXTENDEDPROGRESS, MF_GRAYED);
    this->GetMenu()->EnableMenuItem(ID_VIEW_TOOGLEHISTOGRAMWINDOW, MF_GRAYED);

    // enable files/dirs drag & drop for dialog
    this->DragAcceptFiles(TRUE);

    // create histogram control
    CRect rcHistogram;

    GetDlgItem(IDC_EDIT_INPUT_FILES)->GetWindowRect(rcHistogram);
    ScreenToClient(rcHistogram);
    VERIFY(m_Histogram.Create(rcHistogram, this, IDC_HISTOGRAM, false));

    this->m_Histogram.Init(false);
    this->m_Histogram.Erase(true);

    // create conversion status control
    CRect rcCnvStatus;

    GetDlgItem(IDC_EDIT_INPUT_FILES)->GetWindowRect(rcCnvStatus);
    ScreenToClient(rcCnvStatus);
    VERIFY(m_CnvStatus.Create(rcCnvStatus, this, IDC_CNVSTATUS, false));

    this->m_CnvStatus.Init();
    this->m_CnvStatus.Erase(true);

	// setup resize anchors
    AddAnchor(IDC_STATIC_GROUP_OUTPUT, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_STATIC_TEXT_FORMAT, TOP_LEFT);
    AddAnchor(IDC_COMBO_FORMAT, TOP_LEFT);
    AddAnchor(IDC_STATIC_TEXT_PRESET, TOP_LEFT);
    AddAnchor(IDC_COMBO_PRESETS, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_EDIT_INPUT_FILES, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_CHECK_OUT_PATH, BOTTOM_LEFT);
    AddAnchor(IDC_EDIT_OUT_PATH, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON_BROWSE_PATH, BOTTOM_RIGHT);
    AddAnchor(IDC_PROGRESS_WORK, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON_RUN, BOTTOM_RIGHT);
    AddAnchor(IDC_STATUSBAR, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_HISTOGRAM, MIDDLE_CENTER); // TOP_LEFT, BOTTOM_RIGHT
    AddAnchor(IDC_CNVSTATUS, MIDDLE_CENTER); // TOP_LEFT, BOTTOM_RIGHT

    // load program settings from file
    if(this->LoadSettings() == false)
    {
        // when settings file is missing we are exiting
        this->EndDialog(-1);
        return FALSE;
    }

    // update statusbar message text
    this->UpdateStatusBar();

    // clean options flags
    // ZeroMemory(this->m_pFo, sizeof(FBATCHENOCDER_OPTIONS));

    // run conversion automatically
    if(this->m_pFo->bHaveStartConversion == true)
        this->OnBnClickedButtonConvert();

    // ResetMinTrackSize();
    // ResetMaxTrackSize();
    // ResetMaximizedRect();

    // m_TransMove.Init(this->GetSafeHwnd());

    return TRUE;
}

BOOL CBatchEncoderDlg::PreTranslateMessage(MSG* pMsg)
{
    // translate here all accelerators, becose by default they are not translated
    if(m_hAccel != NULL)
    {
        if(::TranslateAccelerator(this->GetSafeHwnd(), m_hAccel, pMsg))
            return TRUE;
    }

    return CDialog::PreTranslateMessage(pMsg);
}

bool CBatchEncoderDlg::CreateBatchFile(CString szFileName, bool bUseListCtrl)
{
    // TODO: handle command-line options (params)

    // NOTE: this function use same logic as WorkThread(...)

    char *szPrefix = "@echo off\r\n@setlocal\r\n";
    char *szPostfix = "@endlocal\r\n@pause\r\n";
    char *szPreDel = "@del /F /Q \"";
    char *szPostDel = "\"\r\n";
    char *szShutdown = "@shutdown -t 30 -s\r\n";
    char *szLineEnd = "\r\n";

    ::UpdatePath();

    CFile fp;
    if(fp.Open(szFileName, fp.modeCreate | fp.modeReadWrite) == FALSE)
        return false;

    fp.Write(szPrefix, (UINT) strlen(szPrefix));

    bool bDeleteAfterConverion = false;
    if(bUseListCtrl == true)
    {
        if(this->GetMenu()->GetMenuState(ID_OPTIONS_DELETESOURCEFILEWHENDONE, MF_BYCOMMAND) == MF_CHECKED)
            bDeleteAfterConverion = true;
        else
            bDeleteAfterConverion = false;
    }
    else
    {
        // TODO:
    }

    CString szOutPath;
    bool bOutPath = false;

    if(bUseListCtrl == true)
    {
        this->m_EdtOutPath.GetWindowText(szOutPath);
        if(this->m_ChkOutPath.GetCheck() == BST_CHECKED)
            bOutPath = true;
        else
            bOutPath = false;
    }
    else
    {
        // TODO:
    } 

    int nFiles = 0;
    if(bUseListCtrl == true)
        nFiles = this->m_LstInputFiles.GetItemCount();
    else
        nFiles = this->m_FileList.GetSize();

    for(int i = 0; i < nFiles; i++)
    {
        if(bUseListCtrl == true)
        {
            if(this->m_LstInputFiles.GetCheck(i) == FALSE)
                continue;
        }
        else
        {
            // TODO:
        }

        int nProcessingMode = -1;
        int nIntputFormat = this->m_FileList.GetItemInFormat(i);
        int nOutputFormat = this->m_FileList.GetItemOutFormat(i);
        int nPreset = this->m_FileList.GetItemOutPreset(i);

        CString szInputFile = this->m_FileList.GetItemFilePath(i);

        if(bOutPath == false)
        {
            szOutPath = szInputFile;
            CString szToRemove = this->m_FileList.GetFileName(szInputFile);
            int nNewLenght = szOutPath.GetLength() - szToRemove.GetLength();
            szOutPath.Truncate(nNewLenght);
        }

        CString szDecoderExePath;
        CString szDecoderOptions;

        if(bUseListCtrl == true)
        {
            szDecoderExePath = this->GetDecoderExe(nIntputFormat);
            GetFullPathName(szDecoderExePath);

            szDecoderOptions = this->GetDecoderOpt(nIntputFormat, -1);
        }
        else
        {
            // TODO:
        }

        CString szName = this->m_FileList.GetItemFileName(i);

        CString szEncoderExePath;
        CString szEncoderOptions;

        if(bUseListCtrl == true)
        {
            szEncoderExePath = this->GetEncoderExe(nOutputFormat);
            GetFullPathName(szEncoderExePath);

            szEncoderOptions = this->GetEncoderOpt(nOutputFormat, nPreset);
        }
        else
        {
            // TODO:
        }

        szName = szName + _T(".") + this->m_FileList.GetItemOutExt(i).MakeLower();

        CString szOutputFile;

        if(szOutPath.GetLength() >= 1)
        {
            if(szOutPath[szOutPath.GetLength() - 1] == '\\' || szOutPath[szOutPath.GetLength() - 1] == '/' )
                szOutputFile = szOutPath + szName;
            else
                szOutputFile = szOutPath + _T("\\") + szName;
        }
        else
        {
            szOutputFile = szName;
        }

        nProcessingMode = 1;
        if(nIntputFormat == 0)
            nProcessingMode = 0;

        if(nProcessingMode == 1)
            nProcessingMode = 2;

        if(nOutputFormat == 0)
        {
            bool bNeedResampling = (szEncoderOptions.GetLength() > 0) ? true : false;

            if((nIntputFormat == 0) && (bNeedResampling == false))
                nProcessingMode = 0;

            if((nIntputFormat == 0) && (bNeedResampling == true))
                nProcessingMode = 0;

            if((nIntputFormat > 0) && (bNeedResampling == false))
                nProcessingMode = 1;

            if((nIntputFormat > 0) && (bNeedResampling == true))
                nProcessingMode = 2;
        }

        CString csExecute;            
        bool bDecode = false;
        int nTool = -1;

        CString szOrgInputFile = szInputFile;
        CString szOrgOutputFile = szOutputFile;

        if(nProcessingMode == 2)
            szOutputFile = szOutputFile + _T(".wav");

        if((nProcessingMode == 1) || (nProcessingMode == 2))
        {
            csExecute = this->szFormatTemplate[(NUM_OUTPUT_EXT + nIntputFormat - 1)];
            csExecute.Replace(_T("$EXE"), _T("\"$EXE\""));
            csExecute.Replace(_T("$EXE"), szDecoderExePath);
            csExecute.Replace(_T("$OPTIONS"), szDecoderOptions);
            csExecute.Replace(_T("$INFILE"), _T("\"$INFILE\""));
            csExecute.Replace(_T("$INFILE"), szInputFile);
            csExecute.Replace(_T("$OUTFILE"), _T("\"$OUTFILE\""));
            csExecute.Replace(_T("$OUTFILE"), szOutputFile);

            bDecode = true;
            nTool = (NUM_OUTPUT_EXT + nIntputFormat - 1);

            CUtf8String szBuffUtf8;
            char *szCommandLine = szBuffUtf8.Create(csExecute);
            fp.Write(szCommandLine, (UINT) strlen(szCommandLine));
            szBuffUtf8.Clear();

            fp.Write(szLineEnd, (UINT) strlen(szLineEnd));

            if(bDeleteAfterConverion == true)
            {
                fp.Write(szPreDel, (UINT) strlen(szPreDel));

                CUtf8String szBuffUtf8;
                char *szDelFile = szBuffUtf8.Create(szOrgInputFile);
                fp.Write(szDelFile, (UINT) strlen(szDelFile));
                szBuffUtf8.Clear();

                fp.Write(szPostDel, (UINT) strlen(szPostDel));
            }
        }

        if(nProcessingMode == 2)
        {
            szInputFile = szOutputFile;
            szOrgInputFile = szOutputFile;
            szOutputFile = szOrgOutputFile;
        }

        if((nProcessingMode == 0) || (nProcessingMode == 2))
        {
            csExecute = this->szFormatTemplate[nOutputFormat];
            csExecute.Replace(_T("$EXE"), _T("\"$EXE\""));
            csExecute.Replace(_T("$EXE"), szEncoderExePath);
            csExecute.Replace(_T("$OPTIONS"), szEncoderOptions);
            csExecute.Replace(_T("$INFILE"), _T("\"$INFILE\""));
            csExecute.Replace(_T("$INFILE"), szInputFile);
            csExecute.Replace(_T("$OUTFILE"), _T("\"$OUTFILE\""));
            csExecute.Replace(_T("$OUTFILE"), szOutputFile);

            bDecode = false;
            nTool = nOutputFormat;

            CUtf8String szBuffUtf8;
            char *szCommandLine = szBuffUtf8.Create(csExecute);
            fp.Write(szCommandLine, (UINT) strlen(szCommandLine));
            szBuffUtf8.Clear();

            fp.Write(szLineEnd, (UINT) strlen(szLineEnd));

            if((bDeleteAfterConverion == true) || (nProcessingMode == 2))
            {
                fp.Write(szPreDel, (UINT) strlen(szPreDel));

                CUtf8String szBuffUtf8;
                char *szDelFile = szBuffUtf8.Create(szOrgInputFile);
                fp.Write(szDelFile, (UINT) strlen(szDelFile));
                szBuffUtf8.Clear();

                fp.Write(szPostDel, (UINT) strlen(szPostDel));
            }
        }
    }

    if(bUseListCtrl == true)
    {
        if(this->GetMenu()->GetMenuState(ID_OPTIONS_SHUTDOWN_WHEN_FINISHED, MF_BYCOMMAND) == MF_CHECKED)
            fp.Write(szShutdown, (UINT) strlen(szShutdown));
    }
    else
    {
        // TODO:
    }

    fp.Write(szPostfix, (UINT) strlen(szPostfix));
    fp.Close();

    return true;
}

void CBatchEncoderDlg::UpdateStatusBar()
{
    int nCount = m_LstInputFiles.GetItemCount();
    if(nCount > 0)
    {
        // NOTE: slow update with many files
        /*
        int nChecked = 0;
        int nSelected = 0;

        for(int i = 0; i < nCount; i++)
        {
            if(m_LstInputFiles.GetCheck(i) == TRUE)
                nChecked++;

            if(m_LstInputFiles.GetItemState(i,  LVIS_SELECTED) == LVIS_SELECTED)
                nSelected++;
        }

        CString szText;

        if((nChecked > 0) && (nSelected > 0))
        {
            szText.Format(_T("%d %s | %d Checked | %d Selected"), 
                nCount,
                (nCount > 1) ? _T("Files") : _T("File"),
                nChecked, 
                nSelected);
        }
        else if((nChecked > 0) && (nSelected == 0))
        {
            szText.Format(_T("%d %s | %d Checked"), 
                nCount,
                (nCount > 1) ? _T("Files") : _T("File"),
                nChecked);
        }
        else if((nChecked == 0) && (nSelected > 0))
        {
            szText.Format(_T("%d %s | %d Selected"), 
                nCount,
                (nCount > 1) ? _T("Files") : _T("File"),
                nSelected);
        }
        else
        {
            szText.Format(_T("%d %s"), 
                nCount,
                (nCount > 1) ? _T("Files") : _T("File"));
        }
        */

        CString szText;
        szText.Format(_T("%d %s"), 
            nCount,
            (nCount > 1) ? _T("Files") : _T("File"));

        VERIFY(m_StatusBar.SetText(szText, 0, 0));
    }
    else
    {
        VERIFY(m_StatusBar.SetText(_T("No Files"), 0, 0));
        VERIFY(m_StatusBar.SetText(_T(""), 1, 0));
    }
}

void CBatchEncoderDlg::EnableTrayIcon(bool bEnable, bool bModify)
{
    NOTIFYICONDATA tnd;
    HICON hIconExit;
    hIconExit = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MAINFRAME));

    tnd.cbSize = sizeof(NOTIFYICONDATA);
    tnd.hWnd = this->GetSafeHwnd();
    tnd.uID = 0x1000;
    tnd.hIcon = hIconExit;
    tnd.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tnd.uCallbackMessage = WM_TRAY;

    lstrcpy(tnd.szTip, _T("BatchEncoder"));

    if((bEnable == true) && (bModify == false))
    {
        Shell_NotifyIcon(NIM_ADD, &tnd);
        this->bShowTrayIcon = true;
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_SHOWTRAYICON, MF_CHECKED);
    }
    else if((bEnable == true) && (bModify == true))
    {
        Shell_NotifyIcon(NIM_MODIFY, &tnd);
    }
    else
    {
        // delete tray icon only if exist
        if(this->bShowTrayIcon == true)
        {
            Shell_NotifyIcon(NIM_DELETE, &tnd);
            this->bShowTrayIcon = false;
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_SHOWTRAYICON, MF_UNCHECKED);
        }
    }
}

void CBatchEncoderDlg::ShowProgressTrayIcon(int nProgress)
{
    if(this->bShowTrayIcon == false)
        return;

    int nIndex = 0;

    // (nIndex >= 0) && (nIndex < NUM_PROGRESS_ICONS)
    if((nProgress >= 0) && (nProgress < 5)) nIndex = 0;
    else if((nProgress > 5) && (nProgress <= 14)) nIndex = 1;
    else if((nProgress > 14) && (nProgress <= 23)) nIndex = 2;
    else if((nProgress > 23) && (nProgress <= 32)) nIndex = 3;
    else if((nProgress > 32) && (nProgress <= 41)) nIndex = 4;
    else if((nProgress > 41) && (nProgress <= 50)) nIndex = 5;
    else if((nProgress > 50) && (nProgress <= 59)) nIndex = 6;
    else if((nProgress > 59) && (nProgress <= 67)) nIndex = 7;
    else if((nProgress > 67) && (nProgress <= 76)) nIndex = 8;
    else if((nProgress > 76) && (nProgress <= 85)) nIndex = 9;
    else if((nProgress > 85) && (nProgress <= 95)) nIndex = 10;
    else if((nProgress > 95) && (nProgress <= 100)) nIndex = 11;
    else nIndex = 0;

    HICON hIconProgress = LoadIcon(GetModuleHandle(NULL), 
        MAKEINTRESOURCE(g_nProgressIconResources[nIndex]));

    NOTIFYICONDATA tnd;
    tnd.cbSize = sizeof(NOTIFYICONDATA);
    tnd.hWnd = this->GetSafeHwnd();
    tnd.uID = 0x1000;
    tnd.hIcon = hIconProgress;
    tnd.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tnd.uCallbackMessage = WM_TRAY;

    TCHAR szText[64];

    _stprintf(szText, _T("%d%%"), nProgress);
    _tcscpy(tnd.szTip, szText);

    Shell_NotifyIcon(NIM_MODIFY, &tnd);
}

void CBatchEncoderDlg::OnSize(UINT nType, int cx, int cy)
{
    CResizeDialog::OnSize(nType, cx, cy);

    if((bShowTrayIcon == true) && (nType == SIZE_MINIMIZED))
    {
        ShowWindow(SW_HIDE);
        InvalidateRect(NULL, FALSE);
    }
}

LRESULT CBatchEncoderDlg::OnTrayIconMsg(WPARAM wParam, LPARAM lParam)
{ 
    UINT uID = (UINT) wParam; 
    UINT uMouseMsg = (UINT) lParam; 

    if(bShowTrayIcon == false)
        return(0);

    if(uMouseMsg == WM_RBUTTONDOWN) 
    { 
        if(bRunning == true)
            return(0);

        CMenu menu;
        if(!menu.LoadMenu(IDR_MENU_TRAY))
            return(0);

        CMenu* pSubMenu = menu.GetSubMenu(0);
        if(!pSubMenu) 
            return(0);

        // set first menu item font to bold
        ::SetMenuDefaultItem(pSubMenu->m_hMenu, 0, TRUE);

        CPoint mouse;
        GetCursorPos(&mouse);
        ::SetForegroundWindow(this->GetSafeHwnd());	
        ::TrackPopupMenu(pSubMenu->m_hMenu, 0, mouse.x, mouse.y, 0, this->GetSafeHwnd(), NULL);
        ::PostMessage(this->GetSafeHwnd(), WM_NULL, 0, 0);
    }
    else if(uMouseMsg == WM_LBUTTONDOWN) 
    {
        if(this->IsWindowVisible() == FALSE)
        {
            ShowWindow(SW_SHOW);
            ShowWindow(SW_RESTORE);
            SetFocus();
            SetActiveWindow();
        }
        else
        {
            ShowWindow(SW_MINIMIZE);
        }
    }
    else if(uMouseMsg == WM_LBUTTONDBLCLK) 
    {
        if(bRunning == true)
            return(0);

        // ...
    }

    return(0); 
}

LRESULT CBatchEncoderDlg::OnListItemChaged(WPARAM wParam, LPARAM lParam)
{
    INT nIndex = (INT) wParam; 
    LPTSTR szText = (LPTSTR) lParam; 

    // update item data
    if((nIndex >= 0) && szText != NULL)
        this->m_FileList.SetItemFileName(szText, nIndex);

    return(0);
}

void CBatchEncoderDlg::OnTrayMenuExit()
{
    if(bShowTrayIcon == true)
        this->OnClose();
}

LRESULT CBatchEncoderDlg::OnNotifyFormat(WPARAM wParam,LPARAM lParam)
{
    // NOTE:
    // required for ClistView control to receive notifications messages
    // in UNICODE format when using UnicoWS.dll under Win9x systems
#ifdef _UNICODE
    return NFR_UNICODE;
#else
    return NFR_ANSI;
#endif
}

void CBatchEncoderDlg::OnPaint() 
{
    if(IsIconic())
    {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CResizeDialog::OnPaint();
    }
}

HCURSOR CBatchEncoderDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

CString CBatchEncoderDlg::GetDecoderExe(int nIntputFormat)
{
    return this->szFormatPath[(NUM_OUTPUT_EXT + nIntputFormat - 1)];
}

CString CBatchEncoderDlg::GetDecoderOpt(int nIntputFormat, int nPreset)
{
    CString szRet = _T("");

    // NOTE: presets for decoder are not supported

    return szRet;
}

CString CBatchEncoderDlg::GetEncoderExe(int nOutputFormat)
{
    return this->szFormatPath[nOutputFormat];
}

CString CBatchEncoderDlg::GetEncoderOpt(int nOutputFormat, int nPreset)
{
    return this->m_ListPresets[nOutputFormat].GetPresetOptions(nPreset);
}

bool CBatchEncoderDlg::LoadPresets(CString szPresetsFName, CLListPresets *m_ListPresets)
{
    CTiXmlDocumentW doc;
    if(doc.LoadFileW(szPresetsFName) == true)
    {
        TiXmlHandle hDoc(&doc);
        TiXmlElement* pElem;

        TiXmlHandle hRoot(0);

        // root: Prestes
        pElem = hDoc.FirstChildElement().Element();
        if(!pElem) 
            return false;

        hRoot = TiXmlHandle(pElem);

        // check for "Presets"
        const char *szRoot = pElem->Value(); 
        const char *szRootName = "Presets"; 
        if(strcmp(szRootName, szRoot) != 0)
            return false;

        // remove all presets from list
        m_ListPresets->RemoveAllNodes();

        // fill list with new presets
        int nIndex = 0;
		TiXmlElement* pFilesNode = hRoot.FirstChild("Preset").Element();
		for(pFilesNode; pFilesNode; pFilesNode = pFilesNode->NextSiblingElement())
		{
			const char *pszName = pFilesNode->Attribute("name");
            const char *pszOptions = pFilesNode->Attribute("options");

            CString szNameData = GetConfigString(pszName);
            CString szOptionsData = GetConfigString(pszOptions);

			if((pszName != NULL) && (pszOptions != NULL)) 
            {
                m_ListPresets->InsertNode(szNameData);
                m_ListPresets->SetPresetOptions(szOptionsData, nIndex);
                nIndex++;
            }
		}

        return true;
    }

    return false;
}

void CBatchEncoderDlg::FillPresetComboBox(CLListPresets *m_ListPresets, int nSelIndex)
{
    if(m_ListPresets != NULL)
    {
        int nSize = m_ListPresets->GetSize();

        // check if we have some presets
        if(nSize == 0)
        {
            // reset presets ComboBox
            this->m_CmbPresets.ResetContent();

            // insert Error Message
            this->m_CmbPresets.AddString(_T("Not Available"));

            return;
        }

        // reset ComboBox content
        this->m_CmbPresets.ResetContent();

        // insert all preset names to combobox
        for(int i = 0; i < nSize; i++)
        {
            CString szPresetName = m_ListPresets->GetPresetName(i);
            this->m_CmbPresets.InsertString(i, szPresetName);
        }

        // resize ComboBox only once
        static bool bInitPresetsCombo = false;
        if(bInitPresetsCombo == false)
        {
            ::SetComboBoxHeight(this->GetSafeHwnd(), IDC_COMBO_PRESETS);
            bInitPresetsCombo = true;
        }

        // select default item in presets ComboBox
        if((nSelIndex < 0) || (nSelIndex > nSize - 1))
            this->m_CmbPresets.SetCurSel(0); // ERROR, select first item
        else
            this->m_CmbPresets.SetCurSel(nSelIndex);
    }
}

CLListPresets *CBatchEncoderDlg::GetCurrentPresetsList(void)
{
    int nSelFormatIndex = this->m_CmbFormat.GetCurSel();

    if((nSelFormatIndex >= 0) && (nSelFormatIndex < NUM_PRESET_FILES))
        return &this->m_ListPresets[nSelFormatIndex];
    else
        return NULL; // ERROR
}

void CBatchEncoderDlg::UpdateOutputComboBoxes(int nSelFormatIndex, int nSelPresetIndex)
{
    // check the format index
    if(this->m_CmbFormat.GetCount() < nSelFormatIndex ||  nSelFormatIndex < 0)
    {
        this->m_CmbFormat.SetCurSel(0);
        this->m_CmbPresets.SetCurSel(0);

        return; // ERROR
    }

    if((nSelFormatIndex >= 0) && (nSelFormatIndex < NUM_PRESET_FILES))
    {
        this->FillPresetComboBox(&this->m_ListPresets[nSelFormatIndex], nSelPresetIndex);
    }
    else
    {
        // ERROR
        this->m_CmbPresets.ResetContent();
        this->m_CmbPresets.InsertString(0, _T("Default"));
        this->m_CmbPresets.SetCurSel(0);
    }

    this->m_CmbFormat.SetCurSel(nSelFormatIndex);
}

CString CBatchEncoderDlg::BrowseForSettings()
{
    CFileDialog fd(TRUE, _T("config"), MAIN_APP_CONFIG, 
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER, 
        _T("Config Files (*.config)|*.config|Xml Files (*.xml)|*.xml|All Files|*.*||"), this);

    fd.m_ofn.lpstrInitialDir = ::GetExeFilePath();
    if(fd.DoModal() == IDOK)
    {
        CString szPath;
        szPath = fd.GetPathName();
        return szPath;
    }
    return NULL;
}

LPTSTR CBatchEncoderDlg::GetMenuItemCheck(int nID)
{
    return (this->GetMenu()->GetMenuState(nID, MF_BYCOMMAND) == MF_CHECKED) ? _T("true") : _T("false");
}

void CBatchEncoderDlg::SetMenuItemCheck(int nID, LPTSTR bChecked)
{
    // NOTE: bChecked IS _T("true") OR _T("false")
    this->GetMenu()->CheckMenuItem(nID, (lstrcmp(_T("true"), bChecked) == 0) ? MF_CHECKED : MF_UNCHECKED);
}

bool CBatchEncoderDlg::GridlinesVisible()
{
    DWORD dwExStyle = m_LstInputFiles.GetExtendedStyle();
    if(dwExStyle & LVS_EX_GRIDLINES)
        return true;
    else
        return false;
}

void CBatchEncoderDlg::ShowGridlines(bool bShow)
{
    DWORD dwExStyle = m_LstInputFiles.GetExtendedStyle();
    if(bShow == true)
    {
        dwExStyle |= LVS_EX_GRIDLINES;
        m_LstInputFiles.SetExtendedStyle(dwExStyle);
        this->GetMenu()->CheckMenuItem(ID_VIEW_SHOWGRIDLINES, MF_CHECKED);
    }
    else
    {
        // check if we have gridlines on
        if(dwExStyle & LVS_EX_GRIDLINES)
        {
            dwExStyle = dwExStyle ^ LVS_EX_GRIDLINES;
            m_LstInputFiles.SetExtendedStyle(dwExStyle);
            this->GetMenu()->CheckMenuItem(ID_VIEW_SHOWGRIDLINES, MF_UNCHECKED);
        }
    }
}

bool CBatchEncoderDlg::LoadSettings()
{
    ::UpdatePath();

    CTiXmlDocumentW doc;

    // try to load default config file
    if(doc.LoadFileW(szMainConfigFile) == false)
    {
        // create configuration file from resources
        BOOL bRet = FALSE;
        LPVOID lpvBuf = NULL;
        INT64 dwSize = 0UL;

        lpvBuf = LoadXmlResource(_T("CONFIG"), IDR_CONFIG_BATCHENCODER, &dwSize);
        if((lpvBuf != NULL) && (dwSize > 0UL))
        {
            szMainConfigFile = MAIN_APP_CONFIG;

            CFile fp;
            if(fp.Open(szMainConfigFile, CFile::modeReadWrite | CFile::modeCreate) == TRUE)
            {
                fp.Write(lpvBuf, (UINT) dwSize);
                fp.Close();
                FreeXmlResource(lpvBuf);

                // try again to load configuration
                if(doc.LoadFileW(szMainConfigFile) == false)
                {
                    // ::DeleteFile(szMainConfigFile);
                    return false;
                }
            }
            else
            {
                FreeXmlResource(lpvBuf);
                return false;
            }
        }
        else
        {
            // when failed to load configuration ask user for filename
            CString szPath = this->BrowseForSettings();
            if(szPath.GetLength() > 0)
            {
                if(doc.LoadFileW(szPath) == false)
                    return false;
            }
            else
            {
                return false;
            }
        }
    }

    TiXmlHandle hDoc(&doc);
    TiXmlElement* pElem;
    TiXmlHandle hRoot(0);

    pElem = hDoc.FirstChildElement().Element();
    if(!pElem) 
        return false;

    hRoot = TiXmlHandle(pElem);

    // root: "BatchEncoder"
    const char *szRoot = pElem->Value(); 
    const char *szRootName = "BatchEncoder"; 
    if(strcmp(szRootName, szRoot) != 0)
        return false;

    // root: Settings
    CString szSetting[NUM_PROGRAM_SETTINGS];
    for(int i = 0; i < NUM_PROGRAM_SETTINGS; i++)
    {
        pElem = hRoot.FirstChild("Settings").FirstChild(g_szSettingsTags[i]).Element();
        if(pElem)
        {
            const char *tmpBuff = pElem->GetText();
            szSetting[i] = GetConfigString(tmpBuff);
        }
        else
        {
            szSetting[i] = _T("");
        }
    }

    // root: Colors

    /*  
    NOTE: Default colors:

    <Colors>
        <CnvStatusText>0x00 0x00 0x00</CnvStatusText>
        <CnvStatusTextError>0xFF 0x00 0x00</CnvStatusTextError>
        <CnvStatusProgress>0x33 0x66 0xFF</CnvStatusProgress>
        <CnvStatusBorder>0x00 0x00 0x00</CnvStatusBorder>
        <CnvStatusBack></CnvStatusBack>
        <HistogramLR>0xEF 0xC7 0x7B</HistogramLR>
        <HistogramMS>0xFE 0xF6 0xE4</HistogramMS>
        <HistogramBorder>0x00 0x00 0x00</HistogramBorder>
        <HistogramBack></HistogramBack>
    </Colors>

    NOTE: This colors are set after first startup because they depend on system settings.

    <Colors>
        <CnvStatusBack>0xD4 0xD0 0xC8</CnvStatusBack>
        <HistogramBack>0xD4 0xD0 0xC8</HistogramBack>
    </Colors>
    */

    // NOTE: colors can be in decimal, hexadecimal, or octal integer format
    CString szColor[NUM_PROGRAM_COLORS];
    for(int i = 0; i < NUM_PROGRAM_COLORS; i++)
    {
        pElem = hRoot.FirstChild("Colors").FirstChild(g_szColorsTags[i]).Element();
        if(pElem)
            szColor[i] = GetConfigString(pElem->GetText());
        else
            szColor[i] = _T("");
    }

    // 0
    if(szColor[0].Compare(_T("")) != 0)
    {
        int nRGB[3];
        if(_stscanf(szColor[0], _T("%i %i %i"), &nRGB[0], &nRGB[1], &nRGB[2]) == 3)
            this->m_CnvStatus.crText = RGB(nRGB[0], nRGB[1], nRGB[2]);
    }

    // 1
    if(szColor[1].Compare(_T("")) != 0)
    {
        int nRGB[3];
        if(_stscanf(szColor[1], _T("%i %i %i"), &nRGB[0], &nRGB[1], &nRGB[2]) == 3)
            this->m_CnvStatus.crTextError = RGB(nRGB[0], nRGB[1], nRGB[2]);
    }

    // 2
    if(szColor[2].Compare(_T("")) != 0)
    {
        int nRGB[3];
        if(_stscanf(szColor[2], _T("%i %i %i"), &nRGB[0], &nRGB[1], &nRGB[2]) == 3)
            this->m_CnvStatus.crProgress = RGB(nRGB[0], nRGB[1], nRGB[2]);
    }

    // 3
    if(szColor[3].Compare(_T("")) != 0)
    {
        int nRGB[3];
        if(_stscanf(szColor[3], _T("%i %i %i"), &nRGB[0], &nRGB[1], &nRGB[2]) == 3)
            this->m_CnvStatus.crBorder = RGB(nRGB[0], nRGB[1], nRGB[2]);
    }

    // 4
    if(szColor[4].Compare(_T("")) != 0)
    {
        int nRGB[3];
        if(_stscanf(szColor[4], _T("%i %i %i"), &nRGB[0], &nRGB[1], &nRGB[2]) == 3)
            this->m_CnvStatus.crBack = RGB(nRGB[0], nRGB[1], nRGB[2]);
    }

    // 5
    if(szColor[5].Compare(_T("")) != 0)
    {
        int nRGB[3];
        if(_stscanf(szColor[5], _T("%i %i %i"), &nRGB[0], &nRGB[1], &nRGB[2]) == 3)
            this->m_Histogram.crLR = RGB(nRGB[0], nRGB[1], nRGB[2]);
    }

    // 6
    if(szColor[6].Compare(_T("")) != 0)
    {
        int nRGB[3];
        if(_stscanf(szColor[6], _T("%i %i %i"), &nRGB[0], &nRGB[1], &nRGB[2]) == 3)
            this->m_Histogram.crMS = RGB(nRGB[0], nRGB[1], nRGB[2]);
    }

    // 7
    if(szColor[7].Compare(_T("")) != 0)
    {
        int nRGB[3];
        if(_stscanf(szColor[7], _T("%i %i %i"), &nRGB[0], &nRGB[1], &nRGB[2]) == 3)
            this->m_Histogram.crBorder = RGB(nRGB[0], nRGB[1], nRGB[2]);
    }

    // 8
    if(szColor[8].Compare(_T("")) != 0)
    {
        int nRGB[3];
        if(_stscanf(szColor[8], _T("%i %i %i"), &nRGB[0], &nRGB[1], &nRGB[2]) == 3)
            this->m_Histogram.crBack = RGB(nRGB[0], nRGB[1], nRGB[2]);
    }

    // root: Presets
    if(this->m_pFo->bHavePresets == false)
    {
        for(int i = 0; i < NUM_PRESET_FILES; i++)
        {
            pElem = hRoot.FirstChild("Presets").FirstChild(g_szPresetTags[i]).Element();
            if(pElem)
            {
                const char *tmpBuff = pElem->GetText();
                this->szPresetsFile[i] = GetConfigString(tmpBuff);
            }
            else
            {
                this->szPresetsFile[i] = g_szPresetFiles[i];
            }
        }
    }

    // root: Formats
    if(this->m_pFo->bHaveFormats == false)
    {
        // NOTE: 
        // same code as in CFormatsDlg::OnBnClickedButtonLoadConfig()
        // only FirstChild("Formats").FirstChild("Format") is different

        pElem = hRoot.FirstChild("Formats").FirstChild("Format").Element();
        for(pElem; pElem; pElem = pElem->NextSiblingElement())
        {
            int nFormat = -1;

            const char *pszName = pElem->Attribute("name");
            if(pszName != NULL)
            {
                CString szBuff = GetConfigString(pszName);

                nFormat = ::GetFormatId(szBuff);

                // check if this is valid format name
                if((nFormat < 0) || (nFormat >= NUM_FORMAT_NAMES))
                {
                    // invalid format Id
                    continue;
                }
            }
            else
            {
                // unknown or invalid format
                continue;
            }

            const char *pszTemplate = pElem->Attribute("template");
            if(pszTemplate != NULL)
            {
                szFormatTemplate[nFormat] = GetConfigString(pszTemplate);
            }

            const char *pszPipesInput = pElem->Attribute("input");
            if(pszPipesInput != NULL)
            {
                CString szBuff = GetConfigString(pszPipesInput);
                if(szBuff.CompareNoCase(_T("true")) == 0)
                    bFormatInput[nFormat] = true;
                else
                    bFormatInput[nFormat] = false;
            }

            const char *pszPipesOutput = pElem->Attribute("output");
            if(pszPipesOutput != NULL)
            {
                CString szBuff = GetConfigString(pszPipesOutput);
                if(szBuff.CompareNoCase(_T("true")) == 0)
                    bFormatOutput[nFormat] = true;
                else
                    bFormatOutput[nFormat] = false;
            }

            const char *pszFunction = pElem->Attribute("function");
            if(pszFunction != NULL)
            {
                szFormatFunction[nFormat] = GetConfigString(pszFunction);
            }

            const char *tmpBuff = pElem->GetText();
            szFormatPath[nFormat] = GetConfigString(tmpBuff);
        }
    }

    // root: Browse
    for(int i = 0; i < NUM_BROWSE_PATH; i++)
    {
        char szPathTag[32];
        
        ZeroMemory(szPathTag, sizeof(szPathTag));
        sprintf(szPathTag, "Path_%02d", i);

        pElem = hRoot.FirstChild("Browse").FirstChild(szPathTag).Element();
        if(pElem)
            this->szBrowsePath[i] = GetConfigString(pElem->GetText());
        else
            this->szBrowsePath[i] = ::GetExeFilePath();
    }

    // NOTE:
    // this is special case for this->szBrowsePath[4]
    // check for outpath if not presets set to default value
    if(this->szBrowsePath[4].Compare(_T("")) != 0)
    {
            this->m_EdtOutPath.SetWindowText(this->szBrowsePath[4]);
            szLastBrowse = this->szBrowsePath[4];
    }
    else
    {
        CString szBuff = ::GetExeFilePath();
        m_EdtOutPath.SetWindowText(szBuff);
        szLastBrowse = szBuff;
        this->szBrowsePath[4] = szBuff;
    }

    // root: Files
    if(this->m_pFo->bHaveFileList == false)
    {
        this->OnFileClearList();

        NewItemData nid;
        ::InitNewItemData(nid);

        TiXmlElement* pFilesNode = hRoot.FirstChild("Files").FirstChild().Element();
        for(pFilesNode; pFilesNode; pFilesNode = pFilesNode->NextSiblingElement())
        {
            char *pszAttrib[NUM_FILE_ATTRIBUTES];
            for(int i = 0; i < NUM_FILE_ATTRIBUTES; i++)
                pszAttrib[i] = (char *) pFilesNode->Attribute(g_szFileAttributes[i]);

            bool bValidFile = true;
            for(int i = 0; i < NUM_FILE_ATTRIBUTES; i++)
            {
                if(pszAttrib[i] == NULL)
                {
                    bValidFile = false;
                    break;
                }
            }

            if(bValidFile == true)
            {
                CString szData[NUM_FILE_ATTRIBUTES];
                for(int i = 0; i < NUM_FILE_ATTRIBUTES; i++)
                    szData[i] = GetConfigString(pszAttrib[i]);

                nid.nAction = 2;
                nid.szFileName = szData[0]; 
                nid.nItem = -1;
                nid.szName = szData[2];
                nid.szOutExt = szData[5];
                nid.nPreset = stoi(szData[6]);
                nid.bCheck = (szData[1].Compare(_T("true")) == 0) ? TRUE : FALSE;
                nid.szTime = szData[7];
                nid.szStatus = szData[8];

                this->InsertToList(nid);
            }
        }
    }
    else
    { 
        // add in loop files to control FileList
        NewItemData nid;
        ::InitNewItemData(nid);

        for(int i = 0; i < this->m_FileList.GetSize(); i++)
        {
            nid.nAction = 1;
            nid.szFileName = _T(""); 
            nid.nItem = i;

            this->InsertToList(nid);
        }
    }

    // handle loaded settings
    int nSelFormatIndex = 0;

    // 0
    // Description: get for each output format selected preset index
    if(szSetting[0].Compare(_T("")) != 0)
    {
        CString resToken;
        int curPos = 0;
        int nIndex = 0;

        resToken = szSetting[0].Tokenize(_T(" "), curPos);
        while(resToken != _T("") && nIndex <= (NUM_OUTPUT_EXT - 1))
        {
            nCurSel[nIndex] = stoi(resToken);
            resToken = szSetting[0].Tokenize(_T(" "), curPos);
            nIndex++;
        }
    }
    else
    {
        ZeroMemory(&nCurSel, sizeof(int) * NUM_OUTPUT_EXT);
    }

    // 1
    // Description: get selected format in formats combobox
    if(szSetting[1].Compare(_T("")) != 0)
    {
        nSelFormatIndex = stoi(szSetting[1]);
        if(nSelFormatIndex < 0)
            nSelFormatIndex = 0;
    }
    else
    {
        // by default select 2nd format
        nSelFormatIndex = 0;
    }

    // 2
    // Description: output path checkbox state
    if(szSetting[2].Compare(_T("")) != 0)
    {
        if(szSetting[2].Compare(_T("true")) == 0)
        {
            m_ChkOutPath.SetCheck(BST_CHECKED);
        }
        else
        {
            m_BtnBrowse.EnableWindow(FALSE);
            m_EdtOutPath.EnableWindow(FALSE);
        }
    }
    else
    {
        m_BtnBrowse.EnableWindow(FALSE);
        m_EdtOutPath.EnableWindow(FALSE);
    }

    // 3
    // Description: debug checkbox state
    if(szSetting[3].Compare(_T("")) != 0)
    {
        if(szSetting[3].Compare(_T("true")) == 0)
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_LOGCONSOLEOUTPUT, MF_CHECKED);
        else
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_LOGCONSOLEOUTPUT, MF_UNCHECKED);
    }
    else
    {
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_LOGCONSOLEOUTPUT, MF_UNCHECKED);
    }

    // 4
    // Description: delete source file after successful conversion
    if(szSetting[4].Compare(_T("")) != 0)
    {
        if(szSetting[4].Compare(_T("true")) == 0)
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_DELETESOURCEFILEWHENDONE, MF_CHECKED);
        else
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_DELETESOURCEFILEWHENDONE, MF_UNCHECKED);
    }
    else
    {
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_DELETESOURCEFILEWHENDONE, MF_UNCHECKED);
    }

    // 5
    // Description: make main dialog window on top of all other desktop windows
    if(szSetting[5].Compare(_T("")) != 0)
    {
        if(szSetting[5].Compare(_T("true")) == 0)
        {
            this->SetWindowPos(CWnd::FromHandle(HWND_TOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_STAYONTOP, MF_CHECKED);
        }
        else
        {
            this->SetWindowPos(CWnd::FromHandle(HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_STAYONTOP, MF_UNCHECKED);
        }
    }

    // 6
    // Description: enables recursed flag in browse dialog
    if(szSetting[6].Compare(_T("")) != 0)
    {
        if(szSetting[6].Compare(_T("true")) == 0)
            ::bRecurseChecked = true;
        else
            ::bRecurseChecked = false;
    }
    else
    {
        ::bRecurseChecked = true;
    }

    // 7
    // Description: set main window rectangle and position
    if(szSetting[7].Compare(_T("")) != 0)
    {
        this->SetWindowRectStr(szSetting[7]);
    }

    // 8
    // Description: width of each column in FileList
    if(szSetting[8].Compare(_T("")) != 0)
    {
        int nColWidth[7];
        if(_stscanf(szSetting[8], _T("%d %d %d %d %d %d %d"), 
            &nColWidth[0], 
            &nColWidth[1], 
            &nColWidth[2], 
            &nColWidth[3], 
            &nColWidth[4], 
            &nColWidth[5], 
            &nColWidth[6]) == 7)
        {
            for(int i = 0; i < 7; i++)
                m_LstInputFiles.SetColumnWidth(i, nColWidth[i]);
        }
    }

    // 9
    // Description: show gridlines in ListCtrl
    if(szSetting[9].Compare(_T("")) != 0)
    {
        if(szSetting[9].Compare(_T("true")) == 0)
            ShowGridlines(true);
        else
            ShowGridlines(false);
    }
    else
    {
        ShowGridlines(true);
    }

    // 10
    // Description: show program icon in system tray
    if(szSetting[10].Compare(_T("")) != 0)
    {
        if(szSetting[10].Compare(_T("true")) == 0)
        {
            this->EnableTrayIcon(true);
            bShowTrayIcon = true;
        }
        else
        {
            this->EnableTrayIcon(false);
            bShowTrayIcon = false;
        }
    }
    else
    {
        this->EnableTrayIcon(false);
        bShowTrayIcon = false;
    }

    // 11
    // Description: do not save setting on exit
    if(szSetting[11].Compare(_T("")) != 0)
    {
        if(szSetting[11].Compare(_T("true")) == 0)
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_DO_NOT_SAVE, MF_CHECKED);
        else
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_DO_NOT_SAVE, MF_UNCHECKED);
    }
    else
    {
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_DO_NOT_SAVE, MF_UNCHECKED);
    }

    // 12
    // Description: presets window rectangle and position
    if(szSetting[12].Compare(_T("")) != 0)
    {
        this->szPresetsWndResize = szSetting[12];
    }

    // 13
    // Description: width of each column in PresetsList
    if(szSetting[13].Compare(_T("")) != 0)
    {
        this->szPresetsListColumns = szSetting[13];
    }

    // 14
    // Description: formats window rectangle and position
    if(szSetting[14].Compare(_T("")) != 0)
    {
        this->szFormatsWndResize = szSetting[14];
    }

    // 15
    // Description: width of each column in FormatsList
    if(szSetting[15].Compare(_T("")) != 0)
    {
        this->szFormatsListColumns = szSetting[15];
    }

    // 16
    // Description: encoder/decoder thread priority index
    if(szSetting[16].Compare(_T("")) != 0)
    {
        nThreadPriorityIndex = stoi(szSetting[16]);
    }
    else
    {
        nThreadPriorityIndex = 3;
    }

    // 17
    // Description: encoder/decoder process priority index
    if(szSetting[17].Compare(_T("")) != 0)
    {
        nProcessPriorityIndex = stoi(szSetting[17]);
    }
    else
    {
        nProcessPriorityIndex = 1;
    }

    // 18
    // Description: delete output file on error
    if(szSetting[18].Compare(_T("")) != 0)
    {
        if(szSetting[18].Compare(_T("true")) == 0)
            bDeleteOnError = true;
        else
            bDeleteOnError = false;
    }
    else
    {
        bDeleteOnError = true;
    }

    // 19
    // Description: stop conversion process on error
    if(szSetting[19].Compare(_T("")) != 0)
    {
        if(szSetting[19].Compare(_T("true")) == 0)
            bStopOnErrors = true;
        else
            bStopOnErrors = false;
    }
    else
    {
        bStopOnErrors = false;
    }

    // 20
    // Description: log filename for console output
    if(szSetting[20].Compare(_T("")) != 0)
    {
         szLogFileName = szSetting[20];
    }
    else
    {
        szLogFileName = MAIN_APP_LOG;
    }

    // 21
    // Description: encoding of data stored in logfile
    if(szSetting[21].Compare(_T("")) != 0)
    {
        nLogEncoding = stoi(szSetting[21]);
    }
    else
    {
        nLogEncoding = 2;
    }

    // 22
    // Description: start conversion with extended progress window
    if(szSetting[22].Compare(_T("")) != 0)
    {
        if(szSetting[22].Compare(_T("true")) == 0)
            this->GetMenu()->CheckMenuItem(ID_VIEW_STARTWITHEXTENDEDPROGRESS, MF_CHECKED);
        else
            this->GetMenu()->CheckMenuItem(ID_VIEW_STARTWITHEXTENDEDPROGRESS, MF_UNCHECKED);
    }
    else
    {
        this->GetMenu()->CheckMenuItem(ID_VIEW_STARTWITHEXTENDEDPROGRESS, MF_UNCHECKED);
    }

    // 23
    // Description: force console window instead of conversion progress
    if(szSetting[23].Compare(_T("")) != 0)
    {
        if(szSetting[23].Compare(_T("true")) == 0)
        {
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_FORCECONSOLEWINDOW, MF_CHECKED);
            this->bForceConsoleWindow = true;
        }
        else
        {
            this->GetMenu()->CheckMenuItem(ID_OPTIONS_FORCECONSOLEWINDOW, MF_UNCHECKED);
            this->bForceConsoleWindow = false;
        }
    }
    else
    {
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_FORCECONSOLEWINDOW, MF_UNCHECKED);
        this->bForceConsoleWindow = false;

    }

    // remove all items from current preset
    this->m_CmbPresets.ResetContent();

    // clear presets lists
    for(int i = 0; i < NUM_PRESET_FILES; i++)
        this->m_ListPresets[i].RemoveAllNodes();

    // load all presets configuration files
    for(int i = 0; i < NUM_PRESET_FILES; i++)
    {
        if(this->LoadPresets(this->szPresetsFile[i], &this->m_ListPresets[i]) == false)
        {
            // create presets file from resources
            BOOL bRet = FALSE;
            LPVOID lpvBuf = NULL;
            INT64 dwSize = 0UL;

            lpvBuf = LoadXmlResource(_T("PRESET"), g_nPresetResources[i], &dwSize);
            if((lpvBuf != NULL) && (dwSize > 0UL))
            {
                CFile fp;
                if(fp.Open(g_szPresetFiles[i], CFile::modeReadWrite | CFile::modeCreate) == TRUE)
                {
                    fp.Write(lpvBuf, (UINT) dwSize);
                    fp.Close();
                    FreeXmlResource(lpvBuf);

                    // try again to load presets
                    if(this->LoadPresets(g_szPresetFiles[i], &this->m_ListPresets[i]) == false)
                        return false;
                }
                else
                {
                    FreeXmlResource(lpvBuf);
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }

    // update output ComboBox'es depending on selected format
    this->UpdateOutputComboBoxes(nSelFormatIndex, nCurSel[nSelFormatIndex]);

    return true;
}

bool CBatchEncoderDlg::SaveSettings()
{
    // save all settings to file
	CTiXmlDocumentW doc;
 	TiXmlDeclaration *decl = new TiXmlDeclaration("1.0", "UTF-8", "");
	doc.LinkEndChild(decl);
 
    // root: BatchEncoder
	TiXmlElement *root = new TiXmlElement("BatchEncoder");
	doc.LinkEndChild(root);

    // root: Settings
    TiXmlElement *stg;
	TiXmlElement *settings = new TiXmlElement("Settings");  
	root->LinkEndChild(settings); 

    CString szSetting[NUM_PROGRAM_SETTINGS];

    // 0
    szSetting[0] = _T("");
    for(int i = 0; i < NUM_OUTPUT_EXT; i++)
    {
        CString szTemp;
        szTemp.Format(_T("%d"), nCurSel[i]);
        szSetting[0] += szTemp;

        if(i < (NUM_OUTPUT_EXT - 1))
            szSetting[0] += _T(" ");
    }

    // 1
    szSetting[1].Format(_T("%d\0"), this->m_CmbFormat.GetCurSel());

    // 2
    szSetting[2] = (this->m_ChkOutPath.GetCheck() == BST_CHECKED) ? _T("true") : _T("false");

    // 3
    szSetting[3] = this->GetMenuItemCheck(ID_OPTIONS_LOGCONSOLEOUTPUT);

    // 4
    szSetting[4] = this->GetMenuItemCheck(ID_OPTIONS_DELETESOURCEFILEWHENDONE);

    // 5
    szSetting[5] = this->GetMenuItemCheck(ID_OPTIONS_STAYONTOP);

    // 6
    szSetting[6] = (::bRecurseChecked == true) ? _T("true") : _T("false");

    // 7
    szSetting[7] = this->GetWindowRectStr();

    // 8
    int nColWidth[7];
    for(int i = 0; i < 7; i++)
        nColWidth[i] = m_LstInputFiles.GetColumnWidth(i);
    szSetting[8].Format(_T("%d %d %d %d %d %d %d"), 
        nColWidth[0], 
        nColWidth[1], 
        nColWidth[2], 
        nColWidth[3], 
        nColWidth[4], 
        nColWidth[5], 
        nColWidth[6]);

    // 9
    szSetting[9] = this->GetMenuItemCheck(ID_VIEW_SHOWGRIDLINES);

    // 10
    szSetting[10] = this->GetMenuItemCheck(ID_OPTIONS_SHOWTRAYICON);

    // 11
    szSetting[11] = this->GetMenuItemCheck(ID_OPTIONS_DO_NOT_SAVE);

    // 12
    szSetting[12] = szPresetsWndResize;

    // 13
    szSetting[13] = szPresetsListColumns;

    // 14
    szSetting[14] = szFormatsWndResize;

    // 15
    szSetting[15] = szFormatsListColumns;

    // 16
    szSetting[16].Format(_T("%d"), this->nThreadPriorityIndex);

    // 17
    szSetting[17].Format(_T("%d"), this->nProcessPriorityIndex);

    // 18
    szSetting[18] = (this->bDeleteOnError == true) ? _T("true") : _T("false");

    // 19
    szSetting[19] = (this->bStopOnErrors == true) ? _T("true") : _T("false");

    // 20
    szSetting[20] = this->szLogFileName;

    // 21
    szSetting[21].Format(_T("%d"), nLogEncoding);

    // 22
    szSetting[22] = this->GetMenuItemCheck(ID_VIEW_STARTWITHEXTENDEDPROGRESS);

    // 23
    szSetting[23] = this->GetMenuItemCheck(ID_OPTIONS_FORCECONSOLEWINDOW);

    // store all settings from szSetting buffer
    for(int i = 0; i < NUM_PROGRAM_SETTINGS; i++)
    {
        CUtf8String szBuffUtf8;

        stg = new TiXmlElement(g_szSettingsTags[i]);
        stg->LinkEndChild(new TiXmlText(szBuffUtf8.Create(szSetting[i])));  
        settings->LinkEndChild(stg);
        szBuffUtf8.Clear();
    }

    // root: Colors
    TiXmlElement *clr;
	TiXmlElement *colors = new TiXmlElement("Colors");  
	root->LinkEndChild(colors); 

    CString szColor[NUM_PROGRAM_COLORS];
    COLORREF crColor[NUM_PROGRAM_COLORS];

    crColor[0] = this->m_CnvStatus.crText;
    crColor[1] = this->m_CnvStatus.crTextError;
    crColor[2] = this->m_CnvStatus.crProgress;
    crColor[3] = this->m_CnvStatus.crBorder;
    crColor[4] = this->m_CnvStatus.crBack;
    crColor[5] = this->m_Histogram.crLR;
    crColor[6] = this->m_Histogram.crMS;
    crColor[7] = this->m_Histogram.crBorder;
    crColor[8] = this->m_Histogram.crBack;

    for(int i = 0; i < NUM_PROGRAM_COLORS; i++)
    {
        szColor[i].Format(_T("0x%02X 0x%02X 0x%02X"),
            GetRValue(crColor[i]),
            GetGValue(crColor[i]),
            GetBValue(crColor[i]));
    }

    // store all colors from szColor buffer
    for(int i = 0; i < NUM_PROGRAM_COLORS; i++)
    {
        CUtf8String szBuffUtf8;

        clr = new TiXmlElement(g_szColorsTags[i]);
        clr->LinkEndChild(new TiXmlText(szBuffUtf8.Create(szColor[i])));  
        colors->LinkEndChild(clr);
        szBuffUtf8.Clear();
    }

    // root: Presets
	TiXmlElement* preset;
	TiXmlElement *presets = new TiXmlElement("Presets");  
	root->LinkEndChild(presets);  

    for(int i = 0; i < NUM_PRESET_FILES; i++)
    {
        CUtf8String szBuffUtf8;

        preset = new TiXmlElement(g_szPresetTags[i]);  
        preset->LinkEndChild(new TiXmlText(szBuffUtf8.Create(this->szPresetsFile[i])));  
        presets->LinkEndChild(preset); 
        szBuffUtf8.Clear();
    }

    // root: Formats
    TiXmlElement *formats = new TiXmlElement("Formats");  
    root->LinkEndChild(formats);
    TiXmlElement *format;  

    // NOTE:
    // same code as in CFormatsDlg::OnBnClickedButtonSaveConfig()
    // only root->LinkEndChild(formats) is different

    for(int i = 0; i < NUM_FORMAT_NAMES; i++)
    {
        CUtf8String m_Utf8;

        format = new TiXmlElement("Format");

        format->LinkEndChild(new TiXmlText(m_Utf8.Create(szFormatPath[i])));
        m_Utf8.Clear();

        format->SetAttribute("name", m_Utf8.Create(g_szFormatNames[i]));
        m_Utf8.Clear();

        format->SetAttribute("template", m_Utf8.Create(szFormatTemplate[i]));
        m_Utf8.Clear();

        format->SetAttribute("input", (bFormatInput[i]) ? "true" : "false");
        format->SetAttribute("output", (bFormatOutput[i]) ? "true" : "false");

        format->SetAttribute("function", m_Utf8.Create(szFormatFunction[i]));
        m_Utf8.Clear();

        formats->LinkEndChild(format); 
    }

    // root: Browse
	TiXmlElement* path;
	TiXmlElement *browse = new TiXmlElement("Browse");  
	root->LinkEndChild(browse);  

    // NOTE: 
    // get last browse for outpath this is special case 
    // because user can change this value 
    // without changing this->szBrowsePath[4] variable
    m_EdtOutPath.GetWindowText(this->szBrowsePath[4]);

    for(int i = 0; i < NUM_BROWSE_PATH; i++)
    {
        CUtf8String szBuffUtf8;

        char szPathTag[32];
        
        ZeroMemory(szPathTag, sizeof(szPathTag));
        sprintf(szPathTag, "Path_%02d", i);

        path = new TiXmlElement(szPathTag);  
        path->LinkEndChild(new TiXmlText(szBuffUtf8.Create(this->szBrowsePath[i])));  
        browse->LinkEndChild(path); 
        szBuffUtf8.Clear();
    }

    // root: Files
    TiXmlElement *filesNode = new TiXmlElement("Files");  
    root->LinkEndChild(filesNode);  
    int nFiles = this->m_LstInputFiles.GetItemCount();
    for(int i = 0; i < nFiles; i++)
    {
        // File
        TiXmlElement *file =  new TiXmlElement("File");  
        filesNode->LinkEndChild(file);

        CString szData[NUM_FILE_ATTRIBUTES];

        szData[0] = this->m_FileList.GetItemFilePath(i);
        szData[1] = (this->m_LstInputFiles.GetCheck(i) == TRUE) ? _T("true") : _T("false");
        szData[2] = this->m_FileList.GetItemFileName(i);
        szData[3] = this->m_FileList.GetItemInExt(i);
        szData[4].Format(_T("%I64d"), m_FileList.GetItemFileSize(i));
        szData[5] = this->m_FileList.GetItemOutExt(i);
        szData[6].Format(_T("%d"), m_FileList.GetItemOutPreset(i));
        szData[7] = this->m_LstInputFiles.GetItemText(i, 5);
        szData[8] = this->m_LstInputFiles.GetItemText(i, 6);

        for(int j = 0; j < NUM_FILE_ATTRIBUTES; j++)
        {
            CUtf8String szBuffUtf8;

            file->SetAttribute(g_szFileAttributes[j], szBuffUtf8.Create(szData[j]));
            szBuffUtf8.Clear();
        }
    }

    // save file
    ::UpdatePath();
    return doc.SaveFileW(szMainConfigFile);
}

void CBatchEncoderDlg::LoadUserSettings()
{
    if(bRunning == true)
        return;

    // NOTE: not recommended to use this function at normal usage
    // shortcut: Ctrl+Shift+L
    // load main settings from file

    CFileDialog fd(TRUE, _T("config"), _T(""), 
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER, 
        _T("Config Files (*.config)|*.config|Xml Files (*.xml)|*.xml|All Files|*.*||"), this);

    fd.m_ofn.lpstrInitialDir = ::GetExeFilePath();

    if(fd.DoModal() == IDOK)
    {
        CString szPath;
        szPath = fd.GetPathName();

        // store config filename to temp buffer
        CString szTmp = szMainConfigFile;

        // load settings from user file
        szMainConfigFile = szPath;
        if(this->LoadSettings() == false)
        {
            MessageBox(_T("Failed to load settings!"), 
                _T("ERROR"), 
                MB_OK | MB_ICONERROR);
        }

        // restore config filename from temp buffer
        szMainConfigFile = szTmp;
    }
}

void CBatchEncoderDlg::SaveUserSettings()
{
    if(bRunning == true)
        return;

    // NOTE: not recommended to use this function at normal usage
    // shortcut: Ctrl+Shift+S
    // save main settings to file

    CFileDialog fd(FALSE, _T("config"), _T(""), 
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER | OFN_OVERWRITEPROMPT, 
        _T("Config Files (*.config)|*.config|Xml Files (*.xml)|*.xml|All Files|*.*||"), this);

    fd.m_ofn.lpstrInitialDir = ::GetExeFilePath();

    if(fd.DoModal() == IDOK)
    {
        CString szPath;
        szPath = fd.GetPathName();

        // store config filename to temp buffer
        CString szTmp = szMainConfigFile;

        // load settings from user file
        szMainConfigFile = szPath;
        if(this->SaveSettings() == false)
        {
            MessageBox(_T("Failed to save settings!"), 
                _T("ERROR"), 
                MB_OK | MB_ICONERROR);
        }

        // restore config filename from temp buffer
        szMainConfigFile = szTmp;
    }
}

void CBatchEncoderDlg::LoadDefaultSettings()
{
    if(bRunning == true)
        return;

    // NOTE: not recommended to use this function at normal usage
    // shortcut: Ctrl+Shift+D
    // load default settings from program resources

    ::UpdatePath();

    szMainConfigFile = ::GetExeFilePath() + MAIN_APP_CONFIG;

    // delete default main config file
    ::DeleteFile(MAIN_APP_CONFIG);

    // delete default presets files
    for(int i = 0; i < NUM_PRESET_FILES; i++)
        ::DeleteFile(g_szPresetFiles[i]);

    // load settings from resources
    this->LoadSettings();
}

void CBatchEncoderDlg::OnBnClickedButtonConvert()
{
    /*
    HANDLE hProcess = ::GetCurrentProcess();
    DWORD_PTR dwProcessAffinityMask;
    DWORD_PTR dwSystemAffinityMask;
    int nProcessorsAvail = 1;

    // get number of processors available for our enc/dec processes 
    // to run simultaneously on different processors for greater performance

    if(::GetProcessAffinityMask(hProcess, 
        &dwProcessAffinityMask, 
        &dwSystemAffinityMask) == TRUE)
    {
        switch(dwProcessAffinityMask)
        {
        case 2-1: nProcessorsAvail = 1; break;
        case 4-1: nProcessorsAvail = 2; break;
        case 8-1: nProcessorsAvail = 3; break;
        case 16-1: nProcessorsAvail = 4; break;
        case 32-1: nProcessorsAvail = 5; break;
        case 64-1: nProcessorsAvail = 6; break;
        case 128-1: nProcessorsAvail = 7; break;
        case 256-1: nProcessorsAvail = 8; break;
        // (2^n)-1, n=1..32
        default: nProcessorsAvail = 1; break;
        };
    }


    // select processor #1 for 1st process
    if(::SetProcessAffinityMask(hProcess, 2-1) == TRUE)
    {
        MessageBox(_T("#1 OK"));
    }

    // select processor #2 for 2nd process
    if(::SetProcessAffinityMask(hProcess, 4-1) == TRUE)
    {
        MessageBox(_T("#2 OK"));
    }
    */

    static volatile bool bSafeCheck = false;
    if(bSafeCheck == true)
        return;

    if(bRunning == false)
    {
        bSafeCheck = true;

        // get number of files to encode
        int nFiles = m_LstInputFiles.GetItemCount();
        if(nFiles <= 0)
        {
            // MessageBox(_T("Add files to Input Files List"), _T("WARNING"), MB_OK | MB_ICONWARNING);
            bSafeCheck = false;
            return;
        }

        // create full output path
        if(this->m_ChkOutPath.GetCheck() == BST_CHECKED)
        {
            CString szPath;

            this->m_EdtOutPath.GetWindowText(szPath);
            if(szPath.GetLength() > 0)
            {
                if(::MakeFullPath(szPath) == FALSE)
                {
                    MessageBox(_T("Unable to Create Output Path"), _T("ERROR"), MB_OK | MB_ICONERROR);
                    bSafeCheck = false;
                    return;
                }
            }
        }

        // check if forced console mode is enabled
        if(this->GetMenu()->GetMenuState(ID_OPTIONS_FORCECONSOLEWINDOW, MF_BYCOMMAND) == MF_CHECKED)
            this->bForceConsoleWindow = true;
        else
            this->bForceConsoleWindow = false;

        // create worker thread in background for processing
        // the argument for thread function is pointer to dialog
        dwThreadID = 0;
        hThread = ::CreateThread(NULL, 0, WorkThread, this, CREATE_SUSPENDED, &dwThreadID);
        if(hThread == NULL)
        {
            // ERROR
            MessageBox(_T("Fatal Error when Creating Thread"), _T("ERROR"), MB_OK | MB_ICONERROR);
            bSafeCheck = false;
            return;
        }

        this->EnableUserInterface(FALSE);

        VERIFY(m_StatusBar.SetText(_T(""), 1, 0));

        m_BtnConvert.SetWindowText(_T("S&top"));
        this->GetMenu()->ModifyMenu(ID_ACTION_CONVERT, MF_BYCOMMAND, ID_ACTION_CONVERT, _T("S&top\tF9"));

        nProgressCurrent = 0;
        bRunning = true;

        // wakeup worker thread
        ::ResumeThread(hThread);

        bSafeCheck = false;
    }
    else
    {
        bSafeCheck = true;

        // note that TerminateThread is not used
        // if you wan't do this i nasty way uncommnet
        // the line below but I do'nt recommend this
        // ::TerminateThread(hThread, 0);

        m_BtnConvert.SetWindowText(_T("Conve&rt"));
        this->GetMenu()->ModifyMenu(ID_ACTION_CONVERT, MF_BYCOMMAND, ID_ACTION_CONVERT, _T("Conve&rt\tF9"));

        this->EnableUserInterface(TRUE);

        bRunning = false;
        bSafeCheck = false;
    }
}

bool CBatchEncoderDlg::WorkerCallback(int nProgress, bool bFinished, bool bError, double fTime, int nIndex)
{
    if(bError == true)
    {
        // handle errors here
        m_LstInputFiles.SetItemText(nIndex, 5, _T("--:--")); // Time
        m_LstInputFiles.SetItemText(nIndex, 6, _T("Error")); // Status
        m_FileProgress.SetPos(0);

        bRunning = false;
        return bRunning;
    }

    if(bFinished == false)
    {
        if(nProgress != nProgressCurrent)
        {
            nProgressCurrent = nProgress;

            if(bIsCnvStatusVisible == true)
                this->m_CnvStatus.Draw(nProgress);

            m_FileProgress.SetPos(nProgress);

            this->ShowProgressTrayIcon(nProgress);
        }
    }

    if(bFinished == true)
    {
        // bRunning = false - this is set in the end of worker thread proc
        if(nProgress != 100)
        {
            m_LstInputFiles.SetItemText(nIndex, 5, _T("--:--")); // Time
            m_LstInputFiles.SetItemText(nIndex, 6, _T("Error")); // Status
            m_FileProgress.SetPos(0);
        }
        else
        {
            CString szTime = ::FormatTime(fTime, 1);

            m_LstInputFiles.SetItemText(nIndex, 5, szTime); // Time
            m_LstInputFiles.SetItemText(nIndex, 6, _T("Done")); // Status
        }
    }

    // on false the worker thread will stop
    return bRunning;
}

void CBatchEncoderDlg::HistogramCallback(PLAME_ENC_HISTOGRAM plehData)
{
    this->m_Histogram.Draw(plehData);
}

BOOL CBatchEncoderDlg::OnHelpInfo(HELPINFO* pHelpInfo)
{
    if(bRunning == true)
        return FALSE;

    // note that this is not used
    // return CResizeDialog::OnHelpInfo(pHelpInfo);

    return FALSE;
}

void CBatchEncoderDlg::OnOK()
{
    // CResizeDialog::OnOK();

    if(bRunning == true)
        return;
}

void CBatchEncoderDlg::OnCancel()
{
    // CResizeDialog::OnCancel();

    if(bRunning == true)
        return;
}

void CBatchEncoderDlg::OnClose()
{
    CResizeDialog::OnClose();

    if(bRunning == true)
        return;

    // TODO: 
    // - kill worker thread and any running commandline tool
    // - don't save settings on readonly media
    //   check if the path to exe is on read only media
    //   if true then do not save settings to disk

    if(this->GetMenu()->GetMenuState(ID_OPTIONS_DO_NOT_SAVE, MF_BYCOMMAND) != MF_CHECKED)
        this->SaveSettings();

    this->EnableTrayIcon(false);

    CResizeDialog::OnOK();
}

void CBatchEncoderDlg::OnDestroy()
{
    CResizeDialog::OnDestroy();

    if(bRunning == true)
        return;

    m_FileList.RemoveAllNodes();
}

void CBatchEncoderDlg::UpdateFormatAndPreset()
{
    int nFormat = this->m_CmbFormat.GetCurSel();
    int nPreset = this->m_CmbPresets.GetCurSel();
    int nCount = m_LstInputFiles.GetItemCount();
    if(nCount > 0)
    {
        // get number of selected files
        int nSelected = 0;
        for(int i = 0; i < nCount; i++)
        {
            // if selected then change output format and preset #
            if(m_LstInputFiles.GetItemState(i,  LVIS_SELECTED) == LVIS_SELECTED)
            {
                // output extension
                CString szOutExt = m_FileList.GetOutFormatExt(nFormat);
                this->m_FileList.SetItemOutExt(szOutExt, i);
                this->m_FileList.SetItemOutFormat(nFormat, i);
                this->m_LstInputFiles.SetItemText(i, 3, szOutExt);

                // output preset
                CString szPreset;
                szPreset.Format(_T("%d"), nPreset);
                this->m_FileList.SetItemOutPreset(nPreset, i);
                this->m_LstInputFiles.SetItemText(i, 4, szPreset);

                // update selected items counter
                nSelected++;
            }
        }

        // if none of the items was selected
        // then change output format and preset # for all items
        if(nSelected == 0)
        {
            for(int i = 0; i < nCount; i++)
            {
                // output extension
                CString szOutExt = m_FileList.GetOutFormatExt(nFormat);
                this->m_FileList.SetItemOutExt(szOutExt, i);
                this->m_FileList.SetItemOutFormat(nFormat, i);
                this->m_LstInputFiles.SetItemText(i, 3, szOutExt);

                // output preset
                CString szPreset;
                szPreset.Format(_T("%d"), nPreset);
                this->m_FileList.SetItemOutPreset(nPreset, i);
                this->m_LstInputFiles.SetItemText(i, 4, szPreset);
            }
        }
    }
}

void CBatchEncoderDlg::OnCbnSelchangeComboPresets()
{
    int nSelPresetIndex = this->m_CmbPresets.GetCurSel();
    int nSelIndex = this->m_CmbFormat.GetCurSel();

    nCurSel[nSelIndex] = nSelPresetIndex;

    this->UpdateFormatAndPreset();
}

void CBatchEncoderDlg::OnCbnSelchangeComboFormat()
{
    int nSelIndex = this->m_CmbFormat.GetCurSel();
    if(nSelIndex != -1)
    {
        // update presets combobox
        int nSelPresetIndex = this->m_CmbPresets.GetCurSel();
        int nSelFormatIndex = this->m_CmbFormat.GetCurSel();

        this->UpdateOutputComboBoxes(nSelFormatIndex, 0);

        // set current preset position per format
        this->m_CmbPresets.SetCurSel(nCurSel[nSelFormatIndex]);
    }

    this->UpdateFormatAndPreset();
}

void CBatchEncoderDlg::OnBnClickedButtonBrowsePath()
{
    if(bRunning == true)
        return;

    // browse for ouput directory for converted files
    LPMALLOC pMalloc;
    BROWSEINFO bi; 
    LPITEMIDLIST pidlDesktop;
    LPITEMIDLIST pidlBrowse;
    TCHAR *lpBuffer;

    CString szTmp;
    this->m_EdtOutPath.GetWindowText(szTmp);

    if(szTmp == szBrowsePath[4])
        szLastBrowse = szBrowsePath[4];
    else
        szLastBrowse = szTmp;

    if(SHGetMalloc(&pMalloc) == E_FAIL)
        return;

    if((lpBuffer = (TCHAR *) pMalloc->Alloc(MAX_PATH * 2)) == NULL) 
    {
        pMalloc->Release();
        return; 
    }

    if(!SUCCEEDED(::SHGetSpecialFolderLocation(this->GetSafeHwnd(), CSIDL_DESKTOP, &pidlDesktop)))
    { 
        pMalloc->Free(lpBuffer); 
        pMalloc->Release();
        return; 
    } 

    #ifndef BIF_NEWDIALOGSTYLE
        #define BIF_NEWDIALOGSTYLE 0x0040
    #endif

    bi.hwndOwner = this->GetSafeHwnd(); 
    bi.pidlRoot = pidlDesktop; 
    bi.pszDisplayName = lpBuffer; 
    bi.lpszTitle = _T("Output path for converted files:");
    bi.lpfn = NULL; 
    bi.lParam = 0; 
    bi.ulFlags = BIF_STATUSTEXT | BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.iImage = 0;
    bi.lpfn = ::BrowseCallbackOutPath;

    pidlBrowse = ::SHBrowseForFolder(&bi); 
    if(pidlBrowse != NULL)
    { 
        if(::SHGetPathFromIDList(pidlBrowse, lpBuffer))
        {
            szBrowsePath[4] = szLastBrowse;

            szLastBrowse.Format(_T("%s\0"), lpBuffer);
            m_EdtOutPath.SetWindowText(lpBuffer);
        }
        pMalloc->Free(pidlBrowse);
    } 

    pMalloc->Free(pidlDesktop); 
    pMalloc->Free(lpBuffer); 
    pMalloc->Release();
}

void CBatchEncoderDlg::OnBnClickedCheckOutPath()
{
    if(bRunning == true)
        return;

    bool bIsChecked = false;
    
    if(m_ChkOutPath.GetCheck() == BST_CHECKED)
        bIsChecked = true;

    if(bIsChecked == false)
    {
        if(bSameAsSourceEdit == true)
            this->OnEnSetFocusEditOutPath();

        m_BtnBrowse.EnableWindow(FALSE);
        m_EdtOutPath.EnableWindow(FALSE);
    }
    else
    {
        if(bSameAsSourceEdit == true)
            this->OnEnKillFocusEditOutPath();

        m_BtnBrowse.EnableWindow(TRUE);
        m_EdtOutPath.EnableWindow(TRUE);
    }
}

bool CBatchEncoderDlg::InsertToList(NewItemData &nid)
{
    // nAction:
    // 0 - Adding new item to memory FileList.
    // 1 - Adding new item to control FileList.
    // 2 - Adding new item to memory and control FileLists.

    if(((nid.nAction == 0) || (nid.nAction == 2)) && (nid.nItem == -1))
    {
        WIN32_FIND_DATA FindFileData;
        HANDLE hFind;
        ULARGE_INTEGER ulSize;
        ULONGLONG nFileSize;
        int nCurFormat;
        int nCurPreset;

        // check the file extensions
        if(CLListFiles::IsValidInFileExtension(nid.szFileName) == false)
            return false;

        // check user out extension
        if(nid.szOutExt.Compare(_T("")) != 0)
        {
            if(CLListFiles::IsValidOutExtension(nid.szOutExt) == false)
                return false;

            nCurFormat = m_FileList.GetOutFormatIndex(nid.szOutExt);
        }
        else
        {
            nCurFormat = this->m_CmbFormat.GetCurSel();
        }

        // get selected preset if there is no user preset
        if(nid.nPreset != -1)
            nCurPreset = nid.nPreset;
        else
            nCurPreset = this->m_CmbPresets.GetCurSel();

        // get file size (this also checks if file exists)
        hFind = ::FindFirstFile(nid.szFileName, &FindFileData);
        if(hFind == INVALID_HANDLE_VALUE) 
            return false;

        ::FindClose(hFind);

        ulSize.HighPart = FindFileData.nFileSizeHigh;
        ulSize.LowPart = FindFileData.nFileSizeLow;
        nFileSize = ulSize.QuadPart;

        // add new node to filelist
        nid.nItem = m_FileList.InsertNode(nid.szFileName, 
            nid.szName,
            nFileSize,
            nCurFormat,
            nCurPreset);
    }

    if(((nid.nAction == 1) || (nid.nAction == 2)) && (nid.nItem >= 0))
    {
        CString tmpBuf;
        LVITEM lvi;
        ZeroMemory(&lvi, sizeof(LVITEM));
        lvi.mask = LVIF_TEXT | LVIF_STATE;
        lvi.iItem = nid.nItem;

        // [Name] : file name
        lvi.pszText = (LPTSTR) (LPCTSTR) (m_FileList.GetItemFileName(nid.nItem));
        m_LstInputFiles.InsertItem(&lvi);
        m_LstInputFiles.SetItemData(nid.nItem, nid.nItem);

        // [Type] : intput extension 
        tmpBuf.Format(_T("%s"), m_FileList.GetItemInExt(nid.nItem));
        lvi.iSubItem = 1;
        lvi.pszText = (LPTSTR) (LPCTSTR) (tmpBuf);
        m_LstInputFiles.SetItemText(lvi.iItem, 1, lvi.pszText);
        
        // [Size (bytes)] : file size
        tmpBuf.Format(_T("%I64d"), m_FileList.GetItemFileSize(nid.nItem));
        lvi.iSubItem = 2;
        lvi.pszText = (LPTSTR) (LPCTSTR) (tmpBuf);
        m_LstInputFiles.SetItemText(lvi.iItem, 2, lvi.pszText);

        // [Output] : output extension
        tmpBuf.Format(_T("%s"), m_FileList.GetItemOutExt(nid.nItem));
        lvi.iSubItem = 3;
        lvi.pszText = (LPTSTR) (LPCTSTR) (tmpBuf);
        m_LstInputFiles.SetItemText(lvi.iItem, 3, lvi.pszText);

        // [Preset] : selected preset index
        tmpBuf.Format(_T("%d"), m_FileList.GetItemOutPreset(nid.nItem));
        lvi.iSubItem = 4;
        lvi.pszText = (LPTSTR) (LPCTSTR) (tmpBuf);
        m_LstInputFiles.SetItemText(lvi.iItem, 4, lvi.pszText);

        // [Time] : enc/dec convertion time
        tmpBuf.Format(_T("%s"), (nid.szTime.Compare(_T("")) == 0) ? _T("--:--") : nid.szTime);
        lvi.iSubItem = 5;
        lvi.pszText = (LPTSTR) (LPCTSTR) (tmpBuf);
        m_LstInputFiles.SetItemText(lvi.iItem, 5, lvi.pszText);

        // [Status] : enc/dec progress status
        tmpBuf.Format(_T("%s"), (nid.szStatus.Compare(_T("")) == 0) ? _T("Not Done") : nid.szStatus);
        lvi.iSubItem = 6;
        lvi.pszText = (LPTSTR) (LPCTSTR) (tmpBuf);
        m_LstInputFiles.SetItemText(lvi.iItem, 6, lvi.pszText);

        // set item CheckBox state
        m_LstInputFiles.SetCheck(nid.nItem, nid.bCheck);
    }

    return true;
}

void CBatchEncoderDlg::OnLvnKeydownListInputFiles(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);

    switch(pLVKeyDow->wVKey)
    {
    case VK_INSERT: 
        this->OnEditCrop(); 
        break;
    case VK_DELETE: 
        this->OnEditRemove(); 
        break;
    default: break;
    };

    *pResult = 0;
}

void CBatchEncoderDlg::SearchFolderForFiles(CString szFile,
                                        const bool bRecurse,
                                        const TCHAR *szOutExt,
                                        const int nPreset)
{
    try
    {
        WIN32_FIND_DATA w32FileData;  
        HANDLE hSearch = NULL; 
        BOOL fFinished = FALSE;
        TCHAR cTempBuf[(MAX_PATH * 2) + 1];

        ZeroMemory(&w32FileData, sizeof(WIN32_FIND_DATA));
        ZeroMemory(cTempBuf, MAX_PATH * 2);

        // remove '\' or '/' from end of search path
        szFile.TrimRight(_T("\\"));
        szFile.TrimRight(_T("/"));

        wsprintf(cTempBuf, _T("%s\\*.*\0"), szFile);

        hSearch = FindFirstFile(cTempBuf, &w32FileData); 
        if(hSearch == INVALID_HANDLE_VALUE) 
            return;

        while(fFinished == FALSE) 
        { 
            if(w32FileData.cFileName[0] != '.' &&
                !(w32FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {  
                CString szTempBuf;
                szTempBuf.Format(_T("%s\\%s\0"), szFile, w32FileData.cFileName);

                NewItemData nid;
                ::InitNewItemData(nid);

                if((szOutExt != NULL) && (nPreset != -1))
                {
                    nid.nAction = 0;
                    nid.szFileName = szTempBuf; 
                    nid.nItem = -1;
                    nid.szName = _T("");
                    nid.szOutExt = szOutExt;
                    nid.nPreset = nPreset;

                    this->InsertToList(nid);
                }
                else
                {
                    nid.nAction = 2;
                    nid.szFileName = szTempBuf; 
                    nid.nItem = -1;

                    this->InsertToList(nid);
                }
            }

            if(w32FileData.cFileName[0] != '.' &&
                w32FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                wsprintf(cTempBuf, _T("%s\\%s\0"), szFile, w32FileData.cFileName);

                // recurse subdirs
                if(bRecurse == true)
                    this->SearchFolderForFiles(cTempBuf, true, szOutExt, nPreset);
            }

            if(FindNextFile(hSearch, &w32FileData) == FALSE) 
            {
                if(GetLastError() == ERROR_NO_MORE_FILES) 
                    fFinished = TRUE; 
                else 
                    return;
            }
        }

        if(FindClose(hSearch) == FALSE) 
            return;
    }
    catch(...)
    {
        MessageBox(_T("Error while searching for files!"), _T("ERROR"), MB_OK | MB_ICONERROR);
    }
}

void CBatchEncoderDlg::HandleDropFiles(HDROP hDropInfo)
{
    int nCount = ::DragQueryFile(hDropInfo, (UINT) 0xFFFFFFFF, NULL, 0);
    if(nCount > 0)
    {
        NewItemData nid;
        ::InitNewItemData(nid);

        for(int i = 0; i < nCount; i++)
        {
            int nReqChars = ::DragQueryFile(hDropInfo, i, NULL, 0);

            CString szFile;
            ::DragQueryFile(hDropInfo, 
                i, 
                szFile.GetBuffer(nReqChars * 2 + 8), 
                nReqChars * 2 + 8);
            if(::GetFileAttributes(szFile) & FILE_ATTRIBUTE_DIRECTORY)
            {
                // insert droped files in directory and subdirs
                this->SearchFolderForFiles(szFile, true);
            }
            else
            {
                // insert droped files
                nid.nAction = 2;
                nid.szFileName = szFile; 
                nid.nItem = -1;

                this->InsertToList(nid);
            }

            szFile.ReleaseBuffer();
        }

        this->UpdateStatusBar();
    }

    ::DragFinish(hDropInfo);
}

void CBatchEncoderDlg::OnDropFiles(HDROP hDropInfo)
{
    if(bRunning == true)
        return;

    // TODO: add wait dialog here (wait for hDDThread object)
    if(bHandleDrop == true)
    {
        bHandleDrop = false;

        m_DDParam.pDlg = this;
        m_DDParam.hDropInfo = hDropInfo;

        hDDThread = ::CreateThread(NULL, 0, DragAndDropThread, (LPVOID) &m_DDParam, 0, &dwDDThreadID);
        if(hDDThread == NULL)
            bHandleDrop = true;
    }

    // NOTE: under Win9x this does not work, we use seperate thread to handle drop
    // this->HandleDropFiles(hDropInfo);

    CResizeDialog::OnDropFiles(hDropInfo);
}

void CBatchEncoderDlg::OnNMRclickListInputFiles(NMHDR *pNMHDR, LRESULT *pResult)
{
    // right click contextmenu
    POINT point;
    GetCursorPos(&point);

    CMenu *subMenu = this->GetMenu()->GetSubMenu(1);

    subMenu->TrackPopupMenu(0, point.x, point.y, this, NULL);

    *pResult = 0;
}

void CBatchEncoderDlg::OnViewStartWithExtendedProgress()
{
    if(bRunning == true)
        return;

    if(this->GetMenu()->GetMenuState(ID_VIEW_STARTWITHEXTENDEDPROGRESS, MF_BYCOMMAND) == MF_CHECKED)
        this->GetMenu()->CheckMenuItem(ID_VIEW_STARTWITHEXTENDEDPROGRESS, MF_UNCHECKED);
    else
        this->GetMenu()->CheckMenuItem(ID_VIEW_STARTWITHEXTENDEDPROGRESS, MF_CHECKED);
}

void CBatchEncoderDlg::OnViewToogleExtendedProgress()
{
    this->OnShowCnvStatus();
}

void CBatchEncoderDlg::OnViewToogleHistogramWindow()
{
    this->OnShowHistogram();
}

void CBatchEncoderDlg::OnViewShowGridLines()
{
    if(bRunning == true)
        return;

    if(this->GetMenu()->GetMenuState(ID_VIEW_SHOWGRIDLINES, MF_BYCOMMAND) == MF_CHECKED)
        ShowGridlines(false);
    else
        ShowGridlines(true);
}

void CBatchEncoderDlg::OnNMDblclkListInputFiles(NMHDR *pNMHDR, LRESULT *pResult)
{
    POSITION pos = m_LstInputFiles.GetFirstSelectedItemPosition();
    if(pos != NULL)
    {
        /*
        int nItem = m_LstInputFiles.GetNextSelectedItem(pos);
        CString szPath = this->m_FileList.GetItemFilePath(nItem);

        ::LaunchAndWait(szPath, _T(""), FALSE);
        */
    }

    *pResult = 0;
}

void CBatchEncoderDlg::OnLvnItemchangedListInputFiles(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    // get item format and preset and setup ComboBoxes
    int nSelCount = this->m_LstInputFiles.GetSelectedCount();
    if(nSelCount == 1)
    {
        POSITION pos = m_LstInputFiles.GetFirstSelectedItemPosition();
        if(pos != NULL)
        {
            int nItem = this->m_LstInputFiles.GetNextSelectedItem(pos);

            // check if we have such item in our filelist
            if(nItem < this->m_FileList.GetSize())
            {
                int nSelFormatIndex = this->m_FileList.GetItemOutFormat(nItem);
                int nSelPresetIndex = this->m_FileList.GetItemOutPreset(nItem);

                // load presets only if format is changing
                if(this->m_CmbFormat.GetCurSel() == nSelFormatIndex)
                {
                    this->m_CmbPresets.SetCurSel(nSelPresetIndex);
                }
                else
                {
                    this->m_CmbFormat.SetCurSel(nSelFormatIndex);
                    this->m_CmbPresets.SetCurSel(nSelPresetIndex);

                    this->UpdateOutputComboBoxes(nSelFormatIndex, nSelPresetIndex);
                }
            }
        }
    }

    /*
    if(pNMLV->uChanged == LVIF_STATE)
        this->UpdateStatusBar();
    */

    *pResult = 0;
}

void CBatchEncoderDlg::ResetOutput()
{
    this->UpdateFormatAndPreset();
}

void CBatchEncoderDlg::ResetConvertionTime()
{
    int nCount = m_LstInputFiles.GetItemCount();
    if(nCount > 0)
    {
        int nSelected = 0;
        for(int i = 0; i < nCount; i++)
        {
            if(m_LstInputFiles.GetItemState(i,  LVIS_SELECTED) == LVIS_SELECTED)
            {
                this->m_LstInputFiles.SetItemText(i, 5, _T("--:--"));
                nSelected++;
            }
        }

        if(nSelected == 0)
        {
            for(int i = 0; i < nCount; i++)
                this->m_LstInputFiles.SetItemText(i, 5, _T("--:--"));
        }
    }
}

void CBatchEncoderDlg::ResetConvertionStatus()
{
    int nCount = m_LstInputFiles.GetItemCount();
    if(nCount > 0)
    {
        int nSelected = 0;
        for(int i = 0; i < nCount; i++)
        {
            if(m_LstInputFiles.GetItemState(i,  LVIS_SELECTED) == LVIS_SELECTED)
            {
                this->m_LstInputFiles.SetItemText(i, 6, _T("Not Done"));
                nSelected++;
            }
        }

        if(nSelected == 0)
        {
            for(int i = 0; i < nCount; i++)
                this->m_LstInputFiles.SetItemText(i, 6, _T("Not Done"));
        }
    }
}

void CBatchEncoderDlg::EnableUserInterface(BOOL bEnable)
{
    // check if we are statting with extended progress windows
    bool bShowAdvancedSatus = false;
    if(this->GetMenu()->GetMenuState(ID_VIEW_STARTWITHEXTENDEDPROGRESS, MF_BYCOMMAND) == MF_CHECKED)
        bShowAdvancedSatus = true;

    if(bEnable == FALSE)
    {
        if(this->bForceConsoleWindow == false)
        {
            this->m_Histogram.ShowWindow(SW_HIDE);
            this->bIsHistogramVisible = false;

            this->m_CnvStatus.Erase(true);

            if(bShowAdvancedSatus == true)
            {
                this->m_LstInputFiles.ShowWindow(SW_HIDE);

                this->m_CnvStatus.ShowWindow(SW_SHOW);
                this->bIsCnvStatusVisible = true;

                this->GetMenu()->CheckMenuItem(ID_VIEW_TOOGLEEXTENDEDPROGRESS, MF_CHECKED);
            }
            else
            {
                this->m_CnvStatus.ShowWindow(SW_HIDE);
                this->bIsCnvStatusVisible = false;

                this->m_ChkOutPath.ShowWindow(SW_HIDE);
                this->m_EdtOutPath.ShowWindow(SW_HIDE);
                this->m_BtnBrowse.ShowWindow(SW_HIDE);

                this->m_FileProgress.ShowWindow(SW_SHOW);
            }
        }
    }
    else
    {
        if(this->bForceConsoleWindow == false)
        {
            this->m_FileProgress.ShowWindow(SW_HIDE);
            this->m_ChkOutPath.ShowWindow(SW_SHOW);
            this->m_EdtOutPath.ShowWindow(SW_SHOW);
            this->m_BtnBrowse.ShowWindow(SW_SHOW);

            this->m_CnvStatus.Erase(true);

            this->m_CnvStatus.ShowWindow(SW_HIDE);
            this->bIsCnvStatusVisible = false;

            this->m_Histogram.ShowWindow(SW_HIDE);
            this->bIsHistogramVisible = false;

            this->m_LstInputFiles.ShowWindow(SW_SHOW);
        }
    }

    CMenu* pSysMenu = GetSystemMenu(FALSE);

    // enable/disable close button while convertion process
    if(bEnable == FALSE)
        pSysMenu->EnableMenuItem(SC_CLOSE, MF_GRAYED);
    else
        pSysMenu->EnableMenuItem(SC_CLOSE, MF_ENABLED);

    // enable/disable all main menu items
    UINT nEnable = (bEnable == TRUE) ? MF_ENABLED : MF_GRAYED;
    CMenu *pMainMenu = this->GetMenu();
    UINT nItems = pMainMenu->GetMenuItemCount();
    for(UINT i = 0; i < nItems; i++)
    {
        // skip only Action menu items
        if(i == 3)
            continue;

        CMenu *pSubMenu = pMainMenu->GetSubMenu(i);
        UINT nSubItems = pSubMenu->GetMenuItemCount();
        for(UINT j = 0; j < nSubItems; j++)
        {
            UINT nID = pSubMenu->GetMenuItemID(j);
            pSubMenu->EnableMenuItem(nID, nEnable);
        }
    }

    // enable/disable main dialog items
    this->m_CmbPresets.EnableWindow(bEnable);
    this->m_CmbFormat.EnableWindow(bEnable);
    this->m_LstInputFiles.EnableWindow(bEnable);

    if(this->m_ChkOutPath.GetCheck() == BST_CHECKED)
        this->m_EdtOutPath.EnableWindow(bEnable);

    if(this->m_ChkOutPath.GetCheck() == BST_CHECKED)
        this->m_BtnBrowse.EnableWindow(bEnable);

    this->m_ChkOutPath.EnableWindow(bEnable);

    // enable or disable window toogle items
    this->GetMenu()->EnableMenuItem(ID_VIEW_TOOGLEEXTENDEDPROGRESS, 
        (bEnable == FALSE) ? MF_ENABLED : MF_GRAYED);

    this->GetMenu()->EnableMenuItem(ID_VIEW_TOOGLEHISTOGRAMWINDOW, 
        (bEnable == FALSE) ? MF_ENABLED : MF_GRAYED);

    if(bEnable == TRUE)
    {
        this->GetMenu()->CheckMenuItem(ID_VIEW_TOOGLEHISTOGRAMWINDOW, MF_UNCHECKED);
        this->GetMenu()->CheckMenuItem(ID_VIEW_TOOGLEEXTENDEDPROGRESS, MF_UNCHECKED);
    }
}

void CBatchEncoderDlg::OnEnChangeEditOutPath()
{
    // CString szPath;
    // m_EdtOutPath.GetWindowText(szPath);
}

void CBatchEncoderDlg::OnEnSetFocusEditOutPath()
{
    // TODO: add option in Adv dialog to disable this type of behaviour
    if(bSameAsSourceEdit == true)
    {
        CString szPath;
        m_EdtOutPath.GetWindowText(szPath);
        if(szPath.CompareNoCase(_T("<< same as source file >>")) == 0)
            m_EdtOutPath.SetWindowText(_T(""));
    }
}

void CBatchEncoderDlg::OnEnKillFocusEditOutPath()
{
    if(bSameAsSourceEdit == true)
    {
        CString szPath;
        m_EdtOutPath.GetWindowText(szPath);
        if(szPath.CompareNoCase(_T("")) == 0)
            m_EdtOutPath.SetWindowText(_T("<< same as source file >>"));
    }
}

bool CBatchEncoderDlg::LoadList(CString szFileXml, bool bAddToListCtrl)
{
    CTiXmlDocumentW doc;
    if(doc.LoadFileW(szFileXml) == true)
    {
        TiXmlHandle hDoc(&doc);
        TiXmlElement* pElem;

        TiXmlHandle hRoot(0);

        // root: Files
        pElem = hDoc.FirstChildElement().Element();
        if(!pElem) 
        {
            // MessageBox(_T("Failed to load file!"), _T("ERROR"), MB_OK | MB_ICONERROR);
            return false;
        }

        hRoot = TiXmlHandle(pElem);

        // check for "Files"
        const char *pszRoot = pElem->Value(); 
        const char *pszRootName = "Files";
        if(strcmp(pszRootName, pszRoot) != 0)
        {
            // MessageBox(_T("Failed to load file!"), _T("ERROR"), MB_OK | MB_ICONERROR);
            return false;
        }

        // clear the list
        if(bAddToListCtrl == true)
            this->OnEditClear();

        // File
        NewItemData nid;
        ::InitNewItemData(nid);

        TiXmlElement* pFilesNode = hRoot.FirstChild("File").Element();
        for(pFilesNode; pFilesNode; pFilesNode = pFilesNode->NextSiblingElement())
        {
            char *pszAttrib[NUM_FILE_ATTRIBUTES];
            for(int i = 0; i < NUM_FILE_ATTRIBUTES; i++)
                pszAttrib[i] = (char *) pFilesNode->Attribute(g_szFileAttributes[i]);

            bool bValidFile = true;
            for(int i = 0; i < NUM_FILE_ATTRIBUTES; i++)
            {
                if(pszAttrib[i] == NULL)
                {
                    bValidFile = false;
                    break;
                }
            }

            if(bValidFile == true)
            {
                CString szData[NUM_FILE_ATTRIBUTES];
                for(int i = 0; i < NUM_FILE_ATTRIBUTES; i++)
                    szData[i] = GetConfigString(pszAttrib[i]);

                int nAction = 2;
                if(bAddToListCtrl == false)
                    nAction = 0;

                nid.nAction = nAction;
                nid.szFileName = szData[0]; 
                nid.nItem = -1;
                nid.szName = szData[2];
                nid.szOutExt = szData[5];
                nid.nPreset = stoi(szData[6]);
                nid.bCheck = (szData[1].Compare(_T("true")) == 0) ? TRUE : FALSE;
                nid.szTime = szData[7];
                nid.szStatus = szData[8];

                this->InsertToList(nid);
            }
        }
        return true;
    }

    return false;
}

bool CBatchEncoderDlg::SaveList(CString szFileXml, bool bUseListCtrl)
{
    CTiXmlDocumentW doc;
    TiXmlDeclaration* decl = new TiXmlDeclaration("1.0", "UTF-8", "");
    doc.LinkEndChild(decl);  

    CString szBuff;
    CUtf8String m_Utf8;

    // root: Files
    TiXmlElement *filesNode = new TiXmlElement("Files");  
    doc.LinkEndChild(filesNode); 

    int nFiles = 0;
    if(bUseListCtrl == true)
        nFiles = this->m_LstInputFiles.GetItemCount();
    else
        nFiles = this->m_FileList.GetSize();

    for(int i = 0; i < nFiles; i++)
    {
        // File
        TiXmlElement *file =  new TiXmlElement("File");  
        filesNode->LinkEndChild(file);

        CString szData[NUM_FILE_ATTRIBUTES];

        // get all file entry data
        szData[0] = this->m_FileList.GetItemFilePath(i);

        if(bUseListCtrl == true)
            szData[1] = (this->m_LstInputFiles.GetCheck(i) == TRUE) ? _T("true") : _T("false");
        else
            szData[1] = _T("true");

        szData[2] = this->m_FileList.GetItemFileName(i);
        szData[3] = this->m_FileList.GetItemInExt(i);
        szData[4].Format(_T("%I64d"), m_FileList.GetItemFileSize(i));
        szData[5] = this->m_FileList.GetItemOutExt(i);
        szData[6].Format(_T("%d"), m_FileList.GetItemOutPreset(i));

        if(bUseListCtrl == true)
        {
            szData[7] = this->m_LstInputFiles.GetItemText(i, 5);
            szData[8] = this->m_LstInputFiles.GetItemText(i, 6);
        }
        else
        {
            szData[7] = _T("--:--");
            szData[8] = _T("Not Done");
        }

        for(int j = 0; j < NUM_FILE_ATTRIBUTES; j++)
        {
            CUtf8String szBuffUtf8;

            file->SetAttribute(g_szFileAttributes[j], szBuffUtf8.Create(szData[j]));
            szBuffUtf8.Clear();
        }
    }

    if(doc.SaveFileW(szFileXml) != true)
        return false;
    else
        return true;
}

void CBatchEncoderDlg::OnFileLoadList()
{
    if(bRunning == true)
        return;

    CFileDialog fd(TRUE, _T("list"), _T(""), 
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER, 
        _T("List Files (*.list)|*.list|Xml Files (*.xml)|*.xml|All Files|*.*||"), this);

    ::SetBrowsePath(fd, szBrowsePath[0]);

    if(fd.DoModal() == IDOK)
    {
        szBrowsePath[0] = ::GetBrowsePath(fd);

        CString szFileXml = fd.GetPathName();

        if(this->LoadList(szFileXml, true) == false)
        {
            MessageBox(_T("Failed to load file!"), 
                _T("ERROR"), 
                MB_OK | MB_ICONERROR);
        }
        else
        {
            this->UpdateStatusBar();
        }
    }
}

void CBatchEncoderDlg::OnFileSaveList()
{
    if(bRunning == true)
        return;

    CFileDialog fd(FALSE, _T("list"), _T(""), 
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER | OFN_OVERWRITEPROMPT, 
        _T("List Files (*.list)|*.list|Xml Files (*.xml)|*.xml|All Files|*.*||"), this);

    ::SetBrowsePath(fd, szBrowsePath[1]);

    if(fd.DoModal() == IDOK)
    {
        szBrowsePath[1] = ::GetBrowsePath(fd);

        CString szFileXml = fd.GetPathName();

        if(this->SaveList(szFileXml, true) == false)
        {
            MessageBox(_T("Failed to save file!"), 
                _T("ERROR"), 
                MB_OK | MB_ICONERROR);
        }
    }
}

void CBatchEncoderDlg::OnFileClearList()
{
    if(bRunning == true)
        return;

    this->OnEditClear();
}

void CBatchEncoderDlg::OnFileCreateBatchFile()
{
    if(bRunning == true)
        return;

    CString szInitName;
    SYSTEMTIME st;

    ::GetLocalTime(&st);
    szInitName.Format(_T("convert_%04d%02d%02d_%02d%02d%02d%03d"),
        st.wYear, st.wMonth, st.wDay, 
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    CFileDialog fd(FALSE, _T("bat"), szInitName, 
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER | OFN_OVERWRITEPROMPT, 
        _T("Batch Files (*.bat)|*.bat|Script Files (*.cmd)|*.cmd|All Files|*.*||"), this);

    // TODO: add szBrowsePath[?] entry

    // ::SetBrowsePath(fd, szBrowsePath[?]);

    if(fd.DoModal() == IDOK)
    {
        // szBrowsePath[?] = ::GetBrowsePath(fd);

        CString szFileBatch = fd.GetPathName();

        if(this->CreateBatchFile(szFileBatch, true) == false)
        {
            MessageBox(_T("Failed to create batch-file!"), 
                _T("ERROR"), 
                MB_OK | MB_ICONERROR);
        }
    }
}

void CBatchEncoderDlg::OnFileExit()
{
    if(bRunning == true)
        return;

    if(this->bRunning == false)
        this->OnClose();
}

void CBatchEncoderDlg::OnEditAddFiles()
{
    if(bRunning == true)
        return;

    // buffer for selected files
    TCHAR *pFiles = NULL;
    const DWORD dwMaxSize = (4096 * MAX_PATH);
    try
    {
        // allocate emmory for file buffer
        pFiles = (TCHAR *) malloc(dwMaxSize);
        if(pFiles == NULL)
        {
            MessageBox(_T("Failed to allocate memory for filenames buffer!"), 
                _T("ERROR"), 
                MB_OK | MB_ICONERROR);
            return;
        }

        // zero memory
        ZeroMemory(pFiles, dwMaxSize);

        // init File Open dialog
        CFileDialog fd(TRUE, 
            _T(""), 
            0, 
            OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY | OFN_ENABLESIZING, 
            AUDIO_FILES_FILTER, 
            this);

        // use big buffer 
        fd.m_ofn.lpstrFile = pFiles;
        fd.m_ofn.nMaxFile = (dwMaxSize) / 2;

        ::SetBrowsePath(fd, szBrowsePath[2]);

        // show File Open dialog
        if(fd.DoModal() != IDCANCEL)
        {
            szBrowsePath[2] = ::GetBrowsePath(fd);

            CString sFilePath;
            POSITION pos = fd.GetStartPosition();

            NewItemData nid;
            ::InitNewItemData(nid);

            // insert all files to list
            do
            {
                sFilePath = fd.GetNextPathName(pos);
                if(!sFilePath.IsEmpty())
                {
                    nid.nAction = 2;
                    nid.szFileName = sFilePath; 
                    nid.nItem = -1;

                    this->InsertToList(nid);
                }
            }
            while(pos != NULL);

            this->UpdateStatusBar();
        }
    }
    catch(...)
    {
        // free memory buffer on error
        if(pFiles != NULL)
        {
            free(pFiles);
            pFiles = NULL;
        }
    }

    // free memory buffer
    if(pFiles != NULL)
    {
        free(pFiles);
        pFiles = NULL;
    }
}

void CBatchEncoderDlg::OnEditAddDir()
{
    if(bRunning == true)
        return;

    LPMALLOC pMalloc;
    BROWSEINFO bi; 
    LPITEMIDLIST pidlDesktop;
    LPITEMIDLIST pidlBrowse;
    TCHAR *lpBuffer;

    szLastBrowseAddDir = szBrowsePath[3];

    if(SHGetMalloc(&pMalloc) == E_FAIL)
        return;

    if((lpBuffer = (TCHAR *) pMalloc->Alloc(MAX_PATH * 2)) == NULL) 
    {
        pMalloc->Release();
        return; 
    }

    if(!SUCCEEDED(::SHGetSpecialFolderLocation(this->GetSafeHwnd(), CSIDL_DESKTOP, &pidlDesktop)))
    { 
        pMalloc->Free(lpBuffer); 
        pMalloc->Release();
        return; 
    } 

    #ifndef BIF_NEWDIALOGSTYLE
        #define BIF_NEWDIALOGSTYLE 0x0040
    #endif

    bi.hwndOwner = this->GetSafeHwnd(); 
    bi.pidlRoot = pidlDesktop; 
    bi.pszDisplayName = lpBuffer; 
    bi.lpszTitle = _T("Select folder with Audio Files:");
    bi.lpfn = NULL; 
    bi.lParam = 0; 
    bi.ulFlags = BIF_STATUSTEXT | BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.iImage = 0;
    bi.lpfn = ::BrowseCallbackAddDir;

    pidlBrowse = ::SHBrowseForFolder(&bi); 
    if(pidlBrowse != NULL)
    { 
        if(::SHGetPathFromIDList(pidlBrowse, lpBuffer))
        {
            CString szPath = lpBuffer;

            szBrowsePath[3] = szPath;

            this->SearchFolderForFiles(szPath, bRecurseChecked);

            this->UpdateStatusBar();
        }
        pMalloc->Free(pidlBrowse);
    } 

    pMalloc->Free(pidlDesktop); 
    pMalloc->Free(lpBuffer); 
    pMalloc->Release();
}

void CBatchEncoderDlg::OnEditClear()
{
    if(bRunning == true)
        return;

    // clear node list
    m_FileList.RemoveAllNodes();

    // clear list view
    m_LstInputFiles.DeleteAllItems();

    this->UpdateStatusBar();
}

void CBatchEncoderDlg::OnEditRemoveChecked()
{
    if(bRunning == true)
        return;

    // remove all checked items
    int nItems = m_LstInputFiles.GetItemCount();
    if(nItems <= 0)
        return;
    
    for(int i = (nItems - 1); i >= 0; i--)
    {
        if(m_LstInputFiles.GetCheck(i) == TRUE)
        {
            m_FileList.RemoveNode(i);
            m_LstInputFiles.DeleteItem(i);
        }
    }

    if(m_LstInputFiles.GetItemCount() == 0)
    {
        m_FileList.RemoveAllNodes();
        m_LstInputFiles.DeleteAllItems();
    }

    // NOTE: check all items
    /*
    int nCount = m_LstInputFiles.GetItemCount();
    if(nCount > 0)
    {
        for(int i = 0; i < nCount; i++)
            m_LstInputFiles.SetCheck(i, TRUE);
    }
    */

    this->UpdateStatusBar();
}

void CBatchEncoderDlg::OnEditRemoveUnchecked()
{
    if(bRunning == true)
        return;

    // remove all unchecked items
    int nItems = m_LstInputFiles.GetItemCount();
    if(nItems <= 0)
        return;
    
    for(int i = (nItems - 1); i >= 0; i--)
    {
        if(m_LstInputFiles.GetCheck(i) == FALSE)
        {
            m_FileList.RemoveNode(i);
            m_LstInputFiles.DeleteItem(i);
        }
    }

    if(m_LstInputFiles.GetItemCount() == 0)
    {
        m_FileList.RemoveAllNodes();
        m_LstInputFiles.DeleteAllItems();
    }

    // NOTE: uncheck all items
    /*
    int nCount = m_LstInputFiles.GetItemCount();
    if(nCount > 0)
    {
        for(int i = 0; i < nCount; i++)
            m_LstInputFiles.SetCheck(i, FALSE);
    }
    */

    this->UpdateStatusBar();
}

void CBatchEncoderDlg::OnEditCheckSelected()
{
    if(bRunning == true)
        return;

    int nCount = m_LstInputFiles.GetItemCount();
    if(nCount > 0)
    {
        for(int i = 0; i < nCount; i++)
        {
            if(m_LstInputFiles.GetItemState(i,  LVIS_SELECTED) == LVIS_SELECTED)
                m_LstInputFiles.SetCheck(i, TRUE);
        }
    }
}

void CBatchEncoderDlg::OnEditUncheckSelected()
{
    if(bRunning == true)
        return;

    int nCount = m_LstInputFiles.GetItemCount();
    if(nCount > 0)
    {
        for(int i = 0; i < nCount; i++)
        {
            if(m_LstInputFiles.GetItemState(i,  LVIS_SELECTED) == LVIS_SELECTED)
                m_LstInputFiles.SetCheck(i, FALSE);
        }
    }

    this->UpdateStatusBar();
}

void CBatchEncoderDlg::OnEditRename()
{
    if(bRunning == true)
        return;

    // check if control has keyboard focus
    if(m_LstInputFiles.GetFocus()->GetSafeHwnd() != m_LstInputFiles.GetSafeHwnd())
        return;

    POSITION pos = m_LstInputFiles.GetFirstSelectedItemPosition();
    if(pos != NULL)
    {
        this->m_LstInputFiles.SetFocus();
        int nItem = m_LstInputFiles.GetNextSelectedItem(pos);
        this->m_LstInputFiles.EditLabel(nItem);
    }
}

void CBatchEncoderDlg::OnEditOpen()
{
    if(bRunning == true)
        return;

    POSITION pos = m_LstInputFiles.GetFirstSelectedItemPosition();
    if(pos != NULL)
    {
        int nItem = m_LstInputFiles.GetNextSelectedItem(pos);
        CString szPath = this->m_FileList.GetItemFilePath(nItem);

        ::LaunchAndWait(szPath, _T(""), FALSE);
    }
}

void CBatchEncoderDlg::OnEditExplore()
{
    if(bRunning == true)
        return;

    POSITION pos = m_LstInputFiles.GetFirstSelectedItemPosition();
    if(pos != NULL)
    {
        int nItem = m_LstInputFiles.GetNextSelectedItem(pos);
        CString szPath = this->m_FileList.GetItemFilePath(nItem);

        CString szName = this->m_FileList.GetFileName(szPath);
        szPath.TrimRight(szName);

        ::LaunchAndWait(szPath, _T(""), FALSE);
    }
}

void CBatchEncoderDlg::OnEditCrop()
{
    if(bRunning == true)
        return;

    // invert selection
    int nFiles = m_LstInputFiles.GetItemCount();
    for(int i = 0; i < nFiles; i++)
    {
        if(m_LstInputFiles.GetItemState(i, LVIS_SELECTED) == LVIS_SELECTED)
            m_LstInputFiles.SetItemState(i, 0, LVIS_SELECTED);
        else
            m_LstInputFiles.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
    }

    // now delete selected
    int nItem = -1;
    do
    {
        nItem = m_LstInputFiles.GetNextItem(-1, LVIS_SELECTED);
        if(nItem != -1)
        {            
            m_FileList.RemoveNode(nItem);
            m_LstInputFiles.DeleteItem(nItem);
        }
    }
    while(nItem != -1);

    if(m_LstInputFiles.GetItemCount() == 0)
    {
        m_FileList.RemoveAllNodes();
        m_LstInputFiles.DeleteAllItems();
    }

    this->UpdateStatusBar();
}

void CBatchEncoderDlg::OnEditSelectNone()
{
    if(bRunning == true)
        return;

    m_LstInputFiles.SetItemState(-1,  0, LVIS_SELECTED);
}

void CBatchEncoderDlg::OnEditInvertSelection()
{
    if(bRunning == true)
        return;

    int nCount = m_LstInputFiles.GetItemCount();
    if(nCount > 0)
    {
        for(int i = 0; i < nCount; i++)
        {
            if(m_LstInputFiles.GetItemState(i,  LVIS_SELECTED) == LVIS_SELECTED)
                m_LstInputFiles.SetItemState(i,  0, LVIS_SELECTED);
            else
                m_LstInputFiles.SetItemState(i,  LVIS_SELECTED, LVIS_SELECTED);
        }
    }
}

void CBatchEncoderDlg::OnEditRemove()
{
    if(bRunning == true)
        return;

    int nItem = -1;
    int nItemLastRemoved = -1;
    do
    {
        nItem = m_LstInputFiles.GetNextItem(-1, LVIS_SELECTED);
        if(nItem != -1)
        {            
            m_FileList.RemoveNode(nItem);
            m_LstInputFiles.DeleteItem(nItem);

            nItemLastRemoved = nItem;
        }
    }
    while(nItem != -1);

    // select other item in list
    int nItems = m_LstInputFiles.GetItemCount();
    if(nItemLastRemoved != -1)
    {
        if(nItemLastRemoved < nItems && nItems >= 0)
            m_LstInputFiles.SetItemState(nItemLastRemoved, LVIS_SELECTED, LVIS_SELECTED);
        else if(nItemLastRemoved >= nItems && nItems >= 0)
            m_LstInputFiles.SetItemState(nItemLastRemoved - 1, LVIS_SELECTED, LVIS_SELECTED);
    }

    if(m_LstInputFiles.GetItemCount() == 0)
    {
        m_FileList.RemoveAllNodes();
        m_LstInputFiles.DeleteAllItems();
    }

    this->UpdateStatusBar();
}

void CBatchEncoderDlg::OnEditSelectAll()
{
    if(bRunning == true)
        return;

    m_LstInputFiles.SetItemState(-1,  LVIS_SELECTED, LVIS_SELECTED);
}

void CBatchEncoderDlg::OnEditResetOutput()
{
    if(bRunning == true)
        return;

    this->ResetOutput();
}

void CBatchEncoderDlg::OnEditResetTime()
{
    if(bRunning == true)
        return;

    // NOTE: when user pressed F3 reset StatusBar
    this->m_StatusBar.SetText(_T(""), 1, 0);

    this->ResetConvertionTime();
    this->ResetConvertionStatus();
}

void CBatchEncoderDlg::OnActionConvert()
{
    this->OnBnClickedButtonConvert();
}

void CBatchEncoderDlg::OnOptionsStayOnTop()
{
    if(bRunning == true)
        return;

    if(this->GetMenu()->GetMenuState(ID_OPTIONS_STAYONTOP, MF_BYCOMMAND) == MF_CHECKED)
    {
        this->SetWindowPos(CWnd::FromHandle(HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_STAYONTOP, MF_UNCHECKED);
    }
    else
    {
        this->SetWindowPos(CWnd::FromHandle(HWND_TOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_STAYONTOP, MF_CHECKED);
    }
}

void CBatchEncoderDlg::OnOptionsShowTrayIcon()
{
    if(bRunning == true)
        return;

    if(this->GetMenu()->GetMenuState(ID_OPTIONS_SHOWTRAYICON, MF_BYCOMMAND) == MF_CHECKED)
        this->EnableTrayIcon(false);
    else
        this->EnableTrayIcon(true);
}

void CBatchEncoderDlg::OnOptionsDeleteSourceFileWhenDone()
{
    if(bRunning == true)
        return;

    if(this->GetMenu()->GetMenuState(ID_OPTIONS_DELETESOURCEFILEWHENDONE, MF_BYCOMMAND) == MF_CHECKED)
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_DELETESOURCEFILEWHENDONE, MF_UNCHECKED);
    else
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_DELETESOURCEFILEWHENDONE, MF_CHECKED);
}

void CBatchEncoderDlg::OnOptionsShowLog()
{
    if(bRunning == true)
        return;

    CFileStatus rStatus;
    if(CFile::GetStatus(this->szLogFileName, rStatus) == TRUE)
    {
        // load logfile in default system editor
        ::LaunchAndWait(this->szLogFileName, _T(""), FALSE);
    }
}

void CBatchEncoderDlg::OnOptionsDeleteLog()
{
    CFileStatus rStatus;
    if(CFile::GetStatus(this->szLogFileName, rStatus) == TRUE)
    {
        if(::DeleteFile(this->szLogFileName) == FALSE)
            this->MessageBox(_T("Failed to delete logfile!"), _T("ERROR"), MB_OK | MB_ICONERROR);
    }
}

void CBatchEncoderDlg::OnOptionsLogConsoleOutput()
{
    if(bRunning == true)
        return;

    if(this->GetMenu()->GetMenuState(ID_OPTIONS_LOGCONSOLEOUTPUT, MF_BYCOMMAND) == MF_CHECKED)
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_LOGCONSOLEOUTPUT, MF_UNCHECKED);
    else
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_LOGCONSOLEOUTPUT, MF_CHECKED);
}

void CBatchEncoderDlg::OnOptionsDoNotSave()
{
    if(bRunning == true)
        return;

    if(this->GetMenu()->GetMenuState(ID_OPTIONS_DO_NOT_SAVE, MF_BYCOMMAND) == MF_CHECKED)
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_DO_NOT_SAVE, MF_UNCHECKED);
    else
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_DO_NOT_SAVE, MF_CHECKED);
}

void CBatchEncoderDlg::OnOptionsAdvanced()
{
    if(bRunning == true)
        return;

    CAdvancedDlg dlg;

    for(int i = 0; i < NUM_BROWSE_PATH_ADVANCED; i++)
        dlg.szBrowsePath[i] = this->szBrowsePath[(START_BROWSE_PATH_ADVANCED + i)];

    dlg.nThreadPriorityIndex = this->nThreadPriorityIndex;
    dlg.nProcessPriorityIndex = this->nProcessPriorityIndex;
    dlg.bDeleteOnError = this->bDeleteOnError;
    dlg.bStopOnErrors = this->bStopOnErrors;
    dlg.szLogFileName = this->szLogFileName;
    dlg.nLogEncoding = this->nLogEncoding;

    dlg.m_Color[0] = this->m_CnvStatus.crText;
    dlg.m_Color[1] = this->m_CnvStatus.crTextError;
    dlg.m_Color[2] = this->m_CnvStatus.crProgress;
    dlg.m_Color[3] = this->m_CnvStatus.crBorder;
    dlg.m_Color[4] = this->m_CnvStatus.crBack;
    dlg.m_Color[5] = this->m_Histogram.crLR;
    dlg.m_Color[6] = this->m_Histogram.crMS;
    dlg.m_Color[7] = this->m_Histogram.crBorder;
    dlg.m_Color[8] = this->m_Histogram.crBack;

    if(dlg.DoModal() == IDOK)
    {
        this->nThreadPriorityIndex = dlg.nThreadPriorityIndex;
        this->nProcessPriorityIndex = dlg.nProcessPriorityIndex;
        this->bDeleteOnError = dlg.bDeleteOnError;
        this->bStopOnErrors = dlg.bStopOnErrors;
        this->szLogFileName = dlg.szLogFileName;
        this->nLogEncoding = dlg.nLogEncoding;

        this->m_CnvStatus.crText = dlg.m_Color[0];
        this->m_CnvStatus.crTextError = dlg.m_Color[1];
        this->m_CnvStatus.crProgress = dlg.m_Color[2];
        this->m_CnvStatus.crBorder = dlg.m_Color[3];
        this->m_CnvStatus.crBack = dlg.m_Color[4];
        this->m_Histogram.crLR = dlg.m_Color[5];
        this->m_Histogram.crMS = dlg.m_Color[6];
        this->m_Histogram.crBorder = dlg.m_Color[7];
        this->m_Histogram.crBack = dlg.m_Color[8];

        // re-init Conversion Status and Histogram controls
        this->m_CnvStatus.Clean();
        this->m_CnvStatus.Init();
        this->m_CnvStatus.Erase(true);

        this->m_Histogram.Clean();
        this->m_Histogram.Init(false);
        this->m_Histogram.Erase(true);
    }

    for(int i = 0; i < NUM_BROWSE_PATH_ADVANCED; i++)
        this->szBrowsePath[(START_BROWSE_PATH_ADVANCED + i)] = dlg.szBrowsePath[i];
}

void CBatchEncoderDlg::OnOptionsForceConsoleWindow()
{
    if(bRunning == true)
        return;

    // HELP:
    // To stop forced console mode conversion process you must do the following:
    // Step 1. Press Stop button in main window
    // Step 2. In current console use Ctrl+C shortcut.
    // Step 3. Delete any invalid result files.

    if(this->GetMenu()->GetMenuState(ID_OPTIONS_FORCECONSOLEWINDOW, MF_BYCOMMAND) == MF_CHECKED)
    {
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_FORCECONSOLEWINDOW, MF_UNCHECKED);
        this->bForceConsoleWindow = false;
    }
    else
    {
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_FORCECONSOLEWINDOW, MF_CHECKED);
        this->bForceConsoleWindow = true;
    }
}

void CBatchEncoderDlg::OnOptionsConfigurePresets()
{
    if(bRunning == true)
        return;

    CPresetsDlg dlg;

    if(this->GridlinesVisible() == true)
        dlg.bShowGridLines = true;
    else
        dlg.bShowGridLines = false;

    for(int i = 0; i < NUM_BROWSE_PATH_PRESETS; i++)
        dlg.szBrowsePath[i] = this->szBrowsePath[(START_BROWSE_PATH_PRESETS + i)];

    int nSelFormat = this->m_CmbFormat.GetCurSel();
    int nSelPreset = this->m_CmbPresets.GetCurSel();

    dlg.nSelFormat = nSelFormat;
    dlg.nSelPreset = nSelPreset;

    // config files
    for(int i = 0; i < NUM_PRESET_FILES; i++)
        dlg.szPresetsFile[i] = this->szPresetsFile[i];

    dlg.szPresetsWndResize = szPresetsWndResize;
    dlg.szPresetsListColumns = this->szPresetsListColumns;

    INT_PTR nRet = dlg.DoModal();
    if(nRet == IDOK)
    {
        for(int i = 0; i < NUM_PRESET_FILES; i++)
        {
            // update exe file path
            this->szPresetsFile[i] = dlg.szPresetsFile[i];

            // reload presets from files
            this->LoadPresets(this->szPresetsFile[i], &this->m_ListPresets[i]);
        }

        // update combobox depending on selected format
        this->UpdateOutputComboBoxes(nSelFormat, nSelPreset);
    }
    else
    {
        // NOTE:
        // canceled all changes but some config files could changed
        // they will be loaded next time when app will start
    }

    this->szPresetsWndResize = dlg.szPresetsWndResize;
    this->szPresetsListColumns = dlg.szPresetsListColumns;

    for(int i = 0; i < NUM_BROWSE_PATH_PRESETS; i++)
        this->szBrowsePath[(START_BROWSE_PATH_PRESETS + i)] = dlg.szBrowsePath[i];
}

void CBatchEncoderDlg::OnOptionsConfigureFormat()
{
    if(bRunning == true)
        return;

    CFormatsDlg dlg;

    if(this->GridlinesVisible() == true)
        dlg.bShowGridLines = true;
    else
        dlg.bShowGridLines = false;

    for(int i = 0; i < NUM_FORMAT_NAMES; i++)
    {
        dlg.szFormatTemplate[i] = this->szFormatTemplate[i];
        dlg.szFormatPath[i] = this->szFormatPath[i];
        dlg.bFormatInput[i] = this->bFormatInput[i];
        dlg.bFormatOutput[i] = this->bFormatOutput[i];
        dlg.szFormatFunction[i] = this->szFormatFunction[i];
    }

    for(int i = 0; i < (NUM_BROWSE_PATH_FORMATS + NUM_BROWSE_PATH_PROGRESS); i++)
        dlg.szBrowsePath[i] = this->szBrowsePath[(START_BROWSE_PATH_FORMATS + i)];

    dlg.szFormatsWndResize = this->szFormatsWndResize;
    dlg.szFormatsListColumns = this->szFormatsListColumns;

    INT_PTR nRet = dlg.DoModal();
    if(nRet == IDOK)
    {
        for(int i = 0; i < NUM_FORMAT_NAMES; i++)
        {
            this->szFormatTemplate[i] = dlg.szFormatTemplate[i];
            this->szFormatPath[i] = dlg.szFormatPath[i];
            this->bFormatInput[i] = dlg.bFormatInput[i];
            this->bFormatOutput[i] = dlg.bFormatOutput[i];
            this->szFormatFunction[i] = dlg.szFormatFunction[i];
        }
    }

    this->szFormatsWndResize = dlg.szFormatsWndResize;
    this->szFormatsListColumns = dlg.szFormatsListColumns;

    for(int i = 0; i < (NUM_BROWSE_PATH_FORMATS + NUM_BROWSE_PATH_PROGRESS); i++)
        this->szBrowsePath[(START_BROWSE_PATH_FORMATS + i)] = dlg.szBrowsePath[i];
}

void CBatchEncoderDlg::OnOptionsShutdownWhenFinished()
{
    if(bRunning == true)
        return;

    if(this->GetMenu()->GetMenuState(ID_OPTIONS_SHUTDOWN_WHEN_FINISHED, MF_BYCOMMAND) == MF_CHECKED)
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_SHUTDOWN_WHEN_FINISHED, MF_UNCHECKED);
    else
        this->GetMenu()->CheckMenuItem(ID_OPTIONS_SHUTDOWN_WHEN_FINISHED, MF_CHECKED);
}

void CBatchEncoderDlg::OnHelpWebsite()
{
    if(bRunning == true)
        return;

    ::LaunchAndWait(MAIN_APP_WEBSITE, _T(""), FALSE);
}

void CBatchEncoderDlg::OnHelpAbout()
{
    if(bRunning == true)
        return;

    CAboutDlg dlg;

    dlg.DoModal();
}

void CBatchEncoderDlg::OnShowHistogram()
{
    if(bRunning == false)
        return;

    if(bIsCnvStatusVisible == true)
    {
        this->m_CnvStatus.ShowWindow(SW_HIDE);
        bIsCnvStatusVisible = false;
    }

    if(bRunning = true)
    {
        this->m_ChkOutPath.ShowWindow(SW_HIDE);
        this->m_EdtOutPath.ShowWindow(SW_HIDE);
        this->m_BtnBrowse.ShowWindow(SW_HIDE);

        this->m_FileProgress.ShowWindow(SW_SHOW);
    }

    if(bIsHistogramVisible == false)
    {
        this->m_LstInputFiles.ShowWindow(SW_HIDE);
        this->m_Histogram.ShowWindow(SW_SHOW);
        bIsHistogramVisible = true;
    }
    else
    {
        this->m_Histogram.ShowWindow(SW_HIDE);
        this->m_LstInputFiles.ShowWindow(SW_SHOW);
        bIsHistogramVisible = false;
    }

    if(bIsHistogramVisible == true)
    {
        this->GetMenu()->CheckMenuItem(ID_VIEW_TOOGLEHISTOGRAMWINDOW, MF_CHECKED);
        this->GetMenu()->CheckMenuItem(ID_VIEW_TOOGLEEXTENDEDPROGRESS, MF_UNCHECKED);
    }
    else
    {
        this->GetMenu()->CheckMenuItem(ID_VIEW_TOOGLEHISTOGRAMWINDOW, MF_UNCHECKED);
    }
}

void CBatchEncoderDlg::OnShowCnvStatus()
{
    if(bRunning == false)
        return;

    if(bIsHistogramVisible == true)
    {
        this->m_Histogram.ShowWindow(SW_HIDE);
        bIsHistogramVisible = false;
    }

    if(bIsCnvStatusVisible == false)
    {
        if(bRunning = true)
        {
            this->m_FileProgress.ShowWindow(SW_HIDE);

            this->m_ChkOutPath.ShowWindow(SW_SHOW);
            this->m_EdtOutPath.ShowWindow(SW_SHOW);
            this->m_BtnBrowse.ShowWindow(SW_SHOW);

            // update progress
            this->m_CnvStatus.Draw(this->nProgressCurrent);
        }

        this->m_LstInputFiles.ShowWindow(SW_HIDE);
        this->m_CnvStatus.ShowWindow(SW_SHOW);
        bIsCnvStatusVisible = true;
    }
    else
    {
        if(bRunning = true)
        {
            this->m_ChkOutPath.ShowWindow(SW_HIDE);
            this->m_EdtOutPath.ShowWindow(SW_HIDE);
            this->m_BtnBrowse.ShowWindow(SW_HIDE);

            this->m_FileProgress.ShowWindow(SW_SHOW);

            // update progress
            this->m_FileProgress.SetPos(this->nProgressCurrent);
        }

        this->m_CnvStatus.ShowWindow(SW_HIDE);
        this->m_LstInputFiles.ShowWindow(SW_SHOW);
        bIsCnvStatusVisible = false;
    }

    if(bIsCnvStatusVisible == true)
    {
        this->GetMenu()->CheckMenuItem(ID_VIEW_TOOGLEEXTENDEDPROGRESS, MF_CHECKED);
        this->GetMenu()->CheckMenuItem(ID_VIEW_TOOGLEHISTOGRAMWINDOW, MF_UNCHECKED);
    }
    else
    {
        this->GetMenu()->CheckMenuItem(ID_VIEW_TOOGLEEXTENDEDPROGRESS, MF_UNCHECKED);
    }
}

void CBatchEncoderDlg::OnNcLButtonDown(UINT nHitTest, CPoint point)
{
    // this->m_TransMove.Down(nHitTest, point);

    CResizeDialog::OnNcLButtonDown(nHitTest, point);
}

UINT CBatchEncoderDlg::OnNcHitTest(CPoint point)
{
    // this->m_TransMove.Up(point);

    return CResizeDialog::OnNcHitTest(point);
}
