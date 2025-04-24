#include <Windows.h>
#include "public.h"
#include "vJoyInterface.h"

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

long MAX_AXIS_VALUE = 0x7FFF;

GLFWwindow* window;

bool InitImGui() {
    if (!glfwInit()) return false;

    window = glfwCreateWindow(1500, 1000, "Sampler", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    return true;
}

bool InitVJoy() {
    if (!vJoyEnabled()) {
        std::cout << "vJoy not detected!" << std::endl;
        return false;
    }

    std::cout << "vJoy detected!" << std::endl;
    return true;
}

bool AcquireVJoyPads(int size) {
    for (int i = 0; i < size; ++i) {
        VjdStat status = GetVJDStatus(i);

        // Acquire the target
        if ((status == VJD_STAT_OWN) || ((status == VJD_STAT_FREE) && (!AcquireVJD(i)))) {
            std::cout << "Failed to acquire vJoy device number" << i << std::endl;
            return false;
        }
        else {
            std::cout << "Acquired: vJoy device number " << i << std::endl;
        }

        ResetVJD(i);
    }
    return true;
}

void FreeVJoyPads(int size) {
    for (int i = 0; i < size; ++i) {
        RelinquishVJD(i);
    }
}

enum Curves {
    Linear = 0,
    Cubic,
};

struct CurveSettings {
    bool reversed = false;
    int curve_type = Linear;


    CurveSettings() = default;

    CurveSettings(const std::string& str) {
        std::string strip;
        for (char c : str) {
            if (c == '{' || c == '}' || c == ' ') continue;
            strip += c;
        }
        curve_type = strip[0] - '0';
        reversed = strip[2] - '0';
    }
};

struct JoystickData {
    std::string name;
    int jid;
    std::string guid;
    bool selected = false;
    int mapped_to = 0;
    CurveSettings axes_settings[7];
};

std::vector<JoystickData> CheckForJoysticks() {
    std::vector<JoystickData> buffer;

    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickPresent(jid)) {
            const char* joyName = glfwGetJoystickName(jid);
            std::string name = joyName ? joyName : "Unknown";

            if (name == "vJoy Device") continue;

            const char* guid = glfwGetJoystickGUID(jid);
            std::string GUID = guid ? guid : "Unknown";
            std::cout << "Found joystick: " << name << " " << GUID << std::endl;

            buffer.push_back({ name, jid, GUID });
        }
    }

    return buffer;
}

void EditAxis(float axis_value, JoystickData joystick, int axes_type) {
    CurveSettings settings = joystick.axes_settings[axes_type - 0x30];
    float value;
    switch (settings.curve_type)
    {
    case Linear:
        value = axis_value;
        break;
    case Cubic:
        value = (float)std::pow(axis_value, 3);
        break;
    default:
        std::cout << "invalid curve type" << std::endl;
        return;
    }

    value = (value + 1) / 2;
    if (settings.reversed)
        value = 1 - value;

    SetAxis(value * MAX_AXIS_VALUE, joystick.mapped_to, axes_type);
}

void IncrementSelector(int& value, const int min, const int max, std::vector<int> exclude = {}){
    ImGui::BeginTable("vJoy map selector", 3);

    ImGui::TableNextColumn();

    if (ImGui::Button("-")) {
        value--;
        if (value <= min)
            value = min;
        else {
            for (auto j : exclude) {
                if (j == value)
                    value--;
            }
        }
    }

    ImGui::TableNextColumn();

    ImGui::Text(std::to_string(value).c_str());

    ImGui::TableNextColumn();

    if (ImGui::Button("+")) {
        value++;
        if (value > max)
            value = max;

        for (auto j : exclude) {
            if (j == value)
                value++;
        }
    }

    ImGui::EndTable();
}

void save_file(const char* filename, std::vector<JoystickData> data) {
    std::ofstream file;
    file.open(filename);

    for (auto joystick : data) {
        if (!joystick.selected) continue;

        file << "[" << joystick.guid << "]\n";
        file << joystick.mapped_to << "\n";
        for (auto settings : joystick.axes_settings) {
            file << "{ " << settings.curve_type << "; " << settings.reversed << " }\n";
        }
    }

    file.close();
}

CurveSettings settings_string(std::string str) {
    std::string strip;
    for (char c : str) {
        if (c == '{' || c == '}' || c == ' ') continue;
        strip += c;
    }

    CurveSettings settings;
    if (strip.size() >= 3) {
        settings.curve_type = (int)(strip[0] - '0');
        settings.reversed = (bool)(strip[2] - '0');
    }

    return settings;
}

