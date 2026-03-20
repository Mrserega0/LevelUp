#include <SFML/Graphics.hpp> // Подключаем графику SFML: окно, спрайты/фигуры, текст, события // version 0.5
#include <SFML/Audio.hpp>    // Подключаем аудио SFML: музыка, звуки (пока не используется, но пригодится для звуков прыжка/смерти)
#include <SFML/Window.hpp>   // Подключаем окно SFML для управления окном и событиями (хотя sfml/graphics уже включает его, но на всякий случай)
#include <string>            // std::string
#include <vector>            // std::vector
#include <fstream>           // чтение/запись файлов (progress.txt, уровни)
#include <algorithm>         // std::max
#include <iostream>          // (пока почти не используется, но пригодится для debug)
#include <cmath>             // std::exp для плавности камеры
#include "json.hpp"          // Для удобной работы с JSON (уровни в формате JSON, а не текстовые файлы)
using json = nlohmann::json; // Удобный псевдоним для типа JSON из библиотеки nlohmann

enum class Screen
{
    Main,   // Главное меню (Play/Exit)
    Levels, // Экран выбора уровней
    Game    // Игровой экран
};
static int Dead = 0;          // Счётчик смертей (статистика)
int selectedLevel = 1;        // Текущий выбранный уровень (какой стартовать)
Screen screen = Screen::Main; // Текущий активный экран (menu/levels/game)

struct LevelButton
{
    int level;              // Номер уровня, который запускает кнопка
    sf::RectangleShape btn; // Прямоугольник кнопки
    sf::Text txt;           // Текст на кнопке
};

struct Progress
{
    int unlockedLevel = 1; // Максимальный открытый уровень (всё <= unlockedLevel доступно)
    int lastLevel = 1;     // Последний выбранный/игранный уровень (чтобы помнить меню)
    bool load(const std::string &path)
    {
        std::ifstream f(path); // Открываем файл сохранения
        if (!f.is_open())      // Если файл не найден/не открылся
            return false;

        f >> unlockedLevel >> lastLevel; // Читаем 2 числа из файла
        if (!f)                          // Если чтение сломалось (файл пустой/битый)
        {
            unlockedLevel = 1;
            lastLevel = 1;
            return false;
        }
        f >> Dead;
        // Защита от некорректных значений (например, 0 или -5)
        unlockedLevel = std::max(1, unlockedLevel);
        lastLevel = std::max(1, lastLevel);
        Dead = std::max(0, Dead);
        return true;
    }

    bool save(const std::string &path) const
    {
        std::ofstream f(path); // Открываем файл на запись (перезапишет)
        if (!f.is_open())
            return false;

        f << unlockedLevel << " " << lastLevel << "\n"
          << Dead; // Пишем три числа
        return true;
    }

    void onLevelComleted(int levelJustComleted)
    {
        // Разблокируем следующий уровень (если он больше текущего unlockedLevel)
        unlockedLevel = std::max(unlockedLevel, levelJustComleted + 1);
        // Запоминаем, что этот уровень точно пройден (как последний достигнутый)
        lastLevel = std::max(lastLevel, levelJustComleted);
    }
};

static Progress progress;                                              // Глобальный прогресс (читается/пишется из разных мест)
static const std::string PROGRESS_PATH = "assets\\save\\progress.txt"; // Путь к сохранению

// -------- MENU (логика + рисование) --------
struct MenuUI
{
    sf::Font font;

    sf::RectangleShape playBtn, exitBtn, backBtn;
    sf::Text playTxt, exitTxt, backTxt;

    std::vector<LevelButton> levelButtons;

    const sf::Vector2f BTN_SIZE = {200.f, 60.f};

    const sf::Color BTN_COLOR = sf::Color(255, 215, 0);
    const sf::Color BTN_HOVER = sf::Color(255, 215, 0);
    const sf::Color BTN_LOCKED = sf::Color(105, 105, 105);

    const sf::Color TEXT_COLOR = sf::Color(128, 128, 128);
    const sf::Color TEXT_LOCKED = sf::Color(105, 105, 105);

    void makeButton(sf::RectangleShape &r, sf::Text &t,
                    sf::Vector2f pos, const sf::String &label)
    {
        r.setSize(BTN_SIZE);
        r.setPosition(pos);

        r.setFillColor(BTN_COLOR);
        r.setOutlineThickness(2);
        r.setOutlineColor(sf::Color(255, 255, 255));

        t.setFont(font);
        t.setString(label);
        t.setCharacterSize(28);
        t.setFillColor(TEXT_COLOR);

        auto b = t.getLocalBounds();

        t.setOrigin(
            b.left + b.width / 2.f,
            b.top + b.height / 2.f);

        t.setPosition(
            pos.x + BTN_SIZE.x / 2.f,
            pos.y + BTN_SIZE.y / 2.f);

        t.setOutlineThickness(1);
        t.setOutlineColor(sf::Color(255, 255, 255));
    }

    void createLevelColumn(int start, int end, float x)
    {
        for (int i = start; i <= end; i++)
        {
            LevelButton lb;
            lb.level = i;

            float y = 80.f + (i - start) * 80.f;

            makeButton(
                lb.btn,
                lb.txt,
                {x, y},
                "Level " + std::to_string(i));

            levelButtons.push_back(lb);
        }
    }

    void initLevels(int r1, int r2, int r3, int r4)
    {
        createLevelColumn(1, r1, 250.f);
        createLevelColumn(8, r2, 500.f);
        createLevelColumn(15, r3, 750.f);
        createLevelColumn(22, r4, 1000.f);
    }

    void init()
    {
        font.loadFromFile("assets/fonts/COOPBL.TTF");

        makeButton(playBtn, playTxt, {620, 320}, "Play");
        makeButton(exitBtn, exitTxt, {620, 400}, "Exit");
        makeButton(backBtn, backTxt, {620, 670}, "Back");

        initLevels(7, 14, 21, 28);
    }

    void handleEvent(const sf::Event &e, sf::RenderWindow &window)
    {
        if (e.type != sf::Event::MouseButtonPressed ||
            e.mouseButton.button != sf::Mouse::Left)
            return;

        sf::Vector2f m = window.mapPixelToCoords(
            {e.mouseButton.x, e.mouseButton.y});

        if (screen == Screen::Main)
        {
            if (playBtn.getGlobalBounds().contains(m))
                screen = Screen::Levels;

            if (exitBtn.getGlobalBounds().contains(m))
                window.close();
        }

        else if (screen == Screen::Levels)
        {
            if (backBtn.getGlobalBounds().contains(m))
                screen = Screen::Main;

            for (auto &lb : levelButtons)
            {
                if (lb.btn.getGlobalBounds().contains(m))
                {
                    if (progress.unlockedLevel >= lb.level)
                    {
                        selectedLevel = lb.level;

                        progress.lastLevel = lb.level;
                        progress.save(PROGRESS_PATH);

                        screen = Screen::Game;
                    }
                }
            }
        }
    }

