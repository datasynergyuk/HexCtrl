/****************************************************************************************
* Copyright © 2018-2021 Jovibor https://github.com/jovibor/                             *
* This is a Hex Control for MFC applications.                                           *
* Official git repository of the project: https://github.com/jovibor/HexCtrl/           *
* This software is available under the "MIT License modified with The Commons Clause".  *
* https://github.com/jovibor/HexCtrl/blob/master/LICENSE                                *
****************************************************************************************/
#include "stdafx.h"
#include "CScrollEx.h"
#include <cassert>
#include <cmath>

using namespace HEXCTRL::INTERNAL::SCROLLEX;

namespace HEXCTRL::INTERNAL::SCROLLEX
{
	enum class CScrollEx::EState : WORD
	{
		STATE_DEFAULT,
		FIRSTARROW_HOVER, FIRSTARROW_CLICK,
		FIRSTCHANNEL_CLICK,
		THUMB_HOVER, THUMB_CLICK,
		LASTCHANNEL_CLICK,
		LASTARROW_CLICK, LASTARROW_HOVER
	};

	enum class ETimer : UINT_PTR
	{
		IDT_FIRSTCLICK = 0x7ff0,
		IDT_CLICKREPEAT = 0x7ff1
	};

	constexpr auto THUMB_POS_MAX = 0x7FFFFFFF;
}

BEGIN_MESSAGE_MAP(CScrollEx, CWnd)
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CScrollEx::AddSibling(CScrollEx* pSibling)
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	m_pSibling = pSibling;
}

bool CScrollEx::Create(CWnd* pParent, bool fVert, UINT uiResBmp,
	ULONGLONG ullScrolline, ULONGLONG ullScrollPage, ULONGLONG ullScrollSizeMax)
{
	assert(!m_fCreated); //Already created
	assert(pParent);
	if (m_fCreated || !pParent)
		return false;
	if (!CWnd::CreateEx(0, AfxRegisterWndClass(0), nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr))
		return false;

	m_fScrollVert = fVert;
	m_pwndParent = pParent;
	m_uiScrollBarSizeWH = GetSystemMetrics(fVert ? SM_CXVSCROLL : SM_CXHSCROLL);
	if (!m_bmpArrows.LoadBitmapW(uiResBmp))
		return false;

	m_fCreated = true;
	SetScrollSizes(ullScrolline, ullScrollPage, ullScrollSizeMax);

	return true;
}

CWnd* CScrollEx::GetParent()const
{
	assert(m_fCreated);
	if (!m_fCreated)
		return nullptr;

	return m_pwndParent;
}

ULONGLONG CScrollEx::GetScrollPos()const
{
	assert(m_fCreated);
	if (!m_fCreated)
		return { };

	return m_ullScrollPosCur;
}

LONGLONG CScrollEx::GetScrollPosDelta()const
{
	assert(m_fCreated);
	if (!m_fCreated)
		return { };

	return static_cast<LONGLONG>(m_ullScrollPosCur - m_ullScrollPosPrev);
}

ULONGLONG CScrollEx::GetScrollLineSize()const
{
	assert(m_fCreated);
	if (!m_fCreated)
		return { };

	return m_ullScrollLine;
}

ULONGLONG CScrollEx::GetScrollPageSize()const
{
	assert(m_fCreated);
	if (!m_fCreated)
		return { };

	return m_ullScrollPage;
}

bool CScrollEx::IsThumbReleased()const
{
	assert(m_fCreated);
	if (!m_fCreated)
		return false;

	return m_enState != EState::THUMB_CLICK;
}

bool CScrollEx::IsVisible()const
{
	assert(m_fCreated);
	if (!m_fCreated)
		return false;

	return m_fVisible;
}

