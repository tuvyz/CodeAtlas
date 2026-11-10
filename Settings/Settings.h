#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <SDL.h>
#include "Global/Strings.h"


// Настройки программы (общие для всех проектов): размеры подписей и блоков на холсте, связи, колесо, разбор.
// Хранятся в settings.txt рядом с программой (папка недоступна для записи - в папке пользователя), в проект не пишется ничего
struct Settings {
    double fileTitleSize = 40;   // Высота имени файла, единицы холста - масштабируется с картой (полоса имени - в 1.8 раза выше)
    double folderTitleSize = 22; // Высота имени папки верхнего уровня, единицы холста (вложенные мельче, полоса - в 1.8 раза выше)
    double nodeTextSize = 13;    // Высота имени в блоке, единицы холста (блок растёт вместе с ним)
    double noteTextSize = 15;    // Высота текста пометки, единицы холста
    double edgeThickness = 1.1;  // Толщина связи, пиксели экрана (у выбранного блока - в 1.6 раза толще)
    double wheelZoomStep = 18;   // Шаг масштаба колесом, проценты
    int scanThreads = 0;         // Потоков разбора (0 - половина ядер, не больше 8)
    int nodeSizeMode = 1;        // Размер блока функции (вместе с именем): 0 - один у всех, 1 - ступенями по логарифму числа строк, 2 - площадь по числу строк
    int ideKind = 0;             // IDE для «Открыть в IDE»: 0 - Qt Creator, 1 - Visual Studio, 2 - своя команда (ideCommand)
    std::wstring ideCommand;     // Своя команда открытия файла с {file}, {line}, {project}
    double linesPerStep = 20;    // Строк кода на ступень: в логарифме с него блок в 1.5 раза больше, каждая следующая ступень (+0.5) - втрое больше строк;
                                 // по строкам - столько строк у блока обычного размера, площадь растёт с числом строк

    // Загрузка (нет файла - значения по умолчанию) и сохранение
    void load();
    bool save() const;

    // Файл настроек
    static std::filesystem::path filePath();
};

// Настройки программы (одни на процесс)
Settings &settings();


#endif // SETTINGS_H
