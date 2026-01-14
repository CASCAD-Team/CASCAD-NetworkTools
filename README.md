# CASCAD-NetworkTools
______________________
Универсальная сетевая утилита, поддерживающая UDP, TCP и FTP, упрощает работу с удалёнными серверами.
Разработана под Linux (EndeavourOS)

# Установка

В релизах можно скачать универсальный исполняемый файл для Linux, однако если вас что то не устраивает, то следуйте инструкции ниже.

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



