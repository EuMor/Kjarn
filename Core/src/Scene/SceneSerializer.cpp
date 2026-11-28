#include "Core/Scene/SceneSerializer.h"

#include "Core/Scene/Scene.h"
#include "Core/Tilemap.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

// Формат сцены — текст, одна запись на строку. Пустые строки и строки,
// начинающиеся с '#', пропускаются. Save пишет V4, Load читает V4, V3 и V2.
//
// V4 (текущий):
//   KJARN_SCENE_V4
//   TILE <x> <y> <r> <g> <b>
//   ENTITY id=<n> shape=<RECT|CIRCLE|TRIANGLE> pos=<x>,<y>,<z> size=<w>,<h> color=<r>,<g>,<b>
//          [rot=<x>,<y>,<z>] [scale=<x>,<y>,<z>] [player=1] [static=1] [gravity=1]
//   END tiles=<число TILE> entities=<число ENTITY>
//
// V3 (прежний master): TILE как в V4, ENTITY <shape> <x> <y> <w> <h> <r> <g> <b>.
//   id и флаги в V3 не хранились: id выдаются заново с 1, флаги сброшены.
//
// V2 (коммит 8394e5c): ENTITY_COUNT <n>, затем
//   ENTITY <id> <shape> <pos xyz> <rot xyz> <scale xyz> <w> <h> <r> <g> <b> <isStatic> <useGravity> <isPlayer>
//
// Не сохраняются velocityX/velocityY и isGrounded: это состояние Play-режима.

namespace Core
{
    namespace
    {
        constexpr std::string_view HeaderV2 = "KJARN_SCENE_V2";
        constexpr std::string_view HeaderV3 = "KJARN_SCENE_V3";
        constexpr std::string_view HeaderV4 = "KJARN_SCENE_V4";
        constexpr std::string_view HeaderV5 = "KJARN_SCENE_V5";

        // Файл крупнее — почти наверняка не сцена; целиком в память не читаем.
        constexpr std::uintmax_t MaxFileSize = 256ull * 1024 * 1024;

        bool ReportError(std::string* error, std::string message)
        {
            if (error != nullptr)
                *error = std::move(message);

            return false;
        }

        // Кусок файла для сообщения об ошибке. Только печатный ASCII: сообщение
        // попадает в UI, а UTF-8 с битыми байтами отрисовка отбросит целиком.
        std::string Quote(std::string_view text)
        {
            constexpr std::size_t MaxLength = 40;

            std::string result = "'";

            for (const char c : text.substr(0, MaxLength))
                result += (c >= 0x20 && c < 0x7F) ? c : '?';

            if (text.size() > MaxLength)
                result += "...";

            result += '\'';
            return result;
        }

        // ------------------------------------------------------------------
        // Разбор чисел. std::from_chars не зависит от локали и не принимает
        // пробелов и '+', а проверка ptr == last отсекает хвост вроде "12abc".
        // ------------------------------------------------------------------

        template <typename T>
        bool ParseNumber(std::string_view text, T& value)
        {
            const char* first = text.data();
            const char* last = first + text.size();

            const std::from_chars_result result = std::from_chars(first, last, value);

            return result.ec == std::errc() && result.ptr == last;
        }

        // from_chars читает "nan" и "inf", но в сцене им не место.
        bool ParseFloat(std::string_view text, float& value)
        {
            return ParseNumber(text, value) && std::isfinite(value);
        }

        bool ParseFlag(std::string_view text, bool& value)
        {
            if (text == "0")
                value = false;
            else if (text == "1")
                value = true;
            else
                return false;

            return true;
        }

        bool ParseColor(
            std::string_view r,
            std::string_view g,
            std::string_view b,
            Color& color
        )
        {
            int red = 0;
            int green = 0;
            int blue = 0;

            if (!ParseNumber(r, red) || !ParseNumber(g, green) || !ParseNumber(b, blue))
                return false;

            if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
                return false;

            color = Color(
                static_cast<std::uint8_t>(red),
                static_cast<std::uint8_t>(green),
                static_cast<std::uint8_t>(blue)
            );

            return true;
        }

