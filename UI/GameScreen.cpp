// Copyright (c) 2013- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <algorithm>

#include "ppsspp_config.h"

#include "Common/Render/DrawBuffer.h"
#include "Common/UI/Context.h"
#include "Common/UI/View.h"
#include "Common/UI/ViewGroup.h"

#include "Common/Data/Text/I18n.h"
#include "Common/Data/Text/Parsers.h"
#include "Common/Data/Encoding/Utf8.h"
#include "Common/File/FileUtil.h"
#include "Common/StringUtils.h"
#include "Common/System/System.h"
#include "Common/System/OSD.h"
#include "Common/System/Request.h"
#include "Common/System/NativeApp.h"
#include "Core/Config.h"
#include "Core/Reporting.h"
#include "Core/System.h"
#include "Core/Loaders.h"
#include "Core/Util/GameDB.h"
#include "Core/HLE/Plugins.h"
#include "Core/Util/RecentFiles.h"
#include "UI/OnScreenDisplay.h"
#include "UI/CwCheatScreen.h"
#include "UI/EmuScreen.h"
#include "UI/GameScreen.h"
#include "UI/GameSettingsScreen.h"
#include "UI/GameInfoCache.h"
#include "UI/MiscScreens.h"
#include "UI/MainScreen.h"
#include "UI/BackgroundAudio.h"
#include "UI/SavedataScreen.h"
#include "Core/Reporting.h"

GameScreen::GameScreen(const Path &gamePath, bool inGame) : UIDialogScreenWithGameBackground(gamePath), inGame_(inGame) {
	g_BackgroundAudio.SetGame(gamePath);
	System_PostUIMessage(UIMessage::GAME_SELECTED, gamePath.ToString());
}

GameScreen::~GameScreen() {
	if (CRC32string == "...") {
		Reporting::CancelCRC();
	}
	System_PostUIMessage(UIMessage::GAME_SELECTED, "");
}

template <typename I> std::string int2hexstr(I w, size_t hex_len = sizeof(I) << 1) {
	static const char* digits = "0123456789ABCDEF";
	std::string rc(hex_len, '0');
	for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
		rc[i] = digits[(w >> j) & 0x0f];
	return rc;
}

void GameScreen::update() {
	UIScreen::update();

	// Has the user requested a CRC32?
	if (CRC32string == "...") {
		// Wait until the CRC32 is ready.  It might take time on some devices.
		if (Reporting::HasCRC(gamePath_)) {
			uint32_t crcvalue = Reporting::RetrieveCRC(gamePath_);
			CRC32string = int2hexstr(crcvalue);
			if (tvCRC_) {
				tvCRC_->SetVisibility(UI::V_VISIBLE);
				tvCRC_->SetText(CRC32string);
			}
			if (tvCRCCopy_) {
				tvCRCCopy_->SetVisibility(UI::V_VISIBLE);
			}
			if (btnCalcCRC_) {
				btnCalcCRC_->SetVisibility(UI::V_GONE);
			}
		}
	}
}

