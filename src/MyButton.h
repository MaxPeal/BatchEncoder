﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

class CMyButton : public CButton
{
public:
    CMyButton();
    virtual ~CMyButton();
protected:
    virtual void PreSubclassWindow();
protected:
    CFont m_BoldFont;
    CFont m_StdFont;
protected:
    bool bIsBold;
public:
    void SetBold(bool bBold = true);
    bool GetBold();
protected:
    DECLARE_MESSAGE_MAP()
};
