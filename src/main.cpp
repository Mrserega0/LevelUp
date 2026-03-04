#include <SFML/Graphics.hpp> // Подключаем графику SFML: окно, спрайты/фигуры, текст, события // version 0.5
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

static float clampf(float v, float lo, float hi)
{
    // clamp: ограничивает v диапазоном [lo..hi]
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

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

        // Защита от некорректных значений (например, 0 или -5)
        unlockedLevel = std::max(1, unlockedLevel);
        lastLevel = std::max(1, lastLevel);
        return true;
    }

    bool save(const std::string &path) const
    {
        std::ofstream f(path); // Открываем файл на запись (перезапишет)
        if (!f.is_open())
            return false;

        f << unlockedLevel << " " << lastLevel; // Пишем два числа
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
    sf::Font font; // Шрифт для всех текстов меню (Play, Exit, Back, Level X)

    sf::RectangleShape playBtn, exitBtn; // Кнопки главного меню
    sf::RectangleShape backBtn;          // Кнопка "Back" на экране уровней

    sf::Text playTxt, exitTxt, backTxt; // Тексты для кнопок

    std::vector<LevelButton> levelButtons; // Список кнопок уровней

    void makeButton(sf::RectangleShape &r, sf::Text &t,
                    sf::Vector2f pos, const sf::String &label)
    {
        // Настройка прямоугольника кнопки
        r.setSize({200, 60});                  // Размер кнопки
        r.setPosition(pos);                    // Позиция на экране
        r.setFillColor(sf::Color(60, 60, 60)); // Цвет заливки
        r.setOutlineThickness(2);              // Толщина рамки
        r.setOutlineColor(sf::Color::White);   // Цвет рамки

        // Настройка текста кнопки
        t.setFont(font);                  // Шрифт
        t.setString(label);               // Надпись
        t.setCharacterSize(28);           // Размер букв
        t.setFillColor(sf::Color::White); // Цвет текста

        // Центрируем текст внутри прямоугольника
        auto b = t.getLocalBounds();                             // Границы текста (локальные)
        t.setOrigin(b.left + b.width / 2, b.top + b.height / 2); // Ставим origin в центр текста
        t.setPosition(pos.x + 100, pos.y + 30);                  // Ставим текст в центр кнопки (200x60 => +100 +30)
    }

    void initLevels(int maxLevels)
    {
        for (int i = 1; i <= maxLevels; ++i)
        {
            LevelButton lb;
            lb.level = i; // номер уровня

            // Создаём кнопку и текст для уровня
            makeButton(lb.btn, lb.txt,
                       {250.f, -10.f + i * 80.f},     // позиция (лесенкой вниз)
                       "Level " + std::to_string(i)); // текст

            levelButtons.push_back(lb); // добавляем в список
        }
    }

    void init()
    {
        font.loadFromFile("assets\\fonts\\COOPBL.TTF"); // Загружаем шрифт из файла

        // Создаём кнопки главного меню
        makeButton(playBtn, playTxt, {540, 260}, "Play");
        makeButton(exitBtn, exitTxt, {540, 340}, "Exit");
        // Кнопка назад (на экране выбора уровней)
        makeButton(backBtn, backTxt, {540, 530}, "Back");

        initLevels(5);
    }

    void handleEvent(const sf::Event &e, sf::RenderWindow &window)
    {
        // Реагируем ТОЛЬКО на нажатие ЛКМ
        if (e.type != sf::Event::MouseButtonPressed ||
            e.mouseButton.button != sf::Mouse::Left)
            return;

        // Координаты мыши переводим в координаты мира/вида (учитывает view)
        sf::Vector2f m = window.mapPixelToCoords(
            {e.mouseButton.x, e.mouseButton.y});

        if (screen == Screen::Main)
        {
            // Клик по Play => перейти на экран уровней
            if (playBtn.getGlobalBounds().contains(m))
                screen = Screen::Levels;

            // Клик по Exit => закрыть окно
            if (exitBtn.getGlobalBounds().contains(m))
                window.close();
        }
        else if (screen == Screen::Levels)
        {
            // Back => вернуться в главное меню
            if (backBtn.getGlobalBounds().contains(m))
                screen = Screen::Main;

            // Проверяем клик по каждой кнопке уровня
            for (auto &lb : levelButtons)
            {
                if (lb.btn.getGlobalBounds().contains(m))
                {
                    // Запускаем уровень только если он открыт по прогрессу
                    if (progress.unlockedLevel >= lb.level)
                    {
                        selectedLevel = lb.level; // запоминаем выбор

                        // lastLevel нужен, чтобы в следующий запуск помнить выбранный уровень
                        progress.lastLevel = lb.level;
                        progress.save(PROGRESS_PATH); // сохраняем

                        screen = Screen::Game; // переходим в игру
                    }
                }
            }
        }
    }

    void draw(sf::RenderWindow &window)
    {
        if (screen == Screen::Main)
        {
            // Рисуем 2 кнопки и их текст
            window.draw(playBtn);
            window.draw(playTxt);
            window.draw(exitBtn);
            window.draw(exitTxt);
        }
        else if (screen == Screen::Levels)
        {
            // Рисуем уровни циклом
            for (auto &lb : levelButtons)
            {
                bool unlocked = progress.unlockedLevel >= lb.level; // доступен ли уровень

                // Меняем цвета в зависимости от доступности
                lb.btn.setFillColor(unlocked ? sf::Color(60, 60, 60)
                                             : sf::Color(40, 40, 40));
                lb.txt.setFillColor(unlocked ? sf::Color::White
                                             : sf::Color(120, 120, 120));

                window.draw(lb.btn);
                window.draw(lb.txt);
            }

            // Back рисуем отдельно
            window.draw(backBtn);
            window.draw(backTxt);
        }
    }
};
// ---------------- LEVEL ----------------
struct Level
{
    int tileSize = 32;
    int mapWidth = 0;  // ширина карты в тайлах (не в пикселях!)
    int mapHeight = 0; // высота карты в тайлах (не в пикселях!)

