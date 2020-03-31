/****************************************************************************************
* Copyright � 2018-2020 Jovibor https://github.com/jovibor/                             *
* This is a Hex Control for MFC/Win32 applications.                                     *
* Official git repository: https://github.com/jovibor/HexCtrl/                          *
* This software is available under the "MIT License modified with The Commons Clause".  *
* https://github.com/jovibor/HexCtrl/blob/master/LICENSE                                *
* For more information visit the project's official repository.                         *
****************************************************************************************/
#include "stdafx.h"
#include "CHexDlgDataInterpret.h"
#include "../Helper.h"
#include <algorithm>
#include "strsafe.h"

using namespace HEXCTRL;
using namespace HEXCTRL::INTERNAL;

//Time calculation constants
namespace HEXCTRL::INTERNAL {
	const WCHAR* NIBBLES [] = {
		L"0000", L"0001", L"0010", L"0011", L"0100", L"0101", L"0110", L"0111",
		L"1000", L"1001", L"1010", L"1011", L"1100", L"1101", L"1110", L"1111" };
	const WCHAR NOTAPPLICABLE[] = L"N/A";
	constexpr auto FTTICKSPERMS = 10000U;                //Number of 100ns intervals in a milli-second
	constexpr auto FTTICKSPERSECOND = 10000000UL;        //Number of 100ns intervals in a second
	constexpr auto HOURSPERDAY = 24U;                    //24 hours per day
	constexpr auto SECONDSPERHOUR = 3600U;               //3600 seconds per hour
	constexpr auto FILETIME1582OFFSETDAYS = 6653U;       //FILETIME is based upon 1 Jan 1601 whilst GUID time is from 15 Oct 1582. Add 6653 days to convert to GUID time
	constexpr auto FILETIME1970_LOW = 0xd53e8000UL;      //1st Jan 1970 as FILETIME
	constexpr auto FILETIME1970_HIGH = 0x019db1deUL;	 //Used for Unix and Java times
	constexpr auto UNIXEPOCHDIFFERENCE = 11644473600ULL; //Number of ticks from FILETIME epoch of 1st Jan 1601 to Unix epoch of 1st Jan 1970
}

BEGIN_MESSAGE_MAP(CHexDlgDataInterpret, CDialogEx)
	ON_WM_CLOSE()
	ON_WM_ACTIVATE()
	ON_WM_SIZE()
	ON_REGISTERED_MESSAGE(AFX_WM_PROPERTY_CHANGED, &CHexDlgDataInterpret::OnPropertyChanged)
	ON_COMMAND(IDC_HEXCTRL_DATAINTERPRET_RADIO_LE, &CHexDlgDataInterpret::OnClickRadioLe)
	ON_COMMAND(IDC_HEXCTRL_DATAINTERPRET_RADIO_BE, &CHexDlgDataInterpret::OnClickRadioBe)
	ON_COMMAND(IDC_HEXCTRL_DATAINTERPRET_RADIO_DEC, &CHexDlgDataInterpret::OnClickRadioDec)
	ON_COMMAND(IDC_HEXCTRL_DATAINTERPRET_RADIO_HEX, &CHexDlgDataInterpret::OnClickRadioHex)
END_MESSAGE_MAP()

void CHexDlgDataInterpret::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_HEXCTRL_DATAINTERPRET_PROPERTY_DATA, m_stCtrlGrid);
}

BOOL CHexDlgDataInterpret::Create(UINT nIDTemplate, CHexCtrl* pHexCtrl)
{
	if (!pHexCtrl)
		return FALSE;

	m_pHexCtrl = pHexCtrl;

	return CDialogEx::Create(nIDTemplate, pHexCtrl);
}