void CScrollEx::OnLButtonUp(UINT /*nFlags*/, CPoint /*point*/)
{
	assert(m_fCreated);
	if (!m_fCreated || m_enState == EState::STATE_DEFAULT)
		return;

	m_enState = EState::STATE_DEFAULT;
	SendParentScrollMsg(); //For parent to check IsThumbReleased.
	KillTimer(static_cast<UINT_PTR>(ETimer::IDT_FIRSTCLICK));
	KillTimer(static_cast<UINT_PTR>(ETimer::IDT_CLICKREPEAT));
	ReleaseCapture();
	DrawScrollBar();
}

void CScrollEx::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
	assert(m_fCreated);
	if (!m_fCreated || !IsThumbDragging())
		return;

	const auto rc = GetScrollWorkAreaRect(true);
	const auto iCurrPos = GetThumbPos();
	int iNewPos;

	if (IsVert())
	{
		if (point.y < rc.top)
		{
			iNewPos = 0;
			m_ptCursorCur.y = rc.top;
		}
		else if (point.y > rc.bottom)
		{
			iNewPos = THUMB_POS_MAX;
			m_ptCursorCur.y = rc.bottom;
		}
		else
		{
			iNewPos = iCurrPos + (point.y - m_ptCursorCur.y);
			m_ptCursorCur.y = point.y;
		}
	}
	else
	{
		if (point.x < rc.left)
		{
			iNewPos = 0;
			m_ptCursorCur.x = rc.left;
		}
		else if (point.x > rc.right)
		{
			iNewPos = THUMB_POS_MAX;
			m_ptCursorCur.x = rc.right;
		}
		else
		{
			iNewPos = iCurrPos + (point.x - m_ptCursorCur.x);
			m_ptCursorCur.x = point.x;
		}
	}

	if (iNewPos != iCurrPos)  //Set new thumb pos only if it has been changed.
		SetThumbPos(iNewPos);
}

void CScrollEx::OnNcActivate(BOOL /*bActive*/)const
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	//To repaint NC area.
	GetParent()->SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}

void CScrollEx::OnNcCalcSize(BOOL /*bCalcValidRects*/, NCCALCSIZE_PARAMS * lpncsp)
{
	if (!m_fCreated)
		return;

	const CRect rc = lpncsp->rgrc[0];
	const auto ullCurPos = GetScrollPos();
	if (IsVert())
	{
		UINT uiHeight = rc.Height();
		if (IsSiblingVisible())
			uiHeight = rc.Height() - m_uiScrollBarSizeWH;

		if (uiHeight < m_ullScrollSizeMax)
		{
			m_fVisible = true;
			if (ullCurPos + uiHeight > m_ullScrollSizeMax)
				SetScrollPos(m_ullScrollSizeMax - uiHeight);
			else
				DrawScrollBar();
			lpncsp->rgrc[0].right -= m_uiScrollBarSizeWH;
		}
		else
		{
			SetScrollPos(0);
			m_fVisible = false;
		}
	}
	else
	{
		UINT uiWidth = rc.Width();
		if (IsSiblingVisible())
			uiWidth = rc.Width() - m_uiScrollBarSizeWH;

		if (uiWidth < m_ullScrollSizeMax)
		{
			m_fVisible = true;
			if (ullCurPos + uiWidth > m_ullScrollSizeMax)
				SetScrollPos(m_ullScrollSizeMax - uiWidth);
			else
				DrawScrollBar();
			lpncsp->rgrc[0].bottom -= m_uiScrollBarSizeWH;
		}
		else
		{
			SetScrollPos(0);
			m_fVisible = false;
		}
	}
}

void CScrollEx::OnNcPaint()const
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	DrawScrollBar();
}

