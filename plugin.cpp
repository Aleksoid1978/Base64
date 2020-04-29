/**************************************************************************
 *  Base64 encode/decode plug-in for FAR 3.0                              *
 *  Copyright (C) 2020 by Vodiannikov Aleksandr <Aleksoid1978@mail.ru>    *
 *                                                                        *
 *  This program is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  This program is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

#include "Common/plugin.hpp"
#include "Common/farcolor.hpp"
#include "version.h"
#include <string>
#include "Base64.h"

// Plugin GUID {36C486C4-AC2D-4A96-A2D8-C293C0E2F4CE}
const GUID _FPG = { 0x36C486C4, 0xAC2D, 0x4A96, { 0xA2, 0xD8, 0xC2, 0x93, 0xC0, 0xE2, 0xF4, 0xCE } };

PluginStartupInfo    _PSI;
FarStandardFunctions _FSF;

void WINAPI SetStartupInfoW(const PluginStartupInfo* psi)
{
	_PSI = *psi;
	_FSF = *psi->FSF;
	_PSI.FSF = &_FSF;
}

void WINAPI GetGlobalInfoW(GlobalInfo* info)
{
	info->StructSize = sizeof(GlobalInfo);
	info->MinFarVersion = MAKEFARVERSION(FARMANAGERVERSION_MAJOR,FARMANAGERVERSION_MINOR,FARMANAGERVERSION_REVISION,PLUGIN_FAR_BUILD,VS_RELEASE);
	info->Version = MAKEFARVERSION(PLUGIN_VERSION_NUM, VS_RELEASE);
	info->Guid = _FPG;
	info->Title = TEXT(PLUGIN_NAME);
	info->Description = TEXT(PLUGIN_DESCR);
	info->Author = TEXT(PLUGIN_AUTHOR);
}

void WINAPI GetPluginInfoW(PluginInfo* info)
{
	static wchar_t* szMenu[] = { L"base64 decoder" };

	info->Flags = PF_DISABLEPANELS | PF_EDITOR;

	info->PluginMenu.Guids = &_FPG;
	info->PluginMenu.Strings = szMenu;
	info->PluginMenu.Count = 1;
}

static inline INT_PTR EditCtrl(EDITOR_CONTROL_COMMANDS cmd, void* param) noexcept
{
	return _PSI.EditorControl(-1, cmd, 0, param);
}

HANDLE WINAPI OpenW(const OpenInfo* info)
{
	if (!info || info->StructSize < sizeof(OpenInfo) || info->OpenFrom != OPEN_EDITOR) {
		return nullptr;
	}

	EditorInfo ei = { sizeof(ei) };
	EditCtrl(ECTL_GETINFO, &ei);
	if (ei.BlockType != BTYPE_STREAM) {
		_PSI.Message(&_FPG, &_FPG, FMSG_ALLINONE | FMSG_MB_OK,
					 nullptr, (const wchar_t* const*)L"Base64\nThere is no selection to decode!", 0, 0);
		return nullptr;
	}

	EditorGetString egs = { sizeof(egs) };

	std::wstring selectedString;
	for (egs.StringNumber = ei.BlockStartLine; egs.StringNumber < ei.TotalLines; egs.StringNumber++) {
		EditCtrl(ECTL_GETSTRING, &egs);

		if (egs.SelStart == -1) {
			break;
		}

		if (!selectedString.empty()) {
			selectedString.push_back(L'\n');
		}

		const auto nStart = egs.SelStart;
		const auto nEnd = (egs.SelEnd == -1) ? egs.StringLength : egs.SelEnd;
		const auto nLen = (nEnd - nStart);

		if (nLen > 0) {
			selectedString.append(egs.StringText + nStart, nLen);
		}
	}

	if (selectedString.empty()) {
		return nullptr;
	}

	std::string multiByteString;
	auto convertLen = WideCharToMultiByte((UINT)ei.CodePage, 0, selectedString.c_str(), -1, nullptr, 0, nullptr, nullptr) - 1;
	if (convertLen > 0) {
		multiByteString.resize(convertLen);
		WideCharToMultiByte((UINT)ei.CodePage, 0, selectedString.c_str(), -1, &multiByteString[0], convertLen + 1, 0, 0);
	}

	const FarMenuItem items[] = {
		{0,  L"Base64 decode"},
		{MIF_SEPARATOR},
		{0,  L"Base64 encode"}
	};
	const auto nDecoderType = _PSI.Menu(&_FPG, &_FPG, -1, -1, 0,
										FMENU_AUTOHIGHLIGHT | FMENU_CHANGECONSOLETITLE | FMENU_WRAPMODE,
										L"Choose decoder",
										nullptr, nullptr, nullptr, nullptr, (FarMenuItem*)items, std::size(items));
	std::string outputText;
	if (nDecoderType == 0) {
		macaron::Base64::Decode(multiByteString, outputText);
		if (outputText.empty()) {
			_PSI.Message(&_FPG, &_FPG, FMSG_WARNING | FMSG_ALLINONE | FMSG_MB_OK,
						 nullptr, (const wchar_t* const*)L"Base64 decoder\nSize of selected data is not a multiple of 4!", 0, 0);
		}
	} else if (nDecoderType == 2) {
		outputText = macaron::Base64::Encode(multiByteString);
	}

	if (!outputText.empty() && EditCtrl(ECTL_DELETEBLOCK, nullptr)) {
		convertLen = MultiByteToWideChar((UINT)ei.CodePage, 0, outputText.c_str(), -1, nullptr, 0) - 1;
		if (convertLen > 0) {
			std::wstring widecharString(convertLen, L'\0');
			MultiByteToWideChar((UINT)ei.CodePage, 0, outputText.c_str(), -1, &widecharString[0], convertLen + 1);
			EditCtrl(ECTL_INSERTTEXT, &widecharString[0]);
		}
	}

	return nullptr;
}