BOOL CHexDlgDataInterpret::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	//Determine locale specific date format. Default to UK/European if unable to determine
	//See: https://docs.microsoft.com/en-gb/windows/win32/intl/locale-idate
	int nResult = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_IDATE | LOCALE_RETURN_NUMBER,
		reinterpret_cast<LPTSTR>(&m_dwDateFormat), sizeof(m_dwDateFormat));
	if (!nResult)
		m_dwDateFormat = 1;

	//Determine 'short' date seperator character. Default to UK/European if unable to determine	
	nResult = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SDATE, m_wszDateSeparator, _countof(m_wszDateSeparator));
	if (!nResult)
		_stprintf_s(m_wszDateSeparator, _countof(m_wszDateSeparator), L"/");

	//Update dialog title to include date format
	CString sTitle;
	GetWindowTextW(sTitle);
	sTitle.AppendFormat(L" [%s]", GetCurrentUserDateFormatString().GetString());
	SetWindowTextW(sTitle);

	if (auto pRadio = (CButton*)GetDlgItem(IDC_HEXCTRL_DATAINTERPRET_RADIO_LE); pRadio)
		pRadio->SetCheck(1);
	if (auto pRadio = (CButton*)GetDlgItem(IDC_HEXCTRL_DATAINTERPRET_RADIO_DEC); pRadio)
		pRadio->SetCheck(1);

	m_hdItemPropGrid.mask = HDI_WIDTH;
	m_hdItemPropGrid.cxy = 150;
	m_stCtrlGrid.EnableHeaderCtrl(TRUE, L"Data type", L"Value");
	m_stCtrlGrid.GetHeaderCtrl().SetItem(0, &m_hdItemPropGrid); //Property grid column size.

	m_vecProp.emplace_back(GRIDDATA { EGroup::DIGITS, EName::NAME_BINARY, ESize::SIZE_BYTE, new CMFCPropertyGridProperty(L"binary:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::DIGITS, EName::NAME_CHAR, ESize::SIZE_BYTE, new CMFCPropertyGridProperty(L"char:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::DIGITS, EName::NAME_UCHAR, ESize::SIZE_BYTE, new CMFCPropertyGridProperty(L"unsigned char:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::DIGITS, EName::NAME_SHORT, ESize::SIZE_WORD, new CMFCPropertyGridProperty(L"short:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::DIGITS, EName::NAME_USHORT, ESize::SIZE_WORD, new CMFCPropertyGridProperty(L"unsigned short:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::DIGITS, EName::NAME_LONG, ESize::SIZE_DWORD, new CMFCPropertyGridProperty(L"long:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::DIGITS, EName::NAME_ULONG, ESize::SIZE_DWORD, new CMFCPropertyGridProperty(L"unsigned long:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::DIGITS, EName::NAME_LONGLONG, ESize::SIZE_QWORD, new CMFCPropertyGridProperty(L"long long:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::DIGITS, EName::NAME_ULONGLONG, ESize::SIZE_QWORD, new CMFCPropertyGridProperty(L"unsigned long long:", L"0") });
	auto pDigits = new CMFCPropertyGridProperty(L"Digits:");
	for (auto& iter : m_vecProp)
		if (iter.eGroup == EGroup::DIGITS)
			pDigits->AddSubItem(iter.pProp);
	m_stCtrlGrid.AddProperty(pDigits);

	m_vecProp.emplace_back(GRIDDATA { EGroup::FLOAT, EName::NAME_FLOAT, ESize::SIZE_DWORD, new CMFCPropertyGridProperty(L"Float:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::FLOAT, EName::NAME_DOUBLE, ESize::SIZE_QWORD, new CMFCPropertyGridProperty(L"Double:", L"0") });
	auto pFloats = new CMFCPropertyGridProperty(L"Floats:");
	for (auto& iter : m_vecProp)
		if (iter.eGroup == EGroup::FLOAT)
			pFloats->AddSubItem(iter.pProp);
	m_stCtrlGrid.AddProperty(pFloats);

	m_vecProp.emplace_back(GRIDDATA { EGroup::TIME, EName::NAME_TIME32T, ESize::SIZE_DWORD, new CMFCPropertyGridProperty(L"time32_t:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::TIME, EName::NAME_TIME64T, ESize::SIZE_QWORD, new CMFCPropertyGridProperty(L"time64_t:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::TIME, EName::NAME_FILETIME, ESize::SIZE_QWORD, new CMFCPropertyGridProperty(L"FILETIME:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::TIME, EName::NAME_OLEDATETIME, ESize::SIZE_QWORD, new CMFCPropertyGridProperty(L"OLE time:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::TIME, EName::NAME_JAVATIME, ESize::SIZE_QWORD, new CMFCPropertyGridProperty(L"Java time:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::TIME, EName::NAME_MSDOSTIME, ESize::SIZE_QWORD, new CMFCPropertyGridProperty(L"MS-DOS time:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::TIME, EName::NAME_MSDTTMTIME, ESize::SIZE_DWORD, new CMFCPropertyGridProperty(L"MS-DTTM time:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::TIME, EName::NAME_SYSTEMTIME, ESize::SIZE_DQWORD, new CMFCPropertyGridProperty(L"Windows SYSTEMTIME:", L"0") });
	m_vecProp.emplace_back(GRIDDATA { EGroup::TIME, EName::NAME_GUIDTIME, ESize::SIZE_DQWORD, new CMFCPropertyGridProperty(L"GUID v1 UTC time:", L"0") });
	auto pTime = new CMFCPropertyGridProperty(L"Time:");
	for (auto& iter : m_vecProp)
		if (iter.eGroup == EGroup::TIME)
			pTime->AddSubItem(iter.pProp);
	m_stCtrlGrid.AddProperty(pTime);

	m_vecProp.emplace_back(GRIDDATA { EGroup::MISC, EName::NAME_GUID, ESize::SIZE_DQWORD, new CMFCPropertyGridProperty(L"GUID:", L"0") });
	auto pMisc = new CMFCPropertyGridProperty(L"Misc:");
	for (auto& iter : m_vecProp)
		if (iter.eGroup == EGroup::MISC)
			pMisc->AddSubItem(iter.pProp);
	m_stCtrlGrid.AddProperty(pMisc);

	return TRUE;
}

void CHexDlgDataInterpret::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	if (nState == WA_INACTIVE)
	{
		m_ullSize = 0;
		UpdateHexCtrl();
	}

	CDialogEx::OnActivate(nState, pWndOther, bMinimized);
}

void CHexDlgDataInterpret::OnOK()
{
	if (!m_pHexCtrl->IsMutable() || !m_pPropChanged)
		return;

	const auto& refGridData = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[this](const GRIDDATA& refData) {return refData.pProp == m_pPropChanged; });
	if (refGridData == m_vecProp.end())
		return;

	CStringW wstrValue = m_pPropChanged->GetValue();
	std::wstring_view wstr = wstrValue.GetString();
	bool fSuccess { false };

	switch (refGridData->eName)
	{
	case EName::NAME_BINARY:
		fSuccess = SetDataNAME_BINARY(wstr);
		break;
	case EName::NAME_CHAR:
		fSuccess = SetDataNAME_CHAR(wstr);
		break;
	case EName::NAME_UCHAR:
		fSuccess = SetDataNAME_UCHAR(wstr);
		break;
	case EName::NAME_SHORT:
		fSuccess = SetDataNAME_SHORT(wstr);
		break;
	case EName::NAME_USHORT:
		fSuccess = SetDataNAME_USHORT(wstr);
		break;
	case EName::NAME_LONG:
		fSuccess = SetDataNAME_LONG(wstr);
		break;
	case EName::NAME_ULONG:
		fSuccess = SetDataNAME_ULONG(wstr);
		break;
	case EName::NAME_LONGLONG:
		fSuccess = SetDataNAME_LONGLONG(wstr);
		break;
	case EName::NAME_ULONGLONG:
		fSuccess = SetDataNAME_ULONGLONG(wstr);
		break;
	case EName::NAME_FLOAT:
		fSuccess = SetDataNAME_FLOAT(wstr);
		break;
	case EName::NAME_DOUBLE:
		fSuccess = SetDataNAME_DOUBLE(wstr);
		break;
	case EName::NAME_TIME32T:
		fSuccess = SetDataNAME_TIME32T(wstr);
		break;
	case EName::NAME_TIME64T:
		fSuccess = SetDataNAME_TIME64T(wstr);
		break;
	case EName::NAME_FILETIME:
		fSuccess = SetDataNAME_FILETIME(wstr);
		break;
	case EName::NAME_OLEDATETIME:
		fSuccess = SetDataNAME_OLEDATETIME(wstr);
		break;
	case EName::NAME_JAVATIME:
		fSuccess = SetDataNAME_JAVATIME(wstr);
		break;
	case EName::NAME_MSDOSTIME:
		fSuccess = SetDataNAME_MSDOSTIME(wstr);
		break;
	case EName::NAME_MSDTTMTIME:
		fSuccess = SetDataNAME_MSDTTMTIME(wstr);
		break;
	case EName::NAME_SYSTEMTIME:
		fSuccess = SetDataNAME_SYSTEMTIME(wstr);
		break;
	case EName::NAME_GUIDTIME:
		fSuccess = SetDataNAME_GUIDTIME(wstr);
		break;
	case EName::NAME_GUID:
		fSuccess = SetDataNAME_GUID(wstr);
		break;
	};

	if (!fSuccess)
		MessageBoxW(L"Wrong number format or out of range.", L"Data error...", MB_ICONERROR);
	else
		UpdateHexCtrl();

	InspectOffset(m_ullOffset);
}

void CHexDlgDataInterpret::InspectOffset(ULONGLONG ullOffset)
{
	if (!m_fVisible)
		return;

	auto ullDataSize = m_pHexCtrl->GetDataSize();
	if (ullOffset >= ullDataSize) //Out of data bounds.
		return;

	for (const auto& iter : m_vecProp)
		iter.pProp->AllowEdit(m_pHexCtrl->IsMutable());

	m_ullOffset = ullOffset;
	const auto byte = m_pHexCtrl->GetData<BYTE>(ullOffset);

	ShowNAME_BINARY(byte);
	ShowNAME_CHAR(byte);
	ShowNAME_UCHAR(byte);

	if (ullOffset + static_cast<unsigned>(ESize::SIZE_WORD) > ullDataSize)
	{
		for (const auto& iter : m_vecProp)
		{
			if (iter.eSize >= ESize::SIZE_WORD)
			{
				iter.pProp->SetValue(L"0");
				iter.pProp->Enable(FALSE);
			}
		}
		return;
	}

	//ESize::SIZE_WORD////////////////////////////////////////////
	for (const auto& iter : m_vecProp)
		if (iter.eSize == ESize::SIZE_WORD)
			iter.pProp->Enable(TRUE);

	auto word = m_pHexCtrl->GetData<WORD>(ullOffset);
	if (m_fBigEndian)
		word = _byteswap_ushort(word);

	ShowNAME_SHORT(word);
	ShowNAME_USHORT(word);

	if (ullOffset + static_cast<unsigned>(ESize::SIZE_DWORD) > ullDataSize)
	{
		for (const auto& iter : m_vecProp)
		{
			if (iter.eSize >= ESize::SIZE_DWORD)
			{
				iter.pProp->SetValue(L"0");
				iter.pProp->Enable(FALSE);
			}
		}
		return;
	}

	//ESize::SIZE_DWORD//////////////////////////////////////////////
	for (const auto& iter : m_vecProp)
		if (iter.eSize == ESize::SIZE_DWORD)
			iter.pProp->Enable(TRUE);

	auto dword = m_pHexCtrl->GetData<DWORD>(ullOffset);
	if (m_fBigEndian)
		dword = _byteswap_ulong(dword);

	ShowNAME_LONG(dword);
	ShowNAME_ULONG(dword);
	ShowNAME_FLOAT(dword);
	ShowNAME_TIME32(dword);
	ShowNAME_MSDOSTIME(dword);
	ShowNAME_MSDTTMTIME(dword);

	if (ullOffset + static_cast<unsigned>(ESize::SIZE_QWORD) > ullDataSize)
	{
		for (const auto& iter : m_vecProp)
		{
			if (iter.eSize >= ESize::SIZE_QWORD)
			{
				iter.pProp->SetValue(L"0");
				iter.pProp->Enable(FALSE);
			}
		}
		return;
	}

	//ESize::SIZE_QWORD//////////////////////////////////////////////////
	for (const auto& iter : m_vecProp)
		if (iter.eSize == ESize::SIZE_QWORD)
			iter.pProp->Enable(TRUE);

	auto qword = m_pHexCtrl->GetData<QWORD>(ullOffset);
	if (m_fBigEndian)
		qword = _byteswap_uint64(qword);

	ShowNAME_LONGLONG(qword);
	ShowNAME_ULONGLONG(qword);
	ShowNAME_DOUBLE(qword);
	ShowNAME_TIME64(qword);
	ShowNAME_FILETIME(qword);
	ShowNAME_OLEDATETIME(qword);
	ShowNAME_JAVATIME(qword);

	if (ullOffset + static_cast<unsigned>(ESize::SIZE_DQWORD) > ullDataSize)
	{
		for (const auto& iter : m_vecProp)
		{
			if (iter.eSize >= ESize::SIZE_DQWORD)
			{
				iter.pProp->SetValue(L"0");
				iter.pProp->Enable(FALSE);
			}
		}
		return;
	}

	//ESize::SIZE_DQWORD//////////////////////////////////////////////////
	for (const auto& iter : m_vecProp)
		if (iter.eSize == ESize::SIZE_DQWORD)
			iter.pProp->Enable(TRUE);

	auto dqword = m_pHexCtrl->GetData<DQWORD128>(ullOffset);
	if (m_fBigEndian)
	{
		//TODO: Test this thoroughly
		QWORD tmp = dqword.Value.qwLow;
		dqword.Value.qwLow = _byteswap_uint64(dqword.Value.qwHigh);
		dqword.Value.qwHigh = _byteswap_uint64(tmp);
	}

	ShowNAME_GUID(dqword);
	ShowNAME_GUIDTIME(dqword);
	ShowNAME_SYSTEMTIME(dqword);
}

void CHexDlgDataInterpret::OnClose()
{
	m_ullSize = 0;
	CDialogEx::OnClose();
}

LRESULT CHexDlgDataInterpret::OnPropertyChanged(WPARAM wParam, LPARAM lParam)
{
	if (wParam == IDC_HEXCTRL_DATAINTERPRET_PROPERTY_DATA)
	{
		m_pPropChanged = reinterpret_cast<CMFCPropertyGridProperty*>(lParam);
		OnOK();
	}

	return 0;
}

BOOL CHexDlgDataInterpret::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT * pResult)
{
	if (wParam == HEXCTRL_PROPGRIDCTRL)
	{
		auto pHdr = reinterpret_cast<NMHDR*>(lParam);
		if (pHdr->code != HEXCTRL_PROPGRIDCTRL_SELCHANGED)
			return FALSE;

		auto pData = std::find_if(m_vecProp.begin(), m_vecProp.end(),
			[this](const GRIDDATA& refData) {return refData.pProp == m_stCtrlGrid.GetCurrentProp(); });

		if (pData != m_vecProp.end())
		{
			switch (pData->eSize)
			{
			case ESize::SIZE_BYTE:
				m_ullSize = static_cast<ULONGLONG>(ESize::SIZE_BYTE);
				break;
			case ESize::SIZE_WORD:
				m_ullSize = static_cast<ULONGLONG>(ESize::SIZE_WORD);
				break;
			case ESize::SIZE_DWORD:
				m_ullSize = static_cast<ULONGLONG>(ESize::SIZE_DWORD);
				break;
			case ESize::SIZE_QWORD:
				m_ullSize = static_cast<ULONGLONG>(ESize::SIZE_QWORD);
				break;
			case ESize::SIZE_DQWORD:
				m_ullSize = static_cast<ULONGLONG>(ESize::SIZE_DQWORD);
				break;
			}
			UpdateHexCtrl();
		}
	}

	return CDialogEx::OnNotify(wParam, lParam, pResult);
}

void CHexDlgDataInterpret::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if (m_fVisible)
		m_stCtrlGrid.GetHeaderCtrl().SetItem(0, &m_hdItemPropGrid); //Property grid column size.
}

void CHexDlgDataInterpret::OnClickRadioLe()
{
	m_fBigEndian = false;
	InspectOffset(m_ullOffset);
}

void CHexDlgDataInterpret::OnClickRadioBe()
{
	m_fBigEndian = true;
	InspectOffset(m_ullOffset);
}

void CHexDlgDataInterpret::OnClickRadioDec()
{
	m_fShowAsHex = false;
	InspectOffset(m_ullOffset);
}

void CHexDlgDataInterpret::OnClickRadioHex()
{
	m_fShowAsHex = true;
	InspectOffset(m_ullOffset);
}

ULONGLONG CHexDlgDataInterpret::GetSize()
{
	return m_ullSize;
}

BOOL CHexDlgDataInterpret::ShowWindow(int nCmdShow)
{
	if (!m_pHexCtrl)
		return FALSE;

	m_fVisible = nCmdShow == SW_SHOW;
	if (m_fVisible)
		InspectOffset(m_pHexCtrl->GetCaretPos());

	return CWnd::ShowWindow(nCmdShow);
}

void CHexDlgDataInterpret::UpdateHexCtrl()
{
	if (m_pHexCtrl)
		m_pHexCtrl->RedrawWindow();
}

CString CHexDlgDataInterpret::GetCurrentUserDateFormatString()
{
	CString sResult;

	switch (m_dwDateFormat)
	{
	case 0:	//0=Month-Day-Year
		sResult.Format(L"mm%sdd%syyyy", m_wszDateSeparator, m_wszDateSeparator);
		break;
	case 2:	//2=Year-Month-Day 
		sResult.Format(L"yyyy%smm%sdd", m_wszDateSeparator, m_wszDateSeparator);
		break;
	default: //1=Day-Month-Year (default)
		sResult.Format(L"dd%smm%syyyy", m_wszDateSeparator, m_wszDateSeparator);
		break;
	}

	return sResult;
}

CString CHexDlgDataInterpret::SystemTimeToString(const SYSTEMTIME* pSysTime, bool bIncludeDate, bool bIncludeTime)
{
	if (!pSysTime)
		return L"Invalid";

	if (pSysTime->wDay > 0 && pSysTime->wDay < 32 && pSysTime->wMonth > 0 && pSysTime->wMonth < 13
		&& pSysTime->wYear < 10000 && pSysTime->wHour < 24 && pSysTime->wMinute < 60 && pSysTime->wSecond < 60
		&& pSysTime->wMilliseconds < 1000)
	{
		//Generate human formatted date. Fall back to UK/European if unable to determine
		CString sResult;
		if (bIncludeDate)
		{
			switch (m_dwDateFormat)
			{
			case 0:	//0=Month-Day-Year
				sResult.Format(L"%.2d%s%.2d%s%.4d", pSysTime->wMonth, m_wszDateSeparator, pSysTime->wDay, m_wszDateSeparator, pSysTime->wYear);
				break;
			case 2:	//2=Year-Month-Day 
				sResult.Format(L"%.4d%s%.2d%s%.2d", pSysTime->wYear, m_wszDateSeparator, pSysTime->wMonth, m_wszDateSeparator, pSysTime->wDay);
				break;
			default: //1=Day-Month-Year (default)
				sResult.Format(L"%.2d%s%.2d%s%.4d", pSysTime->wDay, m_wszDateSeparator, pSysTime->wMonth, m_wszDateSeparator, pSysTime->wYear);
			}

			if (bIncludeTime)
				sResult.Append(L" ");
		}

		//Append optional time elements
		if (bIncludeTime)
		{
			sResult.AppendFormat(L"%.2d:%.2d:%.2d", pSysTime->wHour, pSysTime->wMinute, pSysTime->wSecond);

			if (pSysTime->wMilliseconds > 0)
				sResult.AppendFormat(L".%.3d", pSysTime->wMilliseconds);
		}

		return sResult;
	}

	return NOTAPPLICABLE;
}

bool CHexDlgDataInterpret::StringToSystemTime(std::wstring_view wstrDateTime, PSYSTEMTIME pSysTime, bool bIncludeDate, bool bIncludeTime)
{
	if (wstrDateTime.empty() || pSysTime == nullptr)
		return false;

	std::memset(pSysTime, 0, sizeof(SYSTEMTIME));

	//Normalise the input string by replacing non-numeric characters except space with /
	//This should regardless of the current date/time separator character
	CString sDateTimeCooked;
	for (const auto& iter : wstrDateTime)
	{
		if (iswdigit(iter) || iter == L' ')
			sDateTimeCooked.AppendChar(iter);
		else
			sDateTimeCooked.AppendChar(L'/');
	}

	//Parse date component
	if (bIncludeDate)
	{
		switch (m_dwDateFormat)
		{
		case 0:	//0=Month-Day-Year
			swscanf_s(sDateTimeCooked, L"%2hu/%2hu/%4hu", &pSysTime->wMonth, &pSysTime->wDay, &pSysTime->wYear);
			break;
		case 2:	//2=Year-Month-Day 
			swscanf_s(sDateTimeCooked, L"%4hu/%2hu/%2hu", &pSysTime->wYear, &pSysTime->wMonth, &pSysTime->wDay);
			break;
		default: //1=Day-Month-Year (default)
			swscanf_s(sDateTimeCooked, L"%2hu/%2hu/%4hu", &pSysTime->wDay, &pSysTime->wMonth, &pSysTime->wYear);
			break;
		}

		//Find time seperator (if present)
		int nPos = sDateTimeCooked.Find(L" ");
		if (nPos > 0)
			sDateTimeCooked = sDateTimeCooked.Mid(nPos + 1);
	}

	//Parse time component
	if (bIncludeTime)
	{
		swscanf_s(sDateTimeCooked, L"%2hu/%2hu/%2hu/%3hu", &pSysTime->wHour, &pSysTime->wMinute,
			&pSysTime->wSecond, &pSysTime->wMilliseconds);

		//Ensure valid SYSTEMTIME is calculated but only if time was specified but date was not
		if (!bIncludeDate)
		{
			pSysTime->wYear = 1900;
			pSysTime->wMonth = 1;
			pSysTime->wDay = 1;
		}
	}

	FILETIME ftValidCheck;

	return SystemTimeToFileTime(pSysTime, &ftValidCheck);
}

bool CHexDlgDataInterpret::StringToGuid(std::wstring_view pwszSource, LPGUID pGUIDResult)
{
	//Alternative to UuidFromString() that does not require Rpcrt4.dll

	//Check arguments
	if (pwszSource.empty() || pGUIDResult == nullptr)
		return false;

	//Make working copy of source data and empty result GUID
	CString sBuffer = pwszSource.data();
	std::memset(pGUIDResult, 0, sizeof(GUID));

	//Make lower-case and strip any leading or trailing spaces
	sBuffer = sBuffer.MakeLower();
	sBuffer = sBuffer.Trim();

	//Remove all but permitted lower-case hex characters
	CString sResult;
	for (int i = 0; i < sBuffer.GetLength(); i++)
	{
		//TODO: Recode using _istxdigit() - See BinUtil.cpp
		//
		const CString sPermittedHexChars = _T("0123456789abcdef");
		const CString sCurrentCharacter = sBuffer.Mid(i, 1);
		const CString sIgnoredChars = _T("{-}");

		//Test if this character is a permitted hex character - 0123456789abcdef
		if (sPermittedHexChars.FindOneOf(sCurrentCharacter) >= 0)
		{
			sResult.Append(sCurrentCharacter);
		}
		else if (sIgnoredChars.FindOneOf(sCurrentCharacter) >= 0)
		{
			//Do nothing - We always ignore {, } and -
		}
		//Quit due to invalid data
		else
			return false;
	}

	//Confirm we now have exactly 32 characters. If we don't then game over
	//NB: We now have a stripped GUID that is exactly 32 chars long
	if (sResult.GetLength() != 32)
		return false;

	//%.8x pGuid->Data1
	pGUIDResult->Data1 = wcstoul(sResult.Mid(0, 8), nullptr, 16);

	//%.4x pGuid->Data2
	pGUIDResult->Data2 = static_cast<unsigned short>(wcstoul(sResult.Mid(8, 4), nullptr, 16));

	//%.4x pGuid->Data3
	pGUIDResult->Data3 = static_cast<unsigned short>(wcstoul(sResult.Mid(12, 4), nullptr, 16));

	//%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x pGuid->Data4[0], pGuid->Data4[1], pGuid->Data4[2], 
	//pGuid->Data4[3], pGuid->Data4[4], pGuid->Data4[5], pGuid->Data4[6], pGuid->Data4[7] 
	pGUIDResult->Data4[0] = static_cast<unsigned char>(wcstoul(sResult.Mid(16, 2), nullptr, 16));
	pGUIDResult->Data4[1] = static_cast<unsigned char>(wcstoul(sResult.Mid(18, 2), nullptr, 16));
	pGUIDResult->Data4[2] = static_cast<unsigned char>(wcstoul(sResult.Mid(20, 2), nullptr, 16));
	pGUIDResult->Data4[3] = static_cast<unsigned char>(wcstoul(sResult.Mid(22, 2), nullptr, 16));
	pGUIDResult->Data4[4] = static_cast<unsigned char>(wcstoul(sResult.Mid(24, 2), nullptr, 16));
	pGUIDResult->Data4[5] = static_cast<unsigned char>(wcstoul(sResult.Mid(26, 2), nullptr, 16));
	pGUIDResult->Data4[6] = static_cast<unsigned char>(wcstoul(sResult.Mid(28, 2), nullptr, 16));
	pGUIDResult->Data4[7] = static_cast<unsigned char>(wcstoul(sResult.Mid(30, 2), nullptr, 16));

	return true;
}

void CHexDlgDataInterpret::ShowNAME_BINARY(BYTE byte)
{
	WCHAR buff[9];
	swprintf_s(buff, _countof(buff), L"%s%s", NIBBLES[byte >> 4], NIBBLES[byte & 0x0F]);
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_BINARY; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_CHAR(BYTE byte)
{
	std::wstring wstrFormat { };
	if (m_fShowAsHex)
		wstrFormat = L"0x%hhX";
	else
		wstrFormat = L"%hhi";

	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), wstrFormat.data(), static_cast<char>(byte));
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_CHAR; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_UCHAR(BYTE byte)
{
	std::wstring wstrFormat { };
	if (m_fShowAsHex)
		wstrFormat = L"0x%hhX";
	else
		wstrFormat = L"%hhu";

	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), wstrFormat.data(), byte);
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_UCHAR; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_SHORT(WORD word)
{
	std::wstring wstrFormat { };
	if (m_fShowAsHex)
		wstrFormat = L"0x%hX";
	else
		wstrFormat = L"%hi";

	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), wstrFormat.data(), static_cast<short>(word));
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_SHORT; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_USHORT(WORD word)
{
	std::wstring wstrFormat { };
	if (m_fShowAsHex)
		wstrFormat = L"0x%hX";
	else
		wstrFormat = L"%hu";

	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), wstrFormat.data(), word);
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_USHORT; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_LONG(DWORD dword)
{
	std::wstring wstrFormat { };
	if (m_fShowAsHex)
		wstrFormat = L"0x%X";
	else
		wstrFormat = L"%i";

	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), wstrFormat.data(), static_cast<int>(dword));
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_LONG; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_ULONG(DWORD dword)
{
	std::wstring wstrFormat { };
	if (m_fShowAsHex)
		wstrFormat = L"0x%X";
	else
		wstrFormat = L"%u";

	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), wstrFormat.data(), dword);
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_ULONG; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_FLOAT(DWORD dword)
{
	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), L"%.9e", *reinterpret_cast<const float*>(&dword));
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_FLOAT; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_TIME32(DWORD dword)
{
	std::wstring wstrTime = NOTAPPLICABLE;

	//The number of seconds since midnight January 1st 1970 UTC (32-bit). This is signed and wraps on 19 January 2038 
	LONG lDiffSeconds = (LONG)dword;

	//Unix times are signed and value before 1st January 1970 is not considered valid
	//This is apparently because early complilers didn't support unsigned types. _mktime32() has the same limit
	if (lDiffSeconds >= 0)
	{
		//Add seconds from epoch time
		LARGE_INTEGER Time;
		Time.HighPart = FILETIME1970_HIGH;
		Time.LowPart = FILETIME1970_LOW;
		Time.QuadPart += ((LONGLONG)lDiffSeconds) * FTTICKSPERSECOND;

		//Convert to FILETIME
		FILETIME ftTime;
		ftTime.dwHighDateTime = Time.HighPart;
		ftTime.dwLowDateTime = Time.LowPart;

		//Convert to SYSTEMTIME for display
		SYSTEMTIME SysTime{ };
		if (FileTimeToSystemTime(&ftTime, &SysTime))
			wstrTime = SystemTimeToString(&SysTime, true, true).GetString();
	}

	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_TIME32T; }); iter != m_vecProp.end())
		iter->pProp->SetValue(wstrTime.data());
}