        // Делит строку на слова по пробелам и табуляциям.
        void Tokenize(std::string_view line, std::vector<std::string_view>& tokens)
        {
            tokens.clear();

            std::size_t i = 0;

            while (i < line.size())
            {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
                    ++i;

                const std::size_t start = i;

                while (i < line.size() && line[i] != ' ' && line[i] != '\t')
                    ++i;

                if (i > start)
                    tokens.push_back(line.substr(start, i - start));
            }
        }

        // Делит "a,b,c" по запятым. false, если частей не ровно N.
        template <std::size_t N>
        bool SplitList(std::string_view text, std::array<std::string_view, N>& parts)
        {
            std::size_t count = 0;
            std::size_t start = 0;

            for (;;)
            {
                const std::size_t comma = text.find(',', start);

                const std::size_t length =
                    (comma == std::string_view::npos) ? std::string_view::npos : comma - start;

                if (count == N)
                    return false;

                parts[count++] = text.substr(start, length);

                if (comma == std::string_view::npos)
                    return count == N;

                start = comma + 1;
            }
        }

        template <std::size_t N>
        bool ParseFloats(std::string_view text, std::array<float, N>& values)
        {
            std::array<std::string_view, N> parts;

            if (!SplitList(text, parts))
                return false;

            for (std::size_t i = 0; i < N; ++i)
            {
                if (!ParseFloat(parts[i], values[i]))
                    return false;
            }

            return true;
        }

        bool ParseVector3(std::string_view text, Vector3& vector)
        {
            std::array<float, 3> values{};

            if (!ParseFloats(text, values))
                return false;

            vector = Vector3{ values[0], values[1], values[2] };
            return true;
        }

        // Если text начинается с prefix, отрезает его и возвращает true.
        bool ConsumePrefix(std::string_view& text, std::string_view prefix)
        {
            if (text.substr(0, prefix.size()) != prefix)
                return false;

            text.remove_prefix(prefix.size());
            return true;
        }

        const char* ShapeName(ShapeType shape)
        {
            switch (shape)
            {
            case ShapeType::Rectangle: return "RECT";
            case ShapeType::Circle:    return "CIRCLE";
            case ShapeType::Triangle:  return "TRIANGLE";
            }

            return nullptr; // значение вне перечисления
        }

        bool ParseShape(std::string_view text, ShapeType& shape)
        {
            if (text == "RECT")
                shape = ShapeType::Rectangle;
            else if (text == "CIRCLE")
                shape = ShapeType::Circle;
            else if (text == "TRIANGLE")
                shape = ShapeType::Triangle;
            else
                return false;

            return true;
        }

        bool IsFinite(const Vector3& v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }

        // Одни и те же правила для записи и чтения: что Save записал,
        // то Load гарантированно прочитает. Возвращает описание проблемы
        // или nullptr, если с сущностью всё в порядке.
        const char* FindProblem(const Entity& entity)
        {
            if (!entity.id.IsValid() ||
                entity.id.value == std::numeric_limits<std::uint64_t>::max())
                return "invalid id";

            if (!IsFinite(entity.transform.position))
                return "position is not finite";

            if (!IsFinite(entity.transform.rotation))
                return "rotation is not finite";

            if (!IsFinite(entity.transform.scale))
                return "scale is not finite";

            if (!std::isfinite(entity.width) || !std::isfinite(entity.height))
                return "size is not finite";

            if (entity.width <= 0.0f || entity.height <= 0.0f)
                return "size must be positive";

            if (ShapeName(entity.shape) == nullptr)
                return "unknown shape";

            return nullptr;
        }

        // ------------------------------------------------------------------
        // V4: ENTITY id=... pos=... — ключи в любом порядке, все, кроме id, необязательны.
        // ------------------------------------------------------------------

        enum class EntityKey
        {
            Id, Shape, Pos, Rot, Scale, Size, Color, Player, Static, Gravity, Count
        };

        constexpr std::array<std::string_view, static_cast<std::size_t>(EntityKey::Count)> EntityKeyNames = {
            "id", "shape", "pos", "rot", "scale", "size", "color", "player", "static", "gravity"
        };

