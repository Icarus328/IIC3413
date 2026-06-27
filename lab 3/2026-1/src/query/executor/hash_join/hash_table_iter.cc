#include "hash_table_iter.h"

HashTableIter::HashTableIter(const HashTable& htbl, const Value& key)
    : htbl(htbl),
      key(key) {
  this->column_idx = htbl.column_idx;
}

void HashTableIter::begin() {
  auto hash = key.get_hash();
  uint64_t encoded_value = key.encoded();

  auto probe_slot = hash % HashTable::CAPACITY;

  current_hash_table_entry.head_offset = uint64_t(-1);

  // TODO: Problema 2
  for (uint32_t i = 0; i < HashTable::CAPACITY; ++i) {
    uint32_t current_slot = (probe_slot + i) % HashTable::CAPACITY;
    auto entry = htbl.probe_table[current_slot];

    if (entry.head_offset == uint64_t(-1)) {
      break; //Si el slot está vacio, detenemos la busqueda
    }

    if (entry.hash == hash && entry.encoded_value == encoded_value) {
      current_hash_table_entry = entry;
      break; //Encontramos el match con nuestra key
    }
  }

  current_record_buffer_pos = current_hash_table_entry.head_offset;
}

Record HashTableIter::next() {
  // TODO: Problema 2
  if (current_record_buffer_pos == uint64_t(-1)) {
    return Record();
  }

  uint64_t next;
  //Usamos memcy para leer el next
  memcpy(&next, htbl.record_buffer + current_record_buffer_pos, sizeof(int64_t));

  //Calculamos el offset
  uint64_t record_offset = current_record_buffer_pos + sizeof(int64_t);

  //Leemos el record desde el buffer
  Record record = Record::deserialize(htbl.record_buffer + record_offset, htbl.datatypes);

  //Avanzamos el puntero
  current_record_buffer_pos = next;

  return record;
}

void HashTableIter::reset() {
  // HINT: Si begin encuentra la primera ocurrencia correctamente no necesitan hacer nada aquí
  current_record_buffer_pos = current_hash_table_entry.head_offset;
}