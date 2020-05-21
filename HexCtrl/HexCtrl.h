/****************************************************************************************
* Copyright © 2018-2020 Jovibor https://github.com/jovibor/                             *
* This is a Hex Control for MFC/Win32 applications.                                     *
* Official git repository: https://github.com/jovibor/HexCtrl/                          *
* This software is available under the "MIT License modified with The Commons Clause".  *
* https://github.com/jovibor/HexCtrl/blob/master/LICENSE                                *
* For more information visit the project's official repository.                         *
****************************************************************************************/
#pragma once
#include <Windows.h> //Standard Windows header.
#include <deque>
#include <memory>    //std::shared/unique_ptr and related.
#include <optional>
#include <string>
#include <vector>

#ifndef __cpp_lib_byte
static_assert(false, "std::byte compliant compiler required.");
#endif

/**********************************************************************
* If HEXCTRL_IHEXCTRLPTR_UNIQUEPTR defined then IHexCtrlPtr is        *
* resolved to std::unique_ptr. Otherwise it's std::shared_ptr.        *
**********************************************************************/
#define HEXCTRL_IHEXCTRLPTR_UNIQUEPTR

/**********************************************************************
* If HexCtrl is to be used as a .dll, then include this header,       *
* and uncomment the line below.                                       *
**********************************************************************/
//#define HEXCTRL_SHARED_DLL

/**********************************************************************
* For manually initialize MFC.                                        *
* This macro is used only for Win32 non MFC projects, with HexCtrl    *
* built from sources, with "Use MFC in a Shared DLL" setting.         *
**********************************************************************/
//#define HEXCTRL_MANUAL_MFC_INIT

namespace HEXCTRL
{
	/********************************************************************************************
	* EHexCmd - Enum of the commands that can be executed within HexCtrl, used in ExecuteCmd.   *
	********************************************************************************************/
	enum class EHexCmd : WORD
	{
		CMD_SEARCH = 0x01, CMD_SEARCH_NEXT, CMD_SEARCH_PREV,
		CMD_SHOWDATA_BYTE, CMD_SHOWDATA_WORD, CMD_SHOWDATA_DWORD, CMD_SHOWDATA_QWORD,
		CMD_BKM_ADD, CMD_BKM_REMOVE, CMD_BKM_NEXT, CMD_BKM_PREV, CMD_BKM_CLEARALL, CMD_BKM_MANAGER,
		CMD_CLIPBOARD_COPY_HEX, CMD_CLIPBOARD_COPY_HEXLE, CMD_CLIPBOARD_COPY_HEXFMT, CMD_CLIPBOARD_COPY_TEXT,
		CMD_CLIPBOARD_COPY_BASE64, CMD_CLIPBOARD_COPY_CARR, CMD_CLIPBOARD_COPY_GREPHEX, CMD_CLIPBOARD_COPY_PRNTSCRN,
		CMD_CLIPBOARD_PASTE_HEX, CMD_CLIPBOARD_PASTE_TEXT,
		CMD_MODIFY_OPERS, CMD_MODIFY_FILLZEROS, CMD_MODIFY_FILLDATA, CMD_MODIFY_UNDO, CMD_MODIFY_REDO,
		CMD_SEL_MARKSTART, CMD_SEL_MARKEND, CMD_SEL_SELECTALL,
		CMD_DATAINTERPRET,
		CMD_APPEARANCE_FONTINC, CMD_APPEARANCE_FONTDEC, CMD_APPEARANCE_CAPACITYINC, CMD_APPEARANCE_CAPACITYDEC,
		CMD_PRINT, CMD_ABOUT,
		CMD_CODEPAGE
	};

	/********************************************************************************************
	* EHexCreateMode - Enum of HexCtrl creation mode.                                           *
	********************************************************************************************/
	enum class EHexCreateMode : WORD
	{
		CREATE_CHILD, CREATE_POPUP, CREATE_CUSTOMCTRL
	};

