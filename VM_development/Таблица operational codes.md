
## 🔢 **Арифметика (0–16)**

|Название|Hex|Bin|C-функция|Комментарий|
|---|---|---|---|---|
|add|0x00|0000000|op_add|A = B + C|
|sub|0x01|0000001|op_sub|A = B - C|
|mul|0x02|0000010|op_mul|A = B * C|
|div|0x03|0000011|op_div|A = B / C|
|mod|0x04|0000100|op_mod|A = B % C|
|expt|0x05|0000101|op_expt|A = B^C|
|abs|0x06|0000110|op_abs||
|sqrt|0x07|0000111|op_sqrt||
|ln|0x08|0001000|op_ln||
|log|0x09|0001001|op_log||
|exp|0x0A|0001010|op_exp||
|sin|0x0B|0001011|op_sin||
|cos|0x0C|0001100|op_cos||
|tan|0x0D|0001101|op_tan||
|asin|0x0E|0001110|op_asin||
|acos|0x0F|0001111|op_acos||
|atan|0x10|0010000|op_atan||

---

## 🔢 **Логика (17–20)**

|Название|Hex|Bin|C-функция|
|---|---|---|---|
|and|0x11|0010001|op_and|
|or|0x12|0010010|op_or|
|xor|0x13|0010011|op_xor|
|not|0x14|0010100|op_not|

---

## 🔢 **Сравнения (21–26)**

|Название|Hex|Bin|C-функция|
|---|---|---|---|
|eq|0x15|0010101|op_eq|
|ne|0x16|0010110|op_ne|
|gt|0x17|0010111|op_gt|
|ge|0x18|0011000|op_ge|
|lt|0x19|0011001|op_lt|
|le|0x1A|0011010|op_le|

---

## 🕒 **Время/дата (27–38)**

|Название|Hex|Bin|C-функция|
|---|---|---|---|
|time|0x1B|0011011|op_time|
|date|0x1C|0011100|op_date|
|tod|0x1D|0011101|op_tod|
|dt|0x1E|0011110|op_dt|
|add_time|0x1F|0011111|op_add_time|
|sub_time|0x20|0100000|op_sub_time|
|year|0x21|0100001|op_year|
|month|0x22|0100010|op_month|
|day|0x23|0100011|op_day|
|hour|0x24|0100100|op_hour|
|minute|0x25|0100101|op_minute|
|second|0x26|0100110|op_second|

---

## 🔤 **Строки (39–46)**

|Название|Hex|Bin|Функция|
|---|---|---|---|
|len|0x27|0100111|op_len|
|concat|0x28|0101000|op_concat|
|left|0x29|0101001|op_left|
|right|0x2A|0101010|op_right|
|mid|0x2B|0101011|op_mid|
|insert|0x2C|0101100|op_insert|
|delete|0x2D|0101101|op_delete|
|replace|0x2E|0101110|op_replace|

---

## ⏲ **Таймеры (47–55)**

|Название|Hex|Bin|Функция|
|---|---|---|---|
|ton|0x2F|0101111|op_ton|
|tof|0x30|0110000|op_tof|
|tp|0x31|0110001|op_tp|
|ctu|0x32|0110010|op_ctu|
|ctd|0x33|0110011|op_ctd|
|ctud|0x34|0110100|op_ctud|
|limit|0x35|0110101|op_limit|
|sel|0x36|0110110|op_sel|
|mux|0x37|0110111|op_mux|
