#include "debug/ConsoleWindow.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <string_view>

namespace haylen::debug {

bool ConsoleWindow::draw(Console& console) {
    shown = &console;
    bool open = true;
    if (!ImGui::Begin("Console", &open)) {
        ImGui::End();
        return open;
    }

    // The commands and the history open above the output, which takes the room that the line below leaves.
    drawCommands(console);
    drawHistory(console);
    const float footer = ImGui::GetFrameHeightWithSpacing() * 2.0F + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginChild("output", {0.0F, -footer}, ImGuiChildFlags_Borders)) {
        drawOutput(console);
    }
    ImGui::EndChild();

    if (focusPending) {
        ImGui::SetKeyboardFocusHere();
        focusPending = false;
    }
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Run").x - ImGui::GetStyle().FramePadding.x * 2.0F - ImGui::GetStyle().ItemSpacing.x);
    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion;
    const bool entered = ImGui::InputTextWithHint("##line", "Type a command, or \"help\"", &line, flags, &edit, this);
    ImGui::SameLine();
    if (entered || ImGui::Button("Run")) {
        submit(console);
    }

    if (!candidates.empty()) {
        ImGui::TextDisabled("%s", "Completions:");
        for (const std::string& candidate : candidates) {
            ImGui::SameLine();
            if (ImGui::SmallButton(candidate.c_str())) {
                line = candidate;
                candidates.clear();
                focusPending = true;
                break;
            }
        }
    } else {
        ImGui::TextDisabled("%s", "Tab completes, and the arrows go through the history.");
    }

    ImGui::End();
    return open;
}

void ConsoleWindow::drawOutput(const Console& console) {
    for (const Console::Line& entry : console.getOutput()) {
        switch (entry.kind) {
        case Console::Line::Kind::Input:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.55F, 0.75F, 1.0F, 1.0F});
            ImGui::TextWrapped("> %s", entry.text.c_str());
            ImGui::PopStyleColor();
            break;
        case Console::Line::Kind::Error:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0F, 0.45F, 0.4F, 1.0F});
            ImGui::TextWrapped("%s", entry.text.c_str());
            ImGui::PopStyleColor();
            break;
        case Console::Line::Kind::Output:
            ImGui::TextWrapped("%s", entry.text.c_str());
            break;
        }
    }
    // New output scrolls to the end, unless the player scrolled up to read.
    if (console.getOutput().size() != shownLines || scrollPending) {
        if (scrollPending || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0F) {
            ImGui::SetScrollHereY(1.0F);
        }
        shownLines = console.getOutput().size();
        scrollPending = false;
    }
}

// A command without arguments runs from its button, and one with arguments puts its name on the line, where the on-screen keyboard types the rest.
void ConsoleWindow::drawCommands(Console& console) {
    if (!ImGui::CollapsingHeader("Commands")) {
        return;
    }
    for (const Console::Command* command : console.getCommands()) {
        ImGui::PushID(command->name.c_str());
        if (ImGui::Button(Console::describeUsage(*command).c_str())) {
            if (command->parameters.empty() || command->parameters.front().optional) {
                line = command->name;
                submit(console);
            } else {
                line = command->name + " ";
                focusPending = true;
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", command->help.c_str());
        ImGui::PopID();
    }
}

void ConsoleWindow::drawHistory(Console& console) {
    if (console.getHistory().empty() || !ImGui::CollapsingHeader("History")) {
        return;
    }
    const core::RingBuffer<std::string>& history = console.getHistory();
    for (std::size_t index = history.size(); index-- > 0;) {
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::SmallButton("Run")) {
            line = history[index];
            submit(console);
            ImGui::PopID();
            break;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Edit")) {
            line = history[index];
            focusPending = true;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(history[index].c_str());
        ImGui::PopID();
    }
}

void ConsoleWindow::submit(Console& console) {
    (void)console.run(line);
    line.clear();
    candidates.clear();
    recalled = 0;
    focusPending = true;
    scrollPending = true;
}

// One candidate replaces the line, and several complete their common start and show as buttons.
std::string ConsoleWindow::complete(std::string_view typed) {
    candidates = shown->complete(typed);
    if (candidates.empty()) {
        return std::string(typed);
    }
    std::string_view common = candidates.front();
    for (const std::string& candidate : candidates) {
        const auto [first, second] = std::ranges::mismatch(common, candidate);
        common = common.substr(0, static_cast<std::size_t>(first - common.begin()));
    }
    std::string completed(common.size() > typed.size() ? common : typed);
    if (candidates.size() == 1) {
        candidates.clear();
    }
    return completed;
}

std::string ConsoleWindow::recall(int direction) {
    const std::size_t count = shown->getHistory().size();
    if (direction < 0 && recalled < count) {
        ++recalled;
    } else if (direction > 0 && recalled > 0) {
        --recalled;
    }
    return recalled == 0 ? std::string() : shown->getHistory()[count - recalled];
}

// The line is the buffer that ImGui edits during the callback, so the new text goes through the calls of the callback data.
int ConsoleWindow::edit(ImGuiInputTextCallbackData* data) {
    auto& window = *static_cast<ConsoleWindow*>(data->UserData);
    std::string replaced;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        replaced = window.complete({data->Buf, static_cast<std::size_t>(data->BufTextLen)});
    } else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        replaced = window.recall(data->EventKey == ImGuiKey_UpArrow ? -1 : 1);
    } else {
        return 0;
    }
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, replaced.c_str());
    return 0;
}

} // namespace haylen::debug
