#include "app.h"
#include "number_input.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <tinyfiledialogs.h>

namespace {

constexpr const char* kSettingsFile = "isprime_settings.json";

const ImVec4 kErrorColour(1.0f, 0.45f, 0.45f, 1.0f);
const ImU32 kDisagreeRowColour = IM_COL32(110, 30, 30, 255);

float Snap(float v) { return std::floor(v + 0.5f); }

// "0.52 µs", "4.10 ms", "12.84 s" -- the unit adapts so the number stays short.
std::string FormatDuration(double seconds) {
    char buf[64];
    if (seconds < 1e-6)      std::snprintf(buf, sizeof buf, "%.0f ns", seconds * 1e9);
    else if (seconds < 1e-3) std::snprintf(buf, sizeof buf, "%.2f \xC2\xB5s", seconds * 1e6); // UTF-8 µ
    else if (seconds < 1.0)  std::snprintf(buf, sizeof buf, "%.2f ms", seconds * 1e3);
    else                     std::snprintf(buf, sizeof buf, "%.2f s", seconds);
    return buf;
}

// Number box: let through digits and the separators ParseNumber ignores;
// everything else is dropped as it's typed (or pasted).
int NumberCharFilter(ImGuiInputTextCallbackData* data) {
    const ImWchar c = data->EventChar;
    const bool allowed = (c >= '0' && c <= '9') || c == ' ' || c == ',' || c == '_' || c == '\'';
    return allowed ? 0 : 1;
}

// Moves the cursor so an item of the given width ends at the cell's right edge.
void RightAlign(float width) {
    const float avail = ImGui::GetContentRegionAvail().x;
    if (avail > width) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);
}

std::uint64_t FreshSeed() {
    std::random_device rd;
    return (static_cast<std::uint64_t>(rd()) << 32) ^ rd();
}

} // namespace

App::App() {
    // StyleColorsDark makes windows and popups slightly see-through, which
    // lets the rows behind show through menus and Preferences. Make them solid.
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f;
    ImGui::GetStyle().Colors[ImGuiCol_PopupBg].w = 1.0f;
    // Captured before any scaling: SetUiScale() always re-derives from this,
    // because ScaleAllSizes() multiplies in place and would compound.
    baseStyle_ = ImGui::GetStyle();
    LoadSettings();
    SetUiScale(uiScale_);
    AddRow();
}

// ---------------------------------------------------------------------------
// Scale & settings
// ---------------------------------------------------------------------------

void App::ApplySuggestedScaleIfUnset(float suggestedScale) {
    if (hasSavedScale_) return;
    SetUiScale(suggestedScale);
}

void App::SetUiScale(float scale) {
    uiScale_ = std::clamp(scale, 0.5f, 3.0f);
    ImGuiStyle& st = ImGui::GetStyle();
    st = baseStyle_;
    st.ScaleAllSizes(uiScale_);

    // Whole pixels everywhere, so borders and text never sit on half-pixels.
    auto snap = [](float& v) { v = Snap(v); };
    auto snap2 = [&](ImVec2& v) { snap(v.x); snap(v.y); };
    snap2(st.WindowPadding); snap2(st.FramePadding); snap2(st.CellPadding);
    snap2(st.ItemSpacing); snap2(st.ItemInnerSpacing); snap2(st.TouchExtraPadding);
    snap(st.IndentSpacing); snap(st.ScrollbarSize); snap(st.GrabMinSize);
    snap(st.WindowRounding); snap(st.ChildRounding); snap(st.FrameRounding);
    snap(st.PopupRounding); snap(st.ScrollbarRounding); snap(st.GrabRounding);
    snap(st.TabRounding);
    // Borders stay exactly 1px (or off) rather than scaling to a blurry 1.37px.
    st.WindowBorderSize = baseStyle_.WindowBorderSize > 0.0f ? 1.0f : 0.0f;
    st.ChildBorderSize = baseStyle_.ChildBorderSize > 0.0f ? 1.0f : 0.0f;
    st.PopupBorderSize = baseStyle_.PopupBorderSize > 0.0f ? 1.0f : 0.0f;
    st.FrameBorderSize = baseStyle_.FrameBorderSize > 0.0f ? 1.0f : 0.0f;

    ImGui::GetIO().FontGlobalScale = 1.0f; // the font itself is re-baked at the scaled size instead
    fontDirty_ = true;
    SaveSettings();
}

