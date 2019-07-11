/*
    Copyright (C) 2008-2010  Lukas Sommer < SommerLuk at gmail dot com >

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License or (at your option) version 3 or any later version
    accepted by the membership of KDE e.V. (or its successor approved
    by the membership of KDE e.V.), which shall act as a proxy
    defined in Section 14 of version 3 of the license.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "MainWindow.h"
#include "RadioApp.h"
#include <View.h>
#include <Application.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Clipboard.h>
#include <Autolock.h>
#include <ScrollView.h>
#include <LayoutBuilder.h>
#include <GroupLayout.h>
#include <Dragger.h>

MainWindow::MainWindow()
  : BWindow(BRect(0,0,400,200), "StreamRadio", B_DOCUMENT_WINDOW, B_WILL_ACCEPT_FIRST_CLICK),
    fStationFinder(NULL)
{
	fSettings = &((RadioApp*)be_app)->Settings;
    //initialize central widget
    BLayoutBuilder::Menu<>(fMainMenu = new BMenuBar(Bounds(), "MainMenu"))
        .AddMenu("File")
            .AddItem("Quit", B_QUIT_REQUESTED, 'Q')
            .AddSeparator()
            .AddItem("About", B_ABOUT_REQUESTED)
        .End()
        .AddMenu("Edit")
            .AddItem("Paste Shoutcast URL", MSG_PASTE_URL)
            .AddItem("Check Station", MSG_CHECK)
            .AddItem("Remove", MSG_REMOVE, B_DELETE)
        .End()
        .AddItem("Search", MSG_SEARCH, 's')
    .End();
    AddChild(fMainMenu);
    fStationList = new StationListView(true);
	BScrollView* stationScroll = new BScrollView("scrollStation", 
			fStationList, B_FOLLOW_ALL_SIDES, 0, false, true);
	
 	fStationPanel = new StationPanel(this);
	fExpander = new Expander("expStationPanel", fStationPanel);			

    BGroupLayout* layout = (BGroupLayout *)BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_USE_FULL_HEIGHT))
        .Add(stationScroll, 1.0f)
		.Add(fExpander, 0.0f)
		.Add(fStationPanel, 0.0f)
        .Add(fStatusBar = new BStringView("status", ""), 0.0f)            
    .End();
	
    fStationList->Sync(fSettings->Stations);
    fStationList->SetInvocationMessage(new BMessage(MSG_INVOKE_STATION));
	fStationList->SetSelectionMessage(new BMessage(MSG_SELECT_STATION));
    fStationList->SetPlayMessage(new BMessage(MSG_INVOKE_STATION));
	fStatusBar->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_VERTICAL_UNSET));
	fStatusBar->SetExplicitMinSize(BSize(10, B_SIZE_UNSET));
    Layout(true);
	
    ResizeToPreferred();
    CenterOnScreen();

	fExpander->SetExpanded(false);
}

MainWindow::~MainWindow() {
	for (int i = 0; i < fStationList->CountItems(); i++) {
		StationListViewItem* stationItem = fStationList->ItemAt(i);
		StreamPlayer* player = stationItem->Player();
		if (player) {
			player->Stop();
		}
	}
}

void MainWindow::MessageReceived(BMessage* message) {
    status_t status;
    switch(message->what) {
        case B_REFS_RECEIVED : {
            entry_ref ref;
			int32 index = 0;
			Station* station;
			StationListViewItem* stationItem;
			BString result;
			while (message->FindRef("refs", index++, &ref) == B_OK) {
                Station* station = Station::Load(ref.name, new BEntry(&ref));
				if ((station = Station::Load(ref.name, new BEntry(&ref)))) { 
					Station* existingStation = fSettings->Stations->FindItem(station->Name());
					if (existingStation) {
						delete station;
						station = existingStation;
						stationItem = fStationList->Item(station);
						TogglePlay(stationItem);
					} else {
						stationItem = new StationListViewItem(station);
						fStationList->AddItem(stationItem);
						result.SetToFormat("Added station %s to list", station->Name()->String());
					}
				} else {
					result.SetToFormat("File %s could not be loaded as a station");
				}
				fStatusBar->SetText(result.String());
            }
            break;
        }
        
        case MSG_SEARCH : {
            if (fStationFinder == NULL) {
                fStationFinder = new StationFinderWindow(this);
                fStationFinder->Show();
            }
            break;
        }
            
        case MSG_PASTE_URL: {
            be_clipboard->Lock();
            BMessage* data = be_clipboard->Data();
            be_clipboard->Unlock();
            char* url;
            ssize_t numBytes;
            if (data->FindData("text/plain", B_MIME_TYPE, 
					(const void**)&url, &numBytes) == B_OK) {
                url[numBytes]=0;
                BString sUrl(url);
                Station* station = Station::LoadIndirectUrl(sUrl);
                if (station) 
                    if (fSettings->Stations->AddItem(station) == B_OK) {
                        fStationList->Sync(fSettings->Stations);
                        fSettings->Stations->Save();
                    }
            }
            break;
        }
            
        case MSG_CHECK : {
			StationListViewItem* stationItem = fStationList->ItemAt(fStationList->CurrentSelection(0));
            Station* station = fStationList->StationAt(fStationList->CurrentSelection(0));
            if (stationItem) {
				Station* station = stationItem->GetStation();
                BString statusText("Probing station %s %s");
                status_t stationStatus = station->Probe();
                if (stationStatus == B_OK) {
                    statusText.SetToFormat(statusText, station->Name()->String(), "successful");
                } else {
                    statusText.SetToFormat(statusText, station->Name()->String(), "failed");
                }
                fStatusBar->SetText(statusText);
                fStationList->Invalidate();
				fStationPanel->SetStation(stationItem);
            }
            break;
        }
        
        case MSG_REMOVE : {
            Station* station = fStationList->StationAt(fStationList->CurrentSelection(0));
            if (station) {
                fSettings->Stations->RemoveItem(station);
                fStationList->Sync(fSettings->Stations);
                fSettings->Stations->Save();
            }
            break;
        }

        case MSG_ADD_STATION : {
            Station* station = NULL;
            if (B_OK == message->FindPointer("station", (void**)&station)) {
                fSettings->Stations->AddItem(station);
                fStationList->Sync(fSettings->Stations);
                fSettings->Stations->Save();
            }
            break;               
        }
            
        case MSG_SEARCH_CLOSE :
            fStationFinder = NULL;
            break;
        
		case MSG_SELECT_STATION : {
			int index = fStationList->CurrentSelection();
			if (index >= 0) {
				fStationPanel->LockLooper();
				fStationPanel->SetStation(fStationList->ItemAt(index));
				fStationPanel->UnlockLooper();
			}
			break;
		}
	
		case MSG_INVOKE_STATION : {
            int32 stationIndex = message->GetInt32("index", -1);
            StationListViewItem* stationItem = fStationList->ItemAt(stationIndex);
            if (stationItem)
				TogglePlay(stationItem);
            break;
        }
        
        case MSG_PLAYER_STATE_CHANGED : {
            StreamPlayer* player = NULL;
            status_t status = message->FindPointer("player", (void**)&player);
			
            if (player && status == B_OK) {
				StreamPlayer::PlayState state;
				status = message->FindInt32("state", (int32*)&state);

				int stationIndex = fStationList->StationIndex(player->GetStation());
				if (stationIndex >= 0) {
					StationListViewItem* stationItem = fStationList->ItemAt(stationIndex);
					stationItem->StateChanged(player->State());

					if (player->State() == StreamPlayer::Stopped) {
						delete player;
						stationItem->SetPlayer(NULL);
					}

					if (stationIndex == fStationList->CurrentSelection()) {
						fStationPanel->LockLooper();
						fStationPanel->StateChanged(state);
						fStationPanel->UnlockLooper();
					} 
				}
            }
            break;
        }
        
        case MSG_PLAYER_BUFFER_LEVEL : {
            StreamPlayer* player = NULL;
            status_t status = message->FindPointer("player", (void**)&player);
            float level = message->GetFloat("level", 0.0f);
            if (player && status == B_OK) {
                Station* station = player->GetStation();
                StationListViewItem* stationItem = fStationList->Item(station);
                if (stationItem) stationItem->SetFillRatio(level);
            }
            break;
        }
		
		case MSG_META_CHANGE : {
			BString meta;
			meta.SetToFormat("%s now plays %s", 
					message->GetString("station", "Unknown station"),
					message->GetString("streamtitle", "unknown title"));
			fStatusBar->SetText(meta.String());
			break;
		}
		
		case B_ABOUT_REQUESTED :
			be_app->AboutRequested();
			break;
            
        default:
            BWindow::MessageReceived(message);
    }
}

bool MainWindow::QuitRequested() {
	fSettings->Save();
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

void MainWindow::SetVisible(bool visible) {
    if (visible) {
        Show();
    } else {
        Hide();
    }
};


void
MainWindow::TogglePlay(StationListViewItem* stationItem) {
	status_t status;
	switch (stationItem->State()) {
		case StreamPlayer::Stopped : {
			StreamPlayer* player = new StreamPlayer(stationItem->GetStation(), this);
			status = player->InitCheck();
			if ((status == B_OK) && (status = player->Play() == B_OK)) {
				stationItem->SetPlayer(player);
				stationItem->StateChanged(StreamPlayer::Playing);
				BString success;
				success.SetToFormat("Now playing %s", 
				stationItem->GetStation()->Name()->String());
				fStatusBar->SetText(success);
				fStatusBar->Invalidate();
			} else {
				delete player;
				player = NULL;
				BString error;
				error.SetToFormat("Failed playing station %s: %s",
						stationItem->GetStation()->Name()->String(),
						strerror(status));
				fStatusBar->SetText(error);
			}  
			break;
		}
		case StreamPlayer::Buffering :
		case StreamPlayer::Playing : {
			StreamPlayer* player = stationItem->Player();
			if (player) 
				player->Stop();

			stationItem->StateChanged(StreamPlayer::Stopped);
			fStatusBar->SetText(NULL);
			break;
		}
	}
}

void MainWindow::DisplaySettings() {
    return;
}


void MainWindow::DisplayTipOfDay() {
    return;
}


void MainWindow::SetupActions() {

/*  KStandardAction::preferences(this, SLOT(display_global_settings_dialog()),
                                actionCollection());

    KStandardAction::quit(kapp, SLOT(quit()), actionCollection());

    showStreamdirectoryAction = new KToggleAction(this);
    showStreamdirectoryAction->setText(m_streamDirectory->windowTitle());
    QList<QKeySequence> showStreamdirectoryAction_tempShortcutList;
    //showStreamdirectoryAction_tempShortcutList.append(Qt::Key_S);
    //showStreamdirectoryAction_tempShortcutList.append(Qt::Key_MediaRecord);
    //showStreamdirectoryAction_tempShortcutList.append(Qt::CTRL + Qt::Key_S);
    //showStreamdirectoryAction_tempShortcutList.append(Qt::META + Qt::Key_V);
    showStreamdirectoryAction->setShortcuts(showStreamdirectoryAction_tempShortcutList);
    connect(showStreamdirectoryAction, SIGNAL(toggled(bool)),
            this, SLOT(showStreamdirectory(bool)));
    connect(m_streamDirectory, SIGNAL(willAcceptCloseEventFromWindowSystem(bool)),
            showStreamdirectoryAction, SLOT(setChecked(bool)));
    actionCollection()->addAction("showStreamdirectory", showStreamdirectoryAction);
    showStreamdirectoryAction->setChecked(settings_general::showStreamdirectory());

    KAction *importKradioripperSettingsAction = new KAction(this);
    importKradioripperSettingsAction->setText(
      i18nc("@action:inmenu", "Import KRadioRipper settings"));
    connect(importKradioripperSettingsAction, SIGNAL(triggered(bool)),
            this, SLOT(askForKradioripperImport()));
    actionCollection()->addAction("importKradioripperSettings", importKradioripperSettingsAction);

    KStandardAction::tipOfDay(this, SLOT(displayTipOfDay()), actionCollection());
 */
    return;
}

void MainWindow::AskForKradioripperImport() {
//  importKradioripperSettings::askForImport();
    return;
}