    void update(sf::RenderWindow &window)
    {
        sf::Vector2f mouse =
            window.mapPixelToCoords(
                sf::Mouse::getPosition(window));

        auto hover = [&](sf::RectangleShape &btn, sf::Text &txt)
        {
            if (btn.getGlobalBounds().contains(mouse))
            {
                btn.setFillColor(BTN_HOVER);
                btn.setScale(1.05f, 1.05f);
                txt.setFillColor(sf::Color::Yellow);
            }
            else
            {
                btn.setFillColor(BTN_COLOR);
                btn.setScale(1.f, 1.f);
                txt.setFillColor(TEXT_COLOR);
            }
        };

        if (screen == Screen::Main)
        {
            hover(playBtn, playTxt);
            hover(exitBtn, exitTxt);
        }

        else if (screen == Screen::Levels)
        {
            hover(backBtn, backTxt);

            for (auto &lb : levelButtons)
            {
                bool unlocked = progress.unlockedLevel >= lb.level;

                if (!unlocked)
                {
                    lb.btn.setFillColor(BTN_LOCKED);
                    lb.txt.setFillColor(TEXT_LOCKED);
                    continue;
                }

                hover(lb.btn, lb.txt);
            }
        }
    }

    void draw(sf::RenderWindow &window)
    {
        if (screen == Screen::Main)
        {
            window.draw(playBtn);
            window.draw(playTxt);

            window.draw(exitBtn);
            window.draw(exitTxt);
        }

        else if (screen == Screen::Levels)
        {
            for (auto &lb : levelButtons)
            {
                window.draw(lb.btn);
                window.draw(lb.txt);
            }

            window.draw(backBtn);
            window.draw(backTxt);
        }
    }
};
// ---------------- LEVEL ----------------
struct Level
{
    int tileSize = 16;
    int mapWidth = 0;
    int mapHeight = 0;

    std::vector<sf::FloatRect> walls;
    std::vector<sf::FloatRect> spikes;
    std::vector<sf::FloatRect> spikesCoin;
    std::vector<sf::FloatRect> spikesR;
    std::vector<sf::Sprite> backgroundSprites;
    std::vector<sf::Sprite> blockSprites;
    std::vector<sf::Sprite> objectSprites;
    std::vector<sf::Sprite> shipSprite;
    sf::Texture texBlocks;
    sf::Texture texFons;
    sf::Texture texObj;

    sf::Vector2f playerSpawn{0.f, 0.f};
    sf::FloatRect exitRect;
    bool hasExit = false;
    struct TilesetInfo
    {
        int firstGid;
        sf::Texture *texture;
    };
    std::vector<TilesetInfo> tilesets;
    struct Coin
    {
        sf::FloatRect hitbox;
        sf::Sprite sprite;
    };
    std::vector<Coin> coins;
    struct Trigger
    {
        sf::FloatRect area;
        std::string action;
        int id = -1;
        float dx = 0.f;
        float dy = 0.f;
        bool once = true;
        bool activated = false;
        float cooldown = 0.f;
        float delay = 0.f;
        float timer = 0.f;
        bool pending = false;
        int chain = -1;
        int tile = 0;
    };
    std::vector<Trigger> triggers;
    struct BlockAnim
    {
        sf::Sprite sprite;
        float timer = 0.f;
        float duration = 0.2f;
        bool appearing = false;
    };
    std::vector<BlockAnim> animBlocks;
    struct SpikeAnim
    {
        sf::Sprite sprite;
        sf::FloatRect hitbox;
        float timer = 0.f;
        float duration = 0.15f;
        bool appearing = true;
        float startY;
        float endY;
    };
    std::vector<SpikeAnim> animSpikes;
    struct MovingPlatform
    {
        sf::Sprite sprite;
        sf::FloatRect hitbox;
        sf::Vector2f startPos;
        sf::Vector2f endPos;
        float speed = 55.f;
        bool forward = true;

        sf::Vector2f lastPos;            // позиция в прошлом кадре
        sf::Vector2f delta = {0.f, 0.f}; // движение за кадр
    };
    std::vector<MovingPlatform> platforms;
    bool loadFromFile(const std::string &path, int newTileSize)
    {
        tileSize = newTileSize; // Устанавливаем размер тайла (на всякий случай, если нужно изменить)
        // Очищаем все данные, чтобы загрузить новый уровень (на случай, если эта функция вызывается повторно для другого уровня)
        backgroundSprites.clear();
        blockSprites.clear();
        objectSprites.clear();
        coins.clear();
        platforms.clear();
        shipSprite.clear();
        walls.clear();
        spikes.clear();
        spikesCoin.clear();
        spikesR.clear();
        triggers.clear();
        hasExit = false;

        if (!texBlocks.loadFromFile("assets/img/blocks.png"))
            return false;
        if (!texFons.loadFromFile("assets/img/fons.png"))
            return false;
        if (!texObj.loadFromFile("assets/img/obj.png"))
            return false;

        texBlocks.setSmooth(false); // Отключаем сглаживание, чтобы пиксели были чёткими (важно для пиксельной графики)
        texFons.setSmooth(false);
        texObj.setSmooth(false);

        std::ifstream f(path);
        if (!f.is_open())
            return false;

        json j; // Объект JSON из библиотеки nlohmann для хранения данных уровня из файла
        f >> j;

        mapWidth = j["width"];
        mapHeight = j["height"];

        // ===== Читаем tilesets из JSON =====
        tilesets.clear(); // на всякий случай очищаем, если функция вызывается повторно
        tilesets.push_back({j["tilesets"][0]["firstgid"], &texBlocks});
        tilesets.push_back({j["tilesets"][1]["firstgid"], &texFons});
        tilesets.push_back({j["tilesets"][2]["firstgid"], &texObj});

        auto layerBg = j["layers"][0]["data"];  // данные первого слоя (фон)
        auto layerObj = j["layers"][1]["data"]; // данные второго слоя (объекты)

        processLayer(layerBg, false); // обрабатываем слой фона
        processLayer(layerObj, true); // обрабатываем слой объектов

        for (auto &layer : j["layers"])
        {
            if (layer["type"] == "objectgroup" && layer["name"] == "triggers")
            {
                for (auto &obj : layer["objects"])
                {
                    Trigger tr;

                    tr.area = {
                        obj["x"],
                        obj["y"],
                        obj["width"],
                        obj["height"]};
                    if (obj.contains("properties"))
                    {
                        for (auto &p : obj["properties"])
                        {
                            if (p["name"] == "action")
                                tr.action = p["value"];
                            if (p["name"] == "id")
                                tr.id = p["value"];
                            if (p["name"] == "dx")
                                tr.dx = p["value"];
                            if (p["name"] == "dy")
                                tr.dy = p["value"];
                            if (p["name"] == "delay")
                                tr.delay = p["value"];
                            if (p["name"] == "chain")
                                tr.chain = p["value"];
                            if (p["name"] == "tile")
                                tr.tile = p["value"];
                        }
                    }
                    triggers.push_back(tr);
                }
            }
        }

        return true;
    }