std::vector<JoystickData> load_file(const char* filename) {
    std::vector<JoystickData> buffer;

    std::ifstream file;
    file.open(filename);

    if (file.is_open()) {
        std::string line;
        int type = 0;
        int settings_num = 0;

        while (getline(file, line)) {
            if (line[0] == '[') {
                type = 0;
                settings_num = 0;
                JoystickData data;
                data.guid = line.substr(1, line.size()-2);
                buffer.push_back(data);
                type++;
            }
            else if (type == 1) {
                buffer.back().mapped_to = std::atoi(line.c_str());
                type++;
            } else if (type == 2) {
                buffer.back().axes_settings[settings_num] = CurveSettings(line);
                settings_num++;
            }
        }
        file.close();
    }

    return buffer;
}

int main() {
    if (!InitImGui()) return 1;

    if (!InitVJoy()) return 1;

    int vJoy_pads = 0;
    if (!GetvJoyMaxDevices(&vJoy_pads)) return 1;

    if (!AcquireVJoyPads(vJoy_pads)) return 1;

    std::vector<JoystickData> joysticks = CheckForJoysticks();

    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();

    ImFontConfig config;
    config.SizePixels = 20.0f;
    io.FontDefault = io.Fonts->AddFontDefault(&config);

    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Updating VJoy

        for (int index = joysticks.size() - 1; index >= 0; --index) {
            auto& joystick = joysticks[index];
            if (joystick.mapped_to == 0) continue;

            int axis_count;
            const float* axes = glfwGetJoystickAxes(joystick.jid, &axis_count);

            if (axis_count == 0) {
                joysticks.erase(joysticks.begin() + index);
                continue;
            }

            if (axis_count > 0)
                EditAxis(axes[0], joystick, HID_USAGE_X);
            if (axis_count > 1)
                EditAxis(axes[1], joystick, HID_USAGE_Y);
            if (axis_count > 2)
                EditAxis(axes[2], joystick, HID_USAGE_Z);
            if (axis_count > 3)
                EditAxis(axes[3], joystick, HID_USAGE_RX);
            if (axis_count > 4)
                EditAxis(axes[4], joystick, HID_USAGE_RY);
            if (axis_count > 5)
                EditAxis(axes[5], joystick, HID_USAGE_RZ);
            if (axis_count > 6)
                EditAxis(axes[6], joystick, HID_USAGE_SL0);
            if (axis_count > 7)
                EditAxis(axes[7], joystick, HID_USAGE_SL1);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Test");

        ImGui::BeginTable("Controls", 3);
        ImGui::TableNextColumn();

        if (ImGui::Button("Reload")) {
            auto new_buffer = CheckForJoysticks();
            for (auto n : new_buffer) {
                bool found = false;
                for (auto j : joysticks) {
                    if (n.guid == j.guid) {
                        found = true;
                        break;
                    }
                }

                if (!found)
                    joysticks.push_back(n);
            }
        }

        ImGui::TableNextColumn();
        if (ImGui::Button("Save")) {
            save_file("save.data", joysticks);
        }

        ImGui::TableNextColumn();
        if (ImGui::Button("Load")) {
            auto new_buffer = load_file("save.data");
            for (auto& j : joysticks) {
                for (auto n : new_buffer) {
                    if (j.guid == n.guid) {
                        j.mapped_to = n.mapped_to;
                        j.selected = true;
                        for (int i = 0; i < 7; ++i)
                            j.axes_settings[i] = n.axes_settings[i];
                    }
                }
            }
        }

        ImGui::EndTable();

        ImGui::BeginChild("Scrolling");
        for (auto& joystick : joysticks) {
            std::string label = std::to_string(joystick.jid) + " - " + joystick.name;
            if (ImGui::Button(label.c_str())) {
                joystick.selected = !joystick.selected;
                if (!joystick.selected)
                    joystick.mapped_to = 0;
            }
        }
        ImGui::EndChild();

        ImGui::End();

        for (auto& joystick : joysticks) {
            if (!joystick.selected) continue;

            std::string label = std::to_string(joystick.jid) + " - " + joystick.name;
            ImGui::Begin(label.c_str());

            ImGui::Text(joystick.name.c_str());

            std::vector<int> exclude;
            for (auto j : joysticks) {
                if (!j.selected) continue;
                if (j.jid == joystick.jid) continue;

                exclude.push_back(j.mapped_to);
            }
            IncrementSelector(joystick.mapped_to, 0, vJoy_pads, exclude);

            ImGui::BeginChild("Scrolling");

            int axis_count;
            const float* axes = glfwGetJoystickAxes(joystick.jid, &axis_count);
            for (int i = 0; i < min(7, axis_count); ++i) {
                ImGui::BeginTable(std::to_string(i).c_str(), 3);
                ImGui::TableNextColumn();

                ImGui::Text("Axis: %i", i);

                ImGui::TableNextColumn();

                IncrementSelector(joystick.axes_settings[i].curve_type, 0, 1);

                ImGui::TableNextColumn();

                ImGui::Checkbox("Reverse", &joystick.axes_settings[i].reversed);

                ImGui::EndTable();
            }

            ImGui::EndChild();

            ImGui::End();
        }

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    FreeVJoyPads(vJoy_pads);

    return 0;
}