	/********************************************************************************************
	* EHexShowMode - current data mode representation.                                          *
	********************************************************************************************/
	enum class EHexShowMode : WORD
	{
		ASBYTE = 1, ASWORD = 2, ASDWORD = 4, ASQWORD = 8
	};

	/********************************************************************************************
	* EHexDataMode - Enum of the working data mode, used in HEXDATASTRUCT in SetData.           *
	* DATA_MEMORY: Default standard data mode.                                                  *
	* DATA_MSG: Data is handled through WM_NOTIFY messages in handler window.				    *
	* DATA_VIRTUAL: Data is handled through IHexVirtData interface by derived class.            *
	********************************************************************************************/
	enum class EHexDataMode : WORD
	{
		DATA_MEMORY, DATA_MSG, DATA_VIRTUAL
	};

	/********************************************************************************************
	* EHexDlg - control's modeless dialogs.                                                     *
	********************************************************************************************/
	enum class EHexDlg : WORD
	{
		DLG_BKMMANAGER, DLG_DATAINTERPRET, DLG_FILLDATA, DLG_OPERS, DLG_SEARCH
	};

	/********************************************************************************************
	* HEXSPANSTRUCT - Data offset and size, used in some data/size related routines.            *
	********************************************************************************************/
	struct HEXSPANSTRUCT
	{
		ULONGLONG ullOffset { };
		ULONGLONG ullSize { };
	};

	/********************************************************************************************
	* HEXBOOKMARKSTRUCT - Bookmarks.                                                            *
	********************************************************************************************/
	struct HEXBOOKMARKSTRUCT
	{
		std::vector<HEXSPANSTRUCT> vecSpan { };                //Vector of offsets and sizes.
		std::wstring               wstrDesc { };               //Bookmark description.
		ULONGLONG                  ullID { };                  //Bookmark ID, assigned internally by framework.
		ULONGLONG                  ullData { };                //User defined custom data.
		COLORREF                   clrBk { RGB(240, 240, 0) }; //Bk color.
		COLORREF                   clrText { RGB(0, 0, 0) };   //Text color.
	};
	using PHEXBOOKMARKSTRUCT = HEXBOOKMARKSTRUCT*;

	/********************************************************************************************
	* IHexVirtData - Pure abstract data handler class, that can be implemented by client,       *
	* to set its own data handler routines.	Works in EHexDataMode::DATA_VIRTUAL mode.           *
	* Pointer to this class can be set in IHexCtrl::SetData method.                             *
	* Its usage is very similar to DATA_MSG logic, where control sends WM_NOTIFY messages       *
	* to set window to get/set data. But in this case it's just a pointer to a custom           *
	* routine's implementation.                                                                 *
	* All virtual functions must be defined in client's derived class.                          *
	********************************************************************************************/
	class IHexVirtData
	{
	public:
		[[nodiscard]] virtual std::byte* GetData(const HEXSPANSTRUCT&) = 0; //Data index and size to get.
		virtual	void SetData(std::byte*, const HEXSPANSTRUCT&) = 0; //Routine to modify data, if HEXDATASTRUCT::fMutable == true.
	};

	/********************************************************************************************
	* IHexVirtBkm - Pure abstract class for virtual bookmarks.                                  *
	********************************************************************************************/
	class IHexVirtBkm
	{
	public:
		virtual ULONGLONG Add(const HEXBOOKMARKSTRUCT& stBookmark) = 0; //Add new bookmark, return new bookmark's ID.
		virtual void ClearAll() = 0; //Clear all bookmarks.
		[[nodiscard]] virtual ULONGLONG GetCount() = 0; //Get total bookmarks count.
		[[nodiscard]] virtual auto GetByID(ULONGLONG ullID)->HEXBOOKMARKSTRUCT* = 0; //Bookmark by ID.
		[[nodiscard]] virtual auto GetByIndex(ULONGLONG ullIndex)->HEXBOOKMARKSTRUCT* = 0; //Bookmark by index (in inner list).
		[[nodiscard]] virtual auto HitTest(ULONGLONG ullOffset)->HEXBOOKMARKSTRUCT* = 0;   //Does given offset have a bookmark?
		virtual void RemoveByID(ULONGLONG ullID) = 0;   //Remove bookmark by given ID (returned by Add()).
	};