    void processLayer(const json &data, bool objectLayer) // обработка слоя (создание спрайтов и коллизий)
    {                                                     // data - массив GID тайлов в слое, objectLayer - это слой объектов (true) или фона/стен (false), чтобы по-разному обрабатывать коллизии
        for (int i = 0; i < data.size(); ++i)             // проходим по каждому тайлу в слое
        {
            int gid = data[i]; // GID тайла (0 - пустой, >0 - есть тайл)
            if (gid == 0)      // 0 - это пустой тайл, пропускаем его (нет спрайта, нет коллизии)
                continue;

            int x = i % mapWidth; // координаты тайла в тайлах (не в пикселях)
            int y = i / mapWidth; // координаты тайла в тайлах

            TilesetInfo *currentSet = nullptr; // найденный тайлсет для этого GID (поиск от последнего к первому, так как GID отсортированы по возрастанию)

            for (int t = tilesets.size() - 1; t >= 0; --t) // проходим по тайлсетам в обратном порядке, чтобы найти, к какому тайлсету принадлежит этот GID
            {                                              // Если GID больше или равен firstGid тайлсета, значит этот тайл принадлежит этому тайлсету (так как GID отсортированы по возрастанию)
                if (gid >= tilesets[t].firstGid)           //
                {                                          // Нашли тайлсет для этого GID
                    currentSet = &tilesets[t];             // Сохраняем указатель на найденный тайлсет
                    break;
                }
            }
            if (!currentSet) // Если не нашли тайлсет для этого GID (что странно, так как должен был найтись хотя бы первый), пропускаем этот тайл
                continue;

            int localId = gid - currentSet->firstGid; // Локальный ID тайла внутри тайлсета (чтобы понять, какой именно спрайт брать из текстуры тайлсета)

            sf::Texture &tex = *currentSet->texture; // Ссылка на текстуру этого тайлсета (для удобства)

            int tilesPerRow = tex.getSize().x / tileSize; // Количество тайлов в одной строке текстуры тайлсета (чтобы правильно вычислить координаты спрайта)
            int tx = localId % tilesPerRow;               // X координата тайла в текстуре (в тайлах, не в пикселях)
            int ty = localId / tilesPerRow;               // Y координата тайла в текстуре (в тайлах, не в пикселях)

            sf::Sprite sprite;                 // Создаём спрайт для этого тайла
            sprite.setTexture(tex);            // Устанавливаем текстуру для спрайта (это может быть texBlocks, texFons или texObj в зависимости от тайлсета)
            sprite.scale(1.0f, 1.0f);          // Масштабируем спрайт (можно изменить для увеличения или уменьшения размера)
            sprite.setTextureRect(sf::IntRect( // Устанавливаем прямоугольник текстуры для спрайта, чтобы брать нужный тайл из текстуры тайлсета
                tx * tileSize,                 // X координата в пикселях в текстуре (tx в тайлах * размер тайла)
                ty * tileSize,                 // Y координата в пикселях в текстуре (ty в тайлах * размер тайла)
                tileSize,
                tileSize)); // Размер спрайта (обычно равен размеру тайла)

            sprite.setPosition(x * tileSize, y * tileSize); // Устанавливаем позицию спрайта на экране (x и y в тайлах * размер тайла)

            if (!objectLayer)
            {
                if (currentSet->texture == &texBlocks)
                {
                    blockSprites.push_back(sprite);
                    walls.emplace_back(
                        x * tileSize,
                        y * tileSize,
                        tileSize,
                        tileSize);
                }
                else
                {
                    backgroundSprites.push_back(sprite);
                }
            }
            else
            {
                if (currentSet->texture == &texObj)
                {
                    if (localId == 0) // шип
                    {
                        float spikeWidth = tileSize * 0.8f;
                        float spikeHeight = tileSize * 0.1f;

                        float spikeX = x * tileSize + (tileSize - spikeWidth) * 0.5f;
                        float spikeY = y * tileSize + tileSize - spikeHeight;

                        spikes.emplace_back(
                            spikeX,
                            spikeY,
                            spikeWidth,
                            spikeHeight);

                        shipSprite.push_back(sprite);
                    }
                    else if (localId == 1) // портал
                    {
                        float scale = 2.f;

                        sprite.setScale(scale, scale);

                        sprite.setPosition(
                            x * tileSize - tileSize * (scale - 1) * 1.f,
                            y * tileSize - tileSize * (scale - 1) * 1.f);

                        hasExit = true;

                        exitRect = {
                            sprite.getPosition().x,
                            sprite.getPosition().y,
                            tileSize * scale,
                            tileSize * scale};

                        objectSprites.push_back(sprite);
                    }
                    else if (localId == 2) // спавн
                    {
                        playerSpawn = {
                            x * (float)tileSize,
                            y * (float)tileSize};

                        // objectSprites.push_back(sprite);
                    }
                    else if (localId == 3) // coin
                    {
                        float coinSize = tileSize * 0.5f;

                        float coinX = x * tileSize + (tileSize - coinSize) / 2.f;
                        float coinY = y * tileSize + (tileSize - coinSize) / 2.f;
                        Coin coin;
                        coin.hitbox = {
                            coinX,
                            coinY,
                            coinSize,
                            coinSize};
                        coin.sprite = sprite;
                        coins.push_back(coin);
                    }
                    else if (localId == 4) // 180 ship
                    {
                        float spikeWidth = tileSize * 0.8f;
                        float spikeHeight = tileSize * 0.3f;

                        float spikeX = x * tileSize + (tileSize - spikeWidth) * 0.5f;
                        float spikeY = y * tileSize;

                        spikesR.emplace_back(
                            spikeX,
                            spikeY,
                            spikeWidth,
                            spikeHeight);

                        shipSprite.push_back(sprite);
                    }
                    else if (localId == 5) // портал
                    {
                        float scale = 2.f;

                        sprite.setScale(scale, scale);

                        sprite.setPosition(
                            x * tileSize - tileSize * (scale - 1) * 1.f,
                            y * tileSize - tileSize * (scale - 1) * 1.f);

                        hasExit = true;

                        exitRect = {
                            sprite.getPosition().x,
                            sprite.getPosition().y,
                            tileSize * scale,
                            tileSize * scale};

                        objectSprites.push_back(sprite);
                    }
                    else if (localId == 6) // coin ship
                    {
                        float ShipSize = tileSize * 0.5f;

                        float ShipcoinX = x * tileSize + (tileSize - ShipSize) / 2.f;
                        float ShipcoinY = y * tileSize + (tileSize - ShipSize) / 2.f;

                        spikesCoin.emplace_back(
                            ShipcoinX,
                            ShipcoinY,
                            ShipSize,
                            ShipSize);

                        objectSprites.push_back(sprite);
                    }
                }
            }
        }
    }

    sf::Vector2f getPixelSize() const // размер карты в пикселях (для настройки камеры и проверки выхода за границы)
    {
        return {
            mapWidth * (float)tileSize,
            mapHeight * (float)tileSize};
    }

    void resetTriggers()
    {
        for (auto &t : triggers)
        {
            t.pending = false;
            t.timer = 0.f;
            t.activated = false;
            t.cooldown = 0.f;
        }
    }
    void draw(sf::RenderWindow &window)
    {
        for (auto &s : backgroundSprites)
            window.draw(s);
        for (auto &s : objectSprites)
            window.draw(s);
        for (auto &c : coins)
            window.draw(c.sprite);
        for (auto &s : shipSprite)
            window.draw(s);
        for (auto &a : animSpikes)
            window.draw(a.sprite);
        for (auto &s : blockSprites)
            window.draw(s);
        for (auto &p : platforms)
            window.draw(p.sprite);
        for (auto &a : animBlocks)
            window.draw(a.sprite);
        // for (auto &t : triggers) // для дебага триггеров
        // {
        //     sf::RectangleShape r;
        //     r.setPosition(t.area.left, t.area.top);
        //     r.setSize({t.area.width, t.area.height});
        //     r.setFillColor(sf::Color(255, 0, 0, 80));
        //     window.draw(r);
        // }
    }
    std::vector<sf::FloatRect> getNearbyWallRects(const sf::FloatRect &box) const
    {
        std::vector<sf::FloatRect> result;

        for (const auto &w : walls)
        {
            if (w.intersects(box))
                result.push_back(w);
        }
        return result;
    }
};
// ---------------- PLAYER ----------------
struct Player
{
    sf::RectangleShape body;    // Тело игрока (прямоугольник)
    sf::Vector2f vel{0.f, 0.f}; // Скорость (x,y)
    sf::Vector2f spawnPoint;    // Точка респавна
    sf::Texture Ptexture;
    sf::Sprite Psprite;
    bool onGround = false;      // Стоим ли на земле (можно ли прыгать)
    bool alive = true;          // Жив ли игрок (для падения/смерти)
    bool isMoving = false;      // Двигается ли игрок (для анимации)
    bool KeyContol = true;      // true обычное управление. False наоборот
    bool gravityControl = true; // Обычная гравитация true. False потолок
    float moveSpeed = 100.f;    // Скорость движения по X. 100
    float jumpSpeed = 215.f;    // Сила прыжка. delault 205
    float gravity = 1200.f;     // Гравитация
    float animationTimer = 0.f;
    float animationSpeed = 0.1f;

