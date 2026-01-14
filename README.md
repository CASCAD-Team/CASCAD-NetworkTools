# CASCAD-NetworkTools
______________________
Универсальная сетевая утилита, поддерживающая UDP, TCP и FTP, упрощает работу с удалёнными серверами.
Разработана под Linux (EndeavourOS)

# Установка
Клонируем репозиторий:
```
git clone https://github.com/Dobkeshan/CASCAD-NetworkTools.git
```
Далее переходим в папку и устанавливаем все необходимые пакеты:

Вариант для Arch based систем:
```
cd CASCAD-NetworkTools
sudo pacman -Syu
sudo pacman -S qt6
sudo pacman -S qtcreator (опционально)
sudo pacman -S curl
```
Далее, если вы в Qt Creator, то нажимаете на кнопку запусука и видите программу.

Вариант для Debian based систем:
Хоть и эта утилита была написана на EndeavourOS, но так же её можно запустить на Debian, однако все пакеты установив через apt
```
sudo apt update
sudo apt upgrade
sudo apt install build-essential qt6-base-dev curl libcurl4-openssl-dev
sudo apt install clang
sudo apt install cmake
sudo apt intall qtcreator (опционально)
```
Далее, в Qt Creator или в любой другой программе нажимаете на кнопку запуска main.cpp.

# Использование
Когда вы запустили программу, вы видите такое окно:
<img width="793" height="588" alt="image" src="https://github.com/user-attachments/assets/cd1f66fd-8477-4154-b913-42e039d42093" />

Вы можете переключаться между вкладками, для работы с разными средствами, на видео показан пример использования протокола FTP:

https://github.com/user-attachments/assets/9281fbb7-cf99-46b2-a08d-a886ea0c7559

В других режимах, работа гораздо проще, вы вводите адрес и получаете ответ.

# Поддерживаемые системы
Arch based системы (EndeavourOS, Manjaro Linux и. т. д.)
Debian based системы (Ubuntu, ElementaryOS, Ubuntu, Kali Linux)

# Возможные ошибки и их решения

1. Не найдены пакеты в apt:
 
Смените или обновите зеркало
2. Не найдены пакеты в pacman

Попробуйте AUR
4. Предупреждения при сборке

Игнорируйте их

5. Сборка не удалась, программа вылетает, не работает что-то

Открываете Issue в этом репозитории и пишете о своей проблеме, наша команда ответит максимум через два дня

6. Не работает на Windows

Ставь Линукс

7. Не работает на Fedora

Ставь Endeavour или Arch

8. Skill issue

Тут только бог поможет






