#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>
#include "model/HostStorage.hpp"
#include "connection_manager.hpp"
#include "view/GridView.hpp"

class SessionAppSelect : public brls::Box {
public:
    SessionAppSelect(const std::string& hostName);
    ~SessionAppSelect();

    void onLayout() override;

private:
    HostInfo host;
    GridView* gridView = nullptr;
    brls::ProgressSpinner* spinner = nullptr;

    void populateAppList();
    void AppSelected(const RemoteAppInfo& app);

    BRLS_BIND(brls::Label, app_select_title, "app_select_title");
    BRLS_BIND(brls::Label, app_select_subtitle, "app_select_subtitle");
    BRLS_BIND(brls::Box, grid_placeholder, "grid_placeholder");
    BRLS_BIND(brls::Label, app_select_empty, "app_select_empty");
    BRLS_BIND(brls::ProgressSpinner, loading_spinner, "loading_spinner");
};