    const int frameWidth = 13; // W13 H25
    const int frameHeight = 25;
    const int frameCount = 3;
    int currentFrame = 0; // Текущий кадр анимации

    sf::SoundBuffer stepBuffer;
    sf::SoundBuffer jumpBuffer;
    sf::SoundBuffer deathBuffer;
    sf::SoundBuffer coinBuffer;
    sf::SoundBuffer uronBuffer;
    sf::SoundBuffer gravityBuffer;
    sf::Sound stepSound;
    sf::Sound jumpSound;
    sf::Sound deathSound;
    sf::Sound coinSound;
    sf::Sound uronSound;
    sf::Sound gravitySound;
    sf::Clock stepClock;
    float stepDelay = 0.4f;

    bool loadSounds()
    {
        if (!stepBuffer.loadFromFile("assets/sounds/speed.wav"))
            std::cout << "Failed to load step sound\n";
        if (!jumpBuffer.loadFromFile("assets/sounds/jump.wav"))
            std::cout << "Failed to load jump sound\n";
        if (!deathBuffer.loadFromFile("assets/sounds/dead.wav"))
            std::cout << "Failed to load death sound\n";
        if (!coinBuffer.loadFromFile("assets/sounds/coin.wav"))
            std::cout << "Failed to load coin sound\n";
        if (!uronBuffer.loadFromFile("assets/sounds/uron.wav"))
            std::cout << "Failed to load uron sound\n";
        if (!gravityBuffer.loadFromFile("assets/sounds/gravity.wav"))
            std::cout << "Failed to load gravity sound\n";
        stepSound.setBuffer(stepBuffer);
        jumpSound.setBuffer(jumpBuffer);
        deathSound.setBuffer(deathBuffer);
        coinSound.setBuffer(coinBuffer);
        uronSound.setBuffer(uronBuffer);
        gravitySound.setBuffer(gravityBuffer);
        stepSound.setVolume(100.f);
        jumpSound.setVolume(25.f);
        deathSound.setVolume(100.f);
        coinSound.setVolume(100.f);
        uronSound.setVolume(100.f);
        gravitySound.setVolume(350.f);
        return true;
    }

    bool init()
    {
        if (!Ptexture.loadFromFile("assets/img/playerAnimation.png"))
            return false;
        Psprite.setTexture(Ptexture);
        Psprite.setTextureRect(sf::IntRect(0, 0, frameWidth, frameHeight));
        Psprite.setOrigin(frameWidth / 2.f, frameHeight / 2.f);
        Ptexture.setSmooth(false);
        return true;
    }
    void spawn(sf::Vector2f pos)
    {
        spawnPoint = pos; // Запоминаем точку спавна
        body.setSize({10.f, 12.f});
        updateSpriteScale();                        // Обновляем масштаб спрайта, чтобы он соответствовал размеру тела
        body.setPosition(pos.x + 3.f, pos.y + 2.f); // Смещение, чтобы “лежал красиво” в тайле
        vel = {0.f, 0.f};                           // Сбрасываем скорость
        onGround = false;                           // В воздухе до столкновения
        alive = true;                               // Жив
        deathSound.play();
    }
    void updateSpriteScale()
    {
        float scaleX = body.getSize().x / (frameWidth);
        float scaleY = body.getSize().y / (frameHeight);
        Psprite.setScale(scaleX, scaleY);
    }
    void handleInput()
    {
        isMoving = false;
        float dir = 0.f; // направление по X: -1 влево, +1 вправо
        if (KeyContol)
        {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::A))
            {
                dir -= 1.f;
                isMoving = true;
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
            {
                dir += 1.f;
                isMoving = true;
            }
            vel.x = dir * moveSpeed; // выставляем горизонтальную скорость

            // Прыжок: только если на земле, и нажали Space
            if (onGround && sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
            {
                if (gravityControl)
                {
                    vel.y = -jumpSpeed; // вверх (в SFML Y вниз, поэтому минус)
                }
                else
                {
                    vel.y = +jumpSpeed;
                }
                jumpSound.play();
                onGround = false; // мы в воздухе
            }
        }
        else
        {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
            {
                dir -= 1.f;
                isMoving = true;
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::A))
            {
                dir += 1.f;
                isMoving = true;
            }
            vel.x = dir * moveSpeed; // выставляем горизонтальную скорость

            // Прыжок: только если на земле, и нажали Space
            if (onGround && sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
            {
                if (gravityControl)
                {
                    vel.y = -jumpSpeed; // вверх (в SFML Y вниз, поэтому минус)
                }
                else
                {
                    vel.y = +jumpSpeed;
                }
                jumpSound.play();
                onGround = false; // мы в воздухе
            }
        }
    }
    void resolveCollisions(const Level &lvl, float dt)
    {
        // ===== ДВИЖЕНИЕ ПО X =====
        body.move(vel.x * dt, 0.f);
        sf::FloatRect box = body.getGlobalBounds();

        for (const auto &w : lvl.walls)
        {
            if (box.intersects(w))
            {
                if (vel.x > 0) // вправо
                    body.setPosition(w.left - box.width, body.getPosition().y);
                else if (vel.x < 0) // влево
                    body.setPosition(w.left + w.width, body.getPosition().y);

                vel.x = 0.f;
                box = body.getGlobalBounds();
            }
        }

        // ===== ДВИЖЕНИЕ ПО Y =====
        if (gravityControl)
            vel.y += gravity * dt;
        else
            vel.y -= gravity * dt;

        body.move(0.f, vel.y * dt);
        box = body.getGlobalBounds();

        onGround = false;

        for (const auto &w : lvl.walls)
        {
            if (box.intersects(w))
            {
                if (gravityControl) // обычная гравитация (вниз)
                {
                    if (vel.y > 0) // падаем
                    {
                        body.setPosition(body.getPosition().x, w.top - box.height);
                        onGround = true;
                    }
                    else if (vel.y < 0) // удар о потолок
                    {
                        body.setPosition(body.getPosition().x, w.top + w.height);
                    }
                }
                else // перевёрнутая гравитация (вверх)
                {
                    if (vel.y < 0) // падаем вверх
                    {
                        body.setPosition(body.getPosition().x, w.top + w.height);
                        onGround = true;
                    }
                    else if (vel.y > 0) // удар о пол
                    {
                        body.setPosition(body.getPosition().x, w.top - box.height);
                    }
                }
                vel.y = 0;
                box = body.getGlobalBounds();
            }
        }
        for (auto &p : lvl.platforms)
        {
            sf::FloatRect pbox = p.hitbox;
            float playerBottom = box.top + box.height;

            bool insideX =
                box.left + box.width > pbox.left &&
                box.left < pbox.left + pbox.width;

            bool onTop =
                playerBottom >= pbox.top - 2 &&
                playerBottom <= pbox.top + 6;

            if (insideX && onTop && vel.y > 0)
            {
                body.setPosition(
                    body.getPosition().x,
                    pbox.top - box.height);
                vel.y = 0;
                onGround = true;

                body.move(p.delta);

                box = body.getGlobalBounds();
            }
        }
    }
    void update(const Level &lvl, float dt)
    {
        handleInput();              // читаем клавиши
        resolveCollisions(lvl, dt); // двигаем и обрабатываем столкновения
        sf::Vector2f pos = body.getPosition();
        Psprite.setPosition(
            std::round(pos.x + body.getSize().x / 2.f),
            std::round(pos.y + body.getSize().y / 2.f));
        updateAnimation(dt); // обновляем анимацию в зависимости от движения и времени
        float baseScaleX = body.getSize().x / frameWidth;
        float baseScaleY = body.getSize().y / frameHeight;
        if (!gravityControl)
        {
            baseScaleY = -baseScaleY;
        }
        if (vel.x > 0)
        {
            Psprite.setScale(baseScaleX, baseScaleY);
        } // смотрим вправо
        else if (vel.x < 0)
        {
            Psprite.setScale(-baseScaleX, baseScaleY);
        } // смотрим влево
        if (!isMoving || !onGround) // если не двигаемся или в воздухе - стоп звук шагов
        {
            stepSound.stop();
        }
        if (isMoving && onGround)
        {
            if (stepClock.getElapsedTime().asSeconds() >= stepDelay)
            {
                stepSound.play();
                stepClock.restart();
            }
        }
        if (!alive)
        {
            if (deathSound.getStatus() == sf::Sound::Stopped)
            {
                deathSound.play();
            }
        }
    }

