# Filters
2022 Master's thesis & 2021 coursework.

1. Bloom, Cuckoo, Vacuum and Xor filters implementation.
2. SuRF implementation and modification.
3. 6 test cases for filters.

## Сборка и запуск
Сборка
```
g++ main.cpp -std=c++17  -O2 -o main
```

При запуске создается фильтр на основе items_cnt случайных объектов, вид которых задается параметром `test_data`. Проверяется, что все добавленные объекты находятся в фильтре (true positive rate == 100%), а затем на основе items_cnt отсутствующих значений оценивается false positive rate.
```
./main filter_name [test_data] [items_cnt] [filter params]
```
`filter_name` — название фильтра. (`bloom`, `cuckoo` или `xor`)

`test_data` — вид данных для тестирования (`uniform`, `zipf`, `text`, `all`)

`items_cnt` — количество объектов для тестирования. (`1000000` по умолчанию)

`filter params` — опциональные параметры фильтров.


### Для фильтра Блума:
```
./main bloom test_data items_cnt [buckets_count] [hash_functions_count]
```

`buckets_count` — число элементов в каждом из `hash_functions_count` массивов. (`2000000` по умолчанию)

`hash_functions_count` — число используемых хэш-функций. (`4` по умолчанию)


### Для фильтра Cuckoo:
```
./main cuckoo test_data items_cnt [max_buckets_count] [bucket_size] [fingerprint_size_bits] [max_num_kicks]
```
`max_buckets_count` — количество бакетов в хэш-таблице будет равно максимальной степени двойки, не превышающей это число. (`2^18` по умолчанию)

`bucket_size` — размер бакета. (`4` по умолчанию)

`fingerprint_size_bits` — размер fingerprint'а в битах. (`4` по умолчанию)

`max_num_kicks` — максимальное количество перестановок в хэш-таблице при добавлении нового элемента. (`500` по умолчанию)

В случае, если после `max_num_kicks` перестановок для нового числа не находится места, инициализация фильтра считается неудачной, выбрасывается исключение и количеством элементов, которые смогли поместиться в фильтр. В этом случае, нужно перезапустить программу с большими значениями первых трех параметров.


### Для Xor-фильтра:
```
./main xor test_data items_cnt [fingerprint_size_bits] [buckets_count_coefficient] [additional_buckets]
```
`fingerprint_size_bits` — размер fingerprint'а в битах. (`4` по умолчанию)

`buckets_count_coefficient` — вещественное число, связывающее размер хэш-таблицы и количество элементов в фильтре. (`1.23` по умолчанию)

`additional_buckets` — число дополнительных бакетов (`32` по умолчанию). В итоге, размер хэш таблицы будет равен `items_cnt * buckets_count_coefficient + additional_buckets`


### Для Vacuum фильтра:
```
./main vacuum test_data items_cnt [fingerprint_size_bits] [max_num_kicks]
```

`fingerprint_size_bits` — размер fingerprint'а в битах. (`4` по умолчанию)

`max_num_kicks` — максимальное количество перестановок в хэш-таблице при добавлении нового элемента. (`500` по умолчанию)


### Для SuRF:
```
./main surf test_data items_cnt suffix_type [suffix_size] [fix_length] [cut_gain_threshold]
```

`suffix_type` — тип суффикса: empty, hash или real.

`suffix_size` — размер суффикса. Суффиксы типа real длиннее 8 не поддерживаются. (`8` по умолчанию)

`fix_length` — если больше нуля, отсечь все вершины дерева ниже указанного числа. Если -1, использовать символ kTerminator всегда, даже если все строки, поданные на вход, имеют равную длину. (`0` по умолчанию)

`cut_gain_threshold` — порог для отсечания длинных ветвей дерева. 0 — не отсекать. Иначе рекомендуется использовать 0.1. (`0` по умолчанию)


### Тестовые данные:
`uniform` — случайные целые числа типа int, равномерное распределение.

`zipf` — случайные целые числа типа int, Zipf-Mandelbrot distribution. (https://en.wikipedia.org/wiki/Zipf%E2%80%93Mandelbrot_law)

`text` — случайные строки из строчных латинских букв длиной до 100 символов.

`real` — реальные данные из датасета https://www.kaggle.com/datasets/rupakroy/online-payments-fraud-detection-dataset?resource=download

`words` — случайные тексты из 1-5 английских слов, Zipf-Mandelbrot distribution. Словарь: https://github.com/derekchuank/high-frequency-vocabulary

`words_msp` — аналогично `words`, но в каждом слове с вероятностью 0.1 одна случайная буква заменяется на случайную другую.

`all` — запустить все вышеперечисленные тесты.

Скачать данные для `real` и `words` можно так:
```
mkdir data
cd data
wget https://www.dropbox.com/s/8egso4hut3a5etu/payments.csv
wget https://www.dropbox.com/s/lx1oygzjdnjhznq/words30k.txt
```


### Пример запуска:
`./main cuckoo all 75000 50000 4 7` — фильтр cuckoo на 75000 объектов, `max_buckets_count = 50000, bucket_size = 4, fingerprint_size_bits = 7, max_num_kicks = 500 (Default)`, запустить все тесты.
