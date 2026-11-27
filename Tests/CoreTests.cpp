// Тест платформо-независимой части движка:
//   Tilemap, Scene, BrushTool, EditorHistory, SceneSerializer.
//
// Запуск (см. Tests/CMakeLists.txt):
//   cmake -S . -B build-tests -DKJARN_BUILD_TESTS=ON
//   cmake --build build-tests
//   ctest --test-dir build-tests --output-on-failure

#include "Core/Scene/Scene.h"
#include "Core/Scene/SceneSerializer.h"
#include "Core/Tilemap.h"

#include "Sandbox/Editor/History/EditorHistory.h"
#include "Sandbox/Editor/Tools/BrushTool.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
    int g_failures = 0;

    void Check(bool condition, const char* what, int line)
    {
        if (condition)
            return;

        std::fprintf(stderr, "FAIL (line %d): %s\n", line, what);
        ++g_failures;
    }

#define CHECK(expression) Check((expression), #expression, __LINE__)

    // Имитация кадра редактора: Begin → Continue → End → фиксация мазка.
    // Возвращает false, если мазок пуст и в историю ничего не попало.
    bool RunBrushFrame(
        Kjarn::Editor::BrushTool& brush,
        Core::Tilemap& map,
        Kjarn::Editor::EditorHistory& history,
        Kjarn::Editor::BrushMode mode,
        const Core::Color& color,
        int cellX,
        int cellY)
    {
        brush.Begin(mode, color);
        brush.Continue(map, cellX, cellY);
        brush.End();

        Kjarn::Editor::BrushTool::Stroke stroke;

        if (!brush.TryTakeStroke(stroke))
            return false;

        history.PushTileStroke(stroke);
        return true;
    }

    void TestPaintSingleCell()
    {
        Core::Tilemap map;
        Kjarn::Editor::BrushTool brush;

        brush.Begin(Kjarn::Editor::BrushMode::Paint, Core::Color(10, 20, 30));
        brush.Continue(map, 3, -2);
        brush.End();

        CHECK(map.GetTile(3, -2) == Core::Tilemap::SolidTile);
        CHECK(map.GetColor(3, -2) == Core::Color(10, 20, 30));

        Kjarn::Editor::BrushTool::Stroke stroke;
        CHECK(brush.TryTakeStroke(stroke));
        CHECK(stroke.edits.size() == 1);
        CHECK(stroke.edits[0].cell.x == 3);
        CHECK(stroke.edits[0].cell.y == -2);
        CHECK(stroke.edits[0].beforeTile == Core::Tilemap::EmptyTile);
        CHECK(stroke.edits[0].afterTile == Core::Tilemap::SolidTile);
    }

    void TestStrokeConnectsFastMotion()
    {
        Core::Tilemap map;
        Kjarn::Editor::BrushTool brush;

        brush.Begin(Kjarn::Editor::BrushMode::Paint, Core::Color(200, 100, 50));
        brush.Continue(map, 0, 0);
        brush.Continue(map, 5, 0); // резкий скачок мыши
        brush.End();

        for (int x = 0; x <= 5; ++x)
            CHECK(map.GetTile(x, 0) == Core::Tilemap::SolidTile);
    }

    void TestEraseDoesNotDuplicateHistory()
    {
        Core::Tilemap map;
        Kjarn::Editor::BrushTool brush;

        brush.Begin(Kjarn::Editor::BrushMode::Paint, Core::Color(1, 2, 3));
        brush.Continue(map, 0, 0);
        brush.End();

        Kjarn::Editor::BrushTool::Stroke paintStroke;
        CHECK(brush.TryTakeStroke(paintStroke));

        // Ластик по пустому месту: мазок пуст, в историю не попадает.
        brush.Begin(Kjarn::Editor::BrushMode::Erase, Core::Color{});
        brush.Continue(map, 50, 50);
        brush.End();

        Kjarn::Editor::BrushTool::Stroke emptyStroke;
        CHECK(!brush.TryTakeStroke(emptyStroke));

        // Ластик по нарисованному: ровно одна правка.
        brush.Begin(Kjarn::Editor::BrushMode::Erase, Core::Color{});
        brush.Continue(map, 0, 0);
        brush.End();

        Kjarn::Editor::BrushTool::Stroke eraseStroke;
        CHECK(brush.TryTakeStroke(eraseStroke));
        CHECK(eraseStroke.edits.size() == 1);
        CHECK(eraseStroke.edits[0].beforeTile == Core::Tilemap::SolidTile);
        CHECK(map.GetTile(0, 0) == Core::Tilemap::EmptyTile);
    }

    void TestUndoRedo()
    {
        Core::Tilemap map;
        Kjarn::Editor::BrushTool brush;
        Kjarn::Editor::EditorHistory history;

        brush.Begin(Kjarn::Editor::BrushMode::Paint, Core::Color(200, 100, 50));
        brush.Continue(map, 1, 1);
        brush.Continue(map, 2, 1);
        brush.End();

        Kjarn::Editor::BrushTool::Stroke stroke;
        CHECK(brush.TryTakeStroke(stroke));
        history.PushTileStroke(stroke);

        CHECK(history.CanUndo());
        CHECK(!history.CanRedo());

        CHECK(history.Undo(map));
        CHECK(map.GetTile(1, 1) == Core::Tilemap::EmptyTile);
        CHECK(map.GetTile(2, 1) == Core::Tilemap::EmptyTile);

        CHECK(history.Redo(map));
        CHECK(map.GetTile(1, 1) == Core::Tilemap::SolidTile);
        CHECK(map.GetTile(2, 1) == Core::Tilemap::SolidTile);
        CHECK(map.GetColor(2, 1) == Core::Color(200, 100, 50));
    }

    void TestUndoRestoresPreviousColor()
    {
        Core::Tilemap map;
        Kjarn::Editor::BrushTool brush;
        Kjarn::Editor::EditorHistory history;

        // Первый мазок — красный.
        brush.Begin(Kjarn::Editor::BrushMode::Paint, Core::Color(255, 0, 0));
        brush.Continue(map, 0, 0);
        brush.End();

        Kjarn::Editor::BrushTool::Stroke first;
        CHECK(brush.TryTakeStroke(first));
        history.PushTileStroke(first);

        // Второй мазок по той же клетке — зелёный.
        brush.Begin(Kjarn::Editor::BrushMode::Paint, Core::Color(0, 255, 0));
        brush.Continue(map, 0, 0);
        brush.End();

        Kjarn::Editor::BrushTool::Stroke second;
        CHECK(brush.TryTakeStroke(second));
        history.PushTileStroke(second);

        CHECK(map.GetColor(0, 0) == Core::Color(0, 255, 0));

        CHECK(history.Undo(map));
        CHECK(map.GetColor(0, 0) == Core::Color(255, 0, 0));
        CHECK(map.GetTile(0, 0) == Core::Tilemap::SolidTile);
    }

    void TestTilemapNegativeCoordinates()
    {
        Core::Tilemap map;

        map.SetTile(-5, -7, Core::Tilemap::SolidTile, Core::Color(9, 9, 9));
        map.SetTile(3, 2, Core::Tilemap::SolidTile, Core::Color(7, 7, 7));

        CHECK(map.GetTile(-5, -7) == Core::Tilemap::SolidTile);
        CHECK(map.GetColor(-5, -7) == Core::Color(9, 9, 9));
        CHECK(map.GetTile(3, 2) == Core::Tilemap::SolidTile);

        // Чтение за пределами — пусто, без падения.
        CHECK(map.GetTile(1000, 1000) == Core::Tilemap::EmptyTile);
        CHECK(map.GetTile(-1000, -1000) == Core::Tilemap::EmptyTile);
    }


    void TestSingleClickPainting()
    {
        // Быстрый клик: кнопка нажата и отпущена между кадрами —
        // мазок должен начаться с первого же нажатия.
        Core::Tilemap map;
        Kjarn::Editor::BrushTool brush;
        Kjarn::Editor::EditorHistory history;

        CHECK(RunBrushFrame(brush, map, history,
            Kjarn::Editor::BrushMode::Paint, Core::Color(5, 5, 5), 7, 9));

        CHECK(map.GetTile(7, 9) == Core::Tilemap::SolidTile);

        brush.Begin(Kjarn::Editor::BrushMode::Paint, Core::Color(5, 5, 5));
        brush.Continue(map, 7, 9);
        brush.End();

        Kjarn::Editor::BrushTool::Stroke repeated;
        CHECK(!brush.TryTakeStroke(repeated)); // мазок не начат — правок нет
    }

    void TestDiagonalStrokeHasNoGaps()
    {
        Core::Tilemap map;
        Kjarn::Editor::BrushTool brush;

        brush.Begin(Kjarn::Editor::BrushMode::Paint, Core::Color(7, 7, 7));
        brush.Continue(map, 0, 0);
        brush.Continue(map, 6, 4);
        brush.End();

        // Каждый шаг по большой оси должен быть занят (линия без дырок).
        for (int x = 0; x <= 6; ++x)
        {
            bool columnHasTile = false;

            for (int y = 0; y <= 4; ++y)
            {
                if (map.GetTile(x, y) == Core::Tilemap::SolidTile)
                    columnHasTile = true;
            }

            CHECK(columnHasTile);
        }
    }

    void TestUndoAfterEraserRestoresColor()
    {
        Core::Tilemap map;
        Kjarn::Editor::BrushTool brush;
        Kjarn::Editor::EditorHistory history;

        map.SetTile(2, 2, Core::Tilemap::SolidTile, Core::Color(123, 45, 67));

        CHECK(RunBrushFrame(brush, map, history,
            Kjarn::Editor::BrushMode::Erase, Core::Color{}, 2, 2));

        CHECK(map.GetTile(2, 2) == Core::Tilemap::EmptyTile);

        CHECK(history.Undo(map));
        CHECK(map.GetTile(2, 2) == Core::Tilemap::SolidTile);
        CHECK(map.GetColor(2, 2) == Core::Color(123, 45, 67));
    }

    void TestSceneCopyIsDeep()
    {
        Core::Scene scene;
        const Core::EntityId id = scene.CreateEntity();

        Core::Entity* entity = scene.FindEntity(id);
        CHECK(entity != nullptr);
        if (entity != nullptr)
        {
            entity->transform.position.x = 42.0f;
            entity->isPlayer = true;
        }

        scene.GetTilemap().SetTile(0, 0, Core::Tilemap::SolidTile, Core::Color(1, 1, 1));

        Core::Scene copy;
        copy.CopyFrom(scene);

        // Правки копии не должны влиять на оригинал.
        Core::Entity* copied = copy.FindEntity(id);
        CHECK(copied != nullptr);
        if (copied != nullptr)
        {
            copied->transform.position.x = 100.0f;
            CHECK(scene.FindEntity(id)->transform.position.x == 42.0f);
            CHECK(scene.FindEntity(id)->isPlayer);
        }

        copy.GetTilemap().SetTile(0, 0, Core::Tilemap::EmptyTile, Core::Color{});
        CHECK(scene.GetTilemap().GetTile(0, 0) == Core::Tilemap::SolidTile);

        // Сущности, созданные после копирования, не конфликтуют по id.
        const Core::EntityId newId = copy.CreateEntity();
        CHECK(newId != id);
        CHECK(scene.FindEntity(newId) == nullptr);
    }

    void TestTilemapBoundsRestoredExactly()
    {
        Core::Scene scene;
        Core::Tilemap& map = scene.GetTilemap();

        // Карта с отрицательным началом и пустым правым/верхним полем.
        map.Resize(-4, -3, 20, 15);
        map.SetTile(-4, -3, Core::Tilemap::SolidTile, Core::Color(1, 2, 3));
        map.SetTile(15, 11, Core::Tilemap::SolidTile, Core::Color(4, 5, 6));

        const std::string path = "kjarn_bounds_test.kjarn";
        CHECK(Core::SceneSerializer::Save(scene, path));

        Core::Scene loaded;
        CHECK(Core::SceneSerializer::Load(path, loaded));
        std::remove(path.c_str());

        const Core::Tilemap& loadedMap = loaded.GetTilemap();

        CHECK(loadedMap.GetOriginX() == -4);
        CHECK(loadedMap.GetOriginY() == -3);
        CHECK(loadedMap.GetWidth() == 20);
        CHECK(loadedMap.GetHeight() == 15);
        CHECK(loadedMap.GetColor(-4, -3) == Core::Color(1, 2, 3));
        CHECK(loadedMap.GetColor(15, 11) == Core::Color(4, 5, 6));
    }

    void TestSaveLoadRoundTrip()
    {
        Core::Scene scene;

        const Core::EntityId playerId = scene.CreateEntity();
        Core::Entity* player = scene.FindEntity(playerId);
        if (player != nullptr)
        {
            player->transform.position.x = 12.5f;
            player->transform.position.y = -7.25f;
            player->width = 40.0f;
            player->height = 60.0f;
            player->color = Core::Color(50, 150, 255);
            player->isPlayer = true;
            player->isStatic = false;
        }

        Core::Tilemap& map = scene.GetTilemap();
        map.SetTile(0, 0, Core::Tilemap::SolidTile, Core::Color(200, 100, 50));
        map.SetTile(1, 0, Core::Tilemap::SolidTile, Core::Color(200, 100, 50));
        map.SetTile(-3, 4, Core::Tilemap::SolidTile, Core::Color(11, 22, 33));

        const std::string path = "kjarn_roundtrip_test.kjarn";

        CHECK(Core::SceneSerializer::Save(scene, path));

        Core::Scene loaded;
        CHECK(Core::SceneSerializer::Load(path, loaded));

        std::remove(path.c_str());

        CHECK(loaded.GetEntityCount() == scene.GetEntityCount());

        const Core::Entity* loadedPlayer = loaded.FindEntity(playerId);
        CHECK(loadedPlayer != nullptr);
        if (loadedPlayer != nullptr)
        {
            CHECK(loadedPlayer->transform.position.x == 12.5f);
            CHECK(loadedPlayer->transform.position.y == -7.25f);
            CHECK(loadedPlayer->isPlayer);
        }

        const Core::Tilemap& loadedMap = loaded.GetTilemap();
        CHECK(loadedMap.GetTile(0, 0) == Core::Tilemap::SolidTile);
        CHECK(loadedMap.GetTile(1, 0) == Core::Tilemap::SolidTile);
        CHECK(loadedMap.GetTile(-3, 4) == Core::Tilemap::SolidTile);
        CHECK(loadedMap.GetColor(-3, 4) == Core::Color(11, 22, 33));
        CHECK(loadedMap.GetTile(5, 5) == Core::Tilemap::EmptyTile);
    }

    void TestLoadRejectsCorruptFile()
    {
        const std::string path = "kjarn_corrupt_test.kjarn";

        {
            std::FILE* file = std::fopen(path.c_str(), "wb");
            CHECK(file != nullptr);

            if (file != nullptr)
            {
                const char* content = "KJARN_SCENE_V3\nENTITY_COUNT 1\nENTITY broken\n";
                std::fwrite(content, 1, std::char_traits<char>::length(content), file);
                std::fclose(file);
            }
        }

        Core::Scene target;
        const Core::EntityId marker = target.CreateEntity();
        (void)marker;

        const std::size_t before = target.GetEntityCount();

        CHECK(!Core::SceneSerializer::Load(path, target));
        CHECK(target.GetEntityCount() == before);

        std::remove(path.c_str());
    }

    void TestV2WithoutTilemapStillLoads()
    {
        const std::string path = "kjarn_v2_test.kjarn";

        {
            std::FILE* file = std::fopen(path.c_str(), "wb");
            CHECK(file != nullptr);

            if (file != nullptr)
            {
                const char* content =
                    "KJARN_SCENE_V2\n"
                    "ENTITY_COUNT 1\n"
                    "ENTITY 7 RECT 10 20 0 0 0 0 1 1 1 90 90 80 80 80 1 0 0\n";

                std::fwrite(content, 1, std::char_traits<char>::length(content), file);
                std::fclose(file);
            }
        }

        Core::Scene loaded;
        CHECK(Core::SceneSerializer::Load(path, loaded));
        CHECK(loaded.GetEntityCount() == 1);
        CHECK(loaded.GetTilemap().IsEmpty());

        std::remove(path.c_str());
    }
}

int main()
{
    TestPaintSingleCell();
    TestStrokeConnectsFastMotion();
    TestEraseDoesNotDuplicateHistory();
    TestUndoRedo();
    TestUndoRestoresPreviousColor();
    TestTilemapNegativeCoordinates();
    TestSingleClickPainting();
    TestDiagonalStrokeHasNoGaps();
    TestUndoAfterEraserRestoresColor();
    TestSceneCopyIsDeep();
    TestTilemapBoundsRestoredExactly();
    TestSaveLoadRoundTrip();
    TestLoadRejectsCorruptFile();
    TestV2WithoutTilemapStillLoads();

    if (g_failures == 0)
    {
        std::printf("All Kjarn core tests passed.\n");
        return EXIT_SUCCESS;
    }

    std::fprintf(stderr, "%d check(s) failed.\n", g_failures);
    return EXIT_FAILURE;
}