	/********************************************************************************************
	* HEXCOLOR - used with the IHexVirtColors interface.                                        *
	********************************************************************************************/
	struct HEXCOLOR
	{
		COLORREF clrBk { };   //Bk color.
		COLORREF clrText { }; //Text color.
	};
	using PHEXCOLOR = HEXCOLOR*;

	/********************************************************************************************
	* IHexVirtColors - Pure abstract class for chunk colors.                                    *
	********************************************************************************************/
	class IHexVirtColors
	{
	public:
		[[nodiscard]] virtual PHEXCOLOR GetColor(ULONGLONG ullOffset) = 0;
	};

	/********************************************************************************************
	* HEXCOLORSSTRUCT - All HexCtrl colors.                                                     *
	********************************************************************************************/
	struct HEXCOLORSSTRUCT
	{
		COLORREF clrTextHex { GetSysColor(COLOR_WINDOWTEXT) };         //Hex chunks text color.
		COLORREF clrTextAscii { GetSysColor(COLOR_WINDOWTEXT) };       //Ascii text color.
		COLORREF clrTextSelected { GetSysColor(COLOR_HIGHLIGHTTEXT) }; //Selected text color.
		COLORREF clrTextDataInterpret { RGB(250, 250, 250) };          //Data Interpreter text color.
		COLORREF clrTextCaption { RGB(0, 0, 180) };                    //Caption text color
		COLORREF clrTextInfoRect { GetSysColor(COLOR_WINDOWTEXT) };    //Text color of the bottom "Info" rect.
		COLORREF clrTextCursor { RGB(255, 255, 255) };                 //Cursor text color.
		COLORREF clrTextTooltip { GetSysColor(COLOR_INFOTEXT) };       //Tooltip text color.
		COLORREF clrBk { GetSysColor(COLOR_WINDOW) };                  //Background color.
		COLORREF clrBkSelected { GetSysColor(COLOR_HIGHLIGHT) };       //Background color of the selected Hex/Ascii.
		COLORREF clrBkDataInterpret { RGB(147, 58, 22) };              //Data Interpreter Bk color.
		COLORREF clrBkInfoRect { GetSysColor(COLOR_BTNFACE) };         //Background color of the bottom "Info" rect.
		COLORREF clrBkCursor { RGB(0, 0, 255) };                       //Cursor background color.
		COLORREF clrBkCursorSelected { RGB(0, 0, 200) };               //Cursor background color in selection.
		COLORREF clrBkTooltip { GetSysColor(COLOR_INFOBK) };           //Tooltip background color.
	};

	/********************************************************************************************
	* HEXCREATESTRUCT - for IHexCtrl::Create method.                                            *
	********************************************************************************************/
	struct HEXCREATESTRUCT
	{
		EHexCreateMode  enCreateMode { EHexCreateMode::CREATE_CHILD }; //Creation mode of the HexCtrl window.
		EHexShowMode    enShowMode { EHexShowMode::ASBYTE };           //Data representation mode.
		HEXCOLORSSTRUCT stColor { };          //All the control's colors.
		HWND            hwndParent { };       //Parent window handle.
		const LOGFONTW* pLogFont { };         //Font to be used, nullptr for default. This font has to be monospaced.
		RECT            rect { };             //Initial rect. If null, the window is screen centered.
		UINT            uID { };              //Control ID.
		DWORD           dwStyle { };          //Window styles, 0 for default.
		DWORD           dwExStyle { };        //Extended window styles, 0 for default.
		double          dbWheelRatio { 1.0 }; //Ratio for how much to scroll with mouse-wheel.
	};