    // ===== Коллизия =====
    std::vector<sf::FloatRect> walls;  // прямоугольники стен для коллизий (на основе блоков)
    std::vector<sf::FloatRect> spikes; // прямоугольники шипов для коллизий (на основе объектов)

    // ===== Графика =====
    std::vector<sf::Sprite> backgroundSprites; // спрайты для тайлов фона (не коллизия, просто графика)
    std::vector<sf::Sprite> blockSprites;      // спрайты для тайлов блоков (стены, коллизия)
    std::vector<sf::Sprite> objectSprites;     // спрайты для тайлов объектов (шипы, портал, спавн - могут быть коллизией, а могут просто графикой)

    // ===== Текстуры =====
    sf::Texture texBlocks; // текстура для блоков (стен)
    sf::Texture texFons;   // текстура для фонов (графика без коллизии)
    sf::Texture texObj;    // текстура для объектов (шипы, портал, спавн)

    // ===== Логика =====
    sf::Vector2f playerSpawn{0.f, 0.f}; // Координаты спавна игрока (на основе объекта "спавн" в JSON)
    sf::FloatRect exitRect;             // Прямоугольник выхода (на основе объекта "портал" в JSON)
    bool hasExit = false;               // Есть ли вообще выход на уровне (на всякий случай, если в JSON нет портала)

    struct TilesetInfo // Информация о тайлсете из JSON (firstgid + указатель на текстуру), чтобы правильно отображать тайлы из разных тайлсетов
    {
        int firstGid;         // Первый GID этого тайлсета (из JSON)
        sf::Texture *texture; // Указатель на текстуру этого тайлсета
    };

    std::vector<TilesetInfo> tilesets; // Список тайлсетов, которые используются на уровне (чтобы по GID понять, какую текстуру использовать)

