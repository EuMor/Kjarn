#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Application.h"
#include "Core/Camera.h"
#include "Core/Color.h"
#include "Core/Scene/Scene.h"

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

        // Render scene entities.
        for (const auto& entity : m_scene.GetEntities())
        {
            renderer.DrawRect(
                m_camera,
                entity.transform.position.x,
                entity.transform.position.y,
                entity.width,
                entity.height,
                entity.color
            );
        }

        // Highlight selected entity.
        const Core::Entity* selected =
            m_scene.FindEntity(m_selectedEntityId);

        if (selected != nullptr)
        {
            renderer.DrawRect(
                m_camera,
                selected->transform.position.x,
                selected->transform.position.y,
                selected->width,
                selected->height,
                Core::Color(85, 145, 220)
            );
        }

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
            m_hierarchyEntityIds.size()
        );
    }

    void HandleHierarchyClick()
    {
        if (!Core::Input::IsLeftMouseButtonPressed())
            return;

        float mouseX = 0.0f;
        float mouseY = 0.0f;

        Core::Input::GetMousePosition(mouseX, mouseY);

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

        m_selectedEntityId = m_hierarchyEntityIds[rowIndex];
    }

    void HandleViewportClick()
    {
        if (!Core::Input::IsLeftMouseButtonPressed())
            return;

        float mouseX = 0.0f;
        float mouseY = 0.0f;

        Core::Input::GetMousePosition(mouseX, mouseY);

        const float width =
            static_cast<float>(GetRenderer().GetWidth());

        const float height =
            static_cast<float>(GetRenderer().GetHeight());

        const float leftPanelWidth = GetLeftPanelWidth();

        const float rightPanelWidth =
            std::min(280.0f, width * 0.30f);

        const float rightPanelX =
            std::max(leftPanelWidth, width - rightPanelWidth);

        if (mouseY < TopBarHeight ||
            mouseY > height - BottomBarHeight ||
            mouseX < leftPanelWidth ||
            mouseX > rightPanelX)
        {
            return;
        }

        float worldX = 0.0f;
        float worldY = 0.0f;

        m_camera.ScreenToWorld(
            mouseX, mouseY,
            width, height,
            worldX, worldY
        );

        m_selectedEntityId = {};

        const auto& entities = m_scene.GetEntities();

        for (auto it = entities.rbegin(); it != entities.rend(); ++it)
        {
            if (it->ContainsPoint(worldX, worldY))
            {
                m_selectedEntityId = it->id;
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

                const Core::EntityId id = m_scene.CreateEntity();

                Core::Entity* entity = m_scene.FindEntity(id);

                if (entity == nullptr)
                    continue;

                entity->transform.position.x = i * 100.0f;
                entity->transform.position.y = j * 100.0f;

                entity->width = 90.0f;
                entity->height = 90.0f;
                entity->color = color;
            }
        }

        // Add 25 cells around the center to the hierarchy.
        for (int i = -2; i <= 2; ++i)
        {
            for (int j = -2; j <= 2; ++j)
            {
                const std::size_t index =
                    static_cast<std::size_t>(
                        (i + gridSize) * gridWidth +
                        (j + gridSize)
                        );

                const auto& entities = m_scene.GetEntities();

                if (index < entities.size())
                {
                    m_hierarchyEntityIds.push_back(
                        entities[index].id
                    );
                }
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
            const Core::EntityId entityId =
                m_hierarchyEntityIds[i];

            const Core::Entity* entity =
                m_scene.FindEntity(entityId);

            if (entity == nullptr)
                continue;

            const float rowY =
                GetHierarchyStartY() +
                static_cast<float>(i) * HierarchyRowHeight;

            // Highlight selected row.
            if (entityId == m_selectedEntityId)
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

            const int gridX = static_cast<int>(
                entity->transform.position.x / 100.0f
                );

            const int gridY = static_cast<int>(
                entity->transform.position.y / 100.0f
                );

            const std::string label =
                "Cell (" +
                std::to_string(gridX) +
                ", " +
                std::to_string(gridY) +
                ")";

            m_uiDrawList.AddText(
                { 28.0f, rowY + 4.0f },
                label,
                entityId == m_selectedEntityId
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
        const Core::Entity* selected =
            m_scene.FindEntity(m_selectedEntityId);

        if (selected != nullptr)
        {
            const float inspectorX = rightPanelX + 12.0f;

            const int gridX = static_cast<int>(
                selected->transform.position.x / 100.0f
                );

            const int gridY = static_cast<int>(
                selected->transform.position.y / 100.0f
                );

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
                "Position X: " + std::to_string(selected->transform.position.x),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 162.0f },
                "Position Y: " + std::to_string(selected->transform.position.y),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 184.0f },
                "Width: " + std::to_string(selected->width),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 206.0f },
                "Height: " + std::to_string(selected->height),
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
                    static_cast<int>(selected->color.r)
                ),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 294.0f },
                "G: " + std::to_string(
                    static_cast<int>(selected->color.g)
                ),
                secondaryText,
                12.0f
            );

            m_uiDrawList.AddText(
                { inspectorX, 316.0f },
                "B: " + std::to_string(
                    static_cast<int>(selected->color.b)
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
            m_selectedEntityId.IsValid()
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
    Core::Scene m_scene;

    // IDs of entities displayed in Scene Hierarchy.
    std::vector<Core::EntityId> m_hierarchyEntityIds;

    // Invalid ID means that no entity is selected.
    Core::EntityId m_selectedEntityId{};

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