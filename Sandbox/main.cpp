#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Application.h"
#include "Core/Camera.h"
#include "Core/Color.h"
#include "Core/Entity.h"

#include "Platform/Win32/Window.h"
#include "Platform/Win32/Input.h"

#include "Renderer/Win32/GdiRenderer.h"

#include "Sandbox/UI/Rendering/UIDrawList.h"
#include "Sandbox/UI/Rendering/Win32/GdiUIDrawBackend.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

class SandboxApplication final : public Core::Application
{
public:
    SandboxApplication(
        Core::IWindow& window,
        Core::IRenderer& renderer,
        Kjarn::UI::IUIDrawBackend& uiBackend
    )
        : Core::Application(window, renderer)
        , m_uiBackend(uiBackend)
    {
        CreateTestScene();
    }

protected:
    void OnUpdate(float deltaTime) override
    {
        // Select an entity from Scene Hierarchy.
        HandleHierarchyClick();
        HandleViewportClick();
        // Camera movement.
        constexpr float cameraSpeed = 500.0f;

        if (Core::Input::IsKeyDown('W'))
            m_camera.y -= cameraSpeed * deltaTime / m_camera.zoom;

        if (Core::Input::IsKeyDown('S'))
            m_camera.y += cameraSpeed * deltaTime / m_camera.zoom;

        if (Core::Input::IsKeyDown('A'))
            m_camera.x -= cameraSpeed * deltaTime / m_camera.zoom;

        if (Core::Input::IsKeyDown('D'))
            m_camera.x += cameraSpeed * deltaTime / m_camera.zoom;

        // Camera zoom.
        const int wheel = Core::Input::GetMouseWheelDelta();

        if (wheel > 0)
            m_camera.zoom *= 1.1f;

        if (wheel < 0)
            m_camera.zoom /= 1.1f;

        m_camera.zoom = std::clamp(
            m_camera.zoom,
            0.1f,
            10.0f
        );
    }

    void OnRender() override
    {
        auto& renderer = GetRenderer();

        // 1. Render scene entities.
        for (const auto& entity : m_scene)
        {
            renderer.DrawRect(
                m_camera,
                entity.x,
                entity.y,
                entity.width,
                entity.height,
                entity.color
            );
        }

        // 2. Highlight selected entity.
        if (m_selectedEntityIndex >= 0 &&
            m_selectedEntityIndex < static_cast<int>(m_scene.size()))
        {
            const Core::Entity& selected =
                m_scene[static_cast<std::size_t>(m_selectedEntityIndex)];

            renderer.DrawRect(
                m_camera,
                selected.x,
                selected.y,
                selected.width,
                selected.height,
                Core::Color(85, 145, 220)
            );
        }

        // 3. Build and render editor UI.
        BuildEditorUI();
        m_uiBackend.Submit(m_uiDrawList);
    }

private:
    static constexpr float TopBarHeight = 36.0f;
    static constexpr float BottomBarHeight = 24.0f;
    static constexpr float HierarchyRowHeight = 22.0f;

    float GetLeftPanelWidth() const
    {
        const float width = static_cast<float>(
            GetRenderer().GetWidth()
            );

        return std::min(220.0f, width * 0.25f);
    }

    float GetHierarchyStartY() const
    {
        return TopBarHeight + 32.0f + 8.0f;
    }

    std::size_t GetVisibleHierarchyCount() const
    {
        const float height = static_cast<float>(
            GetRenderer().GetHeight()
            );

        const float availableHeight =
            height - BottomBarHeight - GetHierarchyStartY();

        if (availableHeight <= 0.0f)
            return 0;

        const std::size_t maxVisible = static_cast<std::size_t>(
            availableHeight / HierarchyRowHeight
            );

        return std::min(
            maxVisible,
            m_hierarchyEntityIndices.size()
        );
    }

    void HandleHierarchyClick()
    {
        if (!Core::Input::IsLeftMouseButtonPressed())
            return;

        float mouseX = 0.0f;
        float mouseY = 0.0f;

        Core::Input::GetMousePosition(mouseX, mouseY);

        // Ignore clicks outside the left panel.
        if (mouseX < 0.0f || mouseX >= GetLeftPanelWidth())
            return;

        const float startY = GetHierarchyStartY();

        if (mouseY < startY)
            return;

        const std::size_t rowIndex = static_cast<std::size_t>(
            (mouseY - startY) / HierarchyRowHeight
            );

        if (rowIndex >= GetVisibleHierarchyCount())
            return;

        m_selectedEntityIndex = static_cast<int>(
            m_hierarchyEntityIndices[rowIndex]
            );
    }

