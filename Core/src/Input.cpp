#include "Core/Input.h"
#include <cstring>
#include <windowsx.h> // Нужен для GET_X_LPARAM

namespace Core {

    bool Input::s_Keys[256] = {};
    bool Input::s_PrevKeys[256] = {};
    float Input::s_MouseX = 0.0f;
    float Input::s_MouseY = 0.0f;
    
    namespace {
        int s_MouseWheelDelta = 0;
    }

    void Input::Update() {
        // Копируем текущее состояние в предыдущее
        std::memcpy(s_PrevKeys, s_Keys, sizeof(s_Keys));
        s_MouseWheelDelta = 0;// Сбрасываем колесико в конце кадра
    }

    int Input::GetMouseWheelDelta() { 
        return s_MouseWheelDelta; 
    }

    bool Input::IsKeyDown(int key) {
        if (key < 0 || key > 255) return false;
        return s_Keys[key];
    }

    bool Input::IsKeyPressed(int key) {
        if (key < 0 || key > 255) return false;
        return s_Keys[key] && !s_PrevKeys[key];
    }

    void Input::GetMousePosition(float& x, float& y) {
        x = s_MouseX;
        y = s_MouseY;
    }

    void Input::ProcessMessage(
        UINT msg,
        WPARAM wParam,
        LPARAM lParam
    ) {
        switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const auto key = static_cast<unsigned int>(wParam);

            if (key < 256) {
                s_Keys[key] = true;
            }
            break;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            const auto key = static_cast<unsigned int>(wParam);

            if (key < 256) {
                s_Keys[key] = false;
            }
            break;
        }

        case WM_KILLFOCUS:
            std::memset(s_Keys, 0, sizeof(s_Keys));
            break;

        case WM_MOUSEMOVE:
            s_MouseX = static_cast<float>(
                GET_X_LPARAM(lParam)
                );

            s_MouseY = static_cast<float>(
                GET_Y_LPARAM(lParam)
                );
            break;

        case WM_MOUSEWHEEL:
            s_MouseWheelDelta +=
                GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            break;
        }
    }

} // namespace Core