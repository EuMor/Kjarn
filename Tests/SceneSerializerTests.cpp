// Тесты сериализации сцены: запись V4, чтение V4/V3/V2, отказ без потери данных.
//
// Сборка из корня проекта (без Win32), одной строкой:
//   g++ -std=c++20 -Wall -Wextra -Wpedantic -I Core/include Tests/SceneSerializerTests.cpp Core/src/Scene/Scene.cpp Core/src/Scene/SceneSerializer.cpp -o scene-serializer-tests

#include "Core/Scene/Scene.h"
#include "Core/Scene/SceneSerializer.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    int cases = 0;
    int failures = 0;
    int checks = 0;

    void Require(bool condition, const std::string& message)
    {
        ++checks;

        if (!condition)
            throw std::runtime_error(message);
    }

    void Check(bool condition, const char* expression, int line)
    {
        Require(condition, "line " + std::to_string(line) + ": " + expression);
    }

#define CHECK(expression) Check((expression), #expression, __LINE__)

    template <class F>
    void Test(const char* name, F&& run)
    {
        ++cases;

        try
        {
            run();
            std::cout << "PASS: " << name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failures;
            std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
        }
    }

    // ----------------------------------------------------------------------
    // Реальные файлы, записанные старыми сериализаторами из истории проекта.
    // ----------------------------------------------------------------------

    // Записан сериализатором V2 из коммита 8394e5c.
    const std::string FixtureV2 = R"KJARN(KJARN_SCENE_V2
ENTITY_COUNT 3
ENTITY 7 RECT 12.5 -7.25 3 0 0 45 2 0.5 1 40 60 50 150 255 0 0 1
ENTITY 42 CIRCLE 100 200 0 0 0 0 1 1 1 80 80 150 80 50 1 0 0
ENTITY 1000 TRIANGLE -300 0.100000001 0 0 0 0 1 1 1 60 100 50 150 50 0 1 0
)KJARN";

    // Записан сериализатором V3 из master (747cea4).
    const std::string FixtureV3 = R"KJARN(KJARN_SCENE_V3
