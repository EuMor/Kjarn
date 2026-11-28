#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Application.h"
#include "Core/Camera.h"
#include "Core/Color.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/SceneSerializer.h"
#include "Editor/Tools/BrushTool.h"
#include "Editor/History/EditorHistory.h"

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

        
        // Обрабатываем скролл колесиком ДО обработки кликов
        float mx, my;
        Core::Input::GetMousePosition(mx, my);
        const float height = (float)GetRenderer().GetHeight();

        if (mx >= 0.0f && mx <= GetLeftPanelWidth() && my > TopBarHeight && my < height - BottomBarHeight)
        {
            int wheel = Core::Input::GetMouseWheelDelta();
            if (wheel != 0) {
                m_hierarchyScroll -= wheel * 30.0f;
                float maxScroll = std::max(0.0f, (float)m_hierarchyEntityIds.size() * HierarchyRowHeight - (height - TopBarHeight - BottomBarHeight - 40.0f));
                m_hierarchyScroll = std::clamp(m_hierarchyScroll, 0.0f, maxScroll);
            }
        }

        // Обработка перетаскивания скроллбара
        if (m_isDraggingScrollbar)
        {
            if (Core::Input::IsKeyDown(VK_LBUTTON))
            {
                float mx, my;
                Core::Input::GetMousePosition(mx, my);
                float height = (float)GetRenderer().GetHeight();

                float totalHeight = m_hierarchyEntityIds.size() * HierarchyRowHeight;
                float viewHeight = height - BottomBarHeight - GetHierarchyStartY();

                if (totalHeight > viewHeight) {
                    float thumbHeight = std::max(20.0f, (viewHeight / totalHeight) * viewHeight);
                    float maxScroll = totalHeight - viewHeight;

                    // Считаем новую позицию скролла
                    float newThumbY = my - m_scrollbarDragOffsetY - GetHierarchyStartY();
                    float scrollRatio = std::clamp(newThumbY / (viewHeight - thumbHeight), 0.0f, 1.0f);

                    m_hierarchyScroll = scrollRatio * maxScroll;
                }
            }
            else
            {
                m_isDraggingScrollbar = false; // Отпустили кнопку
            }
            return; // Не даем работать остальному редактору, пока тащим
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
            BuildHierarchy();
        }

        // --- ГОРЯЧИЕ КЛАВИШИ ИНСТРУМЕНТОВ ---
        if (Core::Input::IsKeyDown(VK_CONTROL)) {
            if (Core::Input::IsKeyPressed('Z')) m_history.Undo(m_scene.GetTilemap());
            if (Core::Input::IsKeyPressed('Y')) m_history.Redo(m_scene.GetTilemap());
        }
        if (Core::Input::IsKeyPressed('B')) m_currentTool = EditorTool::Brush;
        if (Core::Input::IsKeyPressed('E')) m_currentTool = EditorTool::Eraser;
        if (Core::Input::IsKeyPressed('V')) m_currentTool = EditorTool::Select;
        if (Core::Input::IsKeyPressed('R')) m_currentTool = EditorTool::RectBrush;

        if (!m_selectedEntityIds.empty() && Core::Input::IsKeyPressed(VK_DELETE))
        {
            for (auto id : m_selectedEntityIds)
                m_scene.DestroyEntity(id);

            m_selectedEntityIds.clear();
            BuildHierarchy(); // Обновляем список при удалении
        }

        // --- КАМЕРА: ДВИЖЕНИЕ И ЗУМ ---
        constexpr float cameraSpeed = 500.0f;
        if (Core::Input::IsKeyDown('W')) m_camera.y -= cameraSpeed * deltaTime / m_camera.zoom;
        if (Core::Input::IsKeyDown('S')) m_camera.y += cameraSpeed * deltaTime / m_camera.zoom;
        if (Core::Input::IsKeyDown('A')) m_camera.x -= cameraSpeed * deltaTime / m_camera.zoom;
        if (Core::Input::IsKeyDown('D')) m_camera.x += cameraSpeed * deltaTime / m_camera.zoom;

        // MMB Панорамирование
        if (Core::Input::IsKeyDown(VK_MBUTTON) && IsMouseOverViewport(mx, my))
        {
            if (!m_isPanning) {
                m_isPanning = true;
                m_lastMouseX = mx;
                m_lastMouseY = my;
            }
            else {
                m_camera.x -= (mx - m_lastMouseX) / m_camera.zoom;
                m_camera.y -= (my - m_lastMouseY) / m_camera.zoom;
                m_lastMouseX = mx;
                m_lastMouseY = my;
            }
        }
        else {
            m_isPanning = false;
        }

        // Зум к курсору
        if (IsMouseOverViewport(mx, my))
        {
            const int wheel = Core::Input::GetMouseWheelDelta();
            if (wheel != 0) {
                float oldZoom = m_camera.zoom;
                if (wheel > 0) m_camera.zoom *= 1.1f;
                if (wheel < 0) m_camera.zoom /= 1.1f;
                m_camera.zoom = std::clamp(m_camera.zoom, 0.1f, 10.0f);

                float viewW = (float)GetRenderer().GetWidth();
                float viewH = (float)GetRenderer().GetHeight();
                float worldMouseX = (mx - viewW / 2.0f) / oldZoom + m_camera.x;
                float worldMouseY = (my - viewH / 2.0f) / oldZoom + m_camera.y;

                m_camera.x = worldMouseX - (mx - viewW / 2.0f) / m_camera.zoom;
                m_camera.y = worldMouseY - (my - viewH / 2.0f) / m_camera.zoom;
            }
        }
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

            // 2. Отрисовка Тайлмапа (земля, стены)
            const Core::Tilemap& tilemap = activeScene.GetTilemap();
            for (const auto& [key, tile] : tilemap.GetTiles())
            {
                int cx, cy;
                Core::Tilemap::UnpackKey(key, cx, cy);

                float worldX = cx * CellSize + 5.0f; // +5 для центрирования в клетке
                float worldY = cy * CellSize + 5.0f;
                float size = CellSize - 10.0f;

                // Базовая отрисовка
                renderer.DrawRect(m_camera, worldX, worldY, size, size, tile.color);

                // Отрисовка паттернов (детализация)
                if (tile.pattern == Core::TilePattern::Grass) {
                    // Темно-зеленые травинки (контрастнее)
                    renderer.DrawRect(m_camera, worldX + 20, worldY + 30, 4, 15, Core::Color(20, 80, 20));
                    renderer.DrawRect(m_camera, worldX + 50, worldY + 60, 4, 15, Core::Color(20, 80, 20));
                }
                else if (tile.pattern == Core::TilePattern::Stone) {
                    // Темные трещины
                    renderer.DrawRect(m_camera, worldX + 40, worldY + 10, 3, 40, Core::Color(40, 40, 40));
                    renderer.DrawRect(m_camera, worldX + 10, worldY + 50, 20, 3, Core::Color(40, 40, 40));
                }
                else if (tile.pattern == Core::TilePattern::Water) {
                    // Светлые блики (волны)
                    renderer.DrawRect(m_camera, worldX + 15, worldY + 25, 25, 3, Core::Color(120, 120, 255));
                    renderer.DrawRect(m_camera, worldX + 55, worldY + 65, 25, 3, Core::Color(120, 120, 255));
                }
            }

            // Превью RectBrush
            if (m_currentTool == EditorTool::RectBrush && m_isRectPainting) {
                float mouseX, mouseY;
                Core::Input::GetMousePosition(mouseX, mouseY);
                float worldX, worldY;
                m_camera.ScreenToWorld(mouseX, mouseY, (float)renderer.GetWidth(), (float)renderer.GetHeight(), worldX, worldY);

                int endX = (int)std::floor(worldX / CellSize);
                int endY = (int)std::floor(worldY / CellSize);

                int minX = std::min(m_rectStartX, endX) * (int)CellSize;
                int maxX = (std::max(m_rectStartX, endX) + 1) * (int)CellSize;
                int minY = std::min(m_rectStartY, endY) * (int)CellSize;
                int maxY = (std::max(m_rectStartY, endY) + 1) * (int)CellSize;

                Core::Color previewColor(85, 145, 220);
                float t = 3.0f / m_camera.zoom;

                renderer.DrawRect(m_camera, (float)minX, (float)minY, (float)(maxX - minX), t, previewColor);
                renderer.DrawRect(m_camera, (float)minX, (float)(maxY)-t, (float)(maxX - minX), t, previewColor);
                renderer.DrawRect(m_camera, (float)minX, (float)minY, t, (float)(maxY - minY), previewColor);
                renderer.DrawRect(m_camera, (float)(maxX)-t, (float)minY, t, (float)(maxY - minY), previewColor);
            }

            // 3. Отрисовка Сущностей (с сортировкой по Y)
            // Создаем временный массив указателей и сортируем его
            std::vector<const Core::Entity*> sortedEntities;
            for (const auto& entity : activeScene.GetEntities()) {
                sortedEntities.push_back(&entity);
            }

            // Сортируем: кто выше по экрану (меньше Y), тот рисуется раньше (сзади)
            std::sort(sortedEntities.begin(), sortedEntities.end(),
                [](const Core::Entity* a, const Core::Entity* b) {
                    if (a->zIndex != b->zIndex) return a->zIndex < b->zIndex;
                    return a->transform.position.y < b->transform.position.y;
                });

            // Отрисовываем в отсортированном порядке
            for (const auto* entity : sortedEntities)
            {
                if (entity->shape == Core::ShapeType::Circle) {
                    renderer.DrawCircle(m_camera, entity->transform.position.x, entity->transform.position.y, entity->width, entity->height, entity->color);
                }
                else if (entity->shape == Core::ShapeType::Triangle) {
                    renderer.DrawTriangle(m_camera, entity->transform.position.x, entity->transform.position.y, entity->width, entity->height, entity->color);
                }
                else {
                    renderer.DrawRect(m_camera, entity->transform.position.x, entity->transform.position.y, entity->width, entity->height, entity->color);
                }
            }

            // 4. Подсветка
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

    float m_hierarchyScroll = 0.0f;
    bool m_isDraggingScrollbar = false;
    float m_scrollbarDragOffsetY = 0.0f;

    enum class EditorTool { Select, Brush, Eraser, PlaceEntity, RectBrush};
    EditorTool m_currentTool = EditorTool::Select;

    Kjarn::Editor::BrushTool m_brushTool;
    int m_lastPaintCellX = 0;
    int m_lastPaintCellY = 0;
    bool m_isPainting = false;

    // Состояние Rect инструмента
    bool m_isRectPainting = false;
    int m_rectStartX = 0;
    int m_rectStartY = 0;
    
    // Для MMB (Middle Mouse Button) панорамирования
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
    bool m_isPanning = false;

    Kjarn::Editor::EditorHistory m_history;
    Kjarn::Editor::Stroke m_currentStroke;
    Core::TilePattern m_brushPattern = Core::TilePattern::Solid;

    enum class EntityType { Player, Crate, Tree };
    EntityType m_selectedEntityType = EntityType::Crate;

    // Палитра цветов для кисти
    std::vector<Core::Color> m_paletteColors = {
        Core::Color(80, 80, 80),   // Stone
        Core::Color(50, 150, 50),  // Grass
        Core::Color(50, 50, 200),  // Water
        Core::Color(200, 150, 50), // Sand
        Core::Color(200, 50, 50)   // Lava
    };
    Core::Color m_brushColor = Core::Color(80, 80, 80);

    // Прямоугольник для UI элементов
    struct UIRect {
        float x, y, w, h;
        bool Contains(float mx, float my) const {
            return mx >= x && mx <= x + w && my >= y && my <= y + h;
        }
    };

    // Структура со всеми кнопками верхней панели
    struct TopBarLayout {
        UIRect btnSelect, btnBrush, btnEraser;
        UIRect btnRect, btnCircle, btnTri, btnRectTool;;
        UIRect btnAddBlock, btnAddPlayer;
        UIRect btnPlay;
        UIRect btnNew, btnSave, btnLoad;
    };

    // Вычисляем позиции всех кнопок динамически
    TopBarLayout GetTopBarLayout(float width) const {
        TopBarLayout l;
        float y = 6.0f;
        float h = 24.0f;
        float gap = 5.0f;
        float x = 200.0f; // Отступ от меню "File Edit View"

        // Инструменты
        l.btnSelect = { x, y, 55.0f, h }; x += 55.0f + gap;
        l.btnBrush = { x, y, 65.0f, h }; x += 65.0f + gap;
        l.btnEraser = { x, y, 70.0f, h }; x += 70.0f + 15.0f; // Чуть больше отступ

        // Фигуры
        l.btnRect = { x, y, 50.0f, h }; x += 50.0f + gap;
        l.btnCircle = { x, y, 50.0f, h }; x += 50.0f + gap;
        l.btnTri = { x, y, 50.0f, h }; x += 50.0f + 15.0f;

        // Инструмент Прямоугольник (Рисует тайлы)
        l.btnRectTool = { x, y, 70.0f, h }; x += 70.0f + 15.0f;

        // Создание
        l.btnAddBlock = { x, y, 90.0f, h }; x += 90.0f + gap;
        l.btnAddPlayer = { x, y, 80.0f, h }; x += 80.0f + 15.0f;

        // Играть (фиксируем чуть правее)
        l.btnPlay = { x, y, 80.0f, h };

        // Кнопки справа (выравнивание по правому краю окна)
        float rightX = width - 10.0f;
        l.btnLoad = { rightX - 90.0f, y, 90.0f, h }; rightX -= 90.0f + gap;
        l.btnSave = { rightX - 90.0f, y, 90.0f, h }; rightX -= 90.0f + gap;
        l.btnNew = { rightX - 90.0f, y, 90.0f, h };

        return l;
    }


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

    void SpawnPrefab(EntityType type, float gridX, float gridY)
    {
        Core::EntityId id = m_scene.CreateEntity();
        Core::Entity* ent = m_scene.FindEntity(id);
        if (!ent) return;

        float sizeX = CellSize - 10.0f;
        float sizeY = CellSize - 10.0f;

        if (type == EntityType::Player) {
            sizeX = 40.0f; sizeY = 60.0f;
            ent->isPlayer = true;
            ent->color = Core::Color(50, 150, 255);
        }
        else if (type == EntityType::Crate) {
            sizeX = 80.0f; sizeY = 80.0f;
            ent->color = Core::Color(150, 80, 50);
        }
        else if (type == EntityType::Tree) {
            sizeX = 60.0f; sizeY = 100.0f;
            ent->color = Core::Color(50, 150, 50);
            ent->isStatic = true;
        }

        ent->width = sizeX;
        ent->height = sizeY;
        // Центрируем в клетке
        ent->transform.position.x = gridX + (CellSize - sizeX) / 2.0f;
        ent->transform.position.y = gridY + (CellSize - sizeY) / 2.0f;

        m_selectedEntityIds = { id };
        BuildHierarchy();
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

        // --- РЕЖИМ РАЗМЕЩЕНИЯ ОБЪЕКТОВ (PlaceEntity) ---
        if (m_currentTool == EditorTool::PlaceEntity)
        {
            if (Core::Input::IsLeftMouseButtonPressed() && IsMouseOverViewport(mouseX, mouseY))
            {
                float worldX, worldY;
                m_camera.ScreenToWorld(mouseX, mouseY, w, h, worldX, worldY);

                float gridX = std::floor(worldX / CellSize) * CellSize;
                float gridY = std::floor(worldY / CellSize) * CellSize;

                SpawnPrefab(m_selectedEntityType, gridX, gridY);
            }
            return;
        }

        // --- РЕЖИМ ПРЯМОУГОЛЬНИКА (RectBrush) ---
        if (m_currentTool == EditorTool::RectBrush)
        {
            if (Core::Input::IsLeftMouseButtonPressed() && IsMouseOverViewport(mouseX, mouseY))
            {
                float worldX, worldY;
                m_camera.ScreenToWorld(mouseX, mouseY, w, h, worldX, worldY);
                m_rectStartX = (int)std::floor(worldX / CellSize);
                m_rectStartY = (int)std::floor(worldY / CellSize);
                m_isRectPainting = true;
                m_currentStroke = {};
            }

            if (m_isRectPainting && !Core::Input::IsKeyDown(VK_LBUTTON))
            {
                float worldX, worldY;
                m_camera.ScreenToWorld(mouseX, mouseY, w, h, worldX, worldY);
                int endX = (int)std::floor(worldX / CellSize);
                int endY = (int)std::floor(worldY / CellSize);

                m_brushTool.PaintRect(m_scene.GetTilemap(), m_rectStartX, m_rectStartY, endX, endY, m_brushColor, m_brushPattern, false, m_currentStroke);
                m_history.Push(m_currentStroke);
                m_isRectPainting = false;
            }
            return;
        }

        // --- РЕЖИМ КИСТИ / ЛАСТИКА ---
        if (m_currentTool == EditorTool::Brush || m_currentTool == EditorTool::Eraser)
        {
            if (Core::Input::IsKeyDown(VK_LBUTTON) && IsMouseOverViewport(mouseX, mouseY))
            {
                float worldX, worldY;
                m_camera.ScreenToWorld(mouseX, mouseY, w, h, worldX, worldY);

                int cellX = (int)std::floor(worldX / CellSize);
                int cellY = (int)std::floor(worldY / CellSize);

                bool isErasing = (m_currentTool == EditorTool::Eraser);
                Core::Color paintColor = m_brushColor;

                if (!m_isPainting) {
                    // Начало мазка
                    m_isPainting = true;
                    m_currentStroke = {}; // Очищаем историю текущего мазка
                    m_brushTool.PaintLine(m_scene.GetTilemap(), cellX, cellY, cellX, cellY, paintColor, m_brushPattern, isErasing, m_currentStroke);
                }
                else {
                    // Протяжка
                    m_brushTool.PaintLine(m_scene.GetTilemap(), m_lastPaintCellX, m_lastPaintCellY, cellX, cellY, paintColor, m_brushPattern, isErasing, m_currentStroke);
                }

                m_lastPaintCellX = cellX;
                m_lastPaintCellY = cellY;
            }
            else
            {
                // Отпустили кнопку мыши — завершаем мазок
                if (m_isPainting) {
                    m_isPainting = false;
                    m_history.Push(m_currentStroke); // Сохраняем в историю
                }
            }
            return;
        }

        // --- РЕЖИМ ВЫДЕЛЕНИЯ ---
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

        // Учитываем скролл при вычислении строки!
        float localY = mouseY - GetHierarchyStartY() + m_hierarchyScroll;
        if (localY < 0.0f) return;

        std::size_t rowIndex = (std::size_t)(localY / HierarchyRowHeight);
        if (rowIndex >= m_hierarchyEntityIds.size()) return;

        bool isAdditive = Core::Input::IsKeyDown(VK_CONTROL) || Core::Input::IsKeyDown(VK_SHIFT);
        SelectEntity(m_hierarchyEntityIds[rowIndex], isAdditive);
    }

    void HandleUIClicks()
    {
        float mx, my;
        Core::Input::GetMousePosition(mx, my);
        const float width = (float)GetRenderer().GetWidth();
        const float height = (float)GetRenderer().GetHeight();

        // --- ОБРАБОТКА ПРАВОЙ ПАНЕЛИ (Палитры) ---
        const float rightPanelWidth = std::min(280.0f, width * 0.30f);
        const float rightPanelX = std::max(GetLeftPanelWidth(), width - rightPanelWidth);
        
        if (!Core::Input::IsLeftMouseButtonPressed()) return;

        // Клики по цветам
        for (size_t i = 0; i < m_paletteColors.size(); ++i) {
            float bx = rightPanelX + 12.0f + i * 32.0f;
            float by = TopBarHeight + 60.0f;
            if (mx >= bx && mx <= bx + 26.0f && my >= by && my <= by + 26.0f) {
                m_brushColor = m_paletteColors[i];
                m_currentTool = EditorTool::Brush;

                
                if (i == 0) m_brushPattern = Core::TilePattern::Stone;
                else if (i == 1) m_brushPattern = Core::TilePattern::Grass;
                else if (i == 2) m_brushPattern = Core::TilePattern::Water;
                else m_brushPattern = Core::TilePattern::Solid;

                return;
            }
        }

        // Клики по префабам
        float ey = TopBarHeight + 120.0f;
        if (my >= ey && my <= ey + 30.0f) {
            if (mx >= rightPanelX + 12.0f && mx <= rightPanelX + 82.0f) {
                m_selectedEntityType = EntityType::Player;
                m_currentTool = EditorTool::PlaceEntity;
                return;
            }
            if (mx >= rightPanelX + 92.0f && mx <= rightPanelX + 162.0f) {
                m_selectedEntityType = EntityType::Crate;
                m_currentTool = EditorTool::PlaceEntity;
                return;
            }
            if (mx >= rightPanelX + 172.0f && mx <= rightPanelX + 242.0f) {
                m_selectedEntityType = EntityType::Tree;
                m_currentTool = EditorTool::PlaceEntity;
                return;
            }
        }

        // --- ИНСПЕКТОР (чекбокс) ---

        // Проверяем, не схватили ли мы ползунок скроллбара
        if (Core::Input::IsLeftMouseButtonPressed() && !m_isDraggingScrollbar)
        {
            float totalHeight = m_hierarchyEntityIds.size() * HierarchyRowHeight;
            float viewHeight = height - BottomBarHeight - GetHierarchyStartY();

            if (totalHeight > viewHeight) {
                float scrollbarX = GetLeftPanelWidth() - 8.0f;
                float scrollbarWidth = 6.0f;
                float thumbHeight = std::max(20.0f, (viewHeight / totalHeight) * viewHeight);
                float thumbY = GetHierarchyStartY() + (m_hierarchyScroll / totalHeight) * viewHeight;

                // Если кликнули по ползунку
                if (mx >= scrollbarX && mx <= scrollbarX + scrollbarWidth && my >= thumbY && my <= thumbY + thumbHeight)
                {
                    m_isDraggingScrollbar = true;
                    m_scrollbarDragOffsetY = my - thumbY; // Запоминаем, где именно схватили
                    return;
                }
            }
        }

        

        // Обработка чекбокса Is Static в Inspector.
        if (!m_isPlaying && !m_selectedEntityIds.empty())
        {
            const float rightPanelWidth = std::min(280.0f, width * 0.30f);
            const float rightPanelX = std::max(GetLeftPanelWidth(), width - rightPanelWidth);
            const float inspectorX = rightPanelX + 12.0f;

            float insY = TopBarHeight + 200.0f;
            const float cbY = insY + 100.0f; // 👈 Та же координата, что и при отрисовке
            const float cbX = inspectorX + 120.0f;
            const float cbSize = 14.0f;

            if (mx >= cbX && mx <= cbX + cbSize && my >= cbY && my <= cbY + cbSize)
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
        
        // --- ОБРАБОТКА ИНСПЕКТОРА (Кнопки + / -) ---
        if (!m_isPlaying && !m_selectedEntityIds.empty())
        {
            const float inspectorX = rightPanelX + 12.0f;
            float insY = TopBarHeight + 200.0f;

            // Шаг изменения (сетка = 10, чтобы плавно двигать)
            float step = 10.0f;
            if (Core::Input::IsKeyDown(VK_SHIFT)) step = 1.0f; // Зажали Shift — точная настройка

            // X
            if (my >= insY + 40.0f && my <= insY + 56.0f) {
                if (mx >= inspectorX + 80.0f && mx <= inspectorX + 96.0f) { ModifySelectedEntities(-step, 0, 0, 0); return; }
                if (mx >= inspectorX + 145.0f && mx <= inspectorX + 161.0f) { ModifySelectedEntities(step, 0, 0, 0); return; }
            }
            // Y
            if (my >= insY + 65.0f && my <= insY + 81.0f) {
                if (mx >= inspectorX + 80.0f && mx <= inspectorX + 96.0f) { ModifySelectedEntities(0, -step, 0, 0); return; }
                if (mx >= inspectorX + 145.0f && mx <= inspectorX + 161.0f) { ModifySelectedEntities(0, step, 0, 0); return; }
            }
            // Width
            if (my >= insY + 90.0f && my <= insY + 106.0f) {
                if (mx >= inspectorX + 80.0f && mx <= inspectorX + 96.0f) { ModifySelectedEntities(0, 0, -step, 0); return; }
                if (mx >= inspectorX + 145.0f && mx <= inspectorX + 161.0f) { ModifySelectedEntities(0, 0, step, 0); return; }
            }
            // Height
            if (my >= insY + 115.0f && my <= insY + 131.0f) {
                if (mx >= inspectorX + 80.0f && mx <= inspectorX + 96.0f) { ModifySelectedEntities(0, 0, 0, -step); return; }
                if (mx >= inspectorX + 145.0f && mx <= inspectorX + 161.0f) { ModifySelectedEntities(0, 0, 0, step); return; }
            }
            // Z-Index
            if (my >= insY + 140.0f && my <= insY + 156.0f) {
                if (mx >= inspectorX + 80.0f && mx <= inspectorX + 96.0f) {
                    for (auto id : m_selectedEntityIds) { auto* e = m_scene.FindEntity(id); if (e) e->zIndex--; }
                    return;
                }
                if (mx >= inspectorX + 145.0f && mx <= inspectorX + 161.0f) {
                    for (auto id : m_selectedEntityIds) { auto* e = m_scene.FindEntity(id); if (e) e->zIndex++; }
                    return;
                }
            }

            // Чекбокс Is Static
            float cbY = insY + 155.0f;
            float cbX = inspectorX + 120.0f;
            if (mx >= cbX && mx <= cbX + 14.0f && my >= cbY && my <= cbY + 14.0f)
            {
                bool allStatic = true;
                for (auto id : m_selectedEntityIds) {
                    const auto* entity = m_scene.FindEntity(id);
                    if (entity && !entity->isStatic) { allStatic = false; break; }
                }
                const bool newState = !allStatic;
                for (auto id : m_selectedEntityIds) {
                    auto* entity = m_scene.FindEntity(id);
                    if (entity) entity->isStatic = newState;
                }
                return;
            }
        }

        // Если клик был в верхней панели (высота 36), обрабатываем только кнопки UI
        if (my >= 0.0f && my <= TopBarHeight)
        {
            TopBarLayout layout = GetTopBarLayout(width);

            if (layout.btnSelect.Contains(mx, my)) m_currentTool = EditorTool::Select;
            if (layout.btnBrush.Contains(mx, my)) m_currentTool = EditorTool::Brush;
            if (layout.btnEraser.Contains(mx, my)) m_currentTool = EditorTool::Eraser;

            if (layout.btnRect.Contains(mx, my)) m_currentShapeType = Core::ShapeType::Rectangle;
            if (layout.btnCircle.Contains(mx, my)) m_currentShapeType = Core::ShapeType::Circle;
            if (layout.btnTri.Contains(mx, my)) m_currentShapeType = Core::ShapeType::Triangle;

            if (layout.btnRectTool.Contains(mx, my)) m_currentTool = EditorTool::RectBrush;

            if (layout.btnNew.Contains(mx, my)) {
                m_scene.Clear();
                m_history = Kjarn::Editor::EditorHistory();
                BuildHierarchy();
            }
            if (layout.btnSave.Contains(mx, my)) {
                SaveScene("level.kjarn");
            }
            if (layout.btnLoad.Contains(mx, my)) {
                LoadScene("level.kjarn");
            }

            // Кнопка Spawn Player (630 - 710)
            if (layout.btnAddPlayer.Contains(mx, my)) 
            {
                Core::EntityId newId = m_scene.CreateEntity();
                Core::Entity* newEntity = m_scene.FindEntity(newId);
                if (newEntity)
                {
                    newEntity->width = 40.0f;
                    newEntity->height = 60.0f;
                    newEntity->transform.position.x = m_camera.x;
                    newEntity->transform.position.y = m_camera.y - 200.0f;
                    newEntity->color = Core::Color(50, 150, 255);
                    newEntity->shape = Core::ShapeType::Rectangle;
                    newEntity->isPlayer = true;
                    newEntity->isStatic = false;
                    m_selectedEntityIds = { newId };
                }
                BuildHierarchy();
            }

            // Кнопка + Add Block (530 - 620)
            if (layout.btnAddBlock.Contains(mx, my)) 
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
                BuildHierarchy();
            }

            // Кнопка Play/Stop (720 - 800)
            if (layout.btnPlay.Contains(mx, my)) {
                StartPlayMode();
                return;
            }
            return;
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
        if (mouseX >= 720.0f &&
            mouseX <= 800.0f &&
            mouseY >= 6.0f &&
            mouseY <= 30.0f)
        {
            StopPlayMode();
        }

    }

    void BuildHierarchy()
    {
        m_hierarchyEntityIds.clear();
        // Проходим по ВСЕМ объектам сцены и добавляем их в список
        for (const auto& entity : m_scene.GetEntities())
        {
            m_hierarchyEntityIds.push_back(entity.id);
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
            m_runtimeScene.GetTilemap(),
            CellSize,
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
        m_scene.Clear();
        m_hierarchyEntityIds.clear();
        m_selectedEntityIds.clear();

        auto& tilemap = m_scene.GetTilemap();

        // Заполняем тайлмап клетками (а не сущностями!)
        for (int i = -m_gridRadius; i <= m_gridRadius; ++i)
        {
            for (int j = -m_gridRadius; j <= m_gridRadius; ++j)
            {
                const Core::Color color = (i + j) % 2 == 0 ? Core::Color(80, 80, 80) : Core::Color(100, 100, 100);
                tilemap.SetTile(i, j, Core::Tile{ color });
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

            TopBarLayout layout = GetTopBarLayout(width);

            
            // Инструменты
            DrawButton(layout.btnSelect, "Sel (V)", m_currentTool == EditorTool::Select);
            DrawButton(layout.btnBrush, "Brush (B)", m_currentTool == EditorTool::Brush);
            DrawButton(layout.btnEraser, "Eraser (E)", m_currentTool == EditorTool::Eraser);
            DrawButton(layout.btnRectTool, "Rect (R)", m_currentTool == EditorTool::RectBrush);

            // Фигуры
            m_uiDrawList.AddFillRect({ layout.btnRect.x, layout.btnRect.y, layout.btnRect.w, layout.btnRect.h }, m_currentShapeType == Core::ShapeType::Rectangle ? accent : border);
            m_uiDrawList.AddText({ layout.btnRect.x + 15.0f, 10.0f }, "Rect", text, 12.0f);

            m_uiDrawList.AddFillRect({ layout.btnCircle.x, layout.btnCircle.y, layout.btnCircle.w, layout.btnCircle.h }, m_currentShapeType == Core::ShapeType::Circle ? accent : border);
            m_uiDrawList.AddText({ layout.btnCircle.x + 10.0f, 10.0f }, "Circle", text, 12.0f);

            m_uiDrawList.AddFillRect({ layout.btnTri.x, layout.btnTri.y, layout.btnTri.w, layout.btnTri.h }, m_currentShapeType == Core::ShapeType::Triangle ? accent : border);
            m_uiDrawList.AddText({ layout.btnTri.x + 15.0f, 10.0f }, "Tri", text, 12.0f);

            // Создание
            m_uiDrawList.AddFillRect({ layout.btnAddBlock.x, layout.btnAddBlock.y, layout.btnAddBlock.w, layout.btnAddBlock.h }, border);
            m_uiDrawList.AddText({ layout.btnAddBlock.x + 10.0f, 10.0f }, "+ Add Block", text, 12.0f);

            m_uiDrawList.AddFillRect({ layout.btnAddPlayer.x, layout.btnAddPlayer.y, layout.btnAddPlayer.w, layout.btnAddPlayer.h }, { 50, 150, 255, 255 });
            m_uiDrawList.AddText({ layout.btnAddPlayer.x + 10.0f, 10.0f }, "+ Player", text, 12.0f);

            // PLAY / STOP
            m_uiDrawList.AddFillRect({ layout.btnPlay.x, layout.btnPlay.y, layout.btnPlay.w, layout.btnPlay.h }, m_isPlaying ? Kjarn::UI::UIColor{ 200, 60, 60, 255 } : Kjarn::UI::UIColor{ 80, 180, 80, 255 });
            m_uiDrawList.AddText({ layout.btnPlay.x + 20.0f, 10.0f }, m_isPlaying ? "■ STOP" : "▶ PLAY", text, 12.0f);

            // Кнопки справа (New, Save, Load)
            m_uiDrawList.AddFillRect({ layout.btnNew.x, layout.btnNew.y, layout.btnNew.w, layout.btnNew.h }, border);
            m_uiDrawList.AddText({ layout.btnNew.x + 10.0f, 10.0f }, "New Scene", text, 12.0f);

            m_uiDrawList.AddFillRect({ layout.btnSave.x, layout.btnSave.y, layout.btnSave.w, layout.btnSave.h }, border);
            m_uiDrawList.AddText({ layout.btnSave.x + 25.0f, 10.0f }, "Save", text, 12.0f);

            m_uiDrawList.AddFillRect({ layout.btnLoad.x, layout.btnLoad.y, layout.btnLoad.w, layout.btnLoad.h }, border);
            m_uiDrawList.AddText({ layout.btnLoad.x + 25.0f, 10.0f }, "Load", text, 12.0f);

            m_uiDrawList.AddLine({ 0.0f, TopBarHeight }, { width, TopBarHeight }, border);

            m_uiDrawList.AddFillRect({ 0.0f, TopBarHeight, leftPanelWidth, height - TopBarHeight - BottomBarHeight }, panel);
            m_uiDrawList.AddFillRect({ 0.0f, TopBarHeight, leftPanelWidth, 32.0f }, panelHeader);
            m_uiDrawList.AddText({ 12.0f, TopBarHeight + 8.0f }, "SCENE HIERARCHY", text, 12.0f);
            m_uiDrawList.AddLine({ leftPanelWidth, TopBarHeight }, { leftPanelWidth, height - BottomBarHeight }, border);
            m_uiDrawList.AddText({ 14.0f, TopBarHeight + 40.0f }, "Scene", text, 13.0f);



            const std::size_t visibleCount = GetVisibleHierarchyCount();
            // Отрисовка списка с учетом скролла
            float clipTop = GetHierarchyStartY();
            float clipBottom = height - BottomBarHeight;

            for (std::size_t i = 0; i < m_hierarchyEntityIds.size(); ++i) {
                const Core::EntityId entityId = m_hierarchyEntityIds[i];
                const Core::Entity* entity = m_scene.FindEntity(entityId);
                if (entity == nullptr) continue;

                // Сдвигаем Y с учетом скролла
                float rowY = GetHierarchyStartY() + (float)i * HierarchyRowHeight - m_hierarchyScroll;

                // Отсечение (Clipping): рисуем только то, что внутри панели
                if (rowY + HierarchyRowHeight < clipTop || rowY > clipBottom) continue;

                bool isSelected = std::find(m_selectedEntityIds.begin(), m_selectedEntityIds.end(), entityId) != m_selectedEntityIds.end();
                if (isSelected) m_uiDrawList.AddFillRect({ 0.0f, rowY, leftPanelWidth, HierarchyRowHeight }, selectedRow);

                int gridX = (int)(entity->transform.position.x / CellSize);
                int gridY = (int)(entity->transform.position.y / CellSize);
                std::string label = "Cell (" + std::to_string(gridX) + ", " + std::to_string(gridY) + ")";
                if (entity->isPlayer) label = "Player Unit";

                m_uiDrawList.AddText({ 28.0f, rowY + 4.0f }, label, isSelected ? text : secondaryText, 12.0f);
            }
            // Рисуем визуальный скроллбар
            float totalHeight = m_hierarchyEntityIds.size() * HierarchyRowHeight;
            float viewHeight = height - BottomBarHeight - GetHierarchyStartY();

            if (totalHeight > viewHeight) {
                float scrollbarX = leftPanelWidth - 8.0f;
                float scrollbarWidth = 6.0f;

                // Фон скроллбара
                m_uiDrawList.AddFillRect({ scrollbarX, GetHierarchyStartY(), scrollbarWidth, viewHeight }, panelHeader);

                // Ползунок
                float thumbHeight = std::max(20.0f, (viewHeight / totalHeight) * viewHeight);
                float thumbY = GetHierarchyStartY() + (m_hierarchyScroll / totalHeight) * viewHeight;

                m_uiDrawList.AddFillRect({ scrollbarX, thumbY, scrollbarWidth, thumbHeight }, border);
            }
            // === ПРАВАЯ ПАНЕЛЬ ===
            m_uiDrawList.AddFillRect({ rightPanelX, TopBarHeight, width - rightPanelX, height - TopBarHeight - BottomBarHeight }, panel);
            m_uiDrawList.AddFillRect({ rightPanelX, TopBarHeight, width - rightPanelX, 32.0f }, panelHeader);
            m_uiDrawList.AddText({ rightPanelX + 12.0f, TopBarHeight + 8.0f }, "EDITOR PANEL", text, 12.0f);
            m_uiDrawList.AddLine({ rightPanelX, TopBarHeight }, { rightPanelX, height - BottomBarHeight }, border);

            // 1. Палитра цветов кисти
            m_uiDrawList.AddText({ rightPanelX + 12.0f, TopBarHeight + 45.0f }, "TERRAIN COLORS", text, 11.0f);
            for (size_t i = 0; i < m_paletteColors.size(); ++i) {
                float bx = rightPanelX + 12.0f + i * 32.0f;
                float by = TopBarHeight + 60.0f;
                auto c = m_paletteColors[i];
                m_uiDrawList.AddFillRect({ bx, by, 26.0f, 26.0f }, { c.r, c.g, c.b, 255 });
                // Обводка выбранного цвета
                if (m_brushColor.r == c.r && m_brushColor.g == c.g && m_brushColor.b == c.b) {
                    m_uiDrawList.AddFillRect({ bx - 2, by - 2, 30.0f, 30.0f }, accent); // Фон под рамку
                    m_uiDrawList.AddFillRect({ bx, by, 26.0f, 26.0f }, { c.r, c.g, c.b, 255 });
                }
            }

            // 2. Палитра префабов (Объектов)
            m_uiDrawList.AddText({ rightPanelX + 12.0f, TopBarHeight + 105.0f }, "ENTITY PREFABS", text, 11.0f);
            float ey = TopBarHeight + 120.0f;

            m_uiDrawList.AddFillRect({ rightPanelX + 12.0f, ey, 70.0f, 30.0f }, m_selectedEntityType == EntityType::Player ? accent : border);
            m_uiDrawList.AddText({ rightPanelX + 20.0f, ey + 8.0f }, "Player", text, 12.0f);

            m_uiDrawList.AddFillRect({ rightPanelX + 92.0f, ey, 70.0f, 30.0f }, m_selectedEntityType == EntityType::Crate ? accent : border);
            m_uiDrawList.AddText({ rightPanelX + 100.0f, ey + 8.0f }, "Crate", text, 12.0f);

            m_uiDrawList.AddFillRect({ rightPanelX + 172.0f, ey, 70.0f, 30.0f }, m_selectedEntityType == EntityType::Tree ? accent : border);
            m_uiDrawList.AddText({ rightPanelX + 180.0f, ey + 8.0f }, "Tree", text, 12.0f);

            // 3. Инспектор
            m_uiDrawList.AddText({ rightPanelX + 12.0f, TopBarHeight + 170.0f }, "INSPECTOR", text, 11.0f);
            m_uiDrawList.AddLine({ rightPanelX, TopBarHeight + 190.0f }, { rightPanelX + rightPanelWidth, TopBarHeight + 190.0f }, border);

            float insY = TopBarHeight + 200.0f;

            if (!m_selectedEntityIds.empty())
            {
                const float inspectorX = rightPanelX + 12.0f;

                if (m_selectedEntityIds.size() == 1)
                {
                    const Core::Entity* selected = m_scene.FindEntity(m_selectedEntityIds[0]);
                    if (selected) {
                        int gridX = (int)(selected->transform.position.x / CellSize);
                        int gridY = (int)(selected->transform.position.y / CellSize);
                        m_uiDrawList.AddText({ inspectorX, insY }, "Cell (" + std::to_string(gridX) + ", " + std::to_string(gridY) + ")", accent, 14.0f);

                        // Рисуем строки свойств
                        auto DrawPropertyRow = [&](const std::string& label, float val, float y, Kjarn::UI::UIColor col) {
                            m_uiDrawList.AddText({ inspectorX, y }, label, col, 12.0f);
                            // Кнопка [-]
                            m_uiDrawList.AddFillRect({ inspectorX + 80.0f, y - 2.0f, 16.0f, 16.0f }, border);
                            m_uiDrawList.AddText({ inspectorX + 85.0f, y }, "-", text, 12.0f);
                            // Значение
                            m_uiDrawList.AddText({ inspectorX + 105.0f, y }, std::to_string((int)val), secondaryText, 12.0f);
                            // Кнопка [+]
                            m_uiDrawList.AddFillRect({ inspectorX + 145.0f, y - 2.0f, 16.0f, 16.0f }, border);
                            m_uiDrawList.AddText({ inspectorX + 150.0f, y }, "+", text, 12.0f);
                            };

                        DrawPropertyRow("X:", selected->transform.position.x, insY + 40.0f, secondaryText);
                        DrawPropertyRow("Y:", selected->transform.position.y, insY + 65.0f, secondaryText);
                        DrawPropertyRow("Width:", selected->width, insY + 90.0f, secondaryText);
                        DrawPropertyRow("Height:", selected->height, insY + 115.0f, secondaryText);
                        DrawPropertyRow("Z-Index:", selected->zIndex, insY + 140.0f, secondaryText);
                    }
                }
                else
                {
                    m_uiDrawList.AddText({ inspectorX, insY }, std::to_string(m_selectedEntityIds.size()) + " objects selected", accent, 14.0f);
                }

                // ОБЩИЙ ЧЕКБОКС ДЛЯ ВСЕХ ВЫДЕЛЕННЫХ
                float cbY = insY + 155.0f;
                m_uiDrawList.AddText({ inspectorX, cbY + 2.0f }, "Is Static (Ground):", text, 12.0f);

                float cbX = inspectorX + 120.0f;
                float cbSize = 14.0f;
                m_uiDrawList.AddFillRect({ cbX, cbY, cbSize, cbSize }, border);

                bool allStatic = true;
                for (auto id : m_selectedEntityIds) {
                    auto* e = m_scene.FindEntity(id);
                    if (e && !e->isStatic) { allStatic = false; break; }
                }
                if (allStatic) {
                    m_uiDrawList.AddFillRect({ cbX + 2.0f, cbY + 2.0f, cbSize - 4.0f, cbSize - 4.0f }, accent);
                }
            }
            else
            {
                m_uiDrawList.AddText({ rightPanelX + 12.0f, insY }, "No object selected", secondaryText, 12.0f);
            }


            m_uiDrawList.AddFillRect({ 0.0f, height - BottomBarHeight, width, BottomBarHeight }, panelHeader);
            m_uiDrawList.AddLine({ 0.0f, height - BottomBarHeight }, { width, height - BottomBarHeight }, border);
            m_uiDrawList.AddText({ 12.0f, height - 18.0f }, !m_selectedEntityIds.empty() ? "OBJECTS SELECTED: " + std::to_string(m_selectedEntityIds.size()) : "READY", accent, 11.0f);
            m_uiDrawList.AddText({ width - 150.0f, height - 18.0f }, "Kjarn Sandbox", secondaryText, 11.0f);
        }
    }

    // Хелпер для отрисовки кнопок с Hover-эффектом
    void DrawButton(UIRect rect, const std::string& text, bool isActive) {
        float mx, my;
        Core::Input::GetMousePosition(mx, my);

        Kjarn::UI::UIColor bgColor = { 65, 69, 78, 255 }; // Обычный фон (border)
        if (isActive) bgColor = { 85, 145, 220, 255 };    // Активный (accent)
        if (rect.Contains(mx, my)) {
            // Если мышь наведена, делаем цвет светлее на 20%
            bgColor.r = std::min(255, bgColor.r + 30);
            bgColor.g = std::min(255, bgColor.g + 30);
            bgColor.b = std::min(255, bgColor.b + 30);
        }

        m_uiDrawList.AddFillRect({ rect.x, rect.y, rect.w, rect.h }, bgColor);

        // Центрируем текст (грубо, по 6 пикселей на символ)
        float textW = text.length() * 6.0f;
        float textX = rect.x + (rect.w - textW) / 2.0f;
        m_uiDrawList.AddText({ textX, rect.y + 6.0f }, text, { 220, 223, 230, 255 }, 12.0f);
    }

    void ModifySelectedEntities(float dx, float dy, float dw, float dh) {
        for (auto id : m_selectedEntityIds) {
            Core::Entity* e = m_scene.FindEntity(id);
            if (e) {
                e->transform.position.x += dx;
                e->transform.position.y += dy;
                e->width += dw;
                e->height += dh;
                // Защита от отрицательных размеров
                if (e->width < 10.0f) e->width = 10.0f;
                if (e->height < 10.0f) e->height = 10.0f;
            }
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