        bool ApplyEntityKey(Entity& entity, EntityKey key, std::string_view value)
        {
            switch (key)
            {
            case EntityKey::Id:
                return ParseNumber(value, entity.id.value);

            case EntityKey::Shape:
                return ParseShape(value, entity.shape);

            case EntityKey::Pos:
                return ParseVector3(value, entity.transform.position);

            case EntityKey::Rot:
                return ParseVector3(value, entity.transform.rotation);

            case EntityKey::Scale:
                return ParseVector3(value, entity.transform.scale);

            case EntityKey::Size:
            {
                std::array<float, 2> size{};

                if (!ParseFloats(value, size))
                    return false;

                entity.width = size[0];
                entity.height = size[1];
                return true;
            }

            case EntityKey::Color:
            {
                std::array<std::string_view, 3> parts;

                return SplitList(value, parts) &&
                    ParseColor(parts[0], parts[1], parts[2], entity.color);
            }

            case EntityKey::Player:
                return ParseFlag(value, entity.isPlayer);

            case EntityKey::Static:
                return ParseFlag(value, entity.isStatic);

            case EntityKey::Gravity:
                return ParseFlag(value, entity.useGravity);

            case EntityKey::Count:
                break;
            }

            return false;
        }

        // ------------------------------------------------------------------
        // Разбор целого файла в переданную (временную) сцену.
        // ------------------------------------------------------------------

        class SceneParser
        {
        public:
            explicit SceneParser(Scene& target)
                : m_scene(target)
            {
            }

            bool Parse(std::string_view text);

            const std::string& GetError() const
            {
                return m_error;
            }

        private:
            bool ParseHeader();
            bool ParseTile();
            bool ParseEntityCountV2();
            bool ParseEntityV2();
            bool ParseEntityV3();
            bool ParseEntityV4();
            bool ParseEnd();
            bool AddEntity(const Entity& entity);
            bool Finish();

            bool Fail(const std::string& message);
            bool FailFile(const std::string& message);

            Scene& m_scene;
            std::vector<std::string_view> m_tokens;
            std::string m_error;

            int m_version = 0;
            int m_line = 0;

            std::size_t m_tileCount = 0;
            std::size_t m_entityCount = 0;

            // V3 не хранил id: выдаём 1, 2, 3... в порядке записей.
            std::uint64_t m_nextLegacyId = 1;

            bool m_hasEntityCount = false;
            std::uint64_t m_declaredEntityCount = 0;

            bool m_seenEnd = false;
        };

        bool SceneParser::Fail(const std::string& message)
        {
            m_error = "line " + std::to_string(m_line) + ": " + message;
            return false;
        }

        bool SceneParser::FailFile(const std::string& message)
        {
            m_error = message;
            return false;
        }

        bool SceneParser::Parse(std::string_view text)
        {
            // Блокнот может поставить в начало файла метку порядка байтов UTF-8.
            constexpr std::string_view Bom = "\xEF\xBB\xBF";

            if (text.substr(0, Bom.size()) == Bom)
                text.remove_prefix(Bom.size());

            while (!text.empty())
            {
                // Следующая строка без '\n' и '\r' (файлы бывают с CRLF).
                const std::size_t end = text.find('\n');
                std::string_view line = text.substr(0, end);

                text = (end == std::string_view::npos) ? std::string_view() : text.substr(end + 1);

                if (!line.empty() && line.back() == '\r')
                    line.remove_suffix(1);

                ++m_line;
                Tokenize(line, m_tokens);

                if (m_version == 0)
                {
                    if (!ParseHeader())
                        return false;

                    continue;
                }

                if (m_tokens.empty() || m_tokens[0].front() == '#')
                    continue;

                if (m_seenEnd)
                    return Fail("unexpected data after END");

                const std::string_view record = m_tokens[0];
                bool ok = false;

                if (record == "TILE" && m_version >= 3)
                    ok = ParseTile();
                else if (record == "ENTITY")
                    ok = (m_version == 4) ? ParseEntityV4()
                    : (m_version == 3) ? ParseEntityV3()
                    : ParseEntityV2();
                else if (record == "ENTITY_COUNT" && m_version == 2)
                    ok = ParseEntityCountV2();
                else if (record == "END" && m_version >= 4)
                    ok = ParseEnd();
                else
                    return Fail("unknown record " + Quote(record));

                if (!ok)
                    return false;
            }

            return Finish();
        }