void CHexDlgDataInterpret::ShowNAME_MSDOSTIME(DWORD dword)
{
	std::wstring wstrTime = NOTAPPLICABLE;
	SYSTEMTIME SysTime { };
	FILETIME ftMSDOS;
	MSDOSDATETIME msdosDateTime;
	msdosDateTime.dwTimeDate = dword;
	if (DosDateTimeToFileTime(msdosDateTime.TimeDate.wDate, msdosDateTime.TimeDate.wTime, &ftMSDOS))
	{
		if (FileTimeToSystemTime(&ftMSDOS, &SysTime))
			wstrTime = SystemTimeToString(&SysTime, true, true).GetString();
	}

	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_MSDOSTIME; }); iter != m_vecProp.end())
		iter->pProp->SetValue(wstrTime.data());
}

void CHexDlgDataInterpret::ShowNAME_MSDTTMTIME(DWORD dword)
{
	//Microsoft DTTM time (as used by Microsoft Compound Document format)
	std::wstring wstrTime = NOTAPPLICABLE;
	SYSTEMTIME SysTime { };
	DTTM dttm;
	dttm.dwValue = dword;
	if (dttm.components.dayofmonth > 0 && dttm.components.dayofmonth < 32
		&& dttm.components.hour < 24 && dttm.components.minute < 60
		&& dttm.components.month>0 && dttm.components.month < 13 && dttm.components.weekday < 7)
	{
		SysTime.wYear = 1900 + static_cast<WORD>(dttm.components.year);
		SysTime.wMonth = dttm.components.month;
		SysTime.wDayOfWeek = dttm.components.weekday;
		SysTime.wDay = dttm.components.dayofmonth;
		SysTime.wHour = dttm.components.hour;
		SysTime.wMinute = dttm.components.minute;
		SysTime.wSecond = 0;
		SysTime.wMilliseconds = 0;
		wstrTime = SystemTimeToString(&SysTime, true, true).GetString();
	}

	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_MSDTTMTIME; }); iter != m_vecProp.end())
		iter->pProp->SetValue(wstrTime.data());
}

