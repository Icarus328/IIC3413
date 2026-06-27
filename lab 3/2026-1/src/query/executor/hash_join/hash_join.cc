#include "hash_join.h"

HashJoin::HashJoin(
    std::unique_ptr<QueryIter> _lhs,
    std::unique_ptr<QueryIter> _rhs,
    std::vector<ProjectedColumn>&& _projected_lhs_columns,
    std::vector<ProjectedColumn>&& _projected_rhs_columns,
    std::pair<size_t, size_t>&& _equality
)
    : lhs(std::move(_lhs)),
      rhs(std::move(_rhs)),
      projected_lhs_columns(std::move(_projected_lhs_columns)),
      projected_rhs_columns(std::move(_projected_rhs_columns)),
      equality(std::move(_equality)) {}

void HashJoin::begin() {
  lhs->begin();
  rhs->begin();

  prepare();
}

void HashJoin::prepare() {
  std::vector<DataType> datatypes;
  for (auto& pjc : projected_lhs_columns) {
    datatypes.push_back(pjc.col.info.datatype);
  }

  htbl = std::make_unique<HashTable>(equality.first, datatypes);
  state = State::BUILD;
}

Record HashJoin::next() {
  // TODO: Problema 3
  while (true) {
    if (state == State::BUILD) {
      bool table_full = false;

      //Llenamos la hash-table hsta que se llene lhs
      while (!table_full) {
        Record left_rec;

        //Si quedó una tupla sin procesar, la procesamos
        if (!unprocessed_record.invalid()) {
          left_rec = unprocessed_record;
          unprocessed_record = Record(); 
        } else {
          //Si no, pasamos a la siguiente relación
          left_rec = lhs->next();
        }

        if (left_rec.invalid()) {
          break;
        }

        //Intentamos insertar el record
        if (!htbl->try_insert_record(left_rec)) {
          //La tabla se llenó, guardamos el registro para el proximo bloque
          unprocessed_record = left_rec;
          table_full = true;
        }
      }

      //Terminada BUILD, pasamos a PROBE
      state = State::PROBE;
      
      //Leemos el primer registro a la derecha
      current_rhs = rhs->next();

      if (!current_rhs.invalid()) {
        //Obtenemos la key
        Value rhs_key = current_rhs.values[equality.second];
        
        //Instanciamos e iniciamos el iterador de la hashtable
        hash_lhs = std::make_unique<HashTableIter>(*htbl, rhs_key);
        hash_lhs->begin();
      }
    } 
    
    else if (state == State::PROBE) {
      //Buscamos coincidencias para los registros de rhs
      
      if (current_rhs.invalid()) {
        //Dejamos de iterar sobre la tabla derecha
        
        if (unprocessed_record.invalid()) {
          //Si unprocessed_record es invalido, lhs se acabó
          state = State::DONE;
        } else {
          //Si no, lhs no cabe en memoria
          //Reiniciamos el operador derecho
          rhs->reset();

          //Limpiamos y recreamos la tabla de hash
          std::vector<DataType> datatypes;
          for (auto& pjc : projected_lhs_columns) {
            datatypes.push_back(pjc.col.info.datatype);
          }
          htbl = std::make_unique<HashTable>(equality.first, datatypes);

          //Reconstruimos
          state = State::BUILD;
        }
        continue; //Volvemos al inicio del while
      }

      //Buscamos un match para el rhs actual
      Record left_match = hash_lhs->next();

      if (!left_match.invalid()) {
        //Encontramos un match, extraemos valores y combinamos
        std::vector<Value> joined_values;
        
        //Extraemos valores del lado izquiero
        for (size_t i = 0; i < projected_lhs_columns.size(); ++i) {
          joined_values.push_back(left_match.values[i]);
        }
        //Extraemos valores del lado derecho
        for (size_t i = 0; i < projected_rhs_columns.size(); ++i) {
          joined_values.push_back(current_rhs.values[i]);
        }
        
        //Retornamos la tupla combinada
        return Record(joined_values);
      } else {
        //Si no hay más coincidencias, pasamos al siguiente
        current_rhs = rhs->next();
        
        if (!current_rhs.invalid()) {
          Value rhs_key = current_rhs.values[equality.second];
          hash_lhs = std::make_unique<HashTableIter>(*htbl, rhs_key);
          hash_lhs->begin();
        }
      }
    } 
    
    else if (state == State::DONE) {
      //Terminamos
      return Record();
    }
  }
}

void HashJoin::reset() {
  lhs->reset();
  rhs->reset();
  prepare();
}

std::vector<Column> HashJoin::get_columns() {
  std::vector<Column> res;
  for (auto& c : projected_lhs_columns) {
    res.push_back(c.col);
  }
  for (auto& c : projected_rhs_columns) {
    res.push_back(c.col);
  }
  return res;
}

std::ostream& HashJoin::print_to_ostream(std::ostream& os, int indent) const {
  os << std::string(indent, ' ');
  os << "HashJoin(";
  os << projected_lhs_columns[equality.first].col.alias << "."
     << projected_lhs_columns[equality.first].col.info.name;
  os << " == ";
  os << projected_rhs_columns[equality.second].col.alias << "."
     << projected_rhs_columns[equality.second].col.info.name;
  os << ")\n";
  lhs->print_to_ostream(os, indent + 2);
  rhs->print_to_ostream(os, indent + 2);
  return os;
}