	/********************************************************************************************
	* HEXDATASTRUCT - for IHexCtrl::SetData method.                                             *
	********************************************************************************************/
	struct HEXDATASTRUCT
	{
		EHexDataMode    enDataMode { EHexDataMode::DATA_MEMORY }; //Working data mode.
		ULONGLONG       ullDataSize { };          //Size of the data to display, in bytes.
		HEXSPANSTRUCT   stSelSpan { };            //Select .ullOffset initial position. Works only if .ullSize > 0.
		HWND            hwndMsg { };              //Window for DATA_MSG mode. Parent is used by default.
		IHexVirtData*   pHexVirtData { };         //Pointer for DATA_VIRTUAL mode.
		IHexVirtColors* pHexVirtColors { };       //Pointer for Custom Colors class.
		std::byte*      pData { };                //Data pointer for DATA_MEMORY mode. Not used in other modes.
		DWORD           dwCacheSize { 0x800000 }; //In DATA_MSG and DATA_VIRTUAL max cached size of data to fetch.
		bool            fMutable { false };       //Is data mutable (editable) or read-only.
		bool            fHighLatency { false };   //Do not redraw window until scrolling completes.
	};

	/********************************************************************************************
	* HEXNOTIFYSTRUCT - used in notifications routine.                                          *
	********************************************************************************************/
	struct HEXNOTIFYSTRUCT
	{
		NMHDR         hdr { };     //Standard Windows header. For hdr.code values see HEXCTRL_MSG_* messages.
		HEXSPANSTRUCT stSpan { };  //Offset and size of the bytes.
		ULONGLONG     ullData { }; //Data depending on message (e.g. user defined custom menu ID/caret pos).
		std::byte*    pData { };   //Pointer to a data to get/send.
		POINT         point { };   //Mouse position for menu notifications.
	};
	using PHEXNOTIFYSTRUCT = HEXNOTIFYSTRUCT*;

	/********************************************************************************************
	* HEXHITTESTSTRUCT - used in HitTest method.                                                *
	********************************************************************************************/
	struct HEXHITTESTSTRUCT
	{
		ULONGLONG ullOffset { };      //Offset.
		bool      fIsAscii { false }; //Is cursor at ASCII part or at Hex.
	};


