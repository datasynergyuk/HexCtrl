/****************************************************************************************
* Copyright © 2018-2020 Jovibor https://github.com/jovibor/                             *
* This is a Hex Control for MFC applications.                                           *
* Official git repository of the project: https://github.com/jovibor/HexCtrl/           *
* This software is available under the "MIT License modified with The Commons Clause".  *
* https://github.com/jovibor/HexCtrl/blob/master/LICENSE                                *
* For more information visit the project's official repository.                         *
****************************************************************************************/
#pragma once
#include "../../res/HexCtrlRes.h"
#include "../CHexCtrl.h"
#include <afxdialogex.h>  //Standard MFC's controls header.

namespace HEXCTRL::INTERNAL {
	/***************************************************************************************
	* ESearchMode - type of the search, also used in CHexDlgSearch.                        *
	***************************************************************************************/
	enum class ESearchMode : DWORD
	{
		SEARCH_HEX, SEARCH_ASCII, SEARCH_UTF16
	};

	/********************************************
	* CHexDlgSearch class definition.			*
	********************************************/
	class CHexDlgSearch final : public CDialogEx
	{
	public:
		explicit CHexDlgSearch(CWnd* pParent = nullptr) : CDialogEx(IDD_HEXCTRL_SEARCH, pParent) {}
		BOOL Create(UINT nIDTemplate, CHexCtrl* pHexCtrl);
		void Search(bool fForward);
		[[nodiscard]] bool IsSearchAvail(); //Can we do search next/prev?
	protected:
		void DoDataExchange(CDataExchange* pDX)override;
		BOOL OnInitDialog()override;
		afx_msg void OnButtonSearchF();
		afx_msg void OnButtonSearchB();
		afx_msg void OnButtonReplace();
		afx_msg void OnButtonReplaceAll();
		afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
		afx_msg void OnCancel()override;
		afx_msg void OnDestroy();
		BOOL PreTranslateMessage(MSG* pMsg)override;
		HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
		void OnRadioBnRange(UINT nID);
		bool PrepareSearch();
	private:
		[[nodiscard]] CHexCtrl* GetHexCtrl()const;
		//ullStart will return index of found occurence, if any.
		bool DoSearch(ULONGLONG& ullStart, ULONGLONG ullEnd, std::byte* pSearch, size_t nSize, bool fForward = true);
		void Search();
		void Replace(ULONGLONG ullIndex, std::byte* pData, size_t nSizeData, size_t m_nSizeReplace, bool fRedraw = true);
		void ResetSearch();
		[[nodiscard]] ESearchMode GetSearchMode(); //Returns current search mode.
		void ComboSearchFill(LPCWSTR pwsz);
		void ComboReplaceFill(LPCWSTR pwsz);
		DECLARE_MESSAGE_MAP()
	private:
		CHexCtrl* m_pHexCtrl { };
		UINT m_uRadioCurrent { };
		const COLORREF m_clrSearchFailed { RGB(200, 0, 0) };
		const COLORREF m_clrSearchFound { RGB(0, 200, 0) };
		const COLORREF m_clrBkTextArea { GetSysColor(COLOR_MENU) };
		CBrush m_stBrushDefault;
		std::wstring m_wstrTextSearch { };  //String to search for.
		std::wstring m_wstrTextReplace { }; //Search "Replace with..." wstring.
		ULONGLONG m_ullSearchStart { };
		ULONGLONG m_ullSearchEnd { };
		ULONGLONG m_ullOffset { };       //An offset search should start from.
		DWORD m_dwCount { };             //How many, or what index number.
		DWORD m_dwReplaced { };          //Replaced amount;
		int m_iDirection { };            //Search direction: 1 = Forward, -1 = Backward.
		int m_iWrap { };                 //Wrap direction: -1 = Beginning, 1 = End.
		bool m_fWrap { false };          //Was search wrapped?
		bool m_fSecondMatch { false };   //First or subsequent match. 
		bool m_fFound { false };         //Found or not.
		bool m_fDoCount { true };        //Do we count matches or just print "Found".
		bool m_fReplace { false };       //Find or Find and Replace with...?
		bool m_fAll { false };           //Find/Replace one by one, or all?
		bool m_fReplaceWarning { true }; //Show Replace string size exceeds Warning or not.
		bool m_fSelection { false };     //Search in selection.
		std::byte* m_pSearchData { };
		std::byte* m_pReplaceData { };
		size_t m_nSizeSearch { };
		size_t m_nSizeReplace { };
		std::string m_strSearch;
		std::string m_strReplace;
	};
}