        bool SceneParser::ParseHeader()
        {
            if (m_tokens[0] == HeaderV5) m_version = 5;
            else if (m_tokens[0] == HeaderV4) m_version = 4;
            else if (m_tokens[0] == HeaderV3) m_version = 3;
            else if (m_tokens[0] == HeaderV2) m_version = 2;

            if (m_version == 0)
            {
                const std::string got = m_tokens.empty() ? "an empty line" : Quote(m_tokens[0]);

                return Fail("expected KJARN_SCENE_V2/V3/V4 header, got " + got);
            }

            return true;
        }

        bool SceneParser::Finish()
        {
            if (m_version == 0)
                return FailFile("file is empty (no KJARN_SCENE header)");

            if (m_version == 2)
            {
                if (!m_hasEntityCount)
                    return FailFile("missing ENTITY_COUNT");

                if (m_declaredEntityCount != m_entityCount)
                    return FailFile(
                        "ENTITY_COUNT is " + std::to_string(m_declaredEntityCount) +
                        " but the file has " + std::to_string(m_entityCount) + " ENTITY records");
            }

            // Без END нельзя отличить обрезанный файл от целого.
            if (m_version >= 4 && !m_seenEnd) // 👈 V4 и V5
                return FailFile("missing END record (file is truncated?)");

            return true;
        }

        bool SceneParser::ParseTile()
        {
            int x = 0, y = 0;
            Color color;
            TilePattern pattern = TilePattern::Solid;

            if (m_version >= 5) {
                if (m_tokens.size() != 7) // 👈 TILE x y r g b pattern (7 токенов)
                    return Fail("TILE expects 6 values: x y r g b pattern");

                int patInt = 0;
                if (!ParseNumber(m_tokens[1], x) || !ParseNumber(m_tokens[2], y))
                    return Fail("TILE: cell coordinates must be integers");
                if (!ParseColor(m_tokens[3], m_tokens[4], m_tokens[5], color))
                    return Fail("TILE: color components must be integers 0..255");
                if (!ParseNumber(m_tokens[6], patInt))
                    return Fail("TILE: pattern must be an integer");

                pattern = static_cast<TilePattern>(patInt);
            }
            else {
                if (m_tokens.size() != 6) // 👈 TILE x y r g b (6 токенов)
                    return Fail("TILE expects 5 values: x y r g b");

                if (!ParseNumber(m_tokens[1], x) || !ParseNumber(m_tokens[2], y))
                    return Fail("TILE: cell coordinates must be integers");
                if (!ParseColor(m_tokens[3], m_tokens[4], m_tokens[5], color))
                    return Fail("TILE: color components must be integers 0..255");
            }

            Tilemap& tilemap = m_scene.GetTilemap();

            if (tilemap.IsOccupied(x, y))
                return Fail("duplicate TILE at (" + std::to_string(x) + ", " + std::to_string(y) + ")");

            tilemap.SetTile(x, y, Tile{ color, pattern }); // 👈 Передаем паттерн
            ++m_tileCount;

            return true;
        }

        bool SceneParser::AddEntity(const Entity& entity)
        {
            if (const char* problem = FindProblem(entity))
                return Fail(std::string("ENTITY: ") + problem);

            Entity* slot = m_scene.CreateEntityWithId(entity.id);

            if (slot == nullptr)
                return Fail("ENTITY: duplicate id " + std::to_string(entity.id.value));

            *slot = entity;
            ++m_entityCount;

            return true;
        }