void CScrollEx::OnSetCursor(CWnd * /*pWnd*/, UINT nHitTest, UINT message)
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	if (nHitTest == HTTOPLEFT || nHitTest == HTLEFT || nHitTest == HTTOPRIGHT || nHitTest == HTSIZE
		|| nHitTest == HTBOTTOMLEFT || nHitTest == HTRIGHT || nHitTest == HTBOTTOM || nHitTest == HTBOTTOMRIGHT)
		return;

	switch (message)
	{
	case WM_LBUTTONDOWN:
	{
		POINT pt;
		GetCursorPos(&pt);
		GetParent()->ScreenToClient(&pt);

		if (IsVisible())
		{
			const auto pParent = GetParent();
			pParent->SetFocus();

			if (GetThumbRect(true).PtInRect(pt))
			{
				m_ptCursorCur = pt;
				m_enState = EState::THUMB_CLICK;
				pParent->SetCapture();
			}
			else if (GetFirstArrowRect(true).PtInRect(pt))
			{
				ScrollLineUp();
				m_enState = EState::FIRSTARROW_CLICK;
				pParent->SetCapture();
				SetTimer(static_cast<UINT_PTR>(ETimer::IDT_FIRSTCLICK), m_iTimerFirstClick, nullptr);
			}
			else if (GetLastArrowRect(true).PtInRect(pt))
			{
				ScrollLineDown();
				m_enState = EState::LASTARROW_CLICK;
				pParent->SetCapture();
				SetTimer(static_cast<UINT_PTR>(ETimer::IDT_FIRSTCLICK), m_iTimerFirstClick, nullptr);
			}
			else if (GetFirstChannelRect(true).PtInRect(pt))
			{
				ScrollPageUp();
				m_enState = EState::FIRSTCHANNEL_CLICK;
				pParent->SetCapture();
				SetTimer(static_cast<UINT_PTR>(ETimer::IDT_FIRSTCLICK), m_iTimerFirstClick, nullptr);
			}
			else if (GetLastChannelRect(true).PtInRect(pt))
			{
				ScrollPageDown();
				m_enState = EState::LASTCHANNEL_CLICK;
				pParent->SetCapture();
				SetTimer(static_cast<UINT_PTR>(ETimer::IDT_FIRSTCLICK), m_iTimerFirstClick, nullptr);
			}
		}
	}
	break;
	default:
		break;
	}
}

void CScrollEx::SetScrollSizes(ULONGLONG ullLine, ULONGLONG ullPage, ULONGLONG ullSizeMax)
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	m_ullScrollLine = ullLine;
	m_ullScrollPage = ullPage;
	m_ullScrollSizeMax = ullSizeMax;

	CWnd* pWnd = GetParent();
	if (pWnd) //To repaint NC area.
		pWnd->SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}

ULONGLONG CScrollEx::SetScrollPos(ULONGLONG ullNewPos)
{
	assert(m_fCreated);
	if (!m_fCreated)
		return { };

	m_ullScrollPosPrev = m_ullScrollPosCur;
	if (m_ullScrollPosCur == ullNewPos)
		return m_ullScrollPosPrev;

	m_ullScrollPosCur = ullNewPos;

	const auto rc = GetParentRect();
	const auto iScreenSize { IsVert() ? rc.Height() : rc.Width() };
	const ULONGLONG ullMax { iScreenSize > m_ullScrollSizeMax ? 0 : m_ullScrollSizeMax - iScreenSize };

	if (m_ullScrollPosCur > ullMax)
		m_ullScrollPosCur = ullMax;

	SendParentScrollMsg();
	DrawScrollBar();

	return m_ullScrollPosPrev;
}

void CScrollEx::ScrollLineDown()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	const auto ullCur = GetScrollPos();
	const ULONGLONG ullNew { ULONGLONG_MAX - ullCur < m_ullScrollLine ? ULONGLONG_MAX : ullCur + m_ullScrollLine };
	SetScrollPos(ullNew);
}

void CScrollEx::ScrollLineRight()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	ScrollLineDown();
}

void CScrollEx::ScrollLineUp()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	const auto ullCur = GetScrollPos();
	SetScrollPos(m_ullScrollLine > ullCur ? 0 : ullCur - m_ullScrollLine);
}

void CScrollEx::ScrollLineLeft()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	ScrollLineUp();
}

