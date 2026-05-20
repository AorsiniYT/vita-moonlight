#include "utils/connection_test.hpp"
#include <borealis.hpp>
#include <moonbeam.hpp>
#include "debug.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <curl/curl.h>

#ifdef __PSV__
#include <psp2/kernel/clib.h>
#define VITALOG sceClibPrintf
#else
#define VITALOG(...) ((void)0)
#endif

namespace utils {

void startConnectionTest(const HostInfo& host) {
    brls::Box* content = new brls::Box(brls::Axis::COLUMN);
    content->setAlignItems(brls::AlignItems::CENTER);
    content->setJustifyContent(brls::JustifyContent::CENTER);
    content->setPadding(20.0f);

    brls::Label* titleLabel = new brls::Label();
    titleLabel->setText(brls::getStr("host_dialog/connection_test/title"));
    titleLabel->setFontSize(20.0f);
    titleLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    titleLabel->setMarginBottom(20.0f);
    content->addView(titleLabel);

    brls::ProgressSpinner* spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::NORMAL);
    content->addView(spinner);

    brls::Label* statusLabel = new brls::Label();
    statusLabel->setText(brls::getStr("host_dialog/connection_test/testing"));
    statusLabel->setFontSize(16);
    statusLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    statusLabel->setMarginTop(15.0f);
    content->addView(statusLabel);

    brls::Label* pctLabel = new brls::Label();
    pctLabel->setText("0%");
    pctLabel->setFontSize(14);
    pctLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    pctLabel->setMarginTop(5.0f);
    content->addView(pctLabel);

    brls::Dialog* progressDialog = new brls::Dialog(content);
    progressDialog->setCancelable(false); // Force using the Cancel button to exit safely

    // Create the tester instance (using shared_ptr to safely capture in lambdas/threads)
    std::string currentLocale = brls::Application::getLocale();
    bool limit24G = false;
    std::string deviceName = "Device";
#ifdef __PSV__
    limit24G = true; // PS Vita is hardware-limited to 2.4GHz
    deviceName = "PS Vita";
#elif defined(__SWITCH__)
    deviceName = "Nintendo Switch";
#elif defined(__WIIU__)
    deviceName = "Wii U";
#endif
    auto tester = std::make_shared<moonbeam::ConnectionTester>(host.ip, limit24G, deviceName, currentLocale);

    progressDialog->addButton(brls::getStr("host_dialog/connection_test/cancel"), [tester, statusLabel]() {
        statusLabel->setText(brls::getStr("host_dialog/connection_test/cancelling"));
        tester->cancel();
    });

    progressDialog->open();

    tester->setProgressCallback([statusLabel, pctLabel](float progress, const std::string& status) {
        brls::sync([statusLabel, pctLabel, progress, status]() {
            statusLabel->setText(status);
            int percent = static_cast<int>(progress * 100.0f);
            pctLabel->setText(std::to_string(percent) + "%");
        });
    });

