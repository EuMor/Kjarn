#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Application.h"
#include "Core/Camera.h"
#include "Core/Color.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/SceneSerializer.h"

#include "Platform/Win32/Window.h"
#include "Platform/Win32/Input.h"

#include "Renderer/Win32/GdiRenderer.h"

#include "Sandbox/UI/Rendering/UIDrawList.h"
#include "Sandbox/UI/Rendering/Win32/GdiUIDrawBackend.h"

#include "Core/Physics.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>

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
        // При запуске показываем меню создания сетки
        m_showStartupMenu = true;
    }

protected:
    void OnUpdate(float deltaTime) override
    {
        if (m_showStartupMenu)
        {
            HandleStartupMenu();
            return;
        }

        // Если уже находимся в игровом режиме.
        if (m_isPlaying)
        {
            HandlePlayModeInput();

            if (m_isPlaying)
                UpdateRuntime(deltaTime);

            return;
        }

        // Обрабатываем редакторский интерфейс.
        HandleUIClicks();

        // Возможно, пользователь только что нажал PLAY.
        if (m_isPlaying)
        {
            UpdateRuntime(deltaTime);
            return;
        }

        // Далее работает только редактор.
        HandleViewportMouse();

        if (!m_selectedEntityIds.empty() &&
            Core::Input::IsKeyPressed(VK_DELETE))
        {
            for (auto id : m_selectedEntityIds)
                m_scene.DestroyEntity(id);

            m_selectedEntityIds.clear();
        }

        constexpr float cameraSpeed = 500.0f;

        if (Core::Input::IsKeyDown('W'))
            m_camera.y -= cameraSpeed * deltaTime / m_camera.zoom;

        if (Core::Input::IsKeyDown('S'))
            m_camera.y += cameraSpeed * deltaTime / m_camera.zoom;

        if (Core::Input::IsKeyDown('A'))
            m_camera.x -= cameraSpeed * deltaTime / m_camera.zoom;

        if (Core::Input::IsKeyDown('D'))
            m_camera.x += cameraSpeed * deltaTime / m_camera.zoom;

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

        if (!m_showStartupMenu)
        {
            // 1. Отрисовка сетки
            const Core::Color gridColor(60, 60, 60);
            float halfW = (renderer.GetWidth() / 2.0f) / m_camera.zoom;
            float halfH = (renderer.GetHeight() / 2.0f) / m_camera.zoom;
            float left = m_camera.x - halfW;
            float right = m_camera.x + halfW;
            float top = m_camera.y - halfH;
            float bottom = m_camera.y + halfH;

            int startX = (int)std::floor(left / CellSize) * (int)CellSize;
            int endX = (int)std::ceil(right / CellSize) * (int)CellSize;
            int startY = (int)std::floor(top / CellSize) * (int)CellSize;
            int endY = (int)std::ceil(bottom / CellSize) * (int)CellSize;

            for (int x = startX; x <= endX; x += (int)CellSize) renderer.DrawLine(m_camera, (float)x, top, (float)x, bottom, gridColor);
            for (int y = startY; y <= endY; y += (int)CellSize) renderer.DrawLine(m_camera, left, (float)y, right, (float)y, gridColor);

            // 2. Объекты
            const Core::Scene& activeScene = GetActiveScene();

            for (const auto& entity : activeScene.GetEntities())
            {
                if (entity.shape == Core::ShapeType::Circle) {
                    renderer.DrawCircle(m_camera, entity.transform.position.x, entity.transform.position.y, entity.width, entity.height, entity.color);
                }
                else if (entity.shape == Core::ShapeType::Triangle) {
                    renderer.DrawTriangle(m_camera, entity.transform.position.x, entity.transform.position.y, entity.width, entity.height, entity.color);
                }
                else {
                    renderer.DrawRect(m_camera, entity.transform.position.x, entity.transform.position.y, entity.width, entity.height, entity.color);
                }
            }
            // 3. Подсветка
                        // 3. Подсветка
            for (Core::EntityId id : m_selectedEntityIds)
            {
                const Core::Entity* selected = m_scene.FindEntity(id);
                if (selected != nullptr) {
                    float x = selected->transform.position.x;
                    float y = selected->transform.position.y;
                    float w = selected->width;
                    float h = selected->height;
                    Core::Color selColor(85, 145, 220); // Синий цвет
                    float t = 2.0f / m_camera.zoom; // Толщина рамки 2 пикселя на экране

                    // Рисуем рамку (4 прямоугольника по краям)
                    renderer.DrawRect(m_camera, x, y, w, t, selColor); // Верх
                    renderer.DrawRect(m_camera, x, y + h - t, w, t, selColor); // Низ
                    renderer.DrawRect(m_camera, x, y, t, h, selColor); // Лево
                    renderer.DrawRect(m_camera, x + w - t, y, t, h, selColor); // Право
                }
            }

            // 4. Рамка выделения (теперь в МИРОВЫХ координатах)
            if (m_isMarqueeSelecting)
            {
                float minX = std::min(m_marqueeStartWorldX, m_marqueeCurrentWorldX);
                float maxX = std::max(m_marqueeStartWorldX, m_marqueeCurrentWorldX);
                float minY = std::min(m_marqueeStartWorldY, m_marqueeCurrentWorldY);
                float maxY = std::max(m_marqueeStartWorldY, m_marqueeCurrentWorldY);

                Core::Color marqueeColor(85, 145, 220);
                float t = 2.0f / m_camera.zoom; // Толщина рамки 2 пикселя на экране

                renderer.DrawRect(m_camera, minX, minY, maxX - minX, t, marqueeColor); // Верх
                renderer.DrawRect(m_camera, minX, maxY - t, maxX - minX, t, marqueeColor); // Низ
                renderer.DrawRect(m_camera, minX, minY, t, maxY - minY, marqueeColor); // Лево
                renderer.DrawRect(m_camera, maxX - t, minY, t, maxY - minY, marqueeColor); // Право
            }
        }

        BuildEditorUI();
        m_uiBackend.Submit(m_uiDrawList);
    }