void CScrollEx::ScrollPageDown()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	const auto ullCur = GetScrollPos();
	SetScrollPos(ULONGLONG_MAX - ullCur < m_ullScrollPage ? ULONGLONG_MAX : ullCur + m_ullScrollPage);
}

void CScrollEx::ScrollPageLeft()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	ScrollPageUp();
}

void CScrollEx::ScrollPageRight()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	ScrollPageDown();
}

void CScrollEx::ScrollPageUp()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	const auto ullCur = GetScrollPos();
	SetScrollPos(m_ullScrollPage > ullCur ? 0 : ullCur - m_ullScrollPage);
}

void CScrollEx::ScrollHome()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	SetScrollPos(0);
}

void CScrollEx::ScrollEnd()
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	SetScrollPos(m_ullScrollSizeMax);
}

void CScrollEx::SetScrollPageSize(ULONGLONG ullSize)
{
	assert(m_fCreated);
	if (!m_fCreated)
		return;

	m_ullScrollPage = ullSize;
}

/********************************************************************
* Private methods.
********************************************************************/
void CScrollEx::DrawScrollBar()const
{
	if (!IsVisible())
		return;

	const auto pParent = GetParent();
	CWindowDC dcParent(pParent);
	CDC dcMem;
	dcMem.CreateCompatibleDC(&dcParent);
	CBitmap bitmap;
	const auto rc = GetParentRect(false);
	bitmap.CreateCompatibleBitmap(&dcParent, rc.Width(), rc.Height());
	dcMem.SelectObject(&bitmap);
	CDC* pDC = &dcMem;

	const auto rcSNC = GetScrollRect(true);	//Scroll bar with any additional non client area, to fill it below.
	pDC->FillSolidRect(&rcSNC, m_clrBkNC);	//Scroll bar with NC Bk.
	const auto rcS = GetScrollRect();
	pDC->FillSolidRect(&rcS, m_clrBkScrollBar); //Scroll bar Bk.
	DrawArrows(pDC);
	DrawThumb(pDC);

	//Copy drawn Scrollbar from dcMem to parent window.
	dcParent.BitBlt(rcSNC.left, rcSNC.top, rcSNC.Width(), rcSNC.Height(), &dcMem, rcSNC.left, rcSNC.top, SRCCOPY);
}

void CScrollEx::DrawArrows(CDC* pDC)const
{
	const auto rcScroll = GetScrollRect();
	CDC dcCompat;
	dcCompat.CreateCompatibleDC(pDC);
	dcCompat.SelectObject(m_bmpArrows);

	int iFirstBtnWH;
	int iLastBtnOffsetDrawX, iLastBtnOffsetDrawY, iLastBtnWH, iLastBtnBmpOffsetX, iLastBtnBmpOffsetY;
	if (IsVert())
	{
		//First arrow button: offsets in bitmap to take from and screen coords to print to.
		iFirstBtnWH = rcScroll.Width();

		iLastBtnBmpOffsetX = 0;
		iLastBtnBmpOffsetY = m_uiLastArrowOffset;
		iLastBtnOffsetDrawX = rcScroll.left;
		iLastBtnOffsetDrawY = rcScroll.bottom - rcScroll.Width();
		iLastBtnWH = rcScroll.Width();
	}
	else
	{
		iFirstBtnWH = rcScroll.Height();

		iLastBtnBmpOffsetX = m_uiLastArrowOffset;
		iLastBtnBmpOffsetY = 0;
		iLastBtnOffsetDrawX = rcScroll.right - rcScroll.Height();
		iLastBtnOffsetDrawY = rcScroll.top;
		iLastBtnWH = rcScroll.Height();
	}

	constexpr int iFirstBtnBmpOffsetX { 0 }, iFirstBtnBmpOffsetY { 0 };
	const int iFirstBtnOffsetDrawX { rcScroll.left }, iFirstBtnOffsetDrawY { rcScroll.top };

	//First arrow button.
	pDC->StretchBlt(iFirstBtnOffsetDrawX, iFirstBtnOffsetDrawY, iFirstBtnWH, iFirstBtnWH,
		&dcCompat, iFirstBtnBmpOffsetX, iFirstBtnBmpOffsetY, m_uiArrowSize, m_uiArrowSize, SRCCOPY);

	//Last arrow button.
	pDC->StretchBlt(iLastBtnOffsetDrawX, iLastBtnOffsetDrawY, iLastBtnWH, iLastBtnWH,
		&dcCompat, iLastBtnBmpOffsetX, iLastBtnBmpOffsetY, m_uiArrowSize, m_uiArrowSize, SRCCOPY);
}