    std::thread([tester, progressDialog, host]() {
        auto result = tester->run();

        // Print results to debug log
        vita_debug_log("[Moonbeam] Connection test finished for Host: %s (%s)", host.name.c_str(), host.ip.c_str());
        if (!result.success) {
            vita_debug_log("[Moonbeam] Test failed: %s", result.error_message.c_str());
        } else {
            vita_debug_log("[Moonbeam] Quality Rating: %s", result.rating.c_str());
            vita_debug_log("[Moonbeam] Latency: Avg: %.1f ms (min: %.1f, max: %.1f), Jitter: %.1f ms, Packet Loss: %.1f%%",
                           result.avg_ping_ms, result.min_ping_ms, result.max_ping_ms, result.jitter_ms, result.packet_loss_pct);
            vita_debug_log("[Moonbeam] Bandwidth: %.2f Mbps", result.speed_mbps);
            vita_debug_log("[Moonbeam] Recommendation: %s", result.recommendation.c_str());
        }

        brls::sync([result, progressDialog, host]() {
            progressDialog->close([result, host]() {
                if (!result.success) {
                    if (result.error_message == "Test cancelled by user.") {
                        return;
                    }
                    brls::Dialog* errDialog = new brls::Dialog(result.error_message);
                    errDialog->setCancelable(true);
                    errDialog->addButton(brls::getStr("host_dialog/dialog/ok"), []() {
                        // Closed automatically by Borealis
                    });
                    errDialog->open();
                    return;
                }

                brls::Box* reportContent = new brls::Box(brls::Axis::COLUMN);
                reportContent->setAlignItems(brls::AlignItems::FLEX_START);
                reportContent->setJustifyContent(brls::JustifyContent::FLEX_START);
                reportContent->setPadding(25.0f);
                reportContent->setWidth(550.0f);

                brls::Label* repTitleLabel = new brls::Label();
                repTitleLabel->setText(brls::getStr("host_dialog/connection_test/report_title"));
                repTitleLabel->setFontSize(20.0f);
                repTitleLabel->setMarginBottom(15.0f);
                reportContent->addView(repTitleLabel);

                brls::Label* hostNameLabel = new brls::Label();
                hostNameLabel->setText(host.name + " (" + host.ip + ")");
                hostNameLabel->setFontSize(18);
                hostNameLabel->setMarginBottom(15.0f);
                reportContent->addView(hostNameLabel);

                brls::Label* ratingLabel = new brls::Label();
                std::string ratingStr = brls::getStr("host_dialog/connection_test/quality") + " " + result.rating;
                if (result.rating_raw == "Excellent") {
                    ratingLabel->setTextColor(nvgRGB(46, 204, 113));
                } else if (result.rating_raw == "Good") {
                    ratingLabel->setTextColor(nvgRGB(52, 152, 219));
                } else if (result.rating_raw == "Fair") {
                    ratingLabel->setTextColor(nvgRGB(241, 196, 15));
                } else {
                    ratingLabel->setTextColor(nvgRGB(231, 76, 60));
                }
                ratingLabel->setText(ratingStr);
                ratingLabel->setFontSize(20);
                ratingLabel->setMarginBottom(20.0f);
                reportContent->addView(ratingLabel);

                auto addMetric = [reportContent](const std::string& name, const std::string& value) -> brls::Label* {
                    brls::Box* row = new brls::Box(brls::Axis::ROW);
                    row->setMarginBottom(8.0f);
                    
                    brls::Label* nameLabel = new brls::Label();
                    nameLabel->setText(name);
                    nameLabel->setFontSize(15);
                    nameLabel->setWidth(220.0f);
                    row->addView(nameLabel);

                    brls::Label* valLabel = new brls::Label();
                    valLabel->setText(value);
                    valLabel->setFontSize(15);
                    row->addView(valLabel);

                    reportContent->addView(row);
                    return valLabel;
                };

                char buffer[64];
                
                snprintf(buffer, sizeof(buffer), "%.1f ms (min: %.1f, max: %.1f)", result.avg_ping_ms, result.min_ping_ms, result.max_ping_ms);
                addMetric(brls::getStr("host_dialog/connection_test/avg_latency"), buffer);

                snprintf(buffer, sizeof(buffer), "%.1f ms", result.jitter_ms);
                addMetric(brls::getStr("host_dialog/connection_test/jitter"), buffer);

                snprintf(buffer, sizeof(buffer), "%.1f%%", result.packet_loss_pct);
                addMetric(brls::getStr("host_dialog/connection_test/packet_loss"), buffer);

                snprintf(buffer, sizeof(buffer), "%.1f Mbps", result.speed_mbps);
                addMetric(brls::getStr("host_dialog/connection_test/speed"), buffer);

                if (!result.server_version.empty()) {
                    std::string hostServerVal = result.host_name;
                    if (!result.server_version.empty()) {
                        if (!hostServerVal.empty()) hostServerVal += " (v" + result.server_version + ")";
                        else hostServerVal = "v" + result.server_version;
                    }
                    addMetric(brls::getStr("host_dialog/connection_test/host_server"), hostServerVal);
                }

                if (!result.server_state.empty()) {
                    bool isBusy = (result.server_state.find("BUSY") != std::string::npos);
                    std::string stateVal = isBusy ? 
                        brls::getStr("host_dialog/connection_test/state_busy") : 
                        brls::getStr("host_dialog/connection_test/state_ready");
                    brls::Label* stateLabel = addMetric(brls::getStr("host_dialog/connection_test/host_state"), stateVal);
                    if (isBusy) {
                        stateLabel->setTextColor(nvgRGB(231, 76, 60)); // Red
                    } else {
                        stateLabel->setTextColor(nvgRGB(46, 204, 113)); // Green
                    }
                }

                brls::Label* recTitle = new brls::Label();
                recTitle->setText(brls::getStr("host_dialog/connection_test/recommendation"));
                recTitle->setFontSize(16);
                recTitle->setMarginTop(15.0f);
                recTitle->setMarginBottom(5.0f);
                reportContent->addView(recTitle);

                brls::Label* recLabel = new brls::Label();
                recLabel->setText(result.recommendation);
                recLabel->setFontSize(14);
                recLabel->setWidth(500.0f);
                recLabel->setMarginBottom(10.0f);
                reportContent->addView(recLabel);

                brls::Dialog* reportDialog = new brls::Dialog(reportContent);
                reportDialog->setCancelable(true);
                reportDialog->addButton(brls::getStr("host_dialog/dialog/ok"), []() {
                    // Closed automatically by Borealis
                });
                reportDialog->open();
            });
        });
    }).detach();
}