private:
    static constexpr float TopBarHeight = 36.0f;
    static constexpr float BottomBarHeight = 24.0f;
    static constexpr float HierarchyRowHeight = 22.0f;
    static constexpr float CellSize = 100.0f;

    int m_gridRadius = 10;

    // Состояния меню создания сетки
    bool m_showStartupMenu = true;
    int m_inputRadius = 10; // То, что крутим в меню

    bool m_isDragging = false;
    float m_dragStartWorldX = 0.0f, m_dragStartWorldY = 0.0f;
    struct DragState { Core::EntityId id; float startX; float startY; };
    std::vector<DragState> m_dragStartPositions;

    bool m_isMarqueeSelecting = false;
    float m_marqueeStartWorldX = 0.0f, m_marqueeStartWorldY = 0.0f;
    float m_marqueeCurrentWorldX = 0.0f, m_marqueeCurrentWorldY = 0.0f;

    bool m_isPlaying = false;
    float m_gravity = 1500.0f;

    float GetLeftPanelWidth() const { return std::min(220.0f, GetRenderer().GetWidth() * 0.25f); }
    float GetHierarchyStartY() const { return TopBarHeight + 32.0f + 8.0f; }

    Core::ShapeType m_currentShapeType = Core::ShapeType::Rectangle;

    Core::Scene& GetActiveScene()
    {
        return m_isPlaying ? m_runtimeScene : m_scene;
    }

    const Core::Scene& GetActiveScene() const
    {
        return m_isPlaying ? m_runtimeScene : m_scene;
    }

    std::size_t GetVisibleHierarchyCount() const
    {
        const float availableHeight = GetRenderer().GetHeight() - BottomBarHeight - GetHierarchyStartY();
        if (availableHeight <= 0.0f) return 0;
        return std::min((std::size_t)(availableHeight / HierarchyRowHeight), m_hierarchyEntityIds.size());
    }

    bool IsMouseOverViewport(float mouseX, float mouseY) const
    {
        const float width = (float)GetRenderer().GetWidth();
        const float height = (float)GetRenderer().GetHeight();
        const float leftPanelWidth = GetLeftPanelWidth();
        const float rightPanelWidth = std::min(280.0f, width * 0.30f);
        const float rightPanelX = std::max(leftPanelWidth, width - rightPanelWidth);
        return (mouseY >= TopBarHeight && mouseY <= height - BottomBarHeight && mouseX >= leftPanelWidth && mouseX <= rightPanelX);
    }

    void SelectEntity(Core::EntityId id, bool additive)
    {
        if (!additive) m_selectedEntityIds.clear();
        auto it = std::find(m_selectedEntityIds.begin(), m_selectedEntityIds.end(), id);
        if (it != m_selectedEntityIds.end()) {
            if (additive) m_selectedEntityIds.erase(it);
        }
        else {
            m_selectedEntityIds.push_back(id);
        }
    }

    // --- ЛОГИКА СТАРТОВОГО МЕНЮ ---
    void HandleStartupMenu()
    {
        if (!Core::Input::IsLeftMouseButtonPressed()) return;

        float mx, my;
        Core::Input::GetMousePosition(mx, my);
        const float width = (float)GetRenderer().GetWidth();
        const float height = (float)GetRenderer().GetHeight();
        float cx = width / 2.0f;
        float cy = height / 2.0f;

        // Кнопка "-"
        if (mx >= cx - 80 && mx <= cx - 40 && my >= cy - 20 && my <= cy + 20) {
            if (m_inputRadius > 1) m_inputRadius--;
        }
        // Кнопка "+"
        if (mx >= cx + 40 && mx <= cx + 80 && my >= cy - 20 && my <= cy + 20) {
            if (m_inputRadius < 50) m_inputRadius++;
        }
        // Кнопка "Создать"
        if (mx >= cx - 80 && mx <= cx + 80 && my >= cy + 50 && my <= cy + 90) {
            m_gridRadius = m_inputRadius;
            m_scene.Clear(); // Очищаем сцену
            RegenerateScene();
            m_showStartupMenu = false; // Закрываем меню
        }
    }

    // --- ЛОГИКА МЫШИ ВО ВЬЮПОРТЕ ---
    void HandleViewportMouse()
    {
        if (m_isPlaying) return;
        float mouseX, mouseY;
        Core::Input::GetMousePosition(mouseX, mouseY);
        const float w = (float)GetRenderer().GetWidth();
        const float h = (float)GetRenderer().GetHeight();

        if (Core::Input::IsLeftMouseButtonPressed())
        {
            // Если клик был по UI (левая панель, правая панель, верх, низ) — ничего не делаем
            if (!IsMouseOverViewport(mouseX, mouseY))
                return;

            float worldX, worldY;
            m_camera.ScreenToWorld(mouseX, mouseY, w, h, worldX, worldY);

            bool isAdditive = Core::Input::IsKeyDown(VK_CONTROL) || Core::Input::IsKeyDown(VK_SHIFT);

            Core::EntityId clickedId = {};
            bool clickedOnObject = false;
            const auto& entities = m_scene.GetEntities();
            for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
                if (it->ContainsPoint(worldX, worldY)) {
                    clickedId = it->id;
                    clickedOnObject = true;
                    break;
                }
            }

            if (clickedOnObject)
            {
                bool alreadySelected = std::find(m_selectedEntityIds.begin(), m_selectedEntityIds.end(), clickedId) != m_selectedEntityIds.end();

                if (isAdditive) {
                    if (alreadySelected) {
                        m_selectedEntityIds.erase(std::find(m_selectedEntityIds.begin(), m_selectedEntityIds.end(), clickedId));
                    }
                    else {
                        m_selectedEntityIds.push_back(clickedId);
                    }
                }
                else {
                    if (!alreadySelected) {
                        m_selectedEntityIds.clear();
                        m_selectedEntityIds.push_back(clickedId);
                    }
                }

                m_isDragging = true;
                m_dragStartWorldX = worldX;
                m_dragStartWorldY = worldY;
                m_dragStartPositions.clear();
                for (auto id : m_selectedEntityIds) {
                    auto* ent = m_scene.FindEntity(id);
                    if (ent) m_dragStartPositions.push_back({ id, ent->transform.position.x, ent->transform.position.y });
                }
            }
            else
            {
                if (!isAdditive) m_selectedEntityIds.clear();
                m_isMarqueeSelecting = true;
                m_marqueeStartWorldX = worldX;
                m_marqueeStartWorldY = worldY;
                m_marqueeCurrentWorldX = worldX;
                m_marqueeCurrentWorldY = worldY;
            }
        }

        if (Core::Input::IsKeyDown(VK_LBUTTON))
        {
            if (m_isDragging)
            {
                float worldX, worldY;
                m_camera.ScreenToWorld(mouseX, mouseY, w, h, worldX, worldY);

                float deltaX = worldX - m_dragStartWorldX;
                float deltaY = worldY - m_dragStartWorldY;

                if (!Core::Input::IsKeyDown(VK_CONTROL)) {
                    deltaX = round(deltaX / CellSize) * CellSize;
                    deltaY = round(deltaY / CellSize) * CellSize;
                }

                for (auto& dragState : m_dragStartPositions) {
                    auto* ent = m_scene.FindEntity(dragState.id);
                    if (ent) {
                        ent->transform.position.x = dragState.startX + deltaX;
                        ent->transform.position.y = dragState.startY + deltaY;
                    }
                }
            }
            else if (m_isMarqueeSelecting)
            {
                float worldX, worldY;
                m_camera.ScreenToWorld(mouseX, mouseY, w, h, worldX, worldY);
                m_marqueeCurrentWorldX = worldX;
                m_marqueeCurrentWorldY = worldY;
            }
        }

        if (!Core::Input::IsKeyDown(VK_LBUTTON))
        {
            if (m_isDragging) m_isDragging = false;

            if (m_isMarqueeSelecting)
            {
                m_isMarqueeSelecting = false;

                float minX = std::min(m_marqueeStartWorldX, m_marqueeCurrentWorldX);
                float maxX = std::max(m_marqueeStartWorldX, m_marqueeCurrentWorldX);
                float minY = std::min(m_marqueeStartWorldY, m_marqueeCurrentWorldY);
                float maxY = std::max(m_marqueeStartWorldY, m_marqueeCurrentWorldY);

                bool isAdditive = Core::Input::IsKeyDown(VK_CONTROL) || Core::Input::IsKeyDown(VK_SHIFT);
                if (!isAdditive) m_selectedEntityIds.clear();

                for (const auto& entity : m_scene.GetEntities())
                {
                    if (entity.transform.position.x < maxX &&
                        entity.transform.position.x + entity.width > minX &&
                        entity.transform.position.y < maxY &&
                        entity.transform.position.y + entity.height > minY)
                    {
                        if (std::find(m_selectedEntityIds.begin(), m_selectedEntityIds.end(), entity.id) == m_selectedEntityIds.end()) {
                            m_selectedEntityIds.push_back(entity.id);
                        }
                    }
                }
            }
        }
    }

    void HandleHierarchyClick()
    {
        if (!Core::Input::IsLeftMouseButtonPressed()) return;
        float mouseX, mouseY;
        Core::Input::GetMousePosition(mouseX, mouseY);
        if (mouseX < 0.0f || mouseX >= GetLeftPanelWidth() || mouseY < GetHierarchyStartY()) return;
        std::size_t rowIndex = (std::size_t)((mouseY - GetHierarchyStartY()) / HierarchyRowHeight);
        if (rowIndex >= GetVisibleHierarchyCount()) return;

        bool isAdditive = Core::Input::IsKeyDown(VK_CONTROL) || Core::Input::IsKeyDown(VK_SHIFT);
        SelectEntity(m_hierarchyEntityIds[rowIndex], isAdditive);
    }

    void HandleUIClicks()
    {
        if (!Core::Input::IsLeftMouseButtonPressed()) return;

        float mx, my;
        Core::Input::GetMousePosition(mx, my);
        const float width = (float)GetRenderer().GetWidth();

        // Обработка чекбокса Is Static в Inspector.
        if (!m_isPlaying && !m_selectedEntityIds.empty())
        {
            const float rightPanelWidth = std::min(280.0f, width * 0.30f);
            const float rightPanelX = std::max(
                GetLeftPanelWidth(),
                width - rightPanelWidth
            );

            const float inspectorX = rightPanelX + 12.0f;

            const float cbY = 200.0f;
            const float cbX = inspectorX + 120.0f;
            const float cbSize = 14.0f;

            if (mx >= cbX && mx <= cbX + cbSize &&
                my >= cbY && my <= cbY + cbSize)
            {
                bool allStatic = true;

                for (auto id : m_selectedEntityIds)
                {
                    const auto* entity = m_scene.FindEntity(id);

                    if (entity && !entity->isStatic)
                    {
                        allStatic = false;
                        break;
                    }
                }

                const bool newState = !allStatic;

                for (auto id : m_selectedEntityIds)
                {
                    auto* entity = m_scene.FindEntity(id);

                    if (entity)
                        entity->isStatic = newState;
                }

                return;
            }
        }

        // Если клик был в верхней панели (высота 36), обрабатываем только кнопки UI
        if (my >= 0.0f && my <= TopBarHeight)
        {
            // Кнопки выбора фигуры
            if (mx >= 300.0f && mx <= 360.0f) m_currentShapeType = Core::ShapeType::Rectangle;
            if (mx >= 365.0f && mx <= 425.0f) m_currentShapeType = Core::ShapeType::Circle;
            if (mx >= 430.0f && mx <= 490.0f) m_currentShapeType = Core::ShapeType::Triangle;

            // Кнопки Save и Load
            if (mx >= width - 200.0f && mx <= width - 110.0f) SaveScene("level.kjarn");
            if (mx >= width - 100.0f && mx <= width - 10.0f) LoadScene("level.kjarn");

            // Кнопка Spawn Player
            if (mx >= 610.0f && mx <= 690.0f)
            {
                Core::EntityId newId = m_scene.CreateEntity();
                Core::Entity* newEntity = m_scene.FindEntity(newId);
                if (newEntity)
                {
                    newEntity->width = 40.0f; // Игрок чуть меньше кубика
                    newEntity->height = 60.0f;
                    newEntity->transform.position.x = m_camera.x;
                    newEntity->transform.position.y = m_camera.y - 200.0f; // Чуть выше камеры
                    newEntity->color = Core::Color(50, 150, 255); // Синий
                    newEntity->shape = Core::ShapeType::Rectangle; // Или сделай ему отдельную форму
                    newEntity->isPlayer = true;
                    newEntity->isStatic = false;
                    m_selectedEntityIds = { newId };
                }
            }

            // Кнопка + Add Block
            if (mx >= 500.0f && mx <= 600.0f)
            {
                Core::EntityId newId = m_scene.CreateEntity();
                Core::Entity* newEntity = m_scene.FindEntity(newId);
                if (newEntity)
                {
                    newEntity->width = CellSize - 10.0f;
                    newEntity->height = CellSize - 10.0f;
                    float gridX = round(m_camera.x / CellSize) * CellSize;
                    float gridY = round(m_camera.y / CellSize) * CellSize;
                    newEntity->transform.position.x = gridX + (CellSize - newEntity->width) / 2.0f;
                    newEntity->transform.position.y = gridY + (CellSize - newEntity->height) / 2.0f;
                    newEntity->shape = m_currentShapeType;

                    if (m_currentShapeType == Core::ShapeType::Circle)
                        newEntity->color = Core::Color(220, 80, 80);
                    else if (m_currentShapeType == Core::ShapeType::Triangle)
                        newEntity->color = Core::Color(80, 220, 80);
                    else
                        newEntity->color = Core::Color(200, 100, 50);

                    m_selectedEntityIds = { newId };
                }
            }

            // Кнопка Play/Stop
            if (mx >= 700.0f && mx <= 780.0f && my >= 6.0f && my <= 30.0f)
            {
                StartPlayMode();
                return;
            }

            
            return; // Выходим, чтобы клик по верхней панели не сработал во вьюпорте
        }

        // Если клик был НЕ в верхней панели, проверяем клики по иерархии
        HandleHierarchyClick();
    }

    void HandlePlayModeInput()
    {
        if (!Core::Input::IsLeftMouseButtonPressed())
            return;

        float mouseX = 0.0f;
        float mouseY = 0.0f;

        Core::Input::GetMousePosition(mouseX, mouseY);

        // Обрабатываем только кнопку STOP.
        if (mouseX >= 700.0f &&
            mouseX <= 780.0f &&
            mouseY >= 6.0f &&
            mouseY <= 30.0f)
        {
            StopPlayMode();
        }
    }

    void BuildHierarchy()
    {
        m_hierarchyEntityIds.clear();
        int gridWidth = m_gridRadius * 2 + 1;

        // Берем только центральные 25 объектов для списка
        for (int i = -2; i <= 2; ++i) {
            for (int j = -2; j <= 2; ++j) {
                std::size_t index = (std::size_t)((i + m_gridRadius) * gridWidth + (j + m_gridRadius));
                const auto& entities = m_scene.GetEntities();
                if (index < entities.size()) {
                    m_hierarchyEntityIds.push_back(entities[index].id);
                }
            }
        }
    }


    void UpdateRuntime(float deltaTime)
    {
        Core::Entity* player = nullptr;

        for (auto& entity : m_runtimeScene.GetEntities())
        {
            if (entity.isPlayer)
            {
                player = &entity;
                break;
            }
        }

        // Управление игроком.
        if (player != nullptr)
        {
            constexpr float moveSpeed = 500.0f;
            constexpr float jumpForce = -800.0f;

            if (Core::Input::IsKeyDown('A'))
                player->velocityX = -moveSpeed;
            else if (Core::Input::IsKeyDown('D'))
                player->velocityX = moveSpeed;
            else
                player->velocityX = 0.0f;

            if (Core::Input::IsKeyDown(VK_SPACE) && player->isGrounded)
            {
                player->velocityY = jumpForce;
                player->isGrounded = false;
            }
        }

        // Физика работает только с Runtime Scene.
        Core::Physics::UpdateScene(
            m_runtimeScene.GetEntities(),
            deltaTime,
            m_gravity
        );
    }

    void StartPlayMode()
    {
        if (m_isPlaying)
            return;

        // Создаём копию редакторской сцены.
        m_runtimeScene = m_scene;

        // Включаем гравитацию для динамических объектов.
        for (auto& entity : m_runtimeScene.GetEntities())
        {
            if (!entity.isStatic)
            {
                entity.useGravity = true;
            }
        }

        // Сбрасываем состояние редактора.
        m_selectedEntityIds.clear();
        m_isDragging = false;
        m_isMarqueeSelecting = false;
        m_dragStartPositions.clear();

        m_isPlaying = true;
    }

    void StopPlayMode()
    {
        if (!m_isPlaying)
            return;

        m_isPlaying = false;

        // Уничтожаем игровую копию.
        m_runtimeScene.Clear();

        m_selectedEntityIds.clear();
        m_isDragging = false;
        m_isMarqueeSelecting = false;
        m_dragStartPositions.clear();
    }

    void RegenerateScene()
    {
        m_hierarchyEntityIds.clear();
        m_selectedEntityIds.clear();

        float squareSize = CellSize - 10.0f;
        float offset = (CellSize - squareSize) / 2.0f;

        for (int i = -m_gridRadius; i <= m_gridRadius; ++i)
        {
            for (int j = -m_gridRadius; j <= m_gridRadius; ++j)
            {
                const Core::Color color = (i + j) % 2 == 0 ? Core::Color(80, 80, 80) : Core::Color(100, 100, 100);
                const Core::EntityId id = m_scene.CreateEntity();
                Core::Entity* entity = m_scene.FindEntity(id);
                if (entity == nullptr) continue;

                entity->transform.position.x = (i * CellSize) + offset;
                entity->transform.position.y = (j * CellSize) + offset;
                entity->width = squareSize;
                entity->height = squareSize;
                entity->color = color;
                entity->isStatic = true;
            }
        }

        BuildHierarchy();
    }


    void SaveScene(const std::string& filename)
    {
        if (!Core::SceneSerializer::Save(m_scene, filename))
        {
            std::cerr << "Failed to save scene: "
                << filename << "\n";

            return;
        }

        std::cout << "Scene saved: "
            << filename << "\n";
    }

    void LoadScene(const std::string& filename)
    {
        if (m_isPlaying)
            return;

        if (!Core::SceneSerializer::Load(filename, m_scene))
        {
            std::cerr << "Failed to load scene: "
                << filename << "\n";

            return;
        }

        m_hierarchyEntityIds.clear();
        m_selectedEntityIds.clear();

        m_isDragging = false;
        m_isMarqueeSelecting = false;
        m_dragStartPositions.clear();

        BuildHierarchy();

        std::cout << "Scene loaded: "
            << filename << "\n";
    }

    void BuildEditorUI()
    {
        m_uiDrawList.Clear();
        const float width = (float)GetRenderer().GetWidth();
        const float height = (float)GetRenderer().GetHeight();
        const float leftPanelWidth = GetLeftPanelWidth();
        const float rightPanelWidth = std::min(280.0f, width * 0.30f);
        const float rightPanelX = std::max(leftPanelWidth, width - rightPanelWidth);

        const Kjarn::UI::UIColor panel{ 38, 41, 48, 255 }, panelHeader{ 46, 49, 57, 255 };
        const Kjarn::UI::UIColor border{ 65, 69, 78, 255 }, text{ 220, 223, 230, 255 };
        const Kjarn::UI::UIColor secondaryText{ 145, 151, 163, 255 }, accent{ 85, 145, 220, 255 }, selectedRow{ 55, 80, 115, 255 };

        if (m_showStartupMenu)
        {
            // Затемнение фона
            m_uiDrawList.AddFillRect({ 0, 0, width, height }, { 20, 22, 26, 200 });

            // Окно по центру
            float cx = width / 2.0f, cy = height / 2.0f;
            m_uiDrawList.AddFillRect({ cx - 150, cy - 100, 300, 220 }, panel);
            m_uiDrawList.AddFillRect({ cx - 150, cy - 100, 300, 30 }, panelHeader);
            m_uiDrawList.AddText({ cx - 140, cy - 95 }, "Create New Scene", text, 14.0f);

            m_uiDrawList.AddText({ cx - 60, cy - 40 }, "Grid Radius:", text, 14.0f);
            m_uiDrawList.AddText({ cx - 5, cy - 15 }, std::to_string(m_inputRadius), accent, 18.0f);

            // Кнопки + и -
            m_uiDrawList.AddFillRect({ cx - 80, cy - 20, 40, 30 }, border);
            m_uiDrawList.AddText({ cx - 65, cy - 15 }, "-", text, 18.0f);

            m_uiDrawList.AddFillRect({ cx + 40, cy - 20, 40, 30 }, border);
            m_uiDrawList.AddText({ cx + 55, cy - 15 }, "+", text, 18.0f);

            // Кнопка Создать
            m_uiDrawList.AddFillRect({ cx - 80, cy + 50, 160, 40 }, accent);
            m_uiDrawList.AddText({ cx - 50, cy + 60 }, "CREATE GRID", { 255, 255, 255, 255 }, 14.0f);
        }
        else
        {
            // --- СТАНДАРТНЫЙ ИНТЕРФЕЙС ---
            m_uiDrawList.AddFillRect({ 0.0f, 0.0f, width, TopBarHeight }, panelHeader);
            m_uiDrawList.AddText({ 14.0f, 9.0f }, "KJARN", accent, 17.0f);
            m_uiDrawList.AddText({ 100.0f, 11.0f }, "File    Edit    View    Project    Build    Help", text, 13.0f);
            m_uiDrawList.AddFillRect({ 500.0f, 6.0f, 100.0f, 24.0f }, border);

            // Кнопки Save и Load справа
            m_uiDrawList.AddFillRect({ width - 200.0f, 6.0f, 90.0f, 24.0f }, border);
            m_uiDrawList.AddText({ width - 185.0f, 10.0f }, "Save", text, 12.0f);

            m_uiDrawList.AddFillRect({ width - 100.0f, 6.0f, 90.0f, 24.0f }, border);
            m_uiDrawList.AddText({ width - 85.0f, 10.0f }, "Load", text, 12.0f);

            // Кнопки выбора фигуры
            Kjarn::UI::UIColor activeColor = accent;
            Kjarn::UI::UIColor inactiveColor = border;

            m_uiDrawList.AddFillRect({ 300.0f, 6.0f, 60.0f, 24.0f }, m_currentShapeType == Core::ShapeType::Rectangle ? activeColor : inactiveColor);
            m_uiDrawList.AddText({ 315.0f, 10.0f }, "Rect", text, 12.0f);

            m_uiDrawList.AddFillRect({ 365.0f, 6.0f, 60.0f, 24.0f }, m_currentShapeType == Core::ShapeType::Circle ? activeColor : inactiveColor);
            m_uiDrawList.AddText({ 376.0f, 10.0f }, "Circle", text, 12.0f);

            m_uiDrawList.AddFillRect({ 430.0f, 6.0f, 60.0f, 24.0f }, m_currentShapeType == Core::ShapeType::Triangle ? activeColor : inactiveColor);
            m_uiDrawList.AddText({ 445.0f, 10.0f }, "Tri", text, 12.0f);

            m_uiDrawList.AddText({ 515.0f, 10.0f }, "+ Add Block", text, 12.0f);
            m_uiDrawList.AddLine({ 0.0f, TopBarHeight }, { width, TopBarHeight }, border);

            m_uiDrawList.AddFillRect({ 0.0f, TopBarHeight, leftPanelWidth, height - TopBarHeight - BottomBarHeight }, panel);
            m_uiDrawList.AddFillRect({ 0.0f, TopBarHeight, leftPanelWidth, 32.0f }, panelHeader);
            m_uiDrawList.AddText({ 12.0f, TopBarHeight + 8.0f }, "SCENE HIERARCHY", text, 12.0f);
            m_uiDrawList.AddLine({ leftPanelWidth, TopBarHeight }, { leftPanelWidth, height - BottomBarHeight }, border);
            m_uiDrawList.AddText({ 14.0f, TopBarHeight + 40.0f }, "Scene", text, 13.0f);


            m_uiDrawList.AddFillRect({ 610.0f, 6.0f, 80.0f, 24.0f }, { 50, 150, 255, 255 }); // Синяя
            m_uiDrawList.AddText({ 618.0f, 10.0f }, "+ Player", text, 12.0f);

            // Кнопка PLAY / STOP
            if (!m_isPlaying) {
                m_uiDrawList.AddFillRect({ 700.0f, 6.0f, 80.0f, 24.0f }, { 80, 180, 80, 255 }); // Зеленая
                m_uiDrawList.AddText({ 720.0f, 10.0f }, "▶ PLAY", text, 12.0f);
            }
            else {
                m_uiDrawList.AddFillRect({ 700.0f, 6.0f, 80.0f, 24.0f }, { 200, 60, 60, 255 }); // Красная
                m_uiDrawList.AddText({ 720.0f, 10.0f }, "■ STOP", text, 12.0f);
            }

            const std::size_t visibleCount = GetVisibleHierarchyCount();
            for (std::size_t i = 0; i < visibleCount; ++i) {
                const Core::EntityId entityId = m_hierarchyEntityIds[i];
                const Core::Entity* entity = m_scene.FindEntity(entityId);
                if (entity == nullptr) continue;

                float rowY = GetHierarchyStartY() + (float)i * HierarchyRowHeight;
                bool isSelected = std::find(m_selectedEntityIds.begin(), m_selectedEntityIds.end(), entityId) != m_selectedEntityIds.end();
                if (isSelected) m_uiDrawList.AddFillRect({ 0.0f, rowY, leftPanelWidth, HierarchyRowHeight }, selectedRow);

                int gridX = (int)(entity->transform.position.x / CellSize);
                int gridY = (int)(entity->transform.position.y / CellSize);
                std::string label = "Cell (" + std::to_string(gridX) + ", " + std::to_string(gridY) + ")";
                m_uiDrawList.AddText({ 28.0f, rowY + 4.0f }, label, isSelected ? text : secondaryText, 12.0f);
            }

            m_uiDrawList.AddFillRect({ rightPanelX, TopBarHeight, width - rightPanelX, height - TopBarHeight - BottomBarHeight }, panel);
            m_uiDrawList.AddFillRect({ rightPanelX, TopBarHeight, width - rightPanelX, 32.0f }, panelHeader);
            m_uiDrawList.AddText({ rightPanelX + 12.0f, TopBarHeight + 8.0f }, "INSPECTOR", text, 12.0f);
            m_uiDrawList.AddLine({ rightPanelX, TopBarHeight }, { rightPanelX, height - BottomBarHeight }, border);

            if (!m_selectedEntityIds.empty())
            {
                const float inspectorX = rightPanelX + 12.0f;

                // Показываем информацию о выделении
                if (m_selectedEntityIds.size() == 1)
                {
                    const Core::Entity* selected = m_scene.FindEntity(m_selectedEntityIds[0]);
                    if (selected) {
                        int gridX = (int)(selected->transform.position.x / CellSize);
                        int gridY = (int)(selected->transform.position.y / CellSize);
                        m_uiDrawList.AddText({ inspectorX, 80.0f }, "Cell (" + std::to_string(gridX) + ", " + std::to_string(gridY) + ")", accent, 14.0f);
                        m_uiDrawList.AddText({ inspectorX, 140.0f }, "Position X: " + std::to_string((int)selected->transform.position.x), secondaryText, 12.0f);
                        m_uiDrawList.AddText({ inspectorX, 162.0f }, "Position Y: " + std::to_string((int)selected->transform.position.y), secondaryText, 12.0f);
                    }
                }
                else
                {
                    m_uiDrawList.AddText({ inspectorX, 80.0f }, std::to_string(m_selectedEntityIds.size()) + " objects selected", accent, 14.0f);
                }

                // ОБЩИЙ ЧЕКБОКС ДЛЯ ВСЕХ ВЫДЕЛЕННЫХ
                float cbY = 200.0f;
                m_uiDrawList.AddText({ inspectorX, cbY + 2.0f }, "Is Static (Ground):", text, 12.0f);

                // Координаты квадратика чекбокса
                float cbX = inspectorX + 120.0f;
                float cbSize = 14.0f;

                m_uiDrawList.AddFillRect({ cbX, cbY, cbSize, cbSize }, border);

                // Проверяем, все ли выделенные объекты статичны
                bool allStatic = true;
                for (auto id : m_selectedEntityIds) {
                    auto* e = m_scene.FindEntity(id);
                    if (e && !e->isStatic) { allStatic = false; break; }
                }

                // Если все статичны, рисуем галочку (заливку)
                if (allStatic) {
                    m_uiDrawList.AddFillRect({ cbX + 2.0f, cbY + 2.0f, cbSize - 4.0f, cbSize - 4.0f }, accent);
                }
            }
            else
            {
                m_uiDrawList.AddText({ rightPanelX + 12.0f, 80.0f }, "No object selected", secondaryText, 12.0f);
            }


            m_uiDrawList.AddFillRect({ 0.0f, height - BottomBarHeight, width, BottomBarHeight }, panelHeader);
            m_uiDrawList.AddLine({ 0.0f, height - BottomBarHeight }, { width, height - BottomBarHeight }, border);
            m_uiDrawList.AddText({ 12.0f, height - 18.0f }, !m_selectedEntityIds.empty() ? "OBJECTS SELECTED: " + std::to_string(m_selectedEntityIds.size()) : "READY", accent, 11.0f);
            m_uiDrawList.AddText({ width - 150.0f, height - 18.0f }, "Kjarn Sandbox", secondaryText, 11.0f);
        }
    }

    Core::Camera m_camera;
    // Сцена, которую редактирует пользователь.
    Core::Scene m_scene;

    // Копия сцены для игрового режима.
    Core::Scene m_runtimeScene;

    std::vector<Core::EntityId> m_hierarchyEntityIds;
    std::vector<Core::EntityId> m_selectedEntityIds;
    Kjarn::UI::UIDrawList m_uiDrawList;
    Kjarn::UI::IUIDrawBackend& m_uiBackend;
};

int main()
{
    Core::Window window(L"Kjarn Sandbox", 1280, 800);
    Renderer::Win32::GdiRenderer renderer(window.GetNativeHandle());
    Kjarn::UI::Win32::GdiUIDrawBackend uiBackend(renderer);

    SandboxApplication application(window, renderer, uiBackend);
    application.Run();

    return 0;
}