void CHexDlgDataInterpret::ShowNAME_LONGLONG(QWORD qword)
{
	std::wstring wstrFormat { };
	if (m_fShowAsHex)
		wstrFormat = L"0x%llX";
	else
		wstrFormat = L"%lli";

	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), wstrFormat.data(), static_cast<long long>(qword));
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_LONGLONG; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_ULONGLONG(QWORD qword)
{
	std::wstring wstrFormat { };
	if (m_fShowAsHex)
		wstrFormat = L"0x%llX";
	else
		wstrFormat = L"%llu";

	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), wstrFormat.data(), qword);
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_ULONGLONG; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_DOUBLE(QWORD qword)
{
	WCHAR buff[32];
	swprintf_s(buff, _countof(buff), L"%.18e", *reinterpret_cast<const double*>(&qword));
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_DOUBLE; }); iter != m_vecProp.end())
		iter->pProp->SetValue(buff);
}

void CHexDlgDataInterpret::ShowNAME_TIME64(QWORD qword)
{
	std::wstring wstrTime = NOTAPPLICABLE;

	//The number of seconds since midnight January 1st 1970 UTC (64-bit). This is signed
	LONGLONG llDiffSeconds = (LONGLONG)qword;
	
	//Unix times are signed and value before 1st January 1970 is not considered valid
	//This is apparently because early complilers didn't support unsigned types. _mktime64() has the same limit
	if (llDiffSeconds >= 0)
	{
		//Add seconds from epoch time
		LARGE_INTEGER Time;
		Time.HighPart = FILETIME1970_HIGH;
		Time.LowPart = FILETIME1970_LOW;
		Time.QuadPart += llDiffSeconds * FTTICKSPERSECOND;

		//Convert to FILETIME
		FILETIME ftTime;
		ftTime.dwHighDateTime = Time.HighPart;
		ftTime.dwLowDateTime = Time.LowPart;

		//Convert to SYSTEMTIME for display
		SYSTEMTIME SysTime{ };
		if (FileTimeToSystemTime(&ftTime, &SysTime))
			wstrTime = SystemTimeToString(&SysTime, true, true).GetString();
	}	

	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_TIME64T; }); iter != m_vecProp.end())
		iter->pProp->SetValue(wstrTime.data());
}