        bool SceneParser::ParseEntityV4()
        {
            Entity entity;
            unsigned seen = 0;

            for (std::size_t i = 1; i < m_tokens.size(); ++i)
            {
                const std::string_view token = m_tokens[i];
                const std::size_t equals = token.find('=');

                if (equals == std::string_view::npos)
                    return Fail("ENTITY: expected key=value, got " + Quote(token));

                const std::string_view name = token.substr(0, equals);
                const std::string_view value = token.substr(equals + 1);

                const auto found = std::find(EntityKeyNames.begin(), EntityKeyNames.end(), name);

                if (found == EntityKeyNames.end())
                    return Fail("ENTITY: unknown key " + Quote(name));

                const auto index = static_cast<std::size_t>(found - EntityKeyNames.begin());

                if ((seen & (1u << index)) != 0)
                    return Fail("ENTITY: duplicate key " + Quote(name));

                seen |= 1u << index;

                if (!ApplyEntityKey(entity, static_cast<EntityKey>(index), value))
                    return Fail("ENTITY: bad value for " + Quote(name) + ": " + Quote(value));
            }

            if ((seen & (1u << static_cast<std::size_t>(EntityKey::Id))) == 0)
                return Fail("ENTITY: missing id");

            return AddEntity(entity);
        }

        bool SceneParser::ParseEnd()
        {
            std::string_view tilesText = m_tokens.size() == 3 ? m_tokens[1] : std::string_view();
            std::string_view entitiesText = m_tokens.size() == 3 ? m_tokens[2] : std::string_view();

            std::uint64_t tiles = 0;
            std::uint64_t entities = 0;

            if (!ConsumePrefix(tilesText, "tiles=") || !ParseNumber(tilesText, tiles) ||
                !ConsumePrefix(entitiesText, "entities=") || !ParseNumber(entitiesText, entities))
                return Fail("END expects: tiles=<n> entities=<n>");

            if (tiles != m_tileCount || entities != m_entityCount)
                return Fail(
                    "END says " + std::to_string(tiles) + " tiles and " + std::to_string(entities) +
                    " entities, but the file has " + std::to_string(m_tileCount) + " and " +
                    std::to_string(m_entityCount));

            m_seenEnd = true;
            return true;
        }

        // V3: ENTITY <shape> <x> <y> <w> <h> <r> <g> <b>
        bool SceneParser::ParseEntityV3()
        {
            if (m_tokens.size() != 9)
                return Fail("ENTITY expects 8 values: shape x y w h r g b");

            Entity entity;
            entity.id.value = m_nextLegacyId++;

            if (!ParseShape(m_tokens[1], entity.shape))
                return Fail("ENTITY: unknown shape " + Quote(m_tokens[1]));

            if (!ParseFloat(m_tokens[2], entity.transform.position.x) ||
                !ParseFloat(m_tokens[3], entity.transform.position.y) ||
                !ParseFloat(m_tokens[4], entity.width) ||
                !ParseFloat(m_tokens[5], entity.height))
                return Fail("ENTITY: x, y, w, h must be finite numbers");

            if (!ParseColor(m_tokens[6], m_tokens[7], m_tokens[8], entity.color))
                return Fail("ENTITY: color components must be integers 0..255");

            return AddEntity(entity);
        }

        bool SceneParser::ParseEntityCountV2()
        {
            if (m_hasEntityCount)
                return Fail("duplicate ENTITY_COUNT");

            if (m_tokens.size() != 2 || !ParseNumber(m_tokens[1], m_declaredEntityCount))
                return Fail("ENTITY_COUNT expects one non-negative integer");

            m_hasEntityCount = true;
            return true;
        }

