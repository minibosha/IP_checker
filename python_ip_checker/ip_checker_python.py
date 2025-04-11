import subprocess, os, time, pymem

app_lock_file = "ip_checker_v2.1(1).exe"

while True:
    try:
        pymem.Pymem(app_lock_file)
        os.remove(app_lock_file)
    except:
        True

    address_of_file: str = r'C:\554\ip.txt'
    try:
        file = open(address_of_file, 'r', encoding='utf-8')  # считывание файла
    except (FileNotFoundError, ValueError):
        exit(0) # Выход при программе если файл не найден или с ошибкой

    counter = 0  # создание счётчика
    for line in file:  # проверка и распределение в файле
        if line.strip():
            if counter == 0:
                ip = line.strip()
                counter += 1
            elif counter == 1:
                cm = line.strip()

    command = 'ping ' + ip
    # Чтение ответа от командной строки
    returned_output = subprocess.check_output(command)
    command_answer = returned_output.decode("CP866")

    # Действие на основе ответа от командной строки
    if 'Заданный узел недоступен.' in command_answer:
        True
    else:
        if cm == '0':
            os.system("shutdown /p")
        if cm == '1':
            os.system("shutdown /r /t 0")
    time.sleep(5)

# Удаляем файл блокировки после завершения работы приложения
os.remove(app_lock_file)