    void HandleViewportClick()
    {
        if (!Core::Input::IsLeftMouseButtonPressed())
            return;

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        Core::Input::GetMousePosition(mouseX, mouseY);

        // Получаем размеры панелей (те же, что в BuildEditorUI)
        const float width = static_cast<float>(GetRenderer().GetWidth());
        const float height = static_cast<float>(GetRenderer().GetHeight());
        const float leftPanelWidth = GetLeftPanelWidth();
        const float rightPanelWidth = std::min(280.0f, width * 0.30f);
        const float rightPanelX = std::max(leftPanelWidth, width - rightPanelWidth);

        // Если клик был по UI (верхнее меню, левая или правая панель, нижний бар) - игнорируем
        if (mouseY < TopBarHeight ||
            mouseY > height - BottomBarHeight ||
            mouseX < leftPanelWidth ||
            mouseX > rightPanelX)
        {
            return;
        }

        // Клик был в центре (Viewport). Переводим координаты.
        float worldX, worldY;
        m_camera.ScreenToWorld(mouseX, mouseY, width, height, worldX, worldY);

        // Ищем объект под курсором (с конца, так как последние рисуются сверху)
        m_selectedEntityIndex = -1; // Сбрасываем
        for (int i = static_cast<int>(m_scene.size()) - 1; i >= 0; --i)
        {
            if (m_scene[i].ContainsPoint(worldX, worldY))
            {
                m_selectedEntityIndex = i;
                break;
            }
        }
    }

    void CreateTestScene()
    {
        constexpr int gridSize = 50;
        constexpr int gridWidth = gridSize * 2 + 1;

        for (int i = -gridSize; i <= gridSize; ++i)
        {
            for (int j = -gridSize; j <= gridSize; ++j)
            {
                const Core::Color color =
                    (i + j) % 2 == 0
                    ? Core::Color(80, 80, 80)
                    : Core::Color(100, 100, 100);

                m_scene.emplace_back(
                    i * 100.0f,
                    j * 100.0f,
                    90.0f,
                    90.0f,
                    color
                );
            }
        }

        // Add 25 cells around the center of the scene
        // to the visible hierarchy list.
        for (int i = -2; i <= 2; ++i)
        {
            for (int j = -2; j <= 2; ++j)
            {
                const std::size_t index =
                    static_cast<std::size_t>(
                        (i + gridSize) * gridWidth +
                        (j + gridSize)
                        );

                m_hierarchyEntityIndices.push_back(index);
            }
        }
    }