void CScrollEx::DrawThumb(CDC* pDC)const
{
	auto rcThumb = GetThumbRect();
	if (!rcThumb.IsRectNull())
		pDC->FillSolidRect(rcThumb, m_clrThumb);
}

CRect CScrollEx::GetScrollRect(bool fWithNCArea)const
{
	if (!m_fCreated)
		return { };

	const auto* const pParent = GetParent();
	auto rcClient = GetParentRect();
	const auto rcWnd = GetParentRect(false);
	pParent->MapWindowPoints(nullptr, &rcClient);

	const auto iTopDelta = GetTopDelta();
	const auto iLeftDelta = GetLeftDelta();

	CRect rcScroll;
	if (IsVert())
	{
		rcScroll.left = rcClient.right + iLeftDelta;
		rcScroll.top = rcClient.top + iTopDelta;
		rcScroll.right = rcScroll.left + m_uiScrollBarSizeWH;
		if (fWithNCArea) //Adding difference here to gain equality in coords when call to pParent->ScreenToClient below.
			rcScroll.bottom = rcWnd.bottom + iTopDelta;
		else
			rcScroll.bottom = rcScroll.top + rcClient.Height();
	}
	else
	{
		rcScroll.left = rcClient.left + iLeftDelta;
		rcScroll.top = rcClient.bottom + iTopDelta;
		rcScroll.bottom = rcScroll.top + m_uiScrollBarSizeWH;
		if (fWithNCArea)
			rcScroll.right = rcWnd.right + iLeftDelta;
		else
			rcScroll.right = rcScroll.left + rcClient.Width();
	}
	pParent->ScreenToClient(&rcScroll);

	return rcScroll;
}

CRect CScrollEx::GetScrollWorkAreaRect(bool fClientCoord)const
{
	auto rc = GetScrollRect();
	if (IsVert())
		rc.DeflateRect(0, m_uiScrollBarSizeWH, 0, m_uiScrollBarSizeWH);
	else
		rc.DeflateRect(m_uiScrollBarSizeWH, 0, m_uiScrollBarSizeWH, 0);

	if (fClientCoord)
		rc.OffsetRect(-GetLeftDelta(), -GetTopDelta());

	return rc;
}

UINT CScrollEx::GetScrollSizeWH()const
{
	return IsVert() ? GetScrollRect().Height() : GetScrollRect().Width();
}

UINT CScrollEx::GetScrollWorkAreaSizeWH()const
{
	const auto uiScrollSize = GetScrollSizeWH();

	return uiScrollSize <= m_uiArrowSize * 2 ? 0 : uiScrollSize - m_uiArrowSize * 2; //Minus two arrow's size.
}

CRect CScrollEx::GetThumbRect(bool fClientCoord)const
{
	CRect rc { };
	const auto uiThumbSize = GetThumbSizeWH();
	if (!uiThumbSize)
		return rc;

	const auto rcScrollWA = GetScrollWorkAreaRect();
	if (IsVert())
	{
		rc.left = rcScrollWA.left;
		rc.top = rcScrollWA.top + GetThumbPos();
		rc.right = rc.left + m_uiScrollBarSizeWH;
		rc.bottom = rc.top + uiThumbSize;
		if (rc.bottom > rcScrollWA.bottom)
			rc.bottom = rcScrollWA.bottom;
	}
	else
	{
		rc.left = rcScrollWA.left + GetThumbPos();
		rc.top = rcScrollWA.top;
		rc.right = rc.left + uiThumbSize;
		rc.bottom = rc.top + m_uiScrollBarSizeWH;
	}
	if (fClientCoord)
		rc.OffsetRect(-GetLeftDelta(), -GetTopDelta());

	return rc;
}