bool App::ConsumeFontRebuild(float& fontPixelSize) {
    if (!fontDirty_) return false;
    fontDirty_ = false;
    fontPixelSize = std::max(1.0f, Snap(13.0f * uiScale_)); // ImGui's built-in font is designed at 13px
    return true;
}

void App::LoadSettings() {
    std::ifstream in(kSettingsFile);
    if (!in) return; // first run -- defaults stand
    try {
        nlohmann::json j = nlohmann::json::parse(in);
        if (j.contains("uiScale")) { uiScale_ = j["uiScale"].get<float>(); hasSavedScale_ = true; }
        if (j.contains("mrRounds")) mrRounds_ = std::clamp(j["mrRounds"].get<int>(), 1, 64);
        if (j.contains("seed")) seed_ = j["seed"].get<std::uint64_t>();
        if (j.contains("repeatFastRuns")) repeatFastRuns_ = j["repeatFastRuns"].get<bool>();
    } catch (const std::exception&) {
        // A corrupt settings file shouldn't stop the app starting.
    }
}

void App::SaveSettings() const {
    nlohmann::json j = {
        {"uiScale", uiScale_},
        {"mrRounds", mrRounds_},
        {"seed", seed_},
        {"repeatFastRuns", repeatFastRuns_},
    };
    std::ofstream(kSettingsFile) << j.dump(2) << "\n";
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

void App::AddRow(const std::string& title, const std::string& number) {
    Row row;
    row.id = nextId_++;
    row.title = title.empty() ? "Number " + std::to_string(row.id) : title;
    row.number = number;
    rows_.push_back(std::move(row));
}

void App::NewFile() {
    for (Row& r : rows_) if (r.run) r.run->cancel = true;
    rows_.clear();
    nextId_ = 1;
    AddRow();
    currentFile_.clear();
    statusMessage_.clear();
}

void App::SaveToFile(const std::string& path) {
    nlohmann::json arr = nlohmann::json::array();
    for (const Row& r : rows_) arr.push_back({{"title", r.title}, {"number", r.number}});
    std::ofstream out(path);
    if (!out) { statusMessage_ = "Could not write " + path; return; }
    out << nlohmann::json{{"rows", arr}}.dump(2) << "\n";
    currentFile_ = path;
    statusMessage_ = "Saved " + path;
}

void App::OpenFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) { statusMessage_ = "Could not open " + path; return; }
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(in);
    } catch (const nlohmann::json::parse_error&) {
        statusMessage_ = path + " is not valid JSON";
        return;
    }
    if (!j.contains("rows") || !j["rows"].is_array()) {
        statusMessage_ = path + " has no \"rows\" list";
        return;
    }
    for (Row& r : rows_) if (r.run) r.run->cancel = true;
    rows_.clear();
    nextId_ = 1;
    for (const auto& item : j["rows"]) {
        if (!item.is_object()) continue;
        AddRow(item.value("title", std::string{}), item.value("number", std::string{}));
    }
    if (rows_.empty()) AddRow();
    currentFile_ = path;
    statusMessage_ = "Opened " + path;
}

void App::SaveAs() {
    const char* patterns[1] = {"*.json"};
    const char* chosen = tinyfd_saveFileDialog("Save numbers",
        currentFile_.empty() ? "numbers.json" : currentFile_.c_str(), 1, patterns, "JSON files");
    if (chosen) SaveToFile(chosen);
}

void App::Open() {
    const char* patterns[1] = {"*.json"};
    const char* chosen = tinyfd_openFileDialog("Open numbers", "", 1, patterns, "JSON files", 0);
    if (chosen) OpenFromFile(chosen);
}