    bool loadFromFile(const std::string &path, int newTileSize)
    {
        tileSize = newTileSize; // Устанавливаем размер тайла (на всякий случай, если нужно изменить)
        // Очищаем все данные, чтобы загрузить новый уровень (на случай, если эта функция вызывается повторно для другого уровня)
        backgroundSprites.clear();
        blockSprites.clear();
        objectSprites.clear();
        walls.clear();
        spikes.clear();
        hasExit = false;

        if (!texBlocks.loadFromFile("assets/img/blocks.png"))
            return false;
        if (!texFons.loadFromFile("assets/img/fons.png"))
            return false;
        if (!texObj.loadFromFile("assets/img/obj.png"))
            return false;

        texBlocks.setSmooth(false);
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
                objectSprites.push_back(sprite);
                if (currentSet->texture == &texObj)
                {
                    if (localId == 0) // шип
                    {
                        float spikeWidth = tileSize * 0.8f;
                        float spikeHeight = tileSize * 0.5f;

                        float spikeX = x * tileSize + (tileSize - spikeWidth) * 0.5f;
                        float spikeY = y * tileSize + tileSize - spikeHeight;

                        spikes.emplace_back(
                            spikeX,
                            spikeY,
                            spikeWidth,
                            spikeHeight);
                    }
                    else if (localId == 1) // портал
                    {
                        hasExit = true;
                        exitRect = {
                            x * (float)tileSize,
                            y * (float)tileSize,
                            (float)tileSize,
                            (float)tileSize};
                    }
                    else if (localId == 2) // спавн
                    {
                        playerSpawn = {
                            x * (float)tileSize,
                            y * (float)tileSize};
                    }
                }
            }
        }
    }

    sf::Vector2f getPixelSize() const
    {
        return {
            mapWidth * (float)tileSize,
            mapHeight * (float)tileSize};
    }

    void draw(sf::RenderWindow &window)
    {
        for (auto &s : backgroundSprites)
            window.draw(s);

        for (auto &s : blockSprites)
            window.draw(s);

        for (auto &s : objectSprites)
            window.draw(s);
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
    bool onGround = false; // Стоим ли на земле (можно ли прыгать)
    bool alive = true;     // Жив ли игрок (для падения/смерти)
    bool isMoving = false; // Двигается ли игрок (для анимации)

    float moveSpeed = 180.f; // Скорость движения по X
    float jumpSpeed = 430.f; // Сила прыжка
    float gravity = 1200.f;  // Гравитация
    float animationTimer = 0.f;
    float animationSpeed = 0.08f; // Время между кадрами анимации (в секундах)

    const int frameWidth = 32;
    const int frameHeight = 32;
    const int frameCount = 4;
    int currentFrame = 0; // Текущий кадр анимации

    bool init()
    {
        if (!Ptexture.loadFromFile("assets/img/playerAnimation.png"))
            return false;

        Psprite.setTexture(Ptexture);
        Psprite.setTextureRect(sf::IntRect(0, 0, frameWidth, frameHeight));
        Ptexture.setSmooth(false);
        return true;
    }

    void spawn(sf::Vector2f pos)
    {
        spawnPoint = pos;                           // Запоминаем точку спавна
        body.setSize({18.f, 22.f});                 // 26,30 deffault. Размер игрока (меньше тайла)
        updateSpriteScale();                       // Обновляем масштаб спрайта, чтобы он соответствовал размеру тела
        body.setPosition(pos.x + 3.f, pos.y + 2.f); // Смещение, чтобы “лежал красиво” в тайле
        vel = {0.f, 0.f};                           // Сбрасываем скорость
        onGround = false;                           // В воздухе до столкновения
        alive = true;                               // Жив
    }

    void updateSpriteScale()
    {
        float scaleX = body.getSize().x / frameWidth;
        float scaleY = body.getSize().y / frameHeight;
        Psprite.setScale(scaleX, scaleY);
    }

    void respawn()
    {
        // Возвращаем игрока в spawnPoint и сбрасываем физику
        body.setPosition(spawnPoint.x + 3.f, spawnPoint.y + 2.f);
        vel = {0.f, 0.f};
        onGround = false;
        alive = true;
    }

    void handleInput()
    {
        isMoving = false;
        float dir = 0.f; // направление по X: -1 влево, +1 вправо
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
            vel.y = -jumpSpeed; // вверх (в SFML Y вниз, поэтому минус)
            onGround = false;   // мы в воздухе
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
        vel.y += gravity * dt;
        body.move(0.f, vel.y * dt);
        box = body.getGlobalBounds();

        onGround = false;

        for (const auto &w : lvl.walls)
        {
            if (box.intersects(w))
            {
                if (vel.y > 0) // падаем
                {
                    body.setPosition(body.getPosition().x, w.top - box.height);
                    onGround = true;
                }
                else if (vel.y < 0) // удар головой
                {
                    body.setPosition(body.getPosition().x, w.top + w.height);
                }

                vel.y = 0.f;
                box = body.getGlobalBounds();
            }
        }
    }

    void update(const Level &lvl, float dt)
    {
        handleInput();              // читаем клавиши
        resolveCollisions(lvl, dt); // двигаем и обрабатываем столкновения
        sf::Vector2f pos = body.getPosition();
        Psprite.setPosition(std::round(pos.x), std::round(pos.y));
        updateAnimation(dt);
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
    sf::View camera;            // Камера которая следует за игроком
    sf::Vector2f camPos;
    sf::Clock reactionClock; // таймер после смерти (автореспавн через 2 секунды)
    sf::Clock portalClock;
    sf::Clock PlayerLevelClock;
    float transT = 0.f; // таймер для анимации перехода между уровнями (на 1.5 секунды, потом меняем уровень и обратно)
    float transDuration = 1.5f;

    int currentLevel = 1; // какой уровень сейчас в игре

    sf::RectangleShape fade;

    void initFade(const sf::RenderWindow &window)
    {
        sf::Vector2u sz = window.getSize();
        fade.setSize({(float)sz.x, (float)sz.y});
        fade.setPosition(0.f, 0.f);
        fade.setFillColor(sf::Color(0, 0, 0, 0));
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

        int ts = 32; // размер тайла

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
        // до player.spawn можно менять параметры игрока, например размер, скорость и т.д., чтобы они применились при спавне
        player.spawn(level.playerSpawn); // ставим игрока в точку спавна, заданную уровнем
        // player.updateSpriteScale();

        // ===== Камера =====
        camera = sf::View(sf::FloatRect(0.f, 0.f, 640.f, 360.f));
        camPos = camera.getCenter();

        inited = true;
    }

    void reset()
    {
        // Сброс состояния игры при выходе в меню уровней / после финиша
        inited = false;
        waitingFor = false;
        rHandled = false;
        ESC = true;
    }

    void handleEvent(const sf::Event &e)
    {
        // Esc — выйти из игры на экран уровней
        if (ESC && e.type == sf::Event::KeyPressed &&
            e.key.code == sf::Keyboard::Escape)
        {
            reset();
            screen = Screen::Levels;
        }

        // Если мы “мертвы/ждём” и нажали R — делаем респавн
        if (waitingFor && !rHandled &&
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
        level.loadFromFile(currentLevelPath, level.tileSize);
        player.spawn(level.playerSpawn);
    }

    void updateTransition(float dt)
    {
        transT += dt;
        float half = transDuration * 0.5f;
        if (half <= 0.f)
            half = 0.001f;

        // 0..half = затемнение, half..duration = проявление
        float alphaF = 0.f;

        if (transT < half)
        {
            float t = transT / half; // 0..1
            alphaF = 255.f * t;      // 0..255
        }
        else
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
            if (waitingFor && !rHandled)
            {
                if (reactionClock.getElapsedTime().asSeconds() >= 2.f)
                    doRactions();
            }
            return; // пока мёртв — не обновляем физику
        }

        if (transitioning)
        {
            updateTransition(dt);
            return;
        }

        player.update(level, dt); // обычное обновление игрока

        sf::FloatRect box = player.body.getGlobalBounds();

        sf::Vector2i playerTile(
            (box.left + box.width * 0.5f) / level.tileSize,
            (box.top + box.height * 0.5f) / level.tileSize);

        auto playerBox = player.body.getGlobalBounds();
        for (auto &spike : level.spikes)
        {
            if (playerBox.intersects(spike))
            {
                player.alive = false;
                waitingFor = true;
                rHandled = false;
                reactionClock.restart();
                return;
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
            if (portalClock.getElapsedTime().asSeconds() >= 1.f)
            {
                if (!transitioning)
                {
                    progress.onLevelComleted(currentLevel);
                    progress.save(PROGRESS_PATH);
                    startTransition();
                }
            }
            return;
        }
        else
        {
            inPortal = false; // вышел из портала — сброс
            ESC = true;       // разрешаем ESC снова
        }

        // --- Камера ---
        sf::Vector2f mapPx = level.getPixelSize();    // размер карты в пикселях
        sf::Vector2f viewSize = camera.getSize();     // размер камеры (обычно 1280x720)
        sf::Vector2f pos = player.body.getPosition(); // позиция игрока
        float halfW = viewSize.x * 0.5f;              // половина ширины view
        float halfH = viewSize.y * 0.5f;              // половина высоты view
        float margin = 200.f;
        float follow = 0.09f; // 0.05 - медленно, 0.15 - быстрее
        float t = 1.f - std::exp(-follow * dt);
        // Центр игрока (позиция + половина размера тела)
        sf::Vector2f target = player.body.getPosition() + player.body.getSize() / 2.f;

        // Если карта меньше экрана — держим камеру по центру карты (иначе clamp сломается)
        if (mapPx.x <= viewSize.x)
            target.x = mapPx.x * 0.5f;
        if (mapPx.y <= viewSize.y)
            target.y = mapPx.y * 0.5f;

        // Ограничиваем камеру границами карты, чтобы не “смотреть за пределы”
        target.x = clampf(target.x, halfW - margin, mapPx.x - halfW + margin);
        target.y = clampf(target.y, halfH - margin, mapPx.y - halfH + margin);

        camPos += (target - camPos) * follow;
        // ограничиваем
        camPos.x = clampf(camPos.x, halfW - margin, mapPx.x - halfW + margin);
        camPos.y = clampf(camPos.y, halfH - margin, mapPx.y - halfH + margin);
        // а в view пишем округлённую
        camera.setCenter(std::round(camPos.x), std::round(camPos.y));
        // --- “Смерть” от падения вниз за карту ---
        if (player.body.getPosition().y > level.mapHeight * level.tileSize + 600.f)
        {
            player.alive = false;    // умер
            waitingFor = true;       // включаем режим ожидания
            rHandled = false;        // разрешаем обработку R/таймера
            reactionClock.restart(); // старт таймера 2 секунды
        }
    }

    void draw(sf::RenderWindow &window)
    {
        window.setView(camera);
        level.draw(window);
        player.draw(window);

        window.setView(window.getDefaultView());
        if (transitioning)
        {
            window.draw(fade);
        }
    }
};

int main()
{
    sf::RenderWindow window(sf::VideoMode(1280, 720), "One-file prototype");
    window.setFramerateLimit(60);
    srand((unsigned)time(nullptr));
    MenuUI menu;
    menu.init();                        // Инициализация кнопок/шрифта
    progress.load(PROGRESS_PATH);       // Загружаем сохранение (если есть)
    selectedLevel = progress.lastLevel; // Стартово выбираем последний уровень

    Game game; // логика игры
    game.initFade(window);
    sf::Clock clock; // Часы для dt (delta time)

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
        }

        // Обновление логики
        if (screen == Screen::Game)
        {
            game.start(selectedLevel); // Важно: сейчас вызывается каждый кадр, но внутри защита inited
            game.update(dt);
        }
        window.clear(sf::Color(8, 8, 10)); // <-- ВНЕ КАРТЫ (основной цвет)
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