void CHexDlgDataInterpret::ShowNAME_FILETIME(QWORD qword)
{
	std::wstring wstrTime = NOTAPPLICABLE;
	SYSTEMTIME SysTime { };
	if (FileTimeToSystemTime(reinterpret_cast<const FILETIME*>(&qword), &SysTime))
		wstrTime = SystemTimeToString(&SysTime, true, true).GetString();

	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_FILETIME; }); iter != m_vecProp.end())
		iter->pProp->SetValue(wstrTime.data());
}

void CHexDlgDataInterpret::ShowNAME_OLEDATETIME(QWORD qword)
{
	//OLE (including MS Office) date/time
	//Implemented using an 8-byte floating-point number. Days are represented as whole number increments starting with 30 December 1899, midnight as time zero.
	//See: https://docs.microsoft.com/en-us/cpp/atl-mfc-shared/date-type?view=vs-2019

	std::wstring wstrTime = NOTAPPLICABLE;
	
	//Convert from ULL to Double cannot be done with static_cast?
	DATE date;
	std::memcpy(&date, &qword, sizeof(date));
	COleDateTime dt(date);

	SYSTEMTIME SysTime{ };
	if (dt.GetAsSystemTime(SysTime))
		wstrTime = SystemTimeToString(&SysTime, true, true).GetString();

	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_OLEDATETIME; }); iter != m_vecProp.end())
		iter->pProp->SetValue(wstrTime.data());

}