	/********************************************************************************************
	* IHexCtrl - pure abstract base class.                                                      *
	********************************************************************************************/
	class IHexCtrl
	{
	public:
		virtual ULONGLONG BkmAdd(const HEXBOOKMARKSTRUCT& hbs, bool fRedraw = false) = 0; //Adds new bookmark.
		virtual void BkmClearAll() = 0;                         //Clear all bookmarks.
		[[nodiscard]] virtual auto BkmGetByID(ULONGLONG ullID)->HEXBOOKMARKSTRUCT* = 0; //Get bookmark by ID.
		[[nodiscard]] virtual auto BkmGetByIndex(ULONGLONG ullIndex)->HEXBOOKMARKSTRUCT* = 0; //Get bookmark by Index.
		[[nodiscard]] virtual ULONGLONG BkmGetCount()const = 0; //Get bookmarks count.
		[[nodiscard]] virtual auto BkmHitTest(ULONGLONG ullOffset)->HEXBOOKMARKSTRUCT* = 0; //HitTest for given offset.
		virtual void BkmRemoveByID(ULONGLONG ullID) = 0;        //Remove bookmark by the given ID.
		virtual void BkmSetVirtual(bool fEnable, IHexVirtBkm* pVirtual = nullptr) = 0; //Enable/disable bookmarks virtual mode.
		virtual void ClearData() = 0;                           //Clears all data from HexCtrl's view (not touching data itself).
		virtual bool Create(const HEXCREATESTRUCT& hcs) = 0;    //Main initialization method.
		virtual bool CreateDialogCtrl(UINT uCtrlID, HWND hwndDlg) = 0; //Сreates custom dialog control.
		virtual void Destroy() = 0;                             //Deleter.
		virtual void ExecuteCmd(EHexCmd enCmd)const = 0;        //Execute a command within the control.
		[[nodiscard]] virtual DWORD GetCapacity()const = 0;                  //Current capacity.
		[[nodiscard]] virtual ULONGLONG GetCaretPos()const = 0;              //Cursor position.
		[[nodiscard]] virtual auto GetColors()const->HEXCOLORSSTRUCT = 0;    //Current colors.
		[[nodiscard]] virtual long GetFontSize()const = 0;                   //Current font size.
		[[nodiscard]] virtual HMENU GetMenuHandle()const = 0;                //Context menu handle.
		[[nodiscard]] virtual DWORD GetSectorSize()const = 0;                //Current sector size.
		[[nodiscard]] virtual auto GetSelection()const->std::vector<HEXSPANSTRUCT> = 0; //Gets current selection.
		[[nodiscard]] virtual auto GetShowMode()const->EHexShowMode = 0;     //Retrieves current show mode.
		[[nodiscard]] virtual HWND GetWindowHandle()const = 0;               //Retrieves control's window handle.
		virtual void GoToOffset(ULONGLONG ullOffset, bool fSelect = false, ULONGLONG ullSize = 1) = 0; //Scrolls to given offset.
		[[nodiscard]] virtual auto HitTest(POINT pt, bool fScreen = true)const->std::optional<HEXHITTESTSTRUCT> = 0; //HitTest given point.
		[[nodiscard]] virtual bool IsCmdAvail(EHexCmd enCmd)const = 0; //Is given Cmd currently available (can be executed)?
		[[nodiscard]] virtual bool IsCreated()const = 0;       //Shows whether control is created or not.
		[[nodiscard]] virtual bool IsDataSet()const = 0;       //Shows whether a data was set to the control or not.
		[[nodiscard]] virtual bool IsDlgVisible(EHexDlg enDlg)const = 0; //Is specific dialog is currently visible.
		[[nodiscard]] virtual bool IsMutable()const = 0;       //Is edit mode enabled or not.
		[[nodiscard]] virtual bool IsOffsetAsHex()const = 0;   //Is "Offset" currently represented (shown) as Hex or as Decimal.
		[[nodiscard]] virtual bool IsOffsetVisible(ULONGLONG ullOffset)const = 0; //Ensures that given offset is visible.
		virtual void Redraw() = 0;                             //Redraw the control's window.
		virtual void SetCapacity(DWORD dwCapacity) = 0;        //Sets the control's current capacity.
		virtual void SetCodePage(int iCodePage) = 0;           //Code-page for text area.
		virtual void SetColors(const HEXCOLORSSTRUCT& clr) = 0;//Sets all the control's colors.
		virtual void SetData(const HEXDATASTRUCT& hds) = 0;    //Main method for setting data to display (and edit).	
		virtual void SetFont(const LOGFONTW* pLogFont) = 0;    //Sets the control's new font. This font has to be monospaced.
		virtual void SetFontSize(UINT uiSize) = 0;             //Sets the control's font size.
		virtual void SetMutable(bool fEnable) = 0;             //Enable or disable mutable/edit mode.
		virtual void SetSectorSize(DWORD dwSize, std::wstring_view wstrName = L"Sector") = 0; //Sets sector/page size and name to draw the lines in-between.
		virtual void SetSelection(const std::vector<HEXSPANSTRUCT>& vecSel) = 0; //Sets current selection.
		virtual void SetShowMode(EHexShowMode enMode) = 0;     //Sets current data show mode.
		virtual void SetWheelRatio(double dbRatio) = 0;        //Sets the ratio for how much to scroll with mouse-wheel.
		virtual void ShowDlg(EHexDlg enDlg, bool fShow = true)const = 0; //Show/hide specific dialog.
	};

