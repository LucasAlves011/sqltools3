/* 
	SQLTools is a tool for Oracle database developers and DBAs.
	Copyright (C) 1997-2025 Aleksey Kochetov

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "stdafx.h"
#include "COutlineTreeCtrl.h"

BEGIN_MESSAGE_MAP(COutlineTreeCtrl, CTreeCtrl)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnNMCustomdraw)
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

void COutlineTreeCtrl::OnNMCustomdraw (NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = CDRF_DODEFAULT;

    try {
        EXCEPTION_FRAME;

        LPNMTVCUSTOMDRAW pNMTVCD = reinterpret_cast<LPNMTVCUSTOMDRAW>(pNMHDR);

        switch (pNMTVCD->nmcd.dwDrawStage)
        {
        case CDDS_PREPAINT:
            *pResult = CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW;
            break;
        case CDDS_ITEMPREPAINT:
        {
            HTREEITEM hItem = (HTREEITEM)(pNMTVCD->nmcd.dwItemSpec);
            CString text = GetItemText(hItem);
            if (text.Find('\t') != -1)
            {
                pNMTVCD->clrText = pNMTVCD->clrTextBk;
                *pResult = CDRF_NOTIFYPOSTPAINT;
            }
        }
        break;
        case CDDS_ITEMPOSTPAINT:
        {
            bool hasFocus = (::GetFocus() == m_hWnd) ? true : false;
            bool isSeleted = pNMTVCD->nmcd.uItemState & CDIS_SELECTED ? true : false;
            HTREEITEM hItem = (HTREEITEM)(pNMTVCD->nmcd.dwItemSpec);

            if (isSeleted)
                ::SetBkColor(pNMTVCD->nmcd.hdc, ::GetSysColor(hasFocus ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
            else
                ::SetBkColor(pNMTVCD->nmcd.hdc, ::GetSysColor(COLOR_WINDOW));

            ::SetTextColor(pNMTVCD->nmcd.hdc, ::GetSysColor(COLOR_GRAYTEXT));

            CString text = GetItemText(hItem);
            int tabPos = text.Find('\t');

            int datatypeOffset;
            INT nTabStopPositions[3];

            TEXTMETRIC tm = { 0 };

            if (GetTextMetrics(pNMTVCD->nmcd.hdc, &tm))
            {
                int length = 0;

                if (!length)
                    length = tabPos != -1 ? tabPos : text.GetLength();

                datatypeOffset = tm.tmAveCharWidth * (((length / 3) * 3) + 1);
                nTabStopPositions[0] = tm.tmAveCharWidth * 3;
                nTabStopPositions[1] = tm.tmAveCharWidth * 6;
                nTabStopPositions[2] = tm.tmAveCharWidth * 9;
            }
            else
            {
                datatypeOffset = 120;
                nTabStopPositions[0] = 20;
                nTabStopPositions[1] = 40;
                nTabStopPositions[2] = 60;
            }

            RECT rc;
            GetItemRect(hItem, &rc, TRUE);

            if (isSeleted && hasFocus)
                ::SetTextColor(pNMTVCD->nmcd.hdc, ::GetSysColor(COLOR_HIGHLIGHTTEXT));

            //LONG extentXY = 
            ::TabbedTextOut(pNMTVCD->nmcd.hdc, rc.left + 2, rc.top + 1, text, text.GetLength(),
                sizeof(nTabStopPositions) / sizeof(nTabStopPositions[0]), nTabStopPositions, rc.left + datatypeOffset);

            if (tabPos != -1)
            {
                LPWSTR buff = text.GetBuffer();
                buff[tabPos] = 0;
                text.ReleaseBuffer();
            }

            if (isSeleted)
                ::SetTextColor(pNMTVCD->nmcd.hdc, ::GetSysColor(hasFocus ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT));
            else
                ::SetTextColor(pNMTVCD->nmcd.hdc, ::GetSysColor(COLOR_WINDOWTEXT));

            ::TabbedTextOut(pNMTVCD->nmcd.hdc, rc.left + 2, rc.top + 1, text, text.GetLength(),
                sizeof(nTabStopPositions) / sizeof(nTabStopPositions[0]), nTabStopPositions, rc.left + datatypeOffset);

            // Draw focus rect over the full tab-extended label rect
            //if (pNMTVCD->nmcd.uItemState & CDIS_FOCUS)
            //{
            //    RECT focusRc = rc;
            //    focusRc.right = rc.left + max(rc.right - rc.left, (LONG)(LOWORD(extentXY)) + 2);
            //    ::DrawFocusRect(pNMTVCD->nmcd.hdc, &focusRc);
            //}
        }
        break;
        }
    }
    _COMMON_DEFAULT_HANDLER_
}

void COutlineTreeCtrl::OnLButtonDown (UINT nFlags, CPoint point)
{
    UINT nHitFlags = 0;
    HTREEITEM hClickedItem = HitTest(point, &nHitFlags);
    SelectItem(hClickedItem); // required to fix a custom draw issue

    CTreeCtrl::OnLButtonDown(nFlags, point);
}

void COutlineTreeCtrl::OnRButtonDown (UINT nFlags, CPoint point)
{
    UINT nHitFlags = 0;
    HTREEITEM hClickedItem = HitTest(point, &nHitFlags);
    SelectItem(hClickedItem); // required to fix a custom draw issue

    CTreeCtrl::OnRButtonDown(nFlags, point);
}