void CHexDlgDataInterpret::ShowNAME_JAVATIME(QWORD qword)
{
	//Javatime (signed)
	//Number of milliseconds after/before January 1, 1970, 00:00:00 UTC
	std::wstring wstrTime = NOTAPPLICABLE;
	
	//Add/subtract milliseconds from epoch time
	LARGE_INTEGER Time;
	Time.HighPart = FILETIME1970_HIGH;
	Time.LowPart = FILETIME1970_LOW;
	if (static_cast<LONGLONG>(qword) >= 0)
		Time.QuadPart += qword * FTTICKSPERMS;
	else
		Time.QuadPart -= qword * FTTICKSPERMS;

	//Convert to FILETIME
	FILETIME ftJavaTime;
	ftJavaTime.dwHighDateTime = Time.HighPart;
	ftJavaTime.dwLowDateTime = Time.LowPart;

	//Convert to SYSTEMTIME
	SYSTEMTIME SysTime{ };
	if (FileTimeToSystemTime(&ftJavaTime, &SysTime))
		wstrTime = SystemTimeToString(&SysTime, true, true).GetString();

	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_JAVATIME; }); iter != m_vecProp.end())
		iter->pProp->SetValue(wstrTime.data());
}

void CHexDlgDataInterpret::ShowNAME_GUID(const DQWORD128 & dqword)
{
	CString sGUID;
	sGUID.Format(_T("{%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}"),
		dqword.gGUID.Data1, dqword.gGUID.Data2, dqword.gGUID.Data3, dqword.gGUID.Data4[0],
		dqword.gGUID.Data4[1], dqword.gGUID.Data4[2], dqword.gGUID.Data4[3], dqword.gGUID.Data4[4],
		dqword.gGUID.Data4[5], dqword.gGUID.Data4[6], dqword.gGUID.Data4[7]);
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_GUID; }); iter != m_vecProp.end())
		iter->pProp->SetValue(sGUID);
}

void CHexDlgDataInterpret::ShowNAME_GUIDTIME(const DQWORD128 & dqword)
{
	//Guid v1 Datetime UTC
	//The time structure within the NAME_GUID.
	//First, verify GUID is actually version 1 style
	std::wstring wstrTime = NOTAPPLICABLE;
	SYSTEMTIME SysTime { };
	unsigned short unGuidVersion = (dqword.gGUID.Data3 & 0xf000) >> 12;

	if (unGuidVersion == 1)
	{
		LARGE_INTEGER qwGUIDTime;
		qwGUIDTime.LowPart = dqword.gGUID.Data1;
		qwGUIDTime.HighPart = dqword.gGUID.Data3 & 0x0fff;
		qwGUIDTime.HighPart = (qwGUIDTime.HighPart << 16) | dqword.gGUID.Data2;

		//RFC4122: The timestamp is a 60-bit value.  For UUID version 1, this is represented by Coordinated Universal Time (UTC) as a count of 100-
		//nanosecond intervals since 00:00:00.00, 15 October 1582 (the date of Gregorian reform to the Christian calendar).
		//
		//Both FILETIME and GUID time are based upon 100ns intervals
		//FILETIME is based upon 1 Jan 1601 whilst GUID time is from 1582. Subtract 6653 days to convert from GUID time
		//NB: 6653 days from 15 Oct 1582 to 1 Jan 1601
		//
		ULARGE_INTEGER ullSubtractTicks;
		ullSubtractTicks.QuadPart = static_cast<QWORD>(FTTICKSPERSECOND) * static_cast<QWORD>(SECONDSPERHOUR)
			* static_cast<QWORD>(HOURSPERDAY) * static_cast<QWORD>(FILETIME1582OFFSETDAYS);
		qwGUIDTime.QuadPart -= ullSubtractTicks.QuadPart;

		//Convert to SYSTEMTIME
		FILETIME ftGUIDTime;
		ftGUIDTime.dwHighDateTime = qwGUIDTime.HighPart;
		ftGUIDTime.dwLowDateTime = qwGUIDTime.LowPart;
		if (FileTimeToSystemTime(&ftGUIDTime, &SysTime))
			wstrTime = SystemTimeToString(&SysTime, true, true).GetString();
	}

	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_GUIDTIME; }); iter != m_vecProp.end())
		iter->pProp->SetValue(wstrTime.data());
}