// A ready-made demo set: edge cases, the composites that fool weak versions
// of Miller-Rabin, and big primes/semiprimes where the algorithms separate.
void App::LoadExamples() {
    for (Row& r : rows_) if (r.run) r.run->cancel = true;
    rows_.clear();
    nextId_ = 1;
    AddRow("One (neither prime nor composite)", "1");
    AddRow("Smallest prime", "2");
    AddRow("Carmichael number (3 x 11 x 17)", "561");
    AddRow("Strong pseudoprime to base 2 (23 x 89)", "2047");
    AddRow("Strong pseudoprime to bases 2, 3, 5, 7", "3215031751");
    AddRow("Prime: 10^9 + 7", "1000000007");
    AddRow("Mersenne prime 2^61 - 1", "2305843009213693951");
    AddRow("Product of the two largest 32-bit primes", "18446743979220271189");
    AddRow("Largest 64-bit prime", "18446744073709551557");
    currentFile_.clear();
    statusMessage_ = "Loaded example numbers -- press Go! on any row";
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void App::Draw() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    const ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoBringToFrontOnFocus; // keeps the Settings window in front of it

    ImGui::Begin("isPrime", nullptr, hostFlags);
    DrawMenuBar();

    int removeId = -1;
    for (Row& row : rows_) {
        ImGui::PushID(row.id);
        bool removeMe = false;
        DrawRow(row, removeMe);
        if (removeMe) removeId = row.id;
        ImGui::PopID();
    }
    if (removeId != -1) {
        auto it = std::find_if(rows_.begin(), rows_.end(), [&](const Row& r) { return r.id == removeId; });
        if (it != rows_.end()) {
            if (it->run) it->run->cancel = true;
            rows_.erase(it);
        }
    }

    if (ImGui::Button("Add Number")) AddRow();
    if (!statusMessage_.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", statusMessage_.c_str());
    }
    ImGui::End();

    DrawSettingsWindow();
}

void App::DrawMenuBar() {
    if (!ImGui::BeginMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) NewFile();
        if (ImGui::MenuItem("Open...")) Open();
        if (ImGui::MenuItem("Save")) {
            if (currentFile_.empty()) SaveAs(); else SaveToFile(currentFile_);
        }
        if (ImGui::MenuItem("Save As...")) SaveAs();
        ImGui::Separator();
        if (ImGui::MenuItem("Load example numbers")) LoadExamples();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Settings")) {
        if (ImGui::MenuItem("Preferences...")) showSettings_ = true;
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

void App::StartRow(Row& row) {
    const ParsedNumber parsed = ParseNumber(row.number);
    if (!parsed.ok) return;
    if (row.run) row.run->cancel = true; // abandon any run still going for this row
    RunSettings s;
    s.mrRounds = mrRounds_;
    s.seed = seed_ != 0 ? seed_ : FreshSeed();
    s.repeatFastRuns = repeatFastRuns_;
    row.run = StartRun(parsed.value, s);
    row.runNumber = parsed.value;
}

void App::DrawRow(Row& row, bool& removeMe) {
    ImGui::BeginChild("row", ImVec2(0, 0), ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
    const ImGuiStyle& st = ImGui::GetStyle();

    // Title: an editable label, drawn without a frame until hovered/focused
    // so it reads as a heading rather than a form field.
    const float removeWidth = ImGui::GetFrameHeight();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - removeWidth - st.ItemSpacing.x);
    ImGui::InputTextWithHint("##title", "Title", &row.title);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("X", ImVec2(removeWidth, removeWidth))) removeMe = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove this number");

    // Number + Go!
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Number:");
    ImGui::SameLine();
    const float goWidth = Snap(std::max(ImGui::CalcTextSize("Go!").x + st.FramePadding.x * 2.0f,
                                        ImGui::GetFrameHeight() * 2.5f));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - goWidth - st.ItemSpacing.x);
    const bool enterPressed = ImGui::InputTextWithHint("##number", "e.g. 561", &row.number,
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter, NumberCharFilter);
    const ParsedNumber parsed = ParseNumber(row.number);
    ImGui::SameLine();
    ImGui::BeginDisabled(!parsed.ok);
    const bool goPressed = ImGui::Button("Go!", ImVec2(goWidth, 0));
    ImGui::EndDisabled();
    if (parsed.ok && (goPressed || enterPressed)) StartRow(row);

    if (!parsed.ok && !parsed.error.empty()) {
        ImGui::TextColored(kErrorColour, "%s", parsed.error.c_str());
    } else if (row.run && parsed.ok && parsed.value != row.runNumber) {
        ImGui::TextDisabled("Results below are for %llu -- press Go! to test the new number",
                            static_cast<unsigned long long>(row.runNumber));
    }

    DrawResultsTable(row);
    ImGui::EndChild();
}