    void updateAnimation(float dt)
    {
        int columns = 2;
        if (isMoving)
        {
            animationTimer += dt;

            if (animationTimer >= animationSpeed)
            {
                animationTimer = 0.f;

                currentFrame = (currentFrame + 1) % frameCount;

                int x = (currentFrame % columns) * frameWidth;
                int y = (currentFrame / columns) * frameHeight;

                Psprite.setTextureRect(
                    sf::IntRect(x, y, frameWidth, frameHeight));
            }
        }
        else
        {
            currentFrame = 0;
            Psprite.setTextureRect(sf::IntRect(0, 0, frameWidth, frameHeight));
        }
    }

    void draw(sf::RenderWindow &window)
    {
        // window.draw(body); // рисуем игрока
        window.draw(Psprite); // текстура игрока
    }
};

// -------- GAME (логика игры) --------
struct Game
{
    Level level;   // текущий уровень
    Player player; // игрок

    std::string currentLevelPath;
    bool inited = false;        // грузили ли уже уровень / инициализировались
    bool waitingFor = false;    // ждём нажатия R или таймера (после смерти)
    bool rHandled = false;      // чтобы не сработало два раза
    bool transitioning = false; // Переход игры в другой лвл
    bool swapped = false;       // что бы сменить уровень один раз
    bool ESC = true;            // Проверка нажатия ESC
    bool inPortal = false;      // Проверка времени для портала
    sf::View camera;
    sf::Clock reactionClock; // таймер после смерти (автореспавн через 2 секунды)
    sf::Clock portalClock;
    sf::Clock PlayerLevelClock;
    float transT = 0.f; // таймер для анимации перехода между уровнями (на 1.5 секунды, потом меняем уровень и обратно)
    float transDuration = 1.2f;

    int currentLevel = 1; // какой уровень сейчас в игре

    sf::RectangleShape fade;

    void initFade(const sf::RenderWindow &window)
    {
        sf::Vector2u sz = window.getSize();
        fade.setSize({(float)sz.x, (float)sz.y});
        fade.setPosition(0.f, 0.f);
        fade.setFillColor(sf::Color(0, 0, 0, 0));
    }

    struct Pixel
    {
        sf::RectangleShape rect;
        sf::Vector2f vel;
        float life;
    };
    std::vector<Pixel> pixels;
    void spawnPixels()
    {
        sf::Vector2f pos = player.body.getPosition();
        sf::Vector2f size = player.body.getSize();

        for (int i = 0; i < 20; i++)
        {
            Pixel p;
            p.rect.setSize({size.x / 4.f, size.y / 4.f});
            p.rect.setPosition(
                pos.x + (std::rand() % (int)size.x),
                pos.y + (std::rand() % (int)size.y));
            p.rect.setFillColor(sf::Color::Black);
            p.vel = {
                (std::rand() % 200 - 100) / 10.f,
                (std::rand() % 200 - 100) / 10.f};
            p.life = 1.f;
            pixels.push_back(p);
        }
    }
    void doRactions()
    {
        if (rHandled)
            return;

        rHandled = true;
        waitingFor = false;

        reloadLevel();
    }
    void start(int lvl)
    {
        // Если уже загружен этот же уровень — ничего не делаем
        if (inited && lvl == currentLevel)
            return;

        currentLevel = lvl;

        // Путь к уровню
        currentLevelPath = "assets/levels/level" + std::to_string(lvl) + ".json";

        int ts = 16; // размер тайла

        // Загружаем уровень
        if (!level.loadFromFile(currentLevelPath, ts))
        {
            std::cout << "Failed to load level: " << currentLevelPath << "\n";
            return;
        }

        // ===== Инициализация игрока =====
        if (!player.init())
        {
            std::cout << "Failed to load player texture\n";
        }
        if (!player.loadSounds())
        {
            std::cout << "Failed to load player sounds\n";
        }
        // до player.spawn можно менять параметры игрока, например размер, скорость и т.д., чтобы они применились при спавне
        player.spawn(level.playerSpawn); // ставим игрока в точку спавна, заданную уровнем
        // player.updateSpriteScale();

        inited = true;

        // камера //
        camera.setSize(level.getPixelSize());
        float centerX = level.mapWidth * level.tileSize / 2.0f;
        float centerY = level.mapHeight * level.tileSize / 2.0f;
        camera.setCenter(centerX, centerY);
    }