void CHexDlgDataInterpret::ShowNAME_SYSTEMTIME(const DQWORD128 & dqword)
{
	if (auto iter = std::find_if(m_vecProp.begin(), m_vecProp.end(),
		[](const GRIDDATA& refData) {return refData.eName == EName::NAME_SYSTEMTIME; }); iter != m_vecProp.end())
		iter->pProp->SetValue(SystemTimeToString(reinterpret_cast<const SYSTEMTIME*>(&dqword), true, true).GetString());
}

bool CHexDlgDataInterpret::SetDataNAME_BINARY(std::wstring_view wstr)
{
	bool fSuccess { false };
	if (wstr.size() != 8 || wstr.find_first_not_of(L"01") != std::string::npos)
		return fSuccess;

	long lData = wcstol(wstr.data(), nullptr, 2);
	fSuccess = SetDigitData<UCHAR>(lData);

	return fSuccess;
}

bool CHexDlgDataInterpret::SetDataNAME_CHAR(std::wstring_view wstr)
{
	bool fSuccess { false };
	LONGLONG llData { };
	if (StrToInt64ExW(wstr.data(), STIF_SUPPORT_HEX, &llData))
		fSuccess = SetDigitData<CHAR>(llData);

	return fSuccess;
}

bool CHexDlgDataInterpret::SetDataNAME_UCHAR(std::wstring_view wstr)
{
	bool fSuccess { false };
	LONGLONG llData { };
	if (StrToInt64ExW(wstr.data(), STIF_SUPPORT_HEX, &llData))
		fSuccess = SetDigitData<UCHAR>(llData);

	return fSuccess;
}

bool CHexDlgDataInterpret::SetDataNAME_SHORT(std::wstring_view wstr)
{
	bool fSuccess { false };
	LONGLONG llData { };
	if (StrToInt64ExW(wstr.data(), STIF_SUPPORT_HEX, &llData))
		fSuccess = SetDigitData<SHORT>(llData);

	return fSuccess;
}

bool CHexDlgDataInterpret::SetDataNAME_USHORT(std::wstring_view wstr)
{
	bool fSuccess { false };
	LONGLONG llData { };
	if (StrToInt64ExW(wstr.data(), STIF_SUPPORT_HEX, &llData))
		fSuccess = SetDigitData<USHORT>(llData);

	return fSuccess;
}

bool CHexDlgDataInterpret::SetDataNAME_LONG(std::wstring_view wstr)
{
	bool fSuccess { false };
	LONGLONG llData { };
	if (StrToInt64ExW(wstr.data(), STIF_SUPPORT_HEX, &llData))
		fSuccess = SetDigitData<LONG>(llData);

	return fSuccess;
}

bool CHexDlgDataInterpret::SetDataNAME_ULONG(std::wstring_view wstr)
{
	bool fSuccess { false };
	LONGLONG llData { };
	if (StrToInt64ExW(wstr.data(), STIF_SUPPORT_HEX, &llData))
		fSuccess = SetDigitData<ULONG>(llData);

	return fSuccess;
}

bool CHexDlgDataInterpret::SetDataNAME_LONGLONG(std::wstring_view wstr)
{
	bool fSuccess { false };
	LONGLONG llData { };
	if (WCharsToll(wstr.data(), llData))
		fSuccess = SetDigitData<LONGLONG>(llData);

	return fSuccess;
}

bool CHexDlgDataInterpret::SetDataNAME_ULONGLONG(std::wstring_view wstr)
{
	bool fSuccess { false };
	ULONGLONG ullData;
	if (WCharsToUll(wstr.data(), ullData))
		fSuccess = SetDigitData<ULONGLONG>(static_cast<LONGLONG>(ullData));

	return fSuccess;
}

