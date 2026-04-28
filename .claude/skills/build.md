# Сборка проекта (Build Project)

Этот навык описывает, как компилировать мод `cp2077-wheel-mod`.

## Требования для сборки
Для успешной компиляции проекта на вашей машине должно быть установлено следующее:
1. **Visual Studio 2022** (рекомендуется Community, Professional или Enterprise версия)
2. Нагрузка (workload) **"Desktop development with C++"** (Разработка классических приложений на C++)
3. **CMake 3.21+**, добавленный в системный `PATH`, **ИЛИ** нагрузка **"C++ CMake tools for Windows"** в Visual Studio.

## Как собрать проект
Проект использует PowerShell-скрипт `build.ps1` для инициализации сабмодулей, сборки форка `mod_settings.dll` и последующей сборки самого ядра мода `direct_wheel.dll`.

### Шаги:
1. Откройте терминал (PowerShell или Developer Command Prompt для VS 2022) в корневой папке проекта.
2. Выполните команду:
   ```powershell
   powershell -ExecutionPolicy Bypass -File build.ps1
   ```
   По умолчанию проект собирается в конфигурации `Release`.

3. Если вам нужна отладочная сборка (для получения бэктрейсов или более детального логирования), используйте:
   ```powershell
   powershell -ExecutionPolicy Bypass -File build.ps1 -Config Debug
   ```

### Результат сборки
В случае успешной компиляции:
- Основной DLL появится по пути: `build/direct_wheel/<Config>/direct_wheel.dll`
- DLL зависимость (форк mod_settings) будет скомпилирована в: `vendor/mod_settings/build/Release/mod_settings.dll`

После успешной сборки можно запустить `deploy.ps1` для упаковки мода или автоматического копирования в папку с игрой.

## Частые ошибки (Troubleshooting)
- **`CMake not found`**: Убедитесь, что CMake установлен и прописан в переменных среды (PATH), либо что вы установили компонент "C++ CMake tools for Windows" через Visual Studio Installer.
- Ошибка поиска путей компилятора MSVC: попробуйте запускать скрипт из "x64 Native Tools Command Prompt for VS 2022" или Developer PowerShell.
