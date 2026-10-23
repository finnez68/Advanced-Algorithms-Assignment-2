// main.cpp -- GLFW + OpenGL3 + Dear ImGui boilerplate. Opens the window and
// calls App::Draw() every frame; all the UI itself lives in app.cpp.

#include "app.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdio>

static void GlfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

int main() {
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS

    GLFWwindow* window = glfwCreateWindow(800, 600, "isPrime", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // no imgui.ini clutter; our settings have their own file
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    App app;
    float contentScaleX = 1.0f, contentScaleY = 1.0f;
    glfwGetWindowContentScale(window, &contentScaleX, &contentScaleY);
    app.ApplySuggestedScaleIfUnset(contentScaleX); // e.g. 2.0 on a HiDPI laptop, first run only

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Re-bake the font at the exact pixel size for the current UI scale.
        // Has to happen between frames; the OpenGL backend re-uploads the
        // atlas in NewFrame once the old texture is destroyed.
        float fontPx;
        if (app.ConsumeFontRebuild(fontPx)) {
            ImGuiIO& io = ImGui::GetIO();
            io.Fonts->Clear();
            ImFontConfig cfg;
            cfg.SizePixels = fontPx;
            cfg.OversampleH = cfg.OversampleV = 1;
            cfg.PixelSnapH = true;
            io.Fonts->AddFontDefault(&cfg);
            ImGui_ImplOpenGL3_DestroyFontsTexture();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        app.Draw();
        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