UINT CScrollEx::GetThumbSizeWH()const
{
	const auto uiScrollWorkAreaSizeWH = GetScrollWorkAreaSizeWH();
	const auto rcParent = GetParentRect();
	const long double dDelta { IsVert() ?
		static_cast<long double>(rcParent.Height()) / m_ullScrollSizeMax :
		static_cast<long double>(rcParent.Width()) / m_ullScrollSizeMax };

	const UINT uiThumbSize { static_cast<UINT>(std::lroundl(uiScrollWorkAreaSizeWH * dDelta)) };

	return uiThumbSize < m_uiThumbSizeMin ? m_uiThumbSizeMin : uiThumbSize;
}

int CScrollEx::GetThumbPos()const
{
	const auto ullScrollPos = GetScrollPos();
	const auto dThumbScrollingSize = GetThumbScrollingSize();

	return ullScrollPos < dThumbScrollingSize ? 0 : std::lroundl(ullScrollPos / dThumbScrollingSize);
}

void CScrollEx::SetThumbPos(int iPos)
{
	if (iPos == GetThumbPos())
		return;

	const auto rcWorkArea = GetScrollWorkAreaRect();
	const auto uiThumbSize = GetThumbSizeWH();
	ULONGLONG ullNewScrollPos;

	if (iPos < 0)
		ullNewScrollPos = 0;
	else if (iPos == THUMB_POS_MAX)
		ullNewScrollPos = m_ullScrollSizeMax;
	else
	{
		if (IsVert())
		{
			if (iPos + static_cast<int>(uiThumbSize) > rcWorkArea.Height())
				iPos = rcWorkArea.Height() - uiThumbSize;
		}
		else
		{
			if (iPos + static_cast<int>(uiThumbSize) > rcWorkArea.Width())
				iPos = rcWorkArea.Width() - uiThumbSize;
		}
		ullNewScrollPos = static_cast<ULONGLONG>(std::llroundl(iPos * GetThumbScrollingSize()));
	}

	SetScrollPos(ullNewScrollPos);
}

long double CScrollEx::GetThumbScrollingSize()const
{
	if (!m_fCreated)
		return 0;

	const auto uiWAWOThumb = GetScrollWorkAreaSizeWH() - GetThumbSizeWH(); //Work area without thumb.
	const auto iPage { IsVert() ? GetParentRect().Height() : GetParentRect().Width() };

	return (m_ullScrollSizeMax - iPage) / static_cast<long double>(uiWAWOThumb);
}

CRect CScrollEx::GetFirstArrowRect(bool fClientCoord)const
{
	auto rc = GetScrollRect();
	if (IsVert())
		rc.bottom = rc.top + m_uiArrowSize;
	else
		rc.right = rc.left + m_uiArrowSize;

	if (fClientCoord)
		rc.OffsetRect(-GetLeftDelta(), -GetTopDelta());

	return rc;
}

CRect CScrollEx::GetLastArrowRect(bool fClientCoord)const
{
	CRect rc = GetScrollRect();
	if (IsVert())
		rc.top = rc.bottom - m_uiArrowSize;
	else
		rc.left = rc.right - m_uiArrowSize;

	if (fClientCoord)
		rc.OffsetRect(-GetLeftDelta(), -GetTopDelta());

	return rc;
}

