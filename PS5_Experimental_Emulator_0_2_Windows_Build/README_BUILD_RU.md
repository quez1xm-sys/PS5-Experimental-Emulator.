# PS5 Experimental Emulator 0.2 — получение EXE

Это демонстрационная оболочка Windows/DirectX 12, а не настоящий эмулятор PlayStation 5. Карточка GTA VI показывает статус игры; запуск GTA VI не реализован.

## Самый простой способ получить EXE без установки Visual Studio на своём ПК

Сборка выполняется на бесплатном Windows runner GitHub Actions. Для этого нужен аккаунт GitHub.

1. Создайте на GitHub новый **публичный** репозиторий (например, `PS5-Experimental-Emulator`).
2. Распакуйте этот архив. Загрузите в репозиторий всё содержимое папки `PS5_Experimental_Emulator_0_1`, а также папку `.github` и файл `README_BUILD_RU.md`. В корне репозитория должны лежать `CMakeLists.txt`, `main.cpp` и `.github/workflows/build-windows.yml`.
3. Откройте вкладку **Actions** репозитория.
4. Выберите **Build Windows EXE** → **Run workflow** → **Run workflow**.
5. Дождитесь зелёной галочки. Откройте завершённый запуск и внизу раздел **Artifacts**.
6. Скачайте `PS5ExperimentalEmulator-Windows-x64.zip`, распакуйте его. Внутри будет `PS5ExperimentalEmulator.exe`.

GitHub соберёт программу на Windows; Visual Studio на вашем компьютере для этого не нужна. Сам EXE после сборки запускается двойным щелчком. Если Windows покажет предупреждение SmartScreen, проверяйте, что файл скачан из вашего собственного запуска GitHub Actions.

## Локальная сборка (если Visual Studio уже установлена)

Запустите `BUILD_RELEASE.bat`. Результат: `build\\Release\\PS5ExperimentalEmulator.exe`.

## Важно

На текущей среде я не могу скомпилировать настоящий Windows EXE: здесь нет Windows SDK/MSVC. Поэтому в архиве нет поддельного или пустого `.exe`; workflow выше создаёт настоящий исполняемый файл на Windows и прикладывает его к запуску.
