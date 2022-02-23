# Filters
2021 coursework: bloom, cuckoo and xor filters implementation

## Сборка и запуск
Сборка
```
g++ main.cpp -std=c++17  -O2 -o main
```

При запуске создается фильтр на основе items_cnt случайных чисел. Проверяется, что все добавленные числа находятся в фильтре (true positive rate == 100%), а затем на основе items_cnt отсутствующих значений оценивается false positive rate.
```
./main filter_name [items_cnt] [filter params]
```
`filter_name` — название фильтра. (`bloom`, `cuckoo` или `xor`)

`items_cnt` — количество чисел для тестирования. (`1000000` по умолчанию)

`filter params` — опциональные параметры фильтров.

### Для фильтра Блума:
```
./main bloom items_cnt [buckets_count] [hash_functions_count]
```
`buckets_count` — число элементов в каждом из `hash_functions_count` массивов. (`2000000` по умолчанию)
`hash_functions_count` — число используемых хэш-функций. (`4` по умолчанию)


### Для фильтра Cuckoo:
```
./main cuckoo items_cnt [max_buckets_count] [bucket_size] [fingerprint_size_bits] [max_num_kicks]
```
`max_buckets_count` — количество бакетов в хэш-таблице будет равно максимальной степени двойки, не превышающей это число. (`2^18` по умолчанию)

`bucket_size` — размер бакета. (`4` по умолчанию)

`fingerprint_size_bits` — размер fingerprint'а в битах. (`4` по умолчанию)

`max_num_kicks` — максимальное количество перестановок в хэш-таблице при добавлении нового элемента. (`500` по умолчанию)

В случае, если после `max_num_kicks` перестановок для нового числа не находится места, инициализация фильтра считается неудачной, выбрасывается исключение и количеством элементов, которые смогли поместиться в фильтр. В этом случае, нужно перезапустить программу с большими значениями первых трех параметров.


### Для Xor-фильтра:
```
./main xor items_cnt [fingerprint_size_bits] [buckets_count_coefficient] [additional_buckets]
```
`fingerprint_size_bits` — размер fingerprint'а в битах. (`4` по умолчанию)

`buckets_count_coefficient` — вещественное число, связывающее размер хэш-таблицы и количество элементов в фильтре. (`1.23` по умолчанию)

`additional_buckets` — число дополнительных бакетов (`32` по умолчанию). В итоге, размер хэш таблицы будет равен `items_cnt * buckets_count_coefficient + additional_buckets`


### Пример запуска:
`./main cuckoo 75000 50000 4 7` — фильтр cuckoo с 75000 случайными числами, `max_buckets_count = 50000, bucket_size = 4, fingerprint_size_bits = 7, max_num_kicks = 500 (Default)`