static std::string replacePlaceholder(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

void startVideoBitrateTest(const HostInfo& host) {
    // Instantiate Moonbeam connection tester immediately to get translations
    std::string currentLocale = brls::Application::getLocale();
    bool limit24G = false;
    std::string deviceName = "Device";
#ifdef __PSV__
    limit24G = true; // PS Vita is hardware-limited to 2.4GHz
    deviceName = "PS Vita";
#elif defined(__SWITCH__)
    deviceName = "Nintendo Switch";
#elif defined(__WIIU__)
    deviceName = "Wii U";
#endif
    auto tester = std::make_shared<moonbeam::ConnectionTester>(host.ip, limit24G, deviceName, currentLocale);

    std::string confirmMsg = tester->translate("video_test_confirm_msg", "This test will evaluate your network stability across 5 different bitrates (2000, 4000, 6000, 8000, and 10000 Kbps).\n\nEach bitrate will be tested for 10 seconds. The entire test will take approximately 50 seconds.\n\nDo you want to start?");
    brls::Dialog* confirmDlg = new brls::Dialog(confirmMsg);
    confirmDlg->setCancelable(true);
    
    confirmDlg->addButton(brls::getStr("host_dialog/yes"), [host, tester]() {
        brls::Box* content = new brls::Box(brls::Axis::COLUMN);
        content->setAlignItems(brls::AlignItems::CENTER);
        content->setJustifyContent(brls::JustifyContent::CENTER);
        content->setPadding(20.0f);

        brls::Label* titleLabel = new brls::Label();
        titleLabel->setText(tester->translate("video_test_title", "Video Bitrate Diagnostic"));
        titleLabel->setFontSize(20.0f);
        titleLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        titleLabel->setMarginBottom(20.0f);
        content->addView(titleLabel);

        brls::ProgressSpinner* spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::NORMAL);
        content->addView(spinner);

        brls::Label* statusLabel = new brls::Label();
        statusLabel->setText("");
        statusLabel->setFontSize(16);
        statusLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        statusLabel->setMarginTop(15.0f);
        content->addView(statusLabel);

        brls::Label* pctLabel = new brls::Label();
        pctLabel->setText("0%");
        pctLabel->setFontSize(14);
        pctLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        pctLabel->setMarginTop(5.0f);
        content->addView(pctLabel);

        brls::Dialog* progressDialog = new brls::Dialog(content);
        progressDialog->setCancelable(false);

        progressDialog->addButton(tester->translate("video_test_cancel", "Cancel"), [tester, statusLabel]() {
            statusLabel->setText(tester->translate("video_test_cancelling", "Cancelling..."));
            tester->cancel();
        });

        progressDialog->open();

        tester->setProgressCallback([statusLabel, pctLabel](float progress, const std::string& status) {
            brls::sync([statusLabel, pctLabel, progress, status]() {
                statusLabel->setText(status);
                int percent = static_cast<int>(progress * 100.0f);
                pctLabel->setText(std::to_string(percent) + "%");
            });
        });

        std::thread([tester, progressDialog, host]() {
            const std::vector<int> bitrates = {2000, 4000, 6000, 8000, 10000};
            auto result = tester->runVideoBitrateTest(bitrates);

            // Log detailed results to Vita Debug Log
            vita_debug_log("[Moonbeam] Video Bitrate Test finished for Host: %s (%s)", host.name.c_str(), host.ip.c_str());
            for (const auto& r : result.steps) {
                vita_debug_log("[Moonbeam] - Bitrate %d Kbps: %s (Achieved: %.1f Kbps)", 
                               r.bitrate, r.stable ? "STABLE" : "UNSTABLE", r.speed_kbps);
                vita_debug_log("[Moonbeam]   Latency under load: Avg: %.2f ms, Min: %.2f ms, Max: %.2f ms, Jitter: %.2f ms", 
                               r.avg_latency_ms, r.min_latency_ms, r.max_latency_ms, r.jitter_ms);
                vita_debug_log("[Moonbeam]   Stutter/Drop events: %d", r.stutters);
            }

            brls::sync([result, progressDialog, host, tester]() {
                progressDialog->close([result, host, tester]() {
                    if (!result.success) {
                        if (result.error_message == tester->translate("video_test_cancelling", "Cancelling...") || 
                            result.error_message == "Test cancelled by user." || 
                            result.error_message == "Prueba cancelada por el usuario.") {
                            return;
                        }
                        brls::Dialog* errDialog = new brls::Dialog(result.error_message);
                        errDialog->setCancelable(true);
                        errDialog->addButton(brls::getStr("host_dialog/dialog/ok"), []() {});
                        errDialog->open();
                        return;
                    }

                    // Show report
                    brls::Box* reportContent = new brls::Box(brls::Axis::COLUMN);
                    reportContent->setAlignItems(brls::AlignItems::FLEX_START);
                    reportContent->setJustifyContent(brls::JustifyContent::FLEX_START);
                    reportContent->setPadding(25.0f);
                    reportContent->setWidth(550.0f);

                    brls::Label* repTitleLabel = new brls::Label();
                    repTitleLabel->setText(tester->translate("video_test_report_title", "Video Bitrate Report"));
                    repTitleLabel->setFontSize(20.0f);
                    repTitleLabel->setMarginBottom(15.0f);
                    reportContent->addView(repTitleLabel);

                    brls::Label* hostNameLabel = new brls::Label();
                    hostNameLabel->setText(host.name + " (" + host.ip + ")");
                    hostNameLabel->setFontSize(18);
                    hostNameLabel->setMarginBottom(15.0f);
                    reportContent->addView(hostNameLabel);

                    // Add each result row
                    for (const auto& r : result.steps) {
                        brls::Box* row = new brls::Box(brls::Axis::ROW);
                        row->setMarginBottom(8.0f);
                        row->setAlignItems(brls::AlignItems::CENTER);

                        brls::Label* brLabel = new brls::Label();
                        brLabel->setText(std::to_string(r.bitrate) + " Kbps:");
                        brLabel->setFontSize(16);
                        brLabel->setWidth(150.0f);
                        row->addView(brLabel);

                        brls::Label* statusVal = new brls::Label();
                        if (r.stable) {
                            statusVal->setText(tester->translate("video_test_stable", "Stable"));
                            statusVal->setTextColor(nvgRGB(46, 204, 113)); // Green
                        } else {
                            statusVal->setText(tester->translate("video_test_unstable", "Unstable"));
                            statusVal->setTextColor(nvgRGB(231, 76, 60)); // Red
                        }
                        statusVal->setFontSize(16);
                        statusVal->setWidth(120.0f);
                        row->addView(statusVal);

                        brls::Label* speedVal = new brls::Label();
                        std::string achStr = tester->translate("video_test_achieved", "Achieved: {speed} Kbps");
                        achStr = replacePlaceholder(achStr, "{speed}", std::to_string(static_cast<int>(r.speed_kbps)));
                        speedVal->setText(achStr);
                        speedVal->setFontSize(14);
                        speedVal->setTextColor(nvgRGB(180, 180, 180));
                        row->addView(speedVal);

                        reportContent->addView(row);
                    }

                    // Add Recommendation
                    brls::Label* recTitle = new brls::Label();
                    recTitle->setText(brls::getStr("host_dialog/connection_test/recommendation"));
                    recTitle->setFontSize(16);
                    recTitle->setMarginTop(20.0f);
                    recTitle->setMarginBottom(5.0f);
                    reportContent->addView(recTitle);

                    brls::Label* recLabel = new brls::Label();
                    if (result.highest_stable_bitrate > 0) {
                        std::string recMsg = tester->translate("video_test_rec_stable", "We recommend setting your streaming bitrate to {speed} Kbps for optimal quality.");
                        recMsg = replacePlaceholder(recMsg, "{speed}", std::to_string(result.highest_stable_bitrate));
                        recLabel->setText(recMsg);
                    } else {
                        recLabel->setText(tester->translate("video_test_rec_unstable", "Connection is unstable. We recommend using a lower resolution or improving your Wi-Fi signal."));
                    }
                    recLabel->setFontSize(14);
                    recLabel->setWidth(500.0f);
                    recLabel->setMarginBottom(10.0f);
                    reportContent->addView(recLabel);

                    brls::Dialog* reportDialog = new brls::Dialog(reportContent);
                    reportDialog->setCancelable(true);
                    reportDialog->addButton(brls::getStr("host_dialog/dialog/ok"), []() {});
                    reportDialog->open();
                });
            });
        }).detach();
    });

    confirmDlg->addButton(brls::getStr("host_dialog/no"), []() {});
    confirmDlg->open();
}

} // namespace utils