    void BuildEditorUI()
    {
        m_uiDrawList.Clear();

        const float width = static_cast<float>(
            GetRenderer().GetWidth()
            );

        const float height = static_cast<float>(
            GetRenderer().GetHeight()
            );

        const float leftPanelWidth = GetLeftPanelWidth();

        const float rightPanelWidth = std::min(
            280.0f,
            width * 0.30f
        );

        const float rightPanelX = std::max(
            leftPanelWidth,
            width - rightPanelWidth
        );

        // Colors.
        const Kjarn::UI::UIColor panel{
            38, 41, 48, 255
        };

        const Kjarn::UI::UIColor panelHeader{
            46, 49, 57, 255
        };

        const Kjarn::UI::UIColor border{
            65, 69, 78, 255
        };

        const Kjarn::UI::UIColor text{
            220, 223, 230, 255
        };

        const Kjarn::UI::UIColor secondaryText{
            145, 151, 163, 255
        };

        const Kjarn::UI::UIColor accent{
            85, 145, 220, 255
        };

        const Kjarn::UI::UIColor selectedRow{
            55, 80, 115, 255
        };

        // Top menu bar.
        m_uiDrawList.AddFillRect(
            { 0.0f, 0.0f, width, TopBarHeight },
            panelHeader
        );

        m_uiDrawList.AddText(
            { 14.0f, 9.0f },
            "KJARN",
            accent,
            17.0f
        );

        m_uiDrawList.AddText(
            { 100.0f, 11.0f },
            "File    Edit    View    Project    Build    Help",
            text,
            13.0f
        );

        m_uiDrawList.AddLine(
            { 0.0f, TopBarHeight },
            { width, TopBarHeight },
            border
        );

        // Left panel background and header.
        m_uiDrawList.AddFillRect(
            {
                0.0f,
                TopBarHeight,
                leftPanelWidth,
                height - TopBarHeight - BottomBarHeight
            },
            panel
        );

        m_uiDrawList.AddFillRect(
            {
                0.0f,
                TopBarHeight,
                leftPanelWidth,
                32.0f
            },
            panelHeader
        );

        m_uiDrawList.AddText(
            { 12.0f, TopBarHeight + 8.0f },
            "SCENE HIERARCHY",
            text,
            12.0f
        );

        m_uiDrawList.AddLine(
            { leftPanelWidth, TopBarHeight },
            { leftPanelWidth, height - BottomBarHeight },
            border
        );

        // Hierarchy root.
        m_uiDrawList.AddText(
            { 14.0f, TopBarHeight + 40.0f },
            "Scene",
            text,
            13.0f
        );

        // Entity rows.
        const std::size_t visibleCount =
            GetVisibleHierarchyCount();

        for (std::size_t i = 0; i < visibleCount; ++i)
        {
            const std::size_t entityIndex =
                m_hierarchyEntityIndices[i];

            const Core::Entity& entity =
                m_scene[entityIndex];

            const float rowY =
                GetHierarchyStartY() +
                static_cast<float>(i) * HierarchyRowHeight;

            // Highlight selected row.
            if (static_cast<int>(entityIndex) ==
                m_selectedEntityIndex)
            {
                m_uiDrawList.AddFillRect(
                    {
                        0.0f,
                        rowY,
                        leftPanelWidth,
                        HierarchyRowHeight
                    },
                    selectedRow
                );
            }

            const int gridX = static_cast<int>(entity.x / 100.0f);
            const int gridY = static_cast<int>(entity.y / 100.0f);

            const std::string label =
                "Cell (" +
                std::to_string(gridX) +
                ", " +
                std::to_string(gridY) +
                ")";

            m_uiDrawList.AddText(
                { 28.0f, rowY + 4.0f },
                label,
                static_cast<int>(entityIndex) == m_selectedEntityIndex
                ? text
                : secondaryText,
                12.0f
            );
        }

        // Right Inspector panel.
        m_uiDrawList.AddFillRect(
            {
                rightPanelX,
                TopBarHeight,
                width - rightPanelX,
                height - TopBarHeight - BottomBarHeight
            },
            panel
        );

        m_uiDrawList.AddFillRect(
            {
                rightPanelX,
                TopBarHeight,
                width - rightPanelX,
                32.0f
            },
            panelHeader
        );

        m_uiDrawList.AddText(
            { rightPanelX + 12.0f, TopBarHeight + 8.0f },
            "INSPECTOR",
            text,
            12.0f
        );

        m_uiDrawList.AddLine(
            { rightPanelX, TopBarHeight },
            { rightPanelX, height - BottomBarHeight },
            border
        );

        // Inspector contents.
        if (m_selectedEntityIndex >= 0 &&
            m_selectedEntityIndex < static_cast<int>(m_scene.size()))
        {
            const Core::Entity& selected =
                m_scene[static_cast<std::size_t>(m_selectedEntityIndex)];

            const float inspectorX = rightPanelX + 12.0f;

            const int gridX = static_cast<int>(selected.x / 100.0f);
            const int gridY = static_cast<int>(selected.y / 100.0f);

            m_uiDrawList.AddText(
                { inspectorX, 80.0f },
                "Cell (" +
                std::to_string(gridX) +
                ", " +
                std::to_string(gridY) +
                ")",
                accent,
                14.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 112.0f },
                "Transform",
                text,
                13.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 140.0f },
                "Position X: " + std::to_string(selected.x),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 162.0f },
                "Position Y: " + std::to_string(selected.y),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 184.0f },
                "Width: " + std::to_string(selected.width),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 206.0f },
                "Height: " + std::to_string(selected.height),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 244.0f },
                "Color",
                text,
                13.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 272.0f },
                "R: " + std::to_string(
                    static_cast<int>(selected.color.r)
                ),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 294.0f },
                "G: " + std::to_string(
                    static_cast<int>(selected.color.g)
                ),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 316.0f },
                "B: " + std::to_string(
                    static_cast<int>(selected.color.b)
                ),
                secondaryText,
                12.0f
            );
        }
        else
        {
            m_uiDrawList.AddText(
                { rightPanelX + 12.0f, 80.0f },
                "No object selected",
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { rightPanelX + 12.0f, 106.0f },
                "Select a cell in the hierarchy.",
                secondaryText,
                11.0f
            );
        }

        // Bottom status bar.
        m_uiDrawList.AddFillRect(
            {
                0.0f,
                height - BottomBarHeight,
                width,
                BottomBarHeight
            },
            panelHeader
        );

        m_uiDrawList.AddLine(
            { 0.0f, height - BottomBarHeight },
            { width, height - BottomBarHeight },
            border
        );

        m_uiDrawList.AddText(
            { 12.0f, height - 18.0f },
            m_selectedEntityIndex >= 0
            ? "OBJECT SELECTED"
            : "READY",
            accent,
            11.0f
        );

        m_uiDrawList.AddText(
            { width - 150.0f, height - 18.0f },
            "Kjarn Sandbox",
            secondaryText,
            11.0f
        );
    }

    Core::Camera m_camera;
    std::vector<Core::Entity> m_scene;

    // Entity indices displayed in Scene Hierarchy.
    std::vector<std::size_t> m_hierarchyEntityIndices;

    // -1 means that no entity is selected.
    int m_selectedEntityIndex = -1;

    Kjarn::UI::UIDrawList m_uiDrawList;
    Kjarn::UI::IUIDrawBackend& m_uiBackend;
};

int main()
{
    Core::Window window(
        L"Kjarn Sandbox",
        1280,
        800
    );

    Renderer::Win32::GdiRenderer renderer(
        window.GetNativeHandle()
    );

    Kjarn::UI::Win32::GdiUIDrawBackend uiBackend(renderer);

    SandboxApplication application(
        window,
        renderer,
        uiBackend
    );

    application.Run();

    return 0;
}