CRect CScrollEx::GetFirstChannelRect(bool fClientCoord)const
{
	const auto rcThumb = GetThumbRect();
	const auto rcArrow = GetFirstArrowRect();
	CRect rc;
	if (IsVert())
		rc.SetRect(rcArrow.left, rcArrow.bottom, rcArrow.right, rcThumb.top);
	else
		rc.SetRect(rcArrow.right, rcArrow.top, rcThumb.left, rcArrow.bottom);

	if (fClientCoord)
		rc.OffsetRect(-GetLeftDelta(), -GetTopDelta());

	return rc;
}

CRect CScrollEx::GetLastChannelRect(bool fClientCoord)const
{
	const auto rcThumb = GetThumbRect();
	const auto rcArrow = GetLastArrowRect();
	CRect rc;
	if (IsVert())
		rc.SetRect(rcArrow.left, rcThumb.bottom, rcArrow.right, rcArrow.top);
	else
		rc.SetRect(rcThumb.left, rcArrow.top, rcArrow.left, rcArrow.bottom);

	if (fClientCoord)
		rc.OffsetRect(-GetLeftDelta(), -GetTopDelta());

	return rc;
}

CRect CScrollEx::GetParentRect(bool fClient)const
{
	CRect rc;
	if (fClient)
		GetParent()->GetClientRect(&rc);
	else
		GetParent()->GetWindowRect(&rc);

	return rc;
}

int CScrollEx::GetTopDelta()const
{
	CRect rcClient = GetParentRect();
	GetParent()->MapWindowPoints(nullptr, &rcClient);

	return rcClient.top - GetParentRect(false).top;
}

int CScrollEx::GetLeftDelta()const
{
	CRect rcClient = GetParentRect();
	GetParent()->MapWindowPoints(nullptr, &rcClient);

	return rcClient.left - GetParentRect(false).left;
}

bool CScrollEx::IsVert()const
{
	return m_fScrollVert;
}

bool CScrollEx::IsThumbDragging()const
{
	return m_enState == EState::THUMB_CLICK;
}

bool CScrollEx::IsSiblingVisible()const
{
	return m_pSibling ? m_pSibling->IsVisible() : false;
}

void CScrollEx::SendParentScrollMsg()const
{
	if (!m_fCreated)
		return;

	GetParent()->SendMessageW(IsVert() ? WM_VSCROLL : WM_HSCROLL);
}

void CScrollEx::OnTimer(UINT_PTR nIDEvent)
{
	switch (nIDEvent)
	{
	case (static_cast<UINT_PTR>(ETimer::IDT_FIRSTCLICK)):
		KillTimer(static_cast<UINT_PTR>(ETimer::IDT_FIRSTCLICK));
		SetTimer(static_cast<UINT_PTR>(ETimer::IDT_CLICKREPEAT), m_iTimerRepeat, nullptr);
		break;
	case (static_cast<UINT_PTR>(ETimer::IDT_CLICKREPEAT)):
		switch (m_enState)
		{
		case EState::FIRSTARROW_CLICK:
			ScrollLineUp();
			break;
		case EState::LASTARROW_CLICK:
			ScrollLineDown();
			break;
		case EState::FIRSTCHANNEL_CLICK:
		{
			CPoint pt;
			GetCursorPos(&pt);
			CRect rc = GetThumbRect(true);
			GetParent()->ClientToScreen(rc);
			if (IsVert()) {
				if (pt.y < rc.top)
					ScrollPageUp();
			}
			else {
				if (pt.x < rc.left)
					ScrollPageUp();
			}
		}
		break;
		case EState::LASTCHANNEL_CLICK:
		{
			CPoint pt;
			GetCursorPos(&pt);
			CRect rc = GetThumbRect(true);
			GetParent()->ClientToScreen(rc);
			if (IsVert())
			{
				if (pt.y > rc.bottom)
					ScrollPageDown();
			}
			else
			{
				if (pt.x > rc.right)
					ScrollPageDown();
			}
		}
		break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	CWnd::OnTimer(nIDEvent);
}

void CScrollEx::OnDestroy()
{
	CWnd::OnDestroy();

	m_bmpArrows.DeleteObject();
	m_fCreated = false;
}