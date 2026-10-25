// app.h -- all of the GUI's state and drawing. main.cpp just owns the
// window and calls App::Draw() once per frame.

#pragma once
#include "imgui.h"
#include "runner.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct Row {
    int id = 0;
    std::string title;
    std::string number;
    std::shared_ptr<Run> run;  // null until Go! is first pressed
    std::uint64_t runNumber = 0; // the n that `run` was started with
};

class App {
public:
    App();
    void Draw();

    // Called once at startup with the monitor's content scale; ignored if
    // the user already saved a UI scale.
    void ApplySuggestedScaleIfUnset(float suggestedScale);

    // The font is baked at the exact pixel size the UI scale needs (sharp
    // text instead of a stretched bitmap). That can't happen mid-frame, so
    // SetUiScale() raises a flag and main.cpp rebuilds between frames.
    bool ConsumeFontRebuild(float& fontPixelSize);

private:
    std::vector<Row> rows_;
    int nextId_ = 1;

    // Settings (persisted to isprime_settings.json)
    float uiScale_ = 1.0f;
    int mrRounds_ = 20;
    std::uint64_t seed_ = 0; // 0 = fresh random seed for every Go!
    bool repeatFastRuns_ = true;
    bool hasSavedScale_ = false;

    // UI state
    bool fontDirty_ = true;
    bool showSettings_ = false;
    float pendingScale_ = 1.0f;     // the slider's value while dragging; applied on release
    bool scaleSliderActive_ = false;
    std::string currentFile_;
    std::string statusMessage_;
    ImGuiStyle baseStyle_{};

    void AddRow(const std::string& title = "", const std::string& number = "");
    void DrawMenuBar();
    void DrawRow(Row& row, bool& removeMe);
    void DrawResultsTable(Row& row);
    void DrawSettingsWindow();
    void StartRow(Row& row);

    void SetUiScale(float scale);
    void LoadSettings();
    void SaveSettings() const;

    void NewFile();
    void SaveToFile(const std::string& path);
    void OpenFromFile(const std::string& path);
    void SaveAs();
    void Open();
    void LoadExamples();
};