    void executeTrigger(Level::Trigger &t)
    {
        if (t.action == "remove_spike")
        {
            for (int i = level.spikes.size() - 1; i >= 0; i--)
            {
                if (level.spikes[i].intersects(t.area))
                {
                    level.shipSprite.erase(level.shipSprite.begin() + i);
                    level.spikes.erase(level.spikes.begin() + i);
                }
            }
            for (int i = level.spikesR.size() - 1; i >= 0; i--)
            {
                if (level.spikesR[i].intersects(t.area))
                {
                    level.shipSprite.erase(level.shipSprite.begin() + i);
                    level.spikesR.erase(level.spikesR.begin() + i);
                }
            }
        }
        else if (t.action == "remove_block")
        {
            for (int i = level.walls.size() - 1; i >= 0; i--)
            {
                if (level.walls[i].intersects(t.area))
                {
                    Level::BlockAnim anim;
                    anim.sprite = level.blockSprites[i];
                    anim.duration = 0.2f;
                    anim.appearing = false;

                    level.animBlocks.push_back(anim);

                    level.blockSprites.erase(level.blockSprites.begin() + i);
                    level.walls.erase(level.walls.begin() + i);
                }
            }
        }
        else if (t.action == "spawn_block")
        {
            sf::Sprite s;
            s.setTexture(level.texBlocks);

            int tilesPerRow = level.texBlocks.getSize().x / level.tileSize;

            int tx = t.tile % tilesPerRow;
            int ty = t.tile / tilesPerRow;

            s.setTextureRect({tx * level.tileSize,
                              ty * level.tileSize,
                              level.tileSize,
                              level.tileSize});

            s.setPosition(t.area.left, t.area.top);

            Level::BlockAnim anim;
            anim.sprite = s;
            anim.duration = 0.2f;
            anim.appearing = true;
            anim.sprite.setOrigin(level.tileSize / 2.f, level.tileSize / 2.f);
            anim.sprite.setPosition(
                t.area.left + level.tileSize / 2.f,
                t.area.top + level.tileSize / 2.f);
            anim.sprite.setScale(0.f, 0.f);
            level.animBlocks.push_back(anim);
        }
        else if (t.action == "spawn_spike")
        {
            int tileX = (t.area.left + t.area.width * 0.5f) / level.tileSize;
            int tileY = (t.area.top + t.area.height * 0.5f) / level.tileSize;

            float spikeWidth = level.tileSize * 0.8f;
            float spikeHeight = level.tileSize * 0.1f;

            float spikeX = tileX * level.tileSize + (level.tileSize - spikeWidth) * 0.5f;
            float spikeY = (tileY + 1) * level.tileSize - spikeHeight;

            sf::Sprite s;
            s.setTexture(level.texObj);
            s.setTextureRect({0, 0, level.tileSize, level.tileSize});

            s.setPosition(tileX * level.tileSize, tileY * level.tileSize);

            Level::SpikeAnim anim;

            anim.sprite = s;
            anim.appearing = true;

            anim.startY = s.getPosition().y + level.tileSize;
            anim.endY = s.getPosition().y;

            anim.sprite.setPosition(s.getPosition().x, anim.startY);

            anim.hitbox = {
                spikeX,
                spikeY,
                spikeWidth,
                spikeHeight};
            level.animSpikes.push_back(anim);
        }
        else if (t.action == "spawn_spikeR")
        {
            int tileX = (t.area.left + t.area.width * 0.5f) / level.tileSize;
            int tileY = (t.area.top + t.area.height * 0.5f) / level.tileSize;

            float spikeWidth = level.tileSize * 0.8f;
            float spikeHeight = level.tileSize * 0.3f;

            float spikeX = tileX * level.tileSize + (level.tileSize - spikeWidth) * 0.5f;
            float spikeY = tileY * level.tileSize;

            level.spikesR.emplace_back(
                spikeX,
                spikeY,
                spikeWidth,
                spikeHeight);
            int tilesPerRow = level.texObj.getSize().x / level.tileSize;
            int tx = 4 % tilesPerRow;
            int ty = 4 / tilesPerRow;
            sf::Sprite sprite;
            sprite.setTexture(level.texObj);
            sprite.setTextureRect({tx * level.tileSize,
                                   ty * level.tileSize,
                                   level.tileSize,
                                   level.tileSize});

            sprite.setPosition(tileX * level.tileSize, tileY * level.tileSize);

            level.shipSprite.push_back(sprite);
        }
        else if (t.action == "spawn_block_area")
        {
            int startX = t.area.left / level.tileSize;
            int endX = (t.area.left + t.area.width) / level.tileSize;

            int startY = t.area.top / level.tileSize;
            int endY = (t.area.top + t.area.height) / level.tileSize;

            int tilesPerRow = level.texBlocks.getSize().x / level.tileSize;

            int tx = t.tile % tilesPerRow;
            int ty = t.tile / tilesPerRow;

            for (int y = startY; y <= endY; y++)
            {
                for (int x = startX; x <= endX; x++)
                {
                    float px = x * level.tileSize;
                    float py = y * level.tileSize;

                    sf::Sprite s;
                    s.setTexture(level.texBlocks);

                    s.setTextureRect({tx * level.tileSize,
                                      ty * level.tileSize,
                                      level.tileSize,
                                      level.tileSize});

                    s.setPosition(px, py);

                    Level::BlockAnim anim;

                    anim.sprite = s;
                    anim.duration = 0.2f;
                    anim.appearing = true;

                    anim.sprite.setOrigin(
                        level.tileSize / 2.f,
                        level.tileSize / 2.f);

                    anim.sprite.setPosition(
                        px + level.tileSize / 2.f,
                        py + level.tileSize / 2.f);

                    anim.sprite.setScale(0.f, 0.f);

                    level.animBlocks.push_back(anim);
                }
            }
        }
        else if (t.action == "remove_portal")
        {
            level.hasExit = false;
            level.objectSprites.clear();
            level.spikesCoin.clear();
        }
        else if (t.action == "spawn_portal")
        {
            int tileX = (t.area.left + t.area.width * 0.5f) / level.tileSize;
            int tileY = (t.area.top + t.area.height * 0.5f) / level.tileSize;
            sf::Sprite sprite;
            sprite.setTexture(level.texObj);
            int tilesPerRow = level.texObj.getSize().x / level.tileSize;
            int tx = t.tile % tilesPerRow;
            int ty = t.tile / tilesPerRow;
            sprite.setTextureRect({tx * level.tileSize,
                                   ty * level.tileSize,
                                   level.tileSize,
                                   level.tileSize});
            float scale = 2.f;
            sprite.setScale(scale, scale);
            sprite.setPosition(
                tileX * level.tileSize - level.tileSize * (scale - 1),
                tileY * level.tileSize - level.tileSize * (scale - 1));
            level.objectSprites.push_back(sprite);
            level.hasExit = true;
            level.exitRect =
                {
                    sprite.getPosition().x,
                    sprite.getPosition().y,
                    level.tileSize * scale,
                    level.tileSize * scale};
        }
        else if (t.action == "platform")
        {
            int tileX = t.area.left / level.tileSize;
            int tileY = t.area.top / level.tileSize;

            sf::Sprite s;
            s.setTexture(level.texBlocks);
            s.setTextureRect({0, 0, level.tileSize, level.tileSize});

            s.setPosition(tileX * level.tileSize, tileY * level.tileSize);
            int tilesPerRow = level.texBlocks.getSize().x / level.tileSize;

            int tx = t.tile % tilesPerRow;
            int ty = t.tile / tilesPerRow;

            s.setTextureRect({tx * level.tileSize,
                              ty * level.tileSize,
                              level.tileSize,
                              level.tileSize});
            Level::MovingPlatform p;

            p.sprite = s;
            p.startPos = s.getPosition();
            p.endPos = {
                p.startPos.x + t.dx,
                p.startPos.y + t.dy};

            p.hitbox = {
                p.startPos.x,
                p.startPos.y,
                static_cast<float>(level.tileSize), // Костыль! static_cast если появятся баги написать "level.tileSize"
                static_cast<float>(level.tileSize)};

            level.platforms.push_back(p);
        }
        else if (t.action == "spawn_portalR")
        {
            int tileX = (t.area.left + t.area.width * 0.5f) / level.tileSize;
            int tileY = (t.area.top + t.area.height * 0.5f) / level.tileSize;

            sf::Sprite sprite;
            sprite.setTexture(level.texObj);

            int tilesPerRow = level.texObj.getSize().x / level.tileSize;

            int tx = t.tile % tilesPerRow;
            int ty = t.tile / tilesPerRow;

            sprite.setTextureRect({tx * level.tileSize,
                                   ty * level.tileSize,
                                   level.tileSize,
                                   level.tileSize});

            float scale = 2.f;

            sprite.setScale(scale, -scale); // ← переворот

            sprite.setPosition(
                tileX * level.tileSize - level.tileSize * (scale - 1),
                tileY * level.tileSize + level.tileSize * scale);

            level.hasExit = true;

            level.exitRect = {
                sprite.getPosition().x,
                sprite.getPosition().y - level.tileSize * scale,
                level.tileSize * scale,
                level.tileSize * scale};

            level.objectSprites.push_back(sprite);
        }
        else if (t.action == "clearspikes")
        {
            level.spikes.clear();
            level.shipSprite.clear();
        }
        else if (t.action == "keycontrolF")
        {
            player.KeyContol = false;
        }
        else if (t.action == "keycontrolT")
        {
            player.KeyContol = true;
        }
        else if (t.action == "gravityT")
        {
            player.gravitySound.play();
            player.gravityControl = true;
            player.vel.y = 0;
        }
        else if (t.action == "gravityF")
        {
            player.gravitySound.play();
            player.gravityControl = false;
            player.vel.y = 0;
        }
        else if (t.action == "speed50")
        {
            player.moveSpeed = 50.f;
        }
        else if (t.action == "speed40")
        {
            player.moveSpeed = 40.f;
        }
        else if (t.action == "speed30")
        {
            player.moveSpeed = 30.f;
        }
        else if (t.action == "speed20")
        {
            player.moveSpeed = 20.f;
        }
        else if (t.action == "speed100")
        {
            player.moveSpeed = 100.f;
        }
        else if (t.action == "speed180")
        {
            player.moveSpeed = 180.f;
        }
        else if (t.action == "speed200")
        {
            player.moveSpeed = 200.f;
        }
        else if (t.action == "speed250")
        {
            player.moveSpeed = 250.f;
        }
        else if (t.action == "jump400")
        {
            player.jumpSpeed = 400.f;
        }
        else if (t.action == "jump215")
        {
            player.jumpSpeed = 215.f;
        }
    }