	/********************************************************************************************
	* Factory function CreateHexCtrl returns IHexCtrlUnPtr - unique_ptr with custom deleter.    *
	* In client code you should use IHexCtrlPtr type which is an alias to either IHexCtrlUnPtr  *
	* - a unique_ptr, or IHexCtrlShPtr - a shared_ptr. Uncomment what serves best for you,      *
	* and comment out the other.                                                                *
	* If you, for some reason, need raw pointer, you can directly call CreateRawHexCtrl         *
	* function, which returns IHexCtrl interface pointer, but in this case you will need to     *
	* call IHexCtrl::Destroy method	afterwards - to manually delete HexCtrl object.             *
	********************************************************************************************/
#ifdef HEXCTRL_SHARED_DLL
#ifdef HEXCTRL_EXPORT
#define HEXCTRLAPI __declspec(dllexport)
#else
#define HEXCTRLAPI __declspec(dllimport)

#ifdef _WIN64
#ifdef _DEBUG
#define LIBNAME_PROPER(x) x"64d.lib"
#else
#define LIBNAME_PROPER(x) x"64.lib"
#endif
#else
#ifdef _DEBUG
#define LIBNAME_PROPER(x) x"d.lib"
#else
#define LIBNAME_PROPER(x) x".lib"
#endif
#endif

#pragma comment(lib, LIBNAME_PROPER("HexCtrl"))
#endif
#else
#define	HEXCTRLAPI
#endif

	extern "C" HEXCTRLAPI IHexCtrl * __cdecl CreateRawHexCtrl();
	using IHexCtrlUnPtr = std::unique_ptr<IHexCtrl, void(*)(IHexCtrl*)>;
	using IHexCtrlShPtr = std::shared_ptr<IHexCtrl>;

	inline IHexCtrlUnPtr CreateHexCtrl()
	{
		return IHexCtrlUnPtr(CreateRawHexCtrl(), [](IHexCtrl* p) { p->Destroy(); });
	};

#ifdef HEXCTRL_IHEXCTRLPTR_UNIQUEPTR
	using IHexCtrlPtr = IHexCtrlUnPtr;
#else
	using IHexCtrlPtr = IHexCtrlShPtr;
#endif

	/********************************************
	* HEXCTRLINFO: service info structure.      *
	********************************************/
	struct HEXCTRLINFO
	{
		const wchar_t* pwszVersion { };        //WCHAR version string.
		union {
			unsigned long long ullVersion { }; //ULONGLONG version number.
			struct {
				short wMajor;
				short wMinor;
				short wMaintenance;
				short wRevision;
			}stVersion;
		};
	};

	/*********************************************
	* Service info export/import function.       *
	* Returns pointer to PCHEXCTRL_INFO struct.  *
	*********************************************/
	extern "C" HEXCTRLAPI HEXCTRLINFO * __cdecl GetHexCtrlInfo();

	/********************************************************************************************
	* WM_NOTIFY message codes (NMHDR.code values).                                              *
	* These codes are used to notify m_hwndMsg window about control's states.                   *
	********************************************************************************************/

	constexpr auto HEXCTRL_MSG_BKMCLICK { 0x0100U };     //Bookmark clicked.
	constexpr auto HEXCTRL_MSG_CARETCHANGE { 0x0101U };  //Caret position changed.
	constexpr auto HEXCTRL_MSG_CONTEXTMENU { 0x0102U };  //OnContextMenu triggered.
	constexpr auto HEXCTRL_MSG_DESTROY { 0x0103U };      //Indicates that HexCtrl is being destroyed.
	constexpr auto HEXCTRL_MSG_GETDATA { 0x0104U };      //Used in DATA_MSG mode to request the data to display.
	constexpr auto HEXCTRL_MSG_MENUCLICK { 0x0105U };    //User defined custom menu clicked.
	constexpr auto HEXCTRL_MSG_SELECTION { 0x0106U };    //Selection has been made.
	constexpr auto HEXCTRL_MSG_SETDATA { 0x0107U };      //Indicates that the data has changed.
	constexpr auto HEXCTRL_MSG_VIEWCHANGE { 0x0108U };   //View of the control has changed.

	/*******************Setting a manifest for ComCtl32.dll version 6.***********************/
#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
}