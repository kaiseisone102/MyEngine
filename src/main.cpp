
#include <cstdlib>
#include <exception>
#include <iostream>

#include "app/engine_app.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

int main() {
    try {
        EngineApp app;
        app.run();
    } catch (const std::exception& e) {
#ifdef _DEBUG
        std::cerr << "[Fatal] " << e.what() << '\n';
#endif
        MessageBoxA(nullptr, e.what(), "MyEngine - Fatal Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