    void reset()
    {
        player.KeyContol = true;
        player.gravityControl = true;
        player.moveSpeed = 100.f;
        player.jumpSpeed = 205.f;
        inited = false;
        waitingFor = false;
        rHandled = false;
        ESC = true;

        player.alive = true;
        player.vel = {0.f, 0.f};
    }

    void handleEvent(const sf::Event &e)
    {
        // Esc — выйти из игры на экран уровней
        if (ESC && e.type == sf::Event::KeyPressed &&
            e.key.code == sf::Keyboard::Escape)
        {
            reset();
            level = Level();
            screen = Screen::Levels;
        }
        if (waitingFor && !rHandled && // если мы в режиме ожидания после смерти и ещё не обработали R
            e.type == sf::Event::KeyPressed &&
            e.key.code == sf::Keyboard::R)
        {
            doRactions();
        }
    }
    void startTransition()
    {
        transitioning = true;
        transT = 0.f;
        swapped = false;
        player.vel = {0.f, 0.f};
    }

    void reloadLevel()
    {
        // Полная очистка уровня
        level.backgroundSprites.clear();
        level.blockSprites.clear();
        level.objectSprites.clear();
        level.shipSprite.clear();
        level.animBlocks.clear();
        level.animSpikes.clear();
        level.walls.clear();
        level.spikes.clear();
        level.spikesR.clear();
        level.spikesCoin.clear();
        level.triggers.clear();
        level.platforms.clear();
        // Перезагрузка уровня
        level.loadFromFile(currentLevelPath, level.tileSize);
        // Сброс игрока
        player.vel = {0.f, 0.f};
        player.KeyContol = true;
        player.gravityControl = true;
        player.moveSpeed = 100.f;
        player.jumpSpeed = 205.f;
        player.spawn(level.playerSpawn);
    }
    void updateTransition(float dt)
    {
        transT += dt;
        float half = transDuration * 0.5f; // половина длительности перехода (0.75 секунды для 1.5 секунд общего времени)
        if (half <= 0.f)
            half = 0.001f;
        // 0..half = затемнение, half..duration = проявление
        float alphaF = 0.f;

        if (transT < half)
        {                            // первый этап перехода — затемнение
            float t = transT / half; // 0..1
            alphaF = 255.f * t;      // 0..255
        }
        else // второй этап перехода — проявление
        {
            // в середине делаем смену уровня один раз
            if (!swapped)
            {
                swapped = true;
                // открыть следующий уровень
                progress.onLevelComleted(currentLevel);
                progress.save(PROGRESS_PATH);

                selectedLevel = currentLevel + 1; // следующий
                reset();                          // сбросить inited и т.д.
                start(selectedLevel);             // загрузить следующий уровень СРАЗУ
                transT = half;
                PlayerLevelClock.restart();
            }
            alphaF = 255.f;
            if (PlayerLevelClock.getElapsedTime().asSeconds() >= 2.f)
            {
                float t = (transT - half) / half; // 0..1
                if (t > 1.f)
                    t = 1.f;
                alphaF = 255.f * (1.f - t); // 255..0
            }
        }
        sf::Uint8 alpha = (sf::Uint8)alphaF;
        fade.setFillColor(sf::Color(0, 0, 0, alpha));

        if (transT >= transDuration)
        {
            transitioning = false;
            fade.setFillColor(sf::Color(0, 0, 0, 0));
        }
    }
    void update(float dt)
    {
        // Если игрок мёртв — либо ждём R, либо автотаймер 2 сек
        if (!player.alive)
        {
            // ОБНОВЛЯЕМ ПИКСЕЛИ ВСЕГДА
            for (auto it = pixels.begin(); it != pixels.end();)
            {
                it->life -= dt;

                if (it->life <= 0.f)
                {
                    it = pixels.erase(it);
                }
                else
                {
                    it->rect.move(it->vel * dt);
                    ++it;
                }
            }

            if (waitingFor && !rHandled)
            {
                if (reactionClock.getElapsedTime().asSeconds() >= 0.5f)
                    doRactions();
            }
            return;
        }
        if (transitioning)
        {
            updateTransition(dt);
            return;
        }
        player.update(level, dt); // обычное обновление игрока
        for (auto it = level.animBlocks.begin(); it != level.animBlocks.end();)
        {
            it->timer += dt;

            float t = it->timer / it->duration;
            if (t > 1.f)
                t = 1.f;

            if (it->appearing)
                it->sprite.setScale(t, t);
            else
                it->sprite.setScale(1.f - t, 1.f - t);

            if (it->timer >= it->duration)
            {
                if (it->appearing)
                {
                    level.blockSprites.push_back(it->sprite);

                    float x = it->sprite.getPosition().x - level.tileSize / 2.f;
                    float y = it->sprite.getPosition().y - level.tileSize / 2.f;
                    sf::FloatRect newWall(
                        x,
                        y,
                        level.tileSize,
                        level.tileSize);
                    level.walls.push_back(newWall);
                    // если игрок внутри блока — вытолкнуть его вверх
                    if (player.body.getGlobalBounds().intersects(newWall))
                    {
                        player.body.setPosition(
                            player.body.getPosition().x,
                            newWall.top - player.body.getGlobalBounds().height);
                        player.vel.y = 0;
                        player.onGround = true;
                    }
                }
                it = level.animBlocks.erase(it);
            }
            else
                ++it;
        }
        for (auto it = level.animSpikes.begin(); it != level.animSpikes.end();)
        {
            it->timer += dt;

            float t = it->timer / it->duration;
            if (t > 1.f)
                t = 1.f;

            float y = it->startY + (it->endY - it->startY) * t;

            float x = it->sprite.getPosition().x;
            it->sprite.setPosition(x, y);

            if (it->timer >= it->duration)
            {
                level.spikes.push_back(it->hitbox);
                level.shipSprite.push_back(it->sprite);
                it = level.animSpikes.erase(it);
            }
            else
            {
                ++it;
            }
        }
        for (auto it = pixels.begin(); it != pixels.end();)
        {
            it->life -= dt;
            if (it->life <= 0.f)
            {
                it = pixels.erase(it);
            }
            else
            {
                it->rect.move(it->vel * dt);
                ++it;
            }
        }
        for (auto &p : level.platforms)
        {
            sf::Vector2f oldPos = p.sprite.getPosition();

            sf::Vector2f pos = oldPos;
            sf::Vector2f dir = p.endPos - pos;

            float len = sqrt(dir.x * dir.x + dir.y * dir.y);

            if (len < p.speed * dt)
            {
                p.sprite.setPosition(p.endPos);

                p.delta = p.endPos - oldPos; // ← ДОБАВИТЬ

                p.hitbox.left = p.endPos.x;
                p.hitbox.top = p.endPos.y;

                p.speed = 0.f;
            }
            else
            {
                dir /= len;

                pos += dir * p.speed * dt;

                p.sprite.setPosition(pos);

                p.delta = pos - oldPos; // ← ДОБАВИТЬ

                p.hitbox.left = pos.x;
                p.hitbox.top = pos.y;
            }
        }
        sf::FloatRect box = player.body.getGlobalBounds();
        sf::Vector2i playerTile(
            (box.left + box.width * 0.5f) / level.tileSize,
            (box.top + box.height * 0.5f) / level.tileSize);

        auto playerBox = player.body.getGlobalBounds();

        // Проверка триггеров
        std::vector<Level::Trigger *> activatedNow;

        for (auto &t : level.triggers)
        {
            // обработка задержанных триггеров
            if (t.pending)
            {
                t.timer -= dt;

                if (t.timer <= 0.f)
                {
                    executeTrigger(t);
                    t.pending = false;
                }
                continue;
            }

            if (t.once && t.activated)
                continue;

            if (playerBox.intersects(t.area) && !t.activated)
            {
                activatedNow.push_back(&t);
                t.activated = true;
            }
        }
        // запускаем chain после обнаружения всех триггеров
        for (auto *t : activatedNow)
        {
            if (t->action == "chain_start")
            {
                for (auto &other : level.triggers)
                {
                    if (other.chain == t->chain)
                    {
                        other.pending = true;
                        other.timer = other.delay;
                    }
                }
            }
        }
        for (auto &spike : level.spikes)
        {
            if (playerBox.intersects(spike))
            {
                player.alive = false;
                spawnPixels();
                waitingFor = true;
                rHandled = false;
                Dead++;
                player.uronSound.play();
                reactionClock.restart();
                return;
            }
        }
        for (auto &spikeR : level.spikesR)
        {
            if (playerBox.intersects(spikeR))
            {
                player.alive = false;
                spawnPixels();
                waitingFor = true;
                rHandled = false;
                Dead++;
                player.uronSound.play();
                reactionClock.restart();
                return;
            }
        }
        for (auto &spikeCoin : level.spikesCoin)
        {
            if (playerBox.intersects(spikeCoin))
            {
                player.alive = false;
                spawnPixels();
                waitingFor = true;
                rHandled = false;
                Dead++;
                player.uronSound.play();
                reactionClock.restart();
                return;
            }
        }
        for (int i = level.coins.size() - 1; i >= 0; i--) // проверяем пересечение с монетками, если пересекаем — удаляем монетку
        {
            if (playerBox.intersects(level.coins[i].hitbox))
            {
                player.coinSound.play();
                level.coins.erase(level.coins.begin() + i);
            }
        }
        // --- Финиш уровня --- //
        if (level.hasExit && playerBox.intersects(level.exitRect))
        {
            if (!inPortal)
            {
                inPortal = true;
                ESC = false;
                portalClock.restart();
            }
            if (!player.isMoving)
            {
                if (portalClock.getElapsedTime().asSeconds() >= 1.f)
                {
                    if (!transitioning)
                    {
                        progress.onLevelComleted(currentLevel);
                        progress.save(PROGRESS_PATH);
                        startTransition();
                    }
                }
            }
            return;
        }
        else
        {
            inPortal = false; // вышел из портала — сброс
            ESC = true;       // разрешаем ESC снова
        }
        if (player.body.getPosition().y > level.mapHeight * level.tileSize + 300.f)
        {
            player.alive = false; // умер
            waitingFor = true;    // включаем режим ожидания
            rHandled = false;     // разрешаем обработку R/таймера
            Dead++;
            reactionClock.restart(); // старт таймера 2 секунды
        }
    }
    void draw(sf::RenderWindow &window)
    {
        if (currentLevel == 5)
        {
            window.clear(sf::Color(26, 22, 15));
        }
        else if (currentLevel == 10)
        {
            window.clear(sf::Color(26, 22, 15));
        }
        else if (currentLevel == 15)
        {
            window.clear(sf::Color(26, 22, 15));
        }
        else if (currentLevel == 20)
        {
            window.clear(sf::Color(26, 22, 15));
        }
        else
        {
            window.clear(sf::Color(255, 156, 0));
        }
        window.setView(camera);
        level.draw(window);
        for (auto &p : pixels)
        {
            window.draw(p.rect);
        }
        if (player.alive)
        {
            player.draw(window);
        }
        window.setView(window.getDefaultView());
        window.draw(fade);
    }
};