        // V2: ENTITY <id> <shape> <pos xyz> <rot xyz> <scale xyz> <w> <h> <r> <g> <b>
        //            <isStatic> <useGravity> <isPlayer>
        bool SceneParser::ParseEntityV2()
        {
            if (!m_hasEntityCount)
                return Fail("ENTITY_COUNT must come before ENTITY records");

            if (m_tokens.size() != 20)
                return Fail("ENTITY expects 19 values");

            Entity entity;

            if (!ParseNumber(m_tokens[1], entity.id.value))
                return Fail("ENTITY: bad id " + Quote(m_tokens[1]));

            if (!ParseShape(m_tokens[2], entity.shape))
                return Fail("ENTITY: unknown shape " + Quote(m_tokens[2]));

            // Подряд идут 11 чисел: position(3), rotation(3), scale(3), width, height.
            std::array<float, 11> numbers{};

            for (std::size_t i = 0; i < numbers.size(); ++i)
            {
                if (!ParseFloat(m_tokens[3 + i], numbers[i]))
                    return Fail("ENTITY: bad number " + Quote(m_tokens[3 + i]));
            }

            entity.transform.position = Vector3{ numbers[0], numbers[1], numbers[2] };
            entity.transform.rotation = Vector3{ numbers[3], numbers[4], numbers[5] };
            entity.transform.scale = Vector3{ numbers[6], numbers[7], numbers[8] };
            entity.width = numbers[9];
            entity.height = numbers[10];

            if (!ParseColor(m_tokens[14], m_tokens[15], m_tokens[16], entity.color))
                return Fail("ENTITY: color components must be integers 0..255");

            if (!ParseFlag(m_tokens[17], entity.isStatic) ||
                !ParseFlag(m_tokens[18], entity.useGravity) ||
                !ParseFlag(m_tokens[19], entity.isPlayer))
                return Fail("ENTITY: flags must be 0 or 1");

            return AddEntity(entity);
        }

        // ------------------------------------------------------------------
        // Запись
        // ------------------------------------------------------------------

        // Самое короткое представление, которое читается обратно в то же число.
        template <typename T>
        void Append(std::string& out, T value)
        {
            char buffer[64];

            const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);

            out.append(buffer, static_cast<std::size_t>(result.ptr - buffer));
        }

        void AppendVector3(std::string& out, const Vector3& v)
        {
            Append(out, v.x);
            out += ',';
            Append(out, v.y);
            out += ',';
            Append(out, v.z);
        }

        bool IsAll(const Vector3& v, float value)
        {
            return v.x == value && v.y == value && v.z == value;
        }

        // Временный файл + переименование: на диске всегда либо старая версия,
        // либо новая целиком. В MSVC STL rename заменяет существующий файл
        // (MoveFileExW с MOVEFILE_REPLACE_EXISTING).
        bool WriteFileAtomically(
            const std::string& filename,
            const std::string& content,
            std::string* error
        )
        {
            namespace fs = std::filesystem;

            const fs::path target(filename);
            fs::path temporary = target;
            temporary += ".tmp";

            std::error_code ignored;

            {
                std::ofstream file(temporary, std::ios::binary | std::ios::trunc);

                if (!file)
                    return ReportError(error, "cannot create '" + temporary.string() + "'");

                file.write(content.data(), static_cast<std::streamsize>(content.size()));
                file.flush();
                file.close();

                if (!file)
                {
                    fs::remove(temporary, ignored);
                    return ReportError(error, "write failed (disk full?): '" + temporary.string() + "'");
                }
            }

            std::error_code ec;
            fs::rename(temporary, target, ec);

            if (ec)
            {
                fs::remove(temporary, ignored);
                return ReportError(error, "cannot replace '" + filename + "': " + ec.message());
            }

            return true;
        }