void GameScreen::CreateViews() {
	std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(NULL, gamePath_, GameInfoFlags::PARAM_SFO | GameInfoFlags::ICON | GameInfoFlags::BG);

	auto di = GetI18NCategory(I18NCat::DIALOG);
	auto ga = GetI18NCategory(I18NCat::GAME);

	// Information in the top left.
	// Back button to the bottom left.
	// Scrolling action menu to the right.
	using namespace UI;

	Margins actionMenuMargins(0, 15, 15, 0);

	root_ = new LinearLayout(ORIENT_HORIZONTAL);

	ViewGroup *leftColumn = new AnchorLayout(new LinearLayoutParams(1.0f));
	root_->Add(leftColumn);

	bool fileTypeSupportCRC = false;
	if (info) {
		switch (info->fileType) {
		case IdentifiedFileType::PSP_PBP:
		case IdentifiedFileType::PSP_PBP_DIRECTORY:
		case IdentifiedFileType::PSP_ISO_NP:
		case IdentifiedFileType::PSP_ISO:
			fileTypeSupportCRC = true;
			break;

		default:
			break;
		}
	}

	leftColumn->Add(new Choice(di->T("Back"), "", false, new AnchorLayoutParams(150, WRAP_CONTENT, 10, NONE, NONE, 10)))->OnClick.Handle(this, &GameScreen::OnSwitchBack);
	if (info->Ready(GameInfoFlags::PARAM_SFO)) {
		ViewGroup *badgeHolder = new LinearLayout(ORIENT_HORIZONTAL, new AnchorLayoutParams(10, 10, 110, NONE));
		LinearLayout *mainGameInfo = new LinearLayout(ORIENT_VERTICAL, new LinearLayoutParams(1.0f));
		mainGameInfo->SetSpacing(3.0f);

		// Need an explicit size here because homebrew uses screenshots as icons.
		badgeHolder->Add(new GameIconView(gamePath_, 2.0f, new LinearLayoutParams(144 * 2, 80 * 2, UI::Margins(0))));
		badgeHolder->Add(mainGameInfo);

		leftColumn->Add(badgeHolder);

		LinearLayout *infoLayout = new LinearLayout(ORIENT_VERTICAL, new AnchorLayoutParams(10, 200, NONE, NONE));
		leftColumn->Add(infoLayout);

		// TODO: Add non-translated title here if available in gameDB.
		tvTitle_ = mainGameInfo->Add(new TextView(info->GetTitle(), ALIGN_LEFT | FLAG_WRAP_TEXT, false, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		tvTitle_->SetShadow(true);
		tvID_ = mainGameInfo->Add(new TextView("", ALIGN_LEFT | FLAG_WRAP_TEXT, true, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		tvID_->SetShadow(true);
		tvRegion_ = mainGameInfo->Add(new TextView("", ALIGN_LEFT, true, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		tvRegion_->SetShadow(true);
		tvGameSize_ = mainGameInfo->Add(new TextView("...", ALIGN_LEFT, true, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		tvGameSize_->SetShadow(true);

		// This one doesn't need to be updated.
		infoLayout->Add(new TextView(gamePath_.ToVisualString(), ALIGN_LEFT | FLAG_WRAP_TEXT, true, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)))->SetShadow(true);
		tvSaveDataSize_ = infoLayout->Add(new TextView("...", ALIGN_LEFT, true, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		tvSaveDataSize_->SetShadow(true);
		tvInstallDataSize_ = infoLayout->Add(new TextView("", ALIGN_LEFT, true, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		tvInstallDataSize_->SetShadow(true);
		tvInstallDataSize_->SetVisibility(V_GONE);
		tvPlayTime_ = infoLayout->Add(new TextView("", ALIGN_LEFT, true, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		tvPlayTime_->SetShadow(true);
		tvPlayTime_->SetVisibility(V_GONE);
		
		LinearLayout *crcHoriz = infoLayout->Add(new LinearLayout(ORIENT_HORIZONTAL));

		if (fileTypeSupportCRC) {
			// CRC button makes sense.
			tvCRC_ = crcHoriz->Add(new TextView("", ALIGN_LEFT, true, new LinearLayoutParams(0.0, G_VCENTER)));
			tvCRC_->SetShadow(true);
			Visibility crcVisibility = Reporting::HasCRC(gamePath_) ? V_VISIBLE : V_GONE;
			tvCRC_->SetVisibility(crcVisibility);
			if (System_GetPropertyBool(SYSPROP_HAS_TEXT_CLIPBOARD)) {
				tvCRCCopy_ = crcHoriz->Add(new Button(di->T("Copy to clipboard"), new LinearLayoutParams(0.0, G_VCENTER)));
				tvCRCCopy_->OnClick.Add([this](UI::EventParams &) {
					u32 crc = Reporting::RetrieveCRC(gamePath_);
					char buffer[16];
					snprintf(buffer, sizeof(buffer), "%08X", crc);
					System_CopyStringToClipboard(buffer);
					// Success indication. Not worth a translatable string.
					g_OSD.Show(OSDType::MESSAGE_SUCCESS, buffer, 1.0f);
					return UI::EVENT_DONE;
				});
				tvCRCCopy_->SetVisibility(crcVisibility);
				tvCRCCopy_->SetScale(0.82f);
			} else {
				tvCRCCopy_ = nullptr;
			}
		}

		tvVerified_ = infoLayout->Add(new NoticeView(NoticeLevel::INFO, ga->T("Click \"Calculate CRC\" to verify ISO"), "", new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		tvVerified_->SetVisibility(UI::V_GONE);
		tvVerified_->SetSquishy(true);

		// Show plugin info, if any. Later might add checkboxes.
		auto plugins = HLEPlugins::FindPlugins(info->id, g_Config.sLanguageIni);
		if (!plugins.empty()) {
			auto sy = GetI18NCategory(I18NCat::SYSTEM);
			infoLayout->Add(new TextView(sy->T("Plugins"), ALIGN_LEFT, true));
			for (const auto &plugin : plugins) {
				infoLayout->Add(new TextView(ApplySafeSubstitutions("* %1", plugin.name), ALIGN_LEFT, true));
			}
		}
	} else {
		tvTitle_ = nullptr;
		tvID_ = nullptr;
		tvGameSize_ = nullptr;
		tvSaveDataSize_ = nullptr;
		tvInstallDataSize_ = nullptr;
		tvRegion_ = nullptr;
		tvPlayTime_ = nullptr;
		tvCRC_ = nullptr;
		tvCRCCopy_ = nullptr;
		tvVerified_ = nullptr;
	}

	ViewGroup *rightColumn = new ScrollView(ORIENT_VERTICAL, new LinearLayoutParams(300, FILL_PARENT, actionMenuMargins));
	root_->Add(rightColumn);

	LinearLayout *rightColumnItems = new LinearLayout(ORIENT_VERTICAL);
	rightColumnItems->SetSpacing(0.0f);
	rightColumn->Add(rightColumnItems);

	if (!inGame_) {
		rightColumnItems->Add(new Choice(ga->T("Play")))->OnClick.Handle(this, &GameScreen::OnPlay);
	}

	btnGameSettings_ = rightColumnItems->Add(new Choice(ga->T("Game Settings")));
	btnGameSettings_->OnClick.Handle(this, &GameScreen::OnGameSettings);
	if (inGame_)
		btnGameSettings_->SetEnabled(false);

	btnDeleteGameConfig_ = rightColumnItems->Add(new Choice(ga->T("Delete Game Config")));
	btnDeleteGameConfig_->OnClick.Handle(this, &GameScreen::OnDeleteConfig);
	if (inGame_)
		btnDeleteGameConfig_->SetEnabled(false);

	btnCreateGameConfig_ = rightColumnItems->Add(new Choice(ga->T("Create Game Config")));
	btnCreateGameConfig_->OnClick.Handle(this, &GameScreen::OnCreateConfig);
	if (inGame_)
		btnCreateGameConfig_->SetEnabled(false);

	btnGameSettings_->SetVisibility(V_GONE);
	btnDeleteGameConfig_->SetVisibility(V_GONE);
	btnCreateGameConfig_->SetVisibility(V_GONE);

	btnDeleteSaveData_ = new Choice(ga->T("Delete Save Data"));
	rightColumnItems->Add(btnDeleteSaveData_)->OnClick.Handle(this, &GameScreen::OnDeleteSaveData);
	btnDeleteSaveData_->SetVisibility(V_GONE);

	otherChoices_.clear();

	// Don't want to be able to delete the game while it's running.
	Choice *deleteChoice = rightColumnItems->Add(AddOtherChoice(new Choice(ga->T("Delete Game"))));
	deleteChoice->OnClick.Handle(this, &GameScreen::OnDeleteGame);
	if (inGame_) {
		deleteChoice->SetEnabled(false);
	}
	if (System_GetPropertyBool(SYSPROP_CAN_CREATE_SHORTCUT)) {
		rightColumnItems->Add(AddOtherChoice(new Choice(ga->T("Create Shortcut"))))->OnClick.Add([=](UI::EventParams &e) {
			std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(NULL, gamePath_, GameInfoFlags::PARAM_SFO);
			if (info->Ready(GameInfoFlags::PARAM_SFO)) {
				// TODO: Should we block on Ready?
				System_CreateGameShortcut(gamePath_, info->GetTitle());
			}
			return UI::EVENT_DONE;
		});
	}

	// TODO: This is synchronous, bad!
	if (g_recentFiles.ContainsFile(gamePath_.ToString())) {
		Choice *removeButton = rightColumnItems->Add(AddOtherChoice(new Choice(ga->T("Remove From Recent"))));
		removeButton->OnClick.Handle(this, &GameScreen::OnRemoveFromRecent);
		if (inGame_) {
			removeButton->SetEnabled(false);
		}
	}

	if (System_GetPropertyBool(SYSPROP_CAN_SHOW_FILE)) {
		rightColumnItems->Add(AddOtherChoice(new Choice(di->T("Show in folder"))))->OnClick.Add([this](UI::EventParams &e) {
			System_ShowFileInFolder(gamePath_);
			return UI::EVENT_DONE;
		});
	}

	if (g_Config.bEnableCheats) {
		auto pa = GetI18NCategory(I18NCat::PAUSE);
		rightColumnItems->Add(AddOtherChoice(new Choice(pa->T("Cheats"))))->OnClick.Handle(this, &GameScreen::OnCwCheat);
	}

	btnSetBackground_ = rightColumnItems->Add(new Choice(ga->T("Use UI background")));
	btnSetBackground_->OnClick.Handle(this, &GameScreen::OnSetBackground);
	btnSetBackground_->SetVisibility(V_GONE);

	isHomebrew_ = info && info->region == GameRegion::HOMEBREW;
	if (fileTypeSupportCRC && !isHomebrew_ && !Reporting::HasCRC(gamePath_) ) {
		btnCalcCRC_ = rightColumnItems->Add(new ChoiceWithValueDisplay(&CRC32string, ga->T("Calculate CRC"), I18NCat::NONE));
		btnCalcCRC_->OnClick.Handle(this, &GameScreen::OnDoCRC32);
	} else {
		btnCalcCRC_ = nullptr;
	}
}

UI::Choice *GameScreen::AddOtherChoice(UI::Choice *choice) {
	otherChoices_.push_back(choice);
	// While loading.
	choice->SetVisibility(UI::V_GONE);
	return choice;
}

UI::EventReturn GameScreen::OnCreateConfig(UI::EventParams &e) {
	std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(nullptr, gamePath_, GameInfoFlags::PARAM_SFO);
	if (!info->Ready(GameInfoFlags::PARAM_SFO)) {
		return UI::EVENT_SKIPPED;
	}
	g_Config.createGameConfig(info->id);
	g_Config.saveGameConfig(info->id, info->GetTitle());
	info->hasConfig = true;

	screenManager()->topScreen()->RecreateViews();
	return UI::EVENT_DONE;
}

UI::EventReturn GameScreen::OnDeleteConfig(UI::EventParams &e) {
	auto di = GetI18NCategory(I18NCat::DIALOG);
	const bool trashAvailable = System_GetPropertyBool(SYSPROP_HAS_TRASH_BIN);
	screenManager()->push(
		new PromptScreen(gamePath_, di->T("DeleteConfirmGameConfig", "Do you really want to delete the settings for this game?"), trashAvailable ? di->T("Move to trash") : di->T("Delete"), di->T("Cancel"),
			[this](bool result) {
		if (result) {
			std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(nullptr, gamePath_, GameInfoFlags::PARAM_SFO);
			if (!info->Ready(GameInfoFlags::PARAM_SFO)) {
				return;
			}
			g_Config.deleteGameConfig(info->id);
			info->hasConfig = false;
			screenManager()->RecreateAllViews();
		}
	}));
	return UI::EVENT_DONE;
}

ScreenRenderFlags GameScreen::render(ScreenRenderMode mode) {
	ScreenRenderFlags flags = UIScreen::render(mode);

	auto ga = GetI18NCategory(I18NCat::GAME);

	Draw::DrawContext *draw = screenManager()->getDrawContext();

	std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(draw, gamePath_, GameInfoFlags::BG | GameInfoFlags::SIZE | GameInfoFlags::UNCOMPRESSED_SIZE);

	if (tvTitle_) {
		tvTitle_->SetText(info->GetTitle());
	}

	if (info->Ready(GameInfoFlags::SIZE | GameInfoFlags::UNCOMPRESSED_SIZE)) {
		char temp[256];
		if (tvGameSize_) {
			snprintf(temp, sizeof(temp), "%s: %s", ga->T_cstr("Game"), NiceSizeFormat(info->gameSizeOnDisk).c_str());
			if (info->gameSizeUncompressed != info->gameSizeOnDisk) {
				size_t len = strlen(temp);
				snprintf(temp + len, sizeof(temp) - len, " (%s: %s)", ga->T_cstr("Uncompressed"), NiceSizeFormat(info->gameSizeUncompressed).c_str());
			}
			tvGameSize_->SetText(temp);
		}
		if (tvSaveDataSize_) {
			if (info->saveDataSize > 0) {
				snprintf(temp, sizeof(temp), "%s: %s", ga->T_cstr("SaveData"), NiceSizeFormat(info->saveDataSize).c_str());
				tvSaveDataSize_->SetText(temp);
			} else {
				tvSaveDataSize_->SetVisibility(UI::V_GONE);
			}
		}
		if (info->installDataSize > 0 && tvInstallDataSize_) {
			snprintf(temp, sizeof(temp), "%s: %1.2f %s", ga->T_cstr("InstallData"), (float) (info->installDataSize) / 1024.f / 1024.f, ga->T_cstr("MB"));
			tvInstallDataSize_->SetText(temp);
			tvInstallDataSize_->SetVisibility(UI::V_VISIBLE);
		}
	}

	if (tvRegion_) {
		if (info->region == GameRegion::OTHER) {
			tvRegion_->SetText(ga->T("Homebrew"));
		} else {
			tvRegion_->SetText(ga->T(GameRegionToString(info->region)));
		}
	}

	if (tvPlayTime_) {
		std::string str;
		if (g_Config.TimeTracker().GetPlayedTimeString(info->id, &str)) {
			tvPlayTime_->SetText(str);
			tvPlayTime_->SetVisibility(UI::V_VISIBLE);
		}
	}

	if (tvCRC_ && Reporting::HasCRC(gamePath_)) {
		auto rp = GetI18NCategory(I18NCat::REPORTING);
		uint32_t crcVal = Reporting::RetrieveCRC(gamePath_);
		std::string crc = StringFromFormat("%08X", crcVal);
		tvCRC_->SetText(ReplaceAll(rp->T("FeedbackCRCValue", "Disc CRC: %1"), "%1", crc));
		tvCRC_->SetVisibility(UI::V_VISIBLE);
		if (tvCRCCopy_) {
			tvCRCCopy_->SetVisibility(UI::V_VISIBLE);
		}

		// Let's check the CRC in the game database, looking up the ID and also matching the crc.
		std::vector<GameDBInfo> dbInfos;
		if (tvVerified_ && info->Ready(GameInfoFlags::PARAM_SFO) && g_gameDB.GetGameInfos(info->id_version, &dbInfos)) {
			bool found = false;
			for (auto &dbInfo : dbInfos) {
				if (dbInfo.crc == crcVal) {
					found = true;
				}
			}
			if (found) {
				tvVerified_->SetText(ga->T("ISO OK according to the ReDump project"));
				tvVerified_->SetLevel(NoticeLevel::SUCCESS);
				tvVerified_->SetVisibility(UI::V_VISIBLE);
			} else {
				// Like the other messages below, disabled until we have a database we have confidence in.
				// tvVerified_->SetText(ga->T("CRC checksum does not match, bad or modified ISO"));
				// tvVerified_->SetLevel(NoticeLevel::ERROR);
				tvVerified_->SetVisibility(UI::V_GONE);
			}
		} else if (tvVerified_) {
			// tvVerified_->SetText(ga->T("Game ID unknown - not in the ReDump database"));
			// tvVerified_->SetVisibility(UI::V_VISIBLE);
			// tvVerified_->SetLevel(NoticeLevel::WARN);
			tvVerified_->SetVisibility(UI::V_GONE);
		}
	} else if (!isHomebrew_) {
		GameDBInfo dbInfo;
		if (tvVerified_) {
			std::vector<GameDBInfo> dbInfos;
			if (info->Ready(GameInfoFlags::PARAM_SFO) && !g_gameDB.GetGameInfos(info->id_version, &dbInfos)) {
				// tvVerified_->SetText(ga->T("Game ID unknown - not in the ReDump database"));
				// tvVerified_->SetVisibility(UI::V_VISIBLE);
				// tvVerified_->SetLevel(NoticeLevel::WARN);
			} else if (info->Ready(GameInfoFlags::UNCOMPRESSED_SIZE) && info->gameSizeUncompressed != 0) {  // don't do this check if info still pending
				bool found = false;
				for (auto &dbInfo : dbInfos) {
					// TODO: Doesn't take CSO/CHD into account.
					if (info->gameSizeUncompressed == dbInfo.size) {
						found = true;
					}
				}
				if (!found) {
					// tvVerified_->SetText(ga->T("File size incorrect, bad or modified ISO"));
					// tvVerified_->SetVisibility(UI::V_VISIBLE);
					// tvVerified_->SetLevel(NoticeLevel::ERROR);
					// INFO_LOG(Log::Loader, "File size %d not matching game DB", (int)info->gameSizeUncompressed);
				} else {
					tvVerified_->SetText(ga->T("Click \"Calculate CRC\" to verify ISO"));
					tvVerified_->SetVisibility(UI::V_VISIBLE);
					tvVerified_->SetLevel(NoticeLevel::INFO);
				}
			}
		}
	}

	if (tvID_) {
		tvID_->SetText(ReplaceAll(info->id_version, "_", " v"));
	}

	if (!info->id.empty()) {
		btnGameSettings_->SetVisibility(info->hasConfig ? UI::V_VISIBLE : UI::V_GONE);
		btnDeleteGameConfig_->SetVisibility(info->hasConfig ? UI::V_VISIBLE : UI::V_GONE);
		btnCreateGameConfig_->SetVisibility(info->hasConfig ? UI::V_GONE : UI::V_VISIBLE);

		if (info->saveDataSize) {
			btnDeleteSaveData_->SetVisibility(UI::V_VISIBLE);
		}
		if (info->pic0.texture || info->pic1.texture) {
			btnSetBackground_->SetVisibility(UI::V_VISIBLE);
		}
	}

	if (info->Ready(GameInfoFlags::PARAM_SFO)) {
		// At this point, the above buttons won't become visible.  We can show these now.
		for (UI::Choice *choice : otherChoices_) {
			choice->SetVisibility(UI::V_VISIBLE);
		}
	}
	return flags;
}

UI::EventReturn GameScreen::OnCwCheat(UI::EventParams &e) {
	screenManager()->push(new CwCheatScreen(gamePath_));
	return UI::EVENT_DONE;
}

UI::EventReturn GameScreen::OnDoCRC32(UI::EventParams& e) {
	CRC32string = "...";
	Reporting::QueueCRC(gamePath_);
	if (btnCalcCRC_) {
		btnCalcCRC_->SetEnabled(false);
	}
	return UI::EVENT_DONE;
}


UI::EventReturn GameScreen::OnSwitchBack(UI::EventParams &e) {
	TriggerFinish(DR_OK);
	return UI::EVENT_DONE;
}

UI::EventReturn GameScreen::OnPlay(UI::EventParams &e) {
	screenManager()->switchScreen(new EmuScreen(gamePath_));
	return UI::EVENT_DONE;
}

UI::EventReturn GameScreen::OnGameSettings(UI::EventParams &e) {
	std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(NULL, gamePath_, GameInfoFlags::PARAM_SFO);
	if (info && info->Ready(GameInfoFlags::PARAM_SFO)) {
		std::string discID = info->GetParamSFO().GetValueString("DISC_ID");
		if ((discID.empty() || !info->disc_total) && gamePath_.FilePathContainsNoCase("PSP/GAME/"))
			discID = g_paramSFO.GenerateFakeID(gamePath_);
		screenManager()->push(new GameSettingsScreen(gamePath_, discID, true));
	}
	return UI::EVENT_DONE;
}

UI::EventReturn GameScreen::OnDeleteSaveData(UI::EventParams &e) {
	std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(NULL, gamePath_, GameInfoFlags::PARAM_SFO | GameInfoFlags::SIZE);
	if (info) {
		// Check that there's any savedata to delete
		if (info->saveDataSize) {
			const bool trashAvailable = System_GetPropertyBool(SYSPROP_HAS_TRASH_BIN);
			auto di = GetI18NCategory(I18NCat::DIALOG);
			Path gamePath = gamePath_;
			screenManager()->push(
				new PromptScreen(gamePath_, di->T("DeleteConfirmAll", "Do you really want to delete all\nyour save data for this game?"), trashAvailable ? di->T("Move to trash") : di->T("Delete"), di->T("Cancel"),
					[gamePath](bool yes) {
				if (yes) {
					std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(NULL, gamePath, GameInfoFlags::PARAM_SFO);
					info->DeleteAllSaveData();
					info->saveDataSize = 0;
					info->installDataSize = 0;
				}
			}));
		}
	}
	RecreateViews();
	return UI::EVENT_DONE;
}

UI::EventReturn GameScreen::OnDeleteGame(UI::EventParams &e) {
	std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(NULL, gamePath_, GameInfoFlags::PARAM_SFO);
	if (info->Ready(GameInfoFlags::PARAM_SFO)) {
		auto di = GetI18NCategory(I18NCat::DIALOG);
		std::string prompt;
		prompt = di->T("DeleteConfirmGame", "Do you really want to delete this game\nfrom your device? You can't undo this.");
		prompt += "\n\n" + gamePath_.ToVisualString(g_Config.memStickDirectory.c_str());
		const bool trashAvailable = System_GetPropertyBool(SYSPROP_HAS_TRASH_BIN);
		Path gamePath = gamePath_;
		ScreenManager *sm = screenManager();
		screenManager()->push(
			new PromptScreen(gamePath_, prompt, trashAvailable ? di->T("Move to trash") : di->T("Delete"), di->T("Cancel"),
				[sm, gamePath](bool yes) {
			if (yes) {
				std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(NULL, gamePath, GameInfoFlags::PARAM_SFO);
				info->Delete();
				g_gameInfoCache->Clear();
				g_recentFiles.Remove(gamePath.c_str());
				sm->switchScreen(new MainScreen());
			}
		}));
	}
	return UI::EVENT_DONE;
}

UI::EventReturn GameScreen::OnRemoveFromRecent(UI::EventParams &e) {
	g_recentFiles.Remove(gamePath_.ToString());
	screenManager()->switchScreen(new MainScreen());
	return UI::EVENT_DONE;
}

class SetBackgroundPopupScreen : public PopupScreen {
public:
	SetBackgroundPopupScreen(std::string_view title, const Path &gamePath)
		: PopupScreen(title), gamePath_(gamePath) {
		timeStart_ = time_now_d();
	}
	const char *tag() const override { return "SetBackgroundPopup"; }

protected:
	bool FillVertical() const override { return false; }
	bool ShowButtons() const override { return false; }
	void CreatePopupContents(UI::ViewGroup *parent) override;
	void update() override;

private:
	Path gamePath_;
	double timeStart_;
	double timeDone_ = 0.0;

	enum class Status {
		PENDING,
		DELAY,
		DONE,
	};
	Status status_ = Status::PENDING;
};

void SetBackgroundPopupScreen::CreatePopupContents(UI::ViewGroup *parent) {
	auto ga = GetI18NCategory(I18NCat::GAME);
	parent->Add(new UI::TextView(ga->T("One moment please..."), ALIGN_LEFT | ALIGN_VCENTER, false, new UI::LinearLayoutParams(UI::Margins(10, 0, 10, 10))));
}

void SetBackgroundPopupScreen::update() {
	PopupScreen::update();

	std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(nullptr, gamePath_, GameInfoFlags::BG);
	if (status_ == Status::PENDING && info->Ready(GameInfoFlags::BG)) {
		GameInfoTex *pic = nullptr;
		if (info->pic1.dataLoaded && info->pic1.data.size()) {
			pic = &info->pic1;
		} else if (info->pic0.dataLoaded && info->pic0.data.size()) {
			pic = &info->pic0;
		}

		if (pic) {
			const Path bgPng = GetSysDirectory(DIRECTORY_SYSTEM) / "background.png";
			File::WriteStringToFile(false, pic->data, bgPng);
		}

		UIBackgroundShutdown();

		// It's worse if it flickers, stay open for at least 1s.
		timeDone_ = timeStart_ + 1.0;
		status_ = Status::DELAY;
	}

	if (status_ == Status::DELAY && timeDone_ <= time_now_d()) {
		TriggerFinish(DR_OK);
		status_ = Status::DONE;
	}
}

UI::EventReturn GameScreen::OnSetBackground(UI::EventParams &e) {
	auto ga = GetI18NCategory(I18NCat::GAME);
	// This popup is used to prevent any race condition:
	// g_gameInfoCache may take time to load the data, and a crash could happen if they exit before then.
	SetBackgroundPopupScreen *pop = new SetBackgroundPopupScreen(ga->T("Setting Background"), gamePath_);
	if (e.v)
		pop->SetPopupOrigin(e.v);
	screenManager()->push(pop);
	return UI::EVENT_DONE;
}