void App::DrawResultsTable(Row& row) {
    const ImGuiStyle& st = ImGui::GetStyle();
    Run* run = row.run.get();

    // Which algorithms disagree with the majority? (Prime == Probably prime.)
    bool odd[kAlgorithmCount] = {};
    bool anyDisagreement = false;
    if (run) {
        int counts[4] = {};
        int done = 0;
        Verdict v[kAlgorithmCount];
        bool isDone[kAlgorithmCount] = {};
        for (int i = 0; i < kAlgorithmCount; i++) {
            if (run->slots[i].state.load(std::memory_order_acquire) != SlotState::Done) continue;
            v[i] = Normalised(run->slots[i].verdict);
            isDone[i] = true;
            counts[static_cast<int>(v[i])]++;
            done++;
        }
        const int maxCount = *std::max_element(counts, counts + 4);
        if (done >= 2 && maxCount < done) {
            anyDisagreement = true;
            for (int i = 0; i < kAlgorithmCount; i++) {
                // With a clear majority, flag only the dissenters; with no
                // majority (2 done, each different), flag them all.
                odd[i] = isDone[i] && (counts[static_cast<int>(v[i])] < maxCount || maxCount * 2 <= done);
            }
        }
    }

    const float resultWidth = ImGui::CalcTextSize("Probably prime").x;
    const float cancelWidth = ImGui::CalcTextSize("Cancel").x + st.FramePadding.x * 2.0f;
    const float runtimeWidth = std::max(ImGui::CalcTextSize("999.99 ms").x,
                                        ImGui::CalcTextSize("999.9 s").x + st.ItemSpacing.x + cancelWidth);

    const ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_BordersInnerH;
    if (!ImGui::BeginTable("results", 3, flags)) return;
    ImGui::TableSetupColumn("Algorithm", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, Snap(resultWidth));
    ImGui::TableSetupColumn("Runtime", ImGuiTableColumnFlags_WidthFixed, Snap(runtimeWidth));
    ImGui::TableHeadersRow();

    for (int i = 0; i < kAlgorithmCount; i++) {
        const AlgorithmInfo& info = kAlgorithms[i];
        ImGui::PushID(i);
        ImGui::TableNextRow();
        if (odd[i]) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, kDisagreeRowColour);

        // Algorithm
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(info.name);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Primary author: %s", info.author);
        if (info.isPlaceholder()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(placeholder)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Still the stand-in body (trial division) -- the real %s\n"
                                  "implementation hasn't been plugged in yet.", info.name);
        }

        SlotState state = run ? run->slots[i].state.load(std::memory_order_acquire) : SlotState::Pending;
        // Cancel takes effect on screen immediately. The worker can't
        // interrupt a call already in progress (e.g. a long AKS run), so it
        // only marks the slot Cancelled once that call returns -- without
        // this, the row would keep showing "running..." until then.
        if (run && run->cancel.load() && (state == SlotState::Running || state == SlotState::Pending))
            state = SlotState::Cancelled;
        const AlgoSlot* slot = run ? &run->slots[i] : nullptr;

        // Result
        ImGui::TableSetColumnIndex(1);
        if (!run) {
            ImGui::TextDisabled("-");
        } else {
            switch (state) {
                case SlotState::Pending:   ImGui::TextDisabled("waiting"); break;
                case SlotState::Running:   ImGui::TextDisabled("running..."); break;
                case SlotState::Done:      ImGui::TextUnformatted(ToString(slot->verdict)); break;
                case SlotState::Cancelled: ImGui::TextDisabled("cancelled"); break;
                case SlotState::Error:
                    ImGui::TextColored(kErrorColour, "error");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", slot->error.c_str());
                    break;
            }
        }

        // Runtime
        ImGui::TableSetColumnIndex(2);
        if (run && state == SlotState::Done) {
            const std::string text = FormatDuration(slot->secondsPerCall);
            RightAlign(ImGui::CalcTextSize(text.c_str()).x);
            ImGui::TextUnformatted(text.c_str());
            if (ImGui::IsItemHovered()) {
                std::string tip = slot->calls > 1
                    ? "Median per-call time over 7 batches (" + std::to_string(slot->calls) + " calls in total)"
                    : std::string("Single run");
                if (i == 1) {
                    tip += "\n" + std::to_string(run->settings.mrRounds) + " rounds, seed " +
                           std::to_string(run->settings.seed);
                }
                ImGui::SetTooltip("%s", tip.c_str());
            }
        } else if (run && state == SlotState::Running) {
            // Live elapsed time while a slow algorithm (AKS) is still going.
            const double elapsed = static_cast<double>(SteadyNowNs() - slot->startNs.load()) / 1e9;
            char buf[32];
            std::snprintf(buf, sizeof buf, "%.1f s", elapsed);
            RightAlign(ImGui::CalcTextSize(buf).x + st.ItemSpacing.x + cancelWidth);
            ImGui::TextDisabled("%s", buf); // SmallButton has no vertical padding, so the row height doesn't jump
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel")) run->cancel = true;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Stops waiting for this run. A call already in progress can't be\n"
                                  "interrupted, so it finishes in the background and is discarded.");
        }
        ImGui::PopID();
    }
    ImGui::EndTable();

    if (anyDisagreement) {
        ImGui::TextColored(kErrorColour, "The algorithms disagree -- at least one of them is wrong for this number.");
    }
}