int main()
{
    sf::RenderWindow window(sf::VideoMode(1440, 900), "One-file prototype");
    window.setFramerateLimit(144);
    srand((unsigned)time(nullptr));
    MenuUI menu;
    menu.init();                        // Инициализация кнопок/шрифта
    progress.load(PROGRESS_PATH);       // Загружаем сохранение (если есть)
    selectedLevel = progress.lastLevel; // Стартово выбираем последний уровень

    Game game;             // логика игры
    game.initFade(window); // Инициализация прямоугольника для затемнения при переходе между уровнями
    sf::Clock clock;       // Часы для dt (delta time)

    while (window.isOpen()) // Главный цикл игры
    {
        float dt = clock.restart().asSeconds(); // Сколько секунд прошло с прошлого кадра
        sf::Event e;
        // Обрабатываем все события окна
        while (window.pollEvent(e))
        {
            if (e.type == sf::Event::Closed)
                window.close();

            // Разная обработка в зависимости от экрана
            if (screen == Screen::Game)
                game.handleEvent(e);
            else
                menu.handleEvent(e, window);
            menu.update(window);
        }
        // Обновление логики
        if (screen == Screen::Game)
        {
            game.start(selectedLevel); // Важно! сейчас вызывается каждый кадр, но внутри защита inited
            game.update(dt);
        }
        window.clear(sf::Color(218, 165, 32)); // <-- ВНЕ КАРТЫ (основной цвет)
        // Рисуем в зависимости от экрана
        if (screen == Screen::Game)
        {
            game.draw(window);
        }
        else
        {
            window.setView(window.getDefaultView()); // Возвращаем обычный view для UI
            menu.draw(window);
        }
        window.display(); // Показываем отрисованный кадр
    }
}
