#include "Core/Window.h"

int main() {
    Core::Window window(L"My Custom Engine - Test Window", 800, 600);
    window.Run();
    return 0;
}