TILE 0 -3 255 0 128
TILE 2 0 40 50 60
TILE -1 0 10 20 30
ENTITY RECT 12.5 -7.25 40 60 50 150 255
ENTITY CIRCLE 100 200 80 80 150 80 50
ENTITY TRIANGLE -300 0.1 60 100 50 150 50
)KJARN";

    // ----------------------------------------------------------------------
    // Помощники
    // ----------------------------------------------------------------------

    std::string ReadFile(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    void WriteFile(const fs::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);

        file.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    fs::path TempDir()
    {
        return fs::temp_directory_path() / "kjarn_scene_serializer_tests";
    }

    bool SameVector(const Core::Vector3& a, const Core::Vector3& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    bool SameColor(const Core::Color& a, const Core::Color& b)
    {
        return a.r == b.r && a.g == b.g && a.b == b.b;
    }

    bool SameEntity(const Core::Entity& a, const Core::Entity& b)
    {
        return a.id == b.id &&
            SameVector(a.transform.position, b.transform.position) &&
            SameVector(a.transform.rotation, b.transform.rotation) &&
            SameVector(a.transform.scale, b.transform.scale) &&
            a.width == b.width && a.height == b.height &&
            SameColor(a.color, b.color) && a.shape == b.shape &&
            a.isPlayer == b.isPlayer && a.isStatic == b.isStatic &&
            a.useGravity == b.useGravity;
    }

    bool SameScene(const Core::Scene& a, const Core::Scene& b)
    {
        if (a.GetEntityCount() != b.GetEntityCount())
            return false;

        for (std::size_t i = 0; i < a.GetEntities().size(); ++i)
        {
            if (!SameEntity(a.GetEntities()[i], b.GetEntities()[i]))
                return false;
        }

        const auto& tilesA = a.GetTilemap().GetTiles();
        const auto& tilesB = b.GetTilemap().GetTiles();

        if (tilesA.size() != tilesB.size())
            return false;

        for (const auto& [key, tile] : tilesA)
        {
            const auto found = tilesB.find(key);

            if (found == tilesB.end() || !SameColor(found->second.color, tile.color))
                return false;
        }

        return true;
    }

    // Независимая от сериализатора проверка того, что в сцене нет мусора.
    bool IsSane(const Core::Scene& scene)
    {
        std::set<std::uint64_t> ids;

        const auto finite3 = [](const Core::Vector3& v)
            {
                return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
            };

        for (const Core::Entity& e : scene.GetEntities())
        {
            if (e.id.value == 0 || e.id.value == std::numeric_limits<std::uint64_t>::max())
                return false;

            if (!ids.insert(e.id.value).second)
                return false;

            if (!finite3(e.transform.position) || !finite3(e.transform.rotation) ||
                !finite3(e.transform.scale))
                return false;

            if (!std::isfinite(e.width) || !std::isfinite(e.height) ||
                e.width <= 0.0f || e.height <= 0.0f)
                return false;
        }

        return true;
    }

    bool IsPlainMessage(const std::string& message)
    {
        if (message.empty() || message.size() > 300)
            return false;

        return std::all_of(message.begin(), message.end(), [](char c)
            {
                return c >= 0x20 && c < 0x7F;
            });
    }

    Core::Entity* AddEntity(
        Core::Scene& scene,
        std::uint64_t id,
        Core::ShapeType shape,
        float x, float y, float width, float height,
        Core::Color color)
    {
        Core::Entity* entity = scene.CreateEntityWithId(Core::EntityId{ id });

        Require(entity != nullptr, "AddEntity: id rejected by Scene");

        entity->shape = shape;
        entity->transform.position.x = x;
        entity->transform.position.y = y;
        entity->width = width;
        entity->height = height;
        entity->color = color;

        return entity;
    }

    void AddTile(Core::Scene& scene, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b)
    {
        scene.GetTilemap().SetTile(x, y, Core::Tile{ Core::Color(r, g, b) });
    }

    // Сцена, соответствующая GoldenText. Тайлы добавлены не по порядку записи.
    Core::Scene MakeGoldenScene()
    {
        Core::Scene scene;

        AddTile(scene, 2, 0, 40, 50, 60);
        AddTile(scene, 0, 1, 255, 0, 128);
        AddTile(scene, -1, 0, 10, 20, 30);

        Core::Entity* player = AddEntity(scene, 7, Core::ShapeType::Rectangle,
            12.5f, -7.25f, 40.0f, 60.0f, Core::Color(50, 150, 255));
        player->isPlayer = true;

        Core::Entity* crate = AddEntity(scene, 42, Core::ShapeType::Circle,
            100.0f, 200.0f, 80.0f, 80.0f, Core::Color(150, 80, 50));
        crate->isStatic = true;

        Core::Entity* tree = AddEntity(scene, 1000, Core::ShapeType::Triangle,
            -300.0f, 0.1f, 60.0f, 100.0f, Core::Color(50, 150, 50));
        tree->transform.rotation = { 0.0f, 0.0f, 45.0f };
        tree->transform.scale = { 2.0f, 0.5f, 1.0f };
        tree->useGravity = true;

        return scene;
    }

    // Так выглядит файл V4: порядок записей, имена ключей и числа — часть формата.
    const std::string GoldenText =
        "KJARN_SCENE_V4\n"
        "TILE -1 0 10 20 30\n"
        "TILE 2 0 40 50 60\n"
        "TILE 0 1 255 0 128\n"
        "ENTITY id=7 shape=RECT pos=12.5,-7.25,0 size=40,60 color=50,150,255 player=1\n"
        "ENTITY id=42 shape=CIRCLE pos=100,200,0 size=80,80 color=150,80,50 static=1\n"
        "ENTITY id=1000 shape=TRIANGLE pos=-300,0.1,0 size=60,100 color=50,150,50 rot=0,0,45 scale=2,0.5,1 gravity=1\n"
        "END tiles=3 entities=3\n";

    // Непустая сцена, которая не должна пострадать от неудачной загрузки.
    Core::Scene MakeSentinelScene()
    {
        Core::Scene scene;

        AddTile(scene, 1, 1, 9, 9, 9);
        AddTile(scene, -4, 2, 1, 2, 3);

        Core::Entity* entity = AddEntity(scene, 5, Core::ShapeType::Rectangle,
            1.0f, 2.0f, 3.0f, 4.0f, Core::Color(5, 6, 7));
        entity->isPlayer = true;

        return scene;
    }

    std::string ToCrlf(const std::string& text)
    {
        std::string result;

        for (const char c : text)
        {
            if (c == '\n')
                result += '\r';

            result += c;
        }

        return result;
    }

    // ----------------------------------------------------------------------
    // Некорректные входы: каждый должен быть отвергнут с понятной причиной.
    // ----------------------------------------------------------------------

    struct BadCase
    {
        std::string name;
        std::string text;
        std::string expected;   // фрагмент сообщения об ошибке
    };

    std::vector<BadCase> MakeBadCases()
    {
        const std::string v4 = "KJARN_SCENE_V4\n";
        const std::string v3 = "KJARN_SCENE_V3\n";
        const std::string v2 = "KJARN_SCENE_V2\n";
        const std::string v2Entity = "ENTITY 7 RECT 12.5 -7.25 3 0 0 45 2 0.5 1 40 60 50 150 255 0 0 1\n";

        return {
            // --- заголовок и обрезанные файлы
            { "empty file", "", "empty" },
            { "only a BOM", "\xEF\xBB\xBF", "empty" },
            { "only a newline", "\n", "expected KJARN_SCENE" },
            { "foreign header", "NOT_A_SCENE\n", "expected KJARN_SCENE" },
            { "future version", "KJARN_SCENE_V9\nEND tiles=0 entities=0\n", "expected KJARN_SCENE" },
            { "header with extra word", "KJARN_SCENE_V4 extra\nEND tiles=0 entities=0\n", "expected KJARN_SCENE" },
            { "blank line before header", "\nKJARN_SCENE_V4\nEND tiles=0 entities=0\n", "expected KJARN_SCENE" },
            { "V4 without END", v4, "missing END" },
            { "V4 cut before END", GoldenText.substr(0, GoldenText.rfind("END")), "missing END" },
            { "V4 cut in the middle of a line", GoldenText.substr(0, GoldenText.rfind("END") - 20), "" },

            // --- TILE
            { "TILE too few values", v4 + "TILE 1 2 3 4\n", "TILE expects" },
            { "TILE too many values", v4 + "TILE 1 2 3 4 5 6\n", "TILE expects" },
            { "TILE letters", v4 + "TILE a 2 3 4 5\n", "integers" },
            { "TILE fractional cell", v4 + "TILE 1.5 2 3 4 5\n", "integers" },
            { "TILE cell out of int range", v4 + "TILE 99999999999 0 1 2 3\n", "integers" },
            { "TILE color 256", v4 + "TILE 1 2 256 0 0\n", "0..255" },
            { "TILE color negative", v4 + "TILE 1 2 -1 0 0\n", "0..255" },
            { "TILE color with plus", v4 + "TILE 1 2 3 4 +5\n", "0..255" },
            { "TILE duplicate cell", v4 + "TILE 1 2 3 4 5\nTILE 1 2 9 9 9\n", "duplicate TILE" },

            // --- ENTITY (V4)
            { "ENTITY without anything", v4 + "ENTITY\n", "missing id" },
            { "ENTITY without id", v4 + "ENTITY shape=RECT\n", "missing id" },
            { "ENTITY id zero", v4 + "ENTITY id=0\n", "invalid id" },
            { "ENTITY id negative", v4 + "ENTITY id=-1\n", "bad value" },
            { "ENTITY id letters", v4 + "ENTITY id=abc\n", "bad value" },
            { "ENTITY id max uint64", v4 + "ENTITY id=18446744073709551615\n", "invalid id" },
            { "ENTITY id overflow", v4 + "ENTITY id=18446744073709551616\n", "bad value" },
            { "ENTITY duplicate id", v4 + "ENTITY id=1\nENTITY id=1\n", "duplicate id" },
            { "ENTITY unknown key", v4 + "ENTITY id=1 colour=1,2,3\n", "unknown key" },
            { "ENTITY duplicate key", v4 + "ENTITY id=1 id=2\n", "duplicate key" },
            { "ENTITY bare word", v4 + "ENTITY id=1 oops\n", "key=value" },
            { "ENTITY unknown shape", v4 + "ENTITY id=1 shape=BLOB\n", "bad value" },
            { "ENTITY shape is case sensitive", v4 + "ENTITY id=1 shape=rect\n", "bad value" },
            { "ENTITY pos two values", v4 + "ENTITY id=1 pos=1,2\n", "bad value" },
            { "ENTITY pos four values", v4 + "ENTITY id=1 pos=1,2,3,4\n", "bad value" },
            { "ENTITY pos nan", v4 + "ENTITY id=1 pos=1,2,nan\n", "bad value" },
            { "ENTITY pos inf", v4 + "ENTITY id=1 pos=inf,0,0\n", "bad value" },
            { "ENTITY pos overflows float", v4 + "ENTITY id=1 pos=1e999,0,0\n", "bad value" },
            { "ENTITY pos empty part", v4 + "ENTITY id=1 pos=1,,3\n", "bad value" },
            { "ENTITY pos empty value", v4 + "ENTITY id=1 pos=\n", "bad value" },
            { "ENTITY rot letters", v4 + "ENTITY id=1 rot=a,b,c\n", "bad value" },
            { "ENTITY scale two values", v4 + "ENTITY id=1 scale=1,1\n", "bad value" },
            { "ENTITY size zero", v4 + "ENTITY id=1 size=0,10\n", "size must be positive" },
            { "ENTITY size negative", v4 + "ENTITY id=1 size=-5,5\n", "size must be positive" },
            { "ENTITY size nan", v4 + "ENTITY id=1 size=nan,1\n", "bad value" },
            { "ENTITY size one value", v4 + "ENTITY id=1 size=10\n", "bad value" },
            { "ENTITY color two values", v4 + "ENTITY id=1 color=1,2\n", "bad value" },
            { "ENTITY color 300", v4 + "ENTITY id=1 color=1,2,300\n", "bad value" },
            { "ENTITY color fractional", v4 + "ENTITY id=1 color=1,2,3.5\n", "bad value" },
            { "ENTITY flag 2", v4 + "ENTITY id=1 player=2\n", "bad value" },
            { "ENTITY flag word", v4 + "ENTITY id=1 static=yes\n", "bad value" },

            // --- END и посторонние записи
            { "END without counts", v4 + "END\n", "END expects" },
            { "END only tiles", v4 + "END tiles=0\n", "END expects" },
            { "END counts in wrong order", v4 + "END entities=0 tiles=0\n", "END expects" },
            { "END negative count", v4 + "END tiles=-1 entities=0\n", "END expects" },
            { "END extra word", v4 + "END tiles=0 entities=0 extra\n", "END expects" },
            { "END claims a tile that is absent", v4 + "END tiles=1 entities=0\n", "END says" },
            { "END claims entity that is absent", v4 + "TILE 0 0 1 2 3\nEND tiles=1 entities=1\n", "END says" },
            { "END twice", v4 + "END tiles=0 entities=0\nEND tiles=0 entities=0\n", "after END" },
            { "record after END", v4 + "END tiles=0 entities=0\nTILE 0 0 1 2 3\n", "after END" },
            { "unknown record", v4 + "FOO 1 2 3\n", "unknown record" },
            { "record names are case sensitive", v4 + "tile 1 2 3 4 5\n", "unknown record" },
            { "NUL inside a record name", v4 + std::string("TILE\0 1 2 3 4\n", 14), "unknown record" },
            { "huge number", v4 + "TILE " + std::string(1000000, '9') + " 0 1 2 3\n", "integers" },

            // --- V3
            { "V3 ENTITY too few values", v3 + "ENTITY RECT 1 2 3 4 5 6\n", "ENTITY expects" },
            { "V3 ENTITY unknown shape", v3 + "ENTITY BLOB 1 2 3 4 5 6 7\n", "unknown shape" },
            { "V3 ENTITY zero width", v3 + "ENTITY RECT 1 2 0 4 5 6 7\n", "size must be positive" },
            { "V3 ENTITY nan", v3 + "ENTITY RECT 1 nan 3 4 5 6 7\n", "finite" },
            { "V3 ENTITY color 300", v3 + "ENTITY RECT 1 2 3 4 5 6 300\n", "0..255" },
            { "V3 has no END", v3 + "END tiles=0 entities=0\n", "unknown record" },

            // --- V2
            { "V2 without ENTITY_COUNT", v2, "missing ENTITY_COUNT" },
            { "V2 count larger than records", v2 + "ENTITY_COUNT 1\n", "ENTITY_COUNT is 1" },
            { "V2 count smaller than records", v2 + "ENTITY_COUNT 0\n" + v2Entity, "ENTITY_COUNT is 0" },
            { "V2 ENTITY before ENTITY_COUNT", v2 + v2Entity + "ENTITY_COUNT 1\n", "must come before" },
            { "V2 count is not a number", v2 + "ENTITY_COUNT x\n", "ENTITY_COUNT expects" },
            { "V2 count twice", v2 + "ENTITY_COUNT 0\nENTITY_COUNT 0\n", "duplicate ENTITY_COUNT" },
            { "V2 has no tiles", v2 + "ENTITY_COUNT 0\nTILE 0 0 1 2 3\n", "unknown record" },
            { "V2 ENTITY missing a value", v2 + "ENTITY_COUNT 1\nENTITY 7 RECT 12.5 -7.25 3 0 0 45 2 0.5 1 40 60 50 150 255 0 0\n", "ENTITY expects 19" },
            { "V2 flag 2", v2 + "ENTITY_COUNT 1\nENTITY 7 RECT 12.5 -7.25 3 0 0 45 2 0.5 1 40 60 50 150 255 0 0 2\n", "flags must be" },
            { "V2 duplicate id", v2 + "ENTITY_COUNT 2\n" + v2Entity + v2Entity, "duplicate id" },
        };
    }
}

int main()
{
    fs::remove_all(TempDir());
    fs::create_directories(TempDir());

    // ======================================================================
    // Формат V4
    // ======================================================================

    Test("V4 text is exactly the documented format", [] {
        std::string text;
        std::string error = "stale";

        CHECK(Core::SceneSerializer::Serialize(MakeGoldenScene(), text, &error));
        CHECK(error.empty());
        CHECK(text == GoldenText);
        });

    Test("V4 golden text loads back into the same scene", [] {
        Core::Scene loaded;
        std::string error;

        CHECK(Core::SceneSerializer::Deserialize(GoldenText, loaded, &error));
        CHECK(SameScene(loaded, MakeGoldenScene()));
        });

    Test("same map written in a different order gives the same file", [] {
        Core::Scene a;
        Core::Scene b;

        for (int i = 0; i < 50; ++i)
            AddTile(a, i % 7 - 3, i / 7, 1, 2, 3);

        for (int i = 49; i >= 0; --i)
            AddTile(b, i % 7 - 3, i / 7, 1, 2, 3);

        std::string textA;
        std::string textB;

        CHECK(Core::SceneSerializer::Serialize(a, textA));
        CHECK(Core::SceneSerializer::Serialize(b, textB));
        CHECK(textA == textB);
        });

    Test("save -> load -> save is byte-identical", [] {
        const Core::Scene original = MakeGoldenScene();
        std::string first;
        std::string second;
        Core::Scene loaded;

        CHECK(Core::SceneSerializer::Serialize(original, first));
        CHECK(Core::SceneSerializer::Deserialize(first, loaded));
        CHECK(Core::SceneSerializer::Serialize(loaded, second));
        CHECK(first == second);
        });

    Test("entity order and ids survive; new ids continue after the largest", [] {
        Core::Scene loaded;

        CHECK(Core::SceneSerializer::Deserialize(GoldenText, loaded));
        CHECK(loaded.GetEntities()[0].id.value == 7);
        CHECK(loaded.GetEntities()[1].id.value == 42);
        CHECK(loaded.GetEntities()[2].id.value == 1000);
        CHECK(loaded.CreateEntity().value == 1001);
        });

    Test("player, static and gravity flags survive (V3 lost them)", [] {
        Core::Scene loaded;

        CHECK(Core::SceneSerializer::Deserialize(GoldenText, loaded));
        CHECK(loaded.FindEntity(Core::EntityId{ 7 })->isPlayer);
        CHECK(!loaded.FindEntity(Core::EntityId{ 7 })->isStatic);
        CHECK(loaded.FindEntity(Core::EntityId{ 42 })->isStatic);
        CHECK(loaded.FindEntity(Core::EntityId{ 1000 })->useGravity);
        });

    Test("runtime state is not saved", [] {
        Core::Scene scene;
        Core::Entity* e = AddEntity(scene, 1, Core::ShapeType::Rectangle, 0, 0, 10, 10, Core::Color(1, 2, 3));

        e->velocityX = 123.0f;
        e->velocityY = -456.0f;
        e->isGrounded = true;

        std::string text;
        Core::Scene loaded;

        CHECK(Core::SceneSerializer::Serialize(scene, text));
        CHECK(Core::SceneSerializer::Deserialize(text, loaded));
        CHECK(loaded.GetEntities()[0].velocityX == 0.0f);
        CHECK(loaded.GetEntities()[0].velocityY == 0.0f);
        CHECK(!loaded.GetEntities()[0].isGrounded);
        });

    Test("floats round-trip bit for bit, including the extremes", [] {
        const float values[] = {
            0.1f, -0.0f, 1.0f / 3.0f, 3.14159265f, 123456.789f, 16777216.0f, 1e-30f, -2.5e20f,
            std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::min(), std::numeric_limits<float>::denorm_min(),
        };

        for (const float value : values)
        {
            Core::Scene scene;
            const float positive = value == 0.0f ? 1.0f : std::fabs(value);

            Core::Entity* e = AddEntity(scene, 1, Core::ShapeType::Rectangle, value, value, positive, positive, Core::Color());
            e->transform.position.z = value;

            std::string text;
            Core::Scene loaded;

            Require(Core::SceneSerializer::Serialize(scene, text), "serialize " + std::to_string(value));
            Require(Core::SceneSerializer::Deserialize(text, loaded), "deserialize: " + text);

            const Core::Entity& back = loaded.GetEntities()[0];

            Require(std::bit_cast<std::uint32_t>(back.transform.position.x) == std::bit_cast<std::uint32_t>(value), "pos.x bits for " + text);
            Require(std::bit_cast<std::uint32_t>(back.transform.position.z) == std::bit_cast<std::uint32_t>(value), "pos.z bits for " + text);
            Require(std::bit_cast<std::uint32_t>(back.width) == std::bit_cast<std::uint32_t>(positive), "size bits for " + text);
        }
        });

    Test("extreme tile coordinates and colors", [] {
        Core::Scene scene;
        const int lo = std::numeric_limits<int>::min();
        const int hi = std::numeric_limits<int>::max();

        AddTile(scene, lo, lo, 0, 0, 0);
        AddTile(scene, hi, hi, 255, 255, 255);
        AddTile(scene, lo, hi, 1, 2, 3);
        AddTile(scene, hi, lo, 4, 5, 6);
        AddTile(scene, 0, 0, 7, 8, 9);

        std::string text;
        Core::Scene loaded;

        CHECK(Core::SceneSerializer::Serialize(scene, text));
        CHECK(Core::SceneSerializer::Deserialize(text, loaded));
        CHECK(SameScene(scene, loaded));
        CHECK(loaded.GetTilemap().GetTiles().size() == 5);
        });

    Test("an empty scene is a valid file", [] {
        Core::Scene scene;
        Core::Scene loaded = MakeSentinelScene();
        std::string text;

        CHECK(Core::SceneSerializer::Serialize(scene, text));
        CHECK(text == "KJARN_SCENE_V4\nEND tiles=0 entities=0\n");
        CHECK(Core::SceneSerializer::Deserialize(text, loaded));
        CHECK(loaded.GetEntityCount() == 0);
        CHECK(loaded.GetTilemap().GetTiles().empty());
        });

    Test("loading replaces everything, including the id counter", [] {
        Core::Scene target = MakeSentinelScene();

        target.CreateEntity();
        target.CreateEntity();

        CHECK(Core::SceneSerializer::Deserialize(GoldenText, target));
        CHECK(SameScene(target, MakeGoldenScene()));
        CHECK(target.CreateEntity().value == 1001);
        });

    // ======================================================================
    // Допуски: что формат прощает
    // ======================================================================

    Test("tolerates CRLF, BOM, comments, blank lines, tabs and a missing final newline", [] {
        const Core::Scene golden = MakeGoldenScene();

        std::string commented = "KJARN_SCENE_V4\n# comment\n\n";
        commented += GoldenText.substr(GoldenText.find('\n') + 1);
        commented += "\n# trailing comment\n\n";

        std::string spaced = "  KJARN_SCENE_V4  \n";
        const std::string body = GoldenText.substr(GoldenText.find('\n') + 1);

        for (const char c : body)
            spaced += (c == ' ') ? std::string(" \t  ") : std::string(1, c);

        const std::vector<std::pair<std::string, std::string>> variants = {
            { "CRLF", ToCrlf(GoldenText) },
            { "BOM", "\xEF\xBB\xBF" + GoldenText },
            { "no final newline", GoldenText.substr(0, GoldenText.size() - 1) },
            { "comments and blank lines", commented },
            { "tabs and runs of spaces", spaced },
        };

        for (const auto& [name, text] : variants)
        {
            Core::Scene loaded;
            std::string error;

            Require(Core::SceneSerializer::Deserialize(text, loaded, &error), name + ": " + error);
            Require(SameScene(loaded, golden), name + ": scene differs");
        }
        });

    Test("entity keys may come in any order and all but id are optional", [] {
        Core::Scene loaded;
        std::string error;

        const std::string text =
            "KJARN_SCENE_V4\n"
            "ENTITY color=1,2,3 size=4,5 pos=6,7,8 shape=CIRCLE id=9\n"
            "ENTITY id=3\n"
            "END tiles=0 entities=2\n";

        CHECK(Core::SceneSerializer::Deserialize(text, loaded, &error));

        const Core::Entity& first = loaded.GetEntities()[0];

        CHECK(first.id.value == 9);
        CHECK(first.shape == Core::ShapeType::Circle);
        CHECK(first.transform.position.z == 8.0f);
        CHECK(first.width == 4.0f && first.height == 5.0f);
        CHECK(first.color.b == 3);

        const Core::Entity& defaults = loaded.GetEntities()[1];

        CHECK(defaults.shape == Core::ShapeType::Rectangle);
        CHECK(defaults.width == 1.0f && defaults.height == 1.0f);
        CHECK(defaults.color.r == 255 && defaults.color.g == 255 && defaults.color.b == 255);
        CHECK(!defaults.isPlayer && !defaults.isStatic && !defaults.useGravity);
        });

    // ======================================================================
    // Старые форматы: настоящие файлы из истории проекта
    // ======================================================================

    Test("V3 file written by the real master serializer loads", [] {
        Core::Scene loaded;
        std::string error;

        CHECK(Core::SceneSerializer::Deserialize(FixtureV3, loaded, &error));
        CHECK(error.empty());

        CHECK(loaded.GetTilemap().GetTiles().size() == 3);
        CHECK(loaded.GetTilemap().IsOccupied(0, -3));
        CHECK(loaded.GetTilemap().IsOccupied(2, 0));
        CHECK(loaded.GetTilemap().IsOccupied(-1, 0));
        CHECK(loaded.GetTilemap().GetTile(0, -3)->color.b == 128);
        CHECK(loaded.GetTilemap().GetTile(-1, 0)->color.r == 10);

        // V3 не хранил id: они выдаются 1, 2, 3 в порядке записей.
        CHECK(loaded.GetEntityCount() == 3);

        const Core::Entity& rect = loaded.GetEntities()[0];
        const Core::Entity& circle = loaded.GetEntities()[1];
        const Core::Entity& triangle = loaded.GetEntities()[2];

        CHECK(rect.id.value == 1 && circle.id.value == 2 && triangle.id.value == 3);
        CHECK(rect.shape == Core::ShapeType::Rectangle);
        CHECK(circle.shape == Core::ShapeType::Circle);
        CHECK(triangle.shape == Core::ShapeType::Triangle);
        CHECK(rect.transform.position.x == 12.5f && rect.transform.position.y == -7.25f);
        CHECK(rect.width == 40.0f && rect.height == 60.0f);
        CHECK(rect.color.r == 50 && rect.color.g == 150 && rect.color.b == 255);
        CHECK(triangle.transform.position.y == 0.1f);

        // Игрок и статика в V3 не сохранялись — это невосполнимо.
        CHECK(!rect.isPlayer && !circle.isStatic);

        CHECK(loaded.CreateEntity().value == 4);
        });

    Test("V3 file with Windows line endings loads the same", [] {
        Core::Scene lf;
        Core::Scene crlf;

        CHECK(Core::SceneSerializer::Deserialize(FixtureV3, lf));
        CHECK(Core::SceneSerializer::Deserialize(ToCrlf(FixtureV3), crlf));
        CHECK(SameScene(lf, crlf));
        });

    Test("a V3 scene migrates to V4 without changes", [] {
        Core::Scene fromV3;
        Core::Scene again;
        std::string text;

        CHECK(Core::SceneSerializer::Deserialize(FixtureV3, fromV3));
        CHECK(Core::SceneSerializer::Serialize(fromV3, text));
        CHECK(text.rfind("KJARN_SCENE_V4\n", 0) == 0);
        CHECK(Core::SceneSerializer::Deserialize(text, again));
        CHECK(SameScene(fromV3, again));
        });

    Test("V2 file written by the real V2 serializer loads with ids and flags", [] {
        Core::Scene loaded;
        std::string error;

        CHECK(Core::SceneSerializer::Deserialize(FixtureV2, loaded, &error));
        CHECK(error.empty());
        CHECK(loaded.GetEntityCount() == 3);
        CHECK(loaded.GetTilemap().GetTiles().empty());

        const Core::Entity* player = loaded.FindEntity(Core::EntityId{ 7 });
        const Core::Entity* wall = loaded.FindEntity(Core::EntityId{ 42 });
        const Core::Entity* tree = loaded.FindEntity(Core::EntityId{ 1000 });

        CHECK(player != nullptr && wall != nullptr && tree != nullptr);

        CHECK(player->shape == Core::ShapeType::Rectangle);
        CHECK(player->transform.position.x == 12.5f && player->transform.position.y == -7.25f);
        CHECK(player->transform.position.z == 3.0f);
        CHECK(player->transform.rotation.z == 45.0f);
        CHECK(player->transform.scale.x == 2.0f && player->transform.scale.y == 0.5f);
        CHECK(player->width == 40.0f && player->height == 60.0f);
        CHECK(player->color.b == 255);

        // Порядок флагов в V2: isStatic, useGravity, isPlayer.
        CHECK(player->isPlayer && !player->isStatic && !player->useGravity);
        CHECK(wall->isStatic && !wall->isPlayer && !wall->useGravity);
        CHECK(tree->useGravity && !tree->isStatic && !tree->isPlayer);

        CHECK(wall->shape == Core::ShapeType::Circle);
        CHECK(tree->shape == Core::ShapeType::Triangle);
        CHECK(tree->transform.position.y == 0.1f);   // V2 писал 0.100000001

        CHECK(loaded.CreateEntity().value == 1001);
        });

    Test("a V2 scene migrates to V4 without losing anything", [] {
        Core::Scene fromV2;
        Core::Scene again;
        std::string text;

        CHECK(Core::SceneSerializer::Deserialize(FixtureV2, fromV2));
        CHECK(Core::SceneSerializer::Serialize(fromV2, text));
        CHECK(Core::SceneSerializer::Deserialize(text, again));
        CHECK(SameScene(fromV2, again));
        });

    // ======================================================================
    // Отказ без потерь
    // ======================================================================

    Test("every malformed input is rejected with a clear message and changes nothing", [] {
        const Core::Scene sentinel = MakeSentinelScene();
        const std::vector<BadCase> cases = MakeBadCases();

        for (const BadCase& bad : cases)
        {
            Core::Scene target = sentinel;
            std::string error = "stale";

            const bool accepted = Core::SceneSerializer::Deserialize(bad.text, target, &error);

            Require(!accepted, bad.name + ": was accepted");
            Require(IsPlainMessage(error), bad.name + ": message is not short plain ASCII: " + error.substr(0, 80));
            Require(error.find(bad.expected) != std::string::npos,
                bad.name + ": expected '" + bad.expected + "' in '" + error + "'");
            Require(SameScene(target, sentinel), bad.name + ": scene was modified");

            // Счётчик id тоже не должен сдвинуться.
            Core::Scene probeA = target;
            Core::Scene probeB = sentinel;

            Require(probeA.CreateEntity() == probeB.CreateEntity(), bad.name + ": id counter changed");
        }

        std::cout << "      (" << cases.size() << " malformed inputs)\n";
        });

    Test("parse errors name the offending line", [] {
        Core::Scene target;
        std::string error;

        const std::string text =
            "KJARN_SCENE_V4\n"
            "TILE 0 0 1 2 3\n"
            "# comment\n"
            "\n"
            "TILE 1 0 1 2 999\n";

        CHECK(!Core::SceneSerializer::Deserialize(text, target, &error));
        CHECK(error.rfind("line 5:", 0) == 0);
        });

    Test("works without an error pointer", [] {
        Core::Scene target = MakeSentinelScene();

        CHECK(!Core::SceneSerializer::Deserialize("garbage", target));
        CHECK(Core::SceneSerializer::Deserialize(GoldenText, target));
        });

    Test("Serialize refuses scenes that Load would reject", [] {
        struct Broken
        {
            const char* name;
            const char* expected;
            void (*apply)(Core::Scene&);
        };

        const Broken broken[] = {
            { "id zero", "invalid id", [](Core::Scene& s) { Core::Entity e; e.id.value = 0; s.GetEntities().push_back(e); } },
            { "id max", "invalid id", [](Core::Scene& s) { Core::Entity e; e.id.value = std::numeric_limits<std::uint64_t>::max(); s.GetEntities().push_back(e); } },
            { "duplicate id", "duplicate entity id", [](Core::Scene& s) { s.GetEntities().push_back(s.GetEntities().front()); } },
            { "nan position", "position is not finite", [](Core::Scene& s) { s.GetEntities().front().transform.position.x = std::nanf(""); } },
            { "inf rotation", "rotation is not finite", [](Core::Scene& s) { s.GetEntities().front().transform.rotation.y = std::numeric_limits<float>::infinity(); } },
            { "nan scale", "scale is not finite", [](Core::Scene& s) { s.GetEntities().front().transform.scale.z = std::nanf(""); } },
            { "inf width", "size is not finite", [](Core::Scene& s) { s.GetEntities().front().width = std::numeric_limits<float>::infinity(); } },
            { "zero height", "size must be positive", [](Core::Scene& s) { s.GetEntities().front().height = 0.0f; } },
            { "negative width", "size must be positive", [](Core::Scene& s) { s.GetEntities().front().width = -1.0f; } },
            { "shape outside the enum", "unknown shape", [](Core::Scene& s) { s.GetEntities().front().shape = static_cast<Core::ShapeType>(77); } },
        };

        for (const Broken& item : broken)
        {
            Core::Scene scene = MakeGoldenScene();

            item.apply(scene);

            std::string text = "previous";
            std::string error;

            Require(!Core::SceneSerializer::Serialize(scene, text, &error), std::string(item.name) + ": was accepted");
            Require(error.find(item.expected) != std::string::npos,
                std::string(item.name) + ": expected '" + item.expected + "' in '" + error + "'");
            Require(text == "previous", std::string(item.name) + ": output was touched");
        }
        });

    // ======================================================================
    // Файлы
    // ======================================================================

    Test("Save then Load through a real file", [] {
        const fs::path path = TempDir() / "roundtrip.kjarn";
        const Core::Scene golden = MakeGoldenScene();
        std::string error;

        CHECK(Core::SceneSerializer::Save(golden, path.string(), &error));
        CHECK(error.empty());
        CHECK(fs::exists(path));
        CHECK(!fs::exists(path.string() + ".tmp"));
        CHECK(ReadFile(path) == GoldenText);

        Core::Scene loaded;

        CHECK(Core::SceneSerializer::Load(path.string(), loaded, &error));
        CHECK(SameScene(loaded, golden));
        });

    Test("Save replaces an existing file and a stale .tmp does no harm", [] {
        const fs::path path = TempDir() / "replace.kjarn";

        WriteFile(path, "old contents");
        WriteFile(path.string() + ".tmp", "stale temporary file from a crashed save");

        CHECK(Core::SceneSerializer::Save(MakeGoldenScene(), path.string()));
        CHECK(ReadFile(path) == GoldenText);
        CHECK(!fs::exists(path.string() + ".tmp"));
        });

    Test("a failed Save leaves the previous file byte for byte", [] {
        const fs::path path = TempDir() / "keep.kjarn";

        CHECK(Core::SceneSerializer::Save(MakeGoldenScene(), path.string()));

        const std::string before = ReadFile(path);

        Core::Scene broken = MakeGoldenScene();
        broken.GetEntities()[1].transform.position.x = std::nanf("");

        std::string error;

        CHECK(!Core::SceneSerializer::Save(broken, path.string(), &error));
        CHECK(error.find("entity 42") != std::string::npos);
        CHECK(ReadFile(path) == before);
        CHECK(!fs::exists(path.string() + ".tmp"));
        });

    Test("Save into a missing directory fails cleanly", [] {
        const fs::path path = TempDir() / "no_such_directory" / "scene.kjarn";
        std::string error;

        CHECK(!Core::SceneSerializer::Save(MakeGoldenScene(), path.string(), &error));
        CHECK(error.find("cannot create") != std::string::npos);
        CHECK(!fs::exists(path));
        });

    Test("Save onto a directory fails and cleans up", [] {
        const fs::path path = TempDir() / "a_directory";

        fs::create_directory(path);

        std::string error;

        CHECK(!Core::SceneSerializer::Save(MakeGoldenScene(), path.string(), &error));
        CHECK(!error.empty());
        CHECK(fs::is_directory(path));
        CHECK(!fs::exists(path.string() + ".tmp"));
        });

    Test("Load of a missing file, a directory and an empty file fails and changes nothing", [] {
        const Core::Scene sentinel = MakeSentinelScene();
        const fs::path emptyFile = TempDir() / "empty.kjarn";
        const fs::path directory = TempDir() / "a_directory2";

        WriteFile(emptyFile, "");
        fs::create_directory(directory);

        const std::vector<std::pair<std::string, std::string>> inputs = {
            { (TempDir() / "missing.kjarn").string(), "cannot open" },
            { directory.string(), "cannot open" },
            { emptyFile.string(), "empty" },
        };

        for (const auto& [path, expected] : inputs)
        {
            Core::Scene target = sentinel;
            std::string error;

            Require(!Core::SceneSerializer::Load(path, target, &error), path + ": was accepted");
            Require(error.find(expected) != std::string::npos, path + ": message was '" + error + "'");
            Require(SameScene(target, sentinel), path + ": scene was modified");
        }
        });

    Test("Load of a corrupt file on disk changes nothing", [] {
        const fs::path path = TempDir() / "corrupt.kjarn";
        const Core::Scene sentinel = MakeSentinelScene();

        WriteFile(path, GoldenText.substr(0, GoldenText.size() - 10));

        Core::Scene target = sentinel;
        std::string error;

        CHECK(!Core::SceneSerializer::Load(path.string(), target, &error));
        CHECK(SameScene(target, sentinel));
        CHECK(!error.empty());
        });

    Test("Load reads files with Windows line endings from disk", [] {
        const fs::path path = TempDir() / "crlf.kjarn";

        WriteFile(path, ToCrlf(GoldenText));

        Core::Scene loaded;

        CHECK(Core::SceneSerializer::Load(path.string(), loaded));
        CHECK(SameScene(loaded, MakeGoldenScene()));
        });

    Test("a large scene round-trips", [] {
        Core::Scene scene;

        for (int y = 0; y < 300; ++y)
            for (int x = -150; x < 150; ++x)
                AddTile(scene, x, y, static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), 7);

        for (std::uint64_t id = 1; id <= 1500; ++id)
            AddEntity(scene, id, Core::ShapeType::Circle, static_cast<float>(id), 0.5f, 10.0f, 10.0f, Core::Color(1, 2, 3));

        std::string text;
        Core::Scene loaded;

        CHECK(Core::SceneSerializer::Serialize(scene, text));
        CHECK(Core::SceneSerializer::Deserialize(text, loaded));
        CHECK(SameScene(scene, loaded));
        CHECK(loaded.GetTilemap().GetTiles().size() == 90000);
        });

    // ======================================================================
    // Случайные сцены и случайные повреждения
    // ======================================================================

    Test("random scenes round-trip exactly and re-save identically", [] {
        std::mt19937 rng(20261127);

        const auto randomFloat = [&rng]()
            {
                for (;;)
                {
                    const float value = std::bit_cast<float>(static_cast<std::uint32_t>(rng()));

                    if (std::isfinite(value))
                        return value;
                }
            };

        const auto randomPositive = [&randomFloat]()
            {
                const float value = std::fabs(randomFloat());

                return value > 0.0f ? value : 1.0f;
            };

        for (int trial = 0; trial < 400; ++trial)
        {
            Core::Scene scene;

            const int tiles = static_cast<int>(rng() % 200);

            for (int i = 0; i < tiles; ++i)
            {
                AddTile(scene,
                    static_cast<int>(rng() % 4001) - 2000, static_cast<int>(rng() % 4001) - 2000,
                    static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()));
            }

            if (rng() % 4 == 0)
                AddTile(scene, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 1, 2, 3);

            std::set<std::uint64_t> used;
            const int entities = static_cast<int>(rng() % 30);

            for (int i = 0; i < entities; ++i)
            {
                std::uint64_t id = 0;

                do
                {
                    id = (static_cast<std::uint64_t>(rng()) << 32 | rng()) % (std::numeric_limits<std::uint64_t>::max() - 1) + 1;
                } while (!used.insert(id).second);

                Core::Entity* e = AddEntity(scene, id, static_cast<Core::ShapeType>(rng() % 3),
                    randomFloat(), randomFloat(), randomPositive(), randomPositive(),
                    Core::Color(static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng())));

                e->transform.position.z = randomFloat();

                if (rng() % 2 == 0)
                {
                    e->transform.rotation = { randomFloat(), randomFloat(), randomFloat() };
                    e->transform.scale = { randomFloat(), randomFloat(), randomFloat() };
                }

                e->isPlayer = rng() % 2 == 0;
                e->isStatic = rng() % 2 == 0;
                e->useGravity = rng() % 2 == 0;
            }

            std::string first;
            std::string second;
            std::string error;
            Core::Scene loaded;

            Require(Core::SceneSerializer::Serialize(scene, first, &error), "serialize: " + error);
            Require(Core::SceneSerializer::Deserialize(first, loaded, &error), "deserialize: " + error);
            Require(SameScene(scene, loaded), "scene differs after a round trip (trial " + std::to_string(trial) + ")");
            Require(Core::SceneSerializer::Serialize(loaded, second, &error), "re-serialize: " + error);
            Require(first == second, "second save differs (trial " + std::to_string(trial) + ")");
        }
        });

    Test("random corruption: never crashes; rejected input changes nothing; accepted input is sane", [] {
        std::mt19937 rng(112720261);

        Core::Scene base = MakeGoldenScene();

        for (int i = 0; i < 12; ++i)
            AddTile(base, i, 5, 1, 2, 3);

        std::string baseText;

        Require(Core::SceneSerializer::Serialize(base, baseText), "serialize base");

        const std::string alphabet = "0123456789-.,=eE+ \tabcdefnitplayerscolorsizepos#\n\r";
        const Core::Scene sentinel = MakeSentinelScene();

        const auto splitLines = [](const std::string& text)
            {
                std::vector<std::string> lines;
                std::size_t start = 0;

                while (start <= text.size())
                {
                    const std::size_t end = text.find('\n', start);

                    if (end == std::string::npos)
                    {
                        lines.push_back(text.substr(start));
                        break;
                    }

                    lines.push_back(text.substr(start, end - start));
                    start = end + 1;
                }

                return lines;
            };

        const auto joinLines = [](const std::vector<std::string>& lines)
            {
                std::string text;

                for (std::size_t i = 0; i < lines.size(); ++i)
                {
                    if (i > 0)
                        text += '\n';

                    text += lines[i];
                }

                return text;
            };

        int accepted = 0;
        int rejected = 0;

        for (int trial = 0; trial < 30000; ++trial)
        {
            std::string text = baseText;
            const int edits = 1 + static_cast<int>(rng() % 3);

            for (int edit = 0; edit < edits && !text.empty(); ++edit)
            {
                switch (rng() % 7)
                {
                case 0:
                    text[rng() % text.size()] = static_cast<char>(rng() % 256);
                    break;

                case 1:
                    text.erase(rng() % text.size(), 1 + rng() % 20);
                    break;

                case 2:
                {
                    std::string junk;
                    const std::size_t length = 1 + rng() % 10;

                    for (std::size_t i = 0; i < length; ++i)
                        junk += alphabet[rng() % alphabet.size()];

                    text.insert(rng() % (text.size() + 1), junk);
                    break;
                }

                case 3:
                    text.resize(rng() % (text.size() + 1));
                    break;

                case 4:
                {
                    std::vector<std::string> lines = splitLines(text);
                    const std::size_t index = rng() % lines.size();

                    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(index), lines[index]);
                    text = joinLines(lines);
                    break;
                }

                case 5:
                {
                    std::vector<std::string> lines = splitLines(text);

                    std::swap(lines[rng() % lines.size()], lines[rng() % lines.size()]);
                    text = joinLines(lines);
                    break;
                }

                default:
                    text[rng() % text.size()] = alphabet[rng() % alphabet.size()];
                    break;
                }
            }

            Core::Scene target = sentinel;
            std::string error;

            if (!Core::SceneSerializer::Deserialize(text, target, &error))
            {
                ++rejected;

                Require(IsPlainMessage(error), "trial " + std::to_string(trial) + ": bad message");
                Require(SameScene(target, sentinel), "trial " + std::to_string(trial) + ": rejected input modified the scene");
                continue;
            }

            ++accepted;

            // Принято — значит сцена корректна, сохраняется и читается обратно тем же.
            Require(IsSane(target), "trial " + std::to_string(trial) + ": accepted a broken scene");

            std::string saved;
            Core::Scene again;

            Require(Core::SceneSerializer::Serialize(target, saved, &error), "trial " + std::to_string(trial) + ": cannot re-save: " + error);
            Require(Core::SceneSerializer::Deserialize(saved, again, &error), "trial " + std::to_string(trial) + ": cannot re-load: " + error);
            Require(SameScene(target, again), "trial " + std::to_string(trial) + ": re-load differs");
        }

        // Если принято всё или отвергнуто всё — мутации ничего не проверяют.
        Require(accepted > 100 && rejected > 100, "mutations are not diverse enough");

        std::cout << "      (" << rejected << " rejected, " << accepted << " harmless)\n";
        });

    fs::remove_all(TempDir());

    std::cout << cases << " cases, " << checks << " checks, " << failures << " failures\n";

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