void App::DrawSettingsWindow() {
    if (!showSettings_) return;
    if (!ImGui::Begin("Preferences", &showSettings_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    const float fieldWidth = Snap(180.0f * uiScale_);

    ImGui::SeparatorText("Display");
    // Applied on release, not while dragging: re-scaling moves the slider
    // under the mouse, which would make it jump around mid-drag.
    if (!scaleSliderActive_) pendingScale_ = uiScale_;
    ImGui::SetNextItemWidth(fieldWidth);
    ImGui::SliderFloat("UI scale", &pendingScale_, 0.5f, 3.0f, "%.2fx");
    scaleSliderActive_ = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) SetUiScale(pendingScale_);

    ImGui::SeparatorText("Miller-Rabin (random)");
    ImGui::SetNextItemWidth(fieldWidth);
    ImGui::SliderInt("Rounds (k)", &mrRounds_, 1, 64);
    if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();
    ImGui::TextDisabled("Error probability <= 4^-k = %.3g", std::pow(4.0, -mrRounds_));
    ImGui::SetNextItemWidth(fieldWidth);
    ImGui::InputScalar("Seed", ImGuiDataType_U64, &seed_);
    if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();
    ImGui::TextDisabled("0 = a new random seed every time Go! is pressed");

    ImGui::SeparatorText("Timing");
    if (ImGui::Checkbox("Repeat fast runs and show the median", &repeatFastRuns_)) SaveSettings();
    ImGui::TextDisabled("Runs under 1 ms are timed in 7 batches; slower runs are timed once.");
    ImGui::TextDisabled("These are demo timings; the report's numbers should come from repeated benchmark runs.");

    ImGui::End();
}