        bool ReadWholeFile(const std::string& filename, std::string& text, std::string* error)
        {
            namespace fs = std::filesystem;

            const fs::path path(filename);
            std::error_code ec;

            if (!fs::is_regular_file(path, ec))
                return ReportError(error, "cannot open '" + filename + "' (missing or not a file)");

            const std::uintmax_t size = fs::file_size(path, ec);

            if (ec)
                return ReportError(error, "cannot get size of '" + filename + "'");

            if (size > MaxFileSize)
                return ReportError(error, "'" + filename + "' is too large to be a scene");

            std::ifstream file(path, std::ios::binary);

            if (!file)
                return ReportError(error, "cannot open '" + filename + "'");

            text.resize(static_cast<std::size_t>(size));
            file.read(text.data(), static_cast<std::streamsize>(size));

            if (static_cast<std::uintmax_t>(file.gcount()) != size)
                return ReportError(error, "read error in '" + filename + "'");

            return true;
        }
    }

    bool SceneSerializer::Serialize(const Scene& scene, std::string& text, std::string* error)
    {
        if (error != nullptr)
            error->clear();

        std::string out;

        out += HeaderV4;
        out += '\n';

        // Тайлы лежат в unordered_map: сортируем по (y, x), чтобы одна и та же
        // карта всегда давала один и тот же файл (стабильные диффы, тесты).
        struct TileRecord
        {
            int x;
            int y;
            Color color;
            TilePattern pattern;
        };

        const auto& tiles = scene.GetTilemap().GetTiles();

        std::vector<TileRecord> records;
        records.reserve(tiles.size());

        for (const auto& [key, tile] : tiles)
        {
            TileRecord record{};
            Tilemap::UnpackKey(key, record.x, record.y);
            record.color = tile.color;
            record.pattern = tile.pattern;
            records.push_back(record);
        }

        std::sort(records.begin(), records.end(), [](const TileRecord& a, const TileRecord& b)
            {
                return a.y != b.y ? a.y < b.y : a.x < b.x;
            });

        for (const TileRecord& record : records)
        {
            out += "TILE ";
            Append(out, record.x);
            out += ' ';
            Append(out, record.y);
            out += ' ';
            Append(out, static_cast<int>(record.color.r));
            out += ' ';
            Append(out, static_cast<int>(record.color.g));
            out += ' ';
            Append(out, static_cast<int>(record.color.b));
            out += ' ';
            Append(out, static_cast<int>(record.pattern));
            out += '\n';
        }

        // Сущности — в порядке сцены: он задаёт порядок отрисовки и выбора мышью.
        std::unordered_set<std::uint64_t> ids;

        for (const Entity& entity : scene.GetEntities())
        {
            if (const char* problem = FindProblem(entity))
                return ReportError(error, "entity " + std::to_string(entity.id.value) + ": " + problem);

            if (!ids.insert(entity.id.value).second)
                return ReportError(error, "duplicate entity id " + std::to_string(entity.id.value));

            out += "ENTITY id=";
            Append(out, entity.id.value);
            out += " shape=";
            out += ShapeName(entity.shape);
            out += " pos=";
            AppendVector3(out, entity.transform.position);
            out += " size=";
            Append(out, entity.width);
            out += ',';
            Append(out, entity.height);
            out += " color=";
            Append(out, static_cast<int>(entity.color.r));
            out += ',';
            Append(out, static_cast<int>(entity.color.g));
            out += ',';
            Append(out, static_cast<int>(entity.color.b));

            // Остальное пишем только если отличается от значений по умолчанию.
            if (!IsAll(entity.transform.rotation, 0.0f))
            {
                out += " rot=";
                AppendVector3(out, entity.transform.rotation);
            }

            if (!IsAll(entity.transform.scale, 1.0f))
            {
                out += " scale=";
                AppendVector3(out, entity.transform.scale);
            }

            if (entity.isPlayer)
                out += " player=1";

            if (entity.isStatic)
                out += " static=1";

            if (entity.useGravity)
                out += " gravity=1";

            out += '\n';
        }

        out += "END tiles=";
        Append(out, records.size());
        out += " entities=";
        Append(out, scene.GetEntityCount());
        out += '\n';

        text = std::move(out);
        return true;
    }

    bool SceneSerializer::Deserialize(std::string_view text, Scene& scene, std::string* error)
    {
        if (error != nullptr)
            error->clear();

        Scene loaded;
        SceneParser parser(loaded);

        if (!parser.Parse(text))
            return ReportError(error, parser.GetError());

        // Единственное место, где меняется scene. Move-присваивание не бросает
        // исключений, а у loaded свежий счётчик id: он продолжается с max(id) + 1.
        scene = std::move(loaded);
        return true;
    }

    bool SceneSerializer::Save(const Scene& scene, const std::string& filename, std::string* error)
    {
        std::string text;

        if (!Serialize(scene, text, error))
            return false;

        return WriteFileAtomically(filename, text, error);
    }

    bool SceneSerializer::Load(const std::string& filename, Scene& scene, std::string* error)
    {
        if (error != nullptr)
            error->clear();

        std::string text;

        if (!ReadWholeFile(filename, text, error))
            return false;

        return Deserialize(text, scene, error);
    }
}
