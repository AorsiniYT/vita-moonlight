/*
    Copyright 2025 AorsiniYT

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
#pragma once

#include "view/grid_view.hpp"
#include "model/HostStorage.hpp"
#include "borealis.hpp"

#include <memory>

#include "GameStreamClient.hpp"
class SessionAppSelect : public brls::Box {
  public:
    SessionAppSelect(const std::string& hostName);
    ~SessionAppSelect();

    void onLayout() override;

  private:
    void populateAppList();
  void AppSelected(const RemoteAppInfo& app, bool forceStart = false);
    // Helpers to show/hide the connection dialog and manage the GridView
    brls::Dialog* showConnectingDialog(const std::string& msg, brls::Visibility& outPrevGridVis);
    void restoreGridViewAndInputs(brls::Visibility prevGridVis);

    BRLS_BIND(brls::Label, app_select_title, "app_select_title");
    BRLS_BIND(brls::Label, app_select_subtitle, "app_select_subtitle");
    BRLS_BIND(brls::Box, app_grid_container, "app_grid_container");
    BRLS_BIND(brls::Box, grid_placeholder, "grid_placeholder");
    BRLS_BIND(brls::Label, app_select_empty, "app_select_empty");
    BRLS_BIND(brls::ProgressSpinner, loading_spinner, "loading_spinner");

    HostInfo host;
    GridView* gridView = nullptr;
    brls::ProgressSpinner* spinner = nullptr;
  // Avoid displaying the active session dialog if the user has already managed it
  bool suppressActiveDialog = false;
  // Avoid showing the active session dialog more than once within the same instance
  bool activeDialogShown = false;
  // Controls waiting before loading apps until Moonmic/Sunshine responds
  bool sunshineReady = false;
  bool sunshineCheckInFlight = false;
  bool moonmicNotified = false;
  bool moonmicLastStatus = false;
  // Resolution prompt (displayed when entering the host, not when launching the app)
  bool resolutionPromptShown = false;

  // Track if view is still alive to prevent async callbacks from accessing destroyed 'this'
  std::shared_ptr<bool> isAlive;
};