bool CHexDlgDataInterpret::SetDataNAME_FLOAT(std::wstring_view wstr)
{
	wchar_t* pEndPtr;
	float fl = wcstof(wstr.data(), &pEndPtr);
	if (fl == 0 && (pEndPtr == wstr.data() || *pEndPtr != '\0'))
		return false;

	//TODO: dwData = std::bit_cast<DWORD>(fl);
	DWORD dwData = *reinterpret_cast<DWORD*>(&fl);
	if (m_fBigEndian)
		dwData = _byteswap_ulong(dwData);
	m_pHexCtrl->SetData(m_ullOffset, dwData);

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_DOUBLE(std::wstring_view wstr)
{
	wchar_t* pEndPtr;
	double dd = wcstod(wstr.data(), &pEndPtr);
	if (dd == 0 && (pEndPtr == wstr.data() || *pEndPtr != '\0'))
		return false;

	QWORD qwData = *reinterpret_cast<QWORD*>(&dd);
	if (m_fBigEndian)
		qwData = _byteswap_uint64(qwData);
	m_pHexCtrl->SetData(m_ullOffset, qwData);

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_TIME32T(std::wstring_view wstr)
{
	//The number of seconds since midnight January 1st 1970 UTC (32-bit). This wraps on 19 January 2038 
	SYSTEMTIME stTime;
	if (!StringToSystemTime(wstr.data(), &stTime, true, true))
		return false;

	//Unix times are signed but value before 1st January 1970 is not considered valid
	//This is apparently because early complilers didn't support unsigned types. _mktime32() has the same limit
	if (stTime.wYear < 1970)
		return false;

	FILETIME ftTime;
	if (!SystemTimeToFileTime(&stTime, &ftTime))
		return false;

	//Convert ticks to seconds and adjust epoch
	LARGE_INTEGER lTicks;
	lTicks.HighPart = ftTime.dwHighDateTime;
	lTicks.LowPart = ftTime.dwLowDateTime;
	lTicks.QuadPart /= FTTICKSPERSECOND;
	lTicks.QuadPart -= UNIXEPOCHDIFFERENCE;

	if (lTicks.QuadPart < LONG_MAX)
	{
		LONG lTime32 = static_cast<LONG>(lTicks.QuadPart);

		if (m_fBigEndian)
			lTime32 = _byteswap_ulong(lTime32);

		m_pHexCtrl->SetData(m_ullOffset, static_cast<LONG>(lTime32));
	}

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_TIME64T(std::wstring_view wstr)
{
	//The number of seconds since midnight January 1st 1970 UTC (32-bit). This wraps on 19 January 2038 
	SYSTEMTIME stTime;
	if (!StringToSystemTime(wstr.data(), &stTime, true, true))
		return false;

	//Unix times are signed but value before 1st January 1970 is not considered valid
	//This is apparently because early complilers didn't support unsigned types. _mktime64() has the same limit
	if (stTime.wYear < 1970)
		return false;

	FILETIME ftTime;
	if (!SystemTimeToFileTime(&stTime, &ftTime))
		return false;

	//Convert ticks to seconds and adjust epoch
	LARGE_INTEGER lTicks;
	lTicks.HighPart = ftTime.dwHighDateTime;
	lTicks.LowPart = ftTime.dwLowDateTime;
	lTicks.QuadPart /= FTTICKSPERSECOND;
	lTicks.QuadPart -= UNIXEPOCHDIFFERENCE;
	auto llTime64 = static_cast<LONGLONG>(lTicks.QuadPart);

	if (m_fBigEndian)
		llTime64 = _byteswap_uint64(llTime64);

	m_pHexCtrl->SetData(m_ullOffset, static_cast<LONGLONG>(llTime64));

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_FILETIME(std::wstring_view wstr)
{
	SYSTEMTIME stTime;
	if (!StringToSystemTime(wstr.data(), &stTime, true, true))
		return false;

	FILETIME ftTime;
	if (!SystemTimeToFileTime(&stTime, &ftTime))
		return false;

	ULARGE_INTEGER ullTime;
	ullTime.LowPart = ftTime.dwLowDateTime;
	ullTime.HighPart = ftTime.dwHighDateTime;

	if (m_fBigEndian)
		ullTime.QuadPart = _byteswap_uint64(ullTime.QuadPart);

	m_pHexCtrl->SetData(m_ullOffset, ullTime.QuadPart);

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_OLEDATETIME(std::wstring_view wstr)
{
	SYSTEMTIME stTime;
	if (!StringToSystemTime(wstr.data(), &stTime, true, true))
		return false;

	COleDateTime dt(stTime);
	if (dt.GetStatus() != COleDateTime::valid)
		return false;
	
	//Cannot convert from Double to ULL with static_cast?
	ULONGLONG ullValue;
	std:memcpy(&ullValue, &dt.m_dt, sizeof(dt.m_dt));

	if (m_fBigEndian)
		ullValue = _byteswap_uint64(ullValue);

	m_pHexCtrl->SetData(m_ullOffset, ullValue);

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_JAVATIME(std::wstring_view wstr)
{
	SYSTEMTIME stTime;
	if (!StringToSystemTime(wstr.data(), &stTime, true, true))
		return false;

	FILETIME ftTime;
	if (!SystemTimeToFileTime(&stTime, &ftTime))
		return false;

	//Number of milliseconds after/before January 1, 1970, 00:00:00 UTC
	LARGE_INTEGER lJavaTicks;
	lJavaTicks.HighPart = ftTime.dwHighDateTime;
	lJavaTicks.LowPart = ftTime.dwLowDateTime;

	LARGE_INTEGER lEpochTicks;
	lEpochTicks.HighPart = FILETIME1970_HIGH;
	lEpochTicks.LowPart = FILETIME1970_LOW;

	LONGLONG llDiffTicks;
	if (lEpochTicks.QuadPart > lJavaTicks.QuadPart)
		llDiffTicks = -(lEpochTicks.QuadPart - lJavaTicks.QuadPart);
	else
		llDiffTicks = lJavaTicks.QuadPart - lEpochTicks.QuadPart;

	LONGLONG llDiffMillis = llDiffTicks / FTTICKSPERMS;

	if (m_fBigEndian)
		llDiffMillis = _byteswap_uint64(llDiffMillis);

	m_pHexCtrl->SetData(m_ullOffset, static_cast<LONGLONG>(llDiffMillis));

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_MSDOSTIME(std::wstring_view wstr)
{
	SYSTEMTIME stTime;
	if (!StringToSystemTime(wstr.data(), &stTime, true, true))
		return false;

	FILETIME ftTime;
	if (!SystemTimeToFileTime(&stTime, &ftTime))
		return false;

	MSDOSDATETIME msdosDateTime;
	if (!FileTimeToDosDateTime(&ftTime, &msdosDateTime.TimeDate.wDate, &msdosDateTime.TimeDate.wTime))
		return false;

	//Note: Big Endian not currently supported. This has never existed in the "wild"

	m_pHexCtrl->SetData(m_ullOffset, static_cast<DWORD>(msdosDateTime.dwTimeDate));

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_MSDTTMTIME(std::wstring_view wstr)
{
	SYSTEMTIME stTime;
	if (StringToSystemTime(wstr.data(), &stTime, true, true))
		return false;

	//Microsoft DTTM time (as used by Microsoft Compound Document format)
	DTTM dttm;
	dttm.components.year = stTime.wYear - 1900;
	dttm.components.month = stTime.wMonth;
	dttm.components.weekday = stTime.wDayOfWeek;
	dttm.components.dayofmonth = stTime.wDay;
	dttm.components.hour = stTime.wHour;
	dttm.components.minute = stTime.wMinute;

	//Note: Big Endian not currently supported. This has never existed in the "wild"

	m_pHexCtrl->SetData(m_ullOffset, static_cast<DWORD>(dttm.dwValue));

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_SYSTEMTIME(std::wstring_view wstr)
{
	SYSTEMTIME stTime;
	if (!StringToSystemTime(wstr.data(), &stTime, true, true))
		return false;

	//Note: Big Endian not currently supported. This has never existed in the "wild"

	m_pHexCtrl->SetData(m_ullOffset, stTime);

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_GUIDTIME(std::wstring_view wstr)
{
	//This time is within NAME_GUID structure, and it depends on it.
	//We can not just set a NAME_GUIDTIME for data range if it's not 
	//a valid NAME_GUID range, so checking first.

	auto dqword = m_pHexCtrl->GetData<DQWORD128>(m_ullOffset);
	unsigned short unGuidVersion = (dqword.gGUID.Data3 & 0xf000) >> 12;
	if (unGuidVersion != 1)
		return false;

	//RFC4122: The timestamp is a 60-bit value.  For UUID version 1, this is represented by Coordinated Universal Time (UTC) as a count of 100-
	//nanosecond intervals since 00:00:00.00, 15 October 1582 (the date of Gregorian reform to the Christian calendar).
	//
	//Both FILETIME and GUID time are based upon 100ns intervals
	//FILETIME is based upon 1 Jan 1601 whilst GUID time is from 1582. Add 6653 days to convert to GUID time
	SYSTEMTIME stTime;
	if (!StringToSystemTime(wstr.data(), &stTime, true, true))
		return false;

	FILETIME ftTime;
	if (!SystemTimeToFileTime(&stTime, &ftTime))
		return false;

	LARGE_INTEGER qwGUIDTime;
	qwGUIDTime.HighPart = ftTime.dwHighDateTime;
	qwGUIDTime.LowPart = ftTime.dwLowDateTime;

	ULARGE_INTEGER ullAddTicks;
	ullAddTicks.QuadPart = static_cast<QWORD>(FTTICKSPERSECOND) * static_cast<QWORD>(SECONDSPERHOUR)
		* static_cast<QWORD>(HOURSPERDAY) * static_cast<QWORD>(FILETIME1582OFFSETDAYS);
	qwGUIDTime.QuadPart += ullAddTicks.QuadPart;

	//Encode version 1 GUID with time
	dqword.gGUID.Data1 = qwGUIDTime.LowPart;
	dqword.gGUID.Data2 = qwGUIDTime.HighPart & 0xffff;
	dqword.gGUID.Data3 = ((qwGUIDTime.HighPart >> 16) & 0x0fff) | 0x1000; //Including Type 1 flag (0x1000)

	m_pHexCtrl->SetData(m_ullOffset, dqword);

	return true;
}

bool CHexDlgDataInterpret::SetDataNAME_GUID(std::wstring_view wstr)
{
	GUID guid { };
	if (!StringToGuid(wstr, &guid))
		return false;

	m_pHexCtrl->SetData(m_ullOffset, guid);

	return true;
}