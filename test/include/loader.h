// loader.h
#ifndef LOADER_H
#define LOADER_H

/* Загружает бинарники в память VM.
   Заполняет modules[], module_count, program_hash, program_size */
void load_programs(const char **fnames, int